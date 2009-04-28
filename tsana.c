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

#define PID_NUM                         0x2000

//=============================================================================
// enum & struct definition:
//=============================================================================
enum
{
        NO_ERROR = 0,
        FOPEN_ERROR,
        MALLOC_ERROR,
        PARA_ERROR
};

enum
{
        MODE_PSI,
        MODE_PCR,
        MODE_CC
};

//=============================================================================
// Variables definition:
//=============================================================================
FILE *fd_i;
FILE *fd_o;

char file_i[FILENAME_MAX] = "";
char file_o[FILENAME_MAX] = "";

int sizeofTS = 188;  // Size of TS package
int mode = MODE_CC; // default mode: Continuity Counter
uint8_t pid_cc[PID_NUM];

//=============================================================================
// Sub-function declare:
//=============================================================================
void deal_with_parameter(int argc, char *argv[]);
FILE *open_file(char *file, char *style, char *memo);
unsigned char *malloc_mem(int size);
void show_help();
void printb(uint32_t x, int head, int tail);

//=============================================================================
// The main function:
//=============================================================================
int main(int argc, char *argv[])
{
        int i;
        uint32_t addr;
        int lost;
        uint8_t cc;
        int nread; // number readed
        uint8_t *line;
        int is_ok = 1;

        for(i = 0; i < PID_NUM; i++)
        {
                pid_cc[i] = 0xFF;
        }

        deal_with_parameter(argc, argv);
        line = malloc_mem(sizeofTS);
        fd_i = open_file( file_i, "rb", "read data" );

        addr = -sizeofTS;
        while(nread = fread(line, 1, sizeofTS, fd_i))
        {
                uint16_t pid;

                addr += sizeofTS;
                if(0x47 != line[0x00])
                {
                        printf("Wrong package head!\n");
                        break;
                }

                pid = line[0x01];
                pid <<= 8;
                pid |= line[0x02];
                pid &= 0x1FFF;
                cc = line[0x03] & 0x0F;

                if(MODE_CC == mode)
                {
                        if(0xFF == pid_cc[pid])
                        {
                                pid_cc[pid] = cc;
                                continue;
                        }
                        else
                        {
                                pid_cc[pid]++;
                                pid_cc[pid] &= 0x0F;
                                if(0x1001 == pid)
                                {
                                        // PCR package, always 0
                                        pid_cc[pid] = 0;
                                }
                                if(pid_cc[pid] == cc)
                                {
                                        is_ok = 1;
                                        continue;
                                }
                        }

                        lost = cc - pid_cc[pid];
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
                        printf(",wait %X but %X lost %2d", pid_cc[pid], cc, lost);
                        printf("\n");
                        pid_cc[pid] = cc;
                        is_ok = 0;
                }

                else if(MODE_PCR == mode)
                {
                        if((line[0x03] & 0x20) && (line[0x05] & 0x10))
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
                                //printf(",%13lu", pcr); // error!
                                printf(",%10lu", pcr_base);
                                printf(",%3u", pcr_ext);
                                printf("\n");
                        }
                }

                else if(MODE_PSI == mode)
                {
                        continue;
                }
        }
        fclose(fd_i);
        free(line);
        exit(NO_ERROR);
}

//=============================================================================
// Subfunctions definition:
//=============================================================================
void deal_with_parameter(int argc, char *argv[])
{
        int i = 1;
        char str[256];

        if(1 == argc)
        {
                // no parameter
                printf("No file to process...\n\n");
                show_help();
                exit(NO_ERROR);
        }
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
                                sizeofTS = atoi(argv[++i]);
                                if(188 != sizeofTS && 204 != sizeofTS)
                                {
                                        sizeofTS = 188;
                                }
                        }
                        else if (0 == strcmp(argv[i], "--mode"))
                        {
                                strcpy(str, argv[++i]);
                                if(0 == strcmp("PSI", str))
                                {
                                        mode = MODE_PSI;
                                }
                                else if(0 == strcmp("PCR", str))
                                {
                                        mode = MODE_PCR;
                                }
                                else
                                {
                                        mode = MODE_CC;
                                }
                        }
                        else if (0 == strcmp(argv[i], "--help"))
                        {
                                show_help();
                                exit(NO_ERROR);
                        }
                        else
                        {
                                printf("Wrong parameter: %s\n", argv[i]);
                                exit(PARA_ERROR);
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
                // fill file_o[]
                file_o[i--] = '\0';
                while(i >= 0)
                {
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
                exit(FOPEN_ERROR);
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
                exit(MALLOC_ERROR);
        }
        return ptr;
}

void show_help()
{
        printf("Usage: tsana [-n *] [--pid *] [-o *] TS_file\n");
        printf("Options:\n");
        printf("  -n <num>       Size of TS package, default: 188\n");
        printf("  --mode <mode>  CC, PCR, PSI... default: CC\n");
        printf("  -o <file>      Output file name, default: *2.ts\n");
        printf("  --help         Display this information\n\n");
        printf("tsana v1.00 by ZHOU Cheng, %s %s\n", __TIME__, __DATE__);
}

void printb(uint32_t x, int head, int tail)
{
        int i;
        uint32_t mask;

        if(tail > head) tail = head;
        printf(",");
        for(i = head; i >= tail; i--)
        {
                mask = 1 << i;
                printf("%c", (x & mask) ? '1' : '0');
        }
}

//=============================================================================
// THE END.
//=============================================================================
