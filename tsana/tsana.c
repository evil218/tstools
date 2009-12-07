/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: tsana.c
// Purpose: analyse certain character with ts file
// To build: gcc -std=c99 -o tsana.exe tsana.c
// Copyright (C) 2009 by ZHOU Cheng. All right reserved.
//=============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strcmp, etc
#include <time.h>   // for time(), etc
#include <stdint.h> // for uint?_t, etc
#include <signal.h> // for signal()

#include "list.h"
#include "url.h"

#define BIT(n)                          (1<<(n))

#define MIN_USER_PID                    0x0020
#define MAX_USER_PID                    0x1FFE

//=============================================================================
// enum definition:
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
        STATE_NEXT_PKG_PCR,
        STATE_NEXT_PKG_CC,
        STATE_NEXT_PMT,
        STATE_NEXT_PAT,
        STATE_EXIT
};

//=============================================================================
// struct definition:
//=============================================================================
typedef struct
{
        uint32_t sync_byte:8;
        uint32_t transport_error_indicator:1;
        uint32_t payload_unit_start_indicator:1;
        uint32_t transport_priority:1;
        uint32_t PID:13;
        uint32_t transport_scrambling_control:2;
        uint32_t adaption_field_control:2;
        uint32_t continuity_counter:4;
}
ts_t;

typedef struct
{
        uint32_t adaption_field_length:8;
        uint32_t discontinuity_indicator:1;
        uint32_t random_access_indicator:1;
        uint32_t elementary_stream_priority_indicator:1;
        uint32_t PCR_flag:1;
        uint32_t OPCR_flag:1;
        uint32_t splicing_pointer_flag:1;
        uint32_t transport_private_data_flag:1;
        uint32_t adaption_field_extension_flag:1;
        uint64_t program_clock_reference_base:33;
        uint32_t reserved:6;
        uint32_t program_clock_reference_extension:9;
}
af_t;

typedef struct
{
        uint32_t table_id:8; // TABLE_ID_XXX
        uint32_t sectin_syntax_indicator:1;
        uint32_t pad0:1; // '0'
        uint32_t reserved0:2;
        uint32_t section_length:12;
        union
        {
                uint32_t idx:16;
                uint32_t transport_stream_id:16;
                uint32_t program_number:16;
        }idx;
        uint32_t reserved1:2;
        uint32_t version_number:5;
        uint32_t current_next_indicator:1;
        uint32_t section_number:8;
        uint32_t last_section_number:8;

        uint32_t CRC3:8; // most significant byte
        uint32_t CRC2:8;
        uint32_t CRC1:8;
        uint32_t CRC0:8; // last significant byte
}
psi_t;

typedef struct
{
        struct NODE *next;
        struct NODE *prev;

        uint16_t PID:13;
        int type; // enum pid_type
        const char *sdes; // short description
        const char *ldes; // long description

        // for check continuity_counter
        uint32_t CC:4;
        uint32_t dCC:4; // 0 or 1
        int is_CC_sync;
}
pids_t;

typedef struct
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t PID:13;
        int stream_type;
        int type; // enum pid_type
}
track_t;

typedef struct
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t program_number:16;
        uint32_t PMT_PID:13;
        uint32_t PCR_PID:13;

        struct LIST *track;
        int parsed;
}
prog_t;

typedef struct
{
        uint16_t min;   // PID range
        uint16_t max;   // PID range
        int dCC;        // delta of CC field: 0 or 1
        char *sdes;     // short description
        char *ldes;     // long description
}
ts_pid_t;

typedef struct
{
        uint8_t min;    // table ID range
        uint8_t max;    // table ID range
        char *sdes;     // short description
        char *ldes;     // long description
}
table_id_t;

typedef struct
{
        char file[FILENAME_MAX]; // input file name without postfix

        char file_i[FILENAME_MAX];
        URL *url;
        uint32_t addr; // addr in input file

        char file_o[FILENAME_MAX];
        FILE *fd_o;

        uint32_t ts_size; // 188 or 204
        uint8_t line[204]; // one TS package
        uint8_t *p; // point to current data in line

        ts_t *ts;
        af_t *af;
        psi_t *psi;
        uint16_t left_length;

        struct LIST *prog;
        struct LIST *pids;

        int mode;
        int state;
}
obj_t;

//=============================================================================
// const definition:
//=============================================================================
const ts_pid_t TS_PID[] =
{
        {0x0000, 0x0000, 1, " PAT", "program association section"},
        {0x0001, 0x0001, 1, " CAT", "conditional access section"},
        {0x0002, 0x0002, 1, "TSDT", "transport stream description section"},
        {0x0003, 0x000f, 0, "____", "reserved"},
        {0x0010, 0x0010, 1, " NIT", "NIT/ST, network information section"},
        {0x0011, 0x0011, 1, " SDT", "SDT/BAT/ST, service description section"},
        {0x0012, 0x0012, 1, " EIT", "EIT/ST, event information section"},
        {0x0013, 0x0013, 1, " RST", "RST/ST, running status section"},
        {0x0014, 0x0014, 1, " TDT", "TDT/TOT/ST, time data section"},
        {0x0015, 0x0015, 1, "____", "Network Synchroniztion"},
        {0x0016, 0x001b, 1, "____", "Reserved for future use"},
        {0x001c, 0x001c, 1, "____", "Inband signaling"},
        {0x001d, 0x001d, 1, "____", "Measurement"},
        {0x001e, 0x001e, 1, " DIT", "discontinuity information section"},
        {0x001f, 0x001f, 1, " SIT", "selection information section"},
        {0x0020, 0x1ffe, 1, "____", "T.B.D."},
        {0x1fff, 0x1fff, 0, "NULL", "empty package"}
};

const table_id_t TABLE_ID[] =
{
        /* 0*/{0x00, 0x00, " PAT", "program association section"},
        /* 1*/{0x01, 0x01, " CAT", "conditional access section"},
        /* 2*/{0x02, 0x02, " PMT", "program map section"},
        /* 3*/{0x03, 0x03, "TSDT", "transport stream description section"},
        /* 4*/{0x04, 0x3f, "????", "reserved"},
        /* 5*/{0x40, 0x40, " NIT", "network information section-actual network"},
        /* 6*/{0x41, 0x41, " NIT", "network information section-other network"},
        /* 7*/{0x42, 0x42, " SDT", "service description section-actual transport stream"},
        /* 8*/{0x43, 0x45, "????", "reserved for future use"},
        /* 9*/{0x46, 0x46, " SDT", "service description section-other transport stream"},
        /*10*/{0x47, 0x49, "????", "reserved for future use"},
        /*11*/{0x4a, 0x4a, " BAT", "bouquet association section"},
        /*12*/{0x4b, 0x4d, "????", "reserved for future use"},
        /*13*/{0x4e, 0x4e, " EIT", "event information section-actual transport stream,P/F"},
        /*14*/{0x4f, 0x4f, " EIT", "event information section-other transport stream,P/F"},
        /*15*/{0x50, 0x5f, " EIT", "event information section-actual transport stream,schedule"},
        /*16*/{0x60, 0x6f, " EIT", "event information section-other transport stream,schedule"},
        /*17*/{0x70, 0x70, " TDT", "time data section"},
        /*18*/{0x71, 0x71, " RST", "running status section"},
        /*19*/{0x72, 0x72, "  ST", "stuffing section"},
        /*20*/{0x73, 0x73, " TOT", "time offset section"},
        /*21*/{0x74, 0x7d, "????", "reserved for future use"},
        /*22*/{0x7e, 0x7e, " DIT", "discontinuity information section"},
        /*23*/{0x7f, 0x7f, " SIT", "selection information section"},
        /*24*/{0x80, 0xfe, "????", "user defined"},
        /*25*/{0xff, 0xff, "????", "reserved"}
};

enum pid_type
{
        PAT_PID,
        CAT_PID,
        PMT_PID,
        TSDT_PID,
        NIT_PID,
        SDT_PID,
        BAT_PID,
        EIT_PID,
        TDT_PID,
        RST_PID,
        ST_PID,
        TOT_PID,
        DIT_PID,
        SIT_PID,
        PCR_PID,
        VID_PID,
        VID_PCR,
        AUD_PID,
        AUD_PCR,
        NULL_PID,
        UNO_PID
};

const char *pid_type_str[] =
{
        " PAT_PID",
        " CAT_PID",
        " PMT_PID",
        "TSDT_PID",
        " NIT_PID",
        " SDT_PID",
        " BAT_PID",
        " EIT_PID",
        " TDT_PID",
        " RST_PID",
        " ST_PID",
        " TOT_PID",
        " DIT_PID",
        " SIT_PID",
        " PCR_PID",
        " VID_PID",
        " VID_PCR",
        " AUD_PID",
        " AUD_PCR",
        "NULL_PID",
        " UNO_PID"
};

//=============================================================================
// Variables definition:
//=============================================================================
static obj_t *obj = NULL;
//static ts_t *ts;
//static af_t *af;
//static psi_t *psi;

//=============================================================================
// Sub-function declare:
//=============================================================================
static void sigfunc(int);

static void state_next_pat(obj_t *obj);
static void state_next_pmt(obj_t *obj);
static void state_next_pkg_cc(obj_t *obj);
static void state_next_pkg_pcr(obj_t *obj);

static obj_t *create(int argc, char *argv[]);
static int delete(obj_t *obj);

static void *malloc_mem(int size, char *memo);
static void show_help();
static void show_version();

static void sync_input(obj_t *obj);
static int get_one_pkg(obj_t *obj);
static void parse_TS(obj_t *obj);
static void parse_AF(obj_t *obj); // adaption_fields
static void parse_PSI(obj_t *obj);
static void parse_PAT_load(obj_t *obj);
static int is_PMT_PID(obj_t *obj);
static int is_unparsed_PROG(obj_t *obj);
static int is_all_PROG_parsed(obj_t *obj);
static void parse_PMT_load(obj_t *obj);
static int PID_type(uint8_t st);
static void pids_add(struct LIST *list, pids_t *pids);
static pids_t *pids_match(struct LIST *list, uint16_t pid);
static void show_pids(obj_t *obj);
static int search_in_TS_PID(uint16_t pid);
#if 0
static void show_TS(obj_t *obj);
static void show_PSI(obj_t *obj);
static void show_PMT(obj_t *obj);
static char *printb(uint32_t x, int bit_cnt);
#endif

//=============================================================================
// The main function:
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
                parse_TS(obj);
                switch(obj->state)
                {
                        case STATE_NEXT_PAT:
                                state_next_pat(obj);
                                break;
                        case STATE_NEXT_PMT:
                                state_next_pmt(obj);
                                break;
                        case STATE_NEXT_PKG_CC:
                                state_next_pkg_cc(obj);
                                break;
                        case STATE_NEXT_PKG_PCR:
                                state_next_pkg_pcr(obj);
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

        if(     STATE_NEXT_PAT == obj->state ||
                STATE_NEXT_PMT == obj->state
        )
        {
                printf("PSI parsing unfinished!\n");
                show_pids(obj);
        }
        
        delete(obj);
        exit(EXIT_SUCCESS);
}

//=============================================================================
// Subfunctions definition:
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

static void state_next_pat(obj_t *obj)
{
        ts_t *ts = obj->ts;

        if(0x0000 == ts->PID)
        {
                parse_PSI(obj);
                parse_PAT_load(obj);
                obj->state = STATE_NEXT_PMT;
        }
}

static void state_next_pmt(obj_t *obj)
{
        if(!is_PMT_PID(obj))
        {
                return;
        }
        parse_PSI(obj);
        if(is_unparsed_PROG(obj))
        {
                parse_PMT_load(obj);
        }
        if(is_all_PROG_parsed(obj))
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
                                obj->state = STATE_NEXT_PKG_CC;
                                break;
                        case MODE_PCR:
                                sync_input(obj);
                                obj->addr -= obj->ts_size;
                                printf("address(X),address(d),   PID,          PCR,  PCR_BASE,PCR_EXT\n");
                                obj->state = STATE_NEXT_PKG_PCR;
                                break;
                        default:
                                obj->state = STATE_EXIT;
                                break;
                }
        }
}

static void state_next_pkg_cc(obj_t *obj)
{
        ts_t *ts = obj->ts;
        pids_t *pids;
        time_t tp;
        struct tm *lt; // local time
        char strtime[255];

        time(&tp);
        lt = localtime(&tp);
        strftime(strtime, 255, "%Y-%m-%d %H:%M:%S", lt);

        pids = pids_match(obj->pids, ts->PID);
        if(NULL == pids)
        {
                if(MIN_USER_PID <= ts->PID && ts->PID <= MAX_USER_PID)
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
                        printf("0x%04X", ts->PID);
                        printf(", Unknown PID!\n");
                }
                else
                {
                        // reserved PID
                }
        }
        else if(pids->is_CC_sync)
        {
                pids->CC += pids->dCC;
                if(pids->CC != ts->continuity_counter)
                {
                        int lost;

                        lost = (int)ts->continuity_counter;
                        lost -= (int)pids->CC;
                        if(lost < 0)
                        {
                                lost += 16;
                        }
                        if(PRTCL_UDP == obj->url->protocol)
                        {
                                printf("%s,", strtime);
                        }
                        else
                        {
                                printf("0x%08lX", obj->addr);
                                printf(",%10lu", obj->addr);
                        }
                        printf("0x%04X", ts->PID);
                        printf(",  %2u,  %2u,  %2d\n",
                               pids->CC,
                               ts->continuity_counter,
                               lost);
                        pids->CC = ts->continuity_counter;
                }
        }
        else
        {
                pids->CC = ts->continuity_counter;
                pids->is_CC_sync = 1;
        }
}

static void state_next_pkg_pcr(obj_t *obj)
{
        ts_t *ts = obj->ts;
        af_t *af = obj->af;
        time_t tp;
        struct tm *lt; // local time
        char strtime[255];

        time(&tp);
        lt = localtime(&tp);
        strftime(strtime, 255, "%Y-%m-%d %H:%M:%S", lt);

        if(     (BIT(1) & ts->adaption_field_control) &&
                (0x00 != af->adaption_field_length) &&
                af->PCR_flag
        )
        {
                uint64_t pcr;
                pcr = af->program_clock_reference_base;
                pcr *= 300;
                pcr += af->program_clock_reference_extension;

                if(PRTCL_UDP == obj->url->protocol)
                {
                        printf("%s,", strtime);
                }
                else
                {
                        printf("0x%08lX", obj->addr);
                        printf(",%10lu", obj->addr);
                }
                printf(",0x%04X", ts->PID);
                printf(",%13llu,%10llu,    %3u\n",
                       pcr,
                       af->program_clock_reference_base,
                       af->program_clock_reference_extension);
        }
}

static obj_t *create(int argc, char *argv[])
{
        int i;
        int dat;
        obj_t *obj;
        pids_t *pids;

        obj = (obj_t *)malloc_mem(sizeof(obj_t), "creat object");

        obj->ts = (ts_t *)malloc_mem(sizeof(ts_t), "creat TS struct");
        obj->af = (af_t *)malloc_mem(sizeof(af_t), "creat AF struct");
        obj->psi = (psi_t *)malloc_mem(sizeof(psi_t), "creat PSI struct");

        obj->ts_size = 188;
        obj->mode = MODE_PSI;
        strcpy(obj->file_i, "unknown.ts");

        obj->prog = list_init();
        obj->pids = list_init();

        // add PAT PID
        pids = (pids_t *)malloc(sizeof(pids_t));
        if(NULL == pids)
        {
                printf("Malloc memory failure!\n");
                exit(EXIT_FAILURE);
        }
        pids->PID = 0x0000;
        pids->type = PAT_PID;
        pids->sdes = TABLE_ID[0].sdes;
        pids->ldes = TABLE_ID[0].ldes;
        pids->CC = 0;
        pids->dCC = 1;
        pids->is_CC_sync = 0;
        pids_add(obj->pids, pids);

        // add NULL PID
        pids = (pids_t *)malloc(sizeof(pids_t));
        if(NULL == pids)
        {
                printf("Malloc memory failure!\n");
                exit(EXIT_FAILURE);
        }
        pids->PID = 0x1FFF;
        pids->type = NULL_PID;
        pids->sdes = "NULL";
        pids->ldes = "empty package";
        pids->CC = 0;
        pids->dCC = 0;
        pids->is_CC_sync = 1;
        pids_add(obj->pids, pids);

        if(1 == argc)
        {
                // no parameter
                printf("tsana: no input files, try --help\n");
                //show_help();
                exit(EXIT_SUCCESS);
        }
        i = 1;
        while (i < argc)
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
                i++;
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

        obj->state = STATE_NEXT_PAT;

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

                list_free(obj->prog);
                list_free(obj->pids);

                free(obj);

                return 1;
        }
}

static void *malloc_mem(int size, char *memo)
{
        void *ptr = NULL;

        ptr = (void *)malloc(size);
        if(NULL == ptr)
        {
                printf("Can not malloc %d-byte memory for %s!\n", size, memo);
                exit(EXIT_FAILURE);
        }
        return ptr;
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
        uint32_t size;

        size = url_read(obj->line, 1, obj->ts_size, obj->url);
        obj->p = obj->line;

        return (size == obj->ts_size);
}

static void parse_TS(obj_t *obj)
{
        uint8_t dat;
        ts_t *ts = obj->ts;

        dat = *(obj->p)++;
        ts->sync_byte = dat;
        if(0x47 != ts->sync_byte)
        {
                int i;

                printf("Wrong package head at 0x%lX!\n", obj->addr);
                printf("%02X ", dat);
                for(i = obj->ts_size; i > 1; i--)
                {
                        dat = *(obj->p)++;
                        printf("%02X ", dat);
                }
                printf("\n");
                exit(EXIT_FAILURE);
        }

        dat = *(obj->p)++;
        ts->transport_error_indicator = (dat & BIT(7)) >> 7;
        ts->payload_unit_start_indicator = (dat & BIT(6)) >> 6;
        ts->transport_priority = (dat & BIT(5)) >> 5;
        ts->PID = dat & 0x1F;

        dat = *(obj->p)++;
        ts->PID <<= 8;
        ts->PID |= dat;

        dat = *(obj->p)++;
        ts->transport_scrambling_control = (dat & (BIT(7) | BIT(6))) >> 6;
        ts->adaption_field_control = (dat & (BIT(5) | BIT(4))) >> 4;;
        ts->continuity_counter = dat & 0x0F;

        if(BIT(1) & ts->adaption_field_control)
        {
                parse_AF(obj);
        }

        if(BIT(0) & ts->adaption_field_control)
        {
                // data_byte
                (obj->p)++;
        }
}

static void parse_AF(obj_t *obj)
{
        uint8_t dat;
        af_t *af = obj->af;

        dat = *(obj->p)++;
        af->adaption_field_length = dat;
        if(0x00 == af->adaption_field_length)
        {
                return;
        }

        dat = *(obj->p)++;
        af->discontinuity_indicator = (dat & BIT(7)) >> 7;
        af->random_access_indicator = (dat & BIT(6)) >> 6;
        af->elementary_stream_priority_indicator = (dat & BIT(5)) >> 5;
        af->PCR_flag = (dat & BIT(4)) >> 4;
        af->OPCR_flag = (dat & BIT(3)) >> 3;
        af->splicing_pointer_flag = (dat & BIT(2)) >> 2;
        af->transport_private_data_flag = (dat & BIT(1)) >> 1;
        af->adaption_field_extension_flag = (dat & BIT(0)) >> 0;

        if(af->PCR_flag)
        {
                dat = *(obj->p)++;
                af->program_clock_reference_base = dat;

                dat = *(obj->p)++;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++;
                af->program_clock_reference_base <<= 1;
                af->program_clock_reference_base |= ((dat & BIT(7)) >> 7);
                af->program_clock_reference_extension = ((dat & BIT(0)) >> 0);

                dat = *(obj->p)++;
                af->program_clock_reference_extension <<= 8;
                af->program_clock_reference_extension |= dat;
        }
}

static void parse_PSI(obj_t *obj)
{
        uint8_t dat;
        psi_t *psi = obj->psi;

        dat = *(obj->p)++;
        psi->table_id = dat;

        dat = *(obj->p)++;
        psi->sectin_syntax_indicator = (dat & BIT(7)) >> 7;
        psi->section_length = dat & 0x0F;

        dat = *(obj->p)++;
        psi->section_length <<= 8;
        psi->section_length |= dat;
        obj->left_length = psi->section_length;

        dat = *(obj->p)++; obj->left_length--;
        psi->idx.idx = dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->idx.idx <<= 8;
        psi->idx.idx |= dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->version_number = (dat & 0x3E) >> 1;
        psi->current_next_indicator = dat & BIT(0);

        dat = *(obj->p)++; obj->left_length--;
        psi->section_number = dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->last_section_number = dat;
}

static void parse_PAT_load(obj_t *obj)
{
        int i;
        uint8_t dat;
        psi_t *psi = obj->psi;
        prog_t *prog;
        pids_t *pids;

        while(obj->left_length > 4)
        {
                // add program
                prog = (prog_t *)malloc(sizeof(prog_t));
                if(NULL == prog)
                {
                        printf("Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }
                prog->track = list_init();
                prog->parsed = 0;

                dat = *(obj->p)++; obj->left_length--;
                prog->program_number = dat;

                dat = *(obj->p)++; obj->left_length--;
                prog->program_number <<= 8;
                prog->program_number |= dat;

                dat = *(obj->p)++; obj->left_length--;
                prog->PMT_PID = dat & 0x1F;

                dat = *(obj->p)++; obj->left_length--;
                prog->PMT_PID <<= 8;
                prog->PMT_PID |= dat;

                // add PID
                pids = (pids_t *)malloc(sizeof(pids_t));
                if(NULL == pids)
                {
                        printf("Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }
                pids->PID = prog->PMT_PID;
                i = search_in_TS_PID(pids->PID);
                pids->CC = 0;
                pids->dCC = TS_PID[i].dCC;
                pids->is_CC_sync = 0;
                pids->sdes = TS_PID[i].sdes;
                pids->ldes = TS_PID[i].ldes;
                pids_add(obj->pids, pids);

                if(0 == prog->program_number)
                {
                        // network PID, not a program
                        pids->type = SIT_PID;
                        free(prog);
                }
                else
                {
                        // PMT PID
                        pids->type = PMT_PID;
                        pids->sdes = TABLE_ID[2].sdes;
                        pids->ldes = TABLE_ID[2].ldes;
                        list_add(obj->prog, (struct NODE *)prog);
                }
        }

        dat = *(obj->p)++; obj->left_length--;
        psi->CRC3 = dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->CRC2 = dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->CRC1 = dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->CRC0 = dat;

        if(0 != obj->left_length)
        {
                printf("PSI load length error!\n");
        }

        //printf("PROG Count: %u\n", list_count(obj->prog));
        //printf("PIDS Count: %u\n", list_count(obj->pids));
}

static void show_pids(obj_t *obj)
{
        pids_t *pids;
        struct NODE *node;

        printf("PID LIST(%d-item):\n\n", list_count(obj->pids));
        printf("-PID--, CC, dCC, --type--, abbr, detail\n");

        node = obj->pids->head;
        while(node)
        {
                pids = (pids_t *)node;
                printf("0x%04X, %2u,  %u , %s, %s, %s\n",
                       pids->PID,
                       pids->CC,
                       pids->dCC,
                       pid_type_str[pids->type],
                       pids->sdes,
                       pids->ldes);
                node = node->next;
        }
}

static pids_t *pids_match(struct LIST *list, uint16_t pid)
{
        struct NODE *node;
        pids_t *pids;

        node = list->head;
        while(node)
        {
                pids = (pids_t *)node;
                if(pid == pids->PID)
                {
                        return pids;
                }
                node = node->next;
        }

        return NULL;
}

static void pids_add(struct LIST *list, pids_t *the_pids)
{
        struct NODE *node;
        pids_t *pids;

        node = list->head;
        while(node)
        {
                pids = (pids_t *)node;
                if(the_pids->PID == pids->PID)
                {
                        if(PCR_PID == pids->type)
                        {
                                if(VID_PID == the_pids->type)
                                {
                                        pids->type = VID_PCR;
                                        pids->sdes = "VPCR";
                                        pids->ldes = "video package with pcr information";
                                }
                                else if(AUD_PID == the_pids->type)
                                {
                                        pids->type = AUD_PCR;
                                        pids->sdes = "APCR";
                                        pids->ldes = "audio package with pcr information";
                                }
                                else
                                {
                                        // error
                                }
                                pids->dCC = 1;
                                pids->is_CC_sync = 0;
                        }
                        else
                        {
                                // same PID, omit...
                        }
                        return;
                }
                node = node->next;
        }

        list_add(list, (struct NODE *)the_pids);
}

static int is_PMT_PID(obj_t *obj)
{
        ts_t *ts;
        struct NODE *node;
        pids_t *pids;

        ts = obj->ts;
        node = obj->pids->head;
        while(node)
        {
                pids = (pids_t *)node;
                if(ts->PID == pids->PID)
                {
                        if(PMT_PID == pids->type)
                        {
                                return 1;
                        }
                        else
                        {
                                return 0;
                        }
                }
                node = node->next;
        }
        return 0;
}

static int is_unparsed_PROG(obj_t *obj)
{
        ts_t *ts;
        psi_t *psi;
        struct NODE *node;
        prog_t *prog;

        ts = obj->ts;
        psi = obj->psi;

        node = obj->prog->head;
        while(node)
        {
                prog = (prog_t *)node;
                if(psi->idx.program_number == prog->program_number)
                {
                        if(0 == prog->parsed)
                        {
                                return 1;
                        }
                        else
                        {
                                return 0;
                        }
                }
                node = node->next;
        }
        return 0;
}

static int is_all_PROG_parsed(obj_t *obj)
{
        ts_t *ts;
        struct NODE *node;
        prog_t *prog;

        ts = obj->ts;
        node = obj->prog->head;
        while(node)
        {
                prog = (prog_t *)node;
                if(     MIN_USER_PID <= prog->PMT_PID &&
                        MAX_USER_PID >= prog->PMT_PID &&
                        0 == prog->parsed
                )
                {
                        return 0;
                }
                node = node->next;
        }
        return 1;
}

static int PID_type(uint8_t st)
{
        switch(st)
        {
                case 0x1B: // "H.264|ISO/IEC 14496-10 Video"
                case 0x01: // "ISO/IEC 11172 Video"
                case 0x02: // "H.262|ISO/IEC 13818-2 Video"
                        return VID_PID;
                case 0x03: // "ISO/IEC 11172 Audio"
                case 0x04: // "ISO/IEC 13818-3 Audio"
                case 0x81: // "AC3 Audio"
                        return AUD_PID;
                default:
                        return UNO_PID; // unknown
        }
}

static void parse_PMT_load(obj_t *obj)
{
        uint8_t dat;
        uint16_t info_length;
        psi_t *psi;
        struct NODE *node;
        prog_t *prog;
        pids_t *pids;
        track_t *track;
        ts_t *ts;

        ts = obj->ts;
        psi = obj->psi;

        node = obj->prog->head;
        while(node)
        {
                prog = (prog_t *)node;
                if(psi->idx.program_number == prog->program_number)
                {
                        break;
                }
                node = node->next;
        }
        if(!node)
        {
                printf("Wrong PID: 0x%04X\n", ts->PID);
                exit(EXIT_FAILURE);
        }

        prog->parsed = 1;
        if(0x02 != psi->table_id)
        {
                // SIT or other PSI
                return;
        }

        dat = *(obj->p)++; obj->left_length--;
        prog->PCR_PID = dat & 0x1F;

        dat = *(obj->p)++; obj->left_length--;
        prog->PCR_PID <<= 8;
        prog->PCR_PID |= dat;

        // add PCR PID
        pids = (pids_t *)malloc(sizeof(pids_t));
        if(NULL == pids)
        {
                printf("Malloc memory failure!\n");
                exit(EXIT_FAILURE);
        }
        pids->PID = prog->PCR_PID;
        pids->type = PCR_PID;
        pids->sdes = " PCR";
        pids->ldes = "program clock reference";
        pids->CC = 0;
        pids->dCC = 0;
        pids->is_CC_sync = 1;
        pids_add(obj->pids, pids);

        // program_info_length
        dat = *(obj->p)++; obj->left_length--;
        info_length = dat & 0x0F;

        dat = *(obj->p)++; obj->left_length--;
        info_length <<= 8;
        info_length |= dat;

        printf("program_info: ");
        while(info_length-- > 0)
        {
                // omit descriptor here
                dat = *(obj->p)++; obj->left_length--;
                printf("%02x ", (int)dat);
        }
        printf("\n");

        while(obj->left_length > 4)
        {
                // add track
                track = (track_t *)malloc(sizeof(track_t));
                if(NULL == track)
                {
                        printf("Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }

                dat = *(obj->p)++; obj->left_length--;
                track->stream_type = dat;

                dat = *(obj->p)++; obj->left_length--;
                track->PID = dat & 0x1F;

                dat = *(obj->p)++; obj->left_length--;
                track->PID <<= 8;
                track->PID |= dat;

                // ES_info_length
                dat = *(obj->p)++; obj->left_length--;
                info_length = dat & 0x0F;

                dat = *(obj->p)++; obj->left_length--;
                info_length <<= 8;
                info_length |= dat;

                printf("     es_info: ");
                while(info_length-- > 0)
                {
                        // omit descriptor here
                        dat = *(obj->p)++; obj->left_length--;
                        printf("%02x ", (int)dat);
                }
                printf("\n");

                track->type = PID_type(track->stream_type);
                list_add(prog->track, (struct NODE *)track);

                // add track PID
                pids = (pids_t *)malloc(sizeof(pids_t));
                if(NULL == pids)
                {
                        printf("Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }
                pids->PID = track->PID;
                pids->type = track->type;
                if(VID_PID == pids->type)
                {
                        pids->sdes = " VID";
                        pids->ldes = "video package";
                }
                else if(AUD_PID == pids->type)
                {
                        pids->sdes = " AUD";
                        pids->ldes = "audio package";
                }
                pids->CC = 0;
                pids->dCC = 1;
                pids->is_CC_sync = 0;
                pids_add(obj->pids, pids);
        }
}

static int search_in_TS_PID(uint16_t pid)
{
        int i;
        int count = sizeof(TS_PID) / sizeof(ts_pid_t);
        //ts_pid_t *table;

        //printf("count of TS_PID: %d\n", count);
        for(i = 0; i < count; i++)
        {
                //table = &(TS_PID[i]);
                if(TS_PID[i].min <= pid && pid <= TS_PID[i].max)
                {
                        return i;
                }
        }

        return 0; // PAT for wrong search state
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
