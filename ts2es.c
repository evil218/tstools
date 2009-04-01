//=============================================================================
// Name: ts2es.c
// Purpose: analyse certain character with ts file
// To build: gcc -o ts2es ts2es.c
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

int sizeofTS = 188; // default size of TS package
int sizeofES = 184; // max length of ES in TS package
int videoPID = 0x003A; // default video PID

//=============================================================================
// Sub-function declare:
//=============================================================================
void deal_with_parameter(int argc, char *argv[]);
FILE *open_file(char *file, char *style, char *memo);
unsigned char *malloc_mem(int size);
void show_help();

//=============================================================================
// The main function:
//=============================================================================
int main(int argc, char *argv[])
{
        int i;
        u32_t count_i;
        u32_t count_o;
        u08_t nread; // number readed
        u08_t *tsPkg;
        u08_t *esBuf;
        u08_t esLen;
        int syncByte;

        deal_with_parameter(argc, argv);
        tsPkg = malloc_mem(sizeofTS);
        esBuf = malloc_mem(sizeofES);
        fd_i = open_file( file_i, "rb", "read data" );
        fd_o = open_file( file_o, "wb", "write data" );
        printf("\nFrom %s to %s, video PID is 0x%04X\n", file_i, file_o, videoPID);

        count_i = 0;
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
        printf("Find first sync byte at 0x%X in file %s.\n", count_i, file_i);

        count_o = 0;
        while(nread = fread(tsPkg, 1, sizeofTS, fd_i))
        {
                u16_t pid;

                pid = tsPkg[0x01] & 0x1F;
                pid <<= 8;
                pid |= tsPkg[0x02];

                if(pid == videoPID)
                {
                        esLen = grabES(esBuf, tsPkg);
                        fwrite(esBuf, 1, esLen, fd_o);
                        count_o += esLen;
                }
        }
        fclose(fd_o);
        fclose(fd_i);
        free(tsPkg);
        free(esBuf);
        printf("File %s created, %d-data.\n", file_o, count_o);
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
                                sscanf(argv[++i], "%i" , &videoPID);
                                videoPID &= 0x1FFF;
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
                strcat(file_o, ".es");
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
        printf("Usage: ts2es [-n *] [--pid *] [-o *] TS_file\n");
        printf("Options:\n");
        printf("  -n <num>       Size of TS package, default: 188\n");
        printf("  --pid          Video PID, default: 0x003A\n");
        printf("  -o <file>      Output file name, default: *2.ts\n");
        printf("  --help         Display this information\n\n");
        printf("ts2es v1.00 by ZHOU Cheng, %s %s\n", __TIME__, __DATE__);
}

int grabES(u08_t *esBuf, u08_t *tsBuf)
{
        int esSize = sizeofTS - 4;
        int adaptLen, pesHeaderLen;
        int newPES    = (tsBuf[1] & 0x40) ? 1 : 0; // 1: have a PES header; 0: no PES header
        int adaptCtrl = (tsBuf[3] & 0x30) >> 4;    // 'adaption_field_control'
        u08_t *esPtr  = tsBuf + 4;

        switch(adaptCtrl)
        {
                case 1: // no adaption, only payload
                        if(newPES)
                        {
                                esPtr += 8; // point to 'PES_header_data_length'
                                pesHeaderLen = *esPtr; // 'PES_header_data_len'
                                esSize -= 9;
                                esPtr += 1;

                                esSize -= pesHeaderLen;
                                esPtr += pesHeaderLen;
                        }
                        else
                        {
                        }
                        memcpy(esBuf, esPtr, esSize);
                        break;
                case 2: // only adaption, no payload
                        esSize = 0;
                        break;
                case 3: // payload after adaption
                        adaptLen = *esPtr; // 'adaption_field_len'
                        esSize -= 1;
                        esPtr += 1; // point to the first byte of adaption field (if have)

                        esSize -= adaptLen;
                        esPtr += adaptLen; // point to the first byte of payload

                        if(newPES)
                        {
                                esPtr += 8; // point to 'PES_header_data_length'
                                pesHeaderLen = *esPtr; // 'PES_header_data_len'
                                esSize -= 9;
                                esPtr += 1;

                                esSize -= pesHeaderLen;
                                esPtr += pesHeaderLen;
                        }
                        else
                        {
                        }
                        memcpy(esBuf, esPtr, esSize);
                        break;
                case 0:
                default:
                        printf("\n'adaptiong_field_control' error!");
                        break;
        }

        return esSize;
}

//=============================================================================
// THE END.
//=============================================================================
