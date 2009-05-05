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
transport_packet_t;

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
adaption_fields_t;

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
program_association_section_t;

//=============================================================================
// Variables definition:
//=============================================================================
FILE *fd_i;
FILE *fd_o;

char file[FILENAME_MAX] = ""; // input file name without postfix
char file_i[FILENAME_MAX] = "2009-04-28.ts";
char file_o[FILENAME_MAX] = "";

uint32_t sizeofTS = 188;  // Size of TS package
int mode = MODE_PSI; // default mode: Continuity Counter
uint8_t pid_cc[PID_NUM];
uint16_t pid;

transport_packet_t TS, *ts;
program_association_section_t PAT, *pat;

//=============================================================================
// Sub-function declare:
//=============================================================================
void deal_with_parameter(int argc, char *argv[]);
FILE *open_file(char *file, char *style, char *memo);
unsigned char *malloc_mem(int size);
void show_help();
void show_version();
void sync_input();
char *printb(uint32_t x, int bit_cnt);
void get_TS(transport_packet_t *ts, uint8_t **p);
void show_TS(transport_packet_t *ts);
void adaption_fields(adaption_fields_t *af, uint8_t **p);
void get_PAT(program_association_section_t *pat, uint8_t **p);
void show_PAT(program_association_section_t *pat);

//=============================================================================
// The main function:
//=============================================================================
int main(int argc, char *argv[])
{
        uint32_t i;
        uint32_t addr;
        int lost;
        uint8_t *line;
        uint8_t *dat;
        int is_ok = 1;

        for(i = 0; i < PID_NUM; i++)
        {
                pid_cc[i] = 0xFF;
        }
        ts = &TS;
        pat = &PAT;

        deal_with_parameter(argc, argv);
        line = malloc_mem(sizeofTS);
        fd_i = open_file( file_i, "rb", "read data" );
        sync_input();

        addr = -sizeofTS;
        while(sizeofTS == fread(line, 1, sizeofTS, fd_i))
        {
                addr += sizeofTS;
                dat = line;
                ts->sync_byte = *dat++; // 0x00
                if(0x47 != ts->sync_byte)
                {
                        printf("Wrong package head!\n");
                        break;
                }

                get_TS(ts, &dat);
                if(BIT1 & ts->adaption_field_control)
                {
                        //adaption_field();
                }
                if(BIT0 & ts->adaption_field_control)
                {
                        dat++; // 0x04
                }

                if(MODE_PSI == mode)
                {
                        if(0x0000 != ts->PID)
                        {
                                continue;
                        }
                        show_TS(ts);

                        get_PAT(pat, &dat);
                        show_PAT(pat);
                        break;
                }
                else if(MODE_CC == mode)
                {
                        if(0xFF == pid_cc[pid])
                        {
                                pid_cc[pid] = ts->continuity_counter;
                                continue;
                        }
                        else
                        {
                                pid_cc[pid]++;
                                pid_cc[pid] &= 0x0F;
                                if(0x1001 == pid || 0x1FFF == pid)
                                {
                                        // PCR or NULL package, always 0
                                        pid_cc[pid] = 0;
                                }
                                if(pid_cc[pid] == ts->continuity_counter)
                                {
                                        is_ok = 1;
                                        continue;
                                }
                        }

                        lost = ts->continuity_counter - pid_cc[pid];
                        if(lost < 0)
                        {
                                lost += 16;
                        }

                        if(is_ok)
                        {
                                printf("\n");
                        }
                        printf("0x%08X", (int)addr);
                        printf(",0x%04X", (int)pid);
                        printf(",wait %X but %X lost %2d", pid_cc[pid], ts->continuity_counter, lost);
                        printf("\n");
                        pid_cc[pid] = ts->continuity_counter;
                        is_ok = 0;
                }

                else if(MODE_PCR == mode)
                {
                        if(BIT1 & ts->adaption_field_control)
//                           && (line[0x05] & 0x10))
                        {
                                uint64_t pcr_base = 0; // 33-bit
                                uint16_t pcr_ext = 0;  //  9-bit
                                uint64_t pcr;

                                pcr_base  |= line[0x06];
                                pcr_base <<= 8;
                                pcr_base  |= line[0x07];
                                pcr_base <<= 8;
                                pcr_base  |= line[0x08];
                                pcr_base <<= 8;
                                pcr_base  |= line[0x09];
                                pcr_base <<= 1;
                                pcr_base  |= ((line[0x0A] & 0x80) ? 0x01 : 0x00);

                                pcr_ext  |= (line[0x0A] & 0x01);
                                pcr_ext <<= 8;
                                pcr_ext  |= line[0x0B];

                                pcr = pcr_base * 300 + pcr_ext;

                                printf("0x%08X", addr);
                                printf(",%10u", addr);
                                printf(",0x%04X", (int)pid);
                                printf(",%13llu", pcr);
                                printf(",%10llu", pcr_base);
                                printf(",%3u", pcr_ext);
                                printf("\n");
                        }
                }
        }
        fclose(fd_i);
        free(line);
        exit(EXIT_SUCCESS);
}

//=============================================================================
// Subfunctions definition:
//=============================================================================
void deal_with_parameter(int argc, char *argv[])
{
        int i;
        int dat;

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
                                strcpy(file_o, argv[++i]);
                        }
                        else if (0 == strcmp(argv[i], "-n"))
                        {
                                sscanf(argv[++i], "%i" , &dat);
                                sizeofTS = (204 == dat) ? 204 : 108;
                        }
                        else if (0 == strcmp(argv[i], "-psi"))
                        {
                                mode = MODE_PSI;
                        }
                        else if (0 == strcmp(argv[i], "-cc"))
                        {
                                mode = MODE_CC;
                        }
                        else if (0 == strcmp(argv[i], "-pcr"))
                        {
                                mode = MODE_PCR;
                        }
                        else if (0 == strcmp(argv[i], "-debug"))
                        {
                                mode = MODE_DEBUG;
                        }
                        else if (0 == strcmp(argv[i], "-pes"))
                        {
                                sscanf(argv[++i], "%i" , &dat);
                                pid = (uint16_t)(dat & 0x1FFF);
                        }
                        else if (0 == strcmp(argv[i], "-flt"))
                        {
                                sscanf(argv[++i], "%i" , &dat);
                                pid = (uint16_t)(dat & 0x1FFF);
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
                        strcpy(file_i, argv[i]);
                        i = argc;
                }
                i++;
        }
        // make output file name with input file name
        if('\0' == file_o[0])
        {
                // search last '.'
                i = strlen(file_i) - 1;
                while(i >= 0)
                {
                        if('.' == file_i[i]) break;
                        i--;
                }
                // fill file_o[] and file[]
                file[i--] = '\0';
                file_o[i--] = '\0';
                while(i >= 0)
                {
                        file[i] = file_i[i];
                        file_o[i] = file_i[i];
                        i--;
                }
                strcat(file_o, "2.ts");
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
        fseek( fd, 0, SEEK_SET );
        return fd;
}

unsigned char *malloc_mem(int size)
{
        unsigned char *ptr = NULL;

        ptr = (unsigned char *)malloc(size);
        if(NULL == ptr)
        {
                printf("Can not malloc %d-byte memory!\n", size);
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

void sync_input()
{
        int syncByte;
        uint32_t count_i = 0;

        while((syncByte = fgetc(fd_i)) != EOF)
        {
                if(syncByte == 0x47)
                {
                        fseek(fd_i, sizeofTS - 1, SEEK_CUR);
                        if((syncByte = fgetc(fd_i)) != 0x47)
                        {
                                // not real sync byte
                                fseek(fd_i, -(sizeofTS), SEEK_CUR);
                        }
                        else
                        {
                                // sync, go back
                                fseek(fd_i, -(sizeofTS + 1), SEEK_CUR);
                                break;
                        }
                }
                count_i++;
        }
        if(0 != count_i)
        {
                printf("Find first sync byte at 0x%X in %s.\n",
                       count_i, file_i);
        }
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

void get_TS(transport_packet_t *ts, uint8_t **p)
{
        uint8_t dat;

        dat = *(*p)++;
        ts->transport_error_indicator = (dat & BIT7) >> 7;
        ts->payload_unit_start_indicator = (dat & BIT6) >> 6;
        ts->transport_priority = (dat & BIT5) >> 5;
        ts->PID = dat & 0x1F;

        dat = *(*p)++;
        ts->PID <<= 8;
        ts->PID |= dat;

        dat = *(*p)++;
        ts->transport_scrambling_control = (dat & (BIT7 | BIT6)) >> 6;
        ts->adaption_field_control = (dat & (BIT5 | BIT4)) >> 4;;
        ts->continuity_counter = dat & 0x0F;
}

void show_TS(transport_packet_t *ts)
{
        printf("TS:\n");
        printf("  sync_byte: 0x%02X\n", ts->sync_byte);
        printf("  transport_error_indicator: %c\n", (ts->transport_error_indicator) ? '1' : '0');
        printf("  payload_unit_start_indicator: %c\n", (ts->payload_unit_start_indicator) ? '1' : '0');
        printf("  transport_priority: %c\n", (ts->transport_priority) ? '1' : '0');
        printf("  PID: 0x%04X\n", ts->PID);
        printf("  transport_scrambling_control: 0b%s\n", printb(ts->transport_scrambling_control, 2));
        printf("  adaption_field_control: 0b%s\n", printb(ts->adaption_field_control, 2));
        printf("  continuity_counter 0x%X\n", ts->continuity_counter);
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
void adaption_fields(adaption_fields_t *af, uint8_t **p)
{
        uint8_t dat;

        dat = *(*p)++;
        af->adaption_field_length = dat;
}

void get_PAT(program_association_section_t *pat, uint8_t **p)
{
        uint8_t dat;
        uint32_t idx;
        uint32_t dat_cnt;

        dat = *(*p)++;
        pat->table_id = dat;

        dat = *(*p)++;
        pat->sectin_syntax_indicator = (dat & BIT7) >> 7;
        pat->section_length = dat & 0x0F;

        dat = *(*p)++;
        pat->section_length <<= 8;
        pat->section_length |= dat;

        dat = *(*p)++;
        pat->transport_stream_id = dat;

        dat = *(*p)++;
        pat->transport_stream_id <<= 8;
        pat->transport_stream_id |= dat;

        dat = *(*p)++;
        pat->version_number = (dat & 0x3E) >> 1;
        pat->current_next_indicator = dat & BIT0;

        dat = *(*p)++;
        pat->section_number = dat;

        dat = *(*p)++;
        pat->last_section_number = dat;

        idx = 0;
        dat_cnt = pat->section_length - 5;
        while(dat_cnt >= 8)
        {
                dat = *(*p)++;
                pat->pn2p[idx].program_number = dat;

                dat = *(*p)++;
                pat->pn2p[idx].program_number <<= 8;
                pat->pn2p[idx].program_number |= dat;

                dat = *(*p)++;
                pat->pn2p[idx].pid = dat & 0x1F;

                dat = *(*p)++;
                pat->pn2p[idx].pid <<= 8;
                pat->pn2p[idx].pid |= dat;

                idx++;
                dat_cnt -= 4;
        }
        pat->program_cnt = idx;
}

void show_PAT(program_association_section_t *pat)
{
        uint32_t idx;

        printf("PAT:\n");
        printf("  section length %u.\n", pat->section_length);
        printf("  TS ID 0x%04X.\n", pat->transport_stream_id);

        idx = 0;
        while(idx < pat->program_cnt)
        {
                printf("ProgNo 0x%04X -> PMT PID 0x%04X.\n",
                       pat->pn2p[idx].program_number, pat->pn2p[idx].pid);
                idx++;
        }
}

//=============================================================================
// THE END.
//=============================================================================
