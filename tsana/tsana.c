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
#include <time.h> // for localtime(), etc
#include <stdint.h> // for uint?_t, etc

#include "error.h"
#include "if.h"
#include "ts.h"

#define ANY_PID 0x2000 // means any PID of [0x0000,0x1FFF]

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
        MODE_CC,
        MODE_PCR,
        MODE_PTSDTS,
        MODE_PES,
        MODE_ES,
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
static void show_ptsdts(obj_t *obj);
static void show_pes(obj_t *obj);
static void show_es(obj_t *obj);

static void print_atp_title(); // atp: address_time_PID
static void print_atp_value(obj_t *obj); // atp: address_time_PID

//=============================================================================
// the main function
//=============================================================================
int main(int argc, char *argv[])
{
        int get_rslt;
        ts_rslt_t *rslt;

        obj = create(argc, argv);
        rslt = obj->rslt;

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
                                if(ANY_PID == obj->aim_pid || rslt->pid == obj->aim_pid)
                                {
                                        state_parse_each(obj);
                                }
                                break;
                        case STATE_EXIT:
                                break;
                        default:
                                fprintf(stderr, "Wrong state(%d)!\n", obj->state);
                                obj->state = STATE_EXIT;
                                break;
                }
        }

        if(STATE_PARSE_PSI == obj->state)
        {
                fprintf(stderr, "PSI parsing unfinished!\n");
                show_pids(rslt->pid_list);
                show_prog(rslt->prog_list);
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

        //if(rslt->pid == rslt->concerned_pid)
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
                                print_atp_title();
                                fprintf(stdout, "wait, find, lost, \n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_PCR:
                                print_atp_title();
                                fprintf(stdout, "PCR, BASE, EXT, \n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_PTSDTS:
                                print_atp_title();
                                fprintf(stdout, "PTS, DTS, \n");
                                obj->state = STATE_PARSE_EACH;
                                break;
                        case MODE_PES:
                        case MODE_ES:
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
        tsParseOther(obj->ts_id);

        switch(obj->mode)
        {
                case MODE_CC:
                        show_cc(obj);
                        break;
                case MODE_PCR:
                        show_pcr(obj);
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
                DBG(ERR_MALLOC_FAILED);
                return NULL;
        }

        obj->mode = MODE_PID;
        obj->state = STATE_PARSE_PSI;
        obj->is_outpsi = 0;
        obj->is_prepsi = 0;
        obj->aim_pid = ANY_PID;
        obj->ts_size = 188; // FIXME: should be established in sync_input()

        for(i = 1; i < argc; i++)
        {
                if('-' == argv[i][0])
                {
                        if(0 == strcmp(argv[i], "-pid-list"))
                        {
                                obj->mode = MODE_PID;
                        }
                        else if(0 == strcmp(argv[i], "-psi-tree"))
                        {
                                obj->mode = MODE_PSI;
                        }
                        else if(0 == strcmp(argv[i], "-outpsi"))
                        {
                                obj->is_outpsi = 1;
                                obj->mode = MODE_EXIT;
                        }
                        else if(0 == strcmp(argv[i], "-prepsi"))
                        {
                                obj->is_prepsi = 1;
                        }
                        else if(0 == strcmp(argv[i], "-cc"))
                        {
                                obj->mode = MODE_CC;
                        }
                        else if(0 == strcmp(argv[i], "-pcr"))
                        {
                                obj->mode = MODE_PCR;
                        }
                        else if(0 == strcmp(argv[i], "-debug"))
                        {
                                obj->mode = MODE_DEBUG;
                        }
                        else if(0 == strcmp(argv[i], "-pid"))
                        {
                                sscanf(argv[++i], "%i" , &dat);
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
                        else if(0 == strcmp(argv[i], "-pes"))
                        {
                                obj->mode = MODE_PES;
                        }
                        else if(0 == strcmp(argv[i], "-es"))
                        {
                                obj->mode = MODE_ES;
                        }
                        else if(0 == strcmp(argv[i], "-ptsdts"))
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
        puts(" -pid-list        show PID list information, default option");
        puts(" -psi-tree        show PSI tree information");
        puts(" -outpsi          output PSI package");
        puts(" -cc              check Continuity Counter of cared <pid>");
        puts(" -pcr             show PCR value of cared <pid>");
        puts(" -pid <pid>       set cared <pid>, default: ANY PID");
        puts(" -pes             output PES data of cared <pid>");
        puts(" -es              output ES data of cared <pid>");
        puts(" -ptsdts          output PTS and DTS of cared <pid>");
#if 0
        puts(" -prepsi <file>   get PSI information from <file> first");
        puts(" -debug           show all errors found");
#endif
        puts("");
        puts(" -h, --help       display this information");
        puts(" -v, --version    display my version");
        puts("");
        puts("Examples:");
        puts("  \"bincat xxx.ts | tsana -cc\" -- report CC error information");
        puts("");
        puts("Report bugs to <zhoucheng@tsinghua.org.cn>.");
        return;
}

static void show_version()
{
        puts("tsana 1.0.0");
        puts("");
        puts("Copyright (C) 2009,2010 ZHOU Cheng.");
        puts("This is free software; contact author for additional information.");
        puts("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR");
        puts("A PARTICULAR PURPOSE.");
        puts("");
        puts("Written by ZHOU Cheng.");
        return;
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
        fprintf(stdout, " PID  , dCC, track,     abbr, detail\n");

        for(node = list->head; node; node = node->next)
        {
                pids = (ts_pid_t *)node;
                fprintf(stdout, "0x%04X,  %u ,     %c, %s, %s\n",
                        pids->PID,
                        pids->dCC,
                        pids->is_track ? '*' : ' ',
                        pids->sdes,
                        pids->ldes);
        }
        return;
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
                fprintf(stdout, "        ");
                for(i = 0; i < prog->program_info_len; i++)
                {
                        if(0x00 == (i & 0x0F))
                        {
                                fprintf(stdout, "\n");
                                fprintf(stdout, "        ");
                        }
                        fprintf(stdout, "%02X ", *(prog->program_info_buf + i));
                }
                fprintf(stdout, "\n");
                show_track(prog->track);
        }
        return;
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
                fprintf(stdout, "        stream_type = 0x%02X, %s, %s\n",
                        track->stream_type,
                        track->sdes,
                        track->ldes);
                fprintf(stdout, "        elementary_PID = 0x%04X\n",
                        track->PID);
                fprintf(stdout, "        ES_info:");
                fprintf(stdout, "            ");
                for(i = 0; i < track->es_info_len; i++)
                {
                        if(0x00 == (i & 0x0F))
                        {
                                fprintf(stdout, "\n");
                                fprintf(stdout, "            ");
                        }
                        fprintf(stdout, "%02X ", *(track->es_info_buf +i));
                }
                fprintf(stdout, "\n");
        }
        return;
}

static void show_cc(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(0 == rslt->CC_lost)
        {
                return;
        }

        print_atp_value(obj);
        fprintf(stdout, "%X, %X, %2u +16n, \n",
                rslt->CC_wait,
                rslt->CC_find,
                rslt->CC_lost);
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
        fprintf(stdout, "%llu, %llu, %u, \n",
                rslt->PCR,
                rslt->PCR_base,
                rslt->PCR_ext);
        return;
}

static void show_ptsdts(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(rslt->has_PTS)
        {
                print_atp_value(obj);
                fprintf(stdout, "%llu, ", rslt->PTS);

                if(rslt->has_DTS)
                {
                        fprintf(stdout, "%llu, \n", rslt->DTS);
                }
                else
                {
                        fprintf(stdout, ", \n");
                }
        }
        return;
}

static void show_pes(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(0 != rslt->PES_len)
        {
                b2t(obj->tbuf, rslt->PES_buf, rslt->PES_len, ' ');
                puts(obj->tbuf);
        }
        return;
}

static void show_es(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(0 != rslt->ES_len)
        {
                b2t(obj->tbuf, rslt->ES_buf, rslt->ES_len, ' ');
                puts(obj->tbuf);
        }
        return;
}

static void print_atp_title()
{
        fprintf(stdout, "yyyy-mm-dd hh:mm:ss, address(byte), address(byte), time(ns), PID, ");
        return;
}

static void print_atp_value(obj_t *obj)
{
        ts_rslt_t *rslt;
        time_t tp;
        struct tm *lt; // local time
        char stime[32];

        rslt = obj->rslt;

        time(&tp);
        lt = localtime(&tp);
        strftime(stime, 32, "%Y-%m-%d %H:%M:%S", lt);

        fprintf(stdout, "%s, 0x%llX, %lld, %llu, 0x%04X, ",
                stime, rslt->addr,
                rslt->addr, rslt->time,
                rslt->pid);
        return;
}

//=============================================================================
// THE END.
//=============================================================================
