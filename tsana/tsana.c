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
#include <signal.h> // for signal()

#include "error.h"
#include "url.h"
#include "ts.h"

#define BIT(n)                          (1<<(n))

#define MIN_USER_PID                    0x0020
#define MAX_USER_PID                    0x1FFE

//=============================================================================
// struct definition
//=============================================================================
typedef struct
{
        char file[FILENAME_MAX]; // input file name without postfix

        char file_i[FILENAME_MAX];
        URL *url;
        uint32_t addr; // addr in input file

        char file_o[FILENAME_MAX];
        FILE *fd_o;

        int mode;
        int state;

        char line[204];
        uint32_t ts_size;

        int ts_id;
        ts_rslt_t *rslt;
}
obj_t;

//=============================================================================
// enum definition
//=============================================================================
enum
{
        MODE_DEBUG,
        MODE_PSI,
        MODE_PCR,
        MODE_CC
};

enum
{
        STATE_PARSE_EACH,
        STATE_PARSE_PSI,
        STATE_EXIT
};

//=============================================================================
// variable definition
//=============================================================================
static obj_t *obj = NULL;

//=============================================================================
// sub-function declaration
//=============================================================================
static void sigfunc(int);

static void state_parse_psi(obj_t *obj);
static void state_parse_each(obj_t *obj);

static obj_t *create(int argc, char *argv[]);
static int delete(obj_t *obj);

static void show_help();
static void show_version();

static void sync_input(obj_t *obj);
static int get_one_pkg(obj_t *obj);

static void show_pids(obj_t *obj);
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
        obj = create(argc, argv);

        if(signal(SIGINT, sigfunc) == SIG_ERR)
        {
                printf("Could not setup signal handler for SIGINT!\n");
                return -1;
        }

        sync_input(obj);
        while(STATE_EXIT != obj->state && get_one_pkg(obj))
        {
                tsParseTS(obj->ts_id, obj->line);
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
                                printf("Wrong state!\n");
                                obj->state = STATE_EXIT;
                                break;
                }
                obj->addr += obj->ts_size;
        }

        if(STATE_PARSE_PSI == obj->state)
        {
                printf("PSI parsing unfinished!\n");
                show_pids(obj);
        }
        
        delete(obj);
        exit(EXIT_SUCCESS);
}

//=============================================================================
// sub-function definition
//=============================================================================
static void sigfunc(int signo)
{
        char str[100];

        if(SIGINT == signo)
        {
                //printf("SIGINT(Ctrl-C) catched.\n");
                obj->state = STATE_EXIT;
        }
        else
        {
                fread(str, 1, 100, stdin); // empty stdin buffer
        }
}

static void state_parse_psi(obj_t *obj)
{
        ts_rslt_t *rslt = obj->rslt;

        if(rslt->pid == rslt->concerned_pid)
        {
                tsParseOther(obj->ts_id);
        }

        if(rslt->is_psi_parsed)
        {
                switch(obj->mode)
                {
                        case MODE_PSI:
                                show_pids(obj);
                                obj->state = STATE_EXIT;
                                break;
                        case MODE_CC:
                                sync_input(obj);
                                obj->addr -= obj->ts_size;
                                if(PRTCL_UDP == obj->url->protocol)
                                {
                                        printf("year-mm-dd HH:MM:SS,");
                                }
                                else
                                {
                                        printf("address(X),address(d),");
                                }
                                printf("   PID,wait,find,lost\n");
                                break;
                        case MODE_PCR:
                                sync_input(obj);
                                obj->addr -= obj->ts_size;
                                printf("address(X),address(d),   PID,          PCR,  PCR_BASE,PCR_EXT\n");
                                break;
                        default:
                                obj->state = STATE_EXIT;
                                break;
                }
                obj->state = STATE_PARSE_EACH;
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
                if(PRTCL_UDP == obj->url->protocol)
                {
                        printf("%s,", strtime);
                }
                else
                {
                        printf("0x%08lX", obj->addr);
                        printf(",%10lu", obj->addr);
                }
                printf("0x%04X", rslt->pid);
                printf(",  %2u,  %2u,  %2d\n",
                       rslt->CC.wait,
                       rslt->CC.find,
                       rslt->CC.lost);
        }

        if(MODE_PCR == obj->mode && rslt->PCR.is_pcr_pid)
        {
                if(PRTCL_UDP == obj->url->protocol)
                {
                        printf("%s,", strtime);
                }
                else
                {
                        printf("0x%08lX", obj->addr);
                        printf(",%10lu", obj->addr);
                }
                printf(",0x%04X", rslt->pid);
                printf(",%13llu,%10llu,    %3u\n",
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

        if(1 == argc)
        {
                // no parameter
                printf("tsana: no input files, try --help\n");
                exit(EXIT_SUCCESS);
        }

        obj = (obj_t *)malloc(sizeof(obj_t));
        if(NULL == obj)
        {
                DBG(ERR_MALLOC_FAILED);
                return NULL;
        }

        obj->mode = MODE_PSI;
        strcpy(obj->file_i, "unknown.ts");
        obj->ts_size = 188;

        for(i = 1; i < argc; i++)
        {
                if ('-' == argv[i][0])
                {
                        if (0 == strcmp(argv[i], "-o"))
                        {
                                strcpy(obj->file_o, argv[++i]);
                        }
                        else if (0 == strcmp(argv[i], "-n"))
                        {
                                sscanf(argv[++i], "%i" , &dat);
                                obj->ts_size = (204 == dat) ? 204 : 108;
                        }
                        else if (0 == strcmp(argv[i], "-psi"))
                        {
                                obj->mode = MODE_PSI;
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
                                printf("Wrong parameter: %s\n", argv[i]);
                                exit(EXIT_FAILURE);
                        }
                }
                else
                {
                        strcpy(obj->file_i, argv[i]);
                }
        }

        // make output file name with input file name
        if('\0' == obj->file_o[0])
        {
                // search last '.'
                i = strlen(obj->file_i) - 1;
                while(i >= 0)
                {
                        if('.' == obj->file_i[i]) break;
                        i--;
                }
                // fill file_o[] and file[]
                obj->file[i--] = '\0';
                obj->file_o[i--] = '\0';
                while(i >= 0)
                {
                        obj->file[i] = obj->file_i[i];
                        obj->file_o[i] = obj->file_i[i];
                        i--;
                }
                strcat(obj->file_o, "2.ts");
        }

        if(NULL == (obj->url = url_open(obj->file_i, "rb")))
        {
                exit(EXIT_FAILURE);
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
                url_close(obj->url);
                tsDelete(obj->ts_id);
                free(obj);

                return 1;
        }
}

static void show_help()
{
        printf("Usage: tsana [options] URL [options]\n");
        printf("URL:\n");
        printf("  0.             [E:\\|\\]path\\filename\n");
        printf("  1.             [file://][E:][/]path/filename\n");
        printf("  2.             udp://@[IP]:port\n");
        printf("Options:\n");
        printf("  -n <num>       Size of TS package, default: 188\n");
        printf("  -o <file>      Output file name, default: *2.ts\n");
        printf("  -psi           Show PSI information\n");
        printf("  -cc            Check Continuity Counter\n");
        printf("  -pcr           Show all PCR value\n");
        printf("  -debug         Show all errors found\n");
        printf("  -flt <pid>     Filter package with <pid>\n");
        printf("  -pes <pid>     Output PES file with <pid>\n");
        printf("  --help         Display this information\n");
        printf("  --version      Display my version\n");
}

static void show_version()
{
        printf("tsana 0.1.0 (by Cygwin), %s %s\n", __TIME__, __DATE__);
        printf("Copyright (C) 2009 ZHOU Cheng.\n");
        printf("This is free software; contact author for additional information.\n");
        printf("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR\n");
        printf("A PARTICULAR PURPOSE.\n");
}

static void sync_input(obj_t *obj)
{
        int sync_byte;
        URL *url = obj->url;

        obj->addr = 0;
        url_seek(url, 0, SEEK_SET);
        do
        {
                if(EOF == (sync_byte = url_getc(obj->url)))
                {
                        break;
                }
                else if(0x47 == sync_byte)
                {
                        url_seek(url, obj->ts_size - 1, SEEK_CUR);
                        if(EOF == (sync_byte = url_getc(obj->url)))
                        {
                                break;
                        }
                        else if(0x47 == sync_byte)
                        {
                                // sync, go back
                                url_seek(url, -(obj->ts_size + 1), SEEK_CUR);
                                break;
                        }
                        else
                        {
                                // not real sync byte
                                url_seek(url, -(obj->ts_size), SEEK_CUR);
                        }
                }
                else
                {
                        (obj->addr)++;
                }
        }while(1);

        if(0 != obj->addr)
        {
                printf("Find first sync byte at 0x%lX in %s.\n",
                       obj->addr, obj->file_i);
        }
}

static int get_one_pkg(obj_t *obj)
{
        return(obj->ts_size == url_read(obj->line, 1, obj->ts_size, obj->url));
}

static void show_pids(obj_t *obj)
{
        ts_pid_t *pids;
        struct NODE *node;

        printf("PID LIST(%d-item):\n\n", list_count(obj->rslt->pid_list));
        printf("-PID--, CC, dCC, --type--, abbr, detail\n");

        node = obj->rslt->pid_list->head;
        while(node)
        {
                pids = (ts_pid_t *)node;
                printf("0x%04X, %2u,  %u , %s, %s\n",
                       pids->PID,
                       pids->CC,
                       pids->dCC,
                       //pid_type_str[pids->type],
                       pids->sdes,
                       pids->ldes);
                node = node->next;
        }
}

#if 0
static void show_TS(obj_t *obj)
{
        ts_t *ts = obj->ts;

        printf("TS:\n");
        printf("  0x%02X  : sync_byte\n", ts->sync_byte);
        printf("  %c     : transport_error_indicator\n", (ts->transport_error_indicator) ? '1' : '0');
        printf("  %c     : payload_unit_start_indicator\n", (ts->payload_unit_start_indicator) ? '1' : '0');
        printf("  %c     : transport_priority\n", (ts->transport_priority) ? '1' : '0');
        printf("  0x%04X: PID\n", ts->PID);
        printf("  0b%s  : transport_scrambling_control\n", printb(ts->transport_scrambling_control, 2));
        printf("  0b%s  : adaption_field_control\n", printb(ts->adaption_field_control, 2));
        printf("  0x%X   : continuity_counter\n", ts->continuity_counter);
}

static void show_PSI(obj_t *obj)
{
        uint32_t idx;
        psi_t *psi = obj->psi;

        printf("0x0000: PAT_PID\n");
        //printf("    0x%04X: section_length\n", psi->section_length);
        printf("    0x%04X: transport_stream_id\n", psi->idx.idx);

        idx = 0;
#if 0
        while(idx < psi->program_cnt)
        {
                if(0x0000 == psi->program[idx].program_number)
                {
                        printf("    0x%04X: network_program_number\n",
                               psi->program[idx].program_number);
                        printf("        0x%04X: network_PID\n",
                               psi->program[idx].PID);
                }
                else
                {
                        printf("    0x%04X: program_number\n",
                               psi->program[idx].program_number);
                        printf("        0x%04X: PMT_PID\n",
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
                        printf("0x%04X: network_PID\n",
                               pmt->PID);
                        //printf("    omit...\n");
                }
                else
                {
                        // normal program
                        printf("0x%04X: PMT_PID\n",
                               pmt->PID);
                        //printf("    0x%04X: section_length\n",
                        //       pmt->section_length);
                        printf("    0x%04X: program_number\n",
                               pmt->program_number);
                        printf("    0x%04X: PCR_PID\n",
                               pmt->PCR_PID);
                        //printf("    0x%04X: program_info_length\n",
                        //       pmt->program_info_length);
                        if(0 != pmt->program_info_length)
                        {
                                //printf("        omit...\n");
                        }

                        i = 0;
                        while(i < pmt->pid_cnt)
                        {
                                printf("    0x%04X: %s\n",
                                       pmt->pid[i].PID,
                                       PID_TYPE_STR[stream_type(pmt->pid[i].stream_type)]);
                                printf("        0x%04X: %s\n",
                                       pmt->pid[i].stream_type,
                                       stream_type_str(pmt->pid[i].stream_type));
                                //printf("        0x%04X: ES_info_length\n",
                                //       pmt->pid[i].ES_info_length);
                                if(0 != pmt->pid[i].ES_info_length)
                                {
                                        //printf("            omit...\n");
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
