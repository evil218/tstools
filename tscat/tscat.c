/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: tscat.c
// Purpose: generate text data file with bin data file
// To build: gcc -std=c99 -o tscat tscat.c
// Copyright (C) 2008 by ZHOU Cheng. All right reserved.
//=============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strcmp, etc

#include "error.h"

#define MAX_STRING_LENGTH 256

//=============================================================================
// Variables definition:
//=============================================================================
static FILE *fd_i = NULL;
static char file_i[FILENAME_MAX] = "";
static int npline = 188; // data number per line

// for high-speed bin to txt convert
static const char *b2t_table[256] =
{
        "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B", "0C", "0D", "0E", "0F",
        "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "1A", "1B", "1C", "1D", "1E", "1F",
        "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "2A", "2B", "2C", "2D", "2E", "2F",
        "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3A", "3B", "3C", "3D", "3E", "3F",
        "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4A", "4B", "4C", "4D", "4E", "4F",
        "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "5A", "5B", "5C", "5D", "5E", "5F",
        "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6A", "6B", "6C", "6D", "6E", "6F",
        "70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "7A", "7B", "7C", "7D", "7E", "7F",
        "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F",
        "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9A", "9B", "9C", "9D", "9E", "9F",
        "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF",
        "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "BA", "BB", "BC", "BD", "BE", "BF",
        "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA", "CB", "CC", "CD", "CE", "CF",
        "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF",
        "E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF",
        "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "FA", "FB", "FC", "FD", "FE", "FF"
};

//=============================================================================
// Sub-function declare:
//=============================================================================
static int deal_with_parameter(int argc, char *argv[]);
static void show_help();

//=============================================================================
// The main function:
//=============================================================================
int main(int argc, char *argv[])
{
        int b;
        int t;
        unsigned char bbuf[ 204 + 10]; // bin data buffer
        char tbuf[1024 + 10]; // txt data buffer

        if(0 != deal_with_parameter(argc, argv))
        {
                return -1;
        }

        fd_i = fopen(file_i, "rb");
        if(NULL == fd_i)
        {
                DBG(ERR_FOPEN_FAILED);
                return -ERR_FOPEN_FAILED;
        }

        while(1 == fread(bbuf, npline, 1, fd_i))
        {
                for(b = 0, t = 0; b < npline; b++, t++)
                {
                        const char *ch = b2t_table[bbuf[b]];

                        tbuf[t++] = *ch++;
                        tbuf[t++] = *ch;
                        tbuf[t] = ' ';
                }
                tbuf[t] = '\0';
                puts(tbuf);
        }

        fclose(fd_i);

        return 0;
}

//=============================================================================
// Subfunctions definition:
//=============================================================================
static int deal_with_parameter(int argc, char *argv[])
{
        int i;

        if(1 == argc)
        {
                // no parameter
                fprintf(stderr, "No binary file to process...\n\n");
                show_help();
                return -1;
        }

        for(i = 1; i < argc; i++)
        {
                if ('-' == argv[i][0])
                {
                        if (0 == strcmp(argv[i], "--help"))
                        {
                                show_help();
                                return -1;
                        }
                        else
                        {
                                fprintf(stderr, "Wrong parameter: %s\n", argv[i]);
                                DBG(ERR_BAD_ARG);
                                return -ERR_BAD_ARG;
                        }
                }
                else
                {
                        strcpy(file_i, argv[i]);
                }
        }

        return 0;
}

static void show_help()
{
        puts("Usage: tscat [options] bin_file [options]");
        puts("Options:");
        puts("  --help         Display this information");
        puts("");
        fprintf(stdout, "tscat v1.00 by ZHOU Cheng, %s %s\n", __TIME__, __DATE__);
        return;
}

#if 0
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
                fprintf(stdout, "Find first sync byte at 0x%lX in %s.\n",
                        obj->addr, obj->file_i);
        }
}
#endif

//=============================================================================
// THE END.
//=============================================================================
