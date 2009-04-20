//=============================================================================
// Name: tsana.c
// Purpose: analyse certain character with ts file
// To build: gcc -o tsana tsana.c
// Copyright (C) 2009 by ZHOU Cheng. All right reserved.
//=============================================================================
#include <stdio.h>
#include <stdlib.h>

#include "def.h"

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

//=============================================================================
// Variables definition:
//=============================================================================
FILE *fd_i;
FILE *fd_o;

char file_i[FILENAME_MAX] = "";
char file_o[FILENAME_MAX] = "";

int sizeofTS = 188; // Size of TS package
int conPID = 0x0000; // default concerned PID

//=============================================================================
// Sub-function declare:
//=============================================================================
void deal_with_parameter(int argc, char *argv[]);
FILE *open_file(char *file, char *style, char *memo);
unsigned char *malloc_mem(int size);
void show_help();
void printb(u32_t x, int head, int tail);

//=============================================================================
// The main function:
//=============================================================================
int main(int argc, char *argv[])
{
        int i;
        u32_t count;
        int con_cnt = -1; // low 4-bits in line[0x03]
        int nread; // number readed
        u08_t *line;

        deal_with_parameter(argc, argv);
        line = malloc_mem(sizeofTS);
        fd_i = open_file( file_i, "rb", "read data" );

        count = 0;
        while(nread = fread(line, 1, sizeofTS, fd_i))
        {
                u16_t pid;

                if(0x47 != line[0x00])
                {
                        printf("Wrong package head!\n");
                        break;
                }

                pid = line[0x01];
                pid <<= 8;
                pid |= line[0x02];
                pid &= 0x1FFF;

                if(conPID == pid)
                {
                        int cc;
                        printf("0x%08X", (int)count);
                        printf(",0x%02X", (int)line[0x00]);
                        printb(line[0x01], 7,7);
                        printb(line[0x01], 6,6);
                        printb(line[0x01], 5,5);
                        printf(",0x%04X", (int)pid);
                        printb(line[0x03], 7,6);
                        printb(line[0x03], 5,4);
                        cc = (int)(line[0x03] & 0x0F);
                        printf(",%2d", cc);
                        if(-1 == con_cnt)
                        {
                                con_cnt = cc;
                        }
                        else
                        {
                                con_cnt++;
                                con_cnt &= 0x0F;
                                if(con_cnt != cc)
                                {
                                        printf(",error");
                                        con_cnt = cc;
                                }
                        }
                        if((line[0x03] & 0x20) && (line[0x05] & 0x10))
                        {
                                u64_t pcr_base = 0;
                                u16_t pcr_ext = 0;
                                u64_t pcr;

                                pcr_base  |= line[0x06];
                                pcr_base <<= 8;
                                pcr_base  |= line[0x07];
                                pcr_base <<= 8;
                                pcr_base  |= line[0x08];
                                pcr_base <<= 8;
                                pcr_base  |= line[0x09];
                                pcr_base <<= 1;
                                pcr_base  |= ((line[0x0A] & 0x80) ? 0x01 : 0x00);
                                printf(",%20ld", pcr_base);

                                pcr_ext  |= (line[0x0A] & 0x01);
                                pcr_ext <<= 8;
                                pcr_ext  |= line[0x0B];
                                printf(",%3d", (int)pcr_ext);

                                pcr = pcr_base * 300 + pcr_ext;
                                printf(",%20ld", pcr);
                        }
                        printf("\n");
                }
                count += sizeofTS;
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
                        else if (0 == strcmp(argv[i], "--pid"))
                        {
                                sscanf(argv[++i], "%i", &conPID);
                                conPID &= 0x1FFF;
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
        printf("  --pid <num>    Concerned PID, default: 0x0000\n");
        printf("  -o <file>      Output file name, default: *2.ts\n");
        printf("  --help         Display this information\n\n");
        printf("tsana v1.00 by ZHOU Cheng, %s %s\n", __TIME__, __DATE__);
}

void printb(u32_t x, int head, int tail)
{
        int i;
        u32_t mask;

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
