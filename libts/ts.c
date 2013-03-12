/* vim: set tabstop=8 shiftwidth=8:
 * name: ts.c
 * funx: analyse ts stream
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* for uint?_t, etc */
#include <string.h> /* for memset, memcpy, etc */

#include "common.h"
#include "if.h" /* for dump(uint8_t *buf, int len) */
#include "crc.h"
#include "buddy.h"
#include "ts.h"

#define PKT_SIZE (188)
#define BIT(n) (1<<(n))

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

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

struct ts_obj {
        int state;
        intptr_t mp; /* id of memory pool, for list malloc and free */
        int need_pes_align; /* 0: dot't need; 1: need PES align */
        int is_verbose; /* 0: shut up; 1: report key step */
        struct ts_rslt rslt;

        /* special variables for packet analyse */
        uint8_t *cur; /* point to the current data in rslt.TS[] */
        uint8_t *tail; /* point to the next data after rslt.TS[] */
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
        {0x1C, AUD_PID, "MPEG-4", "ISO/IEC 14496-3 Audio, without using any additional transport syntax, such as DST, ALS and SLS"},
        {0x1D, USR_PID, "MPEG-4", "ISO/IEC 14496-17 Text"},
        {0x1E, VID_PID, "MPEG-4", "Auxiliary video stream as defined in ISO/IEC 23002-3"},
        {0x42, VID_PID, "AVS", "Advanced Video Standard"},
        {0x7F, USR_PID, "IPMP", "IPMP stream"},
        {0x80, VID_PID, "SVAC|LPCM", "SVAC, LPCM of ATSC"},
        {0x81, AUD_PID, "AC3", "Dolby Digital ATSC"},
        {0x82, AUD_PID, "DTS", "DTS Audio"},
        {0x83, AUD_PID, "MLP", "MLP"},
        {0x84, AUD_PID, "DDP", "Dolby Digital Plus"},
        {0x85, AUD_PID, "DTSHD", "DTSHD"},
        {0x86, AUD_PID, "DTSHD_XLL", "DTSHD_XLL"},
        {0x90, AUD_PID, "G.711", "G.711(A)"},
        {0x92, AUD_PID, "G.722.1", "G.722.1"},
        {0x93, AUD_PID, "G.723.1", "G.723.1"},
        {0x99, AUD_PID, "G.729", "G.729"},
        {0x9A, AUD_PID, "AMR-NB", "AMR-NB"},
        {0x9B, AUD_PID, "SVAC", "SVAC"},
        {0xA1, AUD_PID, "DDP_2", "Dolby Digital Plus"},
        {0xA2, AUD_PID, "DTSHD_2", "DTSHD_2"},
        {0xEA, VID_PID, "VC1", "VC1"},
        {0xEA, AUD_PID, "WMA", "WMA"},
        {0xFF, UNO_PID, "UNKNOWN", "Unknown stream"}, /* loop stop condition! */
};

enum {
        /* note, in any state:  */
        /*     * meet new PID -> add to pid_list */
        /*     * meet new table -> add to section_list and parse */
        /*     * meet old table -> if it was modified, report a warning */
        STATE_NEXT_PAT, /* colloct PAT section, parse to create prog_list */
        STATE_NEXT_PMT, /* collect PMT section, parse to create elem_list */
        STATE_NEXT_PKT  /* parse packet with current lists */
};

static int state_next_pat(struct ts_obj *obj);
static int state_next_pmt(struct ts_obj *obj);
static int state_next_pkt(struct ts_obj *obj);

static int ts_parse_af(struct ts_obj *obj); /* Adaption Fields information */
static int ts_ts2sect(struct ts_obj *obj); /* collect PSI/SI section data */
static int ts_parse_sect(struct ts_obj *obj);
static int ts_parse_sech(struct ts_obj *obj, uint8_t *sect);
static int ts_parse_secb_pat(struct ts_obj *obj, uint8_t *sect);
static int ts_parse_secb_pmt(struct ts_obj *obj, uint8_t *sect);
static int ts_parse_secb_sdt(struct ts_obj *obj, uint8_t *sect);
static int ts_parse_pesh(struct ts_obj *obj); /* PES layer information */
static int ts_parse_pesh_switch(struct ts_obj *obj);
static int ts_parse_pesh_detail(struct ts_obj *obj);

static struct ts_pid *add_to_pid_list(struct ts_obj *obj, struct ts_pid **phead, struct ts_pid *pid);
static struct ts_pid *add_new_pid(struct ts_obj *obj);
static int is_all_prog_parsed(struct ts_obj *obj);
static int pid_type(uint16_t pid);
static const struct table_id_table *table_type(uint8_t id);
static int elem_type(struct ts_elem *elem);

intptr_t ts_create(struct ts_rslt **rslt, size_t mp_order)
{
        struct ts_obj *obj;

        obj = (struct ts_obj *)malloc(sizeof(struct ts_obj));
        if(!obj) {
                RPT(RPT_ERR, "malloc failed");
                return (intptr_t)NULL;
        }

        obj->mp = buddy_create(mp_order, 6); /* borrow a big memory from OS */
        if(0 == obj->mp) {
                RPT(RPT_ERR, "malloc memory pool failed");
                return (intptr_t)NULL;
        }
        buddy_init(obj->mp); /* now, we can use xx_malloc() */

        /* prepare for ts_init() */
        /* do NOT forgot to call ts_init() before use */
        obj->rslt.pid0 = NULL; /* no pid list now */
        obj->rslt.prog0 = NULL; /* no prog list now */
        obj->rslt.table0 = NULL; /* no table list now */

        *rslt = &(obj->rslt);
        return (intptr_t)obj;
}

int ts_destroy(intptr_t id)
{
        struct ts_obj *obj;

        obj = (struct ts_obj *)id;
        if(!obj) {
                RPT(RPT_ERR, "bad id");
                return -1;
        }

        ts_init(id); /* free all list */
        buddy_destroy(obj->mp); /* return the memory to OS */
        free(obj);

        return 0;
}

int ts_init(intptr_t id)
{
        struct ts_obj *obj;

        obj = (struct ts_obj *)id;
        if(!obj) {
                RPT(RPT_ERR, "bad id");
                return -1;
        }

        struct ts_rslt *rslt = &(obj->rslt);
        struct znode *znode;
        struct znode *pop; /* temp node to free */

        /* clear pid list */
        while(NULL != (pop = zlst_pop(&(rslt->pid0)))) {
                buddy_free(obj->mp, pop);
        }

        /* clear prog list */
        for(znode = (struct znode *)(rslt->prog0); znode; znode = znode->next) {
                struct ts_prog *prog = (struct ts_prog *)znode;
                while(NULL != (pop = zlst_pop(&(prog->elem0)))) {
                        buddy_free(obj->mp, pop);
                }
                while(NULL != (pop = zlst_pop(&(prog->table_02.sect0)))) {
                        buddy_free(obj->mp, pop);
                }
        }
        while(NULL != (pop = zlst_pop(&(rslt->prog0)))) {
                buddy_free(obj->mp, pop);
        }

        /* clear table list */
        for(znode = (struct znode *)(rslt->table0); znode; znode = znode->next) {
                struct ts_table *table = (struct ts_table *)znode;
                while(NULL != (pop = zlst_pop(&(table->sect0)))) {
                        buddy_free(obj->mp, pop);
                }
        }
        while(NULL != (pop = zlst_pop(&(rslt->table0)))) {
                buddy_free(obj->mp, pop);
        }

        obj->state = STATE_NEXT_PAT;
        obj->need_pes_align = 1;
        obj->is_verbose = 0;

        rslt->ADDR = -PKT_SIZE; /* count from 0 */
        rslt->cnt = -1; /* count ts packet from 0 */
        rslt->table0 = NULL;
        rslt->has_got_transport_stream_id = 0;
        rslt->transport_stream_id = 0;
        rslt->prog0 = NULL;
        rslt->pid0 = NULL;
        rslt->cnt = 0;
        rslt->CC_lost = 0;
        rslt->is_pat_pmt_parsed = 0;
        rslt->is_psi_parse_finished = 0;
        rslt->concerned_pid = 0x0000; /* PAT_PID */
        rslt->interval = 0;
        rslt->CTS = 0L;
        rslt->CTS0 = 0L;
        rslt->lCTS = 0L; /* for MTS file only, must init as 0L */
        rslt->STC = STC_OVF;
        memset(&(rslt->err), 0, sizeof(struct ts_err)); /* clear error struct */

        return 0;
}

int ts_parse_tsh(intptr_t id)
{
        struct ts_obj *obj;
        struct ts_rslt *rslt;

        obj = (struct ts_obj *)id;
        if(!obj) {
                RPT(RPT_ERR, "ts_parse_tsh: bad id");
                return -1;
        }
        rslt = &(obj->rslt);

        if(!rslt->ts) {
                RPT(RPT_ERR, "ts_parse_tsh: no ts packet");
                return -1;
        }
        if(!(rslt->addr)) {
                rslt->ADDR += PKT_SIZE;
                rslt->addr = &(rslt->ADDR);
        }
        obj->cur = rslt->ts;
        obj->tail = rslt->ts + PKT_SIZE;
        rslt->cnt++;
#if 0
        dump(rslt->TS, PKT_SIZE); /* debug only */
        fprintf(stderr, "cnt: %lld, addr: %lld\n", rslt->cnt, rslt->ADDR); /* debug only */
#endif

        uint8_t dat;
        struct ts_tsh *tsh = &(rslt->tsh);
        struct ts_err *err = &(rslt->err);
        struct ts_pid *pid; /* may be NULL */
        struct ts_prog *prog; /* may be NULL */

        /* init for a new ts packet */
        rslt->stc = NULL; /* no STC */
        rslt->pcr = NULL; /* no PCR */
        rslt->pts = NULL; /* no PTS */
        rslt->dts = NULL; /* no DTS */
        rslt->is_psi_si = 0; /* not PSI/SI */
        rslt->has_sect = 0; /* not an end of a section */
        rslt->has_rate = 0; /* not a new rate calculate peroid */
        rslt->AF_len = 0; /* no AF */
        rslt->PES_len = 0; /* no PES */
        rslt->ES_len = 0; /* no ES */

        /* begin */
        dat = *(obj->cur)++;
        tsh->sync_byte = dat;
        if(0x47 != tsh->sync_byte) {
                err->Sync_byte_error++;
                if(err->Sync_byte_error > 1) {
                        err->TS_sync_loss++;
                }
                fprintf(stderr, "sync_byte(0x%02X) error!\n", tsh->sync_byte);
                dump(rslt->TS, PKT_SIZE);
        }
        else {
                err->TS_sync_loss = 0;
                err->Sync_byte_error = 0;
        }

        dat = *(obj->cur)++;
        tsh->transport_error_indicator = (dat & BIT(7)) >> 7;
        tsh->payload_unit_start_indicator = (dat & BIT(6)) >> 6;
        tsh->transport_priority = (dat & BIT(5)) >> 5;
        tsh->PID = dat & 0x1F;
        err->Transport_error = tsh->transport_error_indicator;

        dat = *(obj->cur)++;
        tsh->PID <<= 8;
        tsh->PID |= dat;

        dat = *(obj->cur)++;
        tsh->transport_scrambling_control = (dat & (BIT(7) | BIT(6))) >> 6;
        tsh->adaption_field_control = (dat & (BIT(5) | BIT(4))) >> 4;;
        tsh->continuity_counter = dat & 0x0F;

        if(0x00 == tsh->adaption_field_control) {
                fprintf(stderr, "Bad adaption_field_control field(00)!\n");
        }

        if(BIT(1) & tsh->adaption_field_control) {
                ts_parse_af(obj);
        }

        if(BIT(0) & tsh->adaption_field_control) {
                /* data_byte, PSI or PES */
        }

        rslt->PID = tsh->PID; /* record into rslt struct */
        RPT(RPT_DBG, "search 0x%04X in pid_list", rslt->PID);
        rslt->pid = (struct ts_pid *)zlst_search(&(rslt->pid0), rslt->PID);
        if(!(rslt->pid)) {
                /* find new PID, add it in pid_list */
                rslt->pid = add_new_pid(obj);
        }
        pid = rslt->pid;

        /* calc STC and CTS, should be as early as possible */
        if(rslt->mts) {
                int64_t dCTS = timestamp_diff(rslt->MTS, rslt->lCTS, MTS_OVF);

                if(STC_OVF != rslt->STC) {
                        rslt->STC = timestamp_add(rslt->STC, dCTS, STC_OVF);
                }
                else {
                        rslt->STC = timestamp_add(0L, dCTS, STC_OVF);
                }
                rslt->lCTS = rslt->MTS; /* record last CTS */

                rslt->CTS = rslt->STC;
        }
        else {
                /* STC: according to pid->prog */
                if(pid && pid->prog) {
                        prog = pid->prog;
                        if((prog->STC_sync) &&
                           (prog->PCRa != prog->PCRb)) {
                                long double delta;

                                /* STCx - PCRb   ADDx - ADDb */
                                /* ----------- = ----------- */
                                /* PCRb - PCRa   ADDb - ADDa */
                                delta = (long double)timestamp_diff(prog->PCRb, prog->PCRa, STC_OVF);
                                delta *= (rslt->ADDR - prog->ADDb);
                                delta /= (prog->ADDb - prog->ADDa);
                                rslt->STC = timestamp_add(prog->PCRb, (int64_t)delta, STC_OVF);
                                rslt->stc = &(rslt->STC);
                        }
                }

                /* CTS: according to prog0 */
                if(!(rslt->cts) && rslt->prog0) {
                        prog = rslt->prog0;
                        if((prog->STC_sync) &&
                           (prog->PCRa != prog->PCRb)) {
                                long double delta;

                                /* CTSx - PCRb   ADDx - ADDb */
                                /* ----------- = ----------- */
                                /* PCRb - PCRa   ADDb - ADDa */
                                delta = (long double)timestamp_diff(prog->PCRb, prog->PCRa, STC_OVF);
                                delta *= (rslt->ADDR - prog->ADDb);
                                delta /= (prog->ADDb - prog->ADDa);
                                rslt->CTS = timestamp_add(prog->PCRb, (int64_t)delta, STC_OVF);
                                rslt->cts = &(rslt->CTS);
                        }
                }
        }
        rslt->STC_base = rslt->STC / 300;
        rslt->STC_ext = rslt->STC % 300;
        rslt->CTS_base = rslt->CTS / 300;
        rslt->CTS_ext = rslt->CTS % 300;

        /* statistic & PSI/SI section collect */
        pid->cnt++;
        rslt->sys_cnt++;
        rslt->nul_cnt += ((0x1FFF == tsh->PID) ? 1 : 0);
        if((tsh->PID < 0x0020) || (PMT_PID == pid->type)) {
                /* PSI/SI packet */
                rslt->psi_cnt++;
                rslt->is_psi_si = 1;
                /*fprintf(stderr, "PSI/SI: 0x%04X\n", tsh->PID);*/
                ts_ts2sect(obj);
        }

        return 0;
}

int ts_parse_tsb(intptr_t id)
{
        struct ts_obj *obj;

        obj = (struct ts_obj *)id;
        if(!obj) {
                RPT(RPT_ERR, "bad id");
                return -1;
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

static int state_next_pat(struct ts_obj *obj)
{
        struct ts_tsh *tsh = &(obj->rslt.tsh);
        struct ts_table *table;
        uint8_t section_number;

        if(0x0000 != tsh->PID) {
                return -1; /* not PAT */
        }

        /* section parse has done in ts_parse_tsh()! */
        RPT(RPT_DBG, "search 0x00 in table_list");
        table = (struct ts_table *)zlst_search(&(obj->rslt.table0), 0x00);
        if(!table) {
                return -1;
        }
        else {
                struct znode *znode;

                section_number = 0;
                for(znode = (struct znode *)(table->sect0); znode; znode = znode->next) {
                        struct ts_sect *sect = (struct ts_sect *)znode;

                        if(section_number > table->last_section_number) {
                                return -1;
                        }
                        if(section_number != sect->section_number) {
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

static int state_next_pmt(struct ts_obj *obj)
{
        struct ts_tsh *tsh = &(obj->rslt.tsh);
        struct ts_pid *pid;

        RPT(RPT_DBG, "search 0x%04X in pid_list", tsh->PID);
        pid = (struct ts_pid *)zlst_search(&(obj->rslt.pid0), tsh->PID);
        if((!pid) || (PMT_PID != pid->type)) {
                return -1; /* not PMT */
        }

        /* section parse has done in ts_parse_tsh()! */
        if(is_all_prog_parsed(obj)) {
                if(obj->is_verbose) {
                        fprintf(stdout, "all PMT section parsed\n");
                }
                obj->rslt.is_pat_pmt_parsed = 1;
                obj->state = STATE_NEXT_PKT;
        }

        return 0;
}

static int state_next_pkt(struct ts_obj *obj)
{
        struct ts_tsh *tsh = &(obj->rslt.tsh);
        struct ts_af *af = &(obj->rslt.af);
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_pid *pid = rslt->pid;
        struct ts_elem *elem = pid->elem; /* may be NULL */
        struct ts_prog *prog = pid->prog; /* may be NULL */
        struct ts_err *err = &(rslt->err);

        /* CC */
        if(pid->is_CC_sync) {
                uint8_t dCC;
                int lost;

                if((1 == tsh->adaption_field_control) || /* 01 */
                   (3 == tsh->adaption_field_control)) { /* 11 */
                        dCC = 1;
                }
                else { /* 00 or 10 */
                        dCC = 0;
                }

                pid->CC += dCC;
                pid->CC &= 0x0F; /* 4-bit */
                lost  = (int)tsh->continuity_counter;
                lost -= (int)pid->CC;
                if(lost < 0) {
                        lost += 16;
                }

                rslt->CC_wait = pid->CC;
                rslt->CC_find = tsh->continuity_counter;
                rslt->CC_lost = lost;
        }
        else {
                pid->is_CC_sync = 1;

                rslt->CC_wait = pid->CC;
                rslt->CC_find = tsh->continuity_counter;
                rslt->CC_lost = 0;
        }
        pid->CC = tsh->continuity_counter; /* update CC */
        err->Continuity_count_error = rslt->CC_lost;
        if(0x1FFF == tsh->PID) {
                /* continuity_counter of null packet is undefined */
                err->Continuity_count_error = 0;
        }

        /* PCR flush */
        if(rslt->pcr) {
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
                        if(STC_OVF != prog->PCRb) {
                                rslt->PCR_interval = timestamp_diff(rslt->STC, prog->PCRb, STC_OVF);
                                if((prog->STC_sync) &&
                                   !(0 < rslt->PCR_interval && rslt->PCR_interval <= 40 * STC_MS)) {
                                        /* !(0 < interval < +40ms) */
                                        err->PCR_repetition_error = 1;
                                }
                        }
                        else {
                                rslt->PCR_interval = 0;
                        }

                        /* PCR_continuity (PCR value interval) */
                        if(STC_OVF != prog->PCRb) {
                                rslt->PCR_continuity = timestamp_diff(rslt->PCR, prog->PCRb, STC_OVF);
                                if((prog->STC_sync) && !(af->discontinuity_indicator) &&
                                   !(0 < rslt->PCR_continuity && rslt->PCR_continuity <= 100 * STC_MS)) {
                                        /* !(0 < continuity < +100ms) */
                                        err->PCR_discontinuity_indicator_error = 1;
                                }
                        }
                        else {
                                rslt->PCR_continuity = 0;
                        }

                        /* PCR_jitter */
                        rslt->PCR_jitter = timestamp_diff(rslt->PCR, rslt->STC, STC_OVF);
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
                        prog->ADDb = rslt->ADDR;

                        /* STC_sync */
                        if(!prog->STC_sync) {
                                int is_first_count_clear = 0;

                                if(rslt->mts) {
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
                                        struct znode *znode;

                                        for(znode = (struct znode *)(rslt->pid0); znode; znode = znode->next) {
                                                struct ts_pid *pid_item = (struct ts_pid *)znode;
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
                                        rslt->CTS = rslt->PCR;
                                        rslt->CTS0 = rslt->CTS;
                                }
                        }
                }
                else {
                        fprintf(stderr, "No program use this PCR packet(0x%04X)!\n", tsh->PID);
                }
        }

        /* interval and statistic */
        if(rslt->prog0 && rslt->prog0->STC_sync) {
                rslt->interval = timestamp_diff(rslt->CTS, rslt->CTS0, STC_OVF);
                if(rslt->interval >= rslt->aim_interval) {
                        struct znode *znode;

                        /* calc bitrate and clear the packet count */
                        for(znode = (struct znode *)(rslt->pid0); znode; znode = znode->next) {
                                struct ts_pid *pid_item = (struct ts_pid *)znode;
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
                        rslt->CTS0 = rslt->CTS;

                        rslt->has_rate = 1;

                        rslt->is_psi_parse_finished = 1;
                }
        }

        /* PES head & ES data */
        if(elem && (0 == tsh->transport_scrambling_control)) {
                if(AUD_PID == pid->type || AUD_PCR == pid->type ||
                   VID_PID == pid->type || VID_PCR == pid->type) {
                        ts_parse_pesh(obj);
                }

                if(rslt->pts) {
                        /* PTS */
                        if(STC_BASE_OVF != elem->PTS) {
                                rslt->PTS_interval = timestamp_diff(rslt->PTS, elem->PTS, STC_BASE_OVF);
                        }
                        else {
                                rslt->PTS_interval = 0;
                        }
                        if(STC_BASE_OVF != rslt->STC_base) {
                                rslt->PTS_minus_STC = timestamp_diff(rslt->PTS, rslt->STC_base, STC_BASE_OVF);
                        }
                        else {
                                rslt->PTS_minus_STC = 0;
                        }
                        elem->PTS = rslt->PTS; /* record last PTS in elem */

                        /* DTS, if no DTS, DTS = PTS */
                        if(STC_BASE_OVF != elem->DTS) {
                                rslt->DTS_interval = timestamp_diff(rslt->DTS, elem->DTS, STC_BASE_OVF);
                        }
                        else {
                                rslt->DTS_interval = 0;
                        }
                        if(STC_BASE_OVF != rslt->STC_base) {
                                rslt->DTS_minus_STC = timestamp_diff(rslt->DTS, rslt->STC_base, STC_BASE_OVF);
                        }
                        else {
                                rslt->DTS_minus_STC = 0;
                        }
                        elem->DTS = rslt->DTS; /* record last DTS in elem */
                }
        }

        return 0;
}

static int ts_parse_af(struct ts_obj *obj)
{
        int i;
        uint8_t dat;
        struct ts_af *af = &(obj->rslt.af);
        struct ts_rslt *rslt = &(obj->rslt);

        rslt->AF = obj->cur;
        dat = *(obj->cur)++;
        af->adaption_field_length = dat;
        rslt->AF_len = af->adaption_field_length + 1; /* add length itself */
        if(0x00 == af->adaption_field_length) {
                return 0;
        }

        uint8_t *tail = obj->cur + af->adaption_field_length; /* point to the data after AF */

        dat = *(obj->cur)++;
        af->discontinuity_indicator = (dat & BIT(7)) >> 7;
        af->random_access_indicator = (dat & BIT(6)) >> 6;
        af->elementary_stream_priority_indicator = (dat & BIT(5)) >> 5;
        af->PCR_flag = (dat & BIT(4)) >> 4;
        af->OPCR_flag = (dat & BIT(3)) >> 3;
        af->splicing_pointer_flag = (dat & BIT(2)) >> 2;
        af->transport_private_data_flag = (dat & BIT(1)) >> 1;
        af->adaption_field_extension_flag = (dat & BIT(0)) >> 0;

        if(af->PCR_flag) {
                dat = *(obj->cur)++;
                af->program_clock_reference_base = dat;

                dat = *(obj->cur)++;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->cur)++;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->cur)++;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->cur)++;
                af->program_clock_reference_base <<= 1;
                af->program_clock_reference_base |= ((dat & BIT(7)) >> 7);
                af->program_clock_reference_extension = ((dat & BIT(0)) >> 0);

                dat = *(obj->cur)++;
                af->program_clock_reference_extension <<= 8;
                af->program_clock_reference_extension |= dat;

                rslt->pcr = &(rslt->PCR);
        }
        if(af->OPCR_flag) {
                dat = *(obj->cur)++;
                af->original_program_clock_reference_base = dat;

                dat = *(obj->cur)++;
                af->original_program_clock_reference_base <<= 8;
                af->original_program_clock_reference_base |= dat;

                dat = *(obj->cur)++;
                af->original_program_clock_reference_base <<= 8;
                af->original_program_clock_reference_base |= dat;

                dat = *(obj->cur)++;
                af->original_program_clock_reference_base <<= 8;
                af->original_program_clock_reference_base |= dat;

                dat = *(obj->cur)++;
                af->original_program_clock_reference_base <<= 1;
                af->original_program_clock_reference_base |= ((dat & BIT(7)) >> 7);
                af->original_program_clock_reference_extension = ((dat & BIT(0)) >> 0);

                dat = *(obj->cur)++;
                af->original_program_clock_reference_extension <<= 8;
                af->original_program_clock_reference_extension |= dat;
        }
        if(af->splicing_pointer_flag) {
                dat = *(obj->cur)++;
                af->splice_countdown = dat;
        }
        if(af->transport_private_data_flag) {
                dat = *(obj->cur)++;
                af->transport_private_data_length = dat;

                for(i = 0; i < af->transport_private_data_length; i++) {
                        af->private_data_byte[i] = dat;
                }
        }
        if(af->adaption_field_extension_flag) {
                dat = *(obj->cur)++;
                af->adaption_field_extension_length = dat;

                /* pass adaption_field_extension part, FIXME */
                obj->cur += af->adaption_field_extension_length;
        }

        /* pass stuffing byte */
        obj->cur = tail;

        return 0;
}

static int ts_ts2sect(struct ts_obj *obj)
{
        uint8_t pointer_field;
        struct ts_tsh *tsh = &(obj->rslt.tsh);
        struct ts_sech *sech = &(obj->rslt.sech);
        struct ts_pid *pid = obj->rslt.pid;

        /* FIXME: if data after CRC_32 is NOT 0xFF, it's another section! */
        if(0 == pid->sect_idx) {
                /* waiting for section head */
                if(1 == tsh->payload_unit_start_indicator) {
                        pointer_field = *(obj->cur)++;

                        /* section head */
                        obj->cur += pointer_field;
                        memcpy(pid->sect_data, obj->cur, obj->tail - obj->cur);
                        pid->sect_idx = obj->tail - obj->cur;
                }
                else { /* (0 == tsh->payload_unit_start_indicator) */
                        /* section async, ignore this packet! */
                        /*dump(obj->cur, obj->tail - obj->cur); */
                }
        }
        else { /* (0 != pid->sect_idx) */
                /* collect section data */
                if(1 == tsh->payload_unit_start_indicator) {
                        pointer_field = *(obj->cur)++;

                        /* section tail */
                        memcpy(pid->sect_data + pid->sect_idx, obj->cur, pointer_field);
                        pid->sect_idx += pointer_field;
                        ts_parse_sect(obj);

                        /* section head */
                        obj->cur += pointer_field;
                        memcpy(pid->sect_data, obj->cur, obj->tail - obj->cur);
                        pid->sect_idx = obj->tail - obj->cur;
                }
                else { /* (0 == tsh->payload_unit_start_indicator) */
                        /* section body */
                        memcpy(pid->sect_data + pid->sect_idx, obj->cur, 184);
                        pid->sect_idx += 184;
                }
        }

        if(pid->sect_idx >= 8) {
                /* PSI head OK */
                if(0 != ts_parse_sech(obj, pid->sect_data)) {
                        pid->sect_idx = 0;
                        return -1;
                }

                if(pid->sect_idx >= (3 + sech->section_length)) {
                        /* section data enough */
                        ts_parse_sect(obj);
                        pid->sect_idx = 0;
                }
        }

        return 0;
}

static int ts_parse_sect(struct ts_obj *obj)
{
        uint8_t *p;
        struct ts_table *table;
        struct znode **psect0;
        struct ts_sect *sect;
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_pid *pid = rslt->pid;
        struct ts_sech *sech = &(rslt->sech);
        struct ts_err *err = &(rslt->err);
#if 0
        int is_new_version = 0;
#endif

        /* get psi head info */
        if(0 != ts_parse_sech(obj, pid->sect_data)) {
                return -1;
        }

        /* CRC check */
        if(sech->check_CRC) {
                p = pid->sect_data + 3 + sech->section_length - 4;
                pid->CRC_32   = *p++;
                pid->CRC_32 <<= 8;
                pid->CRC_32  |= *p++;
                pid->CRC_32 <<= 8;
                pid->CRC_32  |= *p++;
                pid->CRC_32 <<= 8;
                pid->CRC_32  |= *p++;

                pid->CRC_32_calc = CRC_for_TS(pid->sect_data, 3 + sech->section_length - 4, 32);
                /*pid->CRC_32_calc = CRC(pid->sect_data, 3 + sech->section_length - 4, 32); */
                /*pid->CRC_32_calc = crc32(pid->sect_data, 3 + sech->section_length - 4); */
                if(pid->CRC_32_calc != pid->CRC_32) {
                        err->CRC_error = 1;
                        fprintf(stderr, "CRC error(0x%08X! 0x%08X?)\n",
                                pid->CRC_32_calc, pid->CRC_32);
                        dump(pid->sect_data, 3 + sech->section_length);
                        return -1;
                }
        }

        /* get "table" and "psect0" */
        if(0x02 == sech->table_id) {
                /* PMT section */
                if(!(pid->prog)) {
                        RPT(RPT_WRN, "PMT: pid->prog is NULL");
                        return -1;
                }
                table = &(pid->prog->table_02);
        }
        else {
                /* other section */
                struct znode **ptable0 = (struct znode **)&(rslt->table0);

                RPT(RPT_DBG, "search 0x%02X in table_list", sech->table_id);
                table = (struct ts_table *)zlst_search(ptable0, sech->table_id);
                if(!table) {
                        /* add table */
                        table = (struct ts_table *)buddy_malloc(obj->mp, sizeof(struct ts_table));
                        if(!table) {
                                RPT(RPT_ERR, "malloc failed");
                                return -1;
                        }

                        table->table_id = sech->table_id;
                        table->version_number = sech->version_number;
                        table->last_section_number = sech->last_section_number;
                        table->sect0 = NULL;
                        table->STC = STC_OVF;
                        RPT(RPT_DBG, "insert 0x%02X in table_list", sech->table_id);
                        zlst_set_key(table, sech->table_id);
                        if(zlst_insert(ptable0, table)) {
                                buddy_free(obj->mp, table);
                        }
                }
        }
        psect0 = (struct znode **)&(table->sect0);

        /* new table version? */
        if(table->version_number != sech->version_number) {
                struct znode *znode;

                /* clear psect0 and update table parameter */
                RPT(RPT_DBG, "version_number(%d -> %d), free section",
                    table->version_number,
                    sech->version_number);
                table->version_number = sech->version_number;
                table->last_section_number = sech->last_section_number;
                while(NULL != (znode = zlst_pop(psect0))) {
                        buddy_free(obj->mp, znode);
                };
#if 0
                is_new_version = 1;
#endif
        }

        /* get "section" pointer */
        RPT(RPT_DBG, "search 0x%02X in section_list", sech->section_number);
        sect = (struct ts_sect *)zlst_search(psect0, sech->section_number);
        if(!sect) {
                /* add section */
                sect = (struct ts_sect *)buddy_malloc(obj->mp, sizeof(struct ts_sect));
                if(!sect) {
                        RPT(RPT_ERR, "malloc failed");
                        return -1;
                }

                sect->section_number = sech->section_number;
                sect->section_length = sech->section_length;
                memcpy(sect->data, pid->sect_data, 3 + sech->section_length);
                RPT(RPT_DBG, "insert 0x%02X in sect_list", sech->section_number);
                zlst_set_key(sect, sech->section_number);
                if(zlst_insert(psect0, sect)) {
                        buddy_free(obj->mp, sect);
                }
        }
        else {
                /*fprintf(stderr, "has section %02X/%02X(table %02X)\n", */
                /*        sech->sect_number, */
                /*        sech->last_section_number, */
                /*        sech->table_id); */
        }

        /* sect_interval */
        if(STC_OVF != table->STC &&
           STC_OVF != rslt->STC) {
                pid->sect_interval = timestamp_diff(rslt->STC, table->STC, STC_OVF);
        }
        else {
                pid->sect_interval = 0;
        }
        table->STC = rslt->STC;

        /* PAT_error(table_id error) */
        if(0x0000 == pid->PID && 0x00 != sech->table_id) {
                err->PAT_error = ERR_1_3_1;
                dump(rslt->TS, PKT_SIZE);
                dump(pid->sect_data, 8);
                return -1;
        }

        /* parse */
        rslt->has_sect = 1;
        switch(sech->table_id) {
                case 0x00:
                        if(0x0000 != pid->PID) {
                                fprintf(stderr, "PAT: PID is not 0x0000 but 0x%04X, ignore!\n", pid->PID);
                                return -1;
                        }
                        ts_parse_secb_pat(obj, pid->sect_data);
                        break;
                case 0x02:
                        if(PMT_PID != pid->type) {
                                fprintf(stderr, "PMT: PID is NOT PMT_PID but 0x%04X, ignore!\n", pid->PID);
                                return -1;
                        }
                        ts_parse_secb_pmt(obj, pid->sect_data);
                        break;
                case 0x42:
                        if(0x0011 != pid->PID) {
                                fprintf(stderr, "SDT: PID is not 0x0011 but 0x%04X, ignore!\n", pid->PID);
                                return -1;
                        }
                        ts_parse_secb_sdt(obj, pid->sect_data);
                        break;
                default:
                        break;
        }
        return 0;
}

static int ts_parse_sech(struct ts_obj *obj, uint8_t *sect)
{
        uint8_t *p;
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_sech *sech = &(rslt->sech);
        const struct table_id_table *table;

        p = sect;
        sech->table_id = *p++;
        sech->section_syntax_indicator = (*p & BIT(7)) >> 7;
        sech->private_indicator = (*p & BIT(6)) >> 6;
        sech->section_length   = *p++ & 0x0F;
        sech->section_length <<= 8;
        sech->section_length  |= *p++;
        if(1 == sech->section_syntax_indicator) {
                /* normal section syntax */
                sech->table_id_extension   = *p++;
                sech->table_id_extension <<= 8;
                sech->table_id_extension  |= *p++;
                sech->version_number = (*p & 0x3E) >> 1;
                sech->current_next_indicator = *p++ & BIT(0);
                sech->section_number = *p++;
                sech->last_section_number = *p++;

                table = table_type(sech->table_id);
                sech->check_CRC = table->check_CRC;
                sech->type = table->type;
        }
        else {
                /* abnormal section syntax */
                sech->table_id_extension = 0;
                sech->version_number = 0;
                sech->current_next_indicator = 0;
                sech->section_number = 0;
                sech->last_section_number = 0;

                sech->check_CRC = 0;
                sech->type = USR_PID;
        }

#if 0
        /* for debug only */
        fprintf(stderr, "table, 0x%02X, len %4d, sect, %d/%d\n",
                sech->table_id,
                sech->section_length,
                sech->section_number,
                sech->last_section_number);
#endif

        if(0 == sech->private_indicator) {
                /* normal section */
                if(sech->section_length > 1021) {
                        fprintf(stderr, "normal section(0x%02X), length(%d) overflow!\n",
                                sech->table_id,
                                sech->section_length);
                        sech->section_length = 1021;
                        dump(rslt->TS, PKT_SIZE);
                        dump(sect, 8);
                        return -1;
                }
        }
        else {
                /* private section */
                if(sech->section_length > 4093) {
                        fprintf(stderr, "private section(0x%02X), length(%d) overflow!\n",
                                sech->table_id,
                                sech->section_length);
                        sech->section_length = 4093;
                        dump(rslt->TS, PKT_SIZE);
                        dump(sect, 8);
                        return -1;
                }
        }

        return 0;
}

static int ts_parse_secb_pat(struct ts_obj *obj, uint8_t *sect)
{
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_sech *sech = &(rslt->sech);
        struct ts_tsh *tsh = &(rslt->tsh);
        struct ts_err *err = &(rslt->err);
        struct ts_pid *pid = rslt->pid;
        uint8_t dat;
        uint8_t *cur = sect + 8;
        uint8_t *crc = sect + 3 + sech->section_length - 4;
        struct ts_prog *prog;
        struct ts_pid ts_new_pid, *new_pid = &ts_new_pid;

        /* PAT_error */
        if(pid->sect_interval > 500 * STC_MS) {
                err->PAT_error = ERR_1_3_0;
        }
        if(0x00 != tsh->transport_scrambling_control) {
                err->PAT_error = ERR_1_3_2;
        }

        /* to avoid stack overflow, FIXME */
        if(rslt->prog0) {
                return 0;
        }

        /* in PAT, table_id_extension is transport_stream_id */
        rslt->transport_stream_id = sech->table_id_extension;
        rslt->has_got_transport_stream_id = 1;

        while(cur < crc) {
                /* add program */
                prog = (struct ts_prog *)buddy_malloc(obj->mp, sizeof(struct ts_prog));
                if(!prog) {
                        RPT(RPT_ERR, "malloc failed");
                        return -1;
                }

                dat = *cur++;
                prog->program_number = dat;

                dat = *cur++;
                prog->program_number <<= 8;
                prog->program_number |= dat;

                dat = *cur++;
                prog->PMT_PID = dat & 0x1F;

                dat = *cur++;
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
                        buddy_free(obj->mp, prog);
                }
                else {
                        struct znode *znode;

                        new_pid->type = PMT_PID;

                        if(!(rslt->prog0)) {
                                /* traverse pid_list */
                                /* if it des not belong to any program, use prog0 */
                                for(znode = (struct znode *)(rslt->pid0); znode; znode = znode->next) {
                                        struct ts_pid *pid_item = (struct ts_pid *)znode;
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
                        prog->table_02.sect0 = NULL;
                        prog->table_02.STC = STC_OVF;

                        /* elementary stream list */
                        prog->elem0 = NULL;

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

                        RPT(RPT_DBG, "insert 0x%04X in prog_list", prog->program_number);
                        zlst_set_key(prog, prog->program_number);
                        if(zlst_insert(&(rslt->prog0), prog)) {
                                buddy_free(obj->mp, prog);
                        }
                }

                new_pid->cnt = 0;
                new_pid->lcnt = 0;
                new_pid->prog = prog;
                new_pid->elem = NULL;
                new_pid->CC = 0;
                new_pid->is_CC_sync = 0;
                new_pid->sdes = PID_TYPE[new_pid->type].sdes;
                new_pid->ldes = PID_TYPE[new_pid->type].ldes;
                new_pid->is_video = 0;
                new_pid->is_audio = 0;
                add_to_pid_list(obj, &(rslt->pid0), new_pid); /* NIT or PMT PID */
        }

        return 0;
}

static int ts_parse_secb_pmt(struct ts_obj *obj, uint8_t *sect)
{
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_sech *sech = &(rslt->sech);
        struct ts_tsh *tsh = &(rslt->tsh);
        struct ts_err *err = &(rslt->err);
        struct ts_pid *pid = rslt->pid;
        uint8_t dat;
        uint8_t *cur = sect + 8;
        uint8_t *crc = sect + 3 + sech->section_length - 4;
        struct ts_prog *prog;
        struct ts_elem *elem;
        struct ts_pid ts_new_pid, *new_pid = &ts_new_pid;

        /* PMT_error */
        if(pid->sect_interval > 500 * STC_MS) {
                err->PMT_error = ERR_1_5_0;
        }
        if(0x00 != tsh->transport_scrambling_control) {
                err->PMT_error = ERR_1_5_1;
        }

        /* in PMT, table_id_extension is program_number */
        RPT(RPT_DBG, "search 0x%04X in prog_list", sech->table_id_extension);
        prog = (struct ts_prog *)zlst_search(&(obj->rslt.prog0), sech->table_id_extension);
        if((!prog) || (prog->is_parsed)) {
                return -1; /* parsed program, ignore */
        }

        obj->rslt.is_psi_si = 1;
        prog->is_parsed = 1;

        dat = *cur++;
        prog->PCR_PID = dat & 0x1F;

        dat = *cur++;
        prog->PCR_PID <<= 8;
        prog->PCR_PID |= dat;

        /* add PCR PID */
        new_pid->PID = prog->PCR_PID;
        new_pid->cnt = 0;
        new_pid->lcnt = 0;
        new_pid->prog = prog;
        new_pid->elem = NULL;
        new_pid->type = PCR_PID;
        new_pid->CC = 0;
        new_pid->is_CC_sync = 1;
        new_pid->sdes = PID_TYPE[new_pid->type].sdes;
        new_pid->ldes = PID_TYPE[new_pid->type].ldes;
        new_pid->is_video = 0;
        new_pid->is_audio = 0;
        add_to_pid_list(obj, &(obj->rslt.pid0), new_pid); /* PCR_PID */

        /* program_info_length */
        dat = *cur++;
        prog->program_info_len = dat & 0x0F;

        dat = *cur++;
        prog->program_info_len <<= 8;
        prog->program_info_len |= dat;

        /* record program_info */
        if(prog->program_info_len > INFO_LEN_MAX) {
                fprintf(stderr, "PID(0x%04X): program_info_length(%d) too big!\n",
                        tsh->PID, prog->program_info_len);
        }

        /* record program_info */
        memcpy(prog->program_info, cur, prog->program_info_len);
        cur += prog->program_info_len;

        while(cur < crc) {
                /* add elementary stream */
                elem = (struct ts_elem *)buddy_malloc(obj->mp, sizeof(struct ts_elem));
                if(!elem) {
                        fprintf(stderr, "Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }

                dat = *cur++;
                elem->stream_type = dat;

                dat = *cur++;
                elem->PID = dat & 0x1F;

                dat = *cur++;
                elem->PID <<= 8;
                elem->PID |= dat;

                /* ES_info_length */
                dat = *cur++;
                elem->es_info_len = dat & 0x0F;

                dat = *cur++;
                elem->es_info_len <<= 8;
                elem->es_info_len |= dat;

                /* record es_info */
                if(elem->es_info_len > INFO_LEN_MAX) {
                        fprintf(stderr, "PID(0x%04X): ES_info_length(%d) too big!\n",
                                elem->PID, elem->es_info_len);
                }

                /* record es_info */
                memcpy(elem->es_info, cur, elem->es_info_len);
                cur += elem->es_info_len;

                elem_type(elem);
                if(elem->PID == prog->PCR_PID) {
                        switch(elem->type) {
                                case VID_PID: elem->type = VID_PCR; break;
                                case AUD_PID: elem->type = AUD_PCR; break;
                                default:      elem->type = UNO_PCR; break;
                        }
                }
                elem->PTS = STC_BASE_OVF;
                elem->DTS = STC_BASE_OVF;

                elem->is_pes_align = 0;

                RPT(RPT_DBG, "push 0x%04X in elem_list", elem->PID);
                zlst_push(&(prog->elem0), elem);

                /* add elementary PID */
                new_pid->PID = elem->PID;
                new_pid->cnt = 0;
                new_pid->lcnt = 0;
                new_pid->prog = prog;
                new_pid->elem = elem;
                new_pid->type = elem->type;
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
                add_to_pid_list(obj, &(obj->rslt.pid0), new_pid); /* elementary_PID */
        }

        return 0;
}

static int ts_parse_secb_sdt(struct ts_obj *obj, uint8_t *sect)
{
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_sech *sech = &(rslt->sech);
        uint8_t dat;
        uint8_t *cur = sect + 8;
        uint8_t *crc = sect + 3 + sech->section_length - 4;
        uint16_t original_network_id;

        /* in SDT, table_id_extension is transport_stream_id */
        if(rslt->has_got_transport_stream_id &&
           sech->table_id_extension != rslt->transport_stream_id) {
                fprintf(stderr, "table_id(0x%02X): table_id_extension(%d) != transport_stream_id(%d)\n",
                        sech->table_id,
                        sech->table_id_extension,
                        rslt->transport_stream_id);
                return -1; /* bad SDT table, ignore */
        }

        dat = *cur++;
        original_network_id = dat;

        dat = *cur++;
        original_network_id <<= 8;
        original_network_id |= dat;
        if(rslt->has_got_transport_stream_id &&
           original_network_id != rslt->transport_stream_id) {
#if 0
                fprintf(stderr, "table_id(0x%02X): original_network_id(%d) != transport_stream_id(%d)\n",
                        sech->table_id,
                        original_network_id,
                        rslt->transport_stream_id);
                return -1; /* bad SDT table, ignore */
#endif
        }

        dat = *cur++; /* reserved_future_use */

        while(cur < crc) {
                struct ts_prog *prog;

                uint16_t service_id;
#if 0
                uint8_t EIT_schedule_flag; /* 1-bit */
                uint8_t EIT_present_following_flag; /* 1-bit */
                uint8_t running_status; /* 3-bit */
                uint8_t free_CA_mode; /* 1-bit */
#endif
                uint16_t descriptors_loop_length; /* 12-bit */

                dat = *cur++;
                service_id = dat;

                dat = *cur++;
                service_id <<= 8;
                service_id |= dat;
                RPT(RPT_DBG, "search service_id(0x%04X) in prog_list", service_id);
                prog = (struct ts_prog *)zlst_search(&(rslt->prog0), service_id);

                dat = *cur++;
#if 0
                EIT_schedule_flag = (dat & BIT(1)) >> 1;
                EIT_present_following_flag = (dat & BIT(0)) >> 0;
#endif

                dat = *cur++;
#if 0
                running_status = (dat & 0xE0) >> 5;
                free_CA_mode = (dat & BIT(4)) >> 4;
#endif
                descriptors_loop_length = (dat & 0x0F);

                dat = *cur++;
                descriptors_loop_length <<= 8;
                descriptors_loop_length |= dat;

                while(descriptors_loop_length > 0) {
                        uint8_t tag;
                        uint8_t len;
                        uint8_t *pt;

                        tag = *cur++;
                        len = *cur++;
                        pt = cur;
                        if((0xFF == tag) || (0 == len)) {
                                return -1; /* wrong descriptor */
                        }
                        if(0x48 == tag && prog) {
                                /* service_descriptor */
                                pt++; /* ignore type */

                                prog->service_provider_len = *pt++;
                                memcpy(prog->service_provider, pt, prog->service_provider_len);
                                prog->service_provider[prog->service_provider_len] = '\0';

                                pt += prog->service_provider_len; /* pass provider */

                                prog->service_name_len = *pt++;
                                memcpy(prog->service_name, pt, prog->service_name_len);
                                prog->service_name[prog->service_name_len] = '\0';
                        }
                        cur += len;
                        descriptors_loop_length -= (len + 2); /* 2: tag & len */
                }
        }

        return 0;
}

static int ts_parse_pesh(struct ts_obj *obj)
{
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_tsh *tsh = &(rslt->tsh);
        struct ts_pesh *pesh = &(rslt->pesh);
        struct ts_pid *pid = rslt->pid;
        struct ts_elem *elem = pid->elem; /* may be NULL */
        uint8_t dat;

        /* for some bad stream */
        if(tsh->PID < 0x0020) {
                fprintf(stderr, "PES: bad PID(0x%04X)!\n", tsh->PID);
                return -1;
        }
        if(!elem) {
                fprintf(stderr, "PES: not elem PID(0x%04X)!\n", tsh->PID);
                return -1;
        }

        /* PES align */
        if(tsh->payload_unit_start_indicator) {
                elem->is_pes_align = 1;
        }

        /* record PES data */
        if(obj->need_pes_align) {
                if(elem->is_pes_align) {
                        rslt->PES_len = obj->tail - obj->cur;
                        rslt->PES = obj->cur;
                }
                else {
                        /* ignore these PES data */
                }
        }
        else {
                rslt->PES_len = obj->tail - obj->cur;
                rslt->PES = obj->cur;
        }

        /* PES head */
        if(tsh->payload_unit_start_indicator) {

                /* PES head start */
                dat = *(obj->cur)++;
                pesh->packet_start_code_prefix = dat;

                dat = *(obj->cur)++;
                pesh->packet_start_code_prefix <<= 8;
                pesh->packet_start_code_prefix |= dat;

                dat = *(obj->cur)++;
                pesh->packet_start_code_prefix <<= 8;
                pesh->packet_start_code_prefix |= dat;

                if(0x000001 != pesh->packet_start_code_prefix) {
                        fprintf(stderr, "PES packet start code prefix(0x%06X) NOT 0x000001!\n",
                                pesh->packet_start_code_prefix);
                        dump(rslt->TS, PKT_SIZE);
#if 0
                        return -1;
#endif
                }

                dat = *(obj->cur)++;
                pesh->stream_id = dat;

                dat = *(obj->cur)++;
                pesh->PES_packet_length = dat;

                dat = *(obj->cur)++;
                pesh->PES_packet_length <<= 8;
                pesh->PES_packet_length |= dat; /* 0x0000 for many video pes */
#if 0
                fprintf(stderr, "PES_packet_length = %d(0x%X)\n",
                        pesh->PES_packet_length,
                        pesh->PES_packet_length);
#endif

                ts_parse_pesh_switch(obj);
        }

        /* record ES data */
        if(obj->need_pes_align) {
                if(elem->is_pes_align) {
                        rslt->ES_len = obj->tail - obj->cur;
                        rslt->ES = obj->cur;
                }
                else {
                        /* ignore these ES data */
                }
        }
        else {
                rslt->ES_len = obj->tail - obj->cur;
                rslt->ES = obj->cur;
        }

        return 0;
}

static int ts_parse_pesh_switch(struct ts_obj *obj)
{
        struct ts_pesh *pesh = &(obj->rslt.pesh);

        switch(pesh->stream_id) {
                case 0xBE: /* padding_stream */
                        /* subsequent pesh->PES_packet_length data is padding_byte, pass */
                        if(pesh->PES_packet_length > (obj->tail - obj->cur)) {
                                fprintf(stderr, "PES_packet_length(%d) for padding_stream is too large!\n",
                                        pesh->PES_packet_length);
                                return -1;
                        }
                        obj->cur += pesh->PES_packet_length;
                        break;
                case 0xBC: /* program_stream_map */
                case 0xBF: /* private_stream_2 */
                case 0xF0: /* ECM */
                case 0xF1: /* EMM */
                case 0xFF: /* program_stream_directory */
                case 0xF2: /* DSMCC_stream */
                case 0xF8: /* ITU-T Rec. H.222.1 type E stream */
                        /* pesh->PES_packet_length data in pesh->buf is PES_packet_data_byte */
                        /* record after return */
                        break;
                default:
                        ts_parse_pesh_detail(obj);
                        break;
        }

        return 0;
}

static int ts_parse_pesh_detail(struct ts_obj *obj)
{
        struct ts_rslt *rslt = &(obj->rslt);
        struct ts_pesh *pesh = &(rslt->pesh);
        uint8_t dat;
        uint8_t *es;

        dat = *(obj->cur)++;
        pesh->PES_scrambling_control = (dat & (BIT(5) | BIT(4))) >> 4;
        pesh->PES_priority = (dat & BIT(3)) >> 3;
        pesh->data_alignment_indicator = (dat & BIT(2)) >> 2;
        pesh->copyright = (dat & BIT(1)) >> 1;
        pesh->original_or_copy = (dat & BIT(0)) >> 0;

        dat = *(obj->cur)++;
        pesh->PTS_DTS_flags = (dat & (BIT(7) | BIT(6))) >> 6;
        pesh->ESCR_flag = (dat & BIT(5)) >> 5;
        pesh->ES_rate_flag = (dat & BIT(4)) >> 4;
        pesh->DSM_trick_mode_flag = (dat & BIT(3)) >> 3;
        pesh->additional_copy_info_flag = (dat & BIT(2)) >> 2;
        pesh->PES_CRC_flag = (dat & BIT(1)) >> 1;
        pesh->PES_extension_flag = (dat & BIT(0)) >> 0;

        dat = *(obj->cur)++;
        pesh->PES_header_data_length = dat;
        if(pesh->PES_header_data_length > (obj->tail - obj->cur)) {
                fprintf(stderr, "PES_header_data_length(%d) is too long!\n",
                        pesh->PES_header_data_length);
                return -1;
        }
        es = obj->cur + pesh->PES_header_data_length;

        /* get PTS, DTS */
        if(0x02 == pesh->PTS_DTS_flags) { /* '10' */
                /* PTS */
                dat = *(obj->cur)++;
                pesh->PTS = (dat & (BIT(3) | BIT(2) | BIT(1))) >> 1;

                dat = *(obj->cur)++;
                pesh->PTS <<= 8;
                pesh->PTS |= dat;

                dat = *(obj->cur)++;
                dat >>= 1;
                pesh->PTS <<= 7;
                pesh->PTS |= dat;

                dat = *(obj->cur)++;
                pesh->PTS <<= 8;
                pesh->PTS |= dat;

                dat = *(obj->cur)++;
                dat >>= 1;
                pesh->PTS <<= 7;
                pesh->PTS |= dat;

                rslt->pts = &(rslt->PTS);
                rslt->PTS = pesh->PTS;

                /* DTS */
                rslt->DTS = pesh->PTS; /* no DTS, DTS = PTS */
        }
        else if(0x03 == pesh->PTS_DTS_flags) { /* '11' */
                /* PTS */
                dat = *(obj->cur)++;
                pesh->PTS = (dat & (BIT(3) | BIT(2) | BIT(1))) >> 1;

                dat = *(obj->cur)++;
                pesh->PTS <<= 8;
                pesh->PTS |= dat;

                dat = *(obj->cur)++;
                dat >>= 1;
                pesh->PTS <<= 7;
                pesh->PTS |= dat;

                dat = *(obj->cur)++;
                pesh->PTS <<= 8;
                pesh->PTS |= dat;

                dat = *(obj->cur)++;
                dat >>= 1;
                pesh->PTS <<= 7;
                pesh->PTS |= dat;

                rslt->pts = &(rslt->PTS);
                rslt->PTS = pesh->PTS;

                /* DTS */
                dat = *(obj->cur)++;
                pesh->DTS = (dat & (BIT(3) | BIT(2) | BIT(1))) >> 1;

                dat = *(obj->cur)++;
                pesh->DTS <<= 8;
                pesh->DTS |= dat;

                dat = *(obj->cur)++;
                dat >>= 1;
                pesh->DTS <<= 7;
                pesh->DTS |= dat;

                dat = *(obj->cur)++;
                pesh->DTS <<= 8;
                pesh->DTS |= dat;

                dat = *(obj->cur)++;
                dat >>= 1;
                pesh->DTS <<= 7;
                pesh->DTS |= dat;

                rslt->dts = &(rslt->DTS);
                rslt->DTS = pesh->DTS;
        }
        else if(0x01 == pesh->PTS_DTS_flags) { /* '01' */
                fprintf(stderr, "PTS_DTS_flags error!\n");
                dump(rslt->TS, PKT_SIZE);
                return -1;
        }
        else {
                /* no PTS, no DTS, it's OK! */
        }

        if(pesh->ESCR_flag) {
                dat = *(obj->cur)++;
                pesh->ESCR_base = (dat & (BIT(5) | BIT(4) | BIT(3))) >> 3;
                pesh->ESCR_base <<= 2;
                pesh->ESCR_base |= (dat & (BIT(1) | BIT(0)));

                dat = *(obj->cur)++;
                pesh->ESCR_base <<= 8;
                pesh->ESCR_base |= dat;

                dat = *(obj->cur)++;
                pesh->ESCR_base <<= 5;
                pesh->ESCR_base |= ((dat & (BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3))) >> 3);
                pesh->ESCR_base <<= 2;
                pesh->ESCR_base |= (dat & (BIT(1) | BIT(0)));

                dat = *(obj->cur)++;
                pesh->ESCR_base <<= 8;
                pesh->ESCR_base |= dat;

                dat = *(obj->cur)++;
                pesh->ESCR_base <<= 5;
                pesh->ESCR_base |= ((dat & (BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3))) >> 3);
                pesh->ESCR_extension = (dat & (BIT(1) | BIT(0)));

                dat = *(obj->cur)++;
                pesh->ESCR_extension <<= 7;
                pesh->ESCR_extension |= (dat >> 1);
        }

        if(pesh->ES_rate_flag) {
                dat = *(obj->cur)++;
                pesh->ES_rate = (dat & 0x7F);

                dat = *(obj->cur)++;
                pesh->ES_rate <<= 8;
                pesh->ES_rate |= dat;

                dat = *(obj->cur)++;
                pesh->ES_rate <<= 7;
                pesh->ES_rate |= (dat >> 1);
        }

        if(pesh->DSM_trick_mode_flag) {
                dat = *(obj->cur)++;
                pesh->trick_mode_control = (dat & (BIT(7) | BIT(6) | BIT(5))) >> 5;

                if(0x00 == pesh->trick_mode_control) {
                        /* '000', fast_forward */
                        pesh->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                        pesh->intra_slice_refresh = (dat & (BIT(2))) >> 2;
                        pesh->frequency_truncation = (dat & (BIT(1) | BIT(0)));
                }
                else if(0x01 == pesh->trick_mode_control) {
                        /* '001', slow_motion */
                        pesh->rep_cntrl = (dat & 0x1F);
                }
                else if(0x02 == pesh->trick_mode_control) {
                        /* '010', freeze_frame */
                        pesh->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                }
                else if(0x03 == pesh->trick_mode_control) {
                        /* '011', fast_reverse */
                        pesh->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                        pesh->intra_slice_refresh = (dat & (BIT(2))) >> 2;
                        pesh->frequency_truncation = (dat & (BIT(1) | BIT(0)));
                }
                else if(0x04 == pesh->trick_mode_control) {
                        /* '100', slow_reverse */
                        pesh->rep_cntrl = (dat & 0x1F);
                }
        }

        if(pesh->additional_copy_info_flag) {
                dat = *(obj->cur)++;
                pesh->additional_copy_info = (dat & 0x7F);
        }

        if(pesh->PES_CRC_flag) {
                dat = *(obj->cur)++;
                pesh->previous_PES_packet_CRC = dat;

                dat = *(obj->cur)++;
                pesh->previous_PES_packet_CRC <<= 8;
                pesh->previous_PES_packet_CRC |= dat;
        }

        if(pesh->PES_extension_flag) {
                dat = *(obj->cur)++;
                pesh->PES_private_data_flag = (dat & (BIT(7))) >> 7;
                pesh->pack_header_field_flag = (dat & (BIT(6))) >> 6;
                pesh->program_packet_sequence_counter_flag = (dat & (BIT(5))) >> 5;
                pesh->P_STD_buffer_flag = (dat & (BIT(4))) >> 4;
                pesh->PES_extension_flag_2 = (dat & (BIT(0))) >> 0;

                if(pesh->PES_private_data_flag) {
                        for(int i = 16; i > 0; i--) {
                                pesh->PES_private_data[i] = *(obj->cur)++;
                        }
                }

                if(pesh->pack_header_field_flag) {
                        dat = *(obj->cur)++;
                        pesh->pack_field_length = dat;

                        /* pass pack_header() */
                        obj->cur += pesh->pack_field_length;
                }

                if(pesh->program_packet_sequence_counter_flag) {
                        dat = *(obj->cur)++;
                        pesh->program_packet_sequence_counter = (dat & 0x7F);

                        dat = *(obj->cur)++;
                        pesh->MPEG1_MPEG2_identifier = (dat & (BIT(6))) >> 6;
                        pesh->original_stuff_length = (dat & 0x3F);
                }

                if(pesh->P_STD_buffer_flag) {
                        dat = *(obj->cur)++;
                        pesh->P_STD_buffer_scale = (dat & (BIT(5))) >> 5;
                        pesh->P_STD_buffer_size = (dat & 0x1F);

                        dat = *(obj->cur)++;
                        pesh->P_STD_buffer_size <<= 8;
                        pesh->P_STD_buffer_size |= dat;
                }

                if(pesh->PES_extension_flag_2) {
                        dat = *(obj->cur)++;
                        pesh->PES_extension_field_length = (dat & 0x7F);

                        /* pass PES_extension_field */
                        obj->cur += pesh->PES_extension_field_length;
                }
        }

        /* pass stuffing_byte */
        obj->cur = es; /* pass (es - obj->cur)-byte */

        /* subsequent data before obj->tail is ES data */
        return 0;
}

static struct ts_pid *add_new_pid(struct ts_obj *obj)
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
        pid->elem = NULL;
        pid->cnt = 1;
        pid->lcnt = 0;
        /*pid->CC = tsh->continuity_counter; */
        pid->is_CC_sync = 0;
        pid->sdes = PID_TYPE[pid->type].sdes;
        pid->ldes = PID_TYPE[pid->type].ldes;
        pid->is_video = 0;
        pid->is_audio = 0;

        return add_to_pid_list(obj, &(rslt->pid0), pid); /* other_PID */
}

static struct ts_pid *add_to_pid_list(struct ts_obj *obj, struct ts_pid **phead, struct ts_pid *the_pid)
{
        struct ts_pid *pid;

        pid = (struct ts_pid *)zlst_search(phead, the_pid->PID);
        if(pid) {
                /* is in pid_list already, just update information */
                pid->PID = the_pid->PID;
                pid->prog = the_pid->prog;
                pid->elem = the_pid->elem;
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
                pid = (struct ts_pid *)buddy_malloc(obj->mp, sizeof(struct ts_pid));
                if(!pid) {
                        RPT(RPT_ERR, "malloc failed");
                        return NULL;
                }

                pid->PID = the_pid->PID;
                pid->prog = the_pid->prog;
                pid->elem = the_pid->elem;
                pid->type = the_pid->type;
                pid->is_video = the_pid->is_video;
                pid->is_audio = the_pid->is_audio;
                pid->cnt = the_pid->cnt;
                pid->lcnt = the_pid->lcnt;
                pid->CC = the_pid->CC;
                pid->is_CC_sync = the_pid->is_CC_sync;
                pid->sdes = the_pid->sdes;
                pid->ldes = the_pid->ldes;
                pid->sect_idx = 0; /* wait to sync with section head */
                pid->fd = NULL;

                RPT(RPT_DBG, "insert 0x%04X in pid_list", the_pid->PID);
                zlst_set_key(pid, the_pid->PID);
                if(zlst_insert(phead, pid)) {
                        buddy_free(obj->mp, pid);
                }
        }
        return pid;
}

static int is_all_prog_parsed(struct ts_obj *obj)
{
        uint8_t section_number;
        struct znode *znode_p; /* znode of program list */

        for(znode_p = (struct znode *)(obj->rslt.prog0); znode_p; znode_p = znode_p->next) {
                struct ts_prog *prog = (struct ts_prog *)znode_p;
                struct ts_table *table_02 = &(prog->table_02);
                struct znode *znode_s; /* znode of section list */

                if(0xFF == table_02->version_number) {
                        return 0;
                }

                section_number = 0;
                for(znode_s = (struct znode *)(table_02->sect0); znode_s; znode_s = znode_s->next) {
                        struct ts_sect *sect = (struct ts_sect *)znode_s;

                        if(section_number > table_02->last_section_number) {
                                return 0;
                        }
                        if(section_number != sect->section_number) {
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

static int elem_type(struct ts_elem *elem)
{
        const struct stream_type *p;

        for(p = STREAM_TYPE_TABLE; 0xFF != p->stream_type; p++) {
                if(p->stream_type == elem->stream_type) {
                        break;
                }
        }

        elem->type = p->type;
        elem->sdes = p->sdes;
        elem->ldes = p->ldes;
        return 0;
}

#define timestamp_assert(expr, ...) \
        do { \
                if(!(expr)) { \
                        fprintf(stderr, "\"%s\", line %d: assert (%s) failed: ", \
                                __FILE__, __LINE__, #expr); \
                        fprintf(stderr, __VA_ARGS__); \
                        fprintf(stderr, "\n"); \
                        return -1; \
                } \
        }while(0)

int64_t timestamp_add(int64_t t0, int64_t td, int64_t ovf)
{
        int64_t t1; /* t0 + td */
#if 1
        int64_t hovf = ovf >> 1; /* half overflow */

        timestamp_assert(0 < ovf, "0 < %lld", (long long int)ovf);
        timestamp_assert((hovf << 1) == ovf, "%lld is not even", (long long int)ovf);
        timestamp_assert(0 <= t0 && t0 < ovf, "0 <= %lld < %lld", (long long int)t0, (long long int)ovf);
        timestamp_assert(-hovf <= td && td < +hovf, "%lld <= %lld < %lld",
                         (long long int)-hovf, (long long int)td, (long long int)+hovf);
#endif

        t1 = t0 + td; /* add */
        t1 += ((t1 >=  0) ? 0 : ovf); /* t1 >=  0 */
        t1 -= ((t1 <  ovf) ? 0 : ovf); /* t1 <  ovf */

        return t1; /* [0, ovf) */
}

int64_t timestamp_diff(int64_t t1, int64_t t0, int64_t ovf)
{
        int64_t td; /* t1 - t0 */
        int64_t hovf = ovf >> 1; /* half overflow */

#if 1
        timestamp_assert(0 < ovf, "0 < %lld", (long long int)ovf);
        timestamp_assert((hovf << 1) == ovf, "%lld is not even", (long long int)ovf);
        timestamp_assert(0 <= t0 && t0 < ovf, "0 <= %lld < %lld", (long long int)t0, (long long int)ovf);
        timestamp_assert(0 <= t1 && t1 < ovf, "0 <= %lld < %lld", (long long int)t1, (long long int)ovf);
#endif

        td = t1 - t0; /* minus */
        td += ((td >=   0) ? 0 : ovf); /* special: get the distance from t0 to t1 */
        td -= ((td <  hovf) ? 0 : ovf); /* special: (distance < hovf) means t1 is latter or bigger */

        return td; /* [-hovf, +hovf) */
}
