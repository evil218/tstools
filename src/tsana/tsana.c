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

#include "libts/common.h"
#include "libts/error.h"
#include "libts/if.h"
#include "libts/ts.h" /* has "list.h" already */

#define PKT_BBUF                        (256) /* 188 or 204 */
#define PKT_TBUF                        (PKT_BBUF * 3 + 10)

#define ANY_PID                         0x2000 /* any PID of [0x0000,0x1FFF] */
#define ANY_TABLE                       0xFF /* any table_id of [0x00,0xFE] */
#define ANY_PROG                        0x0000 /* any prog of [0x0001,0xFFFF] */

#define PCR_US                          (27) /* 27 clk means 1(us) */
#define PCR_MS                          (27 * 1000) /* uint: do NOT use 1e3  */

typedef struct
{
        int mode;
        int state;

        int is_outpsi; /* output txt psi packet to stdout */
        int is_prepsi; /* get psi information from file first */
        int is_color; /* use colour when print */
        int is_dump; /* output packet directly */
        uint64_t aim_start; /* ignore some packets fisrt, default: 0(no ignore) */
        uint64_t aim_count; /* stop after analyse some packets, default: 0(no stop) */
        uint16_t aim_pid;
        uint8_t aim_table;
        uint16_t aim_prog;
        uint64_t aim_interval; /* for rate calc */

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

static void table_info_SDT(uint8_t *section);
static void table_info_EIT(uint8_t *section);
static void table_info_TDT(uint8_t *section);
static void table_info_TOT(uint8_t *section);

static void MJD_UTC(uint8_t *buf);
static void UTC(uint8_t *buf);
static char *running_status(uint8_t status);

static int descriptor(uint8_t **buf);

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
                char *red_on = "";
                char *color_off = "";

                if(obj->is_color)
                {
                        red_on = FRED;
                        color_off = NONE;
                }
                fprintf(stderr, "%sPSI parsing unfinished because of the bad PCR data!%s\n",
                        red_on, color_off);

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
                                fprintf(stdout, "section\n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_SI:
                                print_atp_title(obj);
                                fprintf(stdout, "id, \n");
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
        obj->is_color = 0;
        obj->is_dump = 0;
        obj->cnt = 0;
        obj->aim_start = 0;
        obj->aim_count = 0;
        obj->aim_pid = ANY_PID;
        obj->aim_table = ANY_TABLE;
        obj->aim_prog = ANY_PROG;
        obj->aim_interval = 1000 * PCR_MS;

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
                                if(0x0000 <= dat && dat <= 0x1FFF)
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
                                if(0x00 <= dat && dat <= 0xFE)
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
                                if(0x0001 <= dat && dat <= 0xFFFF)
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
                                        obj->aim_interval = dat * PCR_MS;
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
        puts(" -color           enable colour effect to help read, default: mono");
        puts("");
        puts(" -start <x>       analyse from packet(x), default: 0, first packet");
        puts(" -count <n>       analyse n-packet then stop, default: 0, no stop");
        puts(" -pid <pid>       set cared PID, default: any PID");
        puts(" -table <id>      set cared table, default: any table");
        puts(" -prog <prog>     set cared prog, default: any program");
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
        char *yellow_on;
        char *color_off;

        if(!(rslt->is_psi_parse_finished))
        {
                return;
        }

        fprintf(stdout, "  PID , abbr, detail\n");
        for(lnode = (lnode_t *)(rslt->pid0); lnode; lnode = lnode->next)
        {
                pid = (ts_pid_t *)lnode;
                yellow_on = "";
                color_off = "";
                if((NULL != pid->track) && (obj->is_color))
                {
                        yellow_on = FYELLOW;
                        color_off = NONE;
                }
                fprintf(stdout, "%s0x%04X, %s, %s%s\n",
                        yellow_on,
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
        char *yellow_on = "";
        char *color_off = "";
        lnode_t *lnode;

        if(!(rslt->is_psi_parse_finished))
        {
                return;
        }

        if(obj->is_color)
        {
                yellow_on = FYELLOW;
                color_off = NONE;
        }

        fprintf(stdout, "transport_stream_id, %s%d%s(0x%04X)\n",
                yellow_on, rslt->transport_stream_id, color_off,
                rslt->transport_stream_id);

        for(lnode = (lnode_t *)(rslt->prog0); lnode; lnode = lnode->next)
        {
                int i;

                ts_prog_t *prog = (ts_prog_t *)lnode;
                fprintf(stdout, "program_number, %s%d%s(0x%04X), "
                        "PMT_PID, %s0x%04X%s, "
                        "PCR_PID, %s0x%04X%s, ",
                        yellow_on, prog->program_number, color_off,
                        prog->program_number,
                        yellow_on, prog->PMT_PID, color_off,
                        yellow_on, prog->PCR_PID, color_off);

                /* service_provider */
                if(prog->service_provider_len)
                {
                        fprintf(stdout, "service_provider, %s\"%s\"%s(%02X",
                                yellow_on, prog->service_provider, color_off,
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
                        fprintf(stdout, "service_name, %s\"%s\"%s(%02X",
                                yellow_on, prog->service_name, color_off,
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
                                yellow_on,
                                prog->program_info[0]);
                        for(i = 1; i < prog->program_info_len; i++)
                        {
                                fprintf(stdout, " %02X", prog->program_info[i]);
                        }
                        fprintf(stdout, "%s", color_off);
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
        char *red_on = "";
        char *yellow_on = "";
        char *color_off = "";

        if(obj->is_color)
        {
                red_on = FRED;
                yellow_on = FYELLOW;
                color_off = NONE;
        }

        for(lnode = *ptrack; lnode; lnode = lnode->next)
        {
                track = (ts_track_t *)lnode;
                fprintf(stdout, "track, %s0x%04X%s, "
                        "stream_type, %s0x%02X%s, ",
                        yellow_on, track->PID, color_off,
                        yellow_on, track->stream_type, color_off);
                fprintf(stdout, "type, %s%s%s, detail, %s%s%s, ",
                        yellow_on, track->sdes, color_off,
                        yellow_on, track->ldes, color_off);
                if(track->es_info_len)
                {
                        fprintf(stdout, "ES_info, %s%02X",
                                yellow_on, track->es_info[0]);
                        for(i = 1; i < track->es_info_len; i++)
                        {
                                fprintf(stdout, " %02X", track->es_info[i]);
                        }
                        fprintf(stdout, "%s, ", color_off);
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
        if(ANY_TABLE != obj->aim_table && psi->table_id != obj->aim_table)
        {
                return;
        }

        print_atp_value(obj);

        /* table_head */
        for(i = 0; i < 7; i++)
        {
                fprintf(stdout, "%02X ", pid->section[i]);
        }
        fprintf(stdout, "%02X, ", pid->section[i++]);

        /* table_body */
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
        ts_rslt_t *rslt = obj->rslt;
        ts_psi_t *psi = &(rslt->psi);
        ts_pid_t *pid = rslt->pid;

        if(!(rslt->has_section))
        {
                return;
        }
        if(ANY_TABLE != obj->aim_table && psi->table_id != obj->aim_table)
        {
                return;
        }

        print_atp_value(obj);

        /* table_head */
        for(i = 0; i < 7; i++)
        {
                fprintf(stdout, "%02X ", pid->section[i]);
        }
        fprintf(stdout, "%02X, ", pid->section[i++]);

        /* table_body */
        switch(psi->table_id)
        {
                case 0x42:
                case 0x46:
                        table_info_SDT(pid->section);
                        break;
                case 0x4E: /* FIXME: 0x50 ~ 0x5F */
                case 0x4F: /* FIXME: 0x60 ~ 0x6F */
                        table_info_EIT(pid->section);
                        break;
                case 0x70:
                        table_info_TDT(pid->section);
                        break;
                case 0x73:
                        table_info_TOT(pid->section);
                        break;
                default:
                        for(; i < psi->section_length + 3; i++)
                        {
                                fprintf(stdout, "%02X ", pid->section[i]);
                        }
                        break;
        }
        fprintf(stdout, "end\n");
        return;
}

static void show_pcr(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(ANY_PID != obj->aim_pid && rslt->PID != obj->aim_pid)
        {
                return;
        }
        if(!(rslt->has_PCR))
        {
                return;
        }

        print_atp_value(obj);
        fprintf(stdout, "%lld, %lld, %3d, %+7.3f, %+4.0f \n",
                rslt->PCR,
                rslt->PCR_base,
                rslt->PCR_ext,
                (double)(rslt->PCR_interval) / PCR_MS,
                (double)(rslt->PCR_jitter) * 1e3 / PCR_US);
        return;
}

static void show_sys_rate(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        char *yellow_on = "";
        char *color_off = "";

        if(!(rslt->has_rate))
        {
                return;
        }

        if(obj->is_color)
        {
                yellow_on = FYELLOW;
                color_off = NONE;
        }

        print_atp_value(obj);
        fprintf(stdout, "%ssys%s, %9.6f, %spsi-si%s, %9.6f, %sempty%s, %9.6f, \n",
                yellow_on, color_off, rslt->last_sys_cnt * 188.0 * 8 * 27 / (rslt->last_interval),
                yellow_on, color_off, rslt->last_psi_cnt * 188.0 * 8 * 27 / (rslt->last_interval),
                yellow_on, color_off, rslt->last_nul_cnt * 188.0 * 8 * 27 / (rslt->last_interval));
        return;
}

static void show_psi_rate(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        char *yellow_on = "";
        char *color_off = "";
        lnode_t *lnode;

        if(!(rslt->has_rate))
        {
                return;
        }

        if(obj->is_color)
        {
                yellow_on = FYELLOW;
                color_off = NONE;
        }

        print_atp_value(obj);
        fprintf(stdout, "%spsi-si%s, %9.6f, ",
                yellow_on, color_off, rslt->last_psi_cnt * 188.0 * 8 * 27 / (rslt->last_interval));

        /* traverse pid0 */
        /* if it is PSI/SI PID, output its bitrate */
        for(lnode = (lnode_t *)(rslt->pid0); lnode; lnode = lnode->next)
        {
                ts_pid_t *pid_item = (ts_pid_t *)lnode;
                if(pid_item->PID < 0x0020)
                {
                        fprintf(stdout, "%s0x%04X%s, %9.6f, ",
                                yellow_on, pid_item->PID, color_off,
                                pid_item->lcnt * 188.0 * 8 * 27 / (rslt->last_interval));
                }
        }
        fprintf(stdout, "\n");
        return;
}

static void show_prog_rate(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        char *yellow_on = "";
        char *color_off = "";
        lnode_t *lnode;

        if(!(rslt->has_rate))
        {
                return;
        }

        if(obj->is_color)
        {
                yellow_on = FYELLOW;
                color_off = NONE;
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
                                yellow_on, pid_item->PID, color_off,
                                pid_item->lcnt * 188.0 * 8 * 27 / (rslt->last_interval));
                }
        }
        fprintf(stdout, "\n");
        return;
}

static void show_rate(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;
        char *yellow_on = "";
        char *color_off = "";
        lnode_t *lnode;

        if(!(rslt->has_rate))
        {
                return;
        }

        if(obj->is_color)
        {
                yellow_on = FYELLOW;
                color_off = NONE;
        }

        print_atp_value(obj);
        /* traverse pid0, output its bitrate */
        for(lnode = (lnode_t *)(rslt->pid0); lnode; lnode = lnode->next)
        {
                ts_pid_t *pid_item = (ts_pid_t *)lnode;
                fprintf(stdout, "%s0x%04X%s, %9.6f, ",
                        yellow_on, pid_item->PID, color_off,
                        pid_item->lcnt * 188.0 * 8 * 27 / (rslt->last_interval));
        }
        fprintf(stdout, "\n");
        return;
}

static void show_ptsdts(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(ANY_PID != obj->aim_pid && rslt->PID != obj->aim_pid)
        {
                return;
        }
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

        if(ANY_PID != obj->aim_pid && rslt->PID != obj->aim_pid)
        {
                return;
        }
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

        if(ANY_PID != obj->aim_pid && rslt->PID != obj->aim_pid)
        {
                return;
        }
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

        if(ANY_PID != obj->aim_pid && rslt->PID != obj->aim_pid)
        {
                return;
        }

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
        if(err->PAT_error_2)
        {
                print_atp_value(obj);
                fprintf(stdout, "1.3a, PAT_error_2\n");
                err->PAT_error_2 = 0;
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
        if(err->PMT_error_2)
        {
                print_atp_value(obj);
                fprintf(stdout, "1.5a, PMT_error_2\n");
                err->PMT_error_2 = 0;
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
                        (double)(rslt->PCR_interval) / PCR_MS);
                err->PCR_repetition_error = 0;
        }
        if(err->PCR_discontinuity_indicator_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "2.3b, PCR_discontinuity_indicator_error(%+7.3f ms)\n",
                        (double)(rslt->PCR_continuity) / PCR_MS);
                err->PCR_discontinuity_indicator_error = 0;
        }
        if(err->PCR_accuracy_error)
        {
                print_atp_value(obj);
                fprintf(stdout, "2.4 , PCR_accuracy_error(%+4.0f ns)\n",
                        (double)(rslt->PCR_jitter) * 1e3 / PCR_US);
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
        char *yellow_on = "";
        char *color_off = "";

        if(obj->is_color)
        {
                yellow_on = FYELLOW;
                color_off = NONE;
        }

        fprintf(stdout,
                "%shh:mm:ss%s, %saddress%s, address, STC, STC_base, %sPID%s, ",
                yellow_on, color_off,
                yellow_on, color_off,
                yellow_on, color_off);
        return;
}

static void print_atp_value(obj_t *obj)
{
        ts_rslt_t *rslt;
        ts_pkt_t *pkt;
        time_t tp;
        struct tm *lt; /* local time */
        char stime[32];
        char *yellow_on = "";
        char *color_off = "";

        rslt = obj->rslt;
        pkt = rslt->pkt;

        time(&tp);
        lt = localtime(&tp);
        strftime(stime, 32, "%H:%M:%S", lt);

        if(obj->is_color)
        {
                yellow_on = FYELLOW;
                color_off = NONE;
        }

        fprintf(stdout,
                "%s%s%s, %s0x%llX%s, %lld, %llu, %llu, %s0x%04X%s, ",
                yellow_on, stime, color_off,
                yellow_on, pkt->ADDR, color_off, pkt->ADDR,
                rslt->STC, rslt->STC_base,
                yellow_on, rslt->PID, color_off);
        return;
}

static void table_info_SDT(uint8_t *section)
{
        uint8_t *p = section;
        int section_length;
        uint16_t transport_stream_id;
        uint16_t original_network_id;

        p += 1;
        section_length = (0x0F & *p++);
        section_length <<= 8;
        section_length |= *p++;

        transport_stream_id = *p++; section_length--;
        transport_stream_id <<= 8;
        transport_stream_id |= *p++; section_length--;
        fprintf(stdout, "transport_stream_id: %d, ", transport_stream_id);
        p += 3; section_length -= 3;
        original_network_id = *p++; section_length--;
        original_network_id <<= 8;
        original_network_id |= *p++; section_length--;
        fprintf(stdout, "original_network_id: %d, ", original_network_id);
        p += 1; section_length -= 1;

        while(section_length > 4)
        {
                uint8_t data;
                uint16_t service_id;
                uint8_t rstatus;
                uint16_t descriptors_loop_length;

                service_id = *p++; section_length--;
                service_id <<= 8;
                service_id |= *p++; section_length--;
                fprintf(stdout, "service_id: %d, ", service_id);
                p += 1; section_length -= 1;
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

                        if(2 == len)
                        {
                                fprintf(stdout, "wrong descriptor, ");
                                return;
                        }
                }
        }

        return;
}

static void table_info_EIT(uint8_t *section)
{
        uint8_t *p = section;
        int section_length;
        uint16_t service_id;
        uint16_t transport_stream_id;
        uint16_t original_network_id;
        uint8_t segment_last_section_number;
        uint8_t last_table_id;

        p += 1;
        section_length = (0x0F & *p++);
        section_length <<= 8;
        section_length |= *p++;

        service_id = *p++; section_length--;
        service_id <<= 8;
        service_id |= *p++; section_length--;
        fprintf(stdout, "service_id: %d, ", service_id);
        p += 3; section_length -= 3;
        transport_stream_id = *p++; section_length--;
        transport_stream_id <<= 8;
        transport_stream_id |= *p++; section_length--;
        fprintf(stdout, "transport_stream_id: %d, ", transport_stream_id);
        original_network_id = *p++; section_length--;
        original_network_id <<= 8;
        original_network_id |= *p++; section_length--;
        fprintf(stdout, "original_network_id: %d, ", original_network_id);
        segment_last_section_number = *p++; section_length--;
        fprintf(stdout, "segment_last_section_number: %d, ", segment_last_section_number);
        last_table_id = *p++; section_length--;
        fprintf(stdout, "last_table_id: %d, ", last_table_id);

        while(section_length > 4)
        {
                uint8_t data;
                uint16_t event_id;
                uint8_t rstatus;
                uint16_t descriptors_loop_length;

                event_id = *p++; section_length--;
                event_id <<= 8;
                event_id |= *p++; section_length--;
                fprintf(stdout, "event_id: %d, ", event_id);
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

                        if(2 == len)
                        {
                                fprintf(stdout, "wrong descriptor, ");
                                return;
                        }
                }
        }

        return;
}

static void table_info_TDT(uint8_t *section)
{
        uint8_t *p = section;

        p += 3;
        MJD_UTC(p); p += 5;

        return;
}

static void table_info_TOT(uint8_t *section)
{
        uint8_t *p = section;
        int descriptors_loop_length;

        p += 3;
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
        uint8_t *p = *buf;
        uint8_t tag;
        uint8_t len;

        tag = *p++;
        len = *p++;

        fprintf(stdout, "tag, 0x%02X, len, 0x%02X, ", tag, len);

        len += 2; /* count tag and len byte */
        *buf += len;

        len = (0xFF == tag) ? 2 : len; /* wrong buffer */
        return len;
}
