/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: tobin.c
// Purpose: generate text data file with bin data file
// To build: gcc -std=c99 -o tobin tobin.c
// Copyright (C) 2008 by ZHOU Cheng. All right reserved.
//=============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strcmp, etc

#include "error.h"
#include "if.h"

//=============================================================================
// Variables definition:
//=============================================================================
static FILE *fd_o = NULL;
static char file_o[FILENAME_MAX] = "";

//=============================================================================
// Sub-function declare:
//=============================================================================
static int deal_with_parameter(int argc, char *argv[]);
static void show_help();
static void show_version();

//=============================================================================
// The main function:
//=============================================================================
int main(int argc, char *argv[])
{
        int size;
        char tbuf[LINE_LENGTH_MAX + 10]; // txt data buffer
        unsigned char bbuf[ LINE_LENGTH_MAX / 3 + 10]; // bin data buffer

        if(0 != deal_with_parameter(argc, argv))
        {
                return -1;
        }

        fd_o = fopen(file_o, "wb");
        if(NULL == fd_o)
        {
                DBG(ERR_FOPEN_FAILED);
                return -ERR_FOPEN_FAILED;
        }

        while(NULL != fgets(tbuf, LINE_LENGTH_MAX, stdin))
        {
                size = t2b(bbuf, tbuf);
                fwrite(bbuf, size, 1, fd_o);
        }

        fclose(fd_o);

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
                fprintf(stderr, "No binary file to write...\n\n");
                show_help();
                return -1;
        }

        for(i = 1; i < argc; i++)
        {
                if('-' == argv[i][0])
                {
                        if(     0 == strcmp(argv[i], "-h") ||
                                0 == strcmp(argv[i], "--help")
                        )
                        {
                                show_help();
                                return -1;
                        }
                        else if(0 == strcmp(argv[i], "-v") ||
                                0 == strcmp(argv[i], "--version")
                        )
                        {
                                show_version();
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
                        strcpy(file_o, argv[i]);
                }
        }

        return 0;
}

static void show_help()
{
        puts("'tobin' read from stdin, translate 'XY ' to 0xXY, send to file.");
        puts("");
        puts("Usage: tobin [OPTION] file [OPTION]");
        puts("");
        puts("Options:");
        puts("");
        puts(" -h, --help       print this information, then exit");
        puts(" -v, --version    print my version, then exit");
        puts("");
        puts("Examples:");
        puts("  tobin xxx.ts");
        puts("");
        puts("Report bugs to <zhoucheng@tsinghua.org.cn>.");
        return;
}

static void show_version()
{
        //fprintf(stdout, "tobin 0.1.0 (by Cygwin), %s %s\n", __TIME__, __DATE__);
        puts("tobin 1.0.0");
        puts("");
        puts("Copyright (C) 2009,2010 ZHOU Cheng.");
        puts("This is free software; contact author for additional information.");
        puts("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR");
        puts("A PARTICULAR PURPOSE.");
        puts("");
        puts("Written by ZHOU Cheng.");
        return;
}

//=============================================================================
// THE END.
//=============================================================================
