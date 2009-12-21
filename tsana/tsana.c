/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: tsana.c
// Purpose: analyse character of ts stream
// To build: gcc -std=c99 -o tsana.exe tsana.c
// Copyright (C) 2009 by ZHOU Cheng. All right reserved.
//=============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strcmp, etc
#include <time.h>   // for time(), etc
#include <stdint.h> // for uint?_t, etc

#include "error.h"
#include "if.h"
#include "ts.h"

//=============================================================================
// struct definition
//=============================================================================
typedef struct
{
        int mode;
        int state;

        int is_outpsi; // output txt psi package to stdout
        int is_prepsi; // get psi information from file first
        uint16_t aim_pid;

        int is_need_time; // time or address
        char time[255];
        long unsigned int addr; // addr in input

        uint32_t ts_size;
        uint8_t bbuf[204];
        char tbuf[1024];

        int ts_id;
        ts_rslt_t *rslt;
}
obj_t;

//=============================================================================
// enum definition
//=============================================================================
enum
{
        MODE_PID,
        MODE_PSI,
        MODE_PES,
        MODE_ES,
        MODE_PTSDTS,
        MODE_PCR,
        MODE_CC,
        MODE_DEBUG,
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
        GOT_WRONG_PKG,
        GOT_RIGHT_PKG,
        GOT_EOF
};

//=============================================================================
// variable definition
//=============================================================================
static obj_t *obj = NULL;

//=============================================================================
// sub-function declaration
//=============================================================================
static void state_parse_psi(obj_t *obj);
static void state_parse_each(obj_t *obj);

static obj_t *create(int argc, char *argv[]);
static int delete(obj_t *obj);

static void show_help();
static void show_version();

static int get_one_pkg(obj_t *obj);

static void show_pids(struct LIST *list);
static void show_prog(struct LIST *list);
static void show_track(struct LIST *list);

static void show_cc(obj_t *obj);
static void show_pcr(obj_t *obj);
static void show_pes(obj_t *obj);
static void show_es(obj_t *obj);
static void show_ptsdts(obj_t *obj);

#if 0
static void show_TS(obj_t *obj);
#endif

//=============================================================================
// the main function
//=============================================================================
int main(int argc, char *argv[])
{
        int get_rslt;
        obj = create(argc, argv);

        while(STATE_EXIT != obj->state && GOT_EOF != (get_rslt = get_one_pkg(obj)))
        {
                if(GOT_WRONG_PKG == get_rslt)
                {
                        break;
                }
                if(0 != tsParseTS(obj->ts_id, obj->bbuf))
                {
                        break;
                }
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
                                fprintf(stderr, "Wrong state!\n");
                                obj->state = STATE_EXIT;
                                break;
                }
                obj->addr += obj->ts_size;
        }

        if(STATE_PARSE_PSI == obj->state)
        {
                fprintf(stderr, "PSI parsing unfinished!\n");
                show_pids(obj->rslt->pid_list);
                show_prog(obj->rslt->prog_list);
        }

        delete(obj);
        exit(EXIT_SUCCESS);
}

//=============================================================================
// sub-function definition
//=============================================================================
static void state_parse_psi(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(rslt->pid == rslt->concerned_pid)
        {
                tsParseOther(obj->ts_id);
                if(obj->is_outpsi)
                {
                        puts(obj->tbuf);
                }
        }

        if(rslt->is_psi_parsed)
        {
                switch(obj->mode)
                {
                        case MODE_PID:
                                show_pids(obj->rslt->pid_list);
                                obj->state = STATE_EXIT;
                                break;
                        case MODE_PSI:
                                show_prog(obj->rslt->prog_list);
                                obj->state = STATE_EXIT;
                                break;
                        case MODE_CC:
                                obj->addr -= obj->ts_size;
                                if(obj->is_need_time)
                                {
                                        fprintf(stdout, "year-mm-dd HH:MM:SS,");
                                }
                                else
                                {
                                        fprintf(stdout, "address(X),address(d),");
                                }
                                fprintf(stdout, "   PID,wait,find,lost\n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_PCR:
                                obj->addr -= obj->ts_size;
                                if(obj->is_need_time)
                                {
                                        fprintf(stdout, "year-mm-dd HH:MM:SS,");
                                }
                                else
                                {
                                        fprintf(stdout, "address(X),address(d),");
                                }
                                fprintf(stdout, "   PID,          PCR,  PCR_BASE,PCR_EXT\n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_PES:
                        case MODE_ES:
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_PTSDTS:
                                if(obj->is_need_time)
                                {
                                        fprintf(stdout, "year-mm-dd HH:MM:SS,");
                                }
                                else
                                {
                                        fprintf(stdout, "address(X),address(d),");
                                }
                                fprintf(stdout, "   PID,       PTS,       DTS\n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_EXIT:
                        default:
                                obj->state = STATE_EXIT;
                                break;
                }
        }
}

static void state_parse_each(obj_t *obj)
{
        time_t tp;
        struct tm *lt; // local time

        time(&tp);
        lt = localtime(&tp);
        strftime(obj->time, 255, "%Y-%m-%d %H:%M:%S", lt);

        tsParseOther(obj->ts_id);

        switch(obj->mode)
        {
                case MODE_CC:
                        show_cc(obj);
                        break;
                case MODE_PCR:
                        show_pcr(obj);
                        break;
                case MODE_PES:
                        show_pes(obj);
                        break;
                case MODE_ES:
                        show_es(obj);
                        break;
                case MODE_PTSDTS:
                        show_ptsdts(obj);
                        break;
                default:
                        fprintf(stderr, "wrong mode\n");
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
                DBG(ERR_MALLOC_FAILED);
                return NULL;
        }

        obj->mode = MODE_PID;
        obj->is_need_time = 0;
        obj->is_outpsi = 0;
        obj->is_prepsi = 0;
        obj->ts_size = 188; // FIXME: should be established in sync_input()

        for(i = 1; i < argc; i++)
        {
                if ('-' == argv[i][0])
                {
                        if (0 == strcmp(argv[i], "-pid-list"))
                        {
                                obj->mode = MODE_PID;
                        }
                        else if (0 == strcmp(argv[i], "-psi-tree"))
                        {
                                obj->mode = MODE_PSI;
                        }
                        else if (0 == strcmp(argv[i], "-outpsi"))
                        {
                                obj->is_outpsi = 1;
                                obj->mode = MODE_EXIT;
                        }
                        else if (0 == strcmp(argv[i], "-prepsi"))
                        {
                                obj->is_prepsi = 1;
                        }
                        else if (0 == strcmp(argv[i], "-cc"))
                        {
                                obj->mode = MODE_CC;
                        }
                        else if (0 == strcmp(argv[i], "-pcr"))
                        {
                                obj->mode = MODE_PCR;
                        }
                        else if (0 == strcmp(argv[i], "-debug"))
                        {
                                obj->mode = MODE_DEBUG;
                        }
                        else if (0 == strcmp(argv[i], "-pid"))
                        {
                                sscanf(argv[++i], "%i" , &dat);
                                obj->aim_pid = (uint16_t)(dat & 0x1FFF);
                        }
                        else if (0 == strcmp(argv[i], "-pes"))
                        {
                                obj->mode = MODE_PES;
                        }
                        else if (0 == strcmp(argv[i], "-es"))
                        {
                                obj->mode = MODE_ES;
                        }
                        else if (0 == strcmp(argv[i], "-ptsdts"))
                        {
                                obj->mode = MODE_PTSDTS;
                        }
                        else if (0 == strcmp(argv[i], "--help"))
                        {
                                show_help();
                                exit(EXIT_SUCCESS);
                        }
                        else if (0 == strcmp(argv[i], "--version"))
                        {
                                show_version();
                                exit(EXIT_SUCCESS);
                        }
                        else
                        {
                                fprintf(stderr, "Wrong parameter: %s\n", argv[i]);
                                exit(EXIT_FAILURE);
                        }
                }
                else
                {
                        fprintf(stderr, "Wrong parameter: %s\n", argv[i]);
                        exit(EXIT_FAILURE);
                }
        }

        obj->ts_id = tsCreate(obj->ts_size, &(obj->rslt));
        obj->state = STATE_PARSE_PSI;

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
        puts("'tsana' get TS package from stdin, analyse, then send the result to stdout.");
        puts("");
        puts("Usage: tsana [OPTION]...");
        puts("");
        puts("Options:");
        puts("  -pid-list      show PID list information, default option");
        puts("  -psi-tree      show PSI tree information");
        puts("  -outpsi        output PSI package");
        puts("  -cc            check Continuity Counter");
        puts("  -pcr           show all PCR value");
        puts("  -pid <pid>     set cared <pid>");
        puts("  -pes           output PES data of <pid>");
        puts("  -es            output ES data of <pid>");
        puts("  -ptsdts        output PTS and DTS of <pid>");
#if 0
        puts("  -o <file>      output file name, default: stdout");
        puts("  -prepsi <file> get PSI information from <file> first");
        puts("  -debug         show all errors found");
        puts("  -flt           filter package with <pid>");
#endif
        puts("");
        puts("  --help         display this information");
        puts("  --version      display my version");
        puts("");
        puts("Examples:");
        puts("  \"tscat xxx.ts | tsana -cc\" -- report CC error information");
        puts("");
        puts("Report bugs to <zhoucheng@tsinghua.org.cn>.");
}

static void show_version()
{
        //fprintf(stdout, "tsana 0.1.0 (by Cygwin), %s %s\n", __TIME__, __DATE__);
        puts("tsana 1.0.0");
        puts("");
        puts("Copyright (C) 2009 ZHOU Cheng.");
        puts("This is free software; contact author for additional information.");
        puts("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR");
        puts("A PARTICULAR PURPOSE.");
        puts("");
        puts("Written by ZHOU Cheng.");
}

static int get_one_pkg(obj_t *obj)
{
        char *rslt;
        int size;

        rslt = fgets(obj->tbuf, 1000, stdin);
        if(NULL == rslt)
        {
                return GOT_EOF;
        }
        //puts(obj->tbuf);

        size = t2b(obj->bbuf, obj->tbuf);
        if(size != 188)
        {
                //fprintf(stderr, "Bad pkg_size:%d\n%s\n", size, obj->tbuf);
                return GOT_WRONG_PKG;
        }

        return GOT_RIGHT_PKG;
}

static void show_pids(struct LIST *list)
{
        struct NODE *node;
        ts_pid_t *pids;

        //fprintf(stdout, "pid_list(%d-item):\n\n", list_count(list));
        fprintf(stdout, " PID  , dCC, track,   type  , abbr, detail\n");

        for(node = list->head; node; node = node->next)
        {
                pids = (ts_pid_t *)node;
                fprintf(stdout, "0x%04X,  %u ,     %c, %s, %s, %s\n",
                        pids->PID,
                        pids->dCC,
                        pids->is_track ? '*' : ' ',
                        pids->type,
                        pids->sdes,
                        pids->ldes);
        }
}

static void show_prog(struct LIST *list)
{
        uint16_t i;
        struct NODE *node;
        ts_prog_t *prog;

        //fprintf(stdout, "program_list(%d-item):\n\n", list_count(list));

        for(node = list->head; node; node = node->next)
        {
                prog = (ts_prog_t *)node;
                fprintf(stdout, "+ program %d(0x%04X), PMT_PID = 0x%04X\n",
                        prog->program_number,
                        prog->program_number,
                        prog->PMT_PID);
                fprintf(stdout, "    PCR_PID = 0x%04X\n",
                        prog->PCR_PID);
                fprintf(stdout, "    program_info:");
                for(i = 0; i < prog->program_info_len; i++)
                {
                        if(0x00 == (i & 0x0F)) fprintf(stdout, "\n");
                        fprintf(stdout, " %02X", *(prog->program_info_buf + i));
                }
                fprintf(stdout, "\n");
                show_track(prog->track);
        }
}

static void show_track(struct LIST *list)
{
        uint16_t i;
        struct NODE *node;
        ts_track_t *track;

        //fprintf(stdout, "track_list(%d-item):\n\n", list_count(list));

        for(node = list->head; node; node = node->next)
        {
                track = (ts_track_t *)node;
                fprintf(stdout, "    track\n");
                fprintf(stdout, "        stream_type = 0x%02X(%s)\n",
                        track->stream_type, track->type);
                fprintf(stdout, "        elementary_PID = 0x%04X\n",
                        track->PID);
                fprintf(stdout, "        ES_info:");
                for(i = 0; i < track->es_info_len; i++)
                {
                        if(0x00 == (i & 0x0F)) fprintf(stdout, "\n");
                        fprintf(stdout, " %02X", *(track->es_info_buf +i));
                }
                fprintf(stdout, "\n");
        }
}

static void show_cc(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(0 == rslt->CC.lost)
        {
                return;
        }

        if(obj->is_need_time)
        {
                fprintf(stdout, "%s,", obj->time);
        }
        else
        {
                fprintf(stdout, "0x%08lX", obj->addr);
                fprintf(stdout, ",%10lu", obj->addr);
        }
        fprintf(stdout, "0x%04X", rslt->pid);
        fprintf(stdout, ",  %2u,  %2u,  %2d\n",
                rslt->CC.wait,
                rslt->CC.find,
                rslt->CC.lost);
        return;
}

static void show_pcr(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(!(rslt->has_PCR))
        {
                return;
        }
        if(obj->is_need_time)
        {
                fprintf(stdout, "%s,", obj->time);
        }
        else
        {
                fprintf(stdout, "0x%08lX", obj->addr);
                fprintf(stdout, ",%10lu", obj->addr);
        }
        fprintf(stdout, ",0x%04X", rslt->pid);
        fprintf(stdout, ",%13llu,%10llu,    %3u\n",
                rslt->PCR,
                rslt->PCR_base,
                rslt->PCR_ext);
}

static void show_pes(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(rslt->pid == obj->aim_pid && 0 != rslt->PES.len)
        {
                b2t(obj->tbuf, rslt->PES.buf, rslt->PES.len);
                puts(obj->tbuf);
        }
}

static void show_es(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(rslt->pid == obj->aim_pid && 0 != rslt->ES.len)
        {
                b2t(obj->tbuf, rslt->ES.buf, rslt->ES.len);
                puts(obj->tbuf);
        }
}

static void show_ptsdts(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(rslt->pid == obj->aim_pid && rslt->has_PTS)
        {
                if(obj->is_need_time)
                {
                        fprintf(stdout, "%s,", obj->time);
                }
                else
                {
                        fprintf(stdout, "0x%08lX", obj->addr);
                        fprintf(stdout, ",%10lu", obj->addr);
                }
                fprintf(stdout, ",0x%04X", rslt->pid);
                fprintf(stdout, ",%10llu", rslt->PTS);

                if(rslt->has_DTS)
                {
                        fprintf(stdout, ",%10llu", rslt->DTS);
                }
                else
                {
                        fprintf(stdout, ",          ");
                }
                fprintf(stdout, "\n");
        }
}

#if 0
static void show_TS(obj_t *obj)
{
        ts_t *ts = obj->ts;

        fprintf(stdout, "TS:\n");
        fprintf(stdout, "  0x%02X  : sync_byte\n", ts->sync_byte);
        fprintf(stdout, "  %c     : transport_error_indicator\n", (ts->transport_error_indicator) ? '1' : '0');
        fprintf(stdout, "  %c     : payload_unit_start_indicator\n", (ts->payload_unit_start_indicator) ? '1' : '0');
        fprintf(stdout, "  %c     : transport_priority\n", (ts->transport_priority) ? '1' : '0');
        fprintf(stdout, "  0x%04X: PID\n", ts->PID);
        fprintf(stdout, "  0b%s  : transport_scrambling_control\n", printb(ts->transport_scrambling_control, 2));
        fprintf(stdout, "  0b%s  : adaption_field_control\n", printb(ts->adaption_field_control, 2));
        fprintf(stdout, "  0x%X   : continuity_counter\n", ts->continuity_counter);
}
#endif
//=============================================================================
// THE END.
//=============================================================================
