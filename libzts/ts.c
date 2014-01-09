/* vim: set tabstop=8 shiftwidth=8:
 * name: ts.c
 * funx: analyse ts stream
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memset, memcpy, etc */
#ifdef _MSC_VER
        #ifdef _M_X64
                #define __PRI64 "l"
        #else
                #define __PRI64 "ll"
        #endif

        #define PRId8  "d"
        #define PRId16 "d"
        #define PRId32 "d"
        #define PRId64 __PRI64"d"
#else /* unix-like */
        #include <inttypes.h> /* for int?_t, PRId64, etc */
#endif

#include "buddy.h"
#include "ts.h"

/* report level and macro */
#define RPT_ERR (1) /* error, system error */
#define RPT_WRN (2) /* warning, maybe wrong, maybe OK */
#define RPT_INF (3) /* important information */
#define RPT_DBG (4) /* debug information */

#ifdef S_SPLINT_S /* FIXME */
#define RPTERR(fmt...) do {if(RPT_ERR <= rpt_lvl) {fprintf(stderr, "%s: %d: err: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0 == 1)
#define RPTWRN(fmt...) do {if(RPT_WRN <= rpt_lvl) {fprintf(stderr, "%s: %d: wrn: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0 == 1)
#define RPTINF(fmt...) do {if(RPT_INF <= rpt_lvl) {fprintf(stderr, "%s: %d: inf: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0 == 1)
#define RPTDBG(fmt...) do {if(RPT_DBG <= rpt_lvl) {fprintf(stderr, "%s: %d: dbg: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0 == 1)
#else
#define RPTERR(fmt, ...) do {if(RPT_ERR <= rpt_lvl) {fprintf(stderr, "%s: %d: err: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);}} while(0 == 1)
#define RPTWRN(fmt, ...) do {if(RPT_WRN <= rpt_lvl) {fprintf(stderr, "%s: %d: wrn: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);}} while(0 == 1)
#define RPTINF(fmt, ...) do {if(RPT_INF <= rpt_lvl) {fprintf(stderr, "%s: %d: inf: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);}} while(0 == 1)
#define RPTDBG(fmt, ...) do {if(RPT_DBG <= rpt_lvl) {fprintf(stderr, "%s: %d: dbg: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);}} while(0 == 1)
#endif

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
        {0x80, 0x81, 0, TS_TYPE_ECM},
        {0x82, 0x8F, 0, TS_TYPE_EMM},
        {0x90, 0xFE, 0, TS_TYPE_USR},
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
        {0xF0, TS_TYPE_EMM}, /* "EMM" */
        {0xF1, TS_TYPE_ECM}, /* "ECM" */
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
#if 0
static int dump(uint8_t *buf, int len);
#endif

/*@only@*/
/*@null@*/
struct ts_obj *ts_create(/*@null@*/ void *mp)
{
        struct ts_obj *obj;

        if(!mp) {
                RPTERR("bad memory pool");
                return NULL;
        }

        obj = (struct ts_obj *)malloc(sizeof(struct ts_obj));
        if(!obj) {
                RPTERR("malloc ts_obj failed");
                return NULL;
        }

        obj->mp = mp;
        memset(&(obj->cfg), 0, sizeof(struct ts_cfg)); /* do nothing */

        /* prepare for ts_init() */
        obj->pid0 = NULL; /* no pid list now */
        obj->prog0 = NULL; /* no prog list now */
        obj->tabl0 = NULL; /* no tabl list now */
        obj->ca0 = NULL; /* no ca list now */
        init(obj);

        return obj;
}

int ts_destroy(/*@only@*/ /*@null@*/ struct ts_obj *obj)
{
        if(!obj) {
                RPTERR("bad obj");
                return -1;
        }

        init(obj); /* free all list */
        free(obj);
        return 0;
}

int ts_ioctl(struct ts_obj *obj, int cmd, void *arg)
{
        if(!obj) {
                RPTERR("bad obj");
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
                                RPTERR("bad cfg");
                        }
                        break;
                case TS_TIDY:
                        tidy(obj);
                        break;
                default:
                        RPTERR("bad cmd");
                        break;
        }
        return 0;
}

static void init(/*@out@*/ struct ts_obj *obj)
{
        struct ts_pid *pid;
        struct ts_prog *prog;
        struct ts_tabl *tabl;
        struct ts_ca *ca;

        /* clear the pid list */
        while(NULL != (pid = (struct ts_pid *)zlst_pop((zhead_t *)&(obj->pid0)))) {
                free_pid(obj->mp, pid);
        }
        obj->pid0 = NULL;

        /* clear the prog list */
        while(NULL != (prog = (struct ts_prog *)zlst_pop((zhead_t *)&(obj->prog0)))) {
                free_prog(obj->mp, prog);
        }
        obj->prog0 = NULL;

        /* clear the table list */
        while(NULL != (tabl = (struct ts_tabl *)zlst_pop((zhead_t *)&(obj->tabl0)))) {
                free_tabl(obj->mp, tabl);
        }
        obj->tabl0 = NULL;

        /* clear the ca list */
        while(NULL != (ca = (struct ts_ca *)zlst_pop((zhead_t *)&(obj->ca0)))) {
                buddy_free(obj->mp, ca);
        }
        obj->ca0 = NULL;

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
        obj->has_scrambling = 0;
        obj->has_CAT = 0;

        memset(&(obj->err), 0, sizeof(struct ts_err)); /* no error */
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
                new_pid.cnt_es = 0;
                new_pid.lcnt_es = 0;
                new_pid.is_CC_sync = 0;
                (void)update_pid_list(obj, &new_pid);
                RPTINF("add pat pid: 0x%04X", (unsigned int)(new_pid.PID));
        }

        /* prog list */
        for(prog = obj->prog0; prog; prog = (struct ts_prog *)(((struct znode *)prog)->next)) {
                RPTINF("tidy prog: %d", (int)(prog->program_number));
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
                new_pid.cnt_es = 0;
                new_pid.lcnt_es = 0;
                new_pid.is_CC_sync = 0;
                (void)update_pid_list(obj, &new_pid);
                RPTINF("add pmt pid: 0x%04X", (unsigned int)(new_pid.PID));

                /* add PCR pid */
                new_pid.PID = prog->PCR_PID;
                new_pid.type = ((0x1FFF != new_pid.PID) ? TS_TYPE_PCR : TS_TYPE_NULP);
                new_pid.prog = prog;
                new_pid.elem = NULL;
                new_pid.cnt = 0;
                new_pid.lcnt = 0;
                new_pid.cnt_es = 0;
                new_pid.lcnt_es = 0;
                new_pid.is_CC_sync = 0;
                (void)update_pid_list(obj, &new_pid);
                RPTINF("add pcr pid: 0x%04X", (unsigned int)(new_pid.PID));

                /* elem list */
                for(elem = prog->elem0; elem; elem = (struct ts_elem *)(((struct znode *)elem)->next)) {
                        RPTINF("tidy elem: 0x%04X", (unsigned int)(elem->PID));
                        elem->PTS = STC_BASE_OVF;
                        elem->DTS = STC_BASE_OVF;
                        elem->STC = STC_OVF;
                        elem->is_pes_align = 0;

                        /* add elem pid */
                        new_pid.PID = elem->PID;
                        new_pid.type = elem->type;
                        new_pid.prog = prog;
                        new_pid.elem = elem;
                        new_pid.cnt = 0;
                        new_pid.lcnt = 0;
                        new_pid.cnt_es = 0;
                        new_pid.lcnt_es = 0;
                        new_pid.is_CC_sync = 0;
                        (void)update_pid_list(obj, &new_pid);
                        RPTINF("add elem pid: 0x%04X", (unsigned int)(new_pid.PID));
                }
        }

        /* tabl list */
        for(tabl = obj->tabl0; tabl; tabl = (struct ts_tabl *)(((struct znode *)tabl)->next)) {
                RPTINF("tidy tabl: 0x%02X", (unsigned int)(tabl->table_id));
                tabl->STC = STC_OVF;
        }

        /* pid list */
        for(pid = obj->pid0; pid; pid = (struct ts_pid *)(((struct znode *)pid)->next)) {
                RPTINF("tidy pid: 0x%02X", (unsigned int)(pid->PID));
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
                pid->cnt_es = 0;
                pid->lcnt_es = 0;
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
        while(NULL != (pkt = (struct ts_pkt *)zlst_pop((zhead_t *)&(pid->pkt0)))) {
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
        while(NULL != (sect = (struct ts_sect *)zlst_pop((zhead_t *)&(tabl->sect0)))) {
                free_sect(mp, sect);
        }

        buddy_free(mp, tabl);
        return;
}

static void free_prog(void *mp, struct ts_prog *prog)
{
        struct ts_elem *elem;
        struct ts_sect *sect;
        struct ts_ca *ca;

        /* clear the elem list */
        while(NULL != (elem = (struct ts_elem *)zlst_pop((zhead_t *)&(prog->elem0)))) {

                if(elem->es_info) {
                        buddy_free(mp, elem->es_info);
                        elem->es_info_len = 0;
                }
                while(NULL != (ca = (struct ts_ca *)zlst_pop((zhead_t *)&(elem->ca0)))) {
                        buddy_free(mp, ca);
                }
                buddy_free(mp, elem);
        }

        /* clear the sect list */
        while(NULL != (sect = (struct ts_sect *)zlst_pop((zhead_t *)&(prog->tabl.sect0)))) {
                free_sect(mp, sect);
        }

        if(prog->program_info) {
                buddy_free(mp, prog->program_info);
                prog->program_info_len = 0;
        }
        while(NULL != (ca = (struct ts_ca *)zlst_pop((zhead_t *)&(prog->ca0)))) {
                buddy_free(mp, ca);
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
                RPTERR("ts_parse_tsh: bad obj");
                return -1;
        }
        ipt = &(obj->ipt);

        /* TS[] */
        if(!(ipt->has_ts)) {
                RPTERR("ts_parse_tsh: no ts packet");
                return -1;
        }
        obj->cur = ipt->TS;
        obj->tail = obj->cur + TS_PKT_SIZE;

        /* packet count and ADDR */
        obj->cnt++;
        obj->ADDR = (ipt->has_addr) ? (ipt->ADDR) : (obj->ADDR + TS_PKT_SIZE);
#if 0
        RPTINF("packet %lld @ %lld:", obj->cnt, obj->ADDR);
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
                err->has_level1_error++;
                obj->has_err++;
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
        if(tsh->transport_error_indicator) {
                err->Transport_error = 1;
                err->has_level2_error++;
                obj->has_err++;
        }

        dat = *(obj->cur)++;
        tsh->PID <<= 8;
        tsh->PID |= dat;

        dat = *(obj->cur)++;
        tsh->transport_scrambling_control = (dat & (BIT(7) | BIT(6))) >> 6;
        tsh->adaption_field_control = (dat & (BIT(5) | BIT(4))) >> 4;;
        tsh->continuity_counter = dat & 0x0F;

        if(0 != tsh->transport_scrambling_control) {
                obj->has_scrambling = 1;
        }

        if(0x00 == tsh->adaption_field_control) {
                err->adaption_field_control_error = 1;
                err->has_other_error++;
                obj->has_err++;
        }

        if((BIT(1) & tsh->adaption_field_control) && obj->cfg.need_af) {
                ts_parse_af(obj);
        }

        if(BIT(0) & tsh->adaption_field_control) {
                /* data_byte, PSI or PES */
        }

        obj->PID = tsh->PID; /* record into obj struct */
#if 0
        RPTDBG("search 0x%04X in pid_list", obj->PID);
#endif
        obj->pid = (struct ts_pid *)zlst_search((zhead_t *)&(obj->pid0), (int)(obj->PID));
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
                new_pid->cnt_es = 0;
                new_pid->lcnt_es = 0;
                new_pid->is_CC_sync = 0;

                obj->pid = update_pid_list(obj, new_pid);
                if(!(obj->pid)) {
                        RPTERR("update_pid_list failed");
                }
        }

        pid = obj->pid; /* maybe NULL */

        /* calc STC and CTS, should be as early as possible */
        if(obj->cfg.need_timestamp) {
                if(ipt->has_mts) {
                        int64_t dCTS = ts_timestamp_diff(ipt->MTS, obj->lCTS, (int64_t)MTS_OVF);

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
                RPTERR("bad obj");
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
        RPTDBG("search 0x00 in table_list");
        tabl = (struct ts_tabl *)zlst_search((zhead_t *)&(obj->tabl0), 0x00);
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

                RPTINF("all PAT section parsed");
                obj->state = STATE_NEXT_PMT;
        }

        if(is_all_prog_parsed(obj)) {
                RPTINF("no PMT section");
                obj->is_pat_pmt_parsed = 1;
                obj->state = STATE_NEXT_PKT;
        }

        return 0;
}

static int state_next_pmt(struct ts_obj *obj)
{
        struct ts_tsh *tsh = &(obj->tsh);
        struct ts_pid *pid;

        RPTDBG("search 0x%04X in pid_list", (unsigned int)(tsh->PID));
        pid = (struct ts_pid *)zlst_search((zhead_t *)&(obj->pid0), (int)(tsh->PID));
        if((!pid) || !IS_TYPE(TS_TYPE_PMT, pid->type)) {
                return -1; /* not PMT */
        }

        /* section parse has done in ts_parse_tsh()! */
        if(is_all_prog_parsed(obj)) {
                RPTINF("all PMT section parsed");
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
                        if(0x1FFF == tsh->PID) {
                                /* adaption_field_control is 01, but: */
                                dCC = 0;
                        }

                        pid->CC += dCC;
                        pid->CC &= 0x0F; /* 4-bit */
                        lost  = (int)tsh->continuity_counter;
                        lost -= (int)pid->CC;
                        if(lost < 0) {
                                lost += 16;
                        }

                        obj->CC_wait = (int)(pid->CC);
                        obj->CC_find = (int)(tsh->continuity_counter);
                        obj->CC_lost = lost;
                }
                else {
                        pid->is_CC_sync = 1;

                        obj->CC_wait = (int)(pid->CC);
                        obj->CC_find = (int)(tsh->continuity_counter);
                        obj->CC_lost = 0;
                }
                pid->CC = tsh->continuity_counter; /* update CC */
                if(obj->CC_lost) {
                        err->Continuity_count_error = 1;
                        err->has_level1_error++;
                        obj->has_err++;
                }
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

                        /* PCR_repetition (PCR packet arrive time interval) */
                        if(STC_OVF != prog->PCRb) {
                                obj->PCR_repetition = ts_timestamp_diff(obj->STC, prog->PCRb, STC_OVF);
                                if((prog->is_STC_sync) &&
                                   !(0 < obj->PCR_repetition && obj->PCR_repetition <= 40 * STC_MS)) {
                                        /* !(0 < interval < +40ms) */
                                        err->PCR_repetition_error = 1;
                                        err->has_level2_error++;
                                        obj->has_err++;
                                }
                        }
                        else {
                                obj->PCR_repetition = 0;
                        }

                        /* PCR_continuity (PCR value interval) */
                        if(STC_OVF != prog->PCRb) {
                                obj->PCR_continuity = ts_timestamp_diff(obj->PCR, prog->PCRb, STC_OVF);
                                if((prog->is_STC_sync) && !(af->discontinuity_indicator) &&
                                   !(0 < obj->PCR_continuity && obj->PCR_continuity <= 100 * STC_MS)) {
                                        /* !(0 < continuity < +100ms) */
                                        err->PCR_discontinuity_indicator_error = 1;
                                        err->has_level2_error++;
                                        obj->has_err++;
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
                                err->has_level2_error++;
                                obj->has_err++;
                        }
                }
                else {
                        err->wild_pcr_packet = 1;
                        err->has_other_error++;
                        obj->has_err++;
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

                        /* for bad PCR value */
                        if(1 == af->discontinuity_indicator ||
                           1 == err->PCR_discontinuity_indicator_error) {
                                /* maybe stream repeated */
                                prog->PCRa = STC_OVF;
                                prog->is_STC_sync = 0;
                        }

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
                                                        pid_item->lcnt_es = 0;
                                                        pid_item->cnt_es = 0;
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
                                pid_item->lcnt_es = pid_item->cnt_es;
                                pid_item->cnt_es = 0;
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

                        /* CAT_error */
                        if((obj->has_scrambling) && !(obj->has_CAT)) {
                                obj->has_scrambling = 0;
                                err->CAT_error |= ERR_2_6_0;
                                err->has_level2_error++;
                                obj->has_err++;
                        }
                }
        }

        /* PES head & ES data */
        if(obj->cfg.need_pes && elem && (0 == tsh->transport_scrambling_control)) {
                if(IS_TYPE(TS_TYPE_AUD, pid->type) || IS_TYPE(TS_TYPE_VID, pid->type)) {
                        ts_parse_pesh(obj);
                }

                if(obj->has_pts) {
                        /* PTS */
                        if(STC_OVF != elem->STC) {
                                obj->PTS_repetition = ts_timestamp_diff(obj->STC, elem->STC, STC_OVF);
                        }
                        else {
                                obj->PTS_repetition = 0;
                        }
                        if(STC_BASE_OVF != elem->PTS) {
                                obj->PTS_continuity = ts_timestamp_diff(obj->PTS, elem->PTS, STC_BASE_OVF);
                        }
                        else {
                                obj->PTS_continuity = 0;
                        }
                        if(STC_BASE_OVF != obj->STC_base) {
                                obj->PTS_minus_STC = ts_timestamp_diff(obj->PTS, obj->STC_base, STC_BASE_OVF);
                        }
                        else {
                                obj->PTS_minus_STC = 0;
                        }
                        elem->PTS = obj->PTS;
                        elem->STC = obj->STC;

                        /* PTS_error */
                        if(!(0 <= obj->PTS_repetition && obj->PTS_repetition <= 700 * STC_MS)) {
                                /* !(0 < interval <= +700ms) */
                                err->PTS_error = 1;
                                err->has_level2_error++;
                                obj->has_err++;
                        }

                        /* DTS, if no DTS, DTS = PTS */
                        if(STC_BASE_OVF != elem->DTS) {
                                obj->DTS_continuity = ts_timestamp_diff(obj->DTS, elem->DTS, STC_BASE_OVF);
                        }
                        else {
                                obj->DTS_continuity = 0;
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
        uint8_t *tail;

        obj->AF = obj->cur;
        dat = *(obj->cur)++;
        af->adaption_field_length = dat;
        obj->AF_len = (int)(af->adaption_field_length) + 1; /* add length itself */
        if(0x00 == af->adaption_field_length) {
                return 0;
        }

        tail = obj->cur + af->adaption_field_length; /* point to the data after AF */

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

                for(i = 0; i < (int)(af->transport_private_data_length); i++) {
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
        struct ts_err *err = &(obj->err);

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

                        if((int)(pid->section_length) > pid->payload_total) {
                                /* multi-packets section, make pkt list */
                                struct ts_pkt *new_pkt = (struct ts_pkt *)buddy_malloc(obj->mp, sizeof(struct ts_pkt));
                                if(!new_pkt) {
                                        RPTERR("malloc for pkt node failed");
                                        return -1;
                                }
                                memcpy(new_pkt->pkt, obj->ipt.TS, TS_PKT_SIZE);
                                new_pkt->payload_unit_start_indicator = 1;
                                new_pkt->payload_size = (obj->tail - obj->cur);
                                zlst_push((zhead_t *)&(pid->pkt0), new_pkt);
                        }
                        else {
                                /* single packet section, for efficienc: directly make section without pkt list */
                                struct ts_sect *new_sect = (struct ts_sect *)buddy_malloc(obj->mp, sizeof(struct ts_sect));
                                if(!new_sect) {
                                        RPTERR("malloc section node failed");
                                        goto ts2sect_free_pkt_list;
                                }

                                new_sect->section = (uint8_t *)buddy_malloc(obj->mp, 3 + pid->section_length);
                                if(!new_sect->section) {
                                        RPTERR("malloc data buffer of section node failed");
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
                        RPTDBG("section async, ignore this packet");
                        return -1;
                }
        }
        else { /* (pid->pkt0) */
                /* next packet of this section */
                struct ts_pkt *new_pkt = (struct ts_pkt *)buddy_malloc(obj->mp, sizeof(struct ts_pkt));
                if(!new_pkt) {
                        RPTERR("malloc for packet node failed");
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
                zlst_push((zhead_t *)&(pid->pkt0), new_pkt);

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
                                        err->normal_section_length_error = 1;
                                        err->has_other_error++;
                                        obj->has_err++;
                                        goto ts2sect_free_pkt_list;
                                }
                        }
                        else { /* !(section_syntax_indicator) */
                                if(pid->section_length > PRIVATE_SECTION_LENGTH_MAX) {
                                        err->private_section_length_error = 1;
                                        err->has_other_error++;
                                        obj->has_err++;
                                        goto ts2sect_free_pkt_list;
                                }
                        }
                        RPTINF("table_id: 0x%02X, length: 3 + %d", (unsigned int)(pid->table_id), (int)(pid->section_length));
                }

                if(pid->payload_total >= (int)(3 + pid->section_length) || pid->has_new_sech) {
                        /* has one section */
                        struct ts_sect *new_sect;
                        int left_length;

                        new_sect = (struct ts_sect *)buddy_malloc(obj->mp, sizeof(struct ts_sect));
                        if(!new_sect) {
                                RPTERR("malloc section node failed");
                                goto ts2sect_free_pkt_list;
                        }

                        new_sect->section = (uint8_t *)buddy_malloc(obj->mp, 3 + pid->section_length);
                        if(!new_sect->section) {
                                RPTERR("malloc data buffer of section node failed");
                                goto ts2sect_free_pkt_list;
                        }

                        p = new_sect->section;
                        left_length = 3 + (int)(pid->section_length);

#ifdef DEBUG_SECTION_FRAGMENT
                        fprintf(stderr, "(%02X %4d) 3+%d ", pid->table_id, pid->payload_total, pid->section_length);
#endif
                        RPTINF("pkt list -> ts_sect and parse, table: 0x%02X", (unsigned int)(pid->table_id));
                        while(NULL != (pkt = (struct ts_pkt *)zlst_shift((zhead_t *)&(pid->pkt0)))) {
                                if(pkt->payload_unit_start_indicator && p != new_sect->section) {
                                        /* use start_indicator instead of section_length to determine section end */
                                        left_length = (int)(pkt->pkt[4]); /* pointer_field is just left length */
                                }

                                if(pkt->payload_size < left_length) {
                                        /* part of big section */
                                        memcpy(p, pkt->pkt + TS_PKT_SIZE - pkt->payload_size, (size_t)(pkt->payload_size));
                                        p += pkt->payload_size;
                                        left_length -= pkt->payload_size;
                                        buddy_free(obj->mp, pkt);
#ifdef DEBUG_SECTION_FRAGMENT
                                        fprintf(stderr, "- %3d = %4d ", pkt->payload_size, left_length);
#endif
                                }
                                else { /* (pkt->payload_size >= left_length) */
                                        /* last packet of the section */
                                        memcpy(p, pkt->pkt + TS_PKT_SIZE - pkt->payload_size, (size_t)(left_length));
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
                                                //dump(obj->cur, pkt->payload_size);
#endif
                                                pid->payload_total = pkt->payload_size;
                                                pid->has_new_sech = 0;
                                                zlst_push((zhead_t *)&(pid->pkt0), pkt);
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
        while(NULL != (pkt = (struct ts_pkt *)zlst_pop((zhead_t *)&(pid->pkt0)))) {
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
        struct znode **psect0;

        /* get section head info */
        p = new_sect->section;
        new_sect->table_id = *p++;
        new_sect->section_syntax_indicator = (*p & BIT(7)) >> 7;
        new_sect->private_indicator = (*p & BIT(6)) >> 6;
        new_sect->section_length   = *p++ & 0x0F;
        new_sect->section_length <<= 8;
        new_sect->section_length  |= *p++;
        if(1 == new_sect->section_syntax_indicator) {
                const struct table_id_table *table_id_table;

                /* normal section syntax */
                new_sect->table_id_extension   = *p++;
                new_sect->table_id_extension <<= 8;
                new_sect->table_id_extension  |= *p++;
                new_sect->version_number = (*p & 0x3E) >> 1;
                new_sect->current_next_indicator = *p++ & BIT(0);
                new_sect->section_number = *p++;
                new_sect->last_section_number = *p++;

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

        RPTINF("table: 0x%02X; len: %4d; sect: %d/%d",
            (unsigned int)(new_sect->table_id),
            (int)(new_sect->section_length),
            (int)(new_sect->section_number),
            (int)(new_sect->last_section_number));

        /* check errors */
        if(0x0000 == pid->PID && 0x00 != new_sect->table_id) {
                err->PAT_error |= ERR_1_3_1; /* PAT_error(table_id error) */
                err->has_level1_error++;
                obj->has_err++;
                goto release_sect;
        }
        if(0x0001 == pid->PID && 0x01 != new_sect->table_id) {
                err->CAT_error |= ERR_2_6_1; /* CAT_error(table_id error) */
                err->has_level2_error++;
                obj->has_err++;
                goto release_sect;
        }
        if(0x00 == new_sect->table_id && 0x0000 != pid->PID) {
                err->pat_pid_error = 1;
                err->has_other_error++;
                obj->has_err++;
                goto release_sect;
        }
        if(0x01 == new_sect->table_id && 0x0001 != pid->PID) {
                err->cat_pid_error = 1;
                err->has_other_error++;
                obj->has_err++;
                goto release_sect;
        }
        if(0x02 == new_sect->table_id && !IS_TYPE(TS_TYPE_PMT, pid->type)) {
                err->pmt_pid_error = 1;
                err->has_other_error++;
                obj->has_err++;
                goto release_sect;
        }
        if(0x42 == new_sect->table_id && 0x0011 != pid->PID) {
                err->sdt_pid_error = 1;
                err->has_other_error++;
                obj->has_err++;
                goto release_sect;
        }

        /* check CRC */
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
                        err->has_level2_error++;
                        obj->has_err++;
                        goto release_sect;
                }
                new_sect->CRC_32 = obj->CRC_32;
        }

        /* locate "tabl" and "psect0" */
        if(0x02 == new_sect->table_id) {
                /* is PMT section */
                if(!(pid->prog)) {
                        RPTWRN("PMT without corresponding program, ignore");
                        goto release_sect;
                }
                if(0x00 != new_sect->section_number ||
                   0x00 != new_sect->last_section_number) {
                        err->pmt_section_number_error = 1;
                        err->has_other_error++;
                        obj->has_err++;
                        goto release_sect;
                }
                tabl = &(pid->prog->tabl);

                if(0xFF == tabl->version_number) {
                        /* first PMT of this prog */
                        tabl->version_number = new_sect->version_number;
                        tabl->last_section_number = new_sect->last_section_number;
                }
        }
        else {
                /* not PMT section */
                RPTDBG("search 0x%02X in table_list", (unsigned int)(new_sect->table_id));
                tabl = (struct ts_tabl *)zlst_search((zhead_t *)&(obj->tabl0),
                                                     (int)(new_sect->table_id));
                if(!tabl) {
                        tabl = (struct ts_tabl *)buddy_malloc(obj->mp, sizeof(struct ts_tabl));
                        if(!tabl) {
                                RPTERR("malloc ts_tabl node failed");
                                goto release_sect;
                        }

                        tabl->sect0 = NULL;
                        tabl->table_id = new_sect->table_id;
                        tabl->version_number = new_sect->version_number;
                        tabl->last_section_number = new_sect->last_section_number;
                        tabl->STC = STC_OVF;

                        RPTDBG("insert 0x%02X in table_list", (unsigned int)(tabl->table_id));
                        if(0 != zlst_insert((zhead_t *)&(obj->tabl0), tabl,
                                            (int)(tabl->table_id))) {
                                free_tabl(obj->mp, tabl);
                                goto release_sect;
                        }
                }
        }
        psect0 = (struct znode **)&(tabl->sect0);

        /* sect_interval */
        if(STC_OVF != tabl->STC &&
           STC_OVF != obj->STC) {
                obj->sect_interval = ts_timestamp_diff(obj->STC, tabl->STC, STC_OVF);
        }
        else {
                obj->sect_interval = 0;
        }
        tabl->STC = obj->STC;

#if 0
        /* FIXME:
         *      PAT or PMT version changed is dangerous!
         *      use section_crc32_error to inform application than be INITed now.
         */
        /* new table version? */
        if(new_sect->version_number != tabl->version_number) {
                struct ts_sect *sect_node;

                /* clear psect0 and update table parameter */
                RPTDBG("version_number(%d -> %d), free old sections",
                    (int)(tabl->version_number), (int)(new_sect->version_number));
                tabl->version_number = new_sect->version_number;
                tabl->last_section_number = new_sect->last_section_number;
                while(NULL != (sect_node = (struct ts_sect *)zlst_pop((zhead_t *)psect0))) {
                        free_sect(obj->mp, sect_node);
                };
        }
#endif

        /* locate sect pointer */
        RPTDBG("search %d/%d in sect_list",
               (int)(new_sect->section_number), (int)(new_sect->last_section_number));
        obj->sect = (struct ts_sect *)zlst_search((zhead_t *)psect0,
                                                  (int)(new_sect->section_number));
        if(NULL == obj->sect) {
                if(0x42 == new_sect->table_id && !(obj->is_pat_pmt_parsed)) {
                        /* got SDT before PMT will lost service info, so ignore this SDT */
                        goto release_sect;
                }
                RPTDBG("insert %d/%d in sect_list",
                       (int)(new_sect->section_number), (int)(new_sect->last_section_number));
                if(0 != zlst_insert((zhead_t *)psect0, new_sect,
                                    (int)(new_sect->section_number))) {
                        goto release_sect;
                }
                obj->sect = new_sect; /* has section */
        }
        else {
                RPTINF("has section %02X/%02X(table %02X) already",
                    (unsigned int)(obj->sect->section_number),
                    (unsigned int)(obj->sect->last_section_number),
                    (unsigned int)(obj->sect->table_id));
                if(obj->sect->CRC_32 != new_sect->CRC_32) {
                        int mask;

                        switch(new_sect->table_id) {
                                case 0x00: mask = ERR_4_0_0; break;
                                case 0x01: mask = ERR_4_0_1; break;
                                case 0x02: mask = ERR_4_0_2; break;
                                default:   mask = 0; break;
                        }
                        err->section_crc32_error |= mask;
                        err->has_other_error += ((mask) ? 1 : 0);
                        obj->has_err += ((mask) ? 1 : 0);
                }
                goto release_sect;
        }
        /* new_sect is in list now, do not free new_sect from here to "return 0"! */

        /* parse */
        switch(new_sect->table_id) {
                case 0x00:
                        ts_parse_secb_pat(obj);
                        break;
                case 0x01:
                        ts_parse_secb_cat(obj);
                        break;
                case 0x02:
                        ts_parse_secb_pmt(obj);
                        break;
                case 0x42:
                        ts_parse_secb_sdt(obj);
                        break;
                default:
                        RPTDBG("meet table(0x%02X), ignore", (unsigned int)(new_sect->table_id));
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
        if(!(0 <= obj->sect_interval && obj->sect_interval <= 500 * STC_MS)) {
                err->PAT_error |= ERR_1_3_0;
                err->has_level1_error++;
                obj->has_err++;
        }
        if(0x00 != tsh->transport_scrambling_control) {
                err->PAT_error |= ERR_1_3_2;
                err->has_level1_error++;
                obj->has_err++;
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
                        RPTERR("malloc prog node failed");
                        return -1;
                }
                prog->elem0 = NULL;
                prog->tabl.sect0 = NULL;
                prog->program_info_len = 0;
                prog->program_info = NULL;
                prog->ca0 = NULL;
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
                                err->nit_pid_error = 1;
                                err->has_other_error++;
                                obj->has_err++;
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
                        prog->ca0 = NULL;

                        /* SDT info */
                        prog->service_name_len = 0;
                        prog->service_name = NULL;
                        prog->service_provider_len = 0;
                        prog->service_provider = NULL;

                        /* elementary stream list */
                        prog->elem0 = NULL;

                        /* PMT table */
                        prog->is_parsed = 0;
                        prog->tabl.sect0 = NULL;
                        prog->tabl.table_id = 0x02;
                        prog->tabl.version_number = 0xFF; /* never reached version */
                        prog->tabl.last_section_number = 0; /* no use */
                        prog->tabl.STC = STC_OVF;

                        /* for STC calc */
                        prog->ADDa = 0;
                        prog->PCRa = STC_OVF;
                        prog->ADDb = 0;
                        prog->PCRb = STC_OVF;
                        prog->is_STC_sync = 0;

                        RPTDBG("insert 0x%04X in prog_list", (unsigned int)(prog->program_number));
                        if(0 != zlst_insert((zhead_t *)&(obj->prog0), prog,
                                            (int)(prog->program_number))) {
                                free_prog(obj->mp, prog);
                                return -1;
                        }
                }

                new_pid->cnt = 0;
                new_pid->lcnt = 0;
                new_pid->cnt_es = 0;
                new_pid->lcnt_es = 0;
                new_pid->prog = prog;
                new_pid->elem = NULL;
                new_pid->CC = 0;
                new_pid->is_CC_sync = 0;
                (void)update_pid_list(obj, new_pid); /* NIT or PMT PID */
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
        struct ts_err *err = &(obj->err);
        struct ts_ca *ca;

        while(cur < crc) {
                uint8_t tag;
                uint8_t len;
                uint8_t *pt;

                tag = *cur++;
                len = *cur++;
                pt = cur;
                if((0xFF == tag) || (0 == len)) {
                        err->descriptor_error = 1;
                        err->has_other_error++;
                        obj->has_err++;
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
                        new_pid->cnt_es = 0;
                        new_pid->lcnt_es = 0;
                        new_pid->prog = NULL;
                        new_pid->elem = NULL;
                        new_pid->type = TS_TYPE_EMM;
                        new_pid->CC = 0;
                        new_pid->is_CC_sync = 0;
                        RPTINF("add EMM_PID(0x%04X)", (unsigned int)CA_PID);
                        (void)update_pid_list(obj, new_pid);

                        /* ca node */
                        ca = (struct ts_ca *)buddy_malloc(obj->mp, sizeof(struct ts_ca));
                        if(!ca) {
                                RPTERR("malloc ca node failed");
                                return -1;
                        }

                        /* init ca here */
                        ca->CA_system_ID = CA_system_ID;
                        ca->CA_PID = CA_PID;

                        /* push ca */
                        RPTINF("push 0x%04X in ca_list", (unsigned int)(ca->CA_PID));
                        zlst_push((zhead_t *)&(obj->ca0), ca);

                        /* for CAT error(ERR_2_6_0) */
                        obj->has_CAT = 1;
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
        uint8_t *next; /* point to the data after descriptors */

        /* PMT_error */
        if(!(0 <= obj->sect_interval && obj->sect_interval <= 500 * STC_MS)) {
                err->PMT_error |= ERR_1_5_0;
                err->has_level1_error++;
                obj->has_err++;
        }
        if(0x00 != tsh->transport_scrambling_control) {
                err->PMT_error |= ERR_1_5_1;
                err->has_level1_error++;
                obj->has_err++;
        }

        /* search prog(table_id_extension in pmt is program_number) */
        RPTDBG("search 0x%04X in prog_list", (unsigned int)(sect->table_id_extension));
        prog = (struct ts_prog *)zlst_search((zhead_t *)&(obj->prog0), (int)(sect->table_id_extension));
        if((!prog) || (prog->is_parsed)) {
                return -1; /* parsed program, ignore */
        }

        /* init prog here */
        prog->is_parsed = 1;
        obj->is_psi_si = 1;

        /* parse each parameter about prog */
        dat = *cur++;
        prog->PCR_PID = dat & 0x1F;
        dat = *cur++;
        prog->PCR_PID <<= 8;
        prog->PCR_PID |= dat;

        dat = *cur++;
        prog->program_info_len = (int)(dat & 0x0F);
        dat = *cur++;
        prog->program_info_len <<= 8;
        prog->program_info_len |= dat;

        if(0 != prog->program_info_len && !(prog->program_info)) {
                if(prog->program_info_len > INFO_LEN_MAX) {
                        err->program_info_length_error = 1;
                        err->has_other_error++;
                        obj->has_err++;
                        return -1;
                }
                else {
                        prog->program_info = (uint8_t *)buddy_malloc(obj->mp, (size_t)(prog->program_info_len));
                        if(!(prog->program_info)) {
                                RPTERR("malloc for prog_info buffer failed");
                                return -1;
                        }
                        memcpy(prog->program_info, cur, (size_t)(prog->program_info_len));
                        /* do not move cur here, program_info will be parsed */
                }
        }

        next = cur + prog->program_info_len;
        while(cur < next) {
                uint8_t tag;
                uint8_t len;
                uint8_t *pt;

                tag = *cur++;
                len = *cur++;
                pt = cur;
                if((0xFF == tag) || (0 == len)) {
                        err->descriptor_error = 1;
                        err->has_other_error++;
                        obj->has_err++;
                        return -1;
                }
                if(0x09 == tag) { /* CA_descriptor in program_info */
                        uint16_t CA_system_ID;
                        uint16_t CA_PID;
                        struct ts_ca *ca;

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

                        /* ECM PID */
                        new_pid->PID = CA_PID;
                        new_pid->cnt = 0;
                        new_pid->lcnt = 0;
                        new_pid->cnt_es = 0;
                        new_pid->lcnt_es = 0;
                        new_pid->prog = prog;
                        new_pid->elem = NULL;
                        new_pid->type = TS_TYPE_ECM;
                        new_pid->CC = 0;
                        new_pid->is_CC_sync = 0;
                        RPTINF("add ECM_PID(0x%04X)", (unsigned int)(CA_PID));
                        (void)update_pid_list(obj, new_pid);

                        /* ca node */
                        ca = (struct ts_ca *)buddy_malloc(obj->mp, sizeof(struct ts_ca));
                        if(!ca) {
                                RPTERR("malloc ca node failed");
                                return -1;
                        }

                        /* init ca here */
                        ca->CA_system_ID = CA_system_ID;
                        ca->CA_PID = CA_PID;

                        /* push ca */
                        RPTINF("push 0x%04X in ca_list", (unsigned int)(ca->CA_PID));
                        zlst_push((zhead_t *)&(prog->ca0), ca);
                }
                cur += len;
        }

        /* add PCR PID */
        new_pid->PID = prog->PCR_PID;
        new_pid->cnt = 0;
        new_pid->lcnt = 0;
        new_pid->cnt_es = 0;
        new_pid->lcnt_es = 0;
        new_pid->prog = prog;
        new_pid->elem = NULL;
        new_pid->type = ((0x1FFF != new_pid->PID) ? TS_TYPE_PCR : TS_TYPE_NULP);
        new_pid->CC = 0;
        new_pid->is_CC_sync = 1;
        (void)update_pid_list(obj, new_pid); /* PCR_PID */

        while(cur < crc) {
                struct ts_elem *elem;

                elem = (struct ts_elem *)buddy_malloc(obj->mp, sizeof(struct ts_elem));
                if(!elem) {
                        RPTERR("malloc elem node failed");
                        return -1;
                }

                /* init elem here */
                elem->es_info = NULL;
                elem->es_info_len = 0;
                elem->ca0 = NULL;
                elem->PTS = STC_BASE_OVF;
                elem->DTS = STC_BASE_OVF;
                elem->STC = STC_OVF;
                elem->is_pes_align = 0;

                /* parse each parameter */
                dat = *cur++;
                elem->stream_type = dat;
                elem->type = elem_type(dat)->type;

                dat = *cur++;
                elem->PID = dat & 0x1F;
                dat = *cur++;
                elem->PID <<= 8;
                elem->PID |= dat;
                elem->type |= ((elem->PID == prog->PCR_PID) ? TS_TMSK_PCR : 0);

                dat = *cur++;
                elem->es_info_len = (int)(dat & 0x0F);
                dat = *cur++;
                elem->es_info_len <<= 8;
                elem->es_info_len |= dat;

                if(0 != elem->es_info_len) {
                        if(elem->es_info_len >= INFO_LEN_MAX) {
                                err->es_info_length_error = 1;
                                err->has_other_error++;
                                obj->has_err++;
                                return -1;
                        }
                        else {
                                elem->es_info = (uint8_t *)buddy_malloc(obj->mp, (size_t)(elem->es_info_len));
                                if(!(elem->es_info)) {
                                        RPTERR("malloc for es_info buffer failed");
                                        return -1;
                                }
                                memcpy(elem->es_info, cur, (size_t)(elem->es_info_len));
                                /* do not move cur here, es_info will be parsed */
                        }
                }

                /* push elem */
                RPTDBG("push 0x%04X in elem_list", (unsigned int)(elem->PID));
                zlst_push((zhead_t *)&(prog->elem0), elem);

                /* add elementary PID */
                new_pid->PID = elem->PID;
                new_pid->cnt = 0;
                new_pid->lcnt = 0;
                new_pid->cnt_es = 0;
                new_pid->lcnt_es = 0;
                new_pid->prog = prog;
                new_pid->elem = elem;
                new_pid->type = elem->type;
                new_pid->CC = 0;
                new_pid->is_CC_sync = 0;
                (void)update_pid_list(obj, new_pid); /* elementary_PID */

                next = cur + elem->es_info_len;
                while(cur < next) {
                        uint8_t tag;
                        uint8_t len;
                        uint8_t *pt;

                        tag = *cur++;
                        len = *cur++;
                        pt = cur;
                        if((0xFF == tag) || (0 == len)) {
                                err->descriptor_error = 1;
                                err->has_other_error++;
                                obj->has_err++;
                                return -1;
                        }
                        if(0x09 == tag) { /* CA_descriptor in es_info */
                                uint16_t CA_system_ID;
                                uint16_t CA_PID;
                                struct ts_ca *ca;

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

                                /* ECM PID */
                                new_pid->PID = CA_PID;
                                new_pid->cnt = 0;
                                new_pid->lcnt = 0;
                                new_pid->cnt_es = 0;
                                new_pid->lcnt_es = 0;
                                new_pid->prog = prog;
                                new_pid->elem = elem;
                                new_pid->type = TS_TYPE_ECM;
                                new_pid->CC = 0;
                                new_pid->is_CC_sync = 0;
                                RPTINF("add ECM_PID(0x%04X)", (unsigned int)(CA_PID));
                                (void)update_pid_list(obj, new_pid);

                                /* ca node */
                                ca = (struct ts_ca *)buddy_malloc(obj->mp, sizeof(struct ts_ca));
                                if(!ca) {
                                        RPTERR("malloc ca node failed");
                                        return -1;
                                }

                                /* init ca here */
                                ca->CA_system_ID = CA_system_ID;
                                ca->CA_PID = CA_PID;

                                /* push ca */
                                RPTINF("push 0x%04X in ca_list", (unsigned int)(ca->CA_PID));
                                zlst_push((zhead_t *)&(elem->ca0), ca);
                        }
                        cur += len;
                }
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
        struct ts_err *err = &(obj->err);

        /* in SDT, table_id_extension is transport_stream_id */
        if(obj->has_got_transport_stream_id &&
           0x42 == sect->table_id && /* actual transport stream */
           sect->table_id_extension != obj->transport_stream_id) {
                err->table_id_extension_error = 1;
                err->has_other_error++;
                obj->has_err++;
                return -1; /* bad SDT table, ignore */
        }

        dat = *cur++;
        original_network_id = dat;

        dat = *cur++;
        original_network_id <<= 8;
        original_network_id |= dat;

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
                RPTDBG("search service_id(0x%04X) in prog_list", (unsigned int)service_id);
                prog = (struct ts_prog *)zlst_search((zhead_t *)&(obj->prog0), (int)service_id);

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
                                err->descriptor_error = 1;
                                err->has_other_error++;
                                obj->has_err++;
                                return -1;
                        }
                        if(0x48 == tag && prog) {
                                /* service_descriptor */
                                pt++; /* ignore type */

                                prog->service_provider_len = (int)(*pt++);
                                if(0 != prog->service_provider_len) {
                                        if(prog->service_provider) {
                                                buddy_free(obj->mp, prog->service_provider);
                                        }
                                        prog->service_provider = (uint8_t *)buddy_malloc(obj->mp, (size_t)(1 + prog->service_provider_len));
                                        if(!(prog->service_provider)) {
                                                RPTERR("malloc for service_provider buffer failed");
                                                return -1;
                                        }
                                        memcpy(prog->service_provider, pt, (size_t)(prog->service_provider_len));
                                        prog->service_provider[prog->service_provider_len] = (uint8_t)'\0';
                                }

                                pt += prog->service_provider_len; /* pass provider */

                                prog->service_name_len = (int)(*pt++);
                                if(0 != prog->service_name_len) {
                                        if(prog->service_name) {
                                                buddy_free(obj->mp, prog->service_name);
                                        }
                                        prog->service_name = (uint8_t *)buddy_malloc(obj->mp, (size_t)(1 + prog->service_name_len));
                                        if(!(prog->service_name)) {
                                                RPTERR("malloc for service_name buffer failed");
                                                return -1;
                                        }
                                        memcpy(prog->service_name, pt, (size_t)(prog->service_name_len));
                                        prog->service_name[prog->service_name_len] = (uint8_t)'\0';
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
        struct ts_err *err = &(obj->err);
        uint8_t dat;

        /* for some bad stream */
        if(tsh->PID < 0x0020) {
                err->pes_pid_error = 1;
                err->has_other_error++;
                obj->has_err++;
                return -1;
        }
        if(!elem) {
                err->pes_elem_error = 1;
                err->has_other_error++;
                obj->has_err++;
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
                        err->pes_start_code_error = 1;
                        err->has_other_error++;
                        obj->has_err++;
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
                RPTERR("PES_packet_length = %d(0x%X)",
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
                        pid->cnt_es += obj->ES_len;
                }
                else {
                        /* ignore these ES data */
                }
        }
        else {
                obj->ES_len = obj->tail - obj->cur;
                obj->ES = obj->cur;
                pid->cnt_es += obj->ES_len;
        }

        return 0;
}

static int ts_parse_pesh_switch(struct ts_obj *obj)
{
        struct ts_pesh *pesh = &(obj->pesh);
        struct ts_err *err = &(obj->err);

        switch(pesh->stream_id) {
                case 0xBE: /* padding_stream */
                        if((int)(pesh->PES_packet_length) > (obj->tail - obj->cur)) {
                                err->pes_packet_length_error = 1;
                                err->has_other_error++;
                                obj->has_err++;
                                return -1;
                        }
                        /* subsequent pesh->PES_packet_length data is padding_byte, pass */
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
        struct ts_err *err = &(obj->err);
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
        if((int)(pesh->PES_header_data_length) > (obj->tail - obj->cur)) {
                err->pes_header_length_error = 1;
                err->has_other_error++;
                obj->has_err++;
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
                err->pts_dts_flags_error = 1;
                err->has_other_error++;
                obj->has_err++;
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
                        int i;

                        for(i = 0; i < 16; i++) {
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

        pid = (struct ts_pid *)zlst_search((zhead_t *)&(obj->pid0), (int)(new_pid->PID));
        if(pid) {
                /* is in pid_list already, just update information */
                pid->PID = new_pid->PID;
                pid->prog = new_pid->prog;
                pid->elem = new_pid->elem;
                pid->type = new_pid->type;
                pid->cnt = new_pid->cnt;
                pid->lcnt = new_pid->lcnt;
                pid->cnt_es = new_pid->cnt_es;
                pid->lcnt_es = new_pid->lcnt_es;
                pid->CC = new_pid->CC;
                pid->is_CC_sync = new_pid->is_CC_sync;
        }
        else {
                pid = (struct ts_pid *)buddy_malloc(obj->mp, sizeof(struct ts_pid));
                if(!pid) {
                        RPTERR("malloc pid node failed");
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
                pid->cnt_es = new_pid->cnt_es;
                pid->lcnt_es = new_pid->lcnt_es;

                RPTDBG("insert 0x%04X in pid_list", (unsigned int)(pid->PID));
                if(0 != zlst_insert((zhead_t *)&(obj->pid0), pid,
                                    (int)(pid->PID))) {
                        free_pid(obj->mp, pid);
                        return NULL;
                }
        }
        return pid;
}

static int is_all_prog_parsed(struct ts_obj *obj)
{
        struct znode *znode_p; /* znode of program list */

        for(znode_p = (struct znode *)(obj->prog0); znode_p; znode_p = znode_p->next) {
                struct ts_prog *prog = (struct ts_prog *)znode_p;
                struct ts_tabl *tabl = &(prog->tabl);

                if(0xFF == tabl->version_number) {
                        /* did not parse this PMT, wait it */
                        return 0;
                }

                /* note:
                 *      each PMT PID has only one section,
                 *      (0xFF != version_number) means we parsed the only section,
                 *      so we do not need 'for' loop here.
                 */
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

#if 0
static int dump(uint8_t *buf, int len)
{
        uint8_t *p = buf;
        int i;

        for(i = 0; i < len; i++) {
                fprintf(stderr, "%02X ", (unsigned int)(*p++));
        }
        fprintf(stderr, "\n");

        return 0;
}
#endif

static uint32_t dvbpsi_crc32_table[256] =
{
        0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
        0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
        0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
        0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
        0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
        0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
        0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
        0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
        0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
        0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
        0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
        0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
        0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
        0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
        0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
        0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
        0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
        0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
        0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
        0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
        0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
        0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
        0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
        0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
        0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
        0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
        0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
        0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
        0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
        0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
        0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
        0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

uint32_t ts_crc(void *buf, size_t size, int mode)
{
        uint8_t *p = (uint8_t *)buf;
        uint8_t *tail = p + size;
        uint32_t crc = 0xFFFFFFFF;

        if(32 == mode) {
                while(p < tail) {
                        crc = (crc << 8) ^ dvbpsi_crc32_table[(crc >> 24) ^ *p++];
                }
        }
        return crc;
}

#ifdef S_SPLINT_S /* FIXME */
#define timestamp_assert(expr, fmt...) do { \
        if(!(expr)) { \
                fprintf(stderr, "%s: %d: !(%s): ", __FILE__, __LINE__, #expr); \
                fprintf(stderr, fmt); \
                fprintf(stderr, "\n"); \
                return -1; \
        } \
} while(0)
#else
#define timestamp_assert(expr, fmt, ...) do { \
        if(!(expr)) { \
                fprintf(stderr, "%s: %d: !(%s): " fmt "\n", \
                        __FILE__, __LINE__, #expr, ##__VA_ARGS__); \
                return -1; \
        } \
} while(0)
#endif

int64_t ts_timestamp_add(int64_t t0, int64_t td, int64_t ovf)
{
        int64_t t1; /* t0 + td */
#if 1
        int64_t hovf = ovf >> 1; /* half overflow */

        timestamp_assert(0 < ovf, "0 < %" PRId64, ovf);
        timestamp_assert((hovf << 1) == ovf, "%" PRId64 " is not even", ovf);
        timestamp_assert(0 <= t0 && t0 < ovf, "0 <= %" PRId64 " < %" PRId64, t0, ovf);
        timestamp_assert(-hovf <= td && td < +hovf, "%" PRId64 " <= %" PRId64 " < %" PRId64, -hovf, td, +hovf);
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
        timestamp_assert(0 < ovf, "0 < %" PRId64, ovf);
        timestamp_assert((hovf << 1) == ovf, "%" PRId64 " is not even", ovf);
        timestamp_assert(0 <= t0 && t0 < ovf, "0 <= %" PRId64 " < %" PRId64, t0, ovf);
        timestamp_assert(0 <= t1 && t1 < ovf, "0 <= %" PRId64 " < %" PRId64, t1, ovf);
#endif

        td = t1 - t0; /* minus */
        td += ((td >=   0) ? 0 : ovf); /* special: get the distance from t0 to t1 */
        td -= ((td <  hovf) ? 0 : ovf); /* special: (distance < hovf) means t1 is latter or bigger */

        return td; /* [-hovf, +hovf) */
}
