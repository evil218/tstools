/* vim: set tabstop=8 shiftwidth=8:
 * name: tobin.c
 * funx: generate bin ts file with text data in stdin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for strcmp, etc */

#include "tstool_config.h"
#include "common.h"
#include "if.h"

static int rpt_lvl = WRN_LVL; /* report level: ERR, WRN, INF, DBG */

static FILE *fd_o = NULL;
static char file_o[FILENAME_MAX] = "";

static int deal_with_parameter(int argc, char *argv[]);
static void show_help();
static void show_version();

int main(int argc, char *argv[])
{
        int cnt;
        char tbuf[LINE_LENGTH_MAX + 10]; /* txt data buffer */
        uint8_t bbuf[LINE_LENGTH_MAX / 3 + 10]; /* bin data buffer */
        char *tag;
        char *pt;

        if(0 != deal_with_parameter(argc, argv)) {
                return -1;
        }

        fd_o = fopen(file_o, "wb");
        if(NULL == fd_o) {
                RPTERR("open \"%s\" failed", file_o);
                return -1;
        }

        while(NULL != fgets(tbuf, LINE_LENGTH_MAX, stdin)) {
                pt = tbuf;
                while(0 == next_tag(&tag, &pt)) {
                        if(0 == strcmp(tag, "*ts") ||
                           0 == strcmp(tag, "*rs") ||
                           0 == strcmp(tag, "*data") ||
                           0 == strcmp(tag, "*pes") ||
                           0 == strcmp(tag, "*es")) {
                                cnt = next_nbyte_hex(bbuf, &pt, LINE_LENGTH_MAX / 3);
                                (void)fwrite(bbuf, (size_t)cnt, 1, fd_o);
                        }
                }
        }

        fclose(fd_o);

        return 0;
}

static int deal_with_parameter(int argc, char *argv[])
{
        int i;

        if(1 == argc) {
                /* no parameter */
                fprintf(stderr, "No binary file to write...\n\n");
                show_help();
                return -1;
        }

        for(i = 1; i < argc; i++) {
                if('-' == argv[i][0]) {
                        if(     0 == strcmp(argv[i], "-h") ||
                                0 == strcmp(argv[i], "--help")) {
                                show_help();
                                return -1;
                        }
                        else if(0 == strcmp(argv[i], "-v") ||
                                0 == strcmp(argv[i], "--version")) {
                                show_version();
                                return -1;
                        }
                        else {
                                RPTERR("wrong parameter: %s", argv[i]);
                                return -1;
                        }
                }
                else {
                        strcpy(file_o, argv[i]);
                }
        }

        return 0;
}

static void show_help()
{
        fprintf(stdout,
                "'tobin' read from stdin, translate 'XY ' to 0xXY, send to file.\n"
                "\n"
                "Usage: tobin [OPTION] file [OPTION]\n"
                "\n"
                "Options:\n"
                "\n"
                " -h, --help       print this information only\n"
                " -v, --version    print my version only\n"
                "\n"
                "Examples:\n"
                "  tobin xxx.ts\n"
                "\n"
                "Report bugs to <zhoucheng@tsinghua.org.cn>.\n");
        return;
}

static void show_version()
{
        fprintf(stdout,
                "tobin of tstools v%s (%s)\n"
                "Build time: %s %s\n"
                "\n"
                "Copyright (C) 2009,2010,2011,2012 ZHOU Cheng.\n"
                "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
                "This is free software; contact author for additional information.\n"
                "There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR\n"
                "A PARTICULAR PURPOSE.\n"
                "\n"
                "Written by ZHOU Cheng.\n",
                VERSION_STR, REVISION, __DATE__, __TIME__);
        return;
}
