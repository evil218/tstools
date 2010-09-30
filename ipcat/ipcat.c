/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: ipcat.c
// Purpose: generate text data file with bin data file
// To build: gcc -std=c99 -o ipcat ipcat.c
// Copyright (C) 2008 by ZHOU Cheng. All right reserved.
//=============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strcmp, etc
#include <sys/time.h> // for gettimeofday(), etc

#include "error.h"
#include "if.h"
#include "url.h"

//=============================================================================
// Variables definition:
//=============================================================================
static URL *fd_i = NULL;
static char file_i[FILENAME_MAX] = "";
static int npline = 188; // data number per line
static char white_space = ' ';

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
        unsigned char bbuf[ 204 + 10]; // bin data buffer
        char tbuf[1024 + 10]; // txt data buffer
        struct timeval tv;
        uint64_t timestamp; // unit: ns

        if(0 != deal_with_parameter(argc, argv))
        {
                return -1;
        }

        fd_i = url_open(file_i, "rb");
        if(NULL == fd_i)
        {
                DBG(ERR_FOPEN_FAILED);
                return -ERR_FOPEN_FAILED;
        }

        while(1 == url_read(bbuf, npline, 1, fd_i))
        {
                gettimeofday(&tv, NULL);
                timestamp = tv.tv_sec;
                timestamp *= 1e6;
                timestamp += tv.tv_usec;
                timestamp *= 1e3;

                b2t(tbuf, bbuf, npline, white_space);
                fprintf(stdout, "%016llX%c", timestamp, white_space);
                puts(tbuf);
        }

        url_close(fd_i);

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
                fprintf(stderr, "No URL to process...\n\n");
                show_help();
                return -1;
        }

        for(i = 1; i < argc; i++)
        {
                if('-' == argv[i][0])
                {
                        if(     0 == strcmp(argv[i], "-s") ||
                                0 == strcmp(argv[i], "--space")
                        )
                        {
                                sscanf(argv[++i], "%c" , &white_space);
                        }
                        else if(0 == strcmp(argv[i], "-h") ||
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
                        strcpy(file_i, argv[i]);
                }
        }

        return 0;
}

static void show_help()
{
        puts("'ipcat' read TS over IP, translate 0xXX to 'XY ' format, then send to stdout.");
        puts("");
        puts("Usage: ipcat [OPTION] udp://@xxx.xxx.xxx.xxx:xxxx [OPTION]");
        puts("");
        puts("Options:");
        puts("");
        puts(" -s, --space 's'  white space, default: ' '");
        puts(" -h, --help       print this information, then exit");
        puts(" -v, --version    print my version, then exit");
        puts("");
        puts("Examples:");
        puts("  ipcat udp://@22.4165.54.210:1234");
        puts("");
        puts("Report bugs to <zhoucheng@tsinghua.org.cn>.");
        return;
}

static void show_version()
{
        //fprintf(stdout, "ipcat 0.1.0 (by Cygwin), %s %s\n", __TIME__, __DATE__);
        puts("ipcat 1.0.0");
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
