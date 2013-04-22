/* vim: set tabstop=8 shiftwidth=8:
 * name: toip.c
 * funx: send UDP packet with text data in stdin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for strcmp, etc */
#include <inttypes.h> /* for PRId64, etc */
#include <sys/time.h> /* for gettimeofday(), etc */

#ifdef PLATFORM_mingw
#       include <winsock.h>
#       define timeradd(a, b, result) \
                do { \
                        (result)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
                        (result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
                        if ((result)->tv_usec >= 1000000) \
                        { \
	                        ++(result)->tv_sec; \
	                        (result)->tv_usec -= 1000000; \
                        } \
                } while (0)
#else /* unix-like PLATFORM */
#       include <sys/select.h> /* for select(), etc */
#endif

#include "version.h"
#include "common.h"
#include "if.h"
#include "url.h"
#include "ts.h"

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

static struct url *fd_o = NULL;
static char file_o[FILENAME_MAX] = "";

static int deal_with_parameter(int argc, char *argv[]);
static void show_help();
static void show_version();

int main(int argc, char *argv[])
{
        int cnt;
        char tbuf[LINE_LENGTH_MAX + 10]; /* txt data buffer */
        uint8_t bbuf[188 * 7 + 10]; /* bin data buffer */
        char *tag;
        char *pt;
        uint8_t *pb = bbuf;

        struct timeval tv_pkt; /* packet time */
        struct timeval tv_cur; /* current time */

        long long int data;
        int64_t lMTS = 0LL; /* last MTS */
        int64_t MTS = 0LL; /* current MTS */
        int64_t dMTS = 0LL; /* delta MTS */
        int64_t *mts = NULL; /* NULL means without MTS data */

        if(0 != deal_with_parameter(argc, argv)) {
                return -1;
        }

        fd_o = url_open(file_o, "wb");
        if(NULL == fd_o) {
                RPT(RPT_ERR, "open \"%s\" failed", file_o);
                return -1;
        }

        /* init time, then wait until delta MTS OK */
        gettimeofday(&tv_pkt, NULL);
        gettimeofday(&tv_cur, NULL);
        while(NULL != fgets(tbuf, LINE_LENGTH_MAX, stdin)) {
                pt = tbuf;
                mts = NULL;
                while(0 == next_tag(&tag, &pt)) {
                        if(0 == strcmp(tag, "*mts")) {
                                next_nuint_hex(&data, &pt, 1);
                                MTS = (int64_t)data;
                                mts = &MTS;
                                dMTS = ts_timestamp_diff(MTS, lMTS, MTS_OVF);
                                lMTS = MTS;
                        }
                }
                if(!mts) {
                        RPT(RPT_ERR, "TS packet without MTS");
                        url_close(fd_o);
                        return -1;
                }
                if(0 < dMTS && dMTS < 100 * MTS_MS) {
                        /* delta MTS is OK now */
                        break;
                }
        }

        /* run */
        while(NULL != fgets(tbuf, LINE_LENGTH_MAX, stdin)) {
                pt = tbuf;
                mts = NULL;
                while(0 == next_tag(&tag, &pt)) {
                        if(0 == strcmp(tag, "*ts")) {
                                cnt = next_nbyte_hex(pb, &pt, LINE_LENGTH_MAX / 3);
                                pb += cnt;
                        }
                        if(0 == strcmp(tag, "*mts")) {
                                struct timeval dtv;
                                struct timeval tv_new;

                                next_nuint_hex(&data, &pt, 1);
                                MTS = (int64_t)data;
                                mts = &MTS;
                                dMTS = ts_timestamp_diff(MTS, lMTS, MTS_OVF);
                                if(0 < dMTS && dMTS < 100 * MTS_MS) {
                                        dtv.tv_sec = dMTS / MTS_1S;
                                        dMTS %= MTS_1S;
                                        dtv.tv_usec = dMTS / MTS_US;
                                        dMTS %= MTS_US;
                                        timeradd(&tv_pkt, &dtv, &tv_new);
                                        tv_pkt = tv_new;
                                        lMTS = ts_timestamp_add(MTS, -dMTS, MTS_OVF);
                                }
                                else {
                                        RPT(RPT_WRN, "!(0 < dMTS < 100ms): %" PRId64, dMTS);
                                        gettimeofday(&tv_pkt, NULL);
                                        lMTS = MTS;
                                }
                        }
                }
                if(!mts) {
                        RPT(RPT_ERR, "TS packet without MTS");
                        url_close(fd_o);
                        return -1;
                }
                if((pb - bbuf) >= (188 * 7)) {
                        url_write(bbuf, pb - bbuf, 1, fd_o);
                        pb = bbuf;
                        if(timercmp(&tv_pkt, &tv_cur, >)) {
                                struct timeval delay; /* send interval */

                                delay.tv_sec = 0;
                                delay.tv_usec = 5000; /* us */
                                select(0, NULL, NULL, NULL, &delay); /* sleep */
                                gettimeofday(&tv_cur, NULL);
                        }
                }
        }

        url_close(fd_o);
        return 0;
}

static int deal_with_parameter(int argc, char *argv[])
{
        int i;

        if(1 == argc) {
                /* no parameter */
                fprintf(stderr, "No URL to process...\n\n");
                show_help();
                return -1;
        }

        for(i = 1; i < argc; i++) {
                if('-' == argv[i][0]) {
                        if(0 == strcmp(argv[i], "-h") ||
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
                                RPT(RPT_ERR, "wrong parameter: %s", argv[i]);
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
        puts("'toip' read from stdin, convert to UDP, send to IP according to MTS.");
        puts("");
        puts("Usage: toip [OPTION] udp://@xxx.xxx.xxx.xxx:xxxx [OPTION]");
        puts("");
        puts("Options:");
        puts("");
        puts(" -h, --help       print this information only");
        puts(" -v, --version    print my version only");
        puts("");
        puts("Examples:");
        puts("  catts *.mts | toip udp://@:1234");
        puts("  catts *.mts | toip udp://@224.165.54.210:1234");
        puts("  catts *.ts | tsana -ts -mts | toip udp://@:1234");
        puts("");
        puts("Report bugs to <zhoucheng@tsinghua.org.cn>.");
        return;
}

static void show_version()
{
        char str[100];

        sprintf(str, "toip of tstools v%s.%s.%s", VER_MAJOR, VER_MINOR, VER_RELEA);
        puts(str);
        sprintf(str, "Build time: %s %s", __DATE__, __TIME__);
        puts(str);
        puts("");
        puts("Copyright (C) 2009,2010,2011,2012 ZHOU Cheng.");
        puts("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>");
        puts("This is free software; contact author for additional information.");
        puts("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR");
        puts("A PARTICULAR PURPOSE.");
        puts("");
        puts("Written by ZHOU Cheng.");
        return;
}
