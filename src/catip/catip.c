/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: catip.c
 * funx: generate text data file with bin data file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for strcmp, etc */

#include "libts/error.h"
#include "libts/if.h"
#include "net/url.h"

static URL *fd_i = NULL;
static char file_i[FILENAME_MAX] = "";
static int npline = 188; /* data number per line */
static ts_pkt_t PKT;
static ts_pkt_t *pkt = &PKT;

static int deal_with_parameter(int argc, char *argv[]);
static void show_help();
static void show_version();

int main(int argc, char *argv[])
{
        unsigned char bbuf[ 204 + 10]; /* bin data buffer */
        char tbuf[1024 + 10]; /* txt data buffer */

        if(0 != deal_with_parameter(argc, argv))
        {
                return -1;
        }

        fd_i = url_open(file_i, "rb");
        if(NULL == fd_i)
        {
                DBG(ERR_FOPEN_FAILED, "\n");
                return -ERR_FOPEN_FAILED;
        }

        pkt_init(pkt);
        pkt->ts = (bbuf + 0);
        pkt->addr = &(pkt->ADDR);

        pkt->ADDR = 0;
        while(1 == url_read(bbuf, npline, 1, fd_i))
        {
                b2t(tbuf, pkt);
                puts(tbuf);
                pkt->ADDR += npline;
        }

        url_close(fd_i);

        return 0;
}

static int deal_with_parameter(int argc, char *argv[])
{
        int i;

        if(1 == argc)
        {
                /* no parameter */
                fprintf(stderr, "No URL to process...\n\n");
                show_help();
                return -1;
        }

        for(i = 1; i < argc; i++)
        {
                if('-' == argv[i][0])
                {
                        if(0 == strcmp(argv[i], "-h") ||
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
                                DBG(ERR_BAD_ARG, "\n");
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
        puts("'catip' read TS over IP, translate 0xXX to 'XY ' format, then send to stdout.");
        puts("");
        puts("Usage: catip [OPTION] udp://@xxx.xxx.xxx.xxx:xxxx [OPTION]");
        puts("");
        puts("Options:");
        puts("");
        puts(" -h, --help       print this information, then exit");
        puts(" -v, --version    print my version, then exit");
        puts("");
        puts("Examples:");
        puts("  catip udp://@:1234");
        puts("  catip udp://@224.165.54.210:1234");
        puts("");
        puts("Report bugs to <zhoucheng@tsinghua.org.cn>.");
        return;
}

static void show_version()
{
        puts("catip 1.0.0");
        puts("");
        puts("Copyright (C) 2009,2010,2011 ZHOU Cheng.");
        puts("This is free software; contact author for additional information.");
        puts("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR");
        puts("A PARTICULAR PURPOSE.");
        puts("");
        puts("Written by ZHOU Cheng.");
        return;
}
