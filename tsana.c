//=============================================================================
// Name: tsana.c
// Purpose: analyse certain character with ts file
// To build: gcc -std=c99 -o tsana.exe tsana.c
// Copyright (C) 2009 by ZHOU Cheng. All right reserved.
//=============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strcmp, etc
#include <stdint.h> // for uint?_t, etc

#include "list.h"

#define FALSE                           0
#define TRUE                            1

#define BIT0                            (1 << 0)
#define BIT1                            (1 << 1)
#define BIT2                            (1 << 2)
#define BIT3                            (1 << 3)
#define BIT4                            (1 << 4)
#define BIT5                            (1 << 5)
#define BIT6                            (1 << 6)
#define BIT7                            (1 << 7)

#define TABLE_ID_PAT                    0x00
#define TABLE_ID_CAT                    0x01
#define TABLE_ID_PMT                    0x02

//=============================================================================
// const definition:
//=============================================================================
const char *PAT_PID = "PAT_PID";
const char *PMT_PID = "PMT_PID";
const char *SIT_PID = "SIT_PID";
const char *PCR_PID = "PCR_PID";
const char *VID_PID = "VID_PID";
const char *AUD_PID = "AUD_PID";
const char *NUL_PID = "NUL_PID";
const char *UNO_PID = "UNO_PID";

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
        STATE_NEXT_PKG,
        STATE_NEXT_PMT,
        STATE_NEXT_PAT,
        STATE_EXIT
};

//=============================================================================
// struct definition:
//=============================================================================
struct TS
{
        uint32_t sync_byte:8;
        uint32_t transport_error_indicator:1;
        uint32_t payload_unit_start_indicator:1;
        uint32_t transport_priority:1;
        uint32_t PID:13;
        uint32_t transport_scrambling_control:2;
        uint32_t adaption_field_control:2;
        uint32_t continuity_counter:4;
};

struct AF
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
};

struct PSI
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

#if 0
        if(ts->PID == PAT_PID)
        {
                loop
                {
                        // program_number:16;
                        // reserved:3;
                        if(program_number == 0x0000)
                        {
                                // network_PID:13;
                        }
                        else
                        {
                                // PMT_PID:13;
                        }
                }
        }
        else if(ts->PID belongs to PMT_PIDs)
        {
                // reserved:3;
                // PCR_PID:13;
                // reserved:4;
                // program_info_length:12;
                {
                        descriptor();
                }
                loop
                {
                        // stream_type:8;
                        // reserved:3;
                        // PMT_PID:13;
                        // reserved:4;
                        // ES_info_length:12
                        {
                                descriptor();
                        }
                }
        }
#endif

        uint32_t CRC3:8; // most significant byte
        uint32_t CRC2:8;
        uint32_t CRC1:8;
        uint32_t CRC0:8; // last significant byte
};

struct PIDS
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t PID:13;
        const char *type; // PID type

        // for check continuity_counter
        uint32_t CC:4;
        uint32_t delta_CC:4; // 0 or 1
        int is_CC_sync;
};

struct TRACK
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t PID:13;
        int stream_type;
        const char *type; // PID type
};

struct PROG
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t program_number:16;
        uint32_t PMT_PID:13;
        uint32_t PCR_PID:13;

        struct LIST *track;
        int parsed;
};

struct OBJ
{
        char file[FILENAME_MAX]; // input file name without postfix

        char file_i[FILENAME_MAX];
        FILE *fd_i;
        uint32_t addr; // addr in input file

        char file_o[FILENAME_MAX];
        FILE *fd_o;

        uint32_t ts_size; // 188 or 204
        uint8_t line[204]; // one TS package
        uint8_t *p; // point to current data in line

        struct TS *ts;
        struct PSI *psi;
        uint16_t left_length;

        struct LIST *prog;
        struct LIST *pids;

        int mode;
        int state;
};

//=============================================================================
// Variables definition:
//=============================================================================
struct OBJ *obj = NULL;

//=============================================================================
// Sub-function declare:
//=============================================================================
struct OBJ *create(int argc, char *argv[]);
int delete(struct OBJ *obj);

void *malloc_mem(int size, char *memo);
char *printb(uint32_t x, int bit_cnt);
void show_help();
void show_version();

void sync_input(struct OBJ *obj);
int get_one_pkg(struct OBJ *obj);
void parse_TS(struct OBJ *obj);
void show_TS(struct OBJ *obj);
void parse_AF(struct OBJ *obj); // adaption_fields
void parse_PSI(struct OBJ *obj);
void show_PSI(struct OBJ *obj);
void parse_PAT_load(struct OBJ *obj);
int is_unparsed_PMT(struct OBJ *obj);
int is_all_PMT_parsed(struct OBJ *obj);
void parse_PMT_load(struct OBJ *obj);
void show_PMT(struct OBJ *obj);
const char *PID_type(uint8_t st);
void pids_add(struct LIST *list, struct PIDS *pids);
void show_pids(struct OBJ *obj);
struct PID_LIST *list_match(struct OBJ *obj, uint16_t pid);

//=============================================================================
// The main function:
//=============================================================================
int main(int argc, char *argv[])
{
        struct TS *ts;
        struct PSI *psi;

        obj = create(argc, argv);
        ts = obj->ts;
        psi = obj->psi;

        sync_input(obj);
        while(STATE_EXIT != obj->state && get_one_pkg(obj))
        {
                parse_TS(obj);
                switch(obj->state)
                {
                        case STATE_NEXT_PAT:
                                if(0x0000 == ts->PID)
                                {
                                        //show_TS(obj);
                                        parse_PSI(obj);
                                        parse_PAT_load(obj);
                                        show_pids(obj);
                                        obj->state = STATE_NEXT_PMT;
                                }
                                break;
                        case STATE_NEXT_PMT:
                                if(is_unparsed_PMT(obj))
                                {
                                        parse_PSI(obj);
                                        parse_PMT_load(obj);
                                }
                                if(is_all_PMT_parsed(obj))
                                {
                                        if(MODE_PSI == obj->mode)
                                        {
                                                //show_PSI(obj);
                                                show_pids(obj);
                                                obj->state = STATE_EXIT;
                                        }
                                        else if(MODE_CC == obj->mode)
                                        {
                                                sync_input(obj);
                                                obj->state = STATE_NEXT_PKG;
                                        }
                                        else
                                        {
                                                obj->state = STATE_EXIT;
                                        }
                                }
                                break;
#if 0
                        case STATE_NEXT_PKG:
                                if(MODE_CC == obj->mode)
                                {
                                        item = list_match(obj, ts->PID);
                                        if(NULL == item)
                                        {
                                                printf("0x%08X", (int)obj->addr);
                                                printf(",0x%04X", (int)ts->PID);
                                                printf(", Unknown PID!\n");
                                        }
                                        else if(item->is_cc_sync)
                                        {
                                                item->cc += item->delta_cc;
                                                if(item->cc != ts->continuity_counter)
                                                {
                                                        int lost;

                                                        lost = (int)ts->continuity_counter;
                                                        lost -= (int)item->cc;
                                                        if(lost < 0)
                                                        {
                                                                lost += 16;
                                                        }
                                                        printf("0x%08X", (int)obj->addr);
                                                        printf(",0x%04X", (int)ts->PID);
                                                        printf(",wait %X but %X lost %2d\n",
                                                               item->cc,
                                                               ts->continuity_counter,
                                                               lost);
                                                        item->cc = ts->continuity_counter;
                                                }
                                        }
                                        else
                                        {
                                                item->cc = ts->continuity_counter;
                                                item->is_cc_sync = TRUE;
                                        }
                                }
                                break;
#endif
                        default:
                                printf("Wrong state!\n");
                                obj->state = STATE_EXIT;
                                break;
                }
                obj->addr += obj->ts_size;
        }

#if 0
        else if(MODE_PCR == obj->mode)
        {
                if(BIT1 & ts->adaption_field_control)
                        //                           && (line[0x05] & 0x10))
                {
                        uint64_t pcr_base = 0; // 33-bit
                        uint16_t pcr_ext = 0;  //  9-bit
                        uint64_t pcr;

                        pcr_base  |= obj->line[0x06];
                        pcr_base <<= 8;
                        pcr_base  |= obj->line[0x07];
                        pcr_base <<= 8;
                        pcr_base  |= obj->line[0x08];
                        pcr_base <<= 8;
                        pcr_base  |= obj->line[0x09];
                        pcr_base <<= 1;
                        pcr_base  |= ((obj->line[0x0A] & 0x80) ? 0x01 : 0x00);

                        pcr_ext  |= (obj->line[0x0A] & 0x01);
                        pcr_ext <<= 8;
                        pcr_ext  |= obj->line[0x0B];

                        pcr = pcr_base * 300 + pcr_ext;

                        printf("0x%08X", obj->addr);
                        printf(",%10u", obj->addr);
                        printf(",0x%04X", ts->PID);
                        printf(",%13llu", pcr);
                        printf(",%10llu", pcr_base);
                        printf(",%3u", pcr_ext);
                        printf("\n");
                }
        }
#endif

        delete(obj);
        exit(EXIT_SUCCESS);
}

//=============================================================================
// Subfunctions definition:
//=============================================================================
struct OBJ *create(int argc, char *argv[])
{
        int i;
        int dat;
        struct OBJ *obj;
        struct PIDS *pids;

        obj = (struct OBJ *)malloc_mem(sizeof(struct OBJ), "creat object");

        obj->ts = (struct TS *)malloc_mem(sizeof(struct TS), "creat TS struct");
        obj->psi = (struct PSI *)malloc_mem(sizeof(struct PSI), "creat PSI struct");

        obj->ts_size = 188;
        obj->mode = MODE_PSI;
        strcpy(obj->file_i, "2009-04-28.ts");

        obj->prog = list_init();
        obj->pids = list_init();

        // add PAT PID
        pids = (struct PIDS *)malloc(sizeof(struct PIDS));
        if(NULL == pids)
        {
                printf("Malloc memory failure!\n");
                exit(EXIT_FAILURE);
        }
        pids->PID = 0x0000;
        pids->type = PAT_PID;
        pids->CC = 0;
        pids->delta_CC = 1;
        pids->is_CC_sync = FALSE;
        pids_add(obj->pids, pids);

        // add NUL PID
        pids = (struct PIDS *)malloc(sizeof(struct PIDS));
        if(NULL == pids)
        {
                printf("Malloc memory failure!\n");
                exit(EXIT_FAILURE);
        }
        pids->PID = 0x1FFF;
        pids->type = NUL_PID;
        pids->CC = 0;
        pids->delta_CC = 0;
        pids->is_CC_sync = TRUE;
        pids_add(obj->pids, pids);

        if(1 == argc)
        {
                // no parameter
                printf("tsana: no input files, try -h\n");
                //exit(EXIT_SUCCESS);
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
                        i = argc;
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

        if(NULL == (obj->fd_i = fopen(obj->file_i, "rb")))
        {
                printf("Can not open file \"%s\"!\n", obj->file_i);
                exit(EXIT_FAILURE);
        }

        obj->state = STATE_NEXT_PAT;

        return obj;
}

int delete(struct OBJ *obj)
{
        if(NULL == obj)
        {
                return 0;
        }
        else
        {
                fclose(obj->fd_i);

                list_free(obj->prog);
                list_free(obj->pids);

                free(obj);

                return 1;
        }
}

void *malloc_mem(int size, char *memo)
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

void show_help()
{
        printf("Usage: tsana [options] file\n");
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

void show_version()
{
        printf("tsana 0.1.0 (by MingW), %s %s\n", __TIME__, __DATE__);
        printf("Copyright (C) 2009 ZHOU Cheng.\n");
        printf("This is free software; contact author for additional information.\n");
        printf("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR\n");
        printf("A PARTICULAR PURPOSE.\n");
}

char *printb(uint32_t x, int bit_cnt)
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

void sync_input(struct OBJ *obj)
{
        int sync_byte;
        FILE *fd = obj->fd_i;

        obj->addr = 0;
        fseek(fd, 0, SEEK_SET);
        do
        {
                if(EOF == (sync_byte = fgetc(fd)))
                {
                        break;
                }
                else if(0x47 == sync_byte)
                {
                        fseek(fd, obj->ts_size - 1, SEEK_CUR);
                        if(EOF == (sync_byte = fgetc(fd)))
                        {
                                break;
                        }
                        else if(0x47 == sync_byte)
                        {
                                // sync, go back
                                fseek(fd, -(obj->ts_size + 1), SEEK_CUR);
                                break;
                        }
                        else
                        {
                                // not real sync byte
                                fseek(fd, -(obj->ts_size), SEEK_CUR);
                        }
                }
                else
                {
                        (obj->addr)++;
                }
        }while(1);

        if(0 != obj->addr)
        {
                printf("Find first sync byte at 0x%X in %s.\n",
                       obj->addr, obj->file_i);
        }
}

int get_one_pkg(struct OBJ *obj)
{
        uint32_t size;

        size = fread(obj->line, 1, obj->ts_size, obj->fd_i);
        obj->p = obj->line;

        return (size == obj->ts_size);
}

void parse_TS(struct OBJ *obj)
{
        uint8_t dat;
        struct TS *ts = obj->ts;

        dat = *(obj->p)++;
        ts->sync_byte = dat;
        if(0x47 != ts->sync_byte)
        {
                printf("Wrong package head at 0x%X!\n", obj->addr);
                exit(EXIT_FAILURE);
        }

        dat = *(obj->p)++;
        ts->transport_error_indicator = (dat & BIT7) >> 7;
        ts->payload_unit_start_indicator = (dat & BIT6) >> 6;
        ts->transport_priority = (dat & BIT5) >> 5;
        ts->PID = dat & 0x1F;

        dat = *(obj->p)++;
        ts->PID <<= 8;
        ts->PID |= dat;

        dat = *(obj->p)++;
        ts->transport_scrambling_control = (dat & (BIT7 | BIT6)) >> 6;
        ts->adaption_field_control = (dat & (BIT5 | BIT4)) >> 4;;
        ts->continuity_counter = dat & 0x0F;

        if(BIT1 & obj->ts->adaption_field_control)
        {
                //adaption_field();
        }
        if(BIT0 & ts->adaption_field_control)
        {
                // data_byte
                (obj->p)++;
        }
}

void show_TS(struct OBJ *obj)
{
        struct TS *ts = obj->ts;

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

#if 0
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

void adaption_fields(struct OBJ *obj)
{
        uint8_t dat;
        struct AF *af = obj->af;

        dat = *(obj->p)++;
        af->adaption_field_length = dat;
}
#endif

void parse_PSI(struct OBJ *obj)
{
        uint8_t dat;
        struct PSI *psi = obj->psi;

        dat = *(obj->p)++;
        psi->table_id = dat;

        dat = *(obj->p)++;
        psi->sectin_syntax_indicator = (dat & BIT7) >> 7;
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
        psi->current_next_indicator = dat & BIT0;

        dat = *(obj->p)++; obj->left_length--;
        psi->section_number = dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->last_section_number = dat;
}

void show_PSI(struct OBJ *obj)
{
        uint32_t idx;
        struct PSI *psi = obj->psi;

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

void parse_PAT_load(struct OBJ *obj)
{
        uint8_t dat;
        struct PSI *psi = obj->psi;
        struct PROG *prog;
        struct PIDS *pids;

        while(obj->left_length > 4)
        {
                // add program
                prog = (struct PROG *)malloc(sizeof(struct PROG));
                if(NULL == prog)
                {
                        printf("Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }
                prog->track = list_init();
                prog->parsed = FALSE;

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

                list_add(obj->prog, (struct NODE *)prog);

                // add PMT PID
                pids = (struct PIDS *)malloc(sizeof(struct PIDS));
                if(NULL == pids)
                {
                        printf("Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }
                pids->PID = prog->PMT_PID;
                pids->type = PMT_PID;
                pids->CC = 0;
                pids->delta_CC = 1;
                pids->is_CC_sync = FALSE;
                pids_add(obj->pids, pids);
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

        printf("PROG Count: %u\n", list_count(obj->prog));
        printf("PIDS Count: %u\n", list_count(obj->pids));
}

void show_pids(struct OBJ *obj)
{
        struct PIDS *pids;
        struct NODE *node;

        printf("\nPID LIST:\n");
        printf("    -PID--, -type--, --CC--, -dCC--\n");
        node = obj->pids->head;
        while(node)
        {
                pids = (struct PIDS *)node;
                printf("    0x%04X, %s, 0x%04X, 0x%04X\n",
                       pids->PID,
                       pids->type,
                       pids->CC,
                       pids->delta_CC);
                node = node->next;
        }
}

void pids_add(struct LIST *list, struct PIDS *pids)
{
        struct NODE *node;

        node = list->head;
        while(node)
        {
                if(pids->PID == ((struct PIDS *)node)->PID)
                {
                        // same PID, omit...
                        return;
                }
                node = node->next;
        }

        list_add(list, (struct NODE *)pids);
}

int is_unparsed_PMT(struct OBJ *obj)
{
        struct TS *ts;
        struct NODE *node;
        struct PROG *prog;

        ts = obj->ts;
        node = obj->prog->head;
        while(node)
        {
                prog = (struct PROG *)node;
                if(ts->PID == prog->PMT_PID)
                {
                        if(FALSE == prog->parsed)
                        {
                                return TRUE;
                        }
                        else
                        {
                                return FALSE;
                        }
                }
                node = node->next;
        }
        return FALSE;
}

int is_all_PMT_parsed(struct OBJ *obj)
{
        struct TS *ts;
        struct NODE *node;
        struct PROG *prog;

        ts = obj->ts;
        node = obj->prog->head;
        while(node)
        {
                prog = (struct PROG *)node;
                if(FALSE == prog->parsed)
                {
                        return FALSE;
                }
                node = node->next;
        }
        return TRUE;
}

const char *PID_type(uint8_t st)
{
        switch(st)
        {
                case 0x1B: // "H.264|ISO/IEC 14496-10 Video"
                case 0x01: // "ISO/IEC 11172 Video"
                case 0x02: // "H.262|ISO/IEC 13818-2 Video"
                        return VID_PID;
                case 0x03: // "ISO/IEC 11172 Audio"
                case 0x04: // "ISO/IEC 13818-3 Audio"
                        return AUD_PID;
                default:
                        return UNO_PID; // unknown
        }
}

void parse_PMT_load(struct OBJ *obj)
{
        uint8_t dat;
        uint16_t info_length;
        struct PSI *psi;
        struct NODE *node;
        struct PROG *prog;
        struct PIDS *pids;
        struct TRACK *track;
        struct TS *ts;

        ts = obj->ts;
        psi = obj->psi;

        node = obj->prog->head;
        while(node)
        {
                prog = (struct PROG *)node;
                if(ts->PID == prog->PMT_PID)
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

        prog->parsed = TRUE;
        if(TABLE_ID_PMT != psi->table_id)
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
        pids = (struct PIDS *)malloc(sizeof(struct PIDS));
        if(NULL == pids)
        {
                printf("Malloc memory failure!\n");
                exit(EXIT_FAILURE);
        }
        pids->PID = prog->PCR_PID;
        pids->type = PCR_PID;
        pids->CC = 0;
        pids->delta_CC = 0;
        pids->is_CC_sync = TRUE;
        pids_add(obj->pids, pids);

        // program_info_length
        dat = *(obj->p)++; obj->left_length--;
        info_length = dat & 0x0F;

        dat = *(obj->p)++; obj->left_length--;
        info_length <<= 8;
        info_length |= dat;

        while(info_length-- > 0)
        {
                // omit descriptor here
                dat = *(obj->p)++; obj->left_length--;
        }

        while(obj->left_length > 4)
        {
                // add track
                track = (struct TRACK *)malloc(sizeof(struct TRACK));
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

                while(info_length-- > 0)
                {
                        // omit descriptor here
                        dat = *(obj->p)++; obj->left_length--;
                }

                track->type = PID_type(track->stream_type);
                list_add(prog->track, (struct NODE *)track);

                // add track PID
                pids = (struct PIDS *)malloc(sizeof(struct PIDS));
                if(NULL == pids)
                {
                        printf("Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }
                pids->PID = track->PID;
                pids->type = track->type;
                pids->CC = 0;
                pids->delta_CC = 1;
                pids->is_CC_sync = FALSE;
                pids_add(obj->pids, pids);
        }
}

#if 0
void show_PMT(struct OBJ *obj)
{
        uint32_t i;
        uint32_t idx;
        struct PSI *psi = obj->psi;
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

struct PID_LIST *list_match(struct OBJ *obj, uint16_t pid)
{
        struct PID_LIST *item = obj->head;

        while(item)
        {
                if(pid == item->PID)
                {
                        break;
                }
                item = item->next;
        }

        return item;
}
#endif
//=============================================================================
// THE END.
//=============================================================================
