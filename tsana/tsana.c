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
#include "ts.h"

//=============================================================================
// struct definition
//=============================================================================
typedef struct
{
        int mode;
        int state;

        int is_need_time;
        int is_outpsi; // output txt psi package to stdout
        int is_prepsi; // get psi information from file first

        uint32_t ts_size;
        uint8_t bbuf[204];
        uint8_t tbuf[1024];
        uint32_t addr; // addr in input

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

// for high speed txt to bin convert
static const uint8_t t2b_table_h[256] =
{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x0X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x1X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x2X
        0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x3X
        0x00, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x4X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x5X
        0x00, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x6X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x7X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x8X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x9X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xAX
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xBX
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xCX
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xDX
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xEX
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 0xFX
};

static const uint8_t t2b_table_l[256] =
{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x0X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x1X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x2X
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x3X
        0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x4X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x5X
        0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x6X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x7X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x8X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x9X
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xAX
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xBX
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xCX
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xDX
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xEX
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 0xFX
};

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

#if 0
static void show_TS(obj_t *obj);
static void show_PSI(obj_t *obj);
static void show_PMT(obj_t *obj);
static char *printb(uint32_t x, int bit_cnt);
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
                        continue;
                }
                tsParseTS(obj->ts_id, obj->bbuf);
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
                                fprintf(stdout, "address(X),address(d),   PID,          PCR,  PCR_BASE,PCR_EXT\n");
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
        ts_rslt_t *rslt = obj->rslt;
        time_t tp;
        struct tm *lt; // local time
        char strtime[255];

        time(&tp);
        lt = localtime(&tp);
        strftime(strtime, 255, "%Y-%m-%d %H:%M:%S", lt);

        tsParseOther(obj->ts_id);

        if(MODE_CC == obj->mode && 0 != rslt->CC.lost)
        {
                if(obj->is_need_time)
                {
                        fprintf(stdout, "%s,", strtime);
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
        }

        if(MODE_PCR == obj->mode && rslt->PCR.is_pcr_pid)
        {
                if(obj->is_need_time)
                {
                        fprintf(stdout, "%s,", strtime);
                }
                else
                {
                        fprintf(stdout, "0x%08lX", obj->addr);
                        fprintf(stdout, ",%10lu", obj->addr);
                }
                fprintf(stdout, ",0x%04X", rslt->pid);
                fprintf(stdout, ",%13llu,%10llu,    %3u\n",
                        rslt->PCR.pcr,
                        rslt->PCR.pcr_base,
                        rslt->PCR.pcr_ext);
        }
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
                        if (0 == strcmp(argv[i], "-pid"))
                        {
                                obj->mode = MODE_PID;
                        }
                        else if (0 == strcmp(argv[i], "-psi"))
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
                        else if (0 == strcmp(argv[i], "-pes"))
                        {
                                sscanf(argv[++i], "%i" , &dat);
                                //obj->pid = (uint16_t)(dat & 0x1FFF);
                        }
                        else if (0 == strcmp(argv[i], "-flt"))
                        {
                                sscanf(argv[++i], "%i" , &dat);
                                //obj->pid = (uint16_t)(dat & 0x1FFF);
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
        puts("Usage: tsana [option]");
        puts("Options:");
        puts("  -pid           Show PID list information, default option");
        puts("  -psi           Show PSI tree information");
        puts("  -outpsi        Output PSI package");
        puts("  -prepsi <file> Get PSI information from <file> first");
        puts("  -cc            Check Continuity Counter");
        puts("  -pcr           Show all PCR value");
        puts("  -debug         Show all errors found");
        puts("  -flt <pid>     Filter package with <pid>");
        puts("  -pes <pid>     Output PES file with <pid>");
        puts("  --help         Display this information");
        puts("  --version      Display my version");
}

static void show_version()
{
        fprintf(stdout, "tsana 0.1.0 (by Cygwin), %s %s\n", __TIME__, __DATE__);
        puts("Copyright (C) 2009 ZHOU Cheng.");
        puts("This is free software; contact author for additional information.");
        puts("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR");
        puts("A PARTICULAR PURPOSE.");
}

static int get_one_pkg(obj_t *obj)
{
        int b, t;
        char *rslt;

        rslt = fgets(obj->tbuf, 1000, stdin);
        if(NULL == rslt)
        {
                return GOT_EOF;
        }

        //puts(obj->tbuf);
        for(b = 0, t = 0; ; b++, t++)
        {
                uint8_t h, l;

                h = obj->tbuf[t++];
                if('\0' == h || 0x0a == h || 0x0d == h) break;

                l = obj->tbuf[t++];
                if('\0' == l || 0x0a == l || 0x0d == l) break;

                obj->bbuf[b] = t2b_table_h[h] | t2b_table_l[l];
                //printf("%02X ", (int)obj->bbuf[b]);
        }
        obj->tbuf[t-1] = '\0';

        if(b < 188)
        {
                return GOT_WRONG_PKG;
        }
        else
        {
                return GOT_RIGHT_PKG;
        }
}

static void show_pids(struct LIST *list)
{
        struct NODE *node;
        ts_pid_t *pids;

        //fprintf(stdout, "pid_list(%d-item):\n\n", list_count(list));
        fprintf(stdout, "-PID--, CC, dCC, --type--, abbr, detail\n");

        for(node = list->head; node; node = node->next)
        {
                pids = (ts_pid_t *)node;
                fprintf(stdout, "0x%04X, %2u,  %u , %s, %s, %s\n",
                        pids->PID,
                        pids->CC,
                        pids->dCC,
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
                for(i = 0; i < prog->program_info_length; i++)
                {
                        fprintf(stdout, " %02X", prog->program_info[i]);
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
                for(i = 0; i < track->es_info_length; i++)
                {
                        fprintf(stdout, " %02X", track->es_info[i]);
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

static void show_PSI(obj_t *obj)
{
        uint32_t idx;
        psi_t *psi = obj->psi;

        fprintf(stdout, "0x0000: PAT_PID\n");
        //fprintf(stdout, "    0x%04X: section_length\n", psi->section_length);
        fprintf(stdout, "    0x%04X: transport_stream_id\n", psi->idx.idx);

        idx = 0;
#if 0
        while(idx < psi->program_cnt)
        {
                if(0x0000 == psi->program[idx].program_number)
                {
                        fprintf(stdout, "    0x%04X: network_program_number\n",
                                psi->program[idx].program_number);
                        fprintf(stdout, "        0x%04X: network_PID\n",
                                psi->program[idx].PID);
                }
                else
                {
                        fprintf(stdout, "    0x%04X: program_number\n",
                                psi->program[idx].program_number);
                        fprintf(stdout, "        0x%04X: PMT_PID\n",
                                psi->program[idx].PID);
                }
                idx++;
        }
#endif
}

static void show_PMT(obj_t *obj)
{
        uint32_t i;
        uint32_t idx;
        psi_t *psi = obj->psi;
        struct PMT *pmt;

        for(idx = 0; idx < psi->program_cnt; idx++)
        {
                pmt = obj->pmt[idx];
                if(0x0000 == pmt->program_number)
                {
                        // network information
                        fprintf(stdout, "0x%04X: network_PID\n",
                                pmt->PID);
                        //fprintf(stdout, "    omit...\n");
                }
                else
                {
                        // normal program
                        fprintf(stdout, "0x%04X: PMT_PID\n",
                                pmt->PID);
                        //fprintf(stdout, "    0x%04X: section_length\n",
                        //       pmt->section_length);
                        fprintf(stdout, "    0x%04X: program_number\n",
                                pmt->program_number);
                        fprintf(stdout, "    0x%04X: PCR_PID\n",
                                pmt->PCR_PID);
                        //fprintf(stdout, "    0x%04X: program_info_length\n",
                        //       pmt->program_info_length);
                        if(0 != pmt->program_info_length)
                        {
                                //fprintf(stdout, "        omit...\n");
                        }

                        i = 0;
                        while(i < pmt->pid_cnt)
                        {
                                fprintf(stdout, "    0x%04X: %s\n",
                                        pmt->pid[i].PID,
                                        PID_TYPE_STR[stream_type(pmt->pid[i].stream_type)]);
                                fprintf(stdout, "        0x%04X: %s\n",
                                        pmt->pid[i].stream_type,
                                        stream_type_str(pmt->pid[i].stream_type));
                                //fprintf(stdout, "        0x%04X: ES_info_length\n",
                                //       pmt->pid[i].ES_info_length);
                                if(0 != pmt->pid[i].ES_info_length)
                                {
                                        //fprintf(stdout, "            omit...\n");
                                }

                                i++;
                        }
                }
        }
}

static char *printb(uint32_t x, int bit_cnt)
{
        static char str[64];

        char *p = str;
        uint32_t mask;

        if(bit_cnt < 1 || bit_cnt > 32)
        {
                *p++ = 'e';
                *p++ = 'r';
                *p++ = 'r';
                *p++ = 'o';
                *p++ = 'r';
        }
        else
        {
                mask = (1 << (bit_cnt - 1));
                while(mask)
                {
                        *p++ = (x & mask) ? '1' : '0';
                        mask >>= 1;
                }
        }
        *p = '\0';

        return str;
}
#endif
//=============================================================================
// THE END.
//=============================================================================
