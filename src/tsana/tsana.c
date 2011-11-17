/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: tsana.c
 * funx: analyse character of ts stream
 * 2009-00-00, ZHOU Cheng, init
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for strcmp, etc */
#include <time.h> /* for localtime(), etc */
#include <stdint.h> /* for uint?_t, etc */

#include "common.h"
#include "error.h"
#include "if.h"
#include "ts.h" /* has "list.h" already */
#include "UTF_GB.h"

#define PKT_BBUF                        (256) /* 188 or 204 */
#define PKT_TBUF                        (PKT_BBUF * 3 + 10)

#define ANY_PID                         0x2000 /* any PID of [0x0000,0x1FFF] */
#define ANY_TABLE                       0xFF /* any table_id of [0x00,0xFE] */
#define ANY_PROG                        0x0000 /* any prog of [0x0001,0xFFFF] */

#define STC_US                          (27) /* 27 clk means 1(us) */
#define STC_MS                          (27 * 1000) /* uint: do NOT use 1e3  */

typedef struct
{
        int mode;
        int state;

        int is_outpsi; /* output txt psi packet to stdout */
        int is_prepsi; /* get psi information from file first */
        int is_dump; /* output packet directly */
        uint64_t aim_start; /* ignore some packets fisrt, default: 0(no ignore) */
        uint64_t aim_count; /* stop after analyse some packets, default: 0(no stop) */
        uint16_t aim_pid;
        uint8_t aim_table;
        uint16_t aim_prog;
        uint64_t aim_interval; /* for rate calc */
        int is_color;
        char *color_off; /*  */
        char *color_red; /*  */
        char *color_yellow; /*  */

        uint64_t cnt; /* packet analysed */
        char tbuf[PKT_TBUF];
        char tbak[PKT_TBUF];

        int ts_id;
        ts_rslt_t *rslt;
}
obj_t;

enum
{
        MODE_LST,
        MODE_PSI,
        MODE_SEC,
        MODE_SI,
        MODE_TCP,
        MODE_PCR,
        MODE_PTSDTS,
        MODE_SYS_RATE,
        MODE_PSI_RATE,
        MODE_PROG_RATE,
        MODE_RATE,
        MODE_PES,
        MODE_ES,
        MODE_ALLES,
        MODE_ERROR,
        MODE_EXIT
};

enum
{
        STATE_PARSE_EACH,
        STATE_PARSE_PSI,
        STATE_EXIT
};

enum
{
        GOT_WRONG_PKT,
        GOT_RIGHT_PKT,
        GOT_EOF
};

static obj_t *obj = NULL;

static void state_parse_psi(obj_t *obj);
static void state_parse_each(obj_t *obj);

static obj_t *create(int argc, char *argv[]);
static int delete(obj_t *obj);

static void show_help();
static void show_version();

static int get_one_pkt(obj_t *obj);

static void show_pkt(obj_t *obj);
static void show_list(obj_t *obj);
static void show_prog(obj_t *obj);
static void show_track(void *PTRACK, uint16_t pcr_pid);

static void show_sec(obj_t *obj);
static void show_si(obj_t *obj);
static void show_tcp(obj_t *obj);
static void show_pcr(obj_t *obj);
static void show_sys_rate(obj_t *obj);
static void show_psi_rate(obj_t *obj);
static void show_prog_rate(obj_t *obj);
static void show_rate(obj_t *obj);
static void show_ptsdts(obj_t *obj);
static void show_pes(obj_t *obj);
static void show_es(obj_t *obj);
static void all_es(obj_t *obj);
static void show_error(obj_t *obj);

static void print_atp_title(obj_t *obj); /* atp: address_time_PID */
static void print_atp_value(obj_t *obj); /* atp: address_time_PID */

static void table_info_PAT(ts_psi_t *psi, uint8_t *section);
static void table_info_CAT(ts_psi_t *psi, uint8_t *section);
static void table_info_PMT(ts_psi_t *psi, uint8_t *section);
static void table_info_TSDT(ts_psi_t *psi, uint8_t *section);
static void table_info_NIT(ts_psi_t *psi, uint8_t *section);
static void table_info_SDT(ts_psi_t *psi, uint8_t *section);
static void table_info_BAT(ts_psi_t *psi, uint8_t *section);
static void table_info_EIT(ts_psi_t *psi, uint8_t *section);
static void table_info_TDT(ts_psi_t *psi, uint8_t *section);
static void table_info_RST(ts_psi_t *psi, uint8_t *section);
static void table_info_ST(ts_psi_t *psi, uint8_t *section);
static void table_info_TOT(ts_psi_t *psi, uint8_t *section);
static void table_info_DIT(ts_psi_t *psi, uint8_t *section);
static void table_info_SIT(ts_psi_t *psi, uint8_t *section);

static void MJD_UTC(uint8_t *buf);
static void UTC(uint8_t *buf);
static char *running_status(uint8_t status);

static int descriptor(uint8_t **buf);
static int coding_string(uint8_t *p, int len);

void atsc_mh_tcp(uint8_t *ts_pack, int is_color); /* atsc_mh_tcp.c */

int main(int argc, char *argv[])
{
        int get_rslt;
        ts_rslt_t *rslt;

        obj = create(argc, argv);
        rslt = obj->rslt;
        rslt->aim_interval = obj->aim_interval;

        while(STATE_EXIT != obj->state && GOT_EOF != (get_rslt = get_one_pkt(obj)))
        {
                if(GOT_WRONG_PKT == get_rslt)
                {
                        break;
                }
                if(0 != tsParseTS(obj->ts_id))
                {
                        break;
                }
                if(rslt->cnt < obj->aim_start)
                {
                        continue;
                }

                tsParseOther(obj->ts_id);
                switch(obj->state)
                {
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

                if(obj->is_dump)
                {
                        show_pkt(obj);
                }
                obj->cnt++;
                if((0 != obj->aim_count) && (obj->cnt >= obj->aim_count))
                {
                        break;
                }
        }

        if(!(rslt->is_psi_parse_finished) && !(obj->is_dump))
        {
                fprintf(stderr, "%sPSI parsing unfinished because of the bad PCR data!%s\n",
                        obj->color_red, obj->color_off);

                rslt->is_psi_parse_finished = 1;
                switch(obj->mode)
                {
                        case MODE_LST:
                                show_list(obj);
                                break;
                        case MODE_PSI:
                                show_prog(obj);
                                break;
                        default:
                                break;
                }
        }

        delete(obj);
        exit(EXIT_SUCCESS);
}

static void state_parse_psi(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(obj->is_outpsi && rslt->is_psi_si)
        {
                fprintf(stdout, "%s", obj->tbuf);
        }

        if(rslt->is_pat_pmt_parsed)
        {
                switch(obj->mode)
                {
                        case MODE_SEC:
                                print_atp_title(obj);
                                fprintf(stdout, "section_interval, section_head, section_body\n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_SI:
                                print_atp_title(obj);
                                fprintf(stdout, "section_interval, section_head, section_body\n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_PCR:
                                print_atp_title(obj);
                                fprintf(stdout, "PCR, BASE, EXT, interval(ms), jitter(ns), \n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_PTSDTS:
                                print_atp_title(obj);
                                fprintf(stdout, "PTS, PTS_interval(ms), PTS-PCR(ms), DTS, DTS_interval(ms), DTS-PCR(ms), \n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_ERROR:
                                print_atp_title(obj);
                                fprintf(stdout, "TR-101-290, detail, \n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_LST:
                        case MODE_PSI:
                        case MODE_TCP:
                        case MODE_SYS_RATE:
                        case MODE_PSI_RATE:
                        case MODE_PROG_RATE:
                        case MODE_RATE:
                        case MODE_PES:
                        case MODE_ES:
                        case MODE_ALLES:
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_EXIT:
                        default:
                                obj->state = STATE_EXIT;
                                break;
                }
        }
        return;
}

static void state_parse_each(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        ts_psi_t *psi = &(rslt->psi);

        /* filter */
        if(ANY_PID != obj->aim_pid && rslt->PID != obj->aim_pid)
        {
                return;
        }
        if(ANY_TABLE != obj->aim_table && psi->table_id != obj->aim_table)
        {
                return;
        }

        /* show_xxx() */
        switch(obj->mode)
        {
                case MODE_LST:
                        if(!(obj->is_dump))
                        {
                                show_list(obj);
                        }
                        break;
                case MODE_PSI:
                        if(!(obj->is_dump))
                        {
                                show_prog(obj);
                        }
                        break;
                case MODE_SEC:
                        show_sec(obj);
                        break;
                case MODE_SI:
                        show_si(obj);
                        break;
                case MODE_TCP:
                        show_tcp(obj);
                        break;
                case MODE_PCR:
                        show_pcr(obj);
                        break;
                case MODE_SYS_RATE:
                        show_sys_rate(obj);
                        break;
                case MODE_PSI_RATE:
                        show_psi_rate(obj);
                        break;
                case MODE_PROG_RATE:
                        show_prog_rate(obj);
                        break;
                case MODE_RATE:
                        show_rate(obj);
                        break;
                case MODE_PTSDTS:
                        show_ptsdts(obj);
                        break;
                case MODE_PES:
                        show_pes(obj);
                        break;
                case MODE_ES:
                        show_es(obj);
                        break;
                case MODE_ALLES:
                        all_es(obj);
                        break;
                case MODE_ERROR:
                        show_error(obj);
                        break;
                default:
                        fprintf(stderr, "wrong mode(%d)!\n", obj->mode);
                        break;
        }
        return;
}

static obj_t *create(int argc, char *argv[])
{
        int i;
        int dat;
        obj_t *obj;

        obj = (obj_t *)malloc(sizeof(obj_t));
        if(NULL == obj)
        {
                DBG(ERR_MALLOC_FAILED, "\n");
                return NULL;
        }

        obj->mode = MODE_LST;
        obj->state = STATE_PARSE_PSI;

        obj->is_outpsi = 0;
        obj->is_prepsi = 0;
        obj->is_dump = 0;
        obj->cnt = 0;
        obj->aim_start = 0;
        obj->aim_count = 0;
        obj->aim_pid = ANY_PID;
        obj->aim_table = ANY_TABLE;
        obj->aim_prog = ANY_PROG;
        obj->aim_interval = 1000 * STC_MS;
        obj->is_color = 0;
        obj->color_off = "";
        obj->color_red = "";
        obj->color_yellow = "";

        for(i = 1; i < argc; i++)
        {
                if('-' == argv[i][0])
                {
                        if(0 == strcmp(argv[i], "-lst"))
                        {
                                obj->mode = MODE_LST;
                        }
                        else if(0 == strcmp(argv[i], "-psi"))
                        {
                                obj->mode = MODE_PSI;
                        }
                        else if(0 == strcmp(argv[i], "-sec"))
                        {
                                obj->mode = MODE_SEC;
                        }
                        else if(0 == strcmp(argv[i], "-si"))
                        {
                                obj->mode = MODE_SI;
                        }
                        else if(0 == strcmp(argv[i], "-tcp"))
                        {
                                obj->mode = MODE_TCP;
                        }
                        else if(0 == strcmp(argv[i], "-outpsi"))
                        {
                                obj->is_outpsi = 1;
                                obj->mode = MODE_EXIT;
                        }
                        else if(0 == strcmp(argv[i], "-dump"))
                        {
                                obj->is_dump = 1;
                        }
                        else if(0 == strcmp(argv[i], "-prepsi"))
                        {
                                obj->is_prepsi = 1;
                        }
                        else if(0 == strcmp(argv[i], "-color"))
                        {
                                obj->is_color = 1;
                                obj->color_off = NONE;
                                obj->color_red = FRED;
                                obj->color_yellow = FYELLOW;
                        }
                        else if(0 == strcmp(argv[i], "-pcr"))
                        {
                                obj->mode = MODE_PCR;
                        }
                        else if(0 == strcmp(argv[i], "-sys-rate"))
                        {
                                obj->mode = MODE_SYS_RATE;
                        }
                        else if(0 == strcmp(argv[i], "-psi-rate"))
                        {
                                obj->mode = MODE_PSI_RATE;
                        }
                        else if(0 == strcmp(argv[i], "-prog-rate"))
                        {
                                obj->mode = MODE_PROG_RATE;
                        }
                        else if(0 == strcmp(argv[i], "-rate"))
                        {
                                obj->mode = MODE_RATE;
                        }
                        else if(0 == strcmp(argv[i], "-err"))
                        {
                                obj->mode = MODE_ERROR;
                        }
                        else if(0 == strcmp(argv[i], "-start"))
                        {
                                int start;

                                i++;
                                if(i >= argc)
                                {
                                        fprintf(stderr, "no parameter for '-start'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &start);
                                obj->aim_start = start;
                        }
                        else if(0 == strcmp(argv[i], "-count"))
                        {
                                int count;

                                i++;
                                if(i >= argc)
                                {
                                        fprintf(stderr, "no parameter for '-count'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &count);
                                obj->aim_count = count;
                        }
                        else if(0 == strcmp(argv[i], "-pid"))
                        {
                                i++;
                                if(i >= argc)
                                {
                                        fprintf(stderr, "no parameter for '-pid'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &dat);
                                if(0x0000 <= dat && dat <= 0x2000)
                                {
                                        obj->aim_pid = (uint16_t)dat;
                                }
                                else
                                {
                                        fprintf(stderr,
                                                "bad variable for '-pid': 0x%04X, ignore!\n",
                                                dat);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-table"))
                        {
                                i++;
                                if(i >= argc)
                                {
                                        fprintf(stderr, "no parameter for '-table'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &dat);
                                if(0x00 <= dat && dat <= 0xFF)
                                {
                                        obj->aim_table = (uint8_t)dat;
                                }
                                else
                                {
                                        fprintf(stderr,
                                                "bad variable for '-table': 0x%02X, ignore!\n",
                                                dat);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-prog"))
                        {
                                i++;
                                if(i >= argc)
                                {
                                        fprintf(stderr, "no parameter for '-prog'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &dat);
                                if(0x0000 <= dat && dat <= 0xFFFF)
                                {
                                        obj->aim_prog = dat;
                                }
                                else
                                {
                                        fprintf(stderr,
                                                "bad variable for '-prog': %u, ignore!\n",
                                                dat);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-iv"))
                        {
                                i++;
                                if(i >= argc)
                                {
                                        fprintf(stderr, "no parameter for '-interval'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &dat);
                                if(1 <= dat && dat <= 10000) /* 1ms ~ 10s */
                                {
                                        obj->aim_interval = dat * STC_MS;
                                }
                                else
                                {
                                        fprintf(stderr,
                                                "bad variable for '-interval': %u, use 1000ms instead!\n",
                                                dat);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-pes"))
                        {
                                obj->mode = MODE_PES;
                        }
                        else if(0 == strcmp(argv[i], "-es"))
                        {
                                obj->mode = MODE_ES;
                        }
                        else if(0 == strcmp(argv[i], "-alles"))
                        {
                                obj->mode = MODE_ALLES;
                        }
                        else if(0 == strcmp(argv[i], "-pts"))
                        {
                                obj->mode = MODE_PTSDTS;
                        }
                        else if(0 == strcmp(argv[i], "-h") ||
                                0 == strcmp(argv[i], "--help")
                        )
                        {
                                show_help();
                                exit(EXIT_SUCCESS);
                        }
                        else if(0 == strcmp(argv[i], "-v") ||
                                0 == strcmp(argv[i], "--version")
                        )
                        {
                                show_version();
                                exit(EXIT_SUCCESS);
                        }
                        else if(0 == strcmp(argv[i], "-sex"))
                        {
                                fprintf(stderr, "SEX? Try to use a Decoder instead of me!\n");
                                exit(EXIT_FAILURE);
                        }
                        else
                        {
                                fprintf(stderr, "wrong parameter: '%s'\n", argv[i]);
                                exit(EXIT_FAILURE);
                        }
                }
                else
                {
                        fprintf(stderr, "Wrong parameter: %s\n", argv[i]);
                        exit(EXIT_FAILURE);
                }
        }

        obj->ts_id = tsCreate(&(obj->rslt));

        return obj;
}

static int delete(obj_t *obj)
{
        if(NULL == obj)
        {
                return 0;
        }
        else
        {
                tsDelete(obj->ts_id);
                free(obj);

                return 1;
        }
}

static void show_help()
{
        puts("'tsana' get TS packet from stdin, analyse, then send the result to stdout.");
        puts("");
        puts("Usage: tsana [OPTION]...");
        puts("");
        puts("Options:");
        puts(" -lst             show PID list information, default option");
        puts(" -psi             show PSI tree information");
        puts(" -sec             show SI section data of cared <table>");
        puts(" -si              show SI section information of cared <table>");
        puts(" -tcp             show TCP(ATSC M/H) field information");
        puts(" -outpsi          output PSI packet");
        puts(" -pcr             output PCR information of cared <pid>");
        puts(" -sys-rate        output system bit-rate");
        puts(" -psi-rate        output psi/si bit-rate");
        puts(" -prog-rate       output bit-rate of cared <prog>");
        puts(" -rate            output bit-rate of all PID");
        puts(" -pts             output PTS and DTS information of cared <pid>");
        puts(" -pes             output PES data of cared <pid>");
        puts(" -es              output ES data of cared <pid>");
        puts(" -alles           write ES data into different file by PID");
        puts(" -err             output all errors found");
        puts(" -dump            dump cared packet");
        puts("");
        puts(" -color           enable colour effect to help read, default: mono");
        puts(" -start <x>       analyse from packet(x), default: 0, first packet");
        puts(" -count <n>       analyse n-packet then stop, default: 0, no stop");
        puts(" -pid <pid>       set cared PID, default: any PID(0x2000)");
        puts(" -table <id>      set cared table, default: any table(0xFF)");
        puts(" -prog <prog>     set cared prog, default: any program(0x0000)");
        puts(" -iv <iv>         set cared interval(1ms-10,000ms), default: 1000ms");
#if 0
        puts(" -prepsi <file>   get PSI information from <file> first");
#endif
        puts("");
        puts(" -h, --help       display this information");
        puts(" -v, --version    display my version");
        puts("");
        puts("Examples:");
        puts("  \"bincat xxx.ts | tsana -err\" -- report all error information");
        puts("");
        puts("Report bugs to <zhoucheng@tsinghua.org.cn>.");
        return;
}

static void show_version()
{
        puts("tsana 1.0.0");
        puts("");
        puts("Copyright (C) 2009,2010,2011 ZHOU Cheng.");
        puts("This is free software; contact author for additional information.");
        puts("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR");
        puts("A PARTICULAR PURPOSE.");
        puts("");
        puts("Written by ZHOU Cheng.");
        return;
}

static int get_one_pkt(obj_t *obj)
{
        if(NULL == fgets(obj->tbuf, PKT_TBUF, stdin))
        {
                return GOT_EOF;
        }

        strcpy(obj->tbak, obj->tbuf); /* for dump */
        t2b(obj->rslt->pkt, obj->tbuf);
        return GOT_RIGHT_PKT;
}

static void show_pkt(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(ANY_PID != obj->aim_pid && rslt->PID != obj->aim_pid)
        {
                return;
        }
        fprintf(stdout, "%s", obj->tbak);
}

static void show_list(obj_t *obj)
{
        lnode_t *lnode;
        ts_pid_t *pid;
        ts_rslt_t *rslt = obj->rslt;
        char *color_yellow;
        char *color_off;

        if(!(rslt->is_psi_parse_finished))
        {
                return;
        }

        fprintf(stdout, "  PID , abbr, detail\n");
        for(lnode = (lnode_t *)(rslt->pid0); lnode; lnode = lnode->next)
        {
                pid = (ts_pid_t *)lnode;
                color_yellow = "";
                color_off = "";
                if(NULL != pid->track)
                {
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

static void show_prog(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        lnode_t *lnode;

        if(!(rslt->is_psi_parse_finished))
        {
                return;
        }

        fprintf(stdout, "transport_stream_id, %s%5d%s(0x%04X)\n",
                obj->color_yellow, rslt->transport_stream_id, obj->color_off,
                rslt->transport_stream_id);

        for(lnode = (lnode_t *)(rslt->prog0); lnode; lnode = lnode->next)
        {
                int i;

                ts_prog_t *prog = (ts_prog_t *)lnode;
                fprintf(stdout, "program_number, %s%5d%s(0x%04X), "
                        "PMT_PID, %s0x%04X%s, "
                        "PCR_PID, %s0x%04X%s, ",
                        obj->color_yellow, prog->program_number, obj->color_off,
                        prog->program_number,
                        obj->color_yellow, prog->PMT_PID, obj->color_off,
                        obj->color_yellow, prog->PCR_PID, obj->color_off);

                /* service_provider */
                if(prog->service_provider_len)
                {
                        fprintf(stdout, "service_provider, %s\"",
                                obj->color_yellow);
                        coding_string(prog->service_provider, prog->service_provider_len);
                        fprintf(stdout, "\"%s(%02X",
                                obj->color_off,
                                prog->service_provider[0]);
                        for(i = 1; i < prog->service_provider_len; i++)
                        {
                                fprintf(stdout, " %02X", prog->service_provider[i]);
                        }
                        fprintf(stdout, "), ");
                }

                /* service_name */
                if(prog->service_name_len)
                {
                        fprintf(stdout, "service_name, %s\"",
                                obj->color_yellow);
                        coding_string(prog->service_name, prog->service_name_len);
                        fprintf(stdout, "\"%s(%02X",
                                obj->color_off,
                                prog->service_name[0]);
                        for(i = 1; i < prog->service_name_len; i++)
                        {
                                fprintf(stdout, " %02X", prog->service_name[i]);
                        }
                        fprintf(stdout, "), ");
                }

                /* program_info */
                if(prog->program_info_len)
                {
                        fprintf(stdout, "program_info, %s%02X",
                                obj->color_yellow,
                                prog->program_info[0]);
                        for(i = 1; i < prog->program_info_len; i++)
                        {
                                fprintf(stdout, " %02X", prog->program_info[i]);
                        }
                        fprintf(stdout, "%s, ", obj->color_off);
                }
                fprintf(stdout, "\n");

                /* track */
                show_track(&(prog->track0), prog->PCR_PID);
        }

        obj->state = STATE_EXIT;
        return;
}

static void show_track(void *PTRACK, uint16_t pcr_pid)
{
        uint16_t i;
        lnode_t **ptrack = (lnode_t **)PTRACK;
        lnode_t *lnode;
        ts_track_t *track;
        char *color_pid;

        for(lnode = *ptrack; lnode; lnode = lnode->next)
        {
                track = (ts_track_t *)lnode;

                color_pid = (track->PID == pcr_pid) ? obj->color_red : obj->color_yellow;
                fprintf(stdout, "track, %s0x%04X%s, "
                        "stream_type, %s0x%02X%s, ",
                        color_pid, track->PID, obj->color_off,
                        obj->color_yellow, track->stream_type, obj->color_off);
                fprintf(stdout, "type, %s%s%s, detail, %s%s%s, ",
                        obj->color_yellow, track->sdes, obj->color_off,
                        obj->color_yellow, track->ldes, obj->color_off);
                if(track->es_info_len)
                {
                        fprintf(stdout, "ES_info, %s%02X",
                                obj->color_yellow, track->es_info[0]);
                        for(i = 1; i < track->es_info_len; i++)
                        {
                                fprintf(stdout, " %02X", track->es_info[i]);
                        }
                        fprintf(stdout, "%s, ", obj->color_off);
                }
                fprintf(stdout, "\n");
        }
        return;
}

static void show_sec(obj_t *obj)
{
        int i;
        ts_rslt_t *rslt = obj->rslt;
        ts_psi_t *psi = &(rslt->psi);
        ts_pid_t *pid = rslt->pid;

        if(!(rslt->has_section))
        {
                return;
        }

        print_atp_value(obj);

        /* section_interval */
        fprintf(stdout, "%+9.3f, ", (double)(pid->section_interval) / STC_MS);

        /* section_head */
        for(i = 0; i < 7; i++)
        {
                fprintf(stdout, "%02X ", pid->section[i]);
        }
        fprintf(stdout, "%02X, ", pid->section[i++]);

        /* section_body */
        for(; i < psi->section_length + 3; i++)
        {
                fprintf(stdout, "%02X ", pid->section[i]);
        }
        fprintf(stdout, "\n");
        return;
}

static void show_si(obj_t *obj)
{
        int i;
        int is_unknown_table_id = 0;
        ts_rslt_t *rslt = obj->rslt;
        ts_psi_t *psi = &(rslt->psi);
        ts_pid_t *pid = rslt->pid;

        if(!(rslt->has_section))
        {
                return;
        }

        print_atp_value(obj);

        /* section_interval */
        fprintf(stdout, "%+9.3f, ", (double)(pid->section_interval) / STC_MS);

        /* section_head */
        for(i = 0; i < 7; i++)
        {
                fprintf(stdout, "%02X ", pid->section[i]);
        }
        fprintf(stdout, "%02X, ", pid->section[i++]);

        /* section_body */
        if(     0x00 == psi->table_id) {
                table_info_PAT(psi, pid->section);
        }
        else if(0x01 == psi->table_id) {
                table_info_CAT(psi, pid->section);
        }
        else if(0x02 == psi->table_id) {
                table_info_PMT(psi, pid->section);
        }
        else if(0x03 == psi->table_id) {
                table_info_TSDT(psi, pid->section);
        }
        else if(0x04 <= psi->table_id && psi->table_id <= 0x3F) {
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }
        else if(0x40 == psi->table_id) { /* actual */
                table_info_NIT(psi, pid->section);
        }
        else if(0x41 == psi->table_id) { /* other */
                table_info_NIT(psi, pid->section);
        }
        else if(0x42 == psi->table_id) { /* actual */
                table_info_SDT(psi, pid->section);
        }
        else if(0x43 <= psi->table_id && psi->table_id <= 0x45) {
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }
        else if(0x46 == psi->table_id) { /* other */
                table_info_SDT(psi, pid->section);
        }
        else if(0x47 <= psi->table_id && psi->table_id <= 0x49) {
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }
        else if(0x4A == psi->table_id) {
                table_info_BAT(psi, pid->section);
        }
        else if(0x4B <= psi->table_id && psi->table_id <= 0x4D) {
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }
        else if(0x4E == psi->table_id) { /* actual, P/F */
                table_info_EIT(psi, pid->section);
        }
        else if(0x4F == psi->table_id) { /* other, P/F */
                table_info_EIT(psi, pid->section);
        }
        else if(0x50 <= psi->table_id && psi->table_id <= 0x5F) { /* actual, schedule */
                table_info_EIT(psi, pid->section);
        }
        else if(0x60 <= psi->table_id && psi->table_id <= 0x6F) { /* other, schedule */
                table_info_EIT(psi, pid->section);
        }
        else if(0x70 == psi->table_id) {
                table_info_TDT(psi, pid->section);
        }
        else if(0x71 == psi->table_id) {
                table_info_RST(psi, pid->section);
        }
        else if(0x72 == psi->table_id) {
                table_info_ST(psi, pid->section);
        }
        else if(0x73 == psi->table_id) {
                table_info_TOT(psi, pid->section);
        }
        else if(0x74 <= psi->table_id && psi->table_id <= 0x7D) {
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }
        else if(0x7E == psi->table_id) {
                table_info_DIT(psi, pid->section);
        }
        else if(0x7F == psi->table_id) {
                table_info_SIT(psi, pid->section);
        }
        else if(0x80 <= psi->table_id && psi->table_id <= 0xFE) {
                fprintf(stdout, "user table_id: ");
                is_unknown_table_id = 1;
        }
        else { /* (0xFF == psi->table_id) */
                fprintf(stdout, "reserved table_id: ");
                is_unknown_table_id = 1;
        }

        if(is_unknown_table_id) {
                for(; i < psi->section_length + 3; i++)
                {
                        fprintf(stdout, "%02X ", pid->section[i]);
                }
        }
        fprintf(stdout, "\n");
        return;
}

static void show_tcp(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(0x1FFA != rslt->PID) {
                return;
        }

        print_atp_value(obj);
        atsc_mh_tcp(rslt->pkt->ts, obj->is_color);
        return;
}

static void show_pcr(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(!(rslt->has_PCR))
        {
                return;
        }

        print_atp_value(obj);
        fprintf(stdout, "%lld, %lld, %3d, %+7.3f, %+4.0f \n",
                rslt->PCR,
                rslt->PCR_base,
                rslt->PCR_ext,
                (double)(rslt->PCR_interval) / STC_MS,
                (double)(rslt->PCR_jitter) * 1e3 / STC_US);
        return;
}

static void show_sys_rate(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(!(rslt->has_rate))
        {
                return;
        }

        print_atp_value(obj);
        fprintf(stdout, "%ssys%s, %9.6f, %spsi-si%s, %9.6f, %sempty%s, %9.6f, \n",
                obj->color_yellow, obj->color_off, rslt->last_sys_cnt * 188.0 * 8 * 27 / (rslt->last_interval),
                obj->color_yellow, obj->color_off, rslt->last_psi_cnt * 188.0 * 8 * 27 / (rslt->last_interval),
                obj->color_yellow, obj->color_off, rslt->last_nul_cnt * 188.0 * 8 * 27 / (rslt->last_interval));
        return;
}

static void show_psi_rate(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        lnode_t *lnode;

        if(!(rslt->has_rate))
        {
                return;
        }

        print_atp_value(obj);
        fprintf(stdout, "%spsi-si%s, %9.6f, ",
                obj->color_yellow, obj->color_off, rslt->last_psi_cnt * 188.0 * 8 * 27 / (rslt->last_interval));

        /* traverse pid0 */
        /* if it is PSI/SI PID, output its bitrate */
        for(lnode = (lnode_t *)(rslt->pid0); lnode; lnode = lnode->next)
        {
                ts_pid_t *pid_item = (ts_pid_t *)lnode;
                if(pid_item->PID < 0x0020)
                {
                        fprintf(stdout, "%s0x%04X%s, %9.6f, ",
                                obj->color_yellow, pid_item->PID, obj->color_off,
                                pid_item->lcnt * 188.0 * 8 * 27 / (rslt->last_interval));
                }
        }
        fprintf(stdout, "\n");
        return;
}

static void show_prog_rate(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        lnode_t *lnode;

        if(!(rslt->has_rate))
        {
                return;
        }

        print_atp_value(obj);
        /* traverse pid0 */
        /* if it belongs to this program, output its bitrate */
        for(lnode = (lnode_t *)(rslt->pid0); lnode; lnode = lnode->next)
        {
                ts_pid_t *pid_item = (ts_pid_t *)lnode;
                if(pid_item->PID < 0x0020 || 0x1FFF == pid_item->PID)
                {
                        continue;
                }
                if(ANY_PROG == obj->aim_prog ||
                   (pid_item->prog && (pid_item->prog->program_number == obj->aim_prog)))
                {
                        fprintf(stdout, "%s0x%04X%s, %9.6f, ",
                                obj->color_yellow, pid_item->PID, obj->color_off,
                                pid_item->lcnt * 188.0 * 8 * 27 / (rslt->last_interval));
                }
        }
        fprintf(stdout, "\n");
        return;
}

static void show_rate(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        lnode_t *lnode;

        if(!(rslt->has_rate))
        {
                return;
        }

        print_atp_value(obj);
        /* traverse pid0, output its bitrate */
        for(lnode = (lnode_t *)(rslt->pid0); lnode; lnode = lnode->next)
        {
                ts_pid_t *pid_item = (ts_pid_t *)lnode;
                fprintf(stdout, "%s0x%04X%s, %9.6f, ",
                        obj->color_yellow, pid_item->PID, obj->color_off,
                        pid_item->lcnt * 188.0 * 8 * 27 / (rslt->last_interval));
        }
        fprintf(stdout, "\n");
        return;
}

static void show_ptsdts(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(!(rslt->has_PTS))
        {
                return;
        }

        print_atp_value(obj);
        fprintf(stdout, "%lld, %+8.3f, %+8.3f, ",
                rslt->PTS,
                (double)(rslt->PTS_interval) / (90), /* ms */
                (double)(rslt->PTS_minus_STC) / (90)); /* ms */

        if(rslt->has_DTS)
        {
                fprintf(stdout, "%lld, %+8.3f, %+8.3f,\n",
                        rslt->DTS,
                        (double)(rslt->DTS_interval) / (90), /* ms */
                        (double)(rslt->DTS_minus_STC) / (90)); /* ms */
        }
        else
        {
                fprintf(stdout, "%lld,         ,         ,\n",
                        rslt->PTS);
        }
        return;
}

static void show_pes(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        ts_pkt_t PKT;
        ts_pkt_t *pkt = &PKT;

        if(0 != rslt->PES_len)
        {
                pkt_init(pkt);
                pkt->data = rslt->PES_buf;
                pkt->cnt = rslt->PES_len;

                b2t(obj->tbuf, pkt);
                puts(obj->tbuf);
        }
        return;
}

static void show_es(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        ts_pkt_t PKT;
        ts_pkt_t *pkt = &PKT;

        if(0 != rslt->ES_len)
        {
                pkt_init(pkt);
                pkt->data = rslt->ES_buf;
                pkt->cnt = rslt->ES_len;

                b2t(obj->tbuf, pkt);
                puts(obj->tbuf);
        }
        return;
}

static void all_es(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(0 == rslt->ES_len)
        {
                return;
        }

        if(NULL == rslt->pid)
        {
                fprintf(stderr, "Bad pid point!\n");
                return;
        }

        if(NULL == rslt->pid->fd)
        {
                char name[100];

                sprintf(name, "%04X.es", rslt->pid->PID);
                fprintf(stdout, "open file %s\n", name);
                rslt->pid->fd = fopen(name, "wb");
        }
        if(NULL == rslt->pid->fd)
        {
                DBG(ERR_FOPEN_FAILED, "\n");
                return;
        }

        fwrite(rslt->ES_buf, rslt->ES_len, 1, rslt->pid->fd);
        return;
}

static void show_error(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        ts_error_t *err = &(rslt->err);

        /* First priority: necessary for de-codability (basic monitoring) */
        if(err->TS_sync_loss)
        {
                print_atp_value(obj);
                fprintf(stdout, "1.1, TS_sync_loss\n");
                if(err->Sync_byte_error > 10)
                {
                        fprintf(stdout, "\nToo many continual Sync_byte_error packet, EXIT!\n");
                        exit(EXIT_FAILURE);
                }
                return;
        }
        if(err->Sync_byte_error == 1)
        {
                print_atp_value(obj);
                fprintf(stdout, "1.2 , Sync_byte_error\n");
        }
        if(err->PAT_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "1.3 , PAT_error\n");
                err->PAT_error = 0;
        }
        if(err->Continuity_count_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "1.4 , Continuity_count_error(%X-%X=%2u)\n",
                        rslt->CC_find, rslt->CC_wait, rslt->CC_lost);
        }
        if(err->PMT_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "1.5 , PMT_error\n");
                err->PMT_error = 0;
        }
        if(err->PID_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "1.6 , PID_error\n");
                err->PID_error = 0;
        }

        /* Second priority: recommended for continuous or periodic monitoring */
        if(err->Transport_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "2.1 , Transport_error\n");
                err->Transport_error = 0;
        }
        if(err->CRC_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "2.2 , CRC_error(0x%08X! 0x%08X?)\n",
                        rslt->pid->CRC_32_calc, rslt->pid->CRC_32);
                err->CRC_error = 0;
        }
        if(err->PCR_repetition_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "2.3a, PCR_repetition_error(%+7.3f ms)\n",
                        (double)(rslt->PCR_interval) / STC_MS);
                err->PCR_repetition_error = 0;
        }
        if(err->PCR_discontinuity_indicator_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "2.3b, PCR_discontinuity_indicator_error(%+7.3f ms)\n",
                        (double)(rslt->PCR_continuity) / STC_MS);
                err->PCR_discontinuity_indicator_error = 0;
        }
        if(err->PCR_accuracy_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "2.4 , PCR_accuracy_error(%+4.0f ns)\n",
                        (double)(rslt->PCR_jitter) * 1e3 / STC_US);
                err->PCR_accuracy_error = 0;
        }
        if(err->PTS_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "2.5 , PTS_error\n");
                err->PTS_error = 0;
        }
        if(err->CAT_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "2.6 , CAT_error\n");
                err->CAT_error = 0;
        }

        /* Third priority: application dependant monitoring */
        /* ... */

        return;
}

static void print_atp_title(obj_t *obj)
{
        fprintf(stdout,
                "%shh:mm:ss%s, %saddress%s, address, STC, STC_base, %sPID%s, ",
                obj->color_yellow, obj->color_off,
                obj->color_yellow, obj->color_off,
                obj->color_yellow, obj->color_off);
        return;
}

static void print_atp_value(obj_t *obj)
{
        ts_rslt_t *rslt;
        ts_pkt_t *pkt;
        time_t tp;
        struct tm *lt; /* local time */
        char stime[32];

        rslt = obj->rslt;
        pkt = rslt->pkt;

        time(&tp);
        lt = localtime(&tp);
        strftime(stime, 32, "%H:%M:%S", lt);

        fprintf(stdout,
                "%s%s%s, %s0x%llX%s, %lld, %llu, %llu, %s0x%04X%s, ",
                obj->color_yellow, stime, obj->color_off,
                obj->color_yellow, pkt->ADDR, obj->color_off, pkt->ADDR,
                rslt->STC, rslt->STC_base,
                obj->color_yellow, rslt->PID, obj->color_off);
        return;
}

static void table_info_PAT(ts_psi_t *psi, uint8_t *section)
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

        while(section_length > 4)
        {
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
                        fprintf(stdout, "network_PID, 0x%04X, ", PID);
                }
                else {
                        fprintf(stdout, "program %5d, PID 0x%04X, ", program_number, PID);
                }
        }

        return;
}

static void table_info_CAT(ts_psi_t *psi, uint8_t *section)
{
        uint8_t *p = section + 3;
        int section_length;
        uint16_t descriptors_loop_length;

        /* section head */
        section_length = psi->section_length;

        /* CAT special */
        p += 5; section_length -= 5;
        descriptors_loop_length = section_length - 4;
        while(descriptors_loop_length > 0)
        {
                uint8_t len;

                len = descriptor(&p);
                descriptors_loop_length -= len;

                if(0 == len)
                {
                        fprintf(stdout, "wrong descriptor, ");
                        return;
                }
        }

        return;
}

static void table_info_PMT(ts_psi_t *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void table_info_TSDT(ts_psi_t *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void table_info_NIT(ts_psi_t *psi, uint8_t *section)
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
        while(network_descriptors_length > 0)
        {
                uint8_t len;

                len = descriptor(&p);
                network_descriptors_length -= len;
                section_length -= len;

                if(0 == len)
                {
                        fprintf(stdout, "wrong descriptor, ");
                        return;
                }
        }

        /* transport_stream_loop */
        transport_stream_loop_length = (*p++) & 0x0F; section_length--;
        transport_stream_loop_length <<= 8;
        transport_stream_loop_length |= *p++; section_length--;
        while(transport_stream_loop_length > 0)
        {
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

                while(transport_descriptors_length > 0)
                {
                        uint8_t len;

                        len = descriptor(&p);
                        transport_descriptors_length -= len;
                        section_length -= len;

                        if(0 == len)
                        {
                                fprintf(stdout, "wrong descriptor, ");
                                return;
                        }
                }
        }

        return;
}

static void table_info_SDT(ts_psi_t *psi, uint8_t *section)
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

        while(section_length > 4)
        {
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
                while(descriptors_loop_length > 0)
                {
                        uint8_t len;

                        len = descriptor(&p);
                        descriptors_loop_length -= len;
                        section_length -= len;

                        if(0 == len)
                        {
                                fprintf(stdout, "wrong descriptor, ");
                                return;
                        }
                        /* fprintf(stdout, "descriptors_loop_length(%d), ", descriptors_loop_length); */
                }
                /* fprintf(stdout, "section_length(%d), ", section_length); */
        }

        return;
}

static void table_info_BAT(ts_psi_t *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void table_info_EIT(ts_psi_t *psi, uint8_t *section)
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

        while(section_length > 4)
        {
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

                while(descriptors_loop_length > 0)
                {
                        uint8_t len;

                        len = descriptor(&p);
                        descriptors_loop_length -= len;
                        section_length -= len;

                        if(0 == len)
                        {
                                fprintf(stdout, "wrong descriptor, ");
                                return;
                        }
                }
        }

        return;
}

static void table_info_TDT(ts_psi_t *psi, uint8_t *section)
{
        uint8_t *p = section + 3;

        MJD_UTC(p); p += 5;

        return;
}

static void table_info_RST(ts_psi_t *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void table_info_ST(ts_psi_t *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void table_info_TOT(ts_psi_t *psi, uint8_t *section)
{
        uint8_t *p = section + 3;
        int descriptors_loop_length;

        MJD_UTC(p); p += 5;

        descriptors_loop_length = (0x0F & *p++);
        descriptors_loop_length <<= 8;
        descriptors_loop_length |= *p++;

        while(descriptors_loop_length > 0)
        {
                descriptors_loop_length -= descriptor(&p);
        }

        return;
}

static void table_info_DIT(ts_psi_t *psi, uint8_t *section)
{
        //uint8_t *p = section;
        //int section_length;

        return;
}

static void table_info_SIT(ts_psi_t *psi, uint8_t *section)
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
        switch(status)
        {
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
                        utf16_gb((const uint16_t *)p, str, len, 1);
                        break;
                default:
                        memcpy(str, p, len);
                        str[len] = '\0';
                        break;
        }
        fprintf(stdout, "%s", str);
        return 0;
}
