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
        STATE_NEXT_PSI,
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

        uint32_t crc_3:8; // most significant byte
        uint32_t crc_2:8;
        uint32_t crc_1:8;
        uint32_t crc_0:8; // last significant byte
};

struct PIDS
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t PID:13;
        uint32_t type; // enum PID_TYPE
        uint32_t cc:4; // continuity_counter
        uint32_t delta_cc:4; // 0 or 1
        int is_cc_sync;
};

struct TRACK
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t PID:13;
        int type;
        char *type_str;
};

struct PROG
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t program_number:16;
        uint32_t PMT_PID:13;
        uint32_t PCR_PID:13;

        struct LIST *track;
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
        //uint16_t pid;

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
int is_unparsed_PMT(struct OBJ *obj, uint16_t pid);
int is_all_PMT_parsed(struct OBJ *obj);
void parse_PMT(struct OBJ *obj);
void show_PMT(struct OBJ *obj);
int stream_type(uint8_t st);
char *stream_type_str(uint8_t st);
void list_show(struct OBJ *obj);
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
                        case STATE_NEXT_PSI:
                                if(0x0000 == ts->PID)
                                {
                                        //show_TS(obj);
                                        parse_PSI(obj);
                                        //obj->state = STATE_WAIT_PMT;
                                }
                                break;
                        //case STATE_WAIT_PMT:
                                if(is_unparsed_PMT(obj, ts->PID))
                                {
                                        parse_PMT(obj);
                                }
                                if(is_all_PMT_parsed(obj))
                                {
                                        if(MODE_PSI == obj->mode)
                                        {
                                                show_PSI(obj);
                                                list_show(obj);
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
        struct PID_LIST *pid_item;

        obj = (struct OBJ *)malloc_mem(sizeof(struct OBJ), "creat object");

        obj->ts = (struct TS *)malloc_mem(sizeof(struct TS), "creat TS struct");
        obj->psi = (struct PSI *)malloc_mem(sizeof(struct PSI), "creat PSI struct");

        obj->ts_size = 188;
        obj->mode = MODE_PSI;
        strcpy(obj->file_i, "2009-04-28.ts");

        obj->prog = list_init();
        obj->pids = list_init();

        // add PAT PID
        pid_item = (struct PID_LIST *)malloc_mem(sizeof(struct PID_LIST), "creat PID list item");
        pid_item->PID = 0x0000;
        pid_item->type = PID_TYPE_PAT;
        pid_item->cc = 0;
        pid_item->delta_cc = 1;
        pid_item->is_cc_sync = FALSE;
        list_add(obj, pid_item);

        // add NULL PID
        pid_item = (struct PID_LIST *)malloc_mem(sizeof(struct PID_LIST), "creat PID list item");
        pid_item->PID = 0x1FFF;
        pid_item->type = PID_TYPE_NUL;
        pid_item->cc = 0;
        pid_item->delta_cc = 0;
        pid_item->is_cc_sync = TRUE;
        list_add(obj, pid_item);

        obj->state = STATE_WAIT_PAT;

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
                        else if (0 == strcmp(argv[i], "-h"))
                        {
                                show_help();
                                exit(EXIT_SUCCESS);
                        }
                        else if (0 == strcmp(argv[i], "-v"))
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

        obj->state = STATE_NEXT_PSI;

        return obj;
}

int delete(struct OBJ *obj)
{
        uint32_t idx;

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
        printf("Usage: tsana [options] file...\n");
        printf("Options:\n");
        printf("  -n <num>       Size of TS package, default: 188\n");
        printf("  -o <file>      Output file name, default: *2.ts\n");
        printf("  -psi           Show PSI information\n");
        printf("  -cc            Check Continuity Counter\n");
        printf("  -pcr           Show all PCR value\n");
        printf("  -debug         Show all errors found\n");
        printf("  -flt <pid>     Filter package with <pid>\n");
        printf("  -pes <pid>     Output PES file with <pid>\n");
        printf("  -h             Display this information\n");
        printf("  -v             Display my version\n");
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
                (obj->p)++; // 0x04
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
#endif
void adaption_fields(struct OBJ *obj)
{
        uint8_t dat;
        struct AF *af = obj->af;

        dat = *(obj->p)++;
        af->adaption_field_length = dat;
}

void parse_PSI(struct OBJ *obj)
{
        uint8_t dat;
        uint32_t idx;
        uint32_t dat_cnt;
        struct PAT *pat = obj->pat;
        struct PID_LIST *pid_item;

        dat = *(obj->p)++;
        pat->table_id = dat;

        dat = *(obj->p)++;
        pat->sectin_syntax_indicator = (dat & BIT7) >> 7;
        pat->section_length = dat & 0x0F;

        dat = *(obj->p)++;
        pat->section_length <<= 8;
        pat->section_length |= dat;
        dat_cnt = pat->section_length;

        dat = *(obj->p)++; dat_cnt--;
        pat->transport_stream_id = dat;

        dat = *(obj->p)++; dat_cnt--;
        pat->transport_stream_id <<= 8;
        pat->transport_stream_id |= dat;

        dat = *(obj->p)++; dat_cnt--;
        pat->version_number = (dat & 0x3E) >> 1;
        pat->current_next_indicator = dat & BIT0;

        dat = *(obj->p)++; dat_cnt--;
        pat->section_number = dat;

        dat = *(obj->p)++; dat_cnt--;
        pat->last_section_number = dat;

        idx = 0;
        while(dat_cnt > 4)
        {
                dat = *(obj->p)++; dat_cnt--;
                pat->program[idx].program_number = dat;

                dat = *(obj->p)++; dat_cnt--;
                pat->program[idx].program_number <<= 8;
                pat->program[idx].program_number |= dat;

                dat = *(obj->p)++; dat_cnt--;
                pat->program[idx].PID = dat & 0x1F;

                dat = *(obj->p)++; dat_cnt--;
                pat->program[idx].PID <<= 8;
                pat->program[idx].PID |= dat;

                obj->pmt[idx] = (struct PMT *)malloc_mem(sizeof(struct PMT),
                                                         "creat PMT struct");
                obj->pmt[idx]->PID = pat->program[idx].PID;
#if 1
                if(0x0000 == pat->program[idx].program_number)
                {
                        obj->pmt[idx]->program_number = 0x0000;
                        obj->pmt[idx]->parsed = TRUE;

                        // add SIT PID
                        pid_item = (struct PID_LIST *)malloc_mem(sizeof(struct PID_LIST), "creat PID list item");
                        pid_item->PID = pat->program[idx].PID;
                        pid_item->type = PID_TYPE_SIT;
                        pid_item->cc = 0;
                        pid_item->delta_cc = 1;
                        pid_item->is_cc_sync = FALSE;
                        list_add(obj, pid_item);
                }
                else
#endif
                {
                        obj->pmt[idx]->parsed = FALSE;

                        // add PMT PID
                        pid_item = (struct PID_LIST *)malloc_mem(sizeof(struct PID_LIST), "creat PID list item");
                        pid_item->PID = pat->program[idx].PID;
                        pid_item->type = PID_TYPE_PMT;
                        pid_item->cc = 0;
                        pid_item->delta_cc = 1;
                        pid_item->is_cc_sync = FALSE;
                        list_add(obj, pid_item);
                }

                idx++;
        }
        pat->program_cnt = idx;
}

void show_PSI(struct OBJ *obj)
{
        uint32_t idx;
        struct PAT *pat = obj->pat;

        printf("0x0000: PAT_PID\n");
        //printf("    0x%04X: section_length\n", pat->section_length);
        printf("    0x%04X: transport_stream_id\n", pat->transport_stream_id);

        idx = 0;
        while(idx < pat->program_cnt)
        {
                if(0x0000 == pat->program[idx].program_number)
                {
                        printf("    0x%04X: network_program_number\n",
                               pat->program[idx].program_number);
                        printf("        0x%04X: network_PID\n",
                               pat->program[idx].PID);
                }
                else
                {
                        printf("    0x%04X: program_number\n",
                               pat->program[idx].program_number);
                        printf("        0x%04X: PMT_PID\n",
                               pat->program[idx].PID);
                }
                idx++;
        }
}

int is_unparsed_PMT(struct OBJ *obj, uint16_t pid)
{
        uint32_t idx;
        struct PAT *pat = obj->pat;

        idx = pat->program_cnt;
        while(idx-- > 0)
        {
                if(     pid == obj->pmt[idx]->PID &&
                        FALSE == obj->pmt[idx]->parsed
                )
                {
                        return TRUE;
                }
        }
        return FALSE;
}

int is_all_PMT_parsed(struct OBJ *obj)
{
        uint32_t idx;
        struct PAT *pat = obj->pat;

        idx = pat->program_cnt;
        while(idx-- > 0)
        {
                if(FALSE == obj->pmt[idx]->parsed)
                {
                        return FALSE;
                }
        }
        return TRUE;
}

void parse_PMT(struct OBJ *obj)
{
        uint8_t dat;
        uint32_t i;
        uint32_t idx;
        uint32_t dat_cnt;
        struct PMT *pmt = obj->pmt[0];
        struct PID_LIST *pid_item;

        pmt->parsed = TRUE;

        dat = *(obj->p)++;
        pmt->table_id = dat;

        dat = *(obj->p)++;
        pmt->sectin_syntax_indicator = (dat & BIT7) >> 7;
        pmt->section_length = dat & 0x0F;

        dat = *(obj->p)++;
        pmt->section_length <<= 8;
        pmt->section_length |= dat;
        dat_cnt = pmt->section_length;

        dat = *(obj->p)++; dat_cnt--;
        pmt->program_number = dat;

        dat = *(obj->p)++; dat_cnt--;
        pmt->program_number <<= 8;
        pmt->program_number |= dat;

        dat = *(obj->p)++; dat_cnt--;
        pmt->version_number = (dat & 0x3E) >> 1;
        pmt->current_next_indicator = dat & BIT0;

        dat = *(obj->p)++; dat_cnt--;
        pmt->section_number = dat;

        dat = *(obj->p)++; dat_cnt--;
        pmt->last_section_number = dat;

        dat = *(obj->p)++; dat_cnt--;
        pmt->PCR_PID = dat & 0x1F;

        dat = *(obj->p)++; dat_cnt--;
        pmt->PCR_PID <<= 8;
        pmt->PCR_PID |= dat;

        // add PCR PID
        pid_item = (struct PID_LIST *)malloc_mem(sizeof(struct PID_LIST), "creat PID list item");
        pid_item->PID = pmt->PCR_PID;
        pid_item->type = PID_TYPE_PCR;
        pid_item->cc = 0; // always zero
        pid_item->delta_cc = 0;
        pid_item->is_cc_sync = TRUE;
        list_add(obj, pid_item);

        dat = *(obj->p)++; dat_cnt--;
        pmt->program_info_length = dat & 0x0F;

        dat = *(obj->p)++; dat_cnt--;
        pmt->program_info_length <<= 8;
        pmt->program_info_length |= dat;

        i = pmt->program_info_length;
        while(i-- > 0)
        {
                // omit descriptor here
                dat = *(obj->p)++; dat_cnt--;
        }

        idx = 0;
        while(dat_cnt > 4)
        {
                dat = *(obj->p)++; dat_cnt--;
                pmt->pid[idx].stream_type = dat;

                dat = *(obj->p)++; dat_cnt--;
                pmt->pid[idx].PID = dat & 0x1F;

                dat = *(obj->p)++; dat_cnt--;
                pmt->pid[idx].PID <<= 8;
                pmt->pid[idx].PID |= dat;

                dat = *(obj->p)++; dat_cnt--;
                pmt->pid[idx].ES_info_length = dat & 0x0F;

                dat = *(obj->p)++; dat_cnt--;
                pmt->pid[idx].ES_info_length <<= 8;
                pmt->pid[idx].ES_info_length |= dat;

                i = pmt->pid[idx].ES_info_length;
                while(i-- > 0)
                {
                        // omit descriptor here
                        dat = *(obj->p)++; dat_cnt--;
                }

                // add es PID
                pid_item = (struct PID_LIST *)malloc_mem(sizeof(struct PID_LIST), "creat PID list item");
                pid_item->PID = pmt->pid[idx].PID;
                pid_item->type = stream_type(pmt->pid[idx].stream_type);
                pid_item->cc = 0;
                pid_item->delta_cc = 1;
                pid_item->is_cc_sync = FALSE;
                list_add(obj, pid_item);

                idx++;
        }
        pmt->pid_cnt = idx;
}

void show_PMT(struct OBJ *obj)
{
        uint32_t i;
        uint32_t idx;
        struct PAT *pat = obj->pat;
        struct PMT *pmt;

        for(idx = 0; idx < pat->program_cnt; idx++)
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

int stream_type(uint8_t st)
{
        switch(st)
        {
                case 0x1B:
                case 0x01:
                case 0x02: return PID_TYPE_VID;
                case 0x03:
                case 0x04: return PID_TYPE_AUD;
                default:   return PID_TYPE_UNO; // unknown
        }
}

char *stream_type_str(uint8_t st)
{
        switch(st)
        {
                case 0x01: return "ISO/IEC 11172 Video";
                case 0x02: return "H.262|ISO/IEC 13818-2 Video";
                case 0x03: return "ISO/IEC 11172 Audio";
                case 0x04: return "ISO/IEC 13818-3 Audio";
                case 0x1B: return "H.264|ISO/IEC 14496-10 Video";
                default:   return "Unknown";
        }
}

void list_show(struct OBJ *obj)
{
        struct PID_LIST *item = obj->head;

        printf("\nPID LIST:\n");
        printf("    -PID--, -type--, --CC--, -dCC--\n");
        while(item)
        {
                printf("    0x%04X, %s, 0x%04X, 0x%04X\n",
                       item->PID,
                       PID_TYPE_STR[item->type],
                       item->cc,
                       item->delta_cc);

                item = item->next;
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

//=============================================================================
// THE END.
//=============================================================================
