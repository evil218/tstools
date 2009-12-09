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
FILE *fd_i = NULL;
char file_i[FILENAME_MAX] = "";

int npline = 188; // data number per line

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
        unsigned char tbuf[1024 + 10]; // txt data buffer

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
                        unsigned char x;

                        x = bbuf[b]; // 0xXY
                        x = ((x >> 4) & 0x0f);
                        x = (x > 9) ? x - 10 + 'A' : x + '0';
                        tbuf[t++] = x;

                        x = bbuf[b]; // 0xXY
                        x = ((x >> 0) & 0x0f);
                        x = (x > 9) ? x - 10 + 'A' : x + '0';
                        tbuf[t++] = x;

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

//=============================================================================
// THE END.
//=============================================================================
