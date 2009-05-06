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

#define BIT0                            (1 << 0)
#define BIT1                            (1 << 1)
#define BIT2                            (1 << 2)
#define BIT3                            (1 << 3)
#define BIT4                            (1 << 4)
#define BIT5                            (1 << 5)
#define BIT6                            (1 << 6)
#define BIT7                            (1 << 7)

#define PID_NUM                         0x2000
#define MAX_PROGRAM_CNT                 20 // in PAT
#define MAX_TS_SIZE                     204

//=============================================================================
// enum & struct definition:
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
        STATE_NEXT_PMT,
        STATE_WAIT_PMT,
        STATE_NEXT_PAT,
        STATE_WAIT_PAT
};

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
TS_T;

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
AF_T;

typedef struct
{
        uint32_t table_id:8;
        uint32_t sectin_syntax_indicator:1;
        uint32_t pad0:1;
        uint32_t reserved0:2;
        uint32_t section_length:12;
        uint32_t transport_stream_id:16;
        uint32_t reserved1:2;
        uint32_t version_number:5;
        uint32_t current_next_indicator:1;
        uint32_t section_number:8;
        uint32_t last_section_number:8;
        // user-variable below:
        uint32_t program_cnt;
        struct
        {
                uint32_t program_number:16;
                uint32_t pid:13;
        }
        pn2p[MAX_PROGRAM_CNT]; // program_number to pid
}
PAT_T;

typedef struct
{
        TS_T *ts;
        AF_T *af;
        PAT_T *pat;

        uint32_t ts_size; // 188 or 204
        uint8_t line[MAX_TS_SIZE]; // one TS package
        uint8_t *p; // point to current data in line

        FILE *fd_i;
        FILE *fd_o;

        char file[FILENAME_MAX]; // input file name without postfix
        char file_i[FILENAME_MAX];
        char file_o[FILENAME_MAX];

        int mode;
        uint8_t pid_cc[PID_NUM];
        uint16_t pid;

        int state;
}
OBJ;

//=============================================================================
// Variables definition:
//=============================================================================
OBJ *obj = NULL;

//=============================================================================
// Sub-function declare:
//=============================================================================
OBJ *create(int argc, char *argv[]);
int delete(OBJ *obj);

void sync_input(OBJ *obj);
void get_TS(OBJ *obj);
void show_TS(OBJ *obj);
void get_AF(OBJ *obj); // adaption_fields
void get_PAT(OBJ *obj);
void show_PAT(OBJ *obj);

FILE *open_file(char *file, char *style, char *memo);
void *malloc_mem(int size, char *memo);
char *printb(uint32_t x, int bit_cnt);
void show_help();
void show_version();

//=============================================================================
// The main function:
//=============================================================================
int main(int argc, char *argv[])
{
        uint32_t addr;
        int lost;
        int is_ok = 1;

        obj = create(argc, argv);
        sync_input(obj);

        addr = -(obj->ts_size);
        while(obj->ts_size == fread(obj->line, 1, obj->ts_size, obj->fd_i))
        {
                addr += obj->ts_size;
                obj->p = obj->line;

                get_TS(obj);
                if(BIT1 & obj->ts->adaption_field_control)
                {
                        //adaption_field();
                }
                if(BIT0 & obj->ts->adaption_field_control)
                {
                        (obj->p)++; // 0x04
                }

                switch(obj->state)
                {
                        case STATE_WAIT_PAT:
                                break;
                        case STATE_NEXT_PAT:
                                break;
                        case STATE_WAIT_PMT:
                                break;
                        case STATE_NEXT_PMT:
                                break;
                }

                if(MODE_PSI == obj->mode)
                {
                        if(0x0000 != obj->ts->PID)
                        {
                                continue;
                        }
                        show_TS(obj);

                        get_PAT(obj);
                        show_PAT(obj);
                        break;
                }
                else if(MODE_CC == obj->mode)
                {
                        if(0xFF == obj->pid_cc[obj->pid])
                        {
                                obj->pid_cc[obj->pid] = obj->ts->continuity_counter;
                                continue;
                        }
                        else
                        {
                                obj->pid_cc[obj->pid]++;
                                obj->pid_cc[obj->pid] &= 0x0F;
                                if(0x1001 == obj->pid || 0x1FFF == obj->pid)
                                {
                                        // PCR or NULL package, always 0
                                        obj->pid_cc[obj->pid] = 0;
                                }
                                if(obj->pid_cc[obj->pid] == obj->ts->continuity_counter)
                                {
                                        is_ok = 1;
                                        continue;
                                }
                        }

                        lost = obj->ts->continuity_counter - obj->pid_cc[obj->pid];
                        if(lost < 0)
                        {
                                lost += 16;
                        }

                        if(is_ok)
                        {
                                printf("\n");
                        }
                        printf("0x%08X", (int)addr);
                        printf(",0x%04X", (int)obj->ts->PID);
                        printf(",wait %X but %X lost %2d", obj->pid_cc[obj->pid], obj->ts->continuity_counter, lost);
                        printf("\n");
                        obj->pid_cc[obj->pid] = obj->ts->continuity_counter;
                        is_ok = 0;
                }

                else if(MODE_PCR == obj->mode)
                {
                        if(BIT1 & obj->ts->adaption_field_control)
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

                                printf("0x%08X", addr);
                                printf(",%10u", addr);
                                printf(",0x%04X", obj->ts->PID);
                                printf(",%13llu", pcr);
                                printf(",%10llu", pcr_base);
                                printf(",%3u", pcr_ext);
                                printf("\n");
                        }
                }
        }

        delete(obj);
        exit(EXIT_SUCCESS);
}

//=============================================================================
// Subfunctions definition:
//=============================================================================
OBJ *create(int argc, char *argv[])
{
        int i;
        int dat;
        OBJ *obj;

        obj = (OBJ *)malloc_mem(sizeof(OBJ), "creat object");

        obj->ts = (TS_T *)malloc_mem(sizeof(TS_T), "creat TS struct");
        obj->af = (AF_T *)malloc_mem(sizeof(AF_T), "creat AF struct");
        obj->pat = (PAT_T *)malloc_mem(sizeof(PAT_T), "creat PAT struct");

        obj->ts_size = 188;
        obj->mode = MODE_PSI;
        strcpy(obj->file_i, "2009-04-28.ts");

        for(i = 0; i < PID_NUM; i++)
        {
                obj->pid_cc[i] = 0xFF;
        }
        obj->state = STATE_WAIT_PAT;

        if(1 == argc)
        {
                // no parameter
                printf("tsana: no input files, try --help.\n");
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
                                obj->pid = (uint16_t)(dat & 0x1FFF);
                        }
                        else if (0 == strcmp(argv[i], "-flt"))
                        {
                                sscanf(argv[++i], "%i" , &dat);
                                obj->pid = (uint16_t)(dat & 0x1FFF);
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
        obj->fd_i = open_file(obj->file_i, "rb", "read data");

        return obj;
}

int delete(OBJ *obj)
{
        if(NULL == obj)
        {
                return 0;
        }
        else
        {
                fclose(obj->fd_i);

                free(obj->ts);
                free(obj->af);
                free(obj->pat);
                free(obj);
                return 1;
        }
}

FILE *open_file( char *file, char *style, char *memo )
{
        FILE *fd;
        if ( NULL == ( fd = fopen( file, style ) ) )
        {
                printf( "Can not open file \"%s\" to %s!\n", file, memo );
                exit(EXIT_FAILURE);
        }
        return fd;
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

void sync_input(OBJ *obj)
{
        int syncByte;
        uint32_t add = 0;
        FILE *fd = obj->fd_i;

        fseek(fd, 0, SEEK_SET);
        while((syncByte = fgetc(fd)) != EOF)
        {
                if(syncByte == 0x47)
                {
                        fseek(fd, obj->ts_size - 1, SEEK_CUR);
                        if((syncByte = fgetc(fd)) != 0x47)
                        {
                                // not real sync byte
                                fseek(fd, -(obj->ts_size), SEEK_CUR);
                        }
                        else
                        {
                                // sync, go back
                                fseek(fd, -(obj->ts_size + 1), SEEK_CUR);
                                break;
                        }
                }
                add++;
        }
        if(0 != add)
        {
                printf("Find first sync byte at 0x%X in %s.\n",
                       add, obj->file_i);
        }
}

void get_TS(OBJ *obj)
{
        uint8_t dat;
        TS_T *ts = obj->ts;

        dat = *(obj->p)++;
        ts->sync_byte = dat;
        if(0x47 != ts->sync_byte)
        {
                printf("Wrong package head!\n");
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
}

void show_TS(OBJ *obj)
{
        TS_T *ts = obj->ts;

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
void adaption_fields(OBJ *obj)
{
        uint8_t dat;
        AF_T *af = obj->af;

        dat = *(obj->p)++;
        af->adaption_field_length = dat;
}

void get_PAT(OBJ *obj)
{
        uint8_t dat;
        uint32_t idx;
        uint32_t dat_cnt;
        PAT_T *pat = obj->pat;

        dat = *(obj->p)++;
        pat->table_id = dat;

        dat = *(obj->p)++;
        pat->sectin_syntax_indicator = (dat & BIT7) >> 7;
        pat->section_length = dat & 0x0F;

        dat = *(obj->p)++;
        pat->section_length <<= 8;
        pat->section_length |= dat;
        dat_cnt = pat->section_length;

        dat = *(obj->p)++;
        dat_cnt--;
        pat->transport_stream_id = dat;

        dat = *(obj->p)++;
        dat_cnt--;
        pat->transport_stream_id <<= 8;
        pat->transport_stream_id |= dat;

        dat = *(obj->p)++;
        dat_cnt--;
        pat->version_number = (dat & 0x3E) >> 1;
        pat->current_next_indicator = dat & BIT0;

        dat = *(obj->p)++;
        dat_cnt--;
        pat->section_number = dat;

        dat = *(obj->p)++;
        dat_cnt--;
        pat->last_section_number = dat;

        idx = 0;
        while(dat_cnt >= 8)
        {
                dat = *(obj->p)++;
                dat_cnt--;
                pat->pn2p[idx].program_number = dat;

                dat = *(obj->p)++;
                dat_cnt--;
                pat->pn2p[idx].program_number <<= 8;
                pat->pn2p[idx].program_number |= dat;

                dat = *(obj->p)++;
                dat_cnt--;
                pat->pn2p[idx].pid = dat & 0x1F;

                dat = *(obj->p)++;
                dat_cnt--;
                pat->pn2p[idx].pid <<= 8;
                pat->pn2p[idx].pid |= dat;

                idx++;
        }
        pat->program_cnt = idx;
}

void show_PAT(OBJ *obj)
{
        uint32_t idx;
        PAT_T *pat = obj->pat;

        printf("PAT:\n");
        printf("  0x%04X: section_length\n", pat->section_length);
        printf("  0x%04X: transport_stream_id\n", pat->transport_stream_id);

        idx = 0;
        while(idx < pat->program_cnt)
        {
                printf("    0x%04X(ProgNo) -> 0x%04X(PMT PID)\n",
                       pat->pn2p[idx].program_number,
                       pat->pn2p[idx].pid);
                idx++;
        }
}

//=============================================================================
// THE END.
//=============================================================================
