/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: catbin.c
 * funx: generate text line with bin data file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for strcmp, etc */
#include <stdint.h> /* for uintN_t, etc */

#include "libts/common.h"
#include "libts/error.h"
#include "libts/if.h"

enum FILE_TYPE
{
        FILE_MTS,
        FILE_TSRS,
        FILE_TS,
        FILE_BIN,
        FILE_UNKNOWN
};

static FILE *fd_i = NULL;
static char file_i[FILENAME_MAX] = "";
static int npline = 188; /* data number per line */
static char white_space = ' ';
static int type = FILE_BIN;
static int show_address = 0;
static int dec_address = 0; /* default: hex address */
static int aim_start = 0; /* first byte */
static int aim_stop = 0; /* last byte */
static ts_pkt_t PKT;
static ts_pkt_t *pkt = &PKT;

static int deal_with_parameter(int argc, char *argv[]);
static int show_help();
static int show_version();
static int judge_type();
static int mts_time(uint64_t *mts, uint8_t *bin);

int main(int argc, char *argv[])
{
        unsigned char bbuf[LINE_LENGTH_MAX / 3 + 10]; /* bin data buffer */
        char tbuf[LINE_LENGTH_MAX + 10]; /* txt data buffer */
        char *addr_fmt;

        if(0 != deal_with_parameter(argc, argv))
        {
                return -1;
        }
        addr_fmt = (dec_address) ? "%llu%c" : "%llX%c";

        fd_i = fopen(file_i, "rb");
        if(NULL == fd_i)
        {
                fprintf(stderr, "%s ", file_i);
                DBG(ERR_FOPEN_FAILED, "\n");
                return -ERR_FOPEN_FAILED;
        }

        judge_type();
        while(1 == fread(bbuf, npline, 1, fd_i))
        {
                pkt->ts = NULL;
                pkt->rs = NULL;
                pkt->src = NULL;
                pkt->addr = NULL;
                pkt->cts = NULL;
                pkt->dat = NULL;

                switch(type)
                {
                        case FILE_TSRS:
                                pkt->addr = &(pkt->ADDR);
                                pkt->ts = (bbuf + 0);
                                pkt->rs = (bbuf + 188);
                                break;
                        case FILE_MTS:
                                pkt->addr = &(pkt->ADDR);
                                pkt->ts = (bbuf + 4);
                                pkt->cts = &(pkt->CTS);
                                mts_time(pkt->cts, bbuf);
                                pkt->CTS_overflow = 0x40000000;
                                break;
                        case FILE_TS:
                                pkt->addr = &(pkt->ADDR);
                                pkt->ts = (bbuf + 0);
                                break;
                        default: /* FILE_BIN */
                                pkt->ts = (bbuf + 0);
                                break;
                }
                b2t(tbuf, pkt, white_space);
                if(show_address)
                {
                        fprintf(stdout, addr_fmt, pkt->ADDR, white_space);
                }
                puts(tbuf);
                pkt->ADDR += npline;
        }

        fclose(fd_i);

        return 0;
}

static int deal_with_parameter(int argc, char *argv[])
{
        int i;
        int dat;

        if(1 == argc)
        {
                /* no parameter */
                fprintf(stderr, "No binary file to process...\n\n");
                show_help();
                return -1;
        }

        for(i = 1; i < argc; i++)
        {
                if('-' == argv[i][0])
                {
                        if(     0 == strcmp(argv[i], "-w") ||
                                0 == strcmp(argv[i], "--width")
                        )
                        {
                                sscanf(argv[++i], "%i" , &dat);
                                if(0 < dat && dat <= (LINE_LENGTH_MAX / 3))
                                {
                                        npline = dat;
                                }
                                else
                                {
                                        fprintf(stderr,
                                                "bad variable for '-n': %d, use %d instead!\n",
                                                dat, npline);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-a") ||
                                0 == strcmp(argv[i], "--addr")
                        )
                        {
                                show_address = 1;
                        }
                        else if(0 == strcmp(argv[i], "-d") ||
                                0 == strcmp(argv[i], "--decaddr")
                        )
                        {
                                dec_address = 1;
                        }
                        else if(0 == strcmp(argv[i], "-start"))
                        {
                                int start;

                                i++;
                                if(i >= argc)
                                {
                                        fprintf(stderr, "no parameter for '-start'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &start);
                                aim_start = start;
                        }
                        else if(0 == strcmp(argv[i], "-stop"))
                        {
                                int stop;

                                i++;
                                if(i >= argc)
                                {
                                        fprintf(stderr, "no parameter for '-stop'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &stop);
                                aim_stop = stop;
                        }
                        else if(0 == strcmp(argv[i], "-s") ||
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

static int show_help()
{
        puts("'catbin' read binary file, translate 0xXX to 'XY ' format, then send to stdout.");
        puts("");
        puts("Usage: catbin [OPTION] file [OPTION]");
        puts("");
        puts("Options:");
        puts("");
        puts(" -a, --addr               show data address at line head, default: do NOT show it");
        puts(" -d, --decaddr            dec address format, default: hex");
#if 0
        puts(" -start <a>               cat from a%(file length), default: 0, first byte");
        puts(" -stop <b>                cat to b%(file length), default: 0, last byte");
#endif
        puts(" -s, --seperate <,>       white space, any char except [0-9A-Fa-f], default: ' '");
        puts(" -w, --width <w>          w-byte per line, [1,10922], default: 188");
        puts(" -h, --help               display this information");
        puts(" -v, --version            display my version");
        puts("");
        puts("Examples:");
        puts("  catbin xxx.ts");
        puts("");
        puts("Report bugs to <zhoucheng@tsinghua.org.cn>.");
        return 0;
}

static int show_version()
{
        puts("catbin 1.0.0");
        puts("");
        puts("Copyright (C) 2009,2010,2011 ZHOU Cheng.");
        puts("This is free software; contact author for additional information.");
        puts("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR");
        puts("A PARTICULAR PURPOSE.");
        puts("");
        puts("Written by ZHOU Cheng.");
        return 0;
}

#define SYNC_TIME       3 /* SYNC_TIME syncs means TS sync */
#define ASYNC_BYTE      4096 /* head ASYNC_BYTE bytes async means BIN file */
/* for TS data: use "state machine" to determine sync position and packet size */
static int judge_type()
{
        uint8_t dat;
        int sync_cnt;
        int state = FILE_UNKNOWN;

        pkt->ADDR = 0;
        type = FILE_UNKNOWN;
        while(FILE_UNKNOWN == type)
        {
                switch(state)
                {
                        case FILE_UNKNOWN:
                                fseek(fd_i, pkt->ADDR, SEEK_SET);
                                if(1 != fread(&dat, 1, 1, fd_i))
                                {
                                        return -1;
                                }
                                if(0x47 == dat)
                                {
                                        sync_cnt = 1;
                                        state = FILE_TS;
                                }
                                else
                                {
                                        pkt->ADDR++;
                                        if(pkt->ADDR > ASYNC_BYTE)
                                        {
                                                pkt->ADDR = 0;
                                                type = FILE_BIN;
                                        }
                                }
                                break;
                        case FILE_TS:
                                fseek(fd_i, +187, SEEK_CUR);
                                if(1 != fread(&dat, 1, 1, fd_i))
                                {
                                        return -1;
                                }
                                if(0x47 == dat)
                                {
                                        sync_cnt++;
                                        if(sync_cnt >= SYNC_TIME)
                                        {
                                                npline = 188;
                                                type = FILE_TS;
                                        }
                                }
                                else
                                {
                                        fseek(fd_i, pkt->ADDR + 1, SEEK_SET);
                                        sync_cnt = 1;
                                        state = FILE_MTS;
                                }
                                break;
                        case FILE_MTS:
                                fseek(fd_i, +191, SEEK_CUR);
                                if(1 != fread(&dat, 1, 1, fd_i))
                                {
                                        return -1;
                                }
                                if(0x47 == dat)
                                {
                                        sync_cnt++;
                                        if(sync_cnt >= SYNC_TIME)
                                        {
                                                if(pkt->ADDR < 4)
                                                {
                                                        fseek(fd_i, pkt->ADDR + 1, SEEK_SET);
                                                        sync_cnt = 1;
                                                        state = FILE_TSRS;
                                                }
                                                else
                                                {
                                                        pkt->ADDR -= 4;
                                                        npline = 192;
                                                        type = FILE_MTS;
                                                }
                                        }
                                }
                                else
                                {
                                        fseek(fd_i, pkt->ADDR + 1, SEEK_SET);
                                        sync_cnt = 1;
                                        state = FILE_TSRS;
                                }
                                break;
                        case FILE_TSRS:
                                fseek(fd_i, +203, SEEK_CUR);
                                if(1 != fread(&dat, 1, 1, fd_i))
                                {
                                        return -1;
                                }
                                if(0x47 == dat)
                                {
                                        sync_cnt++;
                                        if(sync_cnt >= SYNC_TIME)
                                        {
                                                npline = 204;
                                                type = FILE_TSRS;
                                        }
                                }
                                else
                                {
                                        pkt->ADDR++;
                                        state = FILE_UNKNOWN;
                                }
                                break;
                        default:
                                DBG(ERR_BAD_CASE, "%d\n", state);
                                return -1;
                }
        }
        fseek(fd_i, pkt->ADDR, SEEK_SET);
        if(pkt->ADDR != 0)
        {
                fprintf(stderr, "%lld-byte passed\n", pkt->ADDR);
        }
        return 0;
}

static int mts_time(uint64_t *mts, uint8_t *bin)
{
        int i;

        for(*mts = 0, i = 0; i < 4; i++)
        {
                *mts <<= 8;
                *mts |= *bin++;
        }

        return 0;
}
