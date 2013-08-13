/* vim: set tabstop=8 shiftwidth=8:
 * name: ts.c
 * funx: analyse ts stream
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* for uint?_t, etc */
#include <string.h> /* for memset, memcpy, etc */

#include "buddy.h"
#include "ts.h"

/* report level */
#define RPT_ERR (1) /* error, system error */
#define RPT_WRN (2) /* warning, maybe wrong, maybe OK */
#define RPT_INF (3) /* important information */
#define RPT_DBG (4) /* debug information */

/* report micro */
#define RPT(lvl, ...) do \
{ \
        if(lvl <= rpt_lvl) \
        { \
                switch(lvl) \
                { \
                        case RPT_ERR: fprintf(stderr, "%s: %d: err: ", __FILE__, __LINE__); break; \
                        case RPT_WRN: fprintf(stderr, "%s: %d: wrn: ", __FILE__, __LINE__); break; \
                        case RPT_INF: fprintf(stderr, "%s: %d: inf: ", __FILE__, __LINE__); break; \
                        case RPT_DBG: fprintf(stderr, "%s: %d: dbg: ", __FILE__, __LINE__); break; \
                        default:      fprintf(stderr, "%s: %d: ???: ", __FILE__, __LINE__); break; \
                } \
                fprintf(stderr, __VA_ARGS__); \
                fprintf(stderr, "\n"); \
        } \
} while (0)

#define BIT(n) (1<<(n))
#define NORMAL_SECTION_LENGTH_MAX (1021)
#define PRIVATE_SECTION_LENGTH_MAX (4093)

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

struct ts_pid_table {
        uint16_t min; /* PID range */
        uint16_t max; /* PID range */
        int     type; /* TS_TYPE_xxx */
};

static const struct ts_pid_table PID_TABLE[] = {
        {0x0000, 0x0000, TS_TYPE_PAT},
        {0x0001, 0x0001, TS_TYPE_CAT},
        {0x0002, 0x0002, TS_TYPE_TSDT},
        {0x0003, 0x000F, TS_TYPE_RSV},
        {0x0010, 0x0010, TS_TYPE_NIT}, /* NIT/ST */
        {0x0011, 0x0011, TS_TYPE_SDT}, /* SDT/BAT/ST */
        {0x0012, 0x0012, TS_TYPE_EIT}, /* EIT/ST */
        {0x0013, 0x0013, TS_TYPE_RST}, /* RST/ST */
        {0x0014, 0x0014, TS_TYPE_TDT}, /* TDT/TOT/ST */
        {0x0015, 0x0015, TS_TYPE_NS},
        {0x0016, 0x001B, TS_TYPE_RSV},
        {0x001C, 0x001C, TS_TYPE_INB},
        {0x001D, 0x001D, TS_TYPE_MSU},
        {0x001E, 0x001E, TS_TYPE_DIT},
        {0x001F, 0x001F, TS_TYPE_SIT},
        {0x0020, 0x1FFE, TS_TYPE_USR},
        {0x1FFF, 0x1FFF, TS_TYPE_NUL},
        {0x2000, 0xFFFF, TS_TYPE_BAD} /* loop stop condition! */
};

struct table_id_table {
        uint8_t min; /* table ID range */
        uint8_t max; /* table ID range */
        int check_CRC; /* some table do not need to check CRC_32 */
        int type; /* TS_TYPE_xxx */
};

static const struct table_id_table TABLE_ID_TABLE[] = {
        {0x00, 0x00, 1, TS_TYPE_PAT},
        {0x01, 0x01, 1, TS_TYPE_CAT},
        {0x02, 0x02, 1, TS_TYPE_PMT},
        {0x03, 0x03, 0, TS_TYPE_TSDT},
        {0x04, 0x3F, 0, TS_TYPE_RSV},
        {0x40, 0x40, 1, TS_TYPE_NIT}, /* actual network */
        {0x41, 0x41, 1, TS_TYPE_NIT}, /* other network */
        {0x42, 0x42, 1, TS_TYPE_SDT}, /* actual transport stream */
        {0x43, 0x45, 0, TS_TYPE_RSV},
        {0x46, 0x46, 1, TS_TYPE_SDT}, /* other transport stream */
        {0x47, 0x49, 0, TS_TYPE_RSV},
        {0x4A, 0x4A, 1, TS_TYPE_BAT},
        {0x4B, 0x4D, 0, TS_TYPE_RSV},
        {0x4E, 0x4E, 1, TS_TYPE_EIT}, /* actual transport stream, P/F */
        {0x4F, 0x4F, 1, TS_TYPE_EIT}, /* other transport stream, P/F */
        {0x50, 0x5F, 1, TS_TYPE_EIT}, /* actual transport stream, schedule */
        {0x60, 0x6F, 1, TS_TYPE_EIT}, /* other transport stream, schedule */
        {0x70, 0x70, 0, TS_TYPE_TDT},
        {0x71, 0x71, 0, TS_TYPE_RST},
        {0x72, 0x72, 0, TS_TYPE_ST},
        {0x73, 0x73, 1, TS_TYPE_TOT},
        {0x74, 0x7D, 0, TS_TYPE_RSV},
        {0x7E, 0x7E, 0, TS_TYPE_DIT},
        {0x7F, 0x7F, 0, TS_TYPE_SIT},
        {0x80, 0xFE, 0, TS_TYPE_USR},
        {0xFF, 0xFF, 0, TS_TYPE_RSV} /* loop stop condition! */
};

struct stream_type_table {
        uint8_t stream_type;
        int   type; /* TS_TYPE_xxx */
};

static const struct stream_type_table STREAM_TYPE_TABLE[] = {
        {0x00, TS_TYPE_RSV}, /* "Reserved", "ITU-T|ISO/IEC Reserved" */
        {0x01, TS_TYPE_VID}, /* "MPEG-1", "ISO/IEC 11172-2 Video" */
        {0x02, TS_TYPE_VID}, /* "MPEG-2", "ITU-T Rec.H.262|ISO/IEC 13818-2 Video or MPEG-1 parameter limited" */
        {0x03, TS_TYPE_AUD}, /* "MPEG-1", "ISO/IEC 11172-3 Audio" */
        {0x04, TS_TYPE_AUD}, /* "MPEG-2", "ISO/IEC 13818-3 Audio" */
        {0x05, TS_TYPE_USR}, /* "private", "ITU-T Rec.H.222.0|ISO/IEC 13818-1 private_sections" */
        {0x06, TS_TYPE_AUD}, /* "AC3|TT|LPCM", "ITU-T Rec.H.222.0|ISO/IEC 13818-1 PES packets containing private data|Dolby Digital DVB|Linear PCM" */
        {0x07, TS_TYPE_USR}, /* "MHEG", "ISO/IEC 13522 MHEG" */
        {0x08, TS_TYPE_USR}, /* "DSM-CC", "ITU-T Rec.H.222.0|ISO/IEC 13818-1 Annex A DSM-CC" */
        {0x09, TS_TYPE_USR}, /* "H.222.1", "ITU-T Rec.H.222.1" */
        {0x0A, TS_TYPE_USR}, /* "MPEG2 type A", "ISO/IEC 13818-6 type A: Multi-protocol Encapsulation" */
        {0x0B, TS_TYPE_USR}, /* "MPEG2 type B", "ISO/IEC 13818-6 type B: DSM-CC U-N Messages" */
        {0x0C, TS_TYPE_USR}, /* "MPEG2 type C", "ISO/IEC 13818-6 type C: DSM-CC Stream Descriptors" */
        {0x0D, TS_TYPE_USR}, /* "MPEG2 type D", "ISO/IEC 13818-6 type D: DSM-CC Sections or DSM-CC Addressable Sections" */
        {0x0E, TS_TYPE_USR}, /* "auxiliary", "ITU-T Rec.H.222.0|ISO/IEC 13818-1 auxiliary" */
        {0x0F, TS_TYPE_AUD}, /* "AAC ADTS", "ISO/IEC 13818-7 Audio with ADTS transport syntax" */
        {0x10, TS_TYPE_VID}, /* "MPEG-4", "ISO/IEC 14496-2 Visual" */
        {0x11, TS_TYPE_AUD}, /* "AAC LATM", "ISO/IEC 14496-3 Audio with LATM transport syntax" */
        {0x12, TS_TYPE_AUD}, /* "MPEG-4", "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets" */
        {0x13, TS_TYPE_AUD}, /* "MPEG-4", "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC 14496_sections" */
        {0x14, TS_TYPE_USR}, /* "MPEG-2", "ISO/IEC 13818-6 Synchronized Download Protocol" */
        {0x15, TS_TYPE_USR}, /* "MPEG-2", "Metadata carried in PES packets" */
        {0x16, TS_TYPE_USR}, /* "MPEG-2", "Metadata carried in metadata_sections" */
        {0x17, TS_TYPE_USR}, /* "MPEG-2", "Metadata carried in ISO/IEC 13818-6 Data Carousel" */
        {0x18, TS_TYPE_USR}, /* "MPEG-2", "Metadata carried in ISO/IEC 13818-6 Object Carousel" */
        {0x19, TS_TYPE_USR}, /* "MPEG-2", "Metadata carried in ISO/IEC 13818-6 Synchronized Dowload Protocol" */
        {0x1A, TS_TYPE_USR}, /* "IPMP", "IPMP stream(ISO/IEC 13818-11, MPEG-2 IPMP)" */
        {0x1B, TS_TYPE_VID}, /* "H.264", "ITU-T Rec.H.264|ISO/IEC 14496-10 Video" */
        {0x1C, TS_TYPE_AUD}, /* "MPEG-4", "ISO/IEC 14496-3 Audio, without using any additional transport syntax, such as DST, ALS and SLS" */
        {0x1D, TS_TYPE_USR}, /* "MPEG-4", "ISO/IEC 14496-17 Text" */
        {0x1E, TS_TYPE_VID}, /* "MPEG-4", "Auxiliary video stream as defined in ISO/IEC 23002-3" */
        {0x42, TS_TYPE_VID}, /* "AVS", "Advanced Video Standard" */
        {0x7F, TS_TYPE_USR}, /* "IPMP", "IPMP stream" */
        {0x80, TS_TYPE_VID}, /* "SVAC|LPCM", "SVAC, LPCM of ATSC" */
        {0x81, TS_TYPE_AUD}, /* "AC3", "Dolby Digital ATSC" */
        {0x82, TS_TYPE_AUD}, /* "DTS", "DTS Audio" */
        {0x83, TS_TYPE_AUD}, /* "MLP", "MLP" */
        {0x84, TS_TYPE_AUD}, /* "DDP", "Dolby Digital Plus" */
        {0x85, TS_TYPE_AUD}, /* "DTSHD", "DTSHD" */
        {0x86, TS_TYPE_AUD}, /* "DTSHD_XLL", "DTSHD_XLL" */
        {0x90, TS_TYPE_AUD}, /* "G.711", "G.711(A)" */
        {0x92, TS_TYPE_AUD}, /* "G.722.1", "G.722.1" */
        {0x93, TS_TYPE_AUD}, /* "G.723.1", "G.723.1" */
        {0x99, TS_TYPE_AUD}, /* "G.729", "G.729" */
        {0x9A, TS_TYPE_AUD}, /* "AMR-NB", "AMR-NB" */
        {0x9B, TS_TYPE_AUD}, /* "SVAC", "SVAC" */
        {0xA1, TS_TYPE_AUD}, /* "DDP_2", "Dolby Digital Plus" */
        {0xA2, TS_TYPE_AUD}, /* "DTSHD_2", "DTSHD_2" */
        {0xEA, TS_TYPE_VID}, /* "VC1", "VC1" */
        {0xEA, TS_TYPE_AUD}, /* "WMA", "WMA" */
        {0xFF, TS_TYPE_UNO}  /* "UNKNOWN", "Unknown stream" loop stop condition! */
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

static void init(/*@out@*/ struct ts_obj *obj);
static void tidy(struct ts_obj *obj);
static int state_next_pat(struct ts_obj *obj);
static int state_next_pmt(struct ts_obj *obj);
static int state_next_pkt(struct ts_obj *obj);

static int ts_parse_af(struct ts_obj *obj); /* Adaption Fields information */
static int ts_ts2sect(struct ts_obj *obj); /* collect PSI/SI section data */
static int ts_parse_sect(struct ts_obj *obj, struct ts_sect *new_sect);
static int ts_parse_secb_pat(struct ts_obj *obj);
static int ts_parse_secb_cat(struct ts_obj *obj);
static int ts_parse_secb_pmt(struct ts_obj *obj);
static int ts_parse_secb_sdt(struct ts_obj *obj);
static int ts_parse_pesh(struct ts_obj *obj); /* PES layer information */
static int ts_parse_pesh_switch(struct ts_obj *obj);
static int ts_parse_pesh_detail(struct ts_obj *obj);

static struct ts_pid *update_pid_list(struct ts_obj *obj, struct ts_pid *new_pid);
static void free_pid(void *mp, struct ts_pid *pid);
static void free_sect(void *mp, struct ts_sect *sect);
static void free_tabl(void *mp, struct ts_tabl *tabl);
static void free_prog(void *mp, struct ts_prog *prog);
static int is_all_prog_parsed(struct ts_obj *obj);
static int pid_type(uint16_t pid);
static const struct table_id_table *table_type(uint8_t id);
static const struct stream_type_table *elem_type(uint8_t stream_type);
static int dump(uint8_t *buf, int len);

/*@only@*/
/*@null@*/
struct ts_obj *ts_create(/*@null@*/ void *mp)
{
        struct ts_obj *obj;

        if(!mp) {
                RPT(RPT_ERR, "bad memory pool");
                return NULL;
        }

        obj = (struct ts_obj *)malloc(sizeof(struct ts_obj));
        if(!obj) {
                RPT(RPT_ERR, "malloc ts_obj failed");
                return NULL;
        }

        obj->mp = mp;

        /* prepare for ts_init() */
        obj->pid0 = NULL; /* no pid list now */
        obj->prog0 = NULL; /* no prog list now */
        obj->tabl0 = NULL; /* no tabl list now */
        init(obj);

        return obj;
}

int ts_destroy(/*@only@*/ /*@null@*/ struct ts_obj *obj)
{
        if(!obj) {
                RPT(RPT_ERR, "bad obj");
                return -1;
        }

        init(obj); /* free all list */
        free(obj);
        return 0;
}

int ts_ioctl(struct ts_obj *obj, int cmd, void *arg)
{
        if(!obj) {
                RPT(RPT_ERR, "bad obj");
                return -1;
        }

        switch(cmd)
        {
                case TS_INIT:
                        init(obj);
                        break;
                case TS_SCFG:
                        if(arg) {
                                memcpy(&(obj->cfg), (struct ts_cfg *)arg, sizeof(struct ts_cfg));
                        }
                        else {
                                RPT(RPT_ERR, "bad cfg");
                        }
                        break;
                case TS_TIDY:
                        tidy(obj);
                        break;
                default:
                        RPT(RPT_ERR, "bad cmd");
                        break;
        }
        return 0;
}

static void init(/*@out@*/ struct ts_obj *obj)
{
        struct ts_pid *pid;
        struct ts_prog *prog;
        struct ts_tabl *tabl;

        /* clear the pid list */
        while(NULL != (pid = (struct ts_pid *)zlst_pop(&(obj->pid0)))) {
                free_pid(obj->mp, pid);
        }
        obj->pid0 = NULL;

        /* clear the prog list */
        while(NULL != (prog = (struct ts_prog *)zlst_pop(&(obj->prog0)))) {
                free_prog(obj->mp, prog);
        }
        obj->prog0 = NULL;

        /* clear the table list */
        while(NULL != (tabl = (struct ts_tabl *)zlst_pop(&(obj->tabl0)))) {
                free_tabl(obj->mp, tabl);
        }
        obj->tabl0 = NULL;

        obj->state = STATE_NEXT_PAT;
        obj->ADDR = -TS_PKT_SIZE; /* count from 0 */
        obj->cnt = -1; /* count ts packet from 0 */
        obj->has_got_transport_stream_id = 0;
        obj->transport_stream_id = 0;
        obj->CC_lost = 0;
        obj->is_pat_pmt_parsed = 0;
        obj->is_psi_si_parsed = 0;
        obj->concerned_pid = 0x0000; /* PAT_PID */
        obj->interval = 0;
        obj->CTS = (int64_t)0;
        obj->CTS0 = (int64_t)0;
        obj->lCTS = (int64_t)0; /* for MTS file only, must init as 0L */
        obj->STC = STC_OVF;

        memset(&(obj->err), 0, sizeof(struct ts_err)); /* no error */
        memset(&(obj->cfg), 0, sizeof(struct ts_cfg)); /* do nothing */
        return;
}

static void tidy(struct ts_obj *obj)
{
        struct ts_pid new_pid;
        struct ts_prog *prog;
        struct ts_elem *elem;
        struct ts_tabl *tabl;
        struct ts_pid *pid;

        /* add PAT pid */
        if(obj->prog0) {
                new_pid.PID = 0x0000;
                new_pid.type = TS_TYPE_PAT;
                new_pid.prog = obj->prog0;
                new_pid.elem = NULL;
                new_pid.cnt = 0;
                new_pid.lcnt = 0;
                new_pid.is_CC_sync = 0;
                update_pid_list(obj, &new_pid);
                RPT(RPT_INF, "add pat pid: 0x%04X", new_pid.PID);
        }

        /* prog list */
        for(prog = obj->prog0; prog; prog = (struct ts_prog *)(((struct znode *)prog)->next)) {
                RPT(RPT_INF, "tidy prog: %d", prog->program_number);
                prog->is_parsed = 1;
                prog->tabl.STC = STC_OVF;
                prog->ADDa = 0;
                prog->PCRa = STC_OVF;
                prog->ADDb = 0;
                prog->PCRb = STC_OVF;
                prog->is_STC_sync = 0;

                /* add PMT pid */
                new_pid.PID = prog->PMT_PID;
                new_pid.type = TS_TYPE_PMT;
                new_pid.prog = prog;
                new_pid.elem = NULL;
                new_pid.cnt = 0;
                new_pid.lcnt = 0;
                new_pid.is_CC_sync = 0;
                update_pid_list(obj, &new_pid);
                RPT(RPT_INF, "add pmt pid: 0x%04X", new_pid.PID);

                /* add PCR pid */
                new_pid.PID = prog->PCR_PID;
                new_pid.type = ((0x1FFF != new_pid.PID) ? TS_TYPE_PCR : TS_TYPE_NULP);
                new_pid.prog = prog;
                new_pid.elem = NULL;
                new_pid.cnt = 0;
                new_pid.lcnt = 0;
                new_pid.is_CC_sync = 0;
                update_pid_list(obj, &new_pid);
                RPT(RPT_INF, "add pcr pid: 0x%04X", new_pid.PID);

                /* elem list */
                for(elem = prog->elem0; elem; elem = (struct ts_elem *)(((struct znode *)elem)->next)) {
                        RPT(RPT_INF, "tidy elem: 0x%04X", elem->PID);
                        elem->PTS = STC_BASE_OVF;
                        elem->DTS = STC_BASE_OVF;
                        elem->is_pes_align = 0;

                        /* add elem pid */
                        new_pid.PID = elem->PID;
                        new_pid.type = elem->type;
                        new_pid.prog = prog;
                        new_pid.elem = elem;
                        new_pid.cnt = 0;
                        new_pid.lcnt = 0;
                        new_pid.is_CC_sync = 0;
                        update_pid_list(obj, &new_pid);
                        RPT(RPT_INF, "add elem pid: 0x%04X", new_pid.PID);
                }
        }

        /* tabl list */
        for(tabl = obj->tabl0; tabl; tabl = (struct ts_tabl *)(((struct znode *)tabl)->next)) {
                RPT(RPT_INF, "tidy tabl: 0x%02X", tabl->table_id);
                tabl->STC = STC_OVF;
        }

        /* pid list */
        for(pid = obj->pid0; pid; pid = (struct ts_pid *)(((struct znode *)pid)->next)) {
                RPT(RPT_INF, "tidy pid: 0x%02X", pid->PID);
                if((obj->prog0) &&
                   (pid->PID < 0x0020 || pid->PID == 0x1FFF)) {
                        pid->prog = obj->prog0;
                }
                else {
                        /* pid->prog is NULL(by xml2list) or set by front code */
                }
                /* pid->elem is NULL(by xml2list) or set by front code */
                pid->cnt = 0;
                pid->lcnt = 0;
                pid->is_CC_sync = 0;
        }

        /* obj */
        obj->state = STATE_NEXT_PKT;
        obj->has_got_transport_stream_id = 0;
        obj->is_pat_pmt_parsed = 1;
        obj->is_psi_si_parsed = 1;
        return;
}

static void free_pid(void *mp, struct ts_pid *pid)
{
        struct ts_pkt *pkt;

        /* clear the pkt list */
        while(NULL != (pkt = (struct ts_pkt *)zlst_pop(&(pid->pkt0)))) {
                buddy_free(mp, pkt);
        }

        buddy_free(mp, pid);
        return;
}

static void free_sect(void *mp, struct ts_sect *sect)
{
        if(sect->section) {
                buddy_free(mp, sect->section);
        }

        buddy_free(mp, sect);
        return;
}

static void free_tabl(void *mp, struct ts_tabl *tabl)
{
        struct ts_sect *sect;

        /* clear the sect list */
        while(NULL != (sect = (struct ts_sect *)zlst_pop(&(tabl->sect0)))) {
                free_sect(mp, sect);
        }

        buddy_free(mp, tabl);
        return;
}

static void free_prog(void *mp, struct ts_prog *prog)
{
        struct ts_elem *elem;
        struct ts_sect *sect;

        /* clear the elem list */
        while(NULL != (elem = (struct ts_elem *)zlst_pop(&(prog->elem0)))) {
                if(elem->es_info) {
                        buddy_free(mp, elem->es_info);
                        elem->es_info_len = 0;
                }
                buddy_free(mp, elem);
        }

        /* clear the sect list */
        while(NULL != (sect = (struct ts_sect *)zlst_pop(&(prog->tabl.sect0)))) {
                free_sect(mp, sect);
        }

        if(prog->program_info) {
                buddy_free(mp, prog->program_info);
                prog->program_info_len = 0;
        }
        if(prog->service_name) {
                buddy_free(mp, prog->service_name);
                prog->service_name_len = 0;
        }
        if(prog->service_provider) {
                buddy_free(mp, prog->service_provider);
                prog->service_provider_len = 0;
        }
        buddy_free(mp, prog);
        return;
}

int ts_parse_tsh(struct ts_obj *obj)
{
        struct ts_ipt *ipt;
        uint8_t dat;
        struct ts_tsh *tsh;
        struct ts_err *err;
        struct ts_pid *pid; /* maybe NULL */

        if(!obj) {
                RPT(RPT_ERR, "ts_parse_tsh: bad obj");
                return -1;
        }
        ipt = &(obj->ipt);

        /* TS[] */
        if(!(ipt->has_ts)) {
                RPT(RPT_ERR, "ts_parse_tsh: no ts packet");
                return -1;
        }
        obj->cur = ipt->TS;
        obj->tail = obj->cur + TS_PKT_SIZE;

        /* packet count and ADDR */
        obj->cnt++;
        obj->ADDR = (ipt->has_addr) ? (ipt->ADDR) : (obj->ADDR + TS_PKT_SIZE);
#if 0
        RPT(RPT_INF, "packet %lld @ %lld:", obj->cnt, obj->ADDR);
        dump(ipt->TS, TS_PKT_SIZE); /* debug only */
#endif

        tsh = &(obj->tsh);
        err = &(obj->err);

        /* init for a new ts packet */
        obj->AF_len = 0; /* no AF */
        obj->has_pcr = 0; /* no PCR */
        obj->PES_len = 0; /* no PES */
        obj->has_pts = 0; /* no PTS */
        obj->has_dts = 0; /* no DTS */
        obj->ES_len = 0; /* no ES */
        obj->is_psi_si = 0; /* not PSI/SI */
        obj->sect = NULL; /* not an end of a section */
        obj->has_rate = 0; /* not a new rate calculate peroid */

        /* begin */
        dat = *(obj->cur)++;
        tsh->sync_byte = dat;
        if(0x47 != tsh->sync_byte) {
                err->Sync_byte_error++;
                if(err->Sync_byte_error > 1) {
                        err->TS_sync_loss++;
                }
                RPT(RPT_ERR, "sync_byte(0x%02X) error!", tsh->sync_byte);
                dump(ipt->TS, TS_PKT_SIZE);
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
                RPT(RPT_ERR, "Bad adaption_field_control field(00)!");
        }

        if((BIT(1) & tsh->adaption_field_control) && obj->cfg.need_af) {
                ts_parse_af(obj);
        }

        if(BIT(0) & tsh->adaption_field_control) {
                /* data_byte, PSI or PES */
        }

        obj->PID = tsh->PID; /* record into obj struct */
#if 0
        RPT(RPT_DBG, "search 0x%04X in pid_list", obj->PID);
#endif
        obj->pid = (struct ts_pid *)zlst_search(&(obj->pid0), obj->PID);
        if(!(obj->pid)) {
                struct ts_pid ts_pid, *new_pid = &ts_pid;

                /* meet new PID, add it in pid_list */
                new_pid->PID = obj->PID;
                new_pid->type = pid_type(new_pid->PID);
                if((obj->prog0) &&
                   (new_pid->PID < 0x0020 || new_pid->PID == 0x1FFF)) {
                        new_pid->prog = obj->prog0;
                }
                else {
                        new_pid->prog = NULL;
                }
                new_pid->elem = NULL;
                new_pid->cnt = 1;
                new_pid->lcnt = 0;
                new_pid->is_CC_sync = 0;

                obj->pid = update_pid_list(obj, new_pid);
                if(!(obj->pid)) {
                        RPT(RPT_ERR, "update_pid_list failed");
                }
        }

        pid = obj->pid; /* maybe NULL */

        /* calc STC and CTS, should be as early as possible */
        if(obj->cfg.need_timestamp) {
                if(ipt->has_mts) {
                        int64_t dCTS = ts_timestamp_diff(ipt->MTS, obj->lCTS, MTS_OVF);

                        if(STC_OVF != obj->STC) {
                                obj->STC = ts_timestamp_add(obj->STC, dCTS, STC_OVF);
                        }
                        else {
                                obj->STC = ts_timestamp_add(0L, dCTS, STC_OVF);
                        }
                        obj->lCTS = ipt->MTS; /* record last CTS */

                        if(ipt->has_cts) {
                                obj->CTS = ipt->CTS;
                        }
                        else {
                                obj->CTS = obj->STC;
                        }
                }
                else {
                        struct ts_prog *prog; /* may be NULL */

                        /* STC: according to pid->prog */
                        if(pid && pid->prog) {
                                prog = pid->prog;
                                if((prog->is_STC_sync) &&
                                   (prog->PCRa != prog->PCRb)) {
                                        long double delta;

                                        /* STCx - PCRb   ADDx - ADDb */
                                        /* ----------- = ----------- */
                                        /* PCRb - PCRa   ADDb - ADDa */
                                        delta = (long double)ts_timestamp_diff(prog->PCRb, prog->PCRa, STC_OVF);
                                        delta *= (obj->ADDR - prog->ADDb);
                                        delta /= (prog->ADDb - prog->ADDa);
                                        obj->STC = ts_timestamp_add(prog->PCRb, (int64_t)delta, STC_OVF);
                                }
                        }

                        /* CTS: according to prog0 */
                        if(ipt->has_cts) {
                                obj->CTS = ipt->CTS;
                        }
                        else {
                                if(obj->prog0) {
                                        prog = obj->prog0;
                                        if((prog->is_STC_sync) &&
                                           (prog->PCRa != prog->PCRb)) {
                                                long double delta;

                                                /* CTSx - PCRb   ADDx - ADDb */
                                                /* ----------- = ----------- */
                                                /* PCRb - PCRa   ADDb - ADDa */
                                                delta = (long double)ts_timestamp_diff(prog->PCRb, prog->PCRa, STC_OVF);
                                                delta *= (obj->ADDR - prog->ADDb);
                                                delta /= (prog->ADDb - prog->ADDa);
                                                obj->CTS = ts_timestamp_add(prog->PCRb, (int64_t)delta, STC_OVF);
                                        }
                                }
                        }
                }
                obj->STC_base = obj->STC / 300;
                obj->STC_ext = obj->STC % 300;
                obj->CTS_base = obj->CTS / 300;
                obj->CTS_ext = obj->CTS % 300;
        }

        /* statistic */
        if(obj->cfg.need_statistic) {
                pid->cnt++;
                obj->sys_cnt++;
                obj->nul_cnt += ((0x1FFF == tsh->PID) ? 1 : 0);
                if((tsh->PID < 0x0020) || IS_TYPE(TS_TYPE_PMT, pid->type)) {
                        obj->psi_cnt++;
                        obj->is_psi_si = 1;
                }
        }

        /* PSI/SI section collect */
        if(obj->cfg.need_psi || obj->cfg.need_si) {
                if((tsh->PID < 0x0020) || IS_TYPE(TS_TYPE_PMT, pid->type)) {
                        ts_ts2sect(obj);
                }
        }

        return 0;
}

int ts_parse_tsb(struct ts_obj *obj)
{
        if(!obj) {
                RPT(RPT_ERR, "bad obj");
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
        struct ts_tsh *tsh = &(obj->tsh);
        struct ts_tabl *tabl;
        uint8_t section_number;

        if(0x0000 != tsh->PID) {
                return -1; /* not PAT */
        }

        /* section parse has done in ts_parse_tsh()! */
        RPT(RPT_DBG, "search 0x00 in table_list");
        tabl = (struct ts_tabl *)zlst_search(&(obj->tabl0), 0x00);
        if(!tabl) {
                return -1;
        }
        else {
                struct znode *znode;

                section_number = 0;
                for(znode = (struct znode *)(tabl->sect0); znode; znode = znode->next) {
                        struct ts_sect *sect = (struct ts_sect *)znode;

                        if(section_number > tabl->last_section_number) {
                                return -1;
                        }
                        if(section_number != sect->section_number) {
                                return -1;
                        }
                        section_number++;
                }

                RPT(RPT_INF, "all PAT section parsed");
                obj->state = STATE_NEXT_PMT;
        }

        if(is_all_prog_parsed(obj)) {
                RPT(RPT_INF, "no PMT section");
                obj->is_pat_pmt_parsed = 1;
                obj->state = STATE_NEXT_PKT;
        }

        return 0;
}

static int state_next_pmt(struct ts_obj *obj)
{
        struct ts_tsh *tsh = &(obj->tsh);
        struct ts_pid *pid;

        RPT(RPT_DBG, "search 0x%04X in pid_list", tsh->PID);
        pid = (struct ts_pid *)zlst_search(&(obj->pid0), tsh->PID);
        if((!pid) || !IS_TYPE(TS_TYPE_PMT, pid->type)) {
                return -1; /* not PMT */
        }

        /* section parse has done in ts_parse_tsh()! */
        if(is_all_prog_parsed(obj)) {
                RPT(RPT_INF, "all PMT section parsed");
                obj->is_pat_pmt_parsed = 1;
                obj->state = STATE_NEXT_PKT;
        }

        return 0;
}

static int state_next_pkt(struct ts_obj *obj)
{
        struct ts_tsh *tsh = &(obj->tsh);
        struct ts_af *af = &(obj->af);
        struct ts_pid *pid = obj->pid;
        struct ts_elem *elem = pid->elem; /* may be NULL */
        struct ts_err *err = &(obj->err);

        /* CC */
        if(obj->cfg.need_cc) {
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

                        obj->CC_wait = pid->CC;
                        obj->CC_find = tsh->continuity_counter;
                        obj->CC_lost = lost;
                }
                else {
                        pid->is_CC_sync = 1;

                        obj->CC_wait = pid->CC;
                        obj->CC_find = tsh->continuity_counter;
                        obj->CC_lost = 0;
                }
                pid->CC = tsh->continuity_counter; /* update CC */
                err->Continuity_count_error = (0x1FFF == tsh->PID) ? 0 : obj->CC_lost;
        }

        /* PCR flush */
        if(obj->cfg.need_af && obj->has_pcr) {
                struct znode *znode_prog;
                struct ts_prog *prog;

                obj->PCR_base = af->program_clock_reference_base;
                obj->PCR_ext  = af->program_clock_reference_extension;

                obj->PCR  = obj->PCR_base;
                obj->PCR *= 300;
                obj->PCR += obj->PCR_ext;

                prog = pid->prog; /* maybe NULL */
                if(prog) {
                        if(!prog->is_STC_sync) {
                                /* use PCR as STC, suppose PCR_jitter is zero */
                                obj->STC = obj->PCR;
                                obj->STC_base = obj->STC / 300;
                                obj->STC_ext = obj->STC % 300;
                        }

                        /* PCR_interval (PCR packet arrive time interval) */
                        if(STC_OVF != prog->PCRb) {
                                obj->PCR_interval = ts_timestamp_diff(obj->STC, prog->PCRb, STC_OVF);
                                if((prog->is_STC_sync) &&
                                   !(0 < obj->PCR_interval && obj->PCR_interval <= 40 * STC_MS)) {
                                        /* !(0 < interval < +40ms) */
                                        err->PCR_repetition_error = 1;
                                }
                        }
                        else {
                                obj->PCR_interval = 0;
                        }

                        /* PCR_continuity (PCR value interval) */
                        if(STC_OVF != prog->PCRb) {
                                obj->PCR_continuity = ts_timestamp_diff(obj->PCR, prog->PCRb, STC_OVF);
                                if((prog->is_STC_sync) && !(af->discontinuity_indicator) &&
                                   !(0 < obj->PCR_continuity && obj->PCR_continuity <= 100 * STC_MS)) {
                                        /* !(0 < continuity < +100ms) */
                                        err->PCR_discontinuity_indicator_error = 1;
                                }
                        }
                        else {
                                obj->PCR_continuity = 0;
                        }

                        /* PCR_jitter */
                        obj->PCR_jitter = ts_timestamp_diff(obj->PCR, obj->STC, STC_OVF);
                        if((prog->is_STC_sync) &&
                           !(-13 <= obj->PCR_jitter && obj->PCR_jitter <= +13)) {
                                /* !(-500ns < jitter < +500ns) */
                                err->PCR_accuracy_error = 1;
                        }
                }
                else {
                        RPT(RPT_ERR, "No program use this PCR packet(0x%04X)!", tsh->PID);
                }

                /* traverse prog_list: maybe some of them use this PCR PID */
                for(znode_prog = (struct znode *)(obj->prog0); znode_prog; znode_prog = znode_prog->next) {
                        prog = (struct ts_prog *)znode_prog;

                        if(prog->PCR_PID != obj->PID) {
                                continue;
                        }

                        /* PCRa: the PCR packet before last PCR packet */
                        prog->PCRa = prog->PCRb;
                        prog->ADDa = prog->ADDb;

                        /* PCRb: last PCR packet */
                        prog->PCRb = obj->PCR;
                        prog->ADDb = obj->ADDR;

                        /* is_STC_sync */
                        if(!prog->is_STC_sync) {
                                int is_first_count_clear = 0;

                                if(obj->ipt.has_mts) {
                                        /* clear count after 1st PCR */
                                        is_first_count_clear = 1;

                                        /* STC sync after 1st PCR */
                                        prog->is_STC_sync = 1;
                                }
                                else {
                                        /* TS */
                                        if(STC_OVF == prog->PCRa) {
                                                /* clear count after 1st PCR */
                                                is_first_count_clear = 1;
                                        }
                                        else {
                                                /* STC sync after 2nd PCR */
                                                prog->is_STC_sync = 1;
                                        }
                                }

                                /* first count clear */
                                if(is_first_count_clear && prog == obj->prog0) {
                                        struct znode *znode_pid;

                                        for(znode_pid = (struct znode *)(obj->pid0); znode_pid; znode_pid = znode_pid->next) {
                                                struct ts_pid *pid_item = (struct ts_pid *)znode_pid;
                                                if(pid_item->prog == prog) {
                                                        pid_item->lcnt = 0;
                                                        pid_item->cnt = 0;
                                                }
                                        }

                                        obj->last_sys_cnt = 0;
                                        obj->sys_cnt = 0;

                                        obj->last_psi_cnt = 0;
                                        obj->psi_cnt = 0;

                                        obj->last_nul_cnt = 0;
                                        obj->nul_cnt = 0;

                                        obj->last_interval = 0;
                                        obj->interval = 0;
                                        obj->CTS = obj->PCR;
                                        obj->CTS0 = obj->CTS;
                                }
                        }
                }
        }

        /* interval and statistic */
        if(obj->cfg.need_statistic && obj->prog0 && obj->prog0->is_STC_sync) {
                obj->interval = ts_timestamp_diff(obj->CTS, obj->CTS0, STC_OVF);
                if(obj->interval >= obj->aim_interval) {
                        struct znode *znode;

                        /* calc bitrate and clear the packet count */
                        for(znode = (struct znode *)(obj->pid0); znode; znode = znode->next) {
                                struct ts_pid *pid_item = (struct ts_pid *)znode;
                                pid_item->lcnt = pid_item->cnt;
                                pid_item->cnt = 0;
                        }

                        obj->last_sys_cnt = obj->sys_cnt;
                        obj->sys_cnt = 0;

                        obj->last_psi_cnt = obj->psi_cnt;
                        obj->psi_cnt = 0;

                        obj->last_nul_cnt = obj->nul_cnt;
                        obj->nul_cnt = 0;

                        obj->last_interval = obj->interval;
                        obj->interval = 0;
                        obj->CTS0 = obj->CTS;

                        obj->has_rate = 1;

                        obj->is_psi_si_parsed = 1;
                }
        }

        /* PES head & ES data */
        if(obj->cfg.need_pes && elem && (0 == tsh->transport_scrambling_control)) {
                if(IS_TYPE(TS_TYPE_AUD, pid->type) || IS_TYPE(TS_TYPE_VID, pid->type)) {
                        ts_parse_pesh(obj);
                }

                if(obj->has_pts) {
                        /* PTS */
                        if(STC_BASE_OVF != elem->PTS) {
                                obj->PTS_interval = ts_timestamp_diff(obj->PTS, elem->PTS, STC_BASE_OVF);
                        }
                        else {
                                obj->PTS_interval = 0;
                        }
                        if(STC_BASE_OVF != obj->STC_base) {
                                obj->PTS_minus_STC = ts_timestamp_diff(obj->PTS, obj->STC_base, STC_BASE_OVF);
                        }
                        else {
                                obj->PTS_minus_STC = 0;
                        }
                        elem->PTS = obj->PTS; /* record last PTS in elem */

                        /* DTS, if no DTS, DTS = PTS */
                        if(STC_BASE_OVF != elem->DTS) {
                                obj->DTS_interval = ts_timestamp_diff(obj->DTS, elem->DTS, STC_BASE_OVF);
                        }
                        else {
                                obj->DTS_interval = 0;
                        }
                        if(STC_BASE_OVF != obj->STC_base) {
                                obj->DTS_minus_STC = ts_timestamp_diff(obj->DTS, obj->STC_base, STC_BASE_OVF);
                        }
                        else {
                                obj->DTS_minus_STC = 0;
                        }
                        elem->DTS = obj->DTS; /* record last DTS in elem */
                }
        }

        return 0;
}

static int ts_parse_af(struct ts_obj *obj)
{
        int i;
        uint8_t dat;
        struct ts_af *af = &(obj->af);

        obj->AF = obj->cur;
        dat = *(obj->cur)++;
        af->adaption_field_length = dat;
        obj->AF_len = af->adaption_field_length + 1; /* add length itself */
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

                obj->has_pcr = 1;
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
        uint8_t dat;
        uint8_t *p;
        struct ts_tsh *tsh = &(obj->tsh);
        struct ts_pid *pid = obj->pid;
        struct ts_pkt *pkt;

        /* collect packet(make pkt list) */
        if(!(pid->pkt0)) {
                /* waiting for section head */
                if(tsh->payload_unit_start_indicator) {
                        /* first packet of this section */
                        dat = *(obj->cur)++; /* pointer_field */
                        obj->cur += dat; /* point to section head now */
                        pid->has_new_sech = 0;
                        pid->payload_total = (obj->tail - obj->cur);
                        pid->section_length = PRIVATE_SECTION_LENGTH_MAX; /* suppose maximum length */

                        /* sect head 3-byte */
                        p = obj->cur;
                        for(pid->sech3_idx = 0; pid->sech3_idx < 3; pid->sech3_idx++) {
                                if(p >= obj->tail) {
                                        break;
                                }
                                pid->sech3[pid->sech3_idx] = *p++;
                        }

                        /* get section_length for single packet section */
                        if(3 == pid->sech3_idx) {
                                dat = pid->sech3[0];
                                pid->table_id = dat;

                                dat = pid->sech3[1];
                                pid->section_length = dat & 0x0F;

                                dat = pid->sech3[2];
                                pid->section_length <<= 8;
                                pid->section_length  |= dat;
                        }

                        if(pid->section_length > pid->payload_total) {
                                /* multi-packets section, make pkt list */
                                struct ts_pkt *new_pkt = (struct ts_pkt *)buddy_malloc(obj->mp, sizeof(struct ts_pkt));
                                if(!new_pkt) {
                                        RPT(RPT_ERR, "malloc for pkt node failed");
                                        return -1;
                                }
                                memcpy(new_pkt->pkt, obj->ipt.TS, TS_PKT_SIZE);
                                new_pkt->payload_unit_start_indicator = 1;
                                new_pkt->payload_size = (obj->tail - obj->cur);
                                zlst_push(&(pid->pkt0), new_pkt);
                        }
                        else {
                                /* single packet section, for efficienc: directly make section without pkt list */
                                struct ts_sect *new_sect = (struct ts_sect *)buddy_malloc(obj->mp, sizeof(struct ts_sect));
                                if(!new_sect) {
                                        RPT(RPT_ERR, "malloc section node failed");
                                        goto ts2sect_free_pkt_list;
                                }

                                new_sect->section = (uint8_t *)buddy_malloc(obj->mp, 3 + pid->section_length);
                                if(!new_sect->section) {
                                        RPT(RPT_ERR, "malloc data buffer of section node failed");
                                        goto ts2sect_free_pkt_list;
                                }

#if 0
#define DEBUG_SECTION_FRAGMENT
#endif
                                memcpy(new_sect->section, obj->cur, 3 + pid->section_length);
#ifdef DEBUG_SECTION_FRAGMENT
                                fprintf(stderr, "(%02X %4d) 3+%d.\n", pid->table_id, pid->payload_total, pid->section_length);
#endif
                                //dump(new_sect->section, 3 + pid->section_length);
                                ts_parse_sect(obj, new_sect); /* note: ts_parse_sect() should free new_sect */
                        }
                }
                else { /* !(tsh->payload_unit_start_indicator) */
                        RPT(RPT_DBG, "section async, ignore this packet");
                        return -1;
                }
        }
        else { /* (pid->pkt0) */
                /* next packet of this section */
                struct ts_pkt *new_pkt = (struct ts_pkt *)buddy_malloc(obj->mp, sizeof(struct ts_pkt));
                if(!new_pkt) {
                        RPT(RPT_ERR, "malloc for packet node failed");
                        goto ts2sect_free_pkt_list;
                }

                memcpy(new_pkt->pkt, obj->ipt.TS, TS_PKT_SIZE);
                if(tsh->payload_unit_start_indicator) {
                        new_pkt->payload_unit_start_indicator = 1;
                        pid->has_new_sech = 1; /* a new section in this packet */
                        dat = *(obj->cur)++; /* pointer_field */
                }
                else {
                        new_pkt->payload_unit_start_indicator = 0;
                }
                new_pkt->payload_size = (obj->tail - obj->cur);
                pid->payload_total += new_pkt->payload_size;
                zlst_push(&(pid->pkt0), new_pkt);

                /* sect head 3-byte */
                p = obj->cur;
                for(; pid->sech3_idx < 3; pid->sech3_idx++) {
                        if(p >= obj->tail) {
                                break;
                        }
                        pid->sech3[pid->sech3_idx] = *p++;
               }
        }

        /* try to make section */
        while(pid->pkt0) {
                /* get section_length */
                if(3 == pid->sech3_idx) {
                        uint8_t section_syntax_indicator; /* 1-bit */

                        dat = pid->sech3[0];
                        pid->table_id = dat;

                        dat = pid->sech3[1];
                        section_syntax_indicator = (dat & BIT(7)) >> 7;
                        pid->section_length = dat & 0x0F;

                        dat = pid->sech3[2];
                        pid->section_length <<= 8;
                        pid->section_length  |= dat;

                        pid->sech3_idx = 4; /* got section_length */

                        if(section_syntax_indicator) {
                                if(pid->section_length > NORMAL_SECTION_LENGTH_MAX) {
                                        RPT(RPT_ERR, "normal section_length(%d) > %d",
                                            pid->section_length, NORMAL_SECTION_LENGTH_MAX);
                                        goto ts2sect_free_pkt_list;
                                }
                        }
                        else { /* !(section_syntax_indicator) */
                                if(pid->section_length > PRIVATE_SECTION_LENGTH_MAX) {
                                        RPT(RPT_ERR, "private section_length(%d) > %d",
                                            pid->section_length, PRIVATE_SECTION_LENGTH_MAX);
                                        goto ts2sect_free_pkt_list;
                                }
                        }
                        RPT(RPT_INF, "table_id: 0x%02X, length: 3 + %d", pid->table_id, pid->section_length);
                }

                if(pid->payload_total >= (3 + pid->section_length) || pid->has_new_sech) {
                        /* has one section */
                        struct ts_sect *new_sect = (struct ts_sect *)buddy_malloc(obj->mp, sizeof(struct ts_sect));
                        if(!new_sect) {
                                RPT(RPT_ERR, "malloc section node failed");
                                goto ts2sect_free_pkt_list;
                        }

                        new_sect->section = (uint8_t *)buddy_malloc(obj->mp, 3 + pid->section_length);
                        if(!new_sect->section) {
                                RPT(RPT_ERR, "malloc data buffer of section node failed");
                                goto ts2sect_free_pkt_list;
                        }

                        p = new_sect->section;
                        int left_length = 3 + pid->section_length;

#ifdef DEBUG_SECTION_FRAGMENT
                        fprintf(stderr, "(%02X %4d) 3+%d ", pid->table_id, pid->payload_total, pid->section_length);
#endif
                        RPT(RPT_INF, "pkt list -> ts_sect and parse, table: 0x%02X", pid->table_id);
                        while(NULL != (pkt = (struct ts_pkt *)zlst_shift(&(pid->pkt0)))) {
                                if(pkt->payload_unit_start_indicator && p != new_sect->section) {
                                        /* use start_indicator instead of section_length to determine section end */
                                        left_length = (int)(pkt->pkt[4]); /* pointer_field is just left length */
                                }

                                if(pkt->payload_size < left_length) {
                                        /* part of big section */
                                        memcpy(p, pkt->pkt + TS_PKT_SIZE - pkt->payload_size, pkt->payload_size);
                                        p += pkt->payload_size;
                                        left_length -= pkt->payload_size;
                                        buddy_free(obj->mp, pkt);
#ifdef DEBUG_SECTION_FRAGMENT
                                        fprintf(stderr, "- %3d = %4d ", pkt->payload_size, left_length);
#endif
                                }
                                else { /* (pkt->payload_size >= left_length) */
                                        /* last packet of the section */
                                        memcpy(p, pkt->pkt + TS_PKT_SIZE - pkt->payload_size, left_length);
                                        pkt->payload_size -= left_length; /* maybe head of next section */
#ifdef DEBUG_SECTION_FRAGMENT
                                        fprintf(stderr, "- %3d ", left_length);
#endif
                                        //dump(new_sect->section, 3 + pid->section_length);
                                        ts_parse_sect(obj, new_sect); /* note: ts_parse_sect() should free new_sect */

                                        if(pkt->payload_unit_start_indicator && p != new_sect->section) {
                                                /* new section */
                                                obj->tail = pkt->pkt + TS_PKT_SIZE;
                                                obj->cur = obj->tail - pkt->payload_size;
#ifdef DEBUG_SECTION_FRAGMENT
                                                fprintf(stderr, "(%d-byte new section head)\n", pkt->payload_size);
                                                dump(obj->cur, pkt->payload_size);
#endif
                                                pid->payload_total = pkt->payload_size;
                                                pid->has_new_sech = 0;
                                                zlst_push(&(pid->pkt0), pkt);
                                                pid->section_length = PRIVATE_SECTION_LENGTH_MAX; /* suppose maximum length */

                                                /* sect head 3-byte */
                                                p = obj->cur;
                                                for(pid->sech3_idx = 0; pid->sech3_idx < 3; pid->sech3_idx++) {
                                                        if(p >= obj->tail) {
                                                                break;
                                                        }
                                                        pid->sech3[pid->sech3_idx] = *p++;
                                                }
                                        }
                                        else {
#ifdef DEBUG_SECTION_FRAGMENT
                                                fprintf(stderr, "(%d-byte padding data)\n", pkt->payload_size);
#endif
                                                buddy_free(obj->mp, pkt);
                                        }
                                        break;
                                }
                        } /* while(pkt) */
                }
                else { /* not enough data to make section */
                        break;
                }
        } /* while(pid->pkt0) */
        return 0;

ts2sect_free_pkt_list:
        while(NULL != (pkt = (struct ts_pkt *)zlst_pop(&(pid->pkt0)))) {
                buddy_free(obj->mp, pkt);
        }
        return -1;
}

static int ts_parse_sect(struct ts_obj *obj, struct ts_sect *new_sect)
{
        uint8_t *p;
        struct ts_tabl *tabl;
        struct ts_pid *pid = obj->pid;
        struct ts_err *err = &(obj->err);
#if 0
        int is_new_version = 0;
#endif

        /* get section head info */
        p = new_sect->section;
        new_sect->table_id = *p++;
        new_sect->section_syntax_indicator = (*p & BIT(7)) >> 7;
        new_sect->private_indicator = (*p & BIT(6)) >> 6;
        new_sect->section_length   = *p++ & 0x0F;
        new_sect->section_length <<= 8;
        new_sect->section_length  |= *p++;
        if(1 == new_sect->section_syntax_indicator) {
                /* normal section syntax */
                new_sect->table_id_extension   = *p++;
                new_sect->table_id_extension <<= 8;
                new_sect->table_id_extension  |= *p++;
                new_sect->version_number = (*p & 0x3E) >> 1;
                new_sect->current_next_indicator = *p++ & BIT(0);
                new_sect->section_number = *p++;
                new_sect->last_section_number = *p++;

                const struct table_id_table *table_id_table;

                table_id_table = table_type(new_sect->table_id);
                new_sect->check_CRC = table_id_table->check_CRC;
                new_sect->type = table_id_table->type;
        }
        else {
                /* private section syntax */
                new_sect->table_id_extension = 0;
                new_sect->version_number = 0;
                new_sect->current_next_indicator = 0;
                new_sect->section_number = 0;
                new_sect->last_section_number = 0;

                new_sect->check_CRC = 0;
                new_sect->type = TS_TYPE_USR;
        }

        RPT(RPT_INF, "table: 0x%02X; len: %4d; sect: %d/%d",
            new_sect->table_id,
            new_sect->section_length,
            new_sect->section_number,
            new_sect->last_section_number);

        /* CRC check */
        if(new_sect->check_CRC) {
                p = new_sect->section + 3 + new_sect->section_length - 4;
                obj->CRC_32   = *p++;
                obj->CRC_32 <<= 8;
                obj->CRC_32  |= *p++;
                obj->CRC_32 <<= 8;
                obj->CRC_32  |= *p++;
                obj->CRC_32 <<= 8;
                obj->CRC_32  |= *p++;

                obj->CRC_32_calc = ts_crc(new_sect->section, 3 + new_sect->section_length - 4, 32);
                if(obj->CRC_32_calc != obj->CRC_32) {
                        err->CRC_error = 1;
#if 0
                        RPT(RPT_ERR, "CRC error(0x%08X! 0x%08X?)",
                            obj->CRC_32_calc, obj->CRC_32);
                        dump(obj->ipt.TS, TS_PKT_SIZE);
                        dump(new_sect->section, 3 + new_sect->section_length);
#endif
                        goto release_sect;
                }
        }

        /* get "tabl" */
        if(0x02 == new_sect->table_id) {
                /* is PMT section */
                if(!(pid->prog)) {
                        RPT(RPT_WRN, "PMT without corresponding program, ignore");
                        goto release_sect;
                }
                tabl = &(pid->prog->tabl);
        }
        else {
                /* not PMT section */
                RPT(RPT_DBG, "search 0x%02X in table_list", new_sect->table_id);
                tabl = (struct ts_tabl *)zlst_search(&(obj->tabl0), new_sect->table_id);
                if(!tabl) {
                        tabl = (struct ts_tabl *)buddy_malloc(obj->mp, sizeof(struct ts_tabl));
                        if(!tabl) {
                                RPT(RPT_ERR, "malloc ts_tabl node failed");
                                goto release_sect;
                        }

                        tabl->sect0 = NULL;
                        tabl->table_id = new_sect->table_id;
                        tabl->version_number = new_sect->version_number;
                        tabl->last_section_number = new_sect->last_section_number;
                        tabl->STC = STC_OVF;

                        RPT(RPT_DBG, "insert 0x%02X in table_list", tabl->table_id);
                        zlst_set_key(tabl, tabl->table_id);
                        if(0 != zlst_insert(&(obj->tabl0), tabl)) {
                                free_tabl(obj->mp, tabl);
                                goto release_sect;
                        }
                }
        }

        /* get "psect0" */
        struct znode **psect0 = (struct znode **)&(tabl->sect0);

        /* new table version? */
        if(tabl->version_number != new_sect->version_number) {
                struct ts_sect *sect_node;

                /* clear psect0 and update table parameter */
                RPT(RPT_DBG, "version_number(%d -> %d), free old sections",
                    tabl->version_number,
                    new_sect->version_number);
                tabl->version_number = new_sect->version_number;
                tabl->last_section_number = new_sect->last_section_number;
                while(NULL != (sect_node = (struct ts_sect *)zlst_pop(psect0))) {
                        free_sect(obj->mp, sect_node);
                };
#if 0
                is_new_version = 1;
#endif
        }

        struct ts_sect *sect;

        /* get "section" pointer */
        RPT(RPT_DBG, "search %d/%d in sect_list", new_sect->section_number, new_sect->last_section_number);
        sect = (struct ts_sect *)zlst_search(psect0, new_sect->section_number);
        if(!sect) {
                sect = new_sect;
                RPT(RPT_DBG, "insert %d/%d in sect_list", sect->section_number, sect->last_section_number);
                zlst_set_key(sect, sect->section_number);
                if(0 != zlst_insert(psect0, sect)) {
                        goto release_sect;
                }
        }
        else {
                RPT(RPT_INF, "has section %02X/%02X(table %02X) already",
                    sect->section_number,
                    sect->last_section_number,
                    sect->table_id);
                free_sect(obj->mp, new_sect);
                //return -1; FIXME: got SDT before PMT will lost service info, so parse again and again now
        }
        obj->sect = sect; /* has section */

        /* sect_interval */
        if(STC_OVF != tabl->STC &&
           STC_OVF != obj->STC) {
                obj->sect_interval = ts_timestamp_diff(obj->STC, tabl->STC, STC_OVF);
        }
        else {
                obj->sect_interval = 0;
        }
        tabl->STC = obj->STC;

        /* PAT_error(table_id error) */
        if(0x0000 == pid->PID && 0x00 != sect->table_id) {
                err->PAT_error = ERR_1_3_1;
                dump(obj->ipt.TS, TS_PKT_SIZE);
                dump(sect->section, 8);
                goto release_sect;
        }

        /* parse */
        switch(sect->table_id) {
                case 0x00:
                        if(0x0000 != pid->PID) {
                                RPT(RPT_ERR, "PAT: PID is not 0x0000 but 0x%04X, ignore!", pid->PID);
                                goto release_sect;
                        }
                        ts_parse_secb_pat(obj);
                        break;
                case 0x01:
                        if(0x0001 != pid->PID) {
                                RPT(RPT_ERR, "CAT: PID is not 0x0001 but 0x%04X, ignore!", pid->PID);
                                goto release_sect;
                        }
                        ts_parse_secb_cat(obj);
                        break;
                case 0x02:
                        if(!IS_TYPE(TS_TYPE_PMT, pid->type)) {
                                RPT(RPT_ERR, "PMT: PID is NOT PMT_PID but 0x%04X, ignore!", pid->PID);
                                goto release_sect;
                        }
                        ts_parse_secb_pmt(obj);
                        break;
                case 0x42:
                        if(0x0011 != pid->PID) {
                                RPT(RPT_ERR, "SDT: PID is not 0x0011 but 0x%04X, ignore!", pid->PID);
                                goto release_sect;
                        }
                        ts_parse_secb_sdt(obj);
                        break;
                default:
                        RPT(RPT_DBG, "meet table(0x%02X), ignore", sect->table_id);
                        break;
        }
        return 0;

release_sect:
        free_sect(obj->mp, new_sect);
        return -1;
}

static int ts_parse_secb_pat(struct ts_obj *obj)
{
        struct ts_sect *sect = obj->sect;
        struct ts_tsh *tsh = &(obj->tsh);
        struct ts_err *err = &(obj->err);
        uint8_t dat;
        uint8_t *cur = sect->section + 8;
        uint8_t *crc = sect->section + 3 + sect->section_length - 4;
        struct ts_prog *prog;
        struct ts_pid ts_new_pid, *new_pid = &ts_new_pid;

        /* PAT_error */
        if(obj->sect_interval > 500 * STC_MS) {
                err->PAT_error = ERR_1_3_0;
        }
        if(0x00 != tsh->transport_scrambling_control) {
                err->PAT_error = ERR_1_3_2;
        }

        /* to avoid stack overflow, FIXME */
        if(obj->prog0) {
                return 0;
        }

        /* in PAT, table_id_extension is transport_stream_id */
        obj->transport_stream_id = sect->table_id_extension;
        obj->has_got_transport_stream_id = 1;

        while(cur < crc) {
                /* add program */
                prog = (struct ts_prog *)buddy_malloc(obj->mp, sizeof(struct ts_prog));
                if(!prog) {
                        RPT(RPT_ERR, "malloc prog node failed");
                        return -1;
                }
                prog->elem0 = NULL;
                prog->tabl.sect0 = NULL;
                prog->program_info_len = 0;
                prog->program_info = NULL;
                prog->service_name_len = 0;
                prog->service_name = NULL;
                prog->service_provider_len = 0;
                prog->service_provider = NULL;

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
                        new_pid->type = TS_TYPE_NIT;

                        if(0x0010 != new_pid->PID) {
#if 1
                                RPT(RPT_ERR, "NIT_PID(0x%04X) is NOT 0x0010!", new_pid->PID);
#endif
                        }
                        free_prog(obj->mp, prog);
                }
                else {
                        struct znode *znode;

                        new_pid->type = TS_TYPE_PMT;

                        if(!(obj->prog0)) {
                                /* traverse pid_list: if it des not belong to any program, use prog0 */
                                for(znode = (struct znode *)(obj->pid0); znode; znode = znode->next) {
                                        struct ts_pid *pid_item = (struct ts_pid *)znode;
                                        if(pid_item->PID < 0x0020 || pid_item->PID == 0x1FFF) {
                                                pid_item->prog = prog;
                                        }
                                }
                        }

                        /* program info */
                        prog->program_info_len = 0;
                        prog->program_info = NULL;

                        /* SDT info */
                        prog->service_name_len = 0;
                        prog->service_name = NULL;
                        prog->service_provider_len = 0;
                        prog->service_provider = NULL;

                        /* elementary stream list */
                        prog->elem0 = NULL;

                        /* PMT table */
                        prog->is_parsed = 0;
                        prog->tabl.table_id = 0x02;
                        prog->tabl.version_number = 0xFF; /* never reached version */
                        prog->tabl.last_section_number = 0; /* no use */
                        prog->tabl.sect0 = NULL;
                        prog->tabl.STC = STC_OVF;

                        /* for STC calc */
                        prog->ADDa = 0;
                        prog->PCRa = STC_OVF;
                        prog->ADDb = 0;
                        prog->PCRb = STC_OVF;
                        prog->is_STC_sync = 0;

                        RPT(RPT_DBG, "insert 0x%04X in prog_list", prog->program_number);
                        zlst_set_key(prog, prog->program_number);
                        if(0 != zlst_insert(&(obj->prog0), prog)) {
                                free_prog(obj->mp, prog);
                                return -1;
                        }
                }

                new_pid->cnt = 0;
                new_pid->lcnt = 0;
                new_pid->prog = prog;
                new_pid->elem = NULL;
                new_pid->CC = 0;
                new_pid->is_CC_sync = 0;
                update_pid_list(obj, new_pid); /* NIT or PMT PID */
        }

        return 0;
}

static int ts_parse_secb_cat(struct ts_obj *obj)
{
        struct ts_sect *sect = obj->sect;
        uint8_t dat;
        uint8_t *cur = sect->section + 8;
        uint8_t *crc = sect->section + 3 + sect->section_length - 4;
        struct ts_pid ts_new_pid, *new_pid = &ts_new_pid;

        while(cur < crc) {
                uint8_t tag;
                uint8_t len;
                uint8_t *pt;

                tag = *cur++;
                len = *cur++;
                pt = cur;
                if((0xFF == tag) || (0 == len)) {
                        RPT(RPT_ERR, "wrong descriptor(tag: %d, len: %d)", tag, len);
                        return -1;
                }
                if(0x09 == tag) { /* CA_descriptor in CAT */
                        uint16_t CA_system_ID;
                        uint16_t CA_PID;

                        dat = *pt++;
                        CA_system_ID = dat;

                        dat = *pt++;
                        CA_system_ID <<= 8;
                        CA_system_ID |= dat;

                        dat = *pt++;
                        CA_PID = dat & 0x1F;

                        dat = *pt++;
                        CA_PID <<= 8;
                        CA_PID |= dat;

                        new_pid->PID = CA_PID;
                        new_pid->cnt = 0;
                        new_pid->lcnt = 0;
                        new_pid->prog = NULL;
                        new_pid->elem = NULL;
                        new_pid->type = TS_TYPE_EMM;
                        new_pid->CC = 0;
                        new_pid->is_CC_sync = 0;
                        RPT(RPT_INF, "add EMM_PID(0x%04X)", CA_PID);
                        update_pid_list(obj, new_pid);
                }
                cur += len;
        }

        return 0;
}

static int ts_parse_secb_pmt(struct ts_obj *obj)
{
        struct ts_sect *sect = obj->sect;
        struct ts_tsh *tsh = &(obj->tsh);
        struct ts_err *err = &(obj->err);
        uint8_t dat;
        uint8_t *cur = sect->section + 8;
        uint8_t *crc = sect->section + 3 + sect->section_length - 4;
        struct ts_prog *prog;
        struct ts_pid ts_new_pid, *new_pid = &ts_new_pid;

        /* PMT_error */
        if(obj->sect_interval > 500 * STC_MS) {
                err->PMT_error = ERR_1_5_0;
        }
        if(0x00 != tsh->transport_scrambling_control) {
                err->PMT_error = ERR_1_5_1;
        }

        /* in PMT, table_id_extension is program_number */
        RPT(RPT_DBG, "search 0x%04X in prog_list", sect->table_id_extension);
        prog = (struct ts_prog *)zlst_search(&(obj->prog0), sect->table_id_extension);
        if((!prog) || (prog->is_parsed)) {
                return -1; /* parsed program, ignore */
        }

        obj->is_psi_si = 1;
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
        new_pid->type = ((0x1FFF != new_pid->PID) ? TS_TYPE_PCR : TS_TYPE_NULP);
        new_pid->CC = 0;
        new_pid->is_CC_sync = 1;
        update_pid_list(obj, new_pid); /* PCR_PID */

        /* program_info_length */
        dat = *cur++;
        prog->program_info_len = dat & 0x0F;

        dat = *cur++;
        prog->program_info_len <<= 8;
        prog->program_info_len |= dat;

        /* record program_info */
        if(0 != prog->program_info_len && !(prog->program_info)) {
                if(prog->program_info_len > INFO_LEN_MAX) {
                        RPT(RPT_ERR, "PID(0x%04X): program_info_length(%d) too big!",
                            tsh->PID, prog->program_info_len);
                        return -1;
                }
                else {
                        prog->program_info = (uint8_t *)buddy_malloc(obj->mp, prog->program_info_len);
                        if(!(prog->program_info)) {
                                RPT(RPT_ERR, "malloc for prog_info buffer failed");
                                return -1;
                        }
                        memcpy(prog->program_info, cur, prog->program_info_len);
                }
        }
        cur += prog->program_info_len;

        while(cur < crc) {
                struct ts_elem *elem;
                uint8_t *next; /* point to the data after descriptors */

                /* add elementary stream */
                elem = (struct ts_elem *)buddy_malloc(obj->mp, sizeof(struct ts_elem));
                if(!elem) {
                        RPT(RPT_ERR, "malloc elem node failed");
                        return -1;
                }
                elem->es_info = NULL;

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

                /* ES_info */
                if(0 != elem->es_info_len && !(elem->es_info)) {
                        if(elem->es_info_len >= INFO_LEN_MAX) {
                                RPT(RPT_ERR, "PID(0x%04X): ES_info_length(%d) too big!",
                                    elem->PID, elem->es_info_len);
                                return -1;
                        }
                        else {
                                elem->es_info = (uint8_t *)buddy_malloc(obj->mp, elem->es_info_len);
                                if(!(elem->es_info)) {
                                        RPT(RPT_ERR, "malloc for es_info buffer failed");
                                        return -1;
                                }
                                memcpy(elem->es_info, cur, elem->es_info_len);
                        }
                }

                next = cur + elem->es_info_len;
                while(cur < next) {
                        uint8_t tag;
                        uint8_t len;
                        uint8_t *pt;

                        tag = *cur++;
                        len = *cur++;
                        pt = cur;
                        if((0xFF == tag) || (0 == len)) {
                                RPT(RPT_ERR, "wrong descriptor(tag: %d, len: %d)", tag, len);
                                return -1;
                        }
                        if(0x09 == tag) { /* CA_descriptor in PMT */
                                uint16_t CA_system_ID;
                                uint16_t CA_PID;

                                dat = *pt++;
                                CA_system_ID = dat;

                                dat = *pt++;
                                CA_system_ID <<= 8;
                                CA_system_ID |= dat;

                                dat = *pt++;
                                CA_PID = dat & 0x1F;

                                dat = *pt++;
                                CA_PID <<= 8;
                                CA_PID |= dat;

                                new_pid->PID = CA_PID;
                                new_pid->cnt = 0;
                                new_pid->lcnt = 0;
                                new_pid->prog = prog;
                                new_pid->elem = elem;
                                new_pid->type = TS_TYPE_ECM;
                                new_pid->CC = 0;
                                new_pid->is_CC_sync = 0;
                                RPT(RPT_INF, "add ECM_PID(0x%04X)", CA_PID);
                                update_pid_list(obj, new_pid);
                        }
                        cur += len;
                }

                elem->type = elem_type(elem->stream_type)->type;
                if(elem->PID == prog->PCR_PID) {
                        elem->type |= TS_TMSK_PCR;
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
                update_pid_list(obj, new_pid); /* elementary_PID */
        }

        return 0;
}

static int ts_parse_secb_sdt(struct ts_obj *obj)
{
        struct ts_sect *sect = obj->sect;
        uint8_t dat;
        uint8_t *cur = sect->section + 8;
        uint8_t *crc = sect->section + 3 + sect->section_length - 4;
        uint16_t original_network_id;

        /* in SDT, table_id_extension is transport_stream_id */
        if(obj->has_got_transport_stream_id &&
           sect->table_id_extension != obj->transport_stream_id) {
                RPT(RPT_ERR, "table_id(0x%02X): table_id_extension(%d) != transport_stream_id(%d)",
                    sect->table_id,
                    sect->table_id_extension,
                    obj->transport_stream_id);
                return -1; /* bad SDT table, ignore */
        }

        dat = *cur++;
        original_network_id = dat;

        dat = *cur++;
        original_network_id <<= 8;
        original_network_id |= dat;
        if(obj->has_got_transport_stream_id &&
           original_network_id != obj->transport_stream_id) {
#if 0
                RPT(RPT_ERR, "table_id(0x%02X): original_network_id(%d) != transport_stream_id(%d)",
                    sect->table_id,
                    original_network_id,
                    obj->transport_stream_id);
                return -1; /* bad SDT table, ignore */
#endif
        }

        dat = *cur++; /* reserved_future_use */

        while(cur < crc) {
                uint8_t *next; /* point to the data after descriptors */
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
                prog = (struct ts_prog *)zlst_search(&(obj->prog0), service_id);

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

                next = cur + descriptors_loop_length;
                while(cur < next) {
                        uint8_t tag;
                        uint8_t len;
                        uint8_t *pt;

                        tag = *cur++;
                        len = *cur++;
                        pt = cur;
                        if((0xFF == tag) || (0 == len)) {
                                RPT(RPT_ERR, "wrong descriptor(tag: %d, len: %d)", tag, len);
                                return -1;
                        }
                        if(0x48 == tag && prog) {
                                /* service_descriptor */
                                pt++; /* ignore type */

                                prog->service_provider_len = *pt++;
                                if(0 != prog->service_provider_len) {
                                        if(prog->service_provider) {
                                                buddy_free(obj->mp, prog->service_provider);
                                        }
                                        prog->service_provider = (uint8_t *)buddy_malloc(obj->mp, (uint16_t)1 + prog->service_provider_len);
                                        if(!(prog->service_provider)) {
                                                RPT(RPT_ERR, "malloc for service_provider buffer failed");
                                                return -1;
                                        }
                                        memcpy(prog->service_provider, pt, prog->service_provider_len);
                                        prog->service_provider[prog->service_provider_len] = '\0';
                                }

                                pt += prog->service_provider_len; /* pass provider */

                                prog->service_name_len = *pt++;
                                if(0 != prog->service_name_len) {
                                        if(prog->service_name) {
                                                buddy_free(obj->mp, prog->service_name);
                                        }
                                        prog->service_name = (uint8_t *)buddy_malloc(obj->mp, (uint16_t)1 + prog->service_name_len);
                                        if(!(prog->service_name)) {
                                                RPT(RPT_ERR, "malloc for service_name buffer failed");
                                                return -1;
                                        }
                                        memcpy(prog->service_name, pt, prog->service_name_len);
                                        prog->service_name[prog->service_name_len] = '\0';
                                }
                        }
                        cur += len;
                }
        }

        return 0;
}

static int ts_parse_pesh(struct ts_obj *obj)
{
        struct ts_tsh *tsh = &(obj->tsh);
        struct ts_pesh *pesh = &(obj->pesh);
        struct ts_pid *pid = obj->pid;
        struct ts_elem *elem = pid->elem; /* may be NULL */
        uint8_t dat;

        /* for some bad stream */
        if(tsh->PID < 0x0020) {
                RPT(RPT_ERR, "PES: bad PID(0x%04X)!", tsh->PID);
                return -1;
        }
        if(!elem) {
                RPT(RPT_ERR, "PES: not elem PID(0x%04X)!", tsh->PID);
                return -1;
        }

        /* PES align */
        if(tsh->payload_unit_start_indicator) {
                elem->is_pes_align = 1;
        }

        /* record PES data */
        if(obj->cfg.need_pes_align) {
                if(elem->is_pes_align) {
                        obj->PES_len = obj->tail - obj->cur;
                        obj->PES = obj->cur;
                }
                else {
                        /* ignore these PES data */
                }
        }
        else {
                obj->PES_len = obj->tail - obj->cur;
                obj->PES = obj->cur;
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
                        RPT(RPT_ERR, "PES packet start code prefix(0x%06X) NOT 0x000001!",
                            pesh->packet_start_code_prefix);
                        dump(obj->ipt.TS, TS_PKT_SIZE);
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
                RPT(RPT_ERR, "PES_packet_length = %d(0x%X)",
                    pesh->PES_packet_length,
                    pesh->PES_packet_length);
#endif

                ts_parse_pesh_switch(obj);
        }

        /* record ES data */
        if(obj->cfg.need_pes_align) {
                if(elem->is_pes_align) {
                        obj->ES_len = obj->tail - obj->cur;
                        obj->ES = obj->cur;
                }
                else {
                        /* ignore these ES data */
                }
        }
        else {
                obj->ES_len = obj->tail - obj->cur;
                obj->ES = obj->cur;
        }

        return 0;
}

static int ts_parse_pesh_switch(struct ts_obj *obj)
{
        struct ts_pesh *pesh = &(obj->pesh);

        switch(pesh->stream_id) {
                case 0xBE: /* padding_stream */
                        /* subsequent pesh->PES_packet_length data is padding_byte, pass */
                        if(pesh->PES_packet_length > (obj->tail - obj->cur)) {
                                RPT(RPT_ERR, "PES_packet_length(%d) for padding_stream is too large!",
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
        struct ts_pesh *pesh = &(obj->pesh);
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
                RPT(RPT_ERR, "PES_header_data_length(%d) is too long!",
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

                obj->has_pts = 1;
                obj->PTS = pesh->PTS;

                /* DTS */
                obj->DTS = pesh->PTS; /* no DTS, DTS = PTS */
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

                obj->has_pts = 1;
                obj->PTS = pesh->PTS;

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

                obj->has_dts = 1;
                obj->DTS = pesh->DTS;
        }
        else if(0x01 == pesh->PTS_DTS_flags) { /* '01' */
                RPT(RPT_ERR, "PTS_DTS_flags error!");
                dump(obj->ipt.TS, TS_PKT_SIZE);
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
                        for(int i = 0; i < 16; i++) {
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

static struct ts_pid *update_pid_list(struct ts_obj *obj, struct ts_pid *new_pid)
{
        struct ts_pid *pid;

        pid = (struct ts_pid *)zlst_search(&(obj->pid0), new_pid->PID);
        if(pid) {
                /* is in pid_list already, just update information */
                pid->PID = new_pid->PID;
                pid->prog = new_pid->prog;
                pid->elem = new_pid->elem;
                pid->type = new_pid->type;
                pid->cnt = new_pid->cnt;
                pid->lcnt = new_pid->lcnt;
                pid->CC = new_pid->CC;
                pid->is_CC_sync = new_pid->is_CC_sync;
        }
        else {
                pid = (struct ts_pid *)buddy_malloc(obj->mp, sizeof(struct ts_pid));
                if(!pid) {
                        RPT(RPT_ERR, "malloc pid node failed");
                        return NULL;
                }
                pid->pkt0 = NULL; /* wait to sync with section head */

                pid->PID = new_pid->PID;
                pid->type = new_pid->type;
                pid->prog = new_pid->prog;
                pid->elem = new_pid->elem;
                pid->is_CC_sync = new_pid->is_CC_sync;
                pid->CC = new_pid->CC;
                pid->cnt = new_pid->cnt;
                pid->lcnt = new_pid->lcnt;

                RPT(RPT_DBG, "insert 0x%04X in pid_list", pid->PID);
                zlst_set_key(pid, pid->PID);
                if(0 != zlst_insert(&(obj->pid0), pid)) {
                        free_pid(obj->mp, pid);
                        return NULL;
                }
        }
        return pid;
}

static int is_all_prog_parsed(struct ts_obj *obj)
{
        uint8_t section_number;
        struct znode *znode_p; /* znode of program list */

        for(znode_p = (struct znode *)(obj->prog0); znode_p; znode_p = znode_p->next) {
                struct ts_prog *prog = (struct ts_prog *)znode_p;
                struct ts_tabl *tabl = &(prog->tabl);
                struct znode *znode_s; /* znode of section list */

                if(0xFF == tabl->version_number) {
                        return 0;
                }

                section_number = 0;
                for(znode_s = (struct znode *)(tabl->sect0); znode_s; znode_s = znode_s->next) {
                        struct ts_sect *sect = (struct ts_sect *)znode_s;

                        if(section_number > tabl->last_section_number) {
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

        for(p = PID_TABLE; p->max != 0xFFFF; p++) {
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

static const struct stream_type_table *elem_type(uint8_t stream_type)
{
        const struct stream_type_table *p;

        for(p = STREAM_TYPE_TABLE; 0xFF != p->stream_type; p++) {
                if(p->stream_type == stream_type) {
                        break;
                }
        }
        return p;
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

uint32_t ts_crc(void *buf, size_t size, int mode)
{
        int bitcount = 0;
        int bitinbyte = 0;
        unsigned short databit;
        unsigned short shiftreg[32];
        /*                      0 1 2 3 4 5 6 7 8 */
        unsigned short g08[] = {1,1,1,0,0,0,0,0,1};
        /*                      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 */
        unsigned short g16[] = {1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,1};
        /*                      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 */
        unsigned short g32[] = {1,1,1,0,1,1,0,1,1,0,1,1,1,0,0,0,1,0,0,0,0,0,1,1,0,0,1,0,0,0,0,0,1};

        unsigned short *g;
        int i,nrbits;
        char *data;
        int cnt;
        uint32_t crc;

        switch(mode) {
                case  8: g = g08; cnt =  8; break;
                case 16: g = g16; cnt = 16; break;
                default: g = g32; cnt = 32; break;
        }

        /* Initialize shift register's to '1' */
        for(i = 0; i < cnt; i++) {
                shiftreg[i] = 1;
        }

        /* Calculate nr of data bits */
        nrbits = ((int) size) * 8;
        data = buf;

        while(bitcount < nrbits) {
                /* Fetch bit from bitstream */
                databit = (short int) (*data  & (0x80 >> bitinbyte));
                databit = databit >> (7 - bitinbyte);
                bitinbyte++;
                bitcount++;
                if(bitinbyte == 8) {
                        bitinbyte = 0;
                        data++;
                }

                /* Perform the shift and modula 2 addition */
                databit ^= shiftreg[cnt - 1];
                i = cnt - 1;
                while (i != 0) {
                        if (g[i]) {
                                shiftreg[i] = shiftreg[i-1] ^ databit;
                        }
                        else {
                                shiftreg[i] = shiftreg[i-1];
                        }
                        i--;
                }
                shiftreg[0] = databit;
        }

        /* make CRC an UIMSBF */
        crc = 0;
        for(i = 0; i < cnt; i++) {
                crc = (crc << 1) | ((unsigned int) shiftreg[cnt - 1 - i]);
        }

        return crc;
}

#define timestamp_assert(expr, ...) \
        do { \
                if(!(expr)) { \
                        fprintf(stderr, "%s: %d: assert (%s) failed: ", \
                                __FILE__, __LINE__, #expr); \
                        fprintf(stderr, __VA_ARGS__); \
                        fprintf(stderr, "\n"); \
                        return -1; \
                } \
        }while(0)

int64_t ts_timestamp_add(int64_t t0, int64_t td, int64_t ovf)
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

int64_t ts_timestamp_diff(int64_t t1, int64_t t0, int64_t ovf)
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
