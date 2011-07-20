/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: ts.c
 * funx: analyse ts stream
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* for uint?_t, etc */
#include <string.h> /* for memset, memcpy, etc */

#include "libts/error.h"
#include "libts/crc.h"
#include "libts/ts.h"

#define BIT(n)                          (1<<(n))

#define STC_BASE_MS                     (90) /* 90 clk == 1(ms) */
#define STC_BASE_1S                     (90 * 1000) /* do NOT use 1e3 */
#define STC_BASE_OVF                    (1LL << 33)         /* 0x0200000000 */

#define STC_US                          (27)               /* 27 clk == 1(us) */
#define STC_MS                          (27 * 1000)        /* do NOT use 1e3  */
#define STC_1S                          (27 * 1000 * 1000) /* do NOT use 1e3  */
#define STC_OVF                         (STC_BASE_OVF * 300) /* 2576980377600 */

typedef struct _ts_t
{
        uint8_t sync_byte;
        uint8_t transport_error_indicator; /* 1-bit */
        uint8_t payload_unit_start_indicator; /* 1-bit */
        uint8_t transport_priority; /* 1-bit */
        uint16_t PID; /* 13-bit */
        uint8_t transport_scrambling_control; /* 2-bit */
        uint8_t adaption_field_control; /* 2-bit */
        uint8_t continuity_counter; /* 4-bit */
}
ts_t;

typedef struct _af_t
{
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
        /* ... */
}
af_t;

typedef struct _pes_t
{
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
}
pes_t;

typedef struct _pid_type_table_t
{
        char *sdes; /* short description */
        char *ldes; /* long description */
}
pid_type_table_t;

typedef struct _ts_pid_table_t
{
        uint16_t min; /* PID range */
        uint16_t max; /* PID range */
        int     type; /* index of item in PID_TYPE[] */
}
ts_pid_table_t;

typedef struct _table_id_table_t
{
        uint8_t min; /* table ID range */
        uint8_t max; /* table ID range */
        int check_CRC; /* some table do not need to check CRC_32 */
        int type; /* index of item in PID_TYPE[] */
}
table_id_table_t;

typedef struct _stream_type_t
{
        uint8_t stream_type;
        int   type; /* index of item in PID_TYPE[] */
        char *sdes; /* short description */
        char *ldes; /* long description */
}
stream_type_t;

typedef struct _obj_t
{
        int is_first_pkt;

        uint8_t *p; /* point to rslt.line */
        int len;

        ts_t ts;
        af_t af;
        pes_t pes;

        uint32_t pkt_size; /* 188 or 204 */
        int state;

        int is_pes_align; /* met first PES head */

        ts_rslt_t rslt;
}
obj_t;

enum PID_TYPE_ENUM
{
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

static const pid_type_table_t PID_TYPE[] =
{
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

static const ts_pid_table_t PID_TABLE[] =
{
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

static const table_id_table_t TABLE_ID_TABLE[] =
{
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

static const stream_type_t STREAM_TYPE_TABLE[] =
{
        {0x00, RSV_PID, "Reserved", "ITU-T|ISO/IEC Reserved"},
        {0x01, VID_PID, "MPEG-1", "ISO/IEC 11172-2 Video"},
        {0x02, VID_PID, "MPEG-2", "ITU-T Rec.H.262|ISO/IEC 13818-2 Video or MPEG-1 parameter limited"},
        {0x03, AUD_PID, "MPEG-1", "ISO/IEC 11172-3 Audio"},
        {0x04, AUD_PID, "MPEG-2", "ISO/IEC 13818-3 Audio"},
        {0x05, USR_PID, "private", "ITU-T Rec.H.222.0|ISO/IEC 13818-1 private_sections"},
        {0x06, AUD_PID, "TT|AC3", "ITU-T Rec.H.222.0|ISO/IEC 13818-1 PES packets containing private data|Dolby Digital DVB"},
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
        {0xFF, UNO_PID, "", ""}, /* loop stop condition! */
};

enum
{
        /* note, in any state:  */
        /*     * meet new PID -> add to pid_list */
        /*     * meet new table -> add to section_list and parse */
        /*     * meet old table -> if it was modified, report a warning */
        STATE_NEXT_PAT, /* colloct PAT section, parse to create prog_list */
        STATE_NEXT_PMT, /* collect PMT section, parse to create track_list */
        STATE_NEXT_PKT  /* parse packet with current lists */
};

static int state_next_pat(obj_t *obj);
static int state_next_pmt(obj_t *obj);
static int state_next_pkt(obj_t *obj);

static int parse_TS_head(obj_t *obj); /* TS head information */
static int parse_AF(obj_t *obj); /* Adaption Fields information */
static int section(obj_t *obj); /* collect PSI/SI section data */
static int parse_table(obj_t *obj);
static int parse_PSI_head(ts_psi_t *psi, uint8_t *section);
static int parse_PAT_load(obj_t *obj, uint8_t *section);
static int parse_PMT_load(obj_t *obj, uint8_t *section);
static int parse_SDT_load(obj_t *obj, uint8_t *section);
static int parse_PES_head(obj_t *obj); /* PES layer information */
static int parse_PES_head_switch(obj_t *obj);
static int parse_PES_head_detail(obj_t *obj);

static ts_pid_t *add_to_pid_list(LIST *list, ts_pid_t *pids);
static ts_pid_t *add_new_pid(obj_t *obj);
static int is_all_prog_parsed(obj_t *obj);
static int pid_type(uint16_t pid);
static const table_id_table_t *table_type(uint8_t id);
static int track_type(ts_track_t *track);

static int64_t lmt_add(int64_t t1, int64_t t2, int64_t ovf);
static int64_t lmt_min(int64_t t1, int64_t t2, int64_t ovf);

static int dump(uint8_t *buf, int len); /* for debug */

int tsCreate(ts_rslt_t **rslt)
{
        obj_t *obj;
        ts_error_t *err;

        obj = (obj_t *)malloc(sizeof(obj_t));
        if(NULL == obj)
        {
                DBG(ERR_MALLOC_FAILED, "\n");
                return (int)NULL;
        }

        *rslt = &(obj->rslt);
        (*rslt)->pkt = &((*rslt)->PKT);
        list_init(&((*rslt)->table_list));
        list_init(&((*rslt)->prog_list));
        list_init(&((*rslt)->pid_list));

        obj->is_first_pkt = 1;
        obj->state = STATE_NEXT_PAT;
        obj->is_pes_align = 0; /* PES align(0: need; 1: dot't need) */

        (*rslt)->cnt = 0;
        (*rslt)->CC_lost = 0;
        (*rslt)->is_psi_parsed = 0;
        (*rslt)->concerned_pid = 0x0000; /* PAT_PID */
        (*rslt)->prog0 = NULL;
        (*rslt)->interval = 0;
        (*rslt)->lCTS = 0L;
        (*rslt)->STC = 0L;

        /* clear error struct */
        err = &((*rslt)->err);
        memset(err, 0, sizeof(ts_error_t));

        return (int)obj;
}

int tsDelete(int id)
{
        obj_t *obj;

        obj = (obj_t *)id;
        if(NULL == obj)
        {
                DBG(ERR_BAD_ID, "\n");
                return -ERR_BAD_ID;
        }
        else
        {
                ts_rslt_t *rslt = &(obj->rslt);
                NODE *node;

                for(node = rslt->prog_list.head; node; node = node->next)
                {
                        ts_prog_t *prog = (ts_prog_t *)node;
                        list_free(&(prog->track_list));
                }
                list_free(&(rslt->prog_list));
                list_free(&(rslt->pid_list));

                free(obj);

                return 0;
        }
}

int tsParseTS(int id)
{
        obj_t *obj;
        ts_rslt_t *rslt;
        ts_pkt_t *pkt;

        obj = (obj_t *)id;
        if(NULL == obj)
        {
                DBG(ERR_BAD_ID, "\n");
                return -ERR_BAD_ID;
        }
        rslt = &(obj->rslt);
        pkt = rslt->pkt;

        obj->p = pkt->ts;
        obj->len = 188;
        /*dump(pkt->ts, 188); */

        /* packet count */
        if(obj->is_first_pkt)
        {
                obj->is_first_pkt = 0;
                rslt->cnt = 0;
        }
        else
        {
                rslt->cnt++;
        }

        /*fprintf(stderr, "cnt: %lld, addr: %lld\n", rslt->cnt, pkt->ADDR); */
        return parse_TS_head(obj);
}

int tsParseOther(int id)
{
        obj_t *obj;

        obj = (obj_t *)id;
        if(NULL == obj)
        {
                DBG(ERR_BAD_ID, "\n");
                return -ERR_BAD_ID;
        }

        switch(obj->state)
        {
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

static int state_next_pat(obj_t *obj)
{
        ts_t *ts = &(obj->ts);
        LIST *table_list = &(obj->rslt.table_list);
        ts_table_t *table;
        uint8_t section_number;

        if(0x0000 != ts->PID)
        {
                return -1; /* not PAT */
        }

        /* section parse has done in parse_TS_head()! */
        if(NULL == (table = (ts_table_t *)list_search(table_list, 0x00)))
        {
                return -1;
        }
        else
        {
                NODE *node;

                section_number = 0;
                for(node = table->section_list.head; node; node = node->next)
                {
                        ts_section_t *section = (ts_section_t *)node;

                        if(section_number > table->last_section_number)
                        {
                                return -1;
                        }
                        if(section_number != section->section_number)
                        {
                                return -1;
                        }
                        section_number++;
                }

                /* all PAT section parsed */
                obj->state = STATE_NEXT_PMT;
        }

        if(is_all_prog_parsed(obj))
        {
                obj->rslt.is_psi_parsed = 1;
                obj->state = STATE_NEXT_PKT;
        }

        return 0;
}

static int state_next_pmt(obj_t *obj)
{
        ts_t *ts = &(obj->ts);
        ts_pid_t *pids;

        pids = (ts_pid_t *)list_search(&(obj->rslt.pid_list), ts->PID);
        if((NULL == pids) || (PMT_PID != pids->type))
        {
                return -1; /* not PMT */
        }

        section(obj);

        if(is_all_prog_parsed(obj))
        {
                obj->rslt.is_psi_parsed = 1;
                obj->state = STATE_NEXT_PKT;
        }

        return 0;
}

static int state_next_pkt(obj_t *obj)
{
        ts_t *ts = &(obj->ts);
        af_t *af = &(obj->af);
        ts_rslt_t *rslt = &(obj->rslt);
        ts_pid_t *pids = rslt->pids;
        ts_track_t *track = pids->track; /* may be NULL */
        ts_prog_t *prog = pids->prog; /* may be NULL */
        ts_error_t *err = &(rslt->err);
        ts_pkt_t *pkt = rslt->pkt;

        /* CC */
        if(pids->is_CC_sync)
        {
                uint8_t dCC;
                int lost;

                if((1 == ts->adaption_field_control) || /* 01 */
                   (3 == ts->adaption_field_control))   /* 11 */
                {
                        dCC = 1;
                }
                else /* 00 or 10 */
                {
                        dCC = 0;
                }

                pids->CC += dCC;
                pids->CC &= 0x0F; /* 4-bit */
                lost  = (int)ts->continuity_counter;
                lost -= (int)pids->CC;
                if(lost < 0)
                {
                        lost += 16;
                }

                rslt->CC_wait = pids->CC;
                rslt->CC_find = ts->continuity_counter;
                rslt->CC_lost = lost;
        }
        else
        {
                pids->is_CC_sync = 1;

                rslt->CC_wait = pids->CC;
                rslt->CC_find = ts->continuity_counter;
                rslt->CC_lost = 0;
        }
        pids->CC = ts->continuity_counter; /* update CC */
        err->Continuity_count_error = rslt->CC_lost;
        if(0x1FFF == ts->PID)
        {
                /* continuity_counter of null packet is undefined */
                err->Continuity_count_error = 0;
        }

        /* calc STC, should be here(before PCR flush) */
        if(NULL != pkt->cts)
        {
                uint64_t dCTS;

                dCTS = pkt->CTS;
                dCTS += (pkt->CTS < rslt->lCTS) ? pkt->CTS_overflow : 0;
                dCTS -= rslt->lCTS;
                rslt->lCTS = pkt->CTS;

                rslt->STC = rslt->lSTC + dCTS;
                rslt->lSTC = rslt->STC;
        }
        else
        {
                if((prog) && 
                   (prog->STC_sync) &&
                   (prog->PCRa != prog->PCRb))
                {
                        long double delta;

                        /* STCx - PCRb   ADDx - ADDb */
                        /* ----------- = ----------- */
                        /* PCRb - PCRa   ADDb - ADDa */
                        delta = (long double)lmt_min(prog->PCRb, prog->PCRa, STC_OVF);
                        delta *= (pkt->ADDR - prog->ADDb);
                        delta /= (prog->ADDb - prog->ADDa);
                        rslt->STC = lmt_add(prog->PCRb, (uint64_t)delta, STC_OVF);
                }
                else
                {
                        rslt->STC = 0;
                }
        }
        rslt->STC_base = rslt->STC / 300;
        rslt->STC_ext = rslt->STC % 300;

        /* PCR flush */
        if(rslt->has_PCR)
        {
                rslt->PCR_base = af->program_clock_reference_base;
                rslt->PCR_ext  = af->program_clock_reference_extension;

                rslt->PCR  = rslt->PCR_base;
                rslt->PCR *= 300;
                rslt->PCR += rslt->PCR_ext;

                if(prog)
                {
                        /* PCR_interval (PCR packet arrive time interval) */
                        rslt->PCR_interval = lmt_min(rslt->STC, prog->PCRb, STC_OVF);
                        if((prog->STC_sync) &&
                           !(0 < rslt->PCR_interval && rslt->PCR_interval <= 40 * STC_MS))
                        {
                                /* !(0 < interval < +40ms) */
                                err->PCR_repetition_error = 1;
                        }

                        /* PCR_continuity (PCR value interval) */
                        rslt->PCR_continuity = lmt_min(rslt->PCR, prog->PCRb, STC_OVF);
                        if((prog->STC_sync) && !(af->discontinuity_indicator) &&
                           !(0 < rslt->PCR_continuity && rslt->PCR_continuity <= 100 * STC_MS))
                        {
                                /* !(0 < continuity < +100ms) */
                                err->PCR_discontinuity_indicator_error = 1;
                        }

                        /* PCR_jitter */
                        rslt->PCR_jitter = lmt_min(rslt->PCR, rslt->STC, STC_OVF);
                        if((prog->STC_sync) &&
                           !(-13 <= rslt->PCR_jitter && rslt->PCR_jitter <= +13))
                        {
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
                        if(!prog->STC_sync)
                        {
                                if(NULL != pkt->cts)
                                {
                                        rslt->lSTC = rslt->PCR;
                                }
                                else
                                {
                                        if(prog->PCRb != STC_OVF)
                                        {
                                                if(prog->PCRa == STC_OVF)
                                                {
                                                        NODE *node;

                                                        /* 1st PCR of this program, clear cnt & interval */
                                                        for(node = rslt->pid_list.head; node; node = node->next)
                                                        {
                                                                ts_pid_t *pid_item = (ts_pid_t *)node;
                                                                if(pid_item->prog == prog)
                                                                {
                                                                        pid_item->lcnt = 0;
                                                                        pid_item->cnt = 0;
                                                                }
                                                        }
                                                        prog->interval = 0;
                                                }
                                                else
                                                {
                                                        /* 2ed PCR of this program, STC can be calc */
                                                        prog->STC_sync = 1;
                                                }
                                        }
                                }
                        }

                        /* the packet that use PCR of first program */
                        if(prog->PCR_PID == rslt->prog0->PCR_PID)
                        {
                                rslt->interval += lmt_min(prog->PCRb, prog->PCRa, STC_OVF);
                                if(rslt->interval >= rslt->aim_interval)
                                {
                                        NODE *node;

                                        /* calc bitrate and clear the packet count */
                                        for(node = rslt->pid_list.head; node; node = node->next)
                                        {
                                                ts_pid_t *pid_item = (ts_pid_t *)node;
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
                                }
                        }
                }
                else
                {
                        fprintf(stderr, "error: PCR packet without program pointer!\n");
                }
        }

        /* PES head & ES data */
        if(track && (0 == ts->transport_scrambling_control))
        {
                parse_PES_head(obj);

                if(rslt->has_PTS)
                {
                        rslt->PTS_interval = lmt_min(rslt->PTS, track->PTS, STC_BASE_OVF);
                        rslt->PTS_minus_STC = lmt_min(rslt->PTS, rslt->STC_base, STC_BASE_OVF);
                        track->PTS = rslt->PTS; /* record last PTS in track */
                }

                if(rslt->has_DTS)
                {
                        rslt->DTS_interval = lmt_min(rslt->DTS, track->DTS, STC_BASE_OVF);
                        rslt->DTS_minus_STC = lmt_min(rslt->DTS, rslt->STC_base, STC_BASE_OVF);
                        track->DTS = rslt->DTS; /* record last DTS in track */
                }
        }

        return 0;
}

static int parse_TS_head(obj_t *obj)
{
        uint8_t dat;
        ts_t *ts = &(obj->ts);
        af_t *af = &(obj->af);
        ts_rslt_t *rslt = &(obj->rslt);
        ts_error_t *err = &(rslt->err);

        /* init rslt */
        rslt->is_psi_si = 0;
        rslt->has_section = 0;
        rslt->pids = NULL;
        rslt->has_rate = 0;
        rslt->has_PCR = 0;
        rslt->has_PTS = 0;
        rslt->has_DTS = 0;
        rslt->PES_len = 0; /* clear PES length */
        rslt->ES_len = 0; /* clear ES length */

        /* begin */
        dat = *(obj->p)++; obj->len--;
        ts->sync_byte = dat;
        if(0x47 != ts->sync_byte)
        {
                err->Sync_byte_error++;
                if(err->Sync_byte_error > 1)
                {
                        err->TS_sync_loss++;
                }
                fprintf(stderr, "sync_byte(0x%02X) error!\n", ts->sync_byte);
                dump(obj->rslt.pkt->ts, 188);
        }
        else
        {
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

        if(0x00 == ts->adaption_field_control)
        {
                fprintf(stderr, "Bad adaption_field_control field(00)!\n");
        }

        if(BIT(1) & ts->adaption_field_control)
        {
                parse_AF(obj);
        }
        else
        {
                af->adaption_field_length = 0x00;
        }

        if(BIT(0) & ts->adaption_field_control)
        {
                /* data_byte, PSI or PES */
        }

        rslt->pid = ts->PID; /* record into rslt struct */
        rslt->pids = (ts_pid_t *)list_search(&(obj->rslt.pid_list), rslt->pid);
        if(NULL == rslt->pids)
        {
                /* find new PID, add it in pid_list */
                rslt->pids = add_new_pid(obj);
        }

        /* statistic & PSI/SI section collect */
        rslt->pids->cnt++;
        rslt->sys_cnt++;
        rslt->nul_cnt += ((0x1FFF == ts->PID) ? 1 : 0);
        if((ts->PID < 0x0020)) /* || (PMT_PID == rslt->pids->type)) */
        {
                /* PSI/SI packet */
                rslt->psi_cnt++;
                rslt->is_psi_si = 1;
                section(obj);
        }

        return 0;
}

static int parse_AF(obj_t *obj)
{
        int i;
        uint8_t dat;
        af_t *af = &(obj->af);
        ts_rslt_t *rslt = &(obj->rslt);

        dat = *(obj->p)++; obj->len--;
        af->adaption_field_length = dat;
        if(0x00 == af->adaption_field_length)
        {
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

        if(af->PCR_flag)
        {
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
        if(af->OPCR_flag)
        {
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
        if(af->splicing_pointer_flag)
        {
                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->splice_countdown = dat;
        }
        if(af->transport_private_data_flag)
        {
                /*... */
        }
        if(af->adaption_field_extension_flag)
        {
                /*... */
        }

        /* pass stuffing byte */
        for(i = af->adaption_field_length; i > 0; i--)
        {
                dat = *(obj->p)++; obj->len--;
        }

        return 0;
}

static int section(obj_t *obj)
{
        uint8_t pointer_field;
        ts_t *ts = &(obj->ts);
        ts_psi_t *psi = &(obj->rslt.psi);
        ts_pid_t *pids = obj->rslt.pids;

        /* FIXME: if data after CRC_32 is NOT 0xFF, it's another section! */
        if(0 == pids->section_idx)
        {
                /* waiting for section head */
                if(1 == ts->payload_unit_start_indicator)
                {
                        pointer_field = *(obj->p)++; obj->len--;
                        obj->p += pointer_field;
                        obj->len -= pointer_field;
                        memcpy(pids->section, obj->p, obj->len);
                        pids->section_idx = obj->len;
                }
                else /* (0 == ts->payload_unit_start_indicator) */
                {
                        /* section async, ignore this packet! */
                        /*dump(obj->p, obj->len); */
                }
        }
        else /* (0 != pids->section_idx) */
        {
                /* collect section data */
                if(1 == ts->payload_unit_start_indicator)
                {
                        pointer_field = *(obj->p)++; obj->len--;
                        memcpy(pids->section + pids->section_idx, obj->p, pointer_field);
                        pids->section_idx += pointer_field;
                        parse_table(obj);
                        obj->p += pointer_field;
                        obj->len -= pointer_field;
                        memcpy(pids->section, obj->p, obj->len);
                        pids->section_idx = obj->len;
                }
                else /* (0 == ts->payload_unit_start_indicator) */
                {
                        memcpy(pids->section + pids->section_idx, obj->p, 184);
                        pids->section_idx += 184;
                }
        }

        if(pids->section_idx >= 8)
        {
                /* PSI head OK */
                if(0 != parse_PSI_head(psi, pids->section))
                {
                        return -1;
                }

                if(pids->section_idx >= (3 + psi->section_length))
                {
                        /* section data enough */
                        parse_table(obj);
                        pids->section_idx = 0;
                }
        }

        return 0;
}

static int parse_table(obj_t *obj)
{
        uint8_t *p;
        ts_table_t *table;
        LIST *section_list;
        ts_section_t *section;
        ts_rslt_t *rslt = &(obj->rslt);
        ts_pid_t *pids = rslt->pids;
        ts_psi_t *psi = &(rslt->psi);
        ts_error_t *err = &(rslt->err);
        int is_new_version = 0;

        /* get psi head info */
        if(0 != parse_PSI_head(psi, pids->section))
        {
                return -1;
        }

        /* CRC check */
        if(psi->check_CRC)
        {
                p = pids->section + 3 + psi->section_length - 4;
                pids->CRC_32   = *p++;
                pids->CRC_32 <<= 8;
                pids->CRC_32  |= *p++;
                pids->CRC_32 <<= 8;
                pids->CRC_32  |= *p++;
                pids->CRC_32 <<= 8;
                pids->CRC_32  |= *p++;

                pids->CRC_32_calc = CRC_for_TS(pids->section, 3 + psi->section_length - 4, 32);
                /*pids->CRC_32_calc = CRC(pids->section, 3 + psi->section_length - 4, 32); */
                /*pids->CRC_32_calc = crc32(pids->section, 3 + psi->section_length - 4); */
                if(pids->CRC_32_calc != pids->CRC_32)
                {
                        err->CRC_error = 1;
                        /*dump(pids->section, 3 + psi->section_length); */
                        return -1;
                }
        }

        /* get "table" and "section_list" pointer */
        if(0x02 == psi->table_id) /* PMT section */
        {
                if(NULL == pids->prog)
                {
                        DBG(ERR_OTHER, "\n");
                        return -1;
                }
                table = &(pids->prog->table);
        }
        else /* other section */
        {
                LIST *table_list = &(obj->rslt.table_list);

                if(NULL == (table = (ts_table_t *)list_search(table_list, psi->table_id)))
                {
                        /* add table */
                        table = (ts_table_t *)malloc(sizeof(ts_table_t));
                        if(NULL == table)
                        {
                                DBG(ERR_MALLOC_FAILED, "\n");
                                return -ERR_MALLOC_FAILED;
                        }

                        table->table_id = psi->table_id;
                        table->version_number = psi->version_number;
                        table->last_section_number = psi->last_section_number;
                        list_init(&(table->section_list));
                        list_insert(table_list, (NODE *)table, psi->table_id);
                }
        }
        section_list = &(table->section_list);

        /* new table version? */
        if(table->version_number != psi->version_number)
        {
                /* clear section_list and update table parameter */
                table->version_number = psi->version_number;
                table->last_section_number = psi->last_section_number;
                list_free(section_list);
                is_new_version = 1;
        }

        /* get "section" pointer */
        if(NULL == (section = (ts_section_t *)list_search(section_list, psi->section_number)))
        {
                /* add section */
                section = (ts_section_t *)malloc(sizeof(ts_section_t));
                if(NULL == section)
                {
                        DBG(ERR_MALLOC_FAILED, "\n");
                        return -ERR_MALLOC_FAILED;
                }

                section->section_number = psi->section_number;
                section->section_length = psi->section_length;
                memcpy(section->data, pids->section, 3 + psi->section_length);
                list_insert(section_list, (NODE *)section, psi->section_number);
        }
        else
        {
                /*fprintf(stderr, "has section %02X/%02X(table %02X)\n", */
                /*        psi->section_number, */
                /*        psi->last_section_number, */
                /*        psi->table_id); */
        }

        /* parse */
        rslt->has_section = 1;
        switch(psi->table_id)
        {
                case 0x00:
                        parse_PAT_load(obj, pids->section);
                        break;
                case 0x02:
                        parse_PMT_load(obj, pids->section);
                        break;
                case 0x42:
                        parse_SDT_load(obj, pids->section);
                        break;
                default:
                        break;
        }

        return 0;
}

static int parse_PSI_head(ts_psi_t *psi, uint8_t *section)
{
        uint8_t *p;
        const table_id_table_t *table;

        p = section;
        psi->table_id = *p++;
        psi->section_syntax_indicator = (*p & BIT(7)) >> 7;
        psi->private_indicator = (*p & BIT(6)) >> 6;
        psi->section_length   = *p++ & 0x0F;
        psi->section_length <<= 8;
        psi->section_length  |= *p++;
        if(1 == psi->section_syntax_indicator)
        {
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
        else
        {
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

        if(0 == psi->private_indicator)
        {
                /* normal section */
                if(psi->section_length > 1021)
                {
                        fprintf(stderr, "normal section, length(%d) overflow!\n",
                                psi->section_length);
                        psi->section_length = 1021;
                        return -1;
                }
        }
        else
        {
                /* private section */
                if(psi->section_length > 4093)
                {
                        fprintf(stderr, "private section, length(%d) overflow!\n",
                                psi->section_length);
                        psi->section_length = 4093;
                        return -1;
                }
        }

        return 0;
}

static int parse_PAT_load(obj_t *obj, uint8_t *section)
{
        uint8_t dat;
        uint8_t *p = section + 8;
        ts_psi_t *psi = &(obj->rslt.psi);
        int len = psi->section_length - 5;
        ts_pid_t ts_pid, *pids = &ts_pid;
        ts_prog_t *prog;
        ts_rslt_t *rslt = &(obj->rslt);

        /* to avoid stack overflow, FIXME */
        if(0 != list_cnt(&(rslt->prog_list)))
        {
                return 0;
        }

        /* in PAT, table_id_extension is transport_stream_id */
        rslt->transport_stream_id = psi->table_id_extension;

        while(len > 4)
        {
                /* add program */
                prog = (ts_prog_t *)malloc(sizeof(ts_prog_t));
                if(NULL == prog)
                {
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
                pids->PID = prog->PMT_PID;

                if(0 == prog->program_number)
                {
                        /* network PID, not a program */
                        pids->type = NIT_PID;

                        if(0x0010 != pids->PID)
                        {
                                fprintf(stderr, "NIT_PID(0x%04X) is NOT 0x0010!\n", pids->PID);
                        }
                        free(prog);
                }
                else
                {
                        pids->type = PMT_PID;

                        /* record first prog for bitrate calc */
                        if(NULL == rslt->prog0)
                        {
                                NODE *node;
                                rslt->prog0 = prog;

                                /* traverse pid_list */
                                /* if it des not belong to any program, use prog0 */
                                for(node = rslt->pid_list.head; node; node = node->next)
                                {
                                        ts_pid_t *pid_item = (ts_pid_t *)node;
                                        if(pid_item->PID < 0x0020 || pid_item->PID == 0x1FFF)
                                        {
                                                pid_item->prog = rslt->prog0;
                                        }
                                }
                        }

                        /* PMT table */
                        prog->is_parsed = 0;
                        prog->table.table_id = 0x02;
                        prog->table.version_number = 0xFF; /* never reached version */
                        prog->table.last_section_number = 0; /* no use */
                        list_init(&(prog->table.section_list));

                        /* track list */
                        list_init(&(prog->track_list));

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

                        list_insert(&(rslt->prog_list), (NODE *)prog, prog->program_number);
                }

                pids->cnt = 0;
                pids->lcnt = 0;
                pids->prog = prog;
                pids->track = NULL;
                pids->CC = 0;
                pids->is_CC_sync = 0;
                pids->sdes = PID_TYPE[pids->type].sdes;
                pids->ldes = PID_TYPE[pids->type].ldes;
                add_to_pid_list(&(rslt->pid_list), pids); /* NIT or PMT PID */
        }

        return 0;
}

static int parse_PMT_load(obj_t *obj, uint8_t *section)
{
        uint8_t dat;
        uint8_t *p = section + 8;
        ts_psi_t *psi = &(obj->rslt.psi);
        int len = psi->section_length - 5;
        ts_pid_t ts_pid, *pids = &ts_pid;
        ts_prog_t *prog;
        ts_track_t *track;

        /* in PMT, table_id_extension is program_number */
        prog = (ts_prog_t *)list_search(&(obj->rslt.prog_list), psi->table_id_extension);
        if((NULL == prog) || (prog->is_parsed))
        {
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
        pids->PID = prog->PCR_PID;
        pids->cnt = 0;
        pids->lcnt = 0;
        pids->prog = prog;
        pids->track = NULL;
        pids->type = PCR_PID;
        pids->CC = 0;
        pids->is_CC_sync = 1;
        pids->sdes = PID_TYPE[pids->type].sdes;
        pids->ldes = PID_TYPE[pids->type].ldes;
        add_to_pid_list(&(obj->rslt.pid_list), pids); /* PCR_PID */

        /* program_info_length */
        dat = *p++; len--;
        prog->program_info_len = dat & 0x0F;

        dat = *p++; len--;
        prog->program_info_len <<= 8;
        prog->program_info_len |= dat;

        /* record program_info */
        if(prog->program_info_len > INFO_LEN_MAX)
        {
                fprintf(stderr, "PID(0x%04X): program_info_length(%d) too big!\n",
                        obj->ts.PID, prog->program_info_len);
        }

        /* record program_info */
        memcpy(prog->program_info, p, prog->program_info_len);
        p += prog->program_info_len;
        len -= prog->program_info_len;

        while(len > 4)
        {
                /* add track */
                track = (ts_track_t *)malloc(sizeof(ts_track_t));
                if(NULL == track)
                {
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
                if(track->es_info_len > INFO_LEN_MAX)
                {
                        fprintf(stderr, "PID(0x%04X): ES_info_length(%d) too big!\n",
                                track->PID, track->es_info_len);
                }

                /* record es_info */
                memcpy(track->es_info, p, track->es_info_len);
                p += track->es_info_len;
                len -= track->es_info_len;

                track_type(track);
                if(track->PID == prog->PCR_PID)
                {
                        switch(track->type)
                        {
                                case VID_PID: track->type = VID_PCR; break;
                                case AUD_PID: track->type = AUD_PCR; break;
                                default:      track->type = UNO_PCR; break;
                        }
                }
                list_add(&(prog->track_list), (NODE *)track, track->PID);

                /* add track PID */
                pids->PID = track->PID;
                pids->cnt = 0;
                pids->lcnt = 0;
                pids->prog = prog;
                pids->track = track;
                pids->type = track->type;
                pids->CC = 0;
                pids->is_CC_sync = 0;
                pids->sdes = PID_TYPE[pids->type].sdes;
                pids->ldes = PID_TYPE[pids->type].ldes;
                add_to_pid_list(&(obj->rslt.pid_list), pids); /* elementary_PID */
        }

        return 0;
}

static int parse_SDT_load(obj_t *obj, uint8_t *section)
{
        uint8_t dat;
        uint8_t *p = section + 8;
        ts_psi_t *psi = &(obj->rslt.psi);
        int len = psi->section_length - 5;
        ts_rslt_t *rslt = &(obj->rslt);
        uint16_t original_network_id;

        /* in SDT, table_id_extension is transport_stream_id */
        if(psi->table_id_extension != rslt->transport_stream_id)
        {
                fprintf(stderr, "table_id_extension(%d) != transport_stream_id(%d)\n",
                        psi->table_id_extension,
                        rslt->transport_stream_id);
                return -1; /* bad SDT table, ignore */
        }

        dat = *p++; len--;
        original_network_id = dat;

        dat = *p++; len--;
        original_network_id <<= 8;
        original_network_id |= dat;
        if(original_network_id != rslt->transport_stream_id)
        {
#if 0
                fprintf(stderr, "original_network_id(%d) != transport_stream_id(%d)\n",
                        original_network_id,
                        rslt->transport_stream_id);
                return -1; /* bad SDT table, ignore */
#endif
        }

        dat = *p++; len--; /* reserved_future_use */

        while(len > 4)
        {
                ts_prog_t *prog;

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
                prog = (ts_prog_t *)list_search(&(rslt->prog_list), service_id);

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

                while(descriptors_loop_length > 0)
                {
                        uint8_t tag;
                        uint8_t length;

                        tag = *p++; len--; descriptors_loop_length--;
                        length = *p++; len--; descriptors_loop_length--;
                        if((0xFF == tag) || (0 == length))
                        {
                                return -1; /* wrong descriptor */
                        }
                        if(0x48 == tag && prog) /* service_descriptor */
                        {
                                uint8_t *pt = p;
                                uint8_t type;

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

static int parse_PES_head(obj_t *obj)
{
        uint8_t dat;
        ts_t *ts = &(obj->ts);
        pes_t *pes = &(obj->pes);
        ts_rslt_t *rslt = &(obj->rslt);

        /* record PES data */
        if(obj->is_pes_align) /* FIXME */
        {
                rslt->PES_len = obj->len;
                rslt->PES_buf = obj->p;
        }

        if(ts->payload_unit_start_indicator)
        {
                obj->is_pes_align = 1; /* FIXME */
                rslt->PES_len = obj->len;
                rslt->PES_buf = obj->p;

                /* PES head start */
                dat = *(obj->p)++; obj->len--;
                pes->packet_start_code_prefix = dat;

                dat = *(obj->p)++; obj->len--;
                pes->packet_start_code_prefix <<= 8;
                pes->packet_start_code_prefix |= dat;

                dat = *(obj->p)++; obj->len--;
                pes->packet_start_code_prefix <<= 8;
                pes->packet_start_code_prefix |= dat;

                if(0x000001 != pes->packet_start_code_prefix)
                {
                        fprintf(stderr, "PES packet start code prefix(0x%06X) NOT 0x000001!\n",
                                pes->packet_start_code_prefix);
                        dump(obj->rslt.pkt->ts, 188);
                        return -1;
                }

                dat = *(obj->p)++; obj->len--;
                pes->stream_id = dat;

                dat = *(obj->p)++; obj->len--;
                pes->PES_packet_length = dat;

                dat = *(obj->p)++; obj->len--;
                pes->PES_packet_length <<= 8;
                pes->PES_packet_length |= dat; /* 0x0000 for many video pes */
                /*fprintf(stderr, "PES_packet_length = %d, ", pes->PES_packet_length); */
                /*fprintf(stderr, "PES_packet_length = 0x%X, ", pes->PES_packet_length); */

                parse_PES_head_switch(obj);
        }

        if(obj->is_pes_align) /* FIXME */
        {
                rslt->ES_len = obj->len;
                rslt->ES_buf = obj->p;
        }

        return 0;
}

static int parse_PES_head_switch(obj_t *obj)
{
        pes_t *pes = &(obj->pes);

        switch(pes->stream_id)
        {
                case 0xBE: /* padding_stream */
                        /* subsequent pes->PES_packet_length data is padding_byte, pass */
                        if(pes->PES_packet_length > obj->len)
                        {
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

static int parse_PES_head_detail(obj_t *obj)
{
        int i;
        int header_data_len;
        uint8_t dat;
        pes_t *pes = &(obj->pes);
        ts_rslt_t *rslt = &(obj->rslt);

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
        if(header_data_len > obj->len)
        {
                fprintf(stderr, "PES_header_data_length(%d) is too long!\n",
                        header_data_len);
                return -1;
        }

        /* get PTS, DTS */
        if(0x02 == pes->PTS_DTS_flags) /* '10' */
        {
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
        else if(0x03 == pes->PTS_DTS_flags) /* '11' */
        {
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
        else if(0x01 == pes->PTS_DTS_flags) /* '01' */
        {
                fprintf(stderr, "PTS_DTS_flags error!\n");
                dump(obj->rslt.pkt->ts, 188);
                return -1;
        }
        else
        {
                /* no PTS, no DTS, it's OK! */
        }

        if(pes->ESCR_flag)
        {
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

        if(pes->ES_rate_flag)
        {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ES_rate = (dat & 0x7F);

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ES_rate <<= 8;
                pes->ES_rate |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ES_rate <<= 7;
                pes->ES_rate |= (dat >> 1);
        }

        if(pes->DSM_trick_mode_flag)
        {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->trick_mode_control = (dat & (BIT(7) | BIT(6) | BIT(5))) >> 5;

                if(0x00 == pes->trick_mode_control)
                {
                        /* '000', fast_forward */
                        pes->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                        pes->intra_slice_refresh = (dat & (BIT(2))) >> 2;
                        pes->frequency_truncation = (dat & (BIT(1) | BIT(0)));
                }
                else if(0x01 == pes->trick_mode_control)
                {
                        /* '001', slow_motion */
                        pes->rep_cntrl = (dat & 0x1F);
                }
                else if(0x02 == pes->trick_mode_control)
                {
                        /* '010', freeze_frame */
                        pes->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                }
                else if(0x03 == pes->trick_mode_control)
                {
                        /* '011', fast_reverse */
                        pes->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                        pes->intra_slice_refresh = (dat & (BIT(2))) >> 2;
                        pes->frequency_truncation = (dat & (BIT(1) | BIT(0)));
                }
                else if(0x04 == pes->trick_mode_control)
                {
                        /* '100', slow_reverse */
                        pes->rep_cntrl = (dat & 0x1F);
                }
        }

        if(pes->additional_copy_info_flag)
        {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->additional_copy_info = (dat & 0x7F);
        }

        if(pes->PES_CRC_flag)
        {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->previous_PES_packet_CRC = dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->previous_PES_packet_CRC <<= 8;
                pes->previous_PES_packet_CRC |= dat;
        }

        if(pes->PES_extension_flag)
        {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PES_private_data_flag = (dat & (BIT(7))) >> 7;
                pes->pack_header_field_flag = (dat & (BIT(6))) >> 6;
                pes->program_packet_sequence_counter_flag = (dat & (BIT(5))) >> 5;
                pes->P_STD_buffer_flag = (dat & (BIT(4))) >> 4;
                pes->PES_extension_flag_2 = (dat & (BIT(0))) >> 0;

                if(pes->PES_private_data_flag)
                {
                        for(i = 16; i > 0; i--)
                        {
                                dat = *(obj->p)++; obj->len--; header_data_len--;
                                pes->PES_private_data[i] = dat;
                        }
                }

                if(pes->pack_header_field_flag)
                {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->pack_field_length = dat;

                        /* pass pack_header() */
                        for(i = pes->pack_field_length; i > 0; i--)
                        {
                                dat = *(obj->p)++; obj->len--; header_data_len--;
                        }
                }

                if(pes->program_packet_sequence_counter_flag)
                {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->program_packet_sequence_counter = (dat & 0x7F);

                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->MPEG1_MPEG2_identifier = (dat & (BIT(6))) >> 6;
                        pes->original_stuff_length = (dat & 0x3F);
                }

                if(pes->P_STD_buffer_flag)
                {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->P_STD_buffer_scale = (dat & (BIT(5))) >> 5;
                        pes->P_STD_buffer_size = (dat & 0x1F);

                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->P_STD_buffer_size <<= 8;
                        pes->P_STD_buffer_size |= dat;
                }

                if(pes->PES_extension_flag_2)
                {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->PES_extension_field_length = (dat & 0x7F);

                        /* pass PES_extension_field */
                        for(i = pes->PES_extension_field_length; i > 0; i--)
                        {
                                dat = *(obj->p)++; obj->len--; header_data_len--;
                        }
                }
        }

        /* pass stuffing_byte */
        for(; header_data_len > 0; header_data_len--)
        {
                dat = *(obj->p)++; obj->len--;
        }

        /* subsequent obj->len data is ES data */

        return 0;
}

static ts_pid_t *add_new_pid(obj_t *obj)
{
        ts_pid_t ts_pid, *pids;
        ts_rslt_t *rslt = &(obj->rslt);

        pids = &ts_pid;

        pids->PID = rslt->pid;
        pids->type = pid_type(pids->PID);
        if((NULL != rslt->prog0) && 
           (pids->PID < 0x0020 || pids->PID == 0x1FFF))
        {
                pids->prog = rslt->prog0;
        }
        else
        {
                pids->prog = NULL;
        }
        pids->track = NULL;
        pids->cnt = 1;
        pids->lcnt = 0;
        /*pids->CC = ts->continuity_counter; */
        pids->is_CC_sync = 0;
        pids->sdes = PID_TYPE[pids->type].sdes;
        pids->ldes = PID_TYPE[pids->type].ldes;

        return add_to_pid_list(&(rslt->pid_list), pids); /* other_PID */
}

static ts_pid_t *add_to_pid_list(LIST *list, ts_pid_t *the_pids)
{
        ts_pid_t *pids;

        if(NULL != (pids = (ts_pid_t *)list_search(list, the_pids->PID)))
        {
                /* in pid_list already, just update information */
                pids->PID = the_pids->PID;
                pids->prog = the_pids->prog;
                pids->track = the_pids->track;
                pids->type = the_pids->type;
                pids->cnt = the_pids->cnt;
                pids->lcnt = the_pids->lcnt;
                pids->CC = the_pids->CC;
                pids->is_CC_sync = the_pids->is_CC_sync;
                pids->sdes = the_pids->sdes;
                pids->ldes = the_pids->ldes;
        }
        else
        {
                pids = (ts_pid_t *)malloc(sizeof(ts_pid_t));
                if(NULL == pids)
                {
                        DBG(ERR_MALLOC_FAILED, "\n");
                        return NULL;
                }

                pids->PID = the_pids->PID;
                pids->prog = the_pids->prog;
                pids->track = the_pids->track;
                pids->type = the_pids->type;
                pids->cnt = the_pids->cnt;
                pids->lcnt = the_pids->lcnt;
                pids->CC = the_pids->CC;
                pids->is_CC_sync = the_pids->is_CC_sync;
                pids->sdes = the_pids->sdes;
                pids->ldes = the_pids->ldes;
                pids->section_idx = 0; /* wait to sync with section head */
                pids->fd = NULL;

                list_insert(list, (NODE *)pids, the_pids->PID);
        }
        return pids;
}

static int is_all_prog_parsed(obj_t *obj)
{
        uint8_t section_number;
        NODE *prog_node;

        for(prog_node = obj->rslt.prog_list.head; prog_node; prog_node = prog_node->next)
        {
                ts_prog_t *prog = (ts_prog_t *)prog_node;
                ts_table_t *table = &(prog->table);
                NODE *node;

                if(0xFF == table->version_number)
                {
                        return 0;
                }

                section_number = 0;
                for(node = table->section_list.head; node; node = node->next)
                {
                        ts_section_t *section = (ts_section_t *)node;

                        if(section_number > table->last_section_number)
                        {
                                return 0;
                        }
                        if(section_number != section->section_number)
                        {
                                return 0;
                        }
                        section_number++;
                }
        }
        return 1;
}

static int pid_type(uint16_t pid)
{
        const ts_pid_table_t *p;

        for(p = PID_TABLE; p->type != BAD_PID; p++)
        {
                if(p->min <= pid && pid <= p->max)
                {
                        break;
                }
        }

        return p->type;
}

static const table_id_table_t *table_type(uint8_t id)
{
        const table_id_table_t *p;

        for(p = TABLE_ID_TABLE; p->min != 0xFF; p++)
        {
                if(p->min <= id && id <= p->max)
                {
                        break;
                }
        }

        return p;
}

static int track_type(ts_track_t *track)
{
        const stream_type_t *p;

        for(p = STREAM_TYPE_TABLE; 0xFF != p->stream_type; p++)
        {
                if(p->stream_type == track->stream_type)
                {
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
        if(ovf < 0)
        {
                /*fprintf(stderr, "Bad overflow value(%llu)\n", ovf); */
                return 0;
        }
        if(t1 < 0 || ovf <= t1 || t2 < 0 || ovf <= t2)
        {
                /*fprintf(stderr, "Bad overflow value(%llu) for %llu and %llu\n", ovf, t1, t2); */
                return 0;
        }

        t1 += t2;
        t1 -= ((t1 >= ovf) ? ovf : 0);

        return t1;
}

/* rslt(int) = t1(uint) - t2(uint), t belongs to [0, ovf - 1] */
static int64_t lmt_min(int64_t t1, int64_t t2, int64_t ovf)
{
        if(ovf < 0)
        {
                /*fprintf(stderr, "Bad overflow value(%llu)\n", ovf); */
                return 0;
        }
        if(t1 < 0 || ovf <= t1 || t2 < 0 || ovf <= t2)
        {
                /*fprintf(stderr, "Bad overflow value(%llu) for %llu and %llu\n", ovf, t1, t2); */
                return 0;
        }

        t1 += ((t1 < t2) ? ovf : 0);
        t1 -= t2;
        if(t1 >= (ovf >> 1))
        {
                t1 -= ovf;
        }

        return t1;
}

static int dump(uint8_t *buf, int len)
{
        uint8_t *p = buf;
        int i;

        for(i = 0; i < len; i++)
        {
                fprintf(stderr, "%02X ", *p++);
        }
        fprintf(stderr, "\n");

        return 0;
}
