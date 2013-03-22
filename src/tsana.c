/* vim: set tabstop=8 shiftwidth=8:
 * name: tsana.c
 * funx: analyse character of ts stream
 * 2009-00-00, ZHOU Cheng, init
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* for isatty() */
#include <string.h> /* for strcmp(), etc */
#include <time.h> /* for localtime(), etc */
#include <stdint.h> /* for uint?_t, etc */

#include "version.h"
#include "common.h"
#include "if.h"
#include "buddy.h" /* for BUDDY_ORDER_MAX */
#include "ts.h" /* has "list.h" already */
#include "UTF_GB.h"

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

#define PKT_BBUF                        (256) /* 188 or 204 */
#define PKT_TBUF                        (PKT_BBUF * 3 + 10)

#define ANY_PID                         (0x2000) /* any PID of [0x0000,0x1FFF] */
#define ANY_TABLE                       (0xFF) /* any table_id of [0x00,0xFE] */
#define ANY_PROG                        (0x0000) /* any prog of [0x0001,0xFFFF] */

#define TYPE_ANY                        (0) /* any PID type */
#define TYPE_VIDEO                      (1) /* video PID */
#define TYPE_AUDIO                      (2) /* audio PID */

#define STC_US                          (27) /* 27 clk means 1(us) */
#define STC_MS                          (27 * 1000) /* uint: do NOT use 1e3  */

#define MP_ORDER_DEFAULT (20) /* default memory pool size: (1 << MP_ORDER_DEFAULT) */

struct aim {
        int bg;
        int cts;
        int stc;
        int pcr;
        int pts;
        int tsh;
        int ts;
        int mts;
        int af;
        int pesh;
        int pes;
        int es;
        int sec;
        int si;
        int rate;
        int rats;
        int ratp;
        int err;
        int tcp;
        int allpes;
        int alles;
};

struct tsana_obj {
        intptr_t mp; /* id of buddy memory pool, for list malloc and free */
        int mode;
        int state;
        struct aim aim;

        int is_outpsi; /* output txt psi packet to stdout */
        int is_prepsi; /* get psi information from file first */
        int is_dump; /* output packet directly */
        uint64_t aim_start; /* ignore some packets fisrt, default: 0(no ignore) */
        uint64_t aim_count; /* stop after analyse some packets, default: 0(no stop) */
        uint16_t aim_pid;
        uint8_t aim_table;
        uint16_t aim_prog;
        uint16_t aim_type; /* 0: any type; 1: video; 2: audio */
        int64_t aim_interval; /* for rate calc */
        char *color_off;
        char *color_gray;
        char *color_red;
        char *color_green;
        char *color_yellow;
        char *color_blue;
        char *color_purple;
        char *color_cyan;
        char *color_white;

        uint64_t cnt; /* packet analysed */
        char tbuf[PKT_TBUF];
        char tbak[PKT_TBUF];

        struct ts_obj *ts;
};

enum {
        MODE_LST,
        MODE_PSI,
        MODE_ALL, /* depend on struct aim */
        MODE_EXIT
};

enum {
        STATE_PARSE_EACH,
        STATE_PARSE_PSI,
        STATE_EXIT
};

enum {
        GOT_WRONG_PKT,
        GOT_RIGHT_PKT,
        GOT_EOF
};

static struct tsana_obj *obj = NULL;

static void state_parse_psi(struct tsana_obj *obj);
static void state_parse_each(struct tsana_obj *obj);

static struct tsana_obj *create(int argc, char *argv[]);
static int destroy(struct tsana_obj *obj);

static void show_help();
static void show_version();

static int get_one_pkt(struct tsana_obj *obj);
static void output_prog(struct tsana_obj *obj);
static void output_elem(void *PELEM, uint16_t pcr_pid);

static void show_lst(struct tsana_obj *obj);
static void show_psi(struct tsana_obj *obj);

static void show_pkt(struct tsana_obj *obj);
static void show_bg(struct tsana_obj *obj);
static void show_cts(struct tsana_obj *obj);
static void show_stc(struct tsana_obj *obj);
static void show_pcr(struct tsana_obj *obj);
static void show_pts(struct tsana_obj *obj);
static void show_tsh(struct tsana_obj *obj);
static void show_ts(struct tsana_obj *obj);
static void show_mts(struct tsana_obj *obj);
static void show_af(struct tsana_obj *obj);
static void show_pesh(struct tsana_obj *obj);
static void show_pes(struct tsana_obj *obj);
static void show_es(struct tsana_obj *obj);
static void show_sec(struct tsana_obj *obj);
static void show_si(struct tsana_obj *obj);
static void show_rate(struct tsana_obj *obj);
static void show_rats(struct tsana_obj *obj);
static void show_ratp(struct tsana_obj *obj);
static void show_error(struct tsana_obj *obj);
static void show_tcp(struct tsana_obj *obj);

static void all_pes(struct tsana_obj *obj);
static void all_es(struct tsana_obj *obj);

static void table_info_PAT(struct ts_sech *sech, uint8_t *section);
static void table_info_CAT(struct ts_sech *sech, uint8_t *section);
static void table_info_PMT(struct ts_sech *sech, uint8_t *section);
static void table_info_TSDT(struct ts_sech *sech, uint8_t *section);
static void table_info_NIT(struct ts_sech *sech, uint8_t *section);
static void table_info_SDT(struct ts_sech *sech, uint8_t *section);
static void table_info_BAT(struct ts_sech *sech, uint8_t *section);
static void table_info_EIT(struct ts_sech *sech, uint8_t *section);
static void table_info_TDT(struct ts_sech *sech, uint8_t *section);
static void table_info_RST(struct ts_sech *sech, uint8_t *section);
static void table_info_ST(struct ts_sech *sech, uint8_t *section);
static void table_info_TOT(struct ts_sech *sech, uint8_t *section);
static void table_info_DIT(struct ts_sech *sech, uint8_t *section);
static void table_info_SIT(struct ts_sech *sech, uint8_t *section);

static void MJD_UTC(uint8_t *buf);
static void UTC(uint8_t *buf);
static char *running_status(uint8_t status);

static int descriptor(uint8_t **buf);
static int coding_string(uint8_t *p, int len);

void atsc_mh_tcp(uint8_t *ts_pack); /* atsc_mh_tcp.c */

int main(int argc, char *argv[])
{
        int get_rslt;
        struct ts_obj *ts;

        obj = create(argc, argv);
        ts = obj->ts;
        ts->aim_interval = obj->aim_interval;

        while(STATE_EXIT != obj->state && GOT_EOF != (get_rslt = get_one_pkt(obj))) {
                if(GOT_WRONG_PKT == get_rslt) {
                        break;
                }
                if(0 != ts_parse_tsh(obj->ts)) {
                        break;
                }
                if(ts->cnt < obj->aim_start) {
                        continue;
                }

                ts_parse_tsb(obj->ts);
                switch(obj->state) {
                        case STATE_PARSE_PSI:
                                state_parse_psi(obj);
                                break;
                        case STATE_PARSE_EACH:
                                state_parse_each(obj);
                                break;
                        case STATE_EXIT:
                                break;
                        default:
                                fprintf(stderr, "Wrong state(%d)!\n", obj->state);
                                obj->state = STATE_EXIT;
                                break;
                }

                if(obj->is_dump) {
                        show_pkt(obj);
                }
                obj->cnt++;
                if((0 != obj->aim_count) && (obj->cnt >= obj->aim_count)) {
                        break;
                }
        }

        if(!(ts->is_psi_parse_finished) && !(obj->is_dump)) {
                fprintf(stderr, "%sPSI parsing unfinished because of the bad PCR data!%s\n",
                        obj->color_red, obj->color_off);

                ts->is_psi_parse_finished = 1;
                switch(obj->mode) {
                        case MODE_LST:
                                show_lst(obj);
                                break;
                        case MODE_PSI:
                                show_psi(obj);
                                break;
                        default:
                                break;
                }
        }

        destroy(obj);
        exit(EXIT_SUCCESS);
}

static void state_parse_psi(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        if(obj->is_outpsi && ts->is_psi_si) {
                fprintf(stdout, "%s", obj->tbuf);
        }

        if(ts->is_pat_pmt_parsed) {
                obj->state = STATE_PARSE_EACH;
                if(MODE_EXIT == obj->mode) {
                        obj->state = STATE_EXIT;
                }
        }
        return;
}

static void state_parse_each(struct tsana_obj *obj)
{
        int i;
        int has_err;
        int has_report;
        struct ts_obj *ts = obj->ts;
        struct ts_pid *pid = ts->pid;
        struct ts_sech *sech = &(ts->sech);

        /* filter for some mode */
        if(obj->aim.sec         ||
           obj->aim.si          ||
           obj->aim.pcr         ||
           obj->aim.pts         ||
           obj->aim.af          ||
           obj->aim.pesh        ||
           obj->aim.pes         ||
           obj->aim.es          ||
           obj->aim.err) {
                /* filter: PID */
                if(ANY_PID != obj->aim_pid &&
                   ts->PID != obj->aim_pid) {
                        return;
                }

                /* filter: table_id */
                if(ANY_TABLE != obj->aim_table &&
                   sech->table_id != obj->aim_table) {
                        return;
                }

                /* filter: program_number */
                if(ANY_PROG != obj->aim_prog &&
                   (!(pid->prog) || (pid->prog->program_number != obj->aim_prog))) {
                        return;
                }
        }

        /* show_xxx() */
        if(MODE_LST == obj->mode) {
                if(!(obj->is_dump)) {
                        show_lst(obj);
                }
        }
        if(MODE_PSI == obj->mode) {
                if(!(obj->is_dump)) {
                        show_psi(obj);
                }
        }

        /* error for this TS packet? */
        has_err = 0;
        for(i = 0; i < sizeof(struct ts_err); i++) {
                if(*((uint8_t *)&(ts->err) + i)) {
                        has_err = 1;
                }
        }

        /* report for this TS packet? */
        has_report = 0;
        if(obj->aim.pts && ts->pts) {
                has_report = 1;
        }
        if(obj->aim.pcr && ts->pcr) {
                has_report = 1;
        }
        if(obj->aim.ts) {
                has_report = 1;
        }
        if(obj->aim.af && ts->AF_len) {
                has_report = 1;
        }
        if(obj->aim.pesh && (ts->PES_len != ts->ES_len)) {
                has_report = 1;
        }
        if(obj->aim.pes && ts->PES_len) {
                has_report = 1;
        }
        if(obj->aim.es && ts->ES_len) {
                has_report = 1;
        }
        if(obj->aim.sec && ts->has_sect) {
                has_report = 1;
        }
        if(obj->aim.si && ts->has_sect) {
                has_report = 1;
        }
        if(obj->aim.rate && ts->has_rate) {
                has_report = 1;
        }
        if(obj->aim.rats && ts->has_rate) {
                has_report = 1;
        }
        if(obj->aim.ratp && ts->has_rate) {
                has_report = 1;
        }
        if(obj->aim.err && has_err) {
                has_report = 1;
        }
        if(obj->aim.tcp && 0x1FFA == ts->PID) {
                has_report = 1;
        }

        /* report */
        if(obj->aim.bg && has_report) {
                show_bg(obj);
        }
        if(obj->aim.cts && has_report) {
                show_cts(obj);
        }
        if(obj->aim.stc && has_report) {
                show_stc(obj);
        }
        if(obj->aim.pts && has_report) {
                show_pts(obj);
        }
        if(obj->aim.pcr && has_report) {
                show_pcr(obj);
        }
        if(obj->aim.tsh && has_report) {
                show_tsh(obj);
        }
        if(obj->aim.ts && has_report) {
                show_ts(obj);
        }
        if(obj->aim.mts && has_report) {
                show_mts(obj);
        }
        if(obj->aim.af && ts->AF_len) {
                show_af(obj);
        }
        if(obj->aim.pesh && (ts->PES_len != ts->ES_len)) {
                show_pesh(obj);
        }
        if(obj->aim.pes && ts->PES_len) {
                show_pes(obj);
        }
        if(obj->aim.es && ts->ES_len) {
                show_es(obj);
        }
        if(obj->aim.sec && ts->has_sect) {
                show_sec(obj);
        }
        if(obj->aim.si && ts->has_sect) {
                show_si(obj);
        }
        if(obj->aim.rate && ts->has_rate) {
                show_rate(obj);
        }
        if(obj->aim.rats && ts->has_rate) {
                show_rats(obj);
        }
        if(obj->aim.ratp && ts->has_rate) {
                show_ratp(obj);
        }
        if(obj->aim.err && has_err) {
                show_error(obj);
        }
        if(obj->aim.tcp && 0x1FFA == ts->PID) {
                show_tcp(obj);
        }
        if(obj->aim.allpes && ts->PES_len) {
                all_pes(obj);
        }
        if(obj->aim.alles && ts->ES_len) {
                all_es(obj);
        }

        if(has_report) {
                fprintf(stdout, "\n");
        }
        return;
}

static struct tsana_obj *create(int argc, char *argv[])
{
        int i;
        int dat;
        size_t mp_order;
        struct tsana_obj *obj;

        obj = (struct tsana_obj *)malloc(sizeof(struct tsana_obj));
        if(NULL == obj) {
                RPT(RPT_ERR, "malloc failed");
                return NULL;
        }

        mp_order = MP_ORDER_DEFAULT; /* big memory for memory pool */
        obj->mode = MODE_LST;
        obj->state = STATE_PARSE_PSI;
        memset(&(obj->aim), 0, sizeof(struct aim));

        obj->is_outpsi = 0;
        obj->is_prepsi = 0;
        obj->is_dump = 0;
        obj->cnt = 0;
        obj->aim_start = 0;
        obj->aim_count = 0;
        obj->aim_pid = ANY_PID;
        obj->aim_table = ANY_TABLE;
        obj->aim_prog = ANY_PROG;
        obj->aim_type = TYPE_ANY;
        obj->aim_interval = 1000 * STC_MS;
        obj->color_off = "";
        obj->color_gray = "";
        obj->color_red = "";
        obj->color_green = "";
        obj->color_yellow = "";
        obj->color_blue = "";
        obj->color_purple = "";
        obj->color_cyan = "";
        obj->color_white = "";

        for(i = 1; i < argc; i++) {
                if('-' == argv[i][0]) {
                        if(0 == strcmp(argv[i], "-lst")) {
                                obj->mode = MODE_LST;
                        }
                        else if(0 == strcmp(argv[i], "-psi")) {
                                obj->mode = MODE_PSI;
                        }
                        else if(0 == strcmp(argv[i], "-outpsi")) {
                                obj->is_outpsi = 1;
                                obj->mode = MODE_EXIT;
                        }
#if 0
                        else if(0 == strcmp(argv[i], "-prepsi")) {
                                obj->is_prepsi = 1;
                                obj->mode = MODE_ALL;
                        }
#endif
                        else if(0 == strcmp(argv[i], "-dump")) {
                                obj->is_dump = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-bg")) {
                                obj->aim.bg = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-cts")) {
                                obj->aim.cts = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-stc")) {
                                obj->aim.stc = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-pcr")) {
                                obj->aim.pcr = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-pts")) {
                                obj->aim.pts = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-tsh")) {
                                obj->aim.tsh = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-ts")) {
                                obj->aim.ts = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-mts")) {
                                obj->aim.mts = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-af")) {
                                obj->aim.af = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-pesh")) {
                                obj->aim.pesh = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-pes")) {
                                obj->aim.pes = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-es")) {
                                obj->aim.es = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-sec")) {
                                obj->aim.sec = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-si")) {
                                obj->aim.si = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-rate")) {
                                obj->aim.rate = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-rats")) {
                                obj->aim.rats = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-ratp")) {
                                obj->aim.ratp = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-err")) {
                                obj->aim.err = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-tcp")) {
                                obj->aim.tcp = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-c") ||
                                0 == strcmp(argv[i], "-color")) {
#ifdef PLATFORM_mingw
                                fprintf(stderr, "do not support color in MinGW\n");
#else
                                if(isatty(fileno(stdout))) {
                                        obj->color_off = NONE;
                                        obj->color_gray = FGRAY;
                                        obj->color_red = FRED;
                                        obj->color_green = FGREEN;
                                        obj->color_yellow = FYELLOW;
                                        obj->color_blue = FBLUE;
                                        obj->color_purple = FPURPLE;
                                        obj->color_cyan = FCYAN;
                                        obj->color_white = FWHITE;
                                }
#endif
                        }
                        else if(0 == strcmp(argv[i], "-start")) {
                                int start;

                                i++;
                                if(i >= argc) {
                                        fprintf(stderr, "no parameter for '-start'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &start);
                                obj->aim_start = start;
                        }
                        else if(0 == strcmp(argv[i], "-count")) {
                                int count;

                                i++;
                                if(i >= argc) {
                                        fprintf(stderr, "no parameter for '-count'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &count);
                                obj->aim_count = count;
                        }
                        else if(0 == strcmp(argv[i], "-pid")) {
                                i++;
                                if(i >= argc) {
                                        fprintf(stderr, "no parameter for '-pid'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &dat);
                                if(0x0000 <= dat && dat <= 0x2000) {
                                        obj->aim_pid = (uint16_t)dat;
                                }
                                else {
                                        fprintf(stderr,
                                                "bad variable for '-pid': 0x%04X, ignore!\n",
                                                dat);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-table")) {
                                i++;
                                if(i >= argc) {
                                        fprintf(stderr, "no parameter for '-table'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &dat);
                                if(0x00 <= dat && dat <= 0xFF) {
                                        obj->aim_table = (uint8_t)dat;
                                }
                                else {
                                        fprintf(stderr,
                                                "bad variable for '-table': 0x%02X, ignore!\n",
                                                dat);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-prog")) {
                                i++;
                                if(i >= argc) {
                                        fprintf(stderr, "no parameter for '-prog'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &dat);
                                if(0x0000 <= dat && dat <= 0xFFFF) {
                                        obj->aim_prog = dat;
                                }
                                else {
                                        fprintf(stderr,
                                                "bad variable for '-prog': %u, ignore!\n",
                                                dat);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-type")) {
                                i++;
                                if(i >= argc) {
                                        fprintf(stderr, "no parameter for '-type'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &dat);
                                if(0 <= dat && dat <= 2) {
                                        obj->aim_type = dat;
                                }
                                else {
                                        fprintf(stderr,
                                                "bad variable for '-type': %u, ignore!\n",
                                                dat);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-iv")) {
                                i++;
                                if(i >= argc) {
                                        fprintf(stderr, "no parameter for '-iv'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%u" , &dat);
                                if(0 <= dat && dat <= 70000) { /* 1ms ~ 70s */
                                        obj->aim_interval = dat * STC_MS;
                                }
                                else {
                                        fprintf(stderr,
                                                "bad variable for '-iv': %u, use 1000ms instead!\n",
                                                dat);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-mp")) {
                                i++;
                                if(i >= argc) {
                                        fprintf(stderr, "no parameter for '-mp'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &dat);
                                if(16 <= dat && dat <= BUDDY_ORDER_MAX) {
                                        mp_order = dat;
                                }
                                else {
                                        fprintf(stderr,
                                                "bad variable for '-mp': %u, use %d instead!\n",
                                                dat, MP_ORDER_DEFAULT);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-allpes")) {
                                obj->aim.allpes = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-alles")) {
                                obj->aim.alles = 1;
                                obj->mode = MODE_ALL;
                        }
                        else if(0 == strcmp(argv[i], "-h") ||
                                0 == strcmp(argv[i], "--help")) {
                                show_help();
                                exit(EXIT_SUCCESS);
                        }
                        else if(0 == strcmp(argv[i], "-v") ||
                                0 == strcmp(argv[i], "--version")) {
                                show_version();
                                exit(EXIT_SUCCESS);
                        }
                        else if(0 == strcmp(argv[i], "-sex")) {
                                fprintf(stderr, "SEX? Try to use a Decoder instead of me!\n");
                                exit(EXIT_FAILURE);
                        }
                        else {
                                fprintf(stderr, "wrong parameter: '%s'\n", argv[i]);
                                exit(EXIT_FAILURE);
                        }
                }
                else {
                        fprintf(stderr, "Wrong parameter: %s\n", argv[i]);
                        exit(EXIT_FAILURE);
                }
        }

        /* create & init buddy module */
        obj->mp = buddy_create(mp_order, 6); /* borrow a big memory from OS */
        if(0 == obj->mp) {
                RPT(RPT_ERR, "malloc memory pool failed");
                exit(EXIT_FAILURE);
        }
        buddy_init(obj->mp); /* now, we can use xx_malloc() */

        /* create & init ts module */
        obj->ts = ts_create(obj->mp);
        ts_init(obj->ts);

        return obj;
}

static int destroy(struct tsana_obj *obj)
{
        if(NULL == obj) {
                return 0;
        }

#if 0
        buddy_status(obj->mp); /* to debug the memory pool */
#endif
        ts_destroy(obj->ts);
#if 0
        buddy_status(obj->mp); /* to debug the memory pool */
#endif
        buddy_destroy(obj->mp); /* return the memory to OS */

        free(obj);

        return 1;
}

static void show_help()
{
        fprintf( stderr,
                 "'tsana' get TS packet from stdin, analyse, then send the result to stdout.\n"
                 "\n"
                 "Usage: tsana [OPTION]...\n"
                 "\n"
                 "Options:\n"
                 " -lst             show PID list information, default option\n"
                 " -psi             show PSI tree information\n"
                 "\n"
#if 0
                 " -outpsi          output PSI packet\n"
                 " -prepsi <file>   get PSI information from <file> first\n"
#endif
                 " -dump            dump cared packet\n"
                 "\n"
                 " -bg              output background information\n"
                 " -cts             \"*cts, CTS, BASE, \"\n"
                 " -stc             \"*stc, STC, BASE, \"\n"
                 " -pcr             \"*pcr, PCR, BASE, EXT, interval(ms), jitter(ns), \"\n"
                 " -pts             \"*pts, PTS, dPTS(ms), PTS-PCR(ms), DTS, dDTS(ms), DTS-PCR(ms), \"\n"
                 " -tsh             \"*tsh, 47, xx, xx, xx, \"\n"
                 " -ts              \"*ts, 47, ..., xx, \"\n"
                 " -mts             \"*mts, 3F4BD, \"\n"
                 " -af              \"*af, xx, ..., xx, \"\n"
                 " -pesh            \"*pesh, xx, ..., xx, \"\n"
                 " -pes             \"*pes, xx, ..., xx, \"\n"
                 " -es              \"*es, xx, ..., xx, \"\n"
                 " -sec             \"*sec, interval(ms), head, body, \"\n"
                 " -rate            \"*rate, interval(ms), PID, rate, ..., PID, rate, \"\n"
                 " -rats            \"*rats, interval(ms), SYS, rate, PSI-SI, rate, 0x1FFF, rate, \"\n"
                 " -ratp            \"*ratp, interval(ms), PSI-SI, rate, PID, rate, ..., PID, rate, \"\n"
                 " -err             \"*err, TR-101-290, datail, \"\n"
                 " -tcp             show TCP(ATSC MH) field information\n"
                 "\n"
                 " -c -color        enable colour effect to help read, default: mono\n"
                 " -start <x>       analyse from packet(x), default: 0, first packet\n"
                 " -count <n>       analyse n-packet then stop, default: 0, no stop\n"
                 " -pid <pid>       set cared PID, default: any PID(0x2000)\n"
                 " -table <id>      set cared table, default: any table(0xFF)\n"
                 " -prog <prog>     set cared prog, default: any program(0x0000)\n"
                 " -type <type>     set cared PID type, default: any type(0)\n"
                 " -iv <iv>         set cared interval(1ms-70,000ms), default: 1000ms\n"
                 " -mp <mp>         set memory pool size order(16-%d), default: %d, means 2^%d bytes\n"
                 "\n"
                 " -allpes          write PES data into different file by PID\n"
                 " -alles           write ES data into different file by PID\n"
#if 0
                 " -prepsi <file>   get PSI information from <file> first\n"
                 " -si              show SI section information of cared <table>\n"
#endif
                 "\n"
                 " -h, --help       display this information\n"
                 " -v, --version    display my version\n"
                 "\n"
                 "Examples:\n"
                 "  \"catts xxx.ts | tsana -c -bg -pcr -pts\" -- report all PCR/PTS/DTS information\n"
                 "\n"
                 "Report bugs to <zhoucheng@tsinghua.org.cn>.\n",
                BUDDY_ORDER_MAX, MP_ORDER_DEFAULT, MP_ORDER_DEFAULT);
        return;
}

static void show_version()
{
        char str[100];

        sprintf(str, "tsana of tstools v%s.%s.%s", VER_MAJOR, VER_MINOR, VER_RELEA);
        puts(str);
        sprintf(str, "Build time: %s %s", __DATE__, __TIME__);
        puts(str);
        puts("");
        puts("Copyright (C) 2009,2010,2011,2012,2013 ZHOU Cheng.");
        puts("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>");
        puts("This is free software; contact author for additional information.");
        puts("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR");
        puts("A PARTICULAR PURPOSE.");
        puts("");
        puts("Written by ZHOU Cheng.");
        return;
}

static int get_one_pkt(struct tsana_obj *obj)
{
        char *tag;
        char *pt = (char *)(obj->tbuf);
        struct ts_obj *ts = obj->ts;
        long long int data;

        if(NULL == fgets(obj->tbuf, PKT_TBUF, stdin)) {
                return GOT_EOF;
        }

        strcpy(obj->tbak, obj->tbuf); /* for dump */

        ts->ts = NULL;
        ts->rs = NULL;
        ts->addr = NULL;
        ts->mts = NULL;
        ts->cts = NULL;

        while(0 == next_tag(&tag, &pt)) {
                if(0 == strcmp(tag, "*ts")) {
                        next_nbyte_hex(ts->TS, &pt, 188);
                        ts->ts = ts->TS;
                }
                else if(0 == strcmp(tag, "*rs")) {
                        next_nbyte_hex(ts->RS, &pt, 16);
                        ts->rs = ts->RS;
                }
                else if(0 == strcmp(tag, "*addr")) {
                        next_nuint_hex(&data, &pt, 1);
                        ts->ADDR = data;
                        ts->addr = &(ts->ADDR);
                }
                else if(0 == strcmp(tag, "*mts")) {
                        next_nuint_hex(&data, &pt, 1);
                        ts->MTS = (int64_t)data;
                        ts->mts = &(ts->MTS);
                }
                else if(0 == strcmp(tag, "*cts")) {
                        next_nuint_hex(&data, &pt, 1);
                        ts->CTS = (int64_t)data;
                        ts->cts = &(ts->CTS);
                }
                else {
                        fprintf(stderr, "wrong tag: \"%s\"\n", tag);
                }
        }
        return GOT_RIGHT_PKT;
}

static void output_prog(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        struct znode *znode;

        fprintf(stdout, "transport_stream_id, %s%5d%s(0x%04X)\n",
                obj->color_yellow, ts->transport_stream_id, obj->color_off,
                ts->transport_stream_id);

        for(znode = (struct znode *)(ts->prog0); znode; znode = znode->next) {
                int i;

                struct ts_prog *prog = (struct ts_prog *)znode;
                fprintf(stdout, "program_number, %s%5d%s(0x%04X), "
                        "PMT_PID, %s0x%04X%s, "
                        "PCR_PID, %s0x%04X%s, ",
                        obj->color_yellow, prog->program_number, obj->color_off,
                        prog->program_number,
                        obj->color_yellow, prog->PMT_PID, obj->color_off,
                        obj->color_yellow, prog->PCR_PID, obj->color_off);

                /* service_provider */
                if(prog->service_provider_len) {
                        fprintf(stdout, "service_provider, %s\"",
                                obj->color_yellow);
                        coding_string(prog->service_provider, prog->service_provider_len);
                        fprintf(stdout, "\"%s(%02X",
                                obj->color_off,
                                prog->service_provider[0]);
                        for(i = 1; i < prog->service_provider_len; i++) {
                                fprintf(stdout, " %02X", prog->service_provider[i]);
                        }
                        fprintf(stdout, "), ");
                }

                /* service_name */
                if(prog->service_name_len) {
                        fprintf(stdout, "service_name, %s\"",
                                obj->color_yellow);
                        coding_string(prog->service_name, prog->service_name_len);
                        fprintf(stdout, "\"%s(%02X",
                                obj->color_off,
                                prog->service_name[0]);
                        for(i = 1; i < prog->service_name_len; i++) {
                                fprintf(stdout, " %02X", prog->service_name[i]);
                        }
                        fprintf(stdout, "), ");
                }

                /* program_info */
                if(prog->program_info_len) {
                        fprintf(stdout, "program_info, %s%02X",
                                obj->color_yellow,
                                prog->program_info[0]);
                        for(i = 1; i < prog->program_info_len; i++) {
                                fprintf(stdout, " %02X", prog->program_info[i]);
                        }
                        fprintf(stdout, "%s, ", obj->color_off);
                }
                fprintf(stdout, "\n");

                /* elementary stream */
                output_elem(&(prog->elem0), prog->PCR_PID);
        }

        obj->state = STATE_EXIT;
        return;
}

static void output_elem(void *PELEM, uint16_t pcr_pid)
{
        uint16_t i;
        struct znode **pelem = (struct znode **)PELEM;
        struct znode *znode;
        struct ts_elem *elem;
        char *color_pid;

        for(znode = *pelem; znode; znode = znode->next) {
                elem = (struct ts_elem *)znode;

                color_pid = (elem->PID == pcr_pid) ? obj->color_red : obj->color_yellow;
                fprintf(stdout, "elementary_PID, %s0x%04X%s, "
                        "stream_type, %s0x%02X%s, ",
                        color_pid, elem->PID, obj->color_off,
                        obj->color_yellow, elem->stream_type, obj->color_off);
                fprintf(stdout, "type, %s%s%s, detail, %s%s%s, ",
                        obj->color_yellow, elem->sdes, obj->color_off,
                        obj->color_yellow, elem->ldes, obj->color_off);
                if(elem->es_info_len) {
                        fprintf(stdout, "ES_info, %s%02X",
                                obj->color_yellow, elem->es_info[0]);
                        for(i = 1; i < elem->es_info_len; i++) {
                                fprintf(stdout, " %02X", elem->es_info[i]);
                        }
                        fprintf(stdout, "%s, ", obj->color_off);
                }
                fprintf(stdout, "\n");
        }
        return;
}

static void show_pkt(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        if(ANY_PID != obj->aim_pid && ts->PID != obj->aim_pid) {
                return;
        }
        fprintf(stdout, "%s", obj->tbak);
}

static void show_lst(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        struct znode *znode;
        struct ts_pid *pid;
        char *color_yellow;
        char *color_off;

        if(!(ts->is_psi_parse_finished)) {
                return;
        }

        for(znode = (struct znode *)(ts->pid0); znode; znode = znode->next) {
                pid = (struct ts_pid *)znode;
                color_yellow = "";
                color_off = "";
                if(NULL != pid->elem) {
                        color_yellow = obj->color_yellow;
                        color_off = obj->color_off;
                }
                fprintf(stdout, "%s0x%04X, %s, %s%s\n",
                        color_yellow,
                        pid->PID,
                        pid->sdes,
                        pid->ldes,
                        color_off);
        }
        obj->state = STATE_EXIT;

        return;
}

static void show_psi(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        if(!(ts->is_psi_parse_finished)) {
                return;
        }

        output_prog(obj);
        return;
}

static void show_bg(struct tsana_obj *obj)
{
        time_t tp;
        struct tm *lt; /* local time */
        char str[16]; /* "12:38:00" */
        struct ts_obj *ts = obj->ts;

        time(&tp);
        lt = localtime(&tp);
        strftime(str, 32, "%H:%M:%S", lt);

        fprintf(stdout,
                "%s*bg%s, %s%s%s, %llu, %s0x%llX%s, %lld, %s0x%04X%s, ",
                obj->color_green, obj->color_off,
                obj->color_yellow, str, obj->color_off, (long long int)ts->CTS,
                obj->color_yellow, ts->ADDR, obj->color_off, ts->ADDR,
                obj->color_yellow, ts->PID, obj->color_off);
        return;
}

static void show_cts(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        if(ts->cts) {
                fprintf(stdout,
                        "%s*cts%s, %13llu, %10llu, ",
                        obj->color_green, obj->color_off,
                        (long long int)ts->CTS, (long long int)ts->CTS_base);
        }
        else {
                fprintf(stdout,
                        "%s*cts%s,              ,           , ",
                        obj->color_green, obj->color_off);
        }
        return;
}

static void show_stc(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        if(ts->stc) {
                fprintf(stdout,
                        "%s*stc%s, %13llu, %10llu, ",
                        obj->color_green, obj->color_off,
                        (long long int)ts->STC, (long long int)ts->STC_base);
        }
        else {
                fprintf(stdout,
                        "%s*stc%s,              ,           , ",
                        obj->color_green, obj->color_off);
        }
        return;
}

static void show_pcr(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        if(ts->pcr) {
                fprintf(stdout, "%s*pcr%s, %13lld, %10lld, %3d, %+7.3f, %+4.0f, ",
                        obj->color_green, obj->color_off,
                        (long long int)ts->PCR,
                        (long long int)ts->PCR_base,
                        ts->PCR_ext,
                        (double)(ts->PCR_interval) / STC_MS,
                        (double)(ts->PCR_jitter) * 1e3 / STC_US);
        }
        else {
                fprintf(stdout, "%s*pcr%s,              ,           ,    ,        ,     , ",
                        obj->color_green, obj->color_off);
        }
        return;
}

static void show_pts(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        if(ts->pts) {
                fprintf(stdout, "%s*pts%s, %10lld, %+8.3f, %+8.3f, ",
                        obj->color_green, obj->color_off,
                        (long long int)ts->PTS,
                        (double)(ts->PTS_interval) / (90), /* ms */
                        (double)(ts->PTS_minus_STC) / (90)); /* ms */

                fprintf(stdout, "%s*dts%s, %10lld, %+8.3f, %+8.3f, ",
                        obj->color_green, obj->color_off,
                        (long long int)ts->DTS,
                        (double)(ts->DTS_interval) / (90), /* ms */
                        (double)(ts->DTS_minus_STC) / (90)); /* ms */
        }
        else {
                fprintf(stdout, "%s*pts%s,           ,         ,         , ",
                        obj->color_green, obj->color_off);
                fprintf(stdout, "%s*dts%s,           ,         ,         , ",
                        obj->color_green, obj->color_off);
        }
        return;
}

static void show_tsh(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        char str[3 * 4 + 3]; /* part of one TS packet */

        fprintf(stdout, "%s*tsh%s, ",
                obj->color_green, obj->color_off);
        b2t(str, ts->ts, 4);
        fprintf(stdout, "%s", str);
        return;
}

static void show_ts(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        char str[3 * 188 + 3]; /* part of one TS packet */

        fprintf(stdout, "%s*ts%s, ",
                obj->color_green, obj->color_off);
        b2t(str, ts->ts, 188);
        fprintf(stdout, "%s", str);
        return;
}

static void show_mts(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        fprintf(stdout, "%s*mts%s, %llX, ",
                obj->color_green, obj->color_off,
                ts->CTS & 0x3FFFFFFF);
        return;
}

static void show_af(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        char str[3 * 188 + 3]; /* part of one TS packet */

        fprintf(stdout, "%s*af%s, ",
                obj->color_green, obj->color_off);
        b2t(str, ts->AF, ts->AF_len);
        fprintf(stdout, "%s", str);
        return;
}

static void show_pesh(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        char str[3 * 188 + 3]; /* part of one TS packet */

        fprintf(stdout, "%s*pesh%s, ",
                obj->color_green, obj->color_off);
        b2t(str, ts->PES, ts->PES_len - ts->ES_len);
        fprintf(stdout, "%s", str);
        return;
}

static void show_pes(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        char str[3 * 188 + 3]; /* part of one TS packet */

        fprintf(stdout, "%s*pes%s, ",
                obj->color_green, obj->color_off);
        b2t(str, ts->PES, ts->PES_len);
        fprintf(stdout, "%s", str);
        return;
}

static void show_es(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        char str[3 * 188 + 3]; /* part of one TS packet */

        fprintf(stdout, "%s*es%s, ",
                obj->color_green, obj->color_off);
        b2t(str, ts->ES, ts->ES_len);
        fprintf(stdout, "%s", str);
        return;
}

static void show_sec(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        char str[3 * 4096 + 3];
        struct ts_sech *sech = &(ts->sech);
        struct ts_pid *pid = ts->pid;

        /* section_interval */
        fprintf(stdout, "%s*sec%s, %+9.3f, ",
                obj->color_green, obj->color_off,
                (double)(pid->sect_interval) / STC_MS);

        /* section_head */
        b2t(str, pid->sect_data, 8);
        fprintf(stdout, "%s", str);

        /* section_body */
        b2t(str, pid->sect_data + 8, sech->section_length + 3 - 8);
        fprintf(stdout, "%s", str);

        return;
}

static void show_si(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        char str[3 * 8 + 3];
        int is_unknown_table_id = 0;
        struct ts_sech *sech = &(ts->sech);
        struct ts_pid *pid = ts->pid;

        /* section_interval */
        fprintf(stdout, "%s*si%s, %+9.3f, ",
                obj->color_green, obj->color_off,
                (double)(pid->sect_interval) / STC_MS);

        /* section_head */
        b2t(str, pid->sect_data, 8);
        fprintf(stdout, "%s", str);

        /* section_body */
        if(     0x00 == sech->table_id) {
                table_info_PAT(sech, pid->sect_data);
        }
        else if(0x01 == sech->table_id) {
                table_info_CAT(sech, pid->sect_data);
        }
        else if(0x02 == sech->table_id) {
                table_info_PMT(sech, pid->sect_data);
        }
        else if(0x03 == sech->table_id) {
                table_info_TSDT(sech, pid->sect_data);
        }
        else if(0x04 <= sech->table_id && sech->table_id <= 0x3F) {
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }
        else if(0x40 == sech->table_id) { /* actual */
                table_info_NIT(sech, pid->sect_data);
        }
        else if(0x41 == sech->table_id) { /* other */
                table_info_NIT(sech, pid->sect_data);
        }
        else if(0x42 == sech->table_id) { /* actual */
                table_info_SDT(sech, pid->sect_data);
        }
        else if(0x43 <= sech->table_id && sech->table_id <= 0x45) {
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }
        else if(0x46 == sech->table_id) { /* other */
                table_info_SDT(sech, pid->sect_data);
        }
        else if(0x47 <= sech->table_id && sech->table_id <= 0x49) {
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }
        else if(0x4A == sech->table_id) {
                table_info_BAT(sech, pid->sect_data);
        }
        else if(0x4B <= sech->table_id && sech->table_id <= 0x4D) {
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }
        else if(0x4E == sech->table_id) { /* actual, P/F */
                table_info_EIT(sech, pid->sect_data);
        }
        else if(0x4F == sech->table_id) { /* other, P/F */
                table_info_EIT(sech, pid->sect_data);
        }
        else if(0x50 <= sech->table_id && sech->table_id <= 0x5F) { /* actual, schedule */
                table_info_EIT(sech, pid->sect_data);
        }
        else if(0x60 <= sech->table_id && sech->table_id <= 0x6F) { /* other, schedule */
                table_info_EIT(sech, pid->sect_data);
        }
        else if(0x70 == sech->table_id) {
                table_info_TDT(sech, pid->sect_data);
        }
        else if(0x71 == sech->table_id) {
                table_info_RST(sech, pid->sect_data);
        }
        else if(0x72 == sech->table_id) {
                table_info_ST(sech, pid->sect_data);
        }
        else if(0x73 == sech->table_id) {
                table_info_TOT(sech, pid->sect_data);
        }
        else if(0x74 <= sech->table_id && sech->table_id <= 0x7D) {
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }
        else if(0x7E == sech->table_id) {
                table_info_DIT(sech, pid->sect_data);
        }
        else if(0x7F == sech->table_id) {
                table_info_SIT(sech, pid->sect_data);
        }
        else if(0x80 <= sech->table_id && sech->table_id <= 0xFE) {
                fprintf(stdout, "user table_id: ");
                is_unknown_table_id = 1;
        }
        else { /* (0xFF == sech->table_id) */
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }

        if(is_unknown_table_id) {
                for(int i = 0; i < sech->section_length + 3; i++) {
                        fprintf(stdout, "%02X ", pid->sect_data[i]);
                }
        }
        return;
}

static void show_rate(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        struct znode *znode;

        fprintf(stdout, "%s*rate%s, %.3f, ",
                obj->color_green, obj->color_off,
                ts->last_interval / 27000.0);
        for(znode = (struct znode *)(ts->pid0); znode; znode = znode->next) {
                struct ts_pid *pid_item = (struct ts_pid *)znode;

                /* filter: user PID only */
                if(pid_item->PID < 0x0020 || 0x1FFF == pid_item->PID) {
                        /* not program */
                        continue;
                }

                /* filter: PID */
                if(ANY_PID != obj->aim_pid && pid_item->PID != obj->aim_pid) {
                        /* not cared PID */
                        continue;
                }

                /* filter: program_number */
                if(ANY_PROG != obj->aim_prog) {
                        if(!(pid_item->prog)) {
                                /* not program */
                                continue;
                        }
                        else if(pid_item->prog->program_number != obj->aim_prog) {
                                /* not cared program */
                                continue;
                        }
                }

                /* filter: type: video or audio */
                if(TYPE_ANY != obj->aim_type) {
                        if(TYPE_VIDEO == obj->aim_type && !(pid_item->is_video)) {
                                /* not video PID */
                                continue;
                        }
                        if(TYPE_AUDIO == obj->aim_type && !(pid_item->is_audio)) {
                                /* not audio PID */
                                continue;
                        }
                }

                fprintf(stdout, "%s0x%04X%s, %9.6f, ",
                        obj->color_yellow, pid_item->PID, obj->color_off,
                        pid_item->lcnt * 188.0 * 8 * 27 / (ts->last_interval));
        }
        return;
}

static void show_rats(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        fprintf(stdout, "%s*rats%s, %.3f, ",
                obj->color_green, obj->color_off,
                ts->last_interval / 27000.0);
        fprintf(stdout, "%ssys%s, %9.6f, %spsi-si%s, %9.6f, %s0x1FFF%s, %9.6f, ",
                obj->color_yellow, obj->color_off, ts->last_sys_cnt * 188.0 * 8 * 27 / (ts->last_interval),
                obj->color_yellow, obj->color_off, ts->last_psi_cnt * 188.0 * 8 * 27 / (ts->last_interval),
                obj->color_yellow, obj->color_off, ts->last_nul_cnt * 188.0 * 8 * 27 / (ts->last_interval));
        return;
}

static void show_ratp(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        struct znode *znode;

        fprintf(stdout, "%s*ratp%s, %.3f, ",
                obj->color_green, obj->color_off,
                ts->last_interval / 27000.0);
        fprintf(stdout, "%spsi-si%s, %9.6f, ",
                obj->color_yellow, obj->color_off, ts->last_psi_cnt * 188.0 * 8 * 27 / (ts->last_interval));

        for(znode = (struct znode *)(ts->pid0); znode; znode = znode->next) {
                struct ts_pid *pid_item = (struct ts_pid *)znode;

                if(pid_item->PID >= 0x0020 && pid_item->PID != pid_item->prog->PMT_PID) {
                        /* not psi/si PID */
                        continue;
                }

                /* without PMT */
                fprintf(stdout, "%s0x%04X%s, %9.6f, ",
                        obj->color_yellow, pid_item->PID, obj->color_off,
                        pid_item->lcnt * 188.0 * 8 * 27 / (ts->last_interval));
        }
        return;
}

static void show_error(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;
        struct ts_err *err = &(ts->err);

        fprintf(stdout, "%s*err%s, ",
                obj->color_green, obj->color_off);

        /* First priority: necessary for de-codability (basic monitoring) */
        if(err->TS_sync_loss) {
                fprintf(stdout, "1.1, TS_sync_loss, ");
                if(err->Sync_byte_error > 10) {
                        fprintf(stdout, "\nToo many continual Sync_byte_error packet, EXIT!\n");
                        exit(EXIT_FAILURE);
                }
                return;
        }
        if(err->Sync_byte_error == 1) {
                fprintf(stdout, "1.2 , Sync_byte, ");
        }
        if(err->PAT_error) {
                if((1<<0) & err->PAT_error) {
                        fprintf(stdout, "1.3a, PAT(section_interval > 0.5s), ");
                }
                if((1<<1) & err->PAT_error) {
                        fprintf(stdout, "1.3b, PAT(table_id != 0x00), ");
                }
                if((1<<2) & err->PAT_error) {
                        fprintf(stdout, "1.3c, PAT(transport_scrambling_field != 0x00), ");
                }
                err->PAT_error = 0;
        }
        if(err->Continuity_count_error) {
                fprintf(stdout, "1.4 , CC(%X-%X=%2u), ",
                        ts->CC_find, ts->CC_wait, ts->CC_lost);
        }
        if(err->PMT_error) {
                if((1<<0) & err->PMT_error) {
                        fprintf(stdout, "1.5a, PMT(section_interval > 0.5s), ");
                }
                if((1<<1) & err->PMT_error) {
                        fprintf(stdout, "1.5b, PMT(transport_scrambling_field != 0x00), ");
                }
                err->PMT_error = 0;
        }
        if(err->PID_error) {
                fprintf(stdout, "1.6 , PID, ");
                err->PID_error = 0;
        }

        /* Second priority: recommended for continuous or periodic monitoring */
        if(err->Transport_error) {
                fprintf(stdout, "2.1 , Transport, ");
                err->Transport_error = 0;
        }
        if(err->CRC_error) {
                fprintf(stdout, "2.2 , CRC(0x%08X! 0x%08X?), ",
                        ts->pid->CRC_32_calc, ts->pid->CRC_32);
                err->CRC_error = 0;
        }
        if(err->PCR_repetition_error) {
                fprintf(stdout, "2.3a, PCR_repetition(%+7.3f ms), ",
                        (double)(ts->PCR_interval) / STC_MS);
                err->PCR_repetition_error = 0;
        }
        if(err->PCR_discontinuity_indicator_error) {
                fprintf(stdout, "2.3b, PCR_discontinuity_indicator(%+7.3f ms), ",
                        (double)(ts->PCR_continuity) / STC_MS);
                err->PCR_discontinuity_indicator_error = 0;
        }
        if(err->PCR_accuracy_error) {
                fprintf(stdout, "2.4 , PCR_accuracy(%+4.0f ns), ",
                        (double)(ts->PCR_jitter) * 1e3 / STC_US);
                err->PCR_accuracy_error = 0;
        }
        if(err->PTS_error) {
                fprintf(stdout, "2.5 , PTS, ");
                err->PTS_error = 0;
        }
        if(err->CAT_error) {
                fprintf(stdout, "2.6 , CAT, ");
                err->CAT_error = 0;
        }

        /* Third priority: application dependant monitoring */
        /* ... */

        return;
}

static void show_tcp(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        fprintf(stdout, "%s*tcp%s, ",
                obj->color_green, obj->color_off);
        atsc_mh_tcp(ts->TS);
        return;
}

static void all_pes(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        if(NULL == ts->pid) {
                fprintf(stderr, "Bad pid point!\n");
                return;
        }

        if(NULL == ts->pid->fd) {
                char name[100];

                sprintf(name, "%04X.pes", ts->pid->PID);
                fprintf(stdout, "open file %s\n", name);
                ts->pid->fd = fopen(name, "wb");
                if(NULL == ts->pid->fd) {
                        RPT(RPT_ERR, "open \"%s\" failed", name);
                        return;
                }
        }

        fwrite(ts->PES, ts->PES_len, 1, ts->pid->fd);
        return;
}

static void all_es(struct tsana_obj *obj)
{
        struct ts_obj *ts = obj->ts;

        if(NULL == ts->pid) {
                fprintf(stderr, "Bad pid point!\n");
                return;
        }

        if(NULL == ts->pid->fd) {
                char name[100];

                sprintf(name, "%04X.es", ts->pid->PID);
                fprintf(stdout, "open file %s\n", name);
                ts->pid->fd = fopen(name, "wb");
                if(NULL == ts->pid->fd) {
                        RPT(RPT_ERR, "open \"%s\" failed", name);
                        return;
                }
        }

        fwrite(ts->ES, ts->ES_len, 1, ts->pid->fd);
        return;
}

static void table_info_PAT(struct ts_sech *psi, uint8_t *section)
{
        uint8_t *p = section + 3;
        int section_length;
        uint16_t transport_stream_id;

        /* section head */
        section_length = psi->section_length;
        transport_stream_id = psi->table_id_extension;
        fprintf(stdout, "transport_stream_id: %5d, ", transport_stream_id);

        /* PAT special */
        p += 5; section_length -= 5;

        /* each program */
        while(section_length > 4) {
                uint8_t data;
                uint16_t program_number;
                uint16_t PID;

                fprintf(stdout, "\n    ");

                data = *p++; section_length--;
                program_number = data;
                data = *p++; section_length--;
                program_number <<= 8;
                program_number |= data;

                data = *p++; section_length--;
                PID = data & 0x1F;
                data = *p++; section_length--;
                PID <<= 8;
                PID |= data;

                if(0 == program_number) {
                        fprintf(stdout, "PID 0x%04X, network", PID);
                }
                else {
                        fprintf(stdout, "PID 0x%04X, program %5d", PID, program_number);
                }
        }

        return;
}

static void table_info_CAT(struct ts_sech *psi, uint8_t *section)
{
        uint8_t *p = section + 3;
        int section_length;
        uint16_t descriptors_loop_length;

        /* section head */
        section_length = psi->section_length;

        /* CAT special */
        p += 5; section_length -= 5;
        descriptors_loop_length = section_length - 4;
        while(descriptors_loop_length > 0) {
                uint8_t len;

                len = descriptor(&p);
                descriptors_loop_length -= len;

                if(0 == len) {
                        fprintf(stdout, "wrong descriptor, ");
                        return;
                }
        }

        return;
}

static void table_info_PMT(struct ts_sech *psi, uint8_t *section)
{
        uint8_t *p = section + 3;
        int section_length;
        uint16_t program_number;
        uint16_t PCR_PID;
        uint16_t program_info_length;

        /* section head */
        section_length = psi->section_length;
        program_number = psi->table_id_extension;
        fprintf(stdout, "program_number: %5d, ", program_number);

        /* PMT special */
        p += 5; section_length -= 5;

        /* PCR_PID */
        PCR_PID = (*p++) & 0x1F; section_length--;
        PCR_PID <<= 8;
        PCR_PID |= *p++; section_length--;
        fprintf(stdout, "PCR_PID: 0x%04X, ", PCR_PID);

        /* program_info */
        program_info_length = (*p++) & 0x0F; section_length--;
        program_info_length <<= 8;
        program_info_length |= *p++; section_length--;
        if(program_info_length) {
                fprintf(stdout, "program_info(%4d): ", program_info_length);
                for(; program_info_length > 0; program_info_length--) {
                        fprintf(stdout, "%02X ", *p++); section_length--;
                }
        }

        /* each ES */
        while(section_length > 4) {
                uint8_t stream_type;
                uint16_t elementary_PID;
                uint16_t ES_info_length;

                fprintf(stdout, "\n    ");

                stream_type = *p++; section_length--;
                fprintf(stdout, "stream_type: 0x%02X, ", stream_type);

                elementary_PID = (*p++) & 0x1F; section_length--;
                elementary_PID <<= 8;
                elementary_PID |= *p++; section_length--;
                fprintf(stdout, "elementary_PID: 0x%04X, ", elementary_PID);

                /* ES_info */
                ES_info_length = (*p++) & 0x0F; section_length--;
                ES_info_length <<= 8;
                ES_info_length |= *p++; section_length--;
                if(ES_info_length) {
                        fprintf(stdout, "ES_info(%4d): ", ES_info_length);
                        for(; ES_info_length > 0; ES_info_length--) {
                                fprintf(stdout, "%02X ", *p++); section_length--;
                        }
                }
        }

        return;
}

static void table_info_TSDT(struct ts_sech *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void table_info_NIT(struct ts_sech *psi, uint8_t *section)
{
        uint8_t *p = section + 3;
        int section_length;
        uint16_t network_id;
        uint16_t network_descriptors_length;
        uint16_t transport_stream_loop_length;

        /* section head */
        section_length = psi->section_length;
        network_id = psi->table_id_extension;
        fprintf(stdout, "network_id: %5d, ", network_id);

        /* SDT special */
        p += 5; section_length -= 5;

        /* network_descriptors */
        network_descriptors_length = (*p++) & 0x0F; section_length--;
        network_descriptors_length <<= 8;
        network_descriptors_length |= *p++; section_length--;
        while(network_descriptors_length > 0) {
                uint8_t len;

                len = descriptor(&p);
                network_descriptors_length -= len;
                section_length -= len;

                if(0 == len) {
                        fprintf(stdout, "wrong descriptor, ");
                        return;
                }
        }

        /* transport_stream_loop */
        transport_stream_loop_length = (*p++) & 0x0F; section_length--;
        transport_stream_loop_length <<= 8;
        transport_stream_loop_length |= *p++; section_length--;
        while(transport_stream_loop_length > 0) {
                uint16_t transport_stream_id;
                uint16_t original_network_id;
                uint16_t transport_descriptors_length;

                fprintf(stdout, "\n    ");

                transport_stream_id = *p++; section_length--;
                transport_stream_id <<= 8;
                transport_stream_id |= *p++; section_length--;
                fprintf(stdout, "transport_stream_id: %5d, ", transport_stream_id);

                original_network_id = *p++; section_length--;
                original_network_id <<= 8;
                original_network_id |= *p++; section_length--;
                fprintf(stdout, "original_network_id: %5d, ", original_network_id);

                /* transport_descriptors */
                transport_descriptors_length = (*p++) & 0x0F; section_length--;
                transport_descriptors_length <<= 8;
                transport_descriptors_length |= *p++; section_length--;

                transport_stream_loop_length -= 6;
                transport_stream_loop_length -= transport_descriptors_length;

                while(transport_descriptors_length > 0) {
                        uint8_t len;

                        len = descriptor(&p);
                        transport_descriptors_length -= len;
                        section_length -= len;

                        if(0 == len) {
                                fprintf(stdout, "wrong descriptor, ");
                                return;
                        }
                }
        }

        return;
}

static void table_info_SDT(struct ts_sech *psi, uint8_t *section)
{
        uint8_t *p = section + 3;
        int section_length;
        uint16_t transport_stream_id;
        uint16_t original_network_id;

        /* section head */
        section_length = psi->section_length;
        transport_stream_id = psi->table_id_extension;
        fprintf(stdout, "transport_stream_id: %5d, ", transport_stream_id);

        /* SDT special */
        p += 5; section_length -= 5;
        original_network_id = *p++; section_length--;
        original_network_id <<= 8;
        original_network_id |= *p++; section_length--;
        fprintf(stdout, "original_network_id: %5d, ", original_network_id);
        p += 1; section_length -= 1;
        /* fprintf(stdout, "section_length(%d), ", section_length); */

        while(section_length > 4) {
                uint8_t data;
                uint16_t service_id;
                uint8_t rstatus;
                uint16_t descriptors_loop_length;

                fprintf(stdout, "\n    ");

                service_id = *p++; section_length--;
                service_id <<= 8;
                service_id |= *p++; section_length--;
                fprintf(stdout, "service_id: %5d, ", service_id);
                p += 1; section_length -= 1;
                data = *p++; section_length--;
                rstatus = (data >> 5) & 0x07;
                fprintf(stdout, "\"%s\", ", running_status(rstatus));
                descriptors_loop_length = 0x0F & data;
                descriptors_loop_length <<= 8;
                descriptors_loop_length |= *p++; section_length--;
                /* fprintf(stdout, "descriptors_loop_length(%d), ", descriptors_loop_length); */

                if(descriptors_loop_length + 4 > section_length) {
                        fprintf(stdout, "wrong section, ");
                        return;
                }
                while(descriptors_loop_length > 0) {
                        uint8_t len;

                        len = descriptor(&p);
                        descriptors_loop_length -= len;
                        section_length -= len;

                        if(0 == len) {
                                fprintf(stdout, "wrong descriptor, ");
                                return;
                        }
                        /* fprintf(stdout, "descriptors_loop_length(%d), ", descriptors_loop_length); */
                }
                /* fprintf(stdout, "section_length(%d), ", section_length); */
        }

        return;
}

static void table_info_BAT(struct ts_sech *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void table_info_EIT(struct ts_sech *psi, uint8_t *section)
{
        uint8_t *p = section + 3;
        int section_length;
        uint16_t service_id;
        uint16_t transport_stream_id;
        uint16_t original_network_id;
        uint8_t segment_last_section_number;
        uint8_t last_table_id;

        /* section head */
        section_length = psi->section_length;
        service_id = psi->table_id_extension;
        fprintf(stdout, "service_id: %5d, ", service_id);

        /* EIT special */
        p += 5; section_length -= 5;
        transport_stream_id = *p++; section_length--;
        transport_stream_id <<= 8;
        transport_stream_id |= *p++; section_length--;
        fprintf(stdout, "transport_stream_id: %5d, ", transport_stream_id);
        original_network_id = *p++; section_length--;
        original_network_id <<= 8;
        original_network_id |= *p++; section_length--;
        fprintf(stdout, "original_network_id: %5d, ", original_network_id);
        segment_last_section_number = *p++; section_length--;
        fprintf(stdout, "segment_last_section_number: %5d, ", segment_last_section_number);
        last_table_id = *p++; section_length--;
        fprintf(stdout, "last_table_id: %5d, ", last_table_id);

        while(section_length > 4) {
                uint8_t data;
                uint16_t event_id;
                uint8_t rstatus;
                uint16_t descriptors_loop_length;

                fprintf(stdout, "\n    ");

                event_id = *p++; section_length--;
                event_id <<= 8;
                event_id |= *p++; section_length--;
                fprintf(stdout, "event_id: %5d, ", event_id);
                MJD_UTC(p); p += 5; section_length -= 5;
                UTC(p); p += 3; section_length -= 3;
                data = *p++; section_length--;
                rstatus = (data >> 5) & 0x07;
                fprintf(stdout, "\"%s\", ", running_status(rstatus));
                descriptors_loop_length = 0x0F & data;
                descriptors_loop_length <<= 8;
                descriptors_loop_length |= *p++; section_length--;

                while(descriptors_loop_length > 0) {
                        uint8_t len;

                        len = descriptor(&p);
                        descriptors_loop_length -= len;
                        section_length -= len;

                        if(0 == len) {
                                fprintf(stdout, "wrong descriptor, ");
                                return;
                        }
                }
        }

        return;
}

static void table_info_TDT(struct ts_sech *psi, uint8_t *section)
{
        uint8_t *p = section + 3;

        MJD_UTC(p); p += 5;

        return;
}

static void table_info_RST(struct ts_sech *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void table_info_ST(struct ts_sech *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void table_info_TOT(struct ts_sech *psi, uint8_t *section)
{
        uint8_t *p = section + 3;
        int descriptors_loop_length;

        MJD_UTC(p); p += 5;

        descriptors_loop_length = (0x0F & *p++);
        descriptors_loop_length <<= 8;
        descriptors_loop_length |= *p++;

        while(descriptors_loop_length > 0) {
                descriptors_loop_length -= descriptor(&p);
        }

        return;
}

static void table_info_DIT(struct ts_sech *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void table_info_SIT(struct ts_sech *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void MJD_UTC(uint8_t *buf)
{
        uint8_t *p = buf;
        int mjd;
        int Y1, M1, K;
        int Y, M, D;

        mjd = *p++;
        mjd <<= 8;
        mjd |= *p++;

        Y1 = (int)((mjd - 15078.2) / 365.25);
        M1 = (int)((mjd - 14956.1 - (int)(Y1 * 365.25)) / 30.6001);
        D = mjd - 14956 - (int)(Y1 * 365.25) - (int)(M1 * 30.6001);
        K = ((14 == M1) || (15 == M1)) ? 1 : 0;
        Y = 1900 + Y1 + K;
        M = M1 - 1 - K * 12;

        fprintf(stdout, "%04d-%02d-%02d_", Y, M, D);

        UTC(p);

        return;
}

static void UTC(uint8_t *buf)
{
        uint8_t *p = buf;
        int H, M, S;

        H = *p++; /* hour */
        M = *p++; /* minute */
        S = *p++; /* second */

        fprintf(stdout, "%02X-%02X-%02X, ", H, M, S);

        return;
}

static char *running_status(uint8_t status)
{
        switch(status) {
                case  0: return "undefined";
                case  1: return "stopped";
                case  2: return "preparing";
                case  3: return "pausing";
                case  4: return "running";
                default: return "reserved running status";
        }
}

static int descriptor(uint8_t **buf)
{
        int i;
        uint8_t *p = *buf;
        uint8_t tag;
        uint8_t len;

        tag = *p++;
        len = *p++;

        fprintf(stdout, "(");
        if(0x40 == tag) { /* network_name_descriptor */
                fprintf(stdout, "\"");
                coding_string(p, len);
                fprintf(stdout, "\"");
        }
        else if(0x48 == tag) { /* service_descriptor */
                int service_provider_name_length;
                int service_name_length;

                fprintf(stdout, "0x%02X, ", *p++);

                fprintf(stdout, "\"");
                service_provider_name_length = *p++;
                coding_string(p, service_provider_name_length);
                p += service_provider_name_length;
                fprintf(stdout, "\", ");

                fprintf(stdout, "\"");
                service_name_length = *p++;
                coding_string(p, service_name_length);
                p += service_name_length;
                fprintf(stdout, "\"");
        }
        else {
                fprintf(stdout, "%02X %02X,", tag, len);
                if(0 != len) {
                        for(i = len; i > 0; i--) {
                                fprintf(stdout, " %02X", *p++);
                        }
                }
        }
        fprintf(stdout, "), ");

        len += 2; /* count tag and len byte */
        *buf += len;

        len = (0xFF == tag) ? 0 : len; /* wrong buffer */
        return len;
}

static int coding_string(uint8_t *p, int len)
{
        uint8_t coding = *p;
        char str[256] = "";

        if(0x01 <= coding && coding <= 0x1F) {
                p++; len--; /* pass first byte */
        }
        switch(coding) {
                case 0x11:
                        utf16_gb((const uint16_t *)p, str, len, BIG_ENDIAN);
                        break;
                default:
                        memcpy(str, p, len);
                        str[len] = '\0';
                        break;
        }
        fprintf(stdout, "%s", str);
        return 0;
}
