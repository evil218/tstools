/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: ts.c
 * funx: analyse ts stream
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* for uint?_t, etc */
#include <string.h> /* for memset, memcpy, etc */

#include "error.h"
#include "crc.h"
#include "ts.h"

#if 0
#define DEBUG /* print detail info. to debug ts module */
#endif

#ifdef DEBUG
#define dbg(level, ...) \
        do { \
                if (level >= 0) { \
                        fprintf(stderr, "\"%s\", line %d: ",__FILE__, __LINE__); \
                        fprintf(stderr, __VA_ARGS__); \
                        fprintf(stderr, "\n"); \
                } \
        } while (0)
#else
#define dbg(level, ...)
#endif /* DEBUG */

#define BIT(n)                          (1<<(n))

#define STC_BASE_MS                     (90) /* 90 clk == 1(ms) */
#define STC_BASE_1S                     (90 * 1000) /* do NOT use 1e3 */
#define STC_BASE_OVF                    (1LL << 33)         /* 0x0200000000 */

#define STC_US                          (27)               /* 27 clk == 1(us) */
#define STC_MS                          (27 * 1000)        /* do NOT use 1e3  */
#define STC_1S                          (27 * 1000 * 1000) /* do NOT use 1e3  */
#define STC_OVF                         (STC_BASE_OVF * 300L) /* 2576980377600 */

struct ts {
        uint8_t sync_byte;
        uint8_t transport_error_indicator; /* 1-bit */
        uint8_t payload_unit_start_indicator; /* 1-bit */
        uint8_t transport_priority; /* 1-bit */
        uint16_t PID; /* 13-bit */
        uint8_t transport_scrambling_control; /* 2-bit */
        uint8_t adaption_field_control; /* 2-bit */
        uint8_t continuity_counter; /* 4-bit */
};

struct af {
        uint8_t adaption_field_length;
        uint8_t discontinuity_indicator; /* 1-bit */
        uint8_t random_access_indicator; /* 1-bit */
        uint8_t elementary_stream_priority_indicator; /* 1-bit */
        uint8_t PCR_flag; /* 1-bit */
        uint8_t OPCR_flag; /* 1-bit */
        uint8_t splicing_pointer_flag; /* 1-bit */
        uint8_t transport_private_data_flag; /* 1-bit */
        uint8_t adaption_field_extension_flag; /* 1-bit */
        uint64_t program_clock_reference_base; /* 33-bit */
        uint16_t program_clock_reference_extension; /* 9-bit */
        uint64_t original_program_clock_reference_base; /* 33-bit */
        uint16_t original_program_clock_reference_extension; /* 9-bit */
        uint8_t splice_countdown;
        uint8_t transport_private_data_length;
        uint8_t private_data_byte[256];
        uint8_t adaption_field_extension_length;
        /* ... */
};

struct pes {
        uint32_t packet_start_code_prefix; /* 24-bit */
        uint8_t stream_id;
        uint16_t PES_packet_length;
        uint8_t PES_scrambling_control; /* 2-bit */
        uint8_t PES_priority; /* 1-bit */
        uint8_t data_alignment_indicator; /* 1-bit */
        uint8_t copyright; /* 1-bit */
        uint8_t original_or_copy; /* 1-bit */
        uint8_t PTS_DTS_flags; /* 2-bit */
        uint8_t ESCR_flag; /* 1-bit */
        uint8_t ES_rate_flag; /* 1-bit */
        uint8_t DSM_trick_mode_flag; /* 1-bit */
        uint8_t additional_copy_info_flag; /* 1-bit */
        uint8_t PES_CRC_flag; /* 1-bit */
        uint8_t PES_extension_flag; /* 1-bit */
        uint8_t PES_header_data_length;
        uint64_t PTS; /* 33-bit */
        uint64_t DTS; /* 33-bit */
        uint64_t ESCR_base; /* 33-bit */
        uint16_t ESCR_extension; /* 9-bit */
        uint32_t ES_rate; /* 22-bit */
        uint8_t trick_mode_control; /* 3-bit */
        uint8_t field_id; /* 2-bit */
        uint8_t intra_slice_refresh; /* 1-bit */
        uint8_t frequency_truncation; /* 2-bit */
        uint8_t rep_cntrl; /* 5-bit */
        uint8_t additional_copy_info; /* 7-bit */
        uint16_t previous_PES_packet_CRC;
        uint8_t PES_private_data_flag; /* 1-bit */
        uint8_t pack_header_field_flag; /* 1-bit */
        uint8_t program_packet_sequence_counter_flag; /* 1-bit */
        uint8_t P_STD_buffer_flag; /* 1-bit */
        uint8_t PES_extension_flag_2; /* 1-bit */
        uint8_t  PES_private_data[16]; /* 128-bit */
        uint8_t pack_field_length;
        uint8_t program_packet_sequence_counter; /* 7-bit */
        uint8_t MPEG1_MPEG2_identifier; /* 1-bit */
        uint8_t original_stuff_length; /* 6-bit */
        uint8_t P_STD_buffer_scale; /* 1-bit */
        uint16_t P_STD_buffer_size; /* 13-bit */
        uint8_t PES_extension_field_length; /* 7-bit */
};

struct pid_type_table {
        char *sdes; /* short description */
        char *ldes; /* long description */
};

struct ts_pid_table {
        uint16_t min; /* PID range */
        uint16_t max; /* PID range */
        int     type; /* index of item in PID_TYPE[] */
};

struct table_id_table {
        uint8_t min; /* table ID range */
        uint8_t max; /* table ID range */
        int check_CRC; /* some table do not need to check CRC_32 */
        int type; /* index of item in PID_TYPE[] */
};

struct stream_type {
        uint8_t stream_type;
        int   type; /* index of item in PID_TYPE[] */
        char *sdes; /* short description */
        char *ldes; /* long description */
};

struct obj {
        int is_first_pkt;

        uint8_t *p; /* point to rslt.line */
        int len;

        struct ts ts;
        struct af af;
        struct pes pes;

        uint32_t pkt_size; /* 188 or 204 */
        int state;

        int need_pes_align; /* 0: dot't need; 1: need PES align */
        int is_verbose; /* 0: shut up; 1: report key step */

        struct ts_rslt rslt;
};

enum PID_TYPE_ENUM {
        /* should be synchronize with PID_TYPE[]! */
        PAT_PID,
        CAT_PID,
        TSDT_PID,
        RSV_PID,
        NIT_PID,
        ST_PID,
        SDT_PID,
        BAT_PID,
        EIT_PID,
        RST_PID,
        TDT_PID,
        TOT_PID,
        NS_PID,
        INB_PID,
        MSU_PID,
        DIT_PID,
        SIT_PID,
        USR_PID,
        PMT_PID,
        VID_PID,
        VID_PCR,
        AUD_PID,
        AUD_PCR,
        PCR_PID,
        NULL_PID,
        UNO_PID,
        UNO_PCR,
        BAD_PID
};

static const struct pid_type_table PID_TYPE[] = {
        /* should be synchronize with enum PID_TYPE_ENUM! */
        {" PAT", "program association section"},
        {" CAT", "conditional access section"},
        {"TSDT", "transport stream description section"},
        {" RSV", "reserved"},
        {" NIT", "network information section"},
        {"  ST", "stuffing section"},
        {" SDT", "service description section"},
        {" BAT", "bouquet association section"},
        {" EIT", "event information section"},
        {" RST", "running status section"},
        {" TDT", "time data section"},
        {" TOT", "time offset section"},
        {"  NS", "Network Synchroniztion"},
        {" INB", "Inband signaling"},
        {" MSU", "Measurement"},
        {" DIT", "discontinuity information section"},
        {" SIT", "selection information section"},
        {" USR", "user define"},
        {" PMT", "program map section"},
        {" VID", "video packet"},
        {"PVID", "video packet with PCR"},
        {" AUD", "audio packet"},
        {"PAUD", "audio packet with PCR"},
        {" PCR", "program counter reference"},
        {"NULL", "empty packet"},
        {" UNO", "unknown"},
        {"PUNO", "unknown packet with PCR"},
        {" BAD", "illegal"}
};

static const struct ts_pid_table PID_TABLE[] = {
        /* 0*/{0x0000, 0x0000,  PAT_PID},
        /* 1*/{0x0001, 0x0001,  CAT_PID},
        /* 2*/{0x0002, 0x0002, TSDT_PID},
        /* 3*/{0x0003, 0x000F,  RSV_PID},
        /* 4*/{0x0010, 0x0010,  NIT_PID}, /* NIT/ST */
        /* 5*/{0x0011, 0x0011,  SDT_PID}, /* SDT/BAT/ST */
        /* 6*/{0x0012, 0x0012,  EIT_PID}, /* EIT/ST */
        /* 7*/{0x0013, 0x0013,  RST_PID}, /* RST/ST */
        /* 8*/{0x0014, 0x0014,  TDT_PID}, /* TDT/TOT/ST */
        /* 9*/{0x0015, 0x0015,   NS_PID},
        /*10*/{0x0016, 0x001B,  RSV_PID},
        /*11*/{0x001C, 0x001C,  INB_PID},
        /*12*/{0x001D, 0x001D,  MSU_PID},
        /*13*/{0x001E, 0x001E,  DIT_PID},
        /*14*/{0x001F, 0x001F,  SIT_PID},
        /*15*/{0x0020, 0x1FFE,  USR_PID},
        /*16*/{0x1FFF, 0x1FFF, NULL_PID},
        /*17*/{0x2000, 0xFFFF,  BAD_PID}, /* loop stop condition! */
};

static const struct table_id_table TABLE_ID_TABLE[] = {
        /* 0*/{0x00, 0x00, 1,  PAT_PID},
        /* 1*/{0x01, 0x01, 1,  CAT_PID},
        /* 2*/{0x02, 0x02, 1,  PMT_PID},
        /* 3*/{0x03, 0x03, 0, TSDT_PID},
        /* 4*/{0x04, 0x3F, 0,  RSV_PID},
        /* 5*/{0x40, 0x40, 1,  NIT_PID}, /* actual network */
        /* 6*/{0x41, 0x41, 1,  NIT_PID}, /* other network */
        /* 7*/{0x42, 0x42, 1,  SDT_PID}, /* actual transport stream */
        /* 8*/{0x43, 0x45, 0,  RSV_PID},
        /* 9*/{0x46, 0x46, 1,  SDT_PID}, /* other transport stream */
        /*10*/{0x47, 0x49, 0,  RSV_PID},
        /*11*/{0x4A, 0x4A, 1,  BAT_PID},
        /*12*/{0x4B, 0x4D, 0,  RSV_PID},
        /*13*/{0x4E, 0x4E, 1,  EIT_PID}, /* actual transport stream, P/F */
        /*14*/{0x4F, 0x4F, 1,  EIT_PID}, /* other transport stream, P/F */
        /*15*/{0x50, 0x5F, 1,  EIT_PID}, /* actual transport stream, schedule */
        /*16*/{0x60, 0x6F, 1,  EIT_PID}, /* other transport stream, schedule */
        /*17*/{0x70, 0x70, 0,  TDT_PID},
        /*18*/{0x71, 0x71, 0,  RST_PID},
        /*19*/{0x72, 0x72, 0,   ST_PID},
        /*20*/{0x73, 0x73, 1,  TOT_PID},
        /*21*/{0x74, 0x7D, 0,  RSV_PID},
        /*22*/{0x7E, 0x7E, 0,  DIT_PID},
        /*23*/{0x7F, 0x7F, 0,  SIT_PID},
        /*24*/{0x80, 0xFE, 0,  USR_PID},
        /*25*/{0xFF, 0xFF, 0,  RSV_PID}, /* loop stop condition! */
};

static const struct stream_type STREAM_TYPE_TABLE[] = {
        {0x00, RSV_PID, "Reserved", "ITU-T|ISO/IEC Reserved"},
        {0x01, VID_PID, "MPEG-1", "ISO/IEC 11172-2 Video"},
        {0x02, VID_PID, "MPEG-2", "ITU-T Rec.H.262|ISO/IEC 13818-2 Video or MPEG-1 parameter limited"},
        {0x03, AUD_PID, "MPEG-1", "ISO/IEC 11172-3 Audio"},
        {0x04, AUD_PID, "MPEG-2", "ISO/IEC 13818-3 Audio"},
        {0x05, USR_PID, "private", "ITU-T Rec.H.222.0|ISO/IEC 13818-1 private_sections"},
        {0x06, AUD_PID, "AC3|TT|LPCM", "ITU-T Rec.H.222.0|ISO/IEC 13818-1 PES packets containing private data|Dolby Digital DVB|Linear PCM"},
        {0x07, USR_PID, "MHEG", "ISO/IEC 13522 MHEG"},
        {0x08, USR_PID, "DSM-CC", "ITU-T Rec.H.222.0|ISO/IEC 13818-1 Annex A DSM-CC"},
        {0x09, USR_PID, "H.222.1", "ITU-T Rec.H.222.1"},
        {0x0A, USR_PID, "MPEG2 type A", "ISO/IEC 13818-6 type A: Multi-protocol Encapsulation"},
        {0x0B, USR_PID, "MPEG2 type B", "ISO/IEC 13818-6 type B: DSM-CC U-N Messages"},
        {0x0C, USR_PID, "MPEG2 type C", "ISO/IEC 13818-6 type C: DSM-CC Stream Descriptors"},
        {0x0D, USR_PID, "MPEG2 type D", "ISO/IEC 13818-6 type D: DSM-CC Sections or DSM-CC Addressable Sections"},
        {0x0E, USR_PID, "auxiliary", "ITU-T Rec.H.222.0|ISO/IEC 13818-1 auxiliary"},
        {0x0F, AUD_PID, "AAC ADTS", "ISO/IEC 13818-7 Audio with ADTS transport syntax"},
        {0x10, VID_PID, "MPEG-4", "ISO/IEC 14496-2 Visual"},
        {0x11, AUD_PID, "AAC LATM", "ISO/IEC 14496-3 Audio with LATM transport syntax"},
        {0x12, AUD_PID, "MPEG-4", "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets"},
        {0x13, AUD_PID, "MPEG-4", "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC 14496_sections"},
        {0x14, USR_PID, "MPEG-2", "ISO/IEC 13818-6 Synchronized Download Protocol"},
        {0x15, USR_PID, "MPEG-2", "Metadata carried in PES packets"},
        {0x16, USR_PID, "MPEG-2", "Metadata carried in metadata_sections"},
        {0x17, USR_PID, "MPEG-2", "Metadata carried in ISO/IEC 13818-6 Data Carousel"},
        {0x18, USR_PID, "MPEG-2", "Metadata carried in ISO/IEC 13818-6 Object Carousel"},
        {0x19, USR_PID, "MPEG-2", "Metadata carried in ISO/IEC 13818-6 Synchronized Dowload Protocol"},
        {0x1A, USR_PID, "IPMP", "IPMP stream(ISO/IEC 13818-11, MPEG-2 IPMP)"},
        {0x1B, VID_PID, "H.264", "ITU-T Rec.H.264|ISO/IEC 14496-10 Video"},
        {0x42, VID_PID, "AVS", "Advanced Video Standard"},
        {0x7F, USR_PID, "IPMP", "IPMP stream"},
        {0x81, AUD_PID, "AC3", "Dolby Digital ATSC"},
        {0xFF, UNO_PID, "UNKNOWN", "Unknown stream"}, /* loop stop condition! */
};

enum {
        /* note, in any state:  */
        /*     * meet new PID -> add to pid_list */
        /*     * meet new table -> add to section_list and parse */
        /*     * meet old table -> if it was modified, report a warning */
        STATE_NEXT_PAT, /* colloct PAT section, parse to create prog_list */
        STATE_NEXT_PMT, /* collect PMT section, parse to create track_list */
        STATE_NEXT_PKT  /* parse packet with current lists */
};

static int state_next_pat(struct obj *obj);
static int state_next_pmt(struct obj *obj);
static int state_next_pkt(struct obj *obj);

static int parse_TS_head(struct obj *obj); /* TS head information */
static int parse_AF(struct obj *obj); /* Adaption Fields information */
static int section(struct obj *obj); /* collect PSI/SI section data */
static int parse_table(struct obj *obj);
static int parse_PSI_head(struct obj *obj, uint8_t *section);
static int parse_PAT_load(struct obj *obj, uint8_t *section);
static int parse_PMT_load(struct obj *obj, uint8_t *section);
static int parse_SDT_load(struct obj *obj, uint8_t *section);
static int parse_PES_head(struct obj *obj); /* PES layer information */
static int parse_PES_head_switch(struct obj *obj);
static int parse_PES_head_detail(struct obj *obj);

static struct ts_pid *add_to_pid_list(struct ts_pid **phead, struct ts_pid *pid);
static struct ts_pid *add_new_pid(struct obj *obj);
static int is_all_prog_parsed(struct obj *obj);
static int pid_type(uint16_t pid);
static const struct table_id_table *table_type(uint8_t id);
static int track_type(struct ts_track *track);

static int64_t lmt_add(int64_t t1, int64_t t2, int64_t ovf);
static int64_t lmt_min(int64_t t1, int64_t t2, int64_t ovf);

static int dump(uint8_t *buf, int len); /* for debug */

int tsCreate(struct ts_rslt **rslt)
{
        struct obj *obj;
        struct ts_error *err;

        obj = (struct obj *)malloc(sizeof(struct obj));
        if(!obj) {
                DBG(ERR_MALLOC_FAILED, "\n");
                return (int)NULL;
        }

        *rslt = &(obj->rslt);
        (*rslt)->pkt = &((*rslt)->PKT);
        (*rslt)->table0 = NULL;
        (*rslt)->has_got_transport_stream_id = 0;
        (*rslt)->transport_stream_id = 0;
        (*rslt)->prog0 = NULL;
        (*rslt)->pid0 = NULL;

        obj->is_first_pkt = 1;
        obj->state = STATE_NEXT_PAT;
        obj->need_pes_align = 1;
        obj->is_verbose = 0;

        (*rslt)->cnt = 0;
        (*rslt)->CC_lost = 0;
        (*rslt)->is_pat_pmt_parsed = 0;
        (*rslt)->is_psi_parse_finished = 0;
        (*rslt)->concerned_pid = 0x0000; /* PAT_PID */
        (*rslt)->interval = 0;
        (*rslt)->lCTS = 0L;
        (*rslt)->STC = 0L;

        /* clear error struct */
        err = &((*rslt)->err);
        memset(err, 0, sizeof(struct ts_error));

        return (int)obj;
}

int tsDelete(int id)
{
        struct obj *obj;

        obj = (struct obj *)id;
        if(!obj) {
                DBG(ERR_BAD_ID, "\n");
                return -ERR_BAD_ID;
        }
        else {
                struct ts_rslt *rslt = &(obj->rslt);
                struct lnode *lnode;

                for(lnode = (struct lnode *)(rslt->prog0); lnode; lnode = lnode->next) {
                        struct ts_prog *prog = (struct ts_prog *)lnode;
                        list_free(&(prog->track0));
                        list_free(&(prog->table_02.section0));
                }
                list_free(&(rslt->prog0));
                list_free(&(rslt->pid0));

                for(lnode = (struct lnode *)(rslt->table0); lnode; lnode = lnode->next) {
                        struct ts_table *table = (struct ts_table *)lnode;
                        list_free(&(table->section0));
                }
                list_free(&(rslt->table0));

                free(obj);

                return 0;
        }
}

int tsParseTS(int id)
{
        struct obj *obj;
        struct ts_rslt *rslt;
        struct ts_pkt *pkt;

        obj = (struct obj *)id;
        if(!obj) {
                DBG(ERR_BAD_ID, "\n");
                return -ERR_BAD_ID;
        }
        rslt = &(obj->rslt);
        pkt = rslt->pkt;

        obj->p = pkt->ts;
        obj->len = 188;
        /*dump(pkt->ts, 188); */

        /* packet count */
        if(obj->is_first_pkt) {
                obj->is_first_pkt = 0;
                rslt->cnt = 0;
        }
        else {
                rslt->cnt++;
        }

        /*fprintf(stderr, "cnt: %lld, addr: %lld\n", rslt->cnt, pkt->ADDR); */
        return parse_TS_head(obj);
}

int tsParseOther(int id)
{
        struct obj *obj;

        obj = (struct obj *)id;
        if(!obj) {
                DBG(ERR_BAD_ID, "\n");
                return -ERR_BAD_ID;
        }

        switch(obj->state) {
                case STATE_NEXT_PAT:
                        state_next_pat(obj);
                        break;
                case STATE_NEXT_PMT:
                        state_next_pmt(obj);
                        break;
                case STATE_NEXT_PKT:
                default:
                        state_next_pkt(obj);
                        break;
        }

        return 0;
}

static int state_next_pat(struct obj *obj)
{
        struct ts *ts = &(obj->ts);
        struct ts_table *table;
        uint8_t section_number;

        if(0x0000 != ts->PID) {
                return -1; /* not PAT */
        }

        /* section parse has done in parse_TS_head()! */
        dbg(1, "search 0x00 in table_list");
        table = (struct ts_table *)list_search(&(obj->rslt.table0), 0x00);
        if(!table) {
                return -1;
        }
        else {
                struct lnode *lnode;

                section_number = 0;
                for(lnode = (struct lnode *)(table->section0); lnode; lnode = lnode->next) {
                        struct ts_section *section = (struct ts_section *)lnode;

                        if(section_number > table->last_section_number) {
                                return -1;
                        }
                        if(section_number != section->section_number) {
                                return -1;
                        }
                        section_number++;
                }

                if(obj->is_verbose) {
                        fprintf(stdout, "all PAT section parsed\n");
                }
                obj->state = STATE_NEXT_PMT;
        }

        if(is_all_prog_parsed(obj)) {
                if(obj->is_verbose) {
                        fprintf(stdout, "no PMT section\n");
                }
                obj->rslt.is_pat_pmt_parsed = 1;
                obj->state = STATE_NEXT_PKT;
        }

        return 0;
}

static int state_next_pmt(struct obj *obj)
{
        struct ts *ts = &(obj->ts);
        struct ts_pid *pid;

        dbg(1, "search 0x%04X in pid_list", ts->PID);
        pid = (struct ts_pid *)list_search(&(obj->rslt.pid0), ts->PID);
        if((!pid) || (PMT_PID != pid->type)) {
                return -1; /* not PMT */
        }

        /* section parse has done in parse_TS_head()! */
        if(is_all_prog_parsed(obj)) {
                if(obj->is_verbose) {
                        fprintf(stdout, "all PMT section parsed\n");
                }
                obj->rslt.is_pat_pmt_parsed = 1;
                obj->state = STATE_NEXT_PKT;
        }

        return 0;
}

static int state_next_pkt(struct obj *obj)
{
        struct ts *ts = &(obj->ts);
        struct af *af = &(obj->af);
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_pid *pid = rslt->pid;
        struct ts_track *track = pid->track; /* may be NULL */
        struct ts_prog *prog = pid->prog; /* may be NULL */
        struct ts_error *err = &(rslt->err);
        struct ts_pkt *pkt = rslt->pkt;

        /* CC */
        if(pid->is_CC_sync) {
                uint8_t dCC;
                int lost;

                if((1 == ts->adaption_field_control) || /* 01 */
                   (3 == ts->adaption_field_control)) { /* 11 */
                        dCC = 1;
                }
                else { /* 00 or 10 */
                        dCC = 0;
                }

                pid->CC += dCC;
                pid->CC &= 0x0F; /* 4-bit */
                lost  = (int)ts->continuity_counter;
                lost -= (int)pid->CC;
                if(lost < 0) {
                        lost += 16;
                }

                rslt->CC_wait = pid->CC;
                rslt->CC_find = ts->continuity_counter;
                rslt->CC_lost = lost;
        }
        else {
                pid->is_CC_sync = 1;

                rslt->CC_wait = pid->CC;
                rslt->CC_find = ts->continuity_counter;
                rslt->CC_lost = 0;
        }
        pid->CC = ts->continuity_counter; /* update CC */
        err->Continuity_count_error = rslt->CC_lost;
        if(0x1FFF == ts->PID) {
                /* continuity_counter of null packet is undefined */
                err->Continuity_count_error = 0;
        }

        /* PCR flush */
        if(rslt->has_PCR) {
                rslt->PCR_base = af->program_clock_reference_base;
                rslt->PCR_ext  = af->program_clock_reference_extension;

                rslt->PCR  = rslt->PCR_base;
                rslt->PCR *= 300;
                rslt->PCR += rslt->PCR_ext;

                if(prog) {
                        if(!prog->STC_sync) {
                                /* use PCR as STC, suppose PCR_jitter is zero */
                                rslt->STC = rslt->PCR;
                                rslt->STC_base = rslt->STC / 300;
                                rslt->STC_ext = rslt->STC % 300;
                        }

                        /* PCR_interval (PCR packet arrive time interval) */
                        rslt->PCR_interval = lmt_min(rslt->STC, prog->PCRb, STC_OVF);
                        if((prog->STC_sync) &&
                           !(0 < rslt->PCR_interval && rslt->PCR_interval <= 40 * STC_MS)) {
                                /* !(0 < interval < +40ms) */
                                err->PCR_repetition_error = 1;
                        }

                        /* PCR_continuity (PCR value interval) */
                        rslt->PCR_continuity = lmt_min(rslt->PCR, prog->PCRb, STC_OVF);
                        if((prog->STC_sync) && !(af->discontinuity_indicator) &&
                           !(0 < rslt->PCR_continuity && rslt->PCR_continuity <= 100 * STC_MS)) {
                                /* !(0 < continuity < +100ms) */
                                err->PCR_discontinuity_indicator_error = 1;
                        }

                        /* PCR_jitter */
                        rslt->PCR_jitter = lmt_min(rslt->PCR, rslt->STC, STC_OVF);
                        if((prog->STC_sync) &&
                           !(-13 <= rslt->PCR_jitter && rslt->PCR_jitter <= +13)) {
                                /* !(-500ns < jitter < +500ns) */
                                err->PCR_accuracy_error = 1;
                        }

                        /* PCRa: the PCR packet before last PCR packet */
                        prog->PCRa = prog->PCRb;
                        prog->ADDa = prog->ADDb;

                        /* PCRb: last PCR packet */
                        prog->PCRb = rslt->PCR;
                        prog->ADDb = pkt->ADDR;

                        /* STC_sync */
                        if(!prog->STC_sync) {
                                int is_first_count_clear = 0;

                                if(pkt->mts) {
                                        /* clear count after 1st PCR */
                                        is_first_count_clear = 1;

                                        /* STC sync after 1st PCR */
                                        prog->STC_sync = 1;
                                }
                                else {
                                        /* TS */
                                        if(STC_OVF == prog->PCRa) {
                                                /* clear count after 1st PCR */
                                                is_first_count_clear = 1;
                                        }
                                        else {
                                                /* STC sync after 2nd PCR */
                                                prog->STC_sync = 1;
                                        }
                                }

                                /* first count clear */
                                if(is_first_count_clear &&
                                   prog->PCR_PID == rslt->prog0->PCR_PID) {
                                        struct lnode *lnode;

                                        for(lnode = (struct lnode *)(rslt->pid0); lnode; lnode = lnode->next) {
                                                struct ts_pid *pid_item = (struct ts_pid *)lnode;
                                                if(pid_item->prog == prog) {
                                                        pid_item->lcnt = 0;
                                                        pid_item->cnt = 0;
                                                }
                                        }

                                        rslt->last_sys_cnt = 0;
                                        rslt->sys_cnt = 0;

                                        rslt->last_psi_cnt = 0;
                                        rslt->psi_cnt = 0;

                                        rslt->last_nul_cnt = 0;
                                        rslt->nul_cnt = 0;

                                        rslt->last_interval = 0;
                                        rslt->interval = 0;
                                }
                        }

                        /* the packet that use PCR of first program */
                        if(prog->PCR_PID == rslt->prog0->PCR_PID) {
                                rslt->interval += lmt_min(prog->PCRb, prog->PCRa, STC_OVF);
                                if(rslt->interval >= rslt->aim_interval) {
                                        struct lnode *lnode;

                                        /* calc bitrate and clear the packet count */
                                        for(lnode = (struct lnode *)(rslt->pid0); lnode; lnode = lnode->next) {
                                                struct ts_pid *pid_item = (struct ts_pid *)lnode;
                                                pid_item->lcnt = pid_item->cnt;
                                                pid_item->cnt = 0;
                                        }

                                        rslt->last_sys_cnt = rslt->sys_cnt;
                                        rslt->sys_cnt = 0;

                                        rslt->last_psi_cnt = rslt->psi_cnt;
                                        rslt->psi_cnt = 0;

                                        rslt->last_nul_cnt = rslt->nul_cnt;
                                        rslt->nul_cnt = 0;

                                        rslt->last_interval = rslt->interval;
                                        rslt->interval = 0;

                                        rslt->has_rate = 1;

                                        rslt->is_psi_parse_finished = 1;
                                }
                        }
                }
                else {
                        fprintf(stderr, "No program use this PCR packet(0x%04X)!\n", ts->PID);
                }
        }

        /* for stream without PCR */
        if(0 == rslt->aim_interval) {
                rslt->is_psi_parse_finished = 1;
        }

        /* PES head & ES data */
        if(track && (0 == ts->transport_scrambling_control)) {
                if(AUD_PID == pid->type || AUD_PCR == pid->type ||
                   VID_PID == pid->type || VID_PCR == pid->type) {
                        parse_PES_head(obj);
                }

                if(rslt->has_PTS) {
                        rslt->PTS_interval = lmt_min(rslt->PTS, track->PTS, STC_BASE_OVF);
                        rslt->PTS_minus_STC = lmt_min(rslt->PTS, rslt->STC_base, STC_BASE_OVF);
                        track->PTS = rslt->PTS; /* record last PTS in track */
                }

                if(rslt->has_DTS) {
                        rslt->DTS_interval = lmt_min(rslt->DTS, track->DTS, STC_BASE_OVF);
                        rslt->DTS_minus_STC = lmt_min(rslt->DTS, rslt->STC_base, STC_BASE_OVF);
                        track->DTS = rslt->DTS; /* record last DTS in track */
                }
        }

        return 0;
}

static int parse_TS_head(struct obj *obj)
{
        uint8_t dat;
        struct ts *ts = &(obj->ts);
        struct af *af = &(obj->af);
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_error *err = &(rslt->err);
        struct ts_pid *pid; /* may be NULL */
        struct ts_prog *prog; /* may be NULL */
        struct ts_pkt *pkt = rslt->pkt;

        /* init rslt */
        rslt->is_psi_si = 0;
        rslt->has_section = 0;
        rslt->pid = NULL;
        rslt->has_rate = 0;
        rslt->has_PCR = 0;
        rslt->has_PTS = 0;
        rslt->has_DTS = 0;
        rslt->PES_len = 0; /* clear PES length */
        rslt->ES_len = 0; /* clear ES length */

        /* begin */
        dat = *(obj->p)++; obj->len--;
        ts->sync_byte = dat;
        if(0x47 != ts->sync_byte) {
                err->Sync_byte_error++;
                if(err->Sync_byte_error > 1) {
                        err->TS_sync_loss++;
                }
                fprintf(stderr, "sync_byte(0x%02X) error!\n", ts->sync_byte);
                dump(obj->rslt.pkt->ts, 188);
        }
        else {
                err->TS_sync_loss = 0;
                err->Sync_byte_error = 0;
        }

        dat = *(obj->p)++; obj->len--;
        ts->transport_error_indicator = (dat & BIT(7)) >> 7;
        ts->payload_unit_start_indicator = (dat & BIT(6)) >> 6;
        ts->transport_priority = (dat & BIT(5)) >> 5;
        ts->PID = dat & 0x1F;
        err->Transport_error = ts->transport_error_indicator;

        dat = *(obj->p)++; obj->len--;
        ts->PID <<= 8;
        ts->PID |= dat;

        dat = *(obj->p)++; obj->len--;
        ts->transport_scrambling_control = (dat & (BIT(7) | BIT(6))) >> 6;
        ts->adaption_field_control = (dat & (BIT(5) | BIT(4))) >> 4;;
        ts->continuity_counter = dat & 0x0F;

        if(0x00 == ts->adaption_field_control) {
                fprintf(stderr, "Bad adaption_field_control field(00)!\n");
        }

        if(BIT(1) & ts->adaption_field_control) {
                parse_AF(obj);
        }
        else {
                af->adaption_field_length = 0x00;
        }

        if(BIT(0) & ts->adaption_field_control) {
                /* data_byte, PSI or PES */
        }

        rslt->PID = ts->PID; /* record into rslt struct */
        dbg(1, "search 0x%04X in pid_list", rslt->PID);
        rslt->pid = (struct ts_pid *)list_search(&(obj->rslt.pid0), rslt->PID);
        if(!(rslt->pid)) {
                /* find new PID, add it in pid_list */
                rslt->pid = add_new_pid(obj);
        }
        pid = rslt->pid;

        /* calc STC, should be as early as possible */
        if(pkt->mts) {
                int64_t dCTS = lmt_min(pkt->MTS, rslt->lCTS, 0x40000000);

                rslt->STC += dCTS; /* got this STC */
                rslt->lCTS = pkt->MTS; /* record last CTS */
        }
        else {
                if(pid) {
                        prog = pid->prog;
                        if((prog) && 
                           (prog->STC_sync) &&
                           (prog->PCRa != prog->PCRb)) {
                                long double delta;

                                /* STCx - PCRb   ADDx - ADDb */
                                /* ----------- = ----------- */
                                /* PCRb - PCRa   ADDb - ADDa */
                                delta = (long double)lmt_min(prog->PCRb, prog->PCRa, STC_OVF);
                                delta *= (pkt->ADDR - prog->ADDb);
                                delta /= (prog->ADDb - prog->ADDa);
                                rslt->STC = lmt_add(prog->PCRb, (uint64_t)delta, STC_OVF);
                        }
                }
        }
        rslt->STC_base = rslt->STC / 300;
        rslt->STC_ext = rslt->STC % 300;

        /* statistic & PSI/SI section collect */
        pid->cnt++;
        rslt->sys_cnt++;
        rslt->nul_cnt += ((0x1FFF == ts->PID) ? 1 : 0);
        if((ts->PID < 0x0020) || (PMT_PID == pid->type)) {
                /* PSI/SI packet */
                rslt->psi_cnt++;
                rslt->is_psi_si = 1;
                /*fprintf(stderr, "PSI/SI: 0x%04X\n", ts->PID);*/
                section(obj);
        }

        return 0;
}

static int parse_AF(struct obj *obj)
{
        int i;
        uint8_t dat;
        struct af *af = &(obj->af);
        struct ts_rslt *rslt = &(obj->rslt);

        dat = *(obj->p)++; obj->len--;
        af->adaption_field_length = dat;
        if(0x00 == af->adaption_field_length) {
                return 0;
        }

        dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
        af->discontinuity_indicator = (dat & BIT(7)) >> 7;
        af->random_access_indicator = (dat & BIT(6)) >> 6;
        af->elementary_stream_priority_indicator = (dat & BIT(5)) >> 5;
        af->PCR_flag = (dat & BIT(4)) >> 4;
        af->OPCR_flag = (dat & BIT(3)) >> 3;
        af->splicing_pointer_flag = (dat & BIT(2)) >> 2;
        af->transport_private_data_flag = (dat & BIT(1)) >> 1;
        af->adaption_field_extension_flag = (dat & BIT(0)) >> 0;

        if(af->PCR_flag) {
                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_base = dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_base <<= 1;
                af->program_clock_reference_base |= ((dat & BIT(7)) >> 7);
                af->program_clock_reference_extension = ((dat & BIT(0)) >> 0);

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_extension <<= 8;
                af->program_clock_reference_extension |= dat;

                rslt->has_PCR = 1;
        }
        if(af->OPCR_flag) {
                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_base = dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_base <<= 8;
                af->original_program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_base <<= 8;
                af->original_program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_base <<= 8;
                af->original_program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_base <<= 1;
                af->original_program_clock_reference_base |= ((dat & BIT(7)) >> 7);
                af->original_program_clock_reference_extension = ((dat & BIT(0)) >> 0);

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_extension <<= 8;
                af->original_program_clock_reference_extension |= dat;
        }
        if(af->splicing_pointer_flag) {
                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->splice_countdown = dat;
        }
        if(af->transport_private_data_flag) {
                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->transport_private_data_length = dat;

                for(i = 0; i < af->transport_private_data_length; i++) {
                        af->private_data_byte[i] = dat;
                }
        }
        if(af->adaption_field_extension_flag) {
                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->adaption_field_extension_length = dat;

                /* pass adaption_field_extension part, FIXME */
                obj->p += af->adaption_field_extension_length;
                obj->len -= af->adaption_field_extension_length;
        }

        /* pass stuffing byte */
        obj->p += af->adaption_field_length;
        obj->len -= af->adaption_field_length;

        return 0;
}

static int section(struct obj *obj)
{
        uint8_t pointer_field;
        struct ts *ts = &(obj->ts);
        struct ts_psi *psi = &(obj->rslt.psi);
        struct ts_pid *pid = obj->rslt.pid;

        /* FIXME: if data after CRC_32 is NOT 0xFF, it's another section! */
        if(0 == pid->section_idx) {
                /* waiting for section head */
                if(1 == ts->payload_unit_start_indicator) {
                        pointer_field = *(obj->p)++; obj->len--;

                        /* section head */
                        obj->p += pointer_field;
                        obj->len -= pointer_field;
                        memcpy(pid->section, obj->p, obj->len);
                        pid->section_idx = obj->len;
                }
                else { /* (0 == ts->payload_unit_start_indicator) */
                        /* section async, ignore this packet! */
                        /*dump(obj->p, obj->len); */
                }
        }
        else { /* (0 != pid->section_idx) */
                /* collect section data */
                if(1 == ts->payload_unit_start_indicator) {
                        pointer_field = *(obj->p)++; obj->len--;

                        /* section tail */
                        memcpy(pid->section + pid->section_idx, obj->p, pointer_field);
                        pid->section_idx += pointer_field;
                        parse_table(obj);

                        /* section head */
                        obj->p += pointer_field;
                        obj->len -= pointer_field;
                        memcpy(pid->section, obj->p, obj->len);
                        pid->section_idx = obj->len;
                }
                else { /* (0 == ts->payload_unit_start_indicator) */
                        /* section body */
                        memcpy(pid->section + pid->section_idx, obj->p, 184);
                        pid->section_idx += 184;
                }
        }

        if(pid->section_idx >= 8) {
                /* PSI head OK */
                if(0 != parse_PSI_head(obj, pid->section)) {
                        pid->section_idx = 0;
                        return -1;
                }

                if(pid->section_idx >= (3 + psi->section_length)) {
                        /* section data enough */
                        parse_table(obj);
                        pid->section_idx = 0;
                }
        }

        return 0;
}

static int parse_table(struct obj *obj)
{
        uint8_t *p;
        struct ts_table *table;
        struct lnode **psection0;
        struct ts_section *section;
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_pid *pid = rslt->pid;
        struct ts_psi *psi = &(rslt->psi);
        struct ts_error *err = &(rslt->err);
        int is_new_version = 0;

        /* get psi head info */
        if(0 != parse_PSI_head(obj, pid->section)) {
                return -1;
        }

        /* CRC check */
        if(psi->check_CRC) {
                p = pid->section + 3 + psi->section_length - 4;
                pid->CRC_32   = *p++;
                pid->CRC_32 <<= 8;
                pid->CRC_32  |= *p++;
                pid->CRC_32 <<= 8;
                pid->CRC_32  |= *p++;
                pid->CRC_32 <<= 8;
                pid->CRC_32  |= *p++;

                pid->CRC_32_calc = CRC_for_TS(pid->section, 3 + psi->section_length - 4, 32);
                /*pid->CRC_32_calc = CRC(pid->section, 3 + psi->section_length - 4, 32); */
                /*pid->CRC_32_calc = crc32(pid->section, 3 + psi->section_length - 4); */
                if(pid->CRC_32_calc != pid->CRC_32) {
                        err->CRC_error = 1;
                        fprintf(stderr, "CRC error(0x%08X! 0x%08X?)\n",
                                pid->CRC_32_calc, pid->CRC_32);
                        dump(pid->section, 3 + psi->section_length);
                        return -1;
                }
        }

        /* get "table" and "psection0" */
        if(0x02 == psi->table_id) {
                /* PMT section */
                if(!(pid->prog)) {
                        DBG(ERR_OTHER, "PMT: pid->prog is NULL!\n");
                        return -1;
                }
                table = &(pid->prog->table_02);
        }
        else {
                /* other section */
                struct lnode **ptable0 = (struct lnode **)&(rslt->table0);

                dbg(1, "search 0x%02X in table_list", psi->table_id);
                table = (struct ts_table *)list_search(ptable0, psi->table_id);
                if(!table) {
                        /* add table */
                        table = (struct ts_table *)malloc(sizeof(struct ts_table));
                        if(!table) {
                                DBG(ERR_MALLOC_FAILED, "\n");
                                return -ERR_MALLOC_FAILED;
                        }

                        table->table_id = psi->table_id;
                        table->version_number = psi->version_number;
                        table->last_section_number = psi->last_section_number;
                        table->section0 = NULL;
                        table->STC = 0L;
                        dbg(1, "insert 0x%02X in table_list", psi->table_id);
                        list_set_key(table, psi->table_id);
                        list_insert(ptable0, table);
                }
        }
        psection0 = (struct lnode **)&(table->section0);

        /* new table version? */
        if(table->version_number != psi->version_number) {
                /* clear psection0 and update table parameter */
                dbg(1, "version_number(%d -> %d), free section",
                    table->version_number,
                    psi->version_number);
                table->version_number = psi->version_number;
                table->last_section_number = psi->last_section_number;
                list_free(psection0);
                is_new_version = 1;
        }

        /* get "section" pointer */
        dbg(1, "search 0x%02X in section_list", psi->section_number);
        section = (struct ts_section *)list_search(psection0, psi->section_number);
        if(!section) {
                /* add section */
                section = (struct ts_section *)malloc(sizeof(struct ts_section));
                if(!section) {
                        DBG(ERR_MALLOC_FAILED, "\n");
                        return -ERR_MALLOC_FAILED;
                }

                section->section_number = psi->section_number;
                section->section_length = psi->section_length;
                memcpy(section->data, pid->section, 3 + psi->section_length);
                dbg(1, "insert 0x%02X in section_list", psi->section_number);
                list_set_key(section, psi->section_number);
                list_insert(psection0, section);
        }
        else {
                /*fprintf(stderr, "has section %02X/%02X(table %02X)\n", */
                /*        psi->section_number, */
                /*        psi->last_section_number, */
                /*        psi->table_id); */
        }

        /* section_interval */
        pid->section_interval = lmt_min(rslt->STC, table->STC, STC_OVF);
        table->STC = rslt->STC;

        /* PAT_error(table_id error) */
        if(0x0000 == pid->PID && 0x00 != psi->table_id) {
                err->PAT_error = 1;
                dump(obj->rslt.pkt->ts, 188);
                dump(pid->section, 8);
                return -1;
        }

        /* parse */
        rslt->has_section = 1;
        switch(psi->table_id) {
                case 0x00:
                        if(0x0000 != pid->PID) {
                                fprintf(stderr, "PAT: PID is not 0x0000 but 0x%04X, ignore!\n", pid->PID);
                                return -1;
                        }
                        parse_PAT_load(obj, pid->section);
                        break;
                case 0x02:
                        if(PMT_PID != pid->type) {
                                fprintf(stderr, "PMT: PID is NOT PMT_PID but 0x%04X, ignore!\n", pid->PID);
                                return -1;
                        }
                        parse_PMT_load(obj, pid->section);
                        break;
                case 0x42:
                        if(0x0011 != pid->PID) {
                                fprintf(stderr, "SDT: PID is not 0x0011 but 0x%04X, ignore!\n", pid->PID);
                                return -1;
                        }
                        parse_SDT_load(obj, pid->section);
                        break;
                default:
                        break;
        }
        return 0;
}

static int parse_PSI_head(struct obj *obj, uint8_t *section)
{
        uint8_t *p;
        struct ts_psi *psi = &(obj->rslt.psi);
        const struct table_id_table *table;

        p = section;
        psi->table_id = *p++;
        psi->section_syntax_indicator = (*p & BIT(7)) >> 7;
        psi->private_indicator = (*p & BIT(6)) >> 6;
        psi->section_length   = *p++ & 0x0F;
        psi->section_length <<= 8;
        psi->section_length  |= *p++;
        if(1 == psi->section_syntax_indicator) {
                /* normal section syntax */
                psi->table_id_extension   = *p++;
                psi->table_id_extension <<= 8;
                psi->table_id_extension  |= *p++;
                psi->version_number = (*p & 0x3E) >> 1;
                psi->current_next_indicator = *p++ & BIT(0);
                psi->section_number = *p++;
                psi->last_section_number = *p++;

                table = table_type(psi->table_id);
                psi->check_CRC = table->check_CRC;
                psi->type = table->type;
        }
        else {
                /* abnormal section syntax */
                psi->table_id_extension = 0;
                psi->version_number = 0;
                psi->current_next_indicator = 0;
                psi->section_number = 0;
                psi->last_section_number = 0;

                psi->check_CRC = 0;
                psi->type = USR_PID;
        }

#if 0
        /* for debug only */
        fprintf(stderr, "table, 0x%02X, len %4d, sect, %d/%d\n",
                psi->table_id,
                psi->section_length,
                psi->section_number,
                psi->last_section_number);
#endif

        if(0 == psi->private_indicator) {
                /* normal section */
                if(psi->section_length > 1021) {
                        fprintf(stderr, "normal section(0x%02X), length(%d) overflow!\n",
                                psi->table_id,
                                psi->section_length);
                        psi->section_length = 1021;
                        dump(obj->rslt.pkt->ts, 188);
                        dump(section, 8);
                        return -1;
                }
        }
        else {
                /* private section */
                if(psi->section_length > 4093) {
                        fprintf(stderr, "private section(0x%02X), length(%d) overflow!\n",
                                psi->table_id,
                                psi->section_length);
                        psi->section_length = 4093;
                        dump(obj->rslt.pkt->ts, 188);
                        dump(section, 8);
                        return -1;
                }
        }

        return 0;
}

static int parse_PAT_load(struct obj *obj, uint8_t *section)
{
        uint8_t dat;
        uint8_t *p = section + 8;
        struct ts_psi *psi = &(obj->rslt.psi);
        int len = psi->section_length - 5;
        struct ts_prog *prog;
        struct ts *ts = &(obj->ts);
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_pid *pid = rslt->pid;
        struct ts_error *err = &(rslt->err);
        struct ts_pid ts_new_pid, *new_pid = &ts_new_pid;

        /* PAT_error */
        if(pid->section_interval > 500 * STC_MS) {
                err->PAT_error = 1;
        }
        if(0x00 != ts->transport_scrambling_control) {
                err->PAT_error = 1;
        }

        /* to avoid stack overflow, FIXME */
        if(rslt->prog0) {
                return 0;
        }

        /* in PAT, table_id_extension is transport_stream_id */
        rslt->transport_stream_id = psi->table_id_extension;
        rslt->has_got_transport_stream_id = 1;

        while(len > 4) {
                /* add program */
                prog = (struct ts_prog *)malloc(sizeof(struct ts_prog));
                if(!prog) {
                        DBG(ERR_MALLOC_FAILED, "\n");
                        return -ERR_MALLOC_FAILED;
                }

                dat = *p++; len--;
                prog->program_number = dat;

                dat = *p++; len--;
                prog->program_number <<= 8;
                prog->program_number |= dat;

                dat = *p++; len--;
                prog->PMT_PID = dat & 0x1F;

                dat = *p++; len--;
                prog->PMT_PID <<= 8;
                prog->PMT_PID |= dat;
                new_pid->PID = prog->PMT_PID;

                if(0 == prog->program_number) {
                        /* network PID, not a program */
                        new_pid->type = NIT_PID;

                        if(0x0010 != new_pid->PID) {
#if 0
                                fprintf(stderr, "NIT_PID(0x%04X) is NOT 0x0010!\n", new_pid->PID);
#endif
                        }
                        free(prog);
                }
                else {
                        struct lnode *lnode;

                        new_pid->type = PMT_PID;

                        if(!(rslt->prog0)) {
                                /* traverse pid_list */
                                /* if it des not belong to any program, use prog0 */
                                for(lnode = (struct lnode *)(rslt->pid0); lnode; lnode = lnode->next) {
                                        struct ts_pid *pid_item = (struct ts_pid *)lnode;
                                        if(pid_item->PID < 0x0020 || pid_item->PID == 0x1FFF) {
                                                pid_item->prog = prog;
                                        }
                                }
                        }

                        /* PMT table */
                        prog->is_parsed = 0;
                        prog->table_02.table_id = 0x02;
                        prog->table_02.version_number = 0xFF; /* never reached version */
                        prog->table_02.last_section_number = 0; /* no use */
                        prog->table_02.section0 = NULL;
                        prog->table_02.STC = 0L;

                        /* track list */
                        prog->track0 = NULL;

                        /* for time */
                        prog->ADDa = 0;
                        prog->PCRa = STC_OVF;
                        prog->ADDb = 0;
                        prog->PCRb = STC_OVF;
                        prog->STC_sync = 0;

                        /* SDT info */
                        prog->service_name_len = 0;
                        prog->service_name[0] = '\0';
                        prog->service_provider_len = 0;
                        prog->service_provider[0] = '\0';

                        dbg(1, "insert 0x%04X in prog_list", prog->program_number);
                        list_set_key(prog, prog->program_number);
                        list_insert(&(rslt->prog0), prog);
                }

                new_pid->cnt = 0;
                new_pid->lcnt = 0;
                new_pid->prog = prog;
                new_pid->track = NULL;
                new_pid->CC = 0;
                new_pid->is_CC_sync = 0;
                new_pid->sdes = PID_TYPE[new_pid->type].sdes;
                new_pid->ldes = PID_TYPE[new_pid->type].ldes;
                new_pid->is_video = 0;
                new_pid->is_audio = 0;
                add_to_pid_list(&(rslt->pid0), new_pid); /* NIT or PMT PID */
        }

        return 0;
}

static int parse_PMT_load(struct obj *obj, uint8_t *section)
{
        uint8_t dat;
        uint8_t *p = section + 8;
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_psi *psi = &(rslt->psi);
        int len = psi->section_length - 5;
        struct ts_prog *prog;
        struct ts_track *track;
        struct ts *ts = &(obj->ts);
        struct ts_pid *pid = rslt->pid;
        struct ts_error *err = &(rslt->err);
        struct ts_pid ts_new_pid, *new_pid = &ts_new_pid;

        /* PMT_error */
        if(pid->section_interval > 500 * STC_MS) {
                err->PMT_error = 1;
        }
        if(0x00 != ts->transport_scrambling_control) {
                err->PAT_error = 1;
        }

        /* in PMT, table_id_extension is program_number */
        dbg(1, "search 0x%04X in prog_list", psi->table_id_extension);
        prog = (struct ts_prog *)list_search(&(obj->rslt.prog0), psi->table_id_extension);
        if((!prog) || (prog->is_parsed)) {
                return -1; /* parsed program, ignore */
        }

        obj->rslt.is_psi_si = 1;
        prog->is_parsed = 1;

        dat = *p++; len--;
        prog->PCR_PID = dat & 0x1F;

        dat = *p++; len--;
        prog->PCR_PID <<= 8;
        prog->PCR_PID |= dat;

        /* add PCR PID */
        new_pid->PID = prog->PCR_PID;
        new_pid->cnt = 0;
        new_pid->lcnt = 0;
        new_pid->prog = prog;
        new_pid->track = NULL;
        new_pid->type = PCR_PID;
        new_pid->CC = 0;
        new_pid->is_CC_sync = 1;
        new_pid->sdes = PID_TYPE[new_pid->type].sdes;
        new_pid->ldes = PID_TYPE[new_pid->type].ldes;
        new_pid->is_video = 0;
        new_pid->is_audio = 0;
        add_to_pid_list(&(obj->rslt.pid0), new_pid); /* PCR_PID */

        /* program_info_length */
        dat = *p++; len--;
        prog->program_info_len = dat & 0x0F;

        dat = *p++; len--;
        prog->program_info_len <<= 8;
        prog->program_info_len |= dat;

        /* record program_info */
        if(prog->program_info_len > INFO_LEN_MAX) {
                fprintf(stderr, "PID(0x%04X): program_info_length(%d) too big!\n",
                        obj->ts.PID, prog->program_info_len);
        }

        /* record program_info */
        memcpy(prog->program_info, p, prog->program_info_len);
        p += prog->program_info_len;
        len -= prog->program_info_len;

        while(len > 4) {
                /* add track */
                track = (struct ts_track *)malloc(sizeof(struct ts_track));
                if(!track) {
                        fprintf(stderr, "Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }

                dat = *p++; len--;
                track->stream_type = dat;

                dat = *p++; len--;
                track->PID = dat & 0x1F;

                dat = *p++; len--;
                track->PID <<= 8;
                track->PID |= dat;

                /* ES_info_length */
                dat = *p++; len--;
                track->es_info_len = dat & 0x0F;

                dat = *p++; len--;
                track->es_info_len <<= 8;
                track->es_info_len |= dat;

                /* record es_info */
                if(track->es_info_len > INFO_LEN_MAX) {
                        fprintf(stderr, "PID(0x%04X): ES_info_length(%d) too big!\n",
                                track->PID, track->es_info_len);
                }

                /* record es_info */
                memcpy(track->es_info, p, track->es_info_len);
                p += track->es_info_len;
                len -= track->es_info_len;

                track_type(track);
                if(track->PID == prog->PCR_PID) {
                        switch(track->type) {
                                case VID_PID: track->type = VID_PCR; break;
                                case AUD_PID: track->type = AUD_PCR; break;
                                default:      track->type = UNO_PCR; break;
                        }
                }
                track->PTS = STC_BASE_OVF;
                track->DTS = STC_BASE_OVF;

                track->is_pes_align = 0;

                dbg(1, "push 0x%04X in track_list", track->PID);
                list_push(&(prog->track0), track);

                /* add track PID */
                new_pid->PID = track->PID;
                new_pid->cnt = 0;
                new_pid->lcnt = 0;
                new_pid->prog = prog;
                new_pid->track = track;
                new_pid->type = track->type;
                new_pid->CC = 0;
                new_pid->is_CC_sync = 0;
                new_pid->sdes = PID_TYPE[new_pid->type].sdes;
                new_pid->ldes = PID_TYPE[new_pid->type].ldes;
                switch(new_pid->type) {
                        case VID_PID:
                        case VID_PCR:
                                new_pid->is_video = 1;
                                new_pid->is_audio = 0;
                                break;
                        case AUD_PID:
                        case AUD_PCR:
                                new_pid->is_video = 0;
                                new_pid->is_audio = 1;
                                break;
                        default:
                                new_pid->is_video = 0;
                                new_pid->is_audio = 0;
                                break;
                }
                add_to_pid_list(&(obj->rslt.pid0), new_pid); /* elementary_PID */
        }

        return 0;
}

static int parse_SDT_load(struct obj *obj, uint8_t *section)
{
        uint8_t dat;
        uint8_t *p = section + 8;
        struct ts_psi *psi = &(obj->rslt.psi);
        int len = psi->section_length - 5;
        struct ts_rslt *rslt = &(obj->rslt);
        uint16_t original_network_id;

        /* in SDT, table_id_extension is transport_stream_id */
        if(rslt->has_got_transport_stream_id &&
           psi->table_id_extension != rslt->transport_stream_id) {
                fprintf(stderr, "table_id(0x%02X): table_id_extension(%d) != transport_stream_id(%d)\n",
                        psi->table_id,
                        psi->table_id_extension,
                        rslt->transport_stream_id);
                return -1; /* bad SDT table, ignore */
        }

        dat = *p++; len--;
        original_network_id = dat;

        dat = *p++; len--;
        original_network_id <<= 8;
        original_network_id |= dat;
        if(rslt->has_got_transport_stream_id &&
           original_network_id != rslt->transport_stream_id) {
#if 0
                fprintf(stderr, "table_id(0x%02X): original_network_id(%d) != transport_stream_id(%d)\n",
                        psi->table_id,
                        original_network_id,
                        rslt->transport_stream_id);
                return -1; /* bad SDT table, ignore */
#endif
        }

        dat = *p++; len--; /* reserved_future_use */

        while(len > 4) {
                struct ts_prog *prog;

                uint16_t service_id;
                uint8_t EIT_schedule_flag; /* 1-bit */
                uint8_t EIT_present_following_flag; /* 1-bit */
                uint8_t running_status; /* 3-bit */
                uint8_t free_CA_mode; /* 1-bit */
                uint16_t descriptors_loop_length; /* 12-bit */

                dat = *p++; len--;
                service_id = dat;

                dat = *p++; len--;
                service_id <<= 8;
                service_id |= dat;
                dbg(1, "search service_id(0x%04X) in prog_list", service_id);
                prog = (struct ts_prog *)list_search(&(rslt->prog0), service_id);

                dat = *p++; len--;
                EIT_schedule_flag = (dat & BIT(1)) >> 1;
                EIT_present_following_flag = (dat & BIT(0)) >> 0;

                dat = *p++; len--;
                running_status = (dat & 0xE0) >> 5;
                free_CA_mode = (dat & BIT(4)) >> 4;
                descriptors_loop_length = (dat & 0x0F);

                dat = *p++; len--;
                descriptors_loop_length <<= 8;
                descriptors_loop_length |= dat;

                while(descriptors_loop_length > 0) {
                        uint8_t tag;
                        uint8_t length;

                        tag = *p++; len--; descriptors_loop_length--;
                        length = *p++; len--; descriptors_loop_length--;
                        if((0xFF == tag) || (0 == length)) {
                                return -1; /* wrong descriptor */
                        }
                        if(0x48 == tag && prog) {
                                uint8_t *pt = p;
                                uint8_t type;

                                /* service_descriptor */
                                type = *pt++; /* ignore */
                                prog->service_provider_len = *pt++;
                                memcpy(prog->service_provider, pt, prog->service_provider_len);
                                prog->service_provider[prog->service_provider_len] = '\0';

                                pt += prog->service_provider_len; /* pass provider */
                                prog->service_name_len = *pt++;
                                memcpy(prog->service_name, pt, prog->service_name_len);
                                prog->service_name[prog->service_name_len] = '\0';
                        }
                        p += length; len -= length; descriptors_loop_length -= length;
                }
        }

        return 0;
}

static int parse_PES_head(struct obj *obj)
{
        uint8_t dat;
        struct ts *ts = &(obj->ts);
        struct pes *pes = &(obj->pes);
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_pid *pid = rslt->pid;
        struct ts_track *track = pid->track; /* may be NULL */

        /* for some bad stream */
        if(ts->PID < 0x0020) {
                fprintf(stderr, "PES: bad PID(0x%04X)!\n", ts->PID);
                return -1;
        }
        if(!track) {
                fprintf(stderr, "PES: not track PID(0x%04X)!\n", ts->PID);
                return -1;
        }

        /* PES align */
        if(ts->payload_unit_start_indicator) {
                track->is_pes_align = 1;
        }

        /* record PES data */
        if(obj->need_pes_align) {
                if(track->is_pes_align) {
                        rslt->PES_len = obj->len;
                        rslt->PES_buf = obj->p;
                }
                else {
                        /* ignore these PES data */
                }
        }
        else {
                rslt->PES_len = obj->len;
                rslt->PES_buf = obj->p;
        }

        /* PES head */
        if(ts->payload_unit_start_indicator) {

                /* PES head start */
                dat = *(obj->p)++; obj->len--;
                pes->packet_start_code_prefix = dat;

                dat = *(obj->p)++; obj->len--;
                pes->packet_start_code_prefix <<= 8;
                pes->packet_start_code_prefix |= dat;

                dat = *(obj->p)++; obj->len--;
                pes->packet_start_code_prefix <<= 8;
                pes->packet_start_code_prefix |= dat;

                if(0x000001 != pes->packet_start_code_prefix) {
                        fprintf(stderr, "PES packet start code prefix(0x%06X) NOT 0x000001!\n",
                                pes->packet_start_code_prefix);
                        dump(obj->rslt.pkt->ts, 188);
#if 0
                        return -1;
#endif
                }

                dat = *(obj->p)++; obj->len--;
                pes->stream_id = dat;

                dat = *(obj->p)++; obj->len--;
                pes->PES_packet_length = dat;

                dat = *(obj->p)++; obj->len--;
                pes->PES_packet_length <<= 8;
                pes->PES_packet_length |= dat; /* 0x0000 for many video pes */
#if 0
                fprintf(stderr, "PES_packet_length = %d(0x%X)\n",
                        pes->PES_packet_length,
                        pes->PES_packet_length);
#endif

                parse_PES_head_switch(obj);
        }

        /* record ES data */
        if(obj->need_pes_align) {
                if(track->is_pes_align) {
                        rslt->ES_len = obj->len;
                        rslt->ES_buf = obj->p;
                }
                else {
                        /* ignore these ES data */
                }
        }
        else {
                rslt->ES_len = obj->len;
                rslt->ES_buf = obj->p;
        }

        return 0;
}

static int parse_PES_head_switch(struct obj *obj)
{
        struct pes *pes = &(obj->pes);

        switch(pes->stream_id) {
                case 0xBE: /* padding_stream */
                        /* subsequent pes->PES_packet_length data is padding_byte, pass */
                        if(pes->PES_packet_length > obj->len) {
                                fprintf(stderr, "PES_packet_length(%d) for padding_stream is too large!\n",
                                        pes->PES_packet_length);
                                return -1;
                        }
                        obj->p += pes->PES_packet_length;
                        obj->len -= pes->PES_packet_length;
                        break;
                case 0xBC: /* program_stream_map */
                case 0xBF: /* private_stream_2 */
                case 0xF0: /* ECM */
                case 0xF1: /* EMM */
                case 0xFF: /* program_stream_directory */
                case 0xF2: /* DSMCC_stream */
                case 0xF8: /* ITU-T Rec. H.222.1 type E stream */
                        /* pes->PES_packet_length data in pes->buf is PES_packet_data_byte */
                        /* record after return */
                        break;
                default:
                        parse_PES_head_detail(obj);
                        break;
        }

        return 0;
}

static int parse_PES_head_detail(struct obj *obj)
{
        int i;
        int header_data_len;
        uint8_t dat;
        struct pes *pes = &(obj->pes);
        struct ts_rslt *rslt = &(obj->rslt);

        dat = *(obj->p)++; obj->len--;
        pes->PES_scrambling_control = (dat & (BIT(5) | BIT(4))) >> 4;
        pes->PES_priority = (dat & BIT(3)) >> 3;
        pes->data_alignment_indicator = (dat & BIT(2)) >> 2;
        pes->copyright = (dat & BIT(1)) >> 1;
        pes->original_or_copy = (dat & BIT(0)) >> 0;

        dat = *(obj->p)++; obj->len--;
        pes->PTS_DTS_flags = (dat & (BIT(7) | BIT(6))) >> 6;
        pes->ESCR_flag = (dat & BIT(5)) >> 5;
        pes->ES_rate_flag = (dat & BIT(4)) >> 4;
        pes->DSM_trick_mode_flag = (dat & BIT(3)) >> 3;
        pes->additional_copy_info_flag = (dat & BIT(2)) >> 2;
        pes->PES_CRC_flag = (dat & BIT(1)) >> 1;
        pes->PES_extension_flag = (dat & BIT(0)) >> 0;

        dat = *(obj->p)++; obj->len--;
        pes->PES_header_data_length = dat;
        header_data_len = pes->PES_header_data_length;
        if(header_data_len > obj->len) {
                fprintf(stderr, "PES_header_data_length(%d) is too long!\n",
                        header_data_len);
                return -1;
        }

        /* get PTS, DTS */
        if(0x02 == pes->PTS_DTS_flags) { /* '10' */
                /* PTS */
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS = (dat & (BIT(3) | BIT(2) | BIT(1))) >> 1;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS <<= 8;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->PTS <<= 7;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS <<= 8;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->PTS <<= 7;
                pes->PTS |= dat;

                rslt->has_PTS = 1;
                rslt->PTS = pes->PTS;
        }
        else if(0x03 == pes->PTS_DTS_flags) { /* '11' */
                /* PTS */
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS = (dat & (BIT(3) | BIT(2) | BIT(1))) >> 1;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS <<= 8;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->PTS <<= 7;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS <<= 8;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->PTS <<= 7;
                pes->PTS |= dat;

                rslt->has_PTS = 1;
                rslt->PTS = pes->PTS;

                /* DTS */
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->DTS = (dat & (BIT(3) | BIT(2) | BIT(1))) >> 1;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->DTS <<= 8;
                pes->DTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->DTS <<= 7;
                pes->DTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->DTS <<= 8;
                pes->DTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->DTS <<= 7;
                pes->DTS |= dat;

                rslt->has_DTS = 1;
                rslt->DTS = pes->DTS;
        }
        else if(0x01 == pes->PTS_DTS_flags) { /* '01' */
                fprintf(stderr, "PTS_DTS_flags error!\n");
                dump(obj->rslt.pkt->ts, 188);
                return -1;
        }
        else {
                /* no PTS, no DTS, it's OK! */
        }

        if(pes->ESCR_flag) {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_base = (dat & (BIT(5) | BIT(4) | BIT(3))) >> 3;
                pes->ESCR_base <<= 2;
                pes->ESCR_base |= (dat & (BIT(1) | BIT(0)));

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_base <<= 8;
                pes->ESCR_base |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_base <<= 5;
                pes->ESCR_base |= ((dat & (BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3))) >> 3);
                pes->ESCR_base <<= 2;
                pes->ESCR_base |= (dat & (BIT(1) | BIT(0)));

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_base <<= 8;
                pes->ESCR_base |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_base <<= 5;
                pes->ESCR_base |= ((dat & (BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3))) >> 3);
                pes->ESCR_extension = (dat & (BIT(1) | BIT(0)));

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_extension <<= 7;
                pes->ESCR_extension |= (dat >> 1);
        }

        if(pes->ES_rate_flag) {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ES_rate = (dat & 0x7F);

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ES_rate <<= 8;
                pes->ES_rate |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ES_rate <<= 7;
                pes->ES_rate |= (dat >> 1);
        }

        if(pes->DSM_trick_mode_flag) {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->trick_mode_control = (dat & (BIT(7) | BIT(6) | BIT(5))) >> 5;

                if(0x00 == pes->trick_mode_control) {
                        /* '000', fast_forward */
                        pes->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                        pes->intra_slice_refresh = (dat & (BIT(2))) >> 2;
                        pes->frequency_truncation = (dat & (BIT(1) | BIT(0)));
                }
                else if(0x01 == pes->trick_mode_control) {
                        /* '001', slow_motion */
                        pes->rep_cntrl = (dat & 0x1F);
                }
                else if(0x02 == pes->trick_mode_control) {
                        /* '010', freeze_frame */
                        pes->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                }
                else if(0x03 == pes->trick_mode_control) {
                        /* '011', fast_reverse */
                        pes->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                        pes->intra_slice_refresh = (dat & (BIT(2))) >> 2;
                        pes->frequency_truncation = (dat & (BIT(1) | BIT(0)));
                }
                else if(0x04 == pes->trick_mode_control) {
                        /* '100', slow_reverse */
                        pes->rep_cntrl = (dat & 0x1F);
                }
        }

        if(pes->additional_copy_info_flag) {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->additional_copy_info = (dat & 0x7F);
        }

        if(pes->PES_CRC_flag) {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->previous_PES_packet_CRC = dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->previous_PES_packet_CRC <<= 8;
                pes->previous_PES_packet_CRC |= dat;
        }

        if(pes->PES_extension_flag) {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PES_private_data_flag = (dat & (BIT(7))) >> 7;
                pes->pack_header_field_flag = (dat & (BIT(6))) >> 6;
                pes->program_packet_sequence_counter_flag = (dat & (BIT(5))) >> 5;
                pes->P_STD_buffer_flag = (dat & (BIT(4))) >> 4;
                pes->PES_extension_flag_2 = (dat & (BIT(0))) >> 0;

                if(pes->PES_private_data_flag) {
                        for(i = 16; i > 0; i--) {
                                dat = *(obj->p)++; obj->len--; header_data_len--;
                                pes->PES_private_data[i] = dat;
                        }
                }

                if(pes->pack_header_field_flag) {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->pack_field_length = dat;

                        /* pass pack_header() */
                        for(i = pes->pack_field_length; i > 0; i--) {
                                dat = *(obj->p)++; obj->len--; header_data_len--;
                        }
                }

                if(pes->program_packet_sequence_counter_flag) {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->program_packet_sequence_counter = (dat & 0x7F);

                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->MPEG1_MPEG2_identifier = (dat & (BIT(6))) >> 6;
                        pes->original_stuff_length = (dat & 0x3F);
                }

                if(pes->P_STD_buffer_flag) {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->P_STD_buffer_scale = (dat & (BIT(5))) >> 5;
                        pes->P_STD_buffer_size = (dat & 0x1F);

                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->P_STD_buffer_size <<= 8;
                        pes->P_STD_buffer_size |= dat;
                }

                if(pes->PES_extension_flag_2) {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->PES_extension_field_length = (dat & 0x7F);

                        /* pass PES_extension_field */
                        for(i = pes->PES_extension_field_length; i > 0; i--) {
                                dat = *(obj->p)++; obj->len--; header_data_len--;
                        }
                }
        }

        /* pass stuffing_byte */
        for(; header_data_len > 0; header_data_len--) {
                dat = *(obj->p)++; obj->len--;
        }

        /* subsequent obj->len data is ES data */

        return 0;
}

static struct ts_pid *add_new_pid(struct obj *obj)
{
        struct ts_pid ts_pid, *pid;
        struct ts_rslt *rslt = &(obj->rslt);

        pid = &ts_pid;

        pid->PID = rslt->PID;
        pid->type = pid_type(pid->PID);
        if((rslt->prog0) && 
           (pid->PID < 0x0020 || pid->PID == 0x1FFF)) {
                pid->prog = rslt->prog0;
        }
        else {
                pid->prog = NULL;
        }
        pid->track = NULL;
        pid->cnt = 1;
        pid->lcnt = 0;
        /*pid->CC = ts->continuity_counter; */
        pid->is_CC_sync = 0;
        pid->sdes = PID_TYPE[pid->type].sdes;
        pid->ldes = PID_TYPE[pid->type].ldes;
        pid->is_video = 0;
        pid->is_audio = 0;

        return add_to_pid_list(&(rslt->pid0), pid); /* other_PID */
}

static struct ts_pid *add_to_pid_list(struct ts_pid **phead, struct ts_pid *the_pid)
{
        struct ts_pid *pid;

        dbg(1, "search 0x%04X in pid_list", the_pid->PID);
        pid = (struct ts_pid *)list_search(phead, the_pid->PID);
        if(pid) {
                /* in pid_list already, just update information */
                pid->PID = the_pid->PID;
                pid->prog = the_pid->prog;
                pid->track = the_pid->track;
                pid->type = the_pid->type;
                pid->is_video = the_pid->is_video;
                pid->is_audio = the_pid->is_audio;
                pid->cnt = the_pid->cnt;
                pid->lcnt = the_pid->lcnt;
                pid->CC = the_pid->CC;
                pid->is_CC_sync = the_pid->is_CC_sync;
                pid->sdes = the_pid->sdes;
                pid->ldes = the_pid->ldes;
        }
        else {
                pid = (struct ts_pid *)malloc(sizeof(struct ts_pid));
                if(!pid) {
                        DBG(ERR_MALLOC_FAILED, "\n");
                        return NULL;
                }

                pid->PID = the_pid->PID;
                pid->prog = the_pid->prog;
                pid->track = the_pid->track;
                pid->type = the_pid->type;
                pid->is_video = the_pid->is_video;
                pid->is_audio = the_pid->is_audio;
                pid->cnt = the_pid->cnt;
                pid->lcnt = the_pid->lcnt;
                pid->CC = the_pid->CC;
                pid->is_CC_sync = the_pid->is_CC_sync;
                pid->sdes = the_pid->sdes;
                pid->ldes = the_pid->ldes;
                pid->section_idx = 0; /* wait to sync with section head */
                pid->fd = NULL;

                dbg(1, "insert 0x%04X in pid_list", the_pid->PID);
                list_set_key(pid, the_pid->PID);
                list_insert(phead, pid);
        }
        return pid;
}

static int is_all_prog_parsed(struct obj *obj)
{
        uint8_t section_number;
        struct lnode *lnode_p; /* lnode of program list */

        for(lnode_p = (struct lnode *)(obj->rslt.prog0); lnode_p; lnode_p = lnode_p->next) {
                struct ts_prog *prog = (struct ts_prog *)lnode_p;
                struct ts_table *table_02 = &(prog->table_02);
                struct lnode *lnode_s; /* lnode of section list */

                if(0xFF == table_02->version_number) {
                        return 0;
                }

                section_number = 0;
                for(lnode_s = (struct lnode *)(table_02->section0); lnode_s; lnode_s = lnode_s->next) {
                        struct ts_section *section = (struct ts_section *)lnode_s;

                        if(section_number > table_02->last_section_number) {
                                return 0;
                        }
                        if(section_number != section->section_number) {
                                return 0;
                        }
                        section_number++;
                }
        }
        return 1;
}

static int pid_type(uint16_t pid)
{
        const struct ts_pid_table *p;

        for(p = PID_TABLE; p->type != BAD_PID; p++) {
                if(p->min <= pid && pid <= p->max) {
                        break;
                }
        }

        return p->type;
}

static const struct table_id_table *table_type(uint8_t id)
{
        const struct table_id_table *p;

        for(p = TABLE_ID_TABLE; p->min != 0xFF; p++) {
                if(p->min <= id && id <= p->max) {
                        break;
                }
        }

        return p;
}

static int track_type(struct ts_track *track)
{
        const struct stream_type *p;

        for(p = STREAM_TYPE_TABLE; 0xFF != p->stream_type; p++) {
                if(p->stream_type == track->stream_type) {
                        break;
                }
        }

        track->type = p->type;
        track->sdes = p->sdes;
        track->ldes = p->ldes;
        return 0;
}

/* rslt(int) = t1(uint) + t2(uint), t belongs to [0, ovf - 1] */
static int64_t lmt_add(int64_t t1, int64_t t2, int64_t ovf)
{
        /* ((0 <= t1 < ovf) && (0 <= t2 < ovf))? */
        if(t1 < 0 || ovf <= t1 || t2 < 0 || ovf <= t2) {
                /* fprintf(stderr, "lmt_add: bad overflow value(%llu) for %llu and %llu\n", ovf, t1, t2); */
                return 0;
        }

        t1 += t2;
        t1 -= ((t1 >= ovf) ? ovf : 0);

        return t1;
}

/* rslt(int) = t1(uint) - t2(uint), t belongs to [0, ovf - 1] */
static int64_t lmt_min(int64_t t1, int64_t t2, int64_t ovf)
{
        /* ((0 <= t1 < ovf) && (0 <= t2 < ovf))? */
        if(t1 < 0 || ovf <= t1 || t2 < 0 || ovf <= t2) {
                /* fprintf(stderr, "lmt_min: bad overflow value(%llu) for %llu and %llu\n", ovf, t1, t2); */
                return 0;
        }

        t1 += ((t1 < t2) ? ovf : 0);
        t1 -= t2;
        if(t1 >= (ovf >> 1)) {
                t1 -= ovf;
        }

        return t1;
}

static int dump(uint8_t *buf, int len)
{
        uint8_t *p = buf;
        int i;

        for(i = 0; i < len; i++) {
                fprintf(stderr, "%02X ", *p++);
        }
        fprintf(stderr, "\n");

        return 0;
}
