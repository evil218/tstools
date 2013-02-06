/* vim: set tabstop=8 shiftwidth=8:
 * name: dtsdi.c
 * funx: analyse ".dtsdi" file(header frame frame ...)
 * 2012-09-15, ZHOU Cheng, init
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> /* for uint?_t, etc */

#include "version.h"
#include "common.h"
#include "if.h"

/* Magic Code: Identifies file as ".dtsdi file" */
#define DTSDI_MAGIC_CODE1       (0x546B6544) /* Magic code part 1 */
#define DTSDI_MAGIC_CODE2       (0x642E6365) /* Magic code part 2 */ 
#define DTSDI_MAGIC_CODE3       (0x69647374) /* Magic code part 3 */

/* (Current) Format Version */
#define DTSDI_FMT_VERSION       (0) 

/* Data Type */
#define DTSDI_TYPE_SDI_625      (0x01) /* 625 lines per SDI frame */
#define DTSDI_TYPE_SDI_525      (0x02) /* 525 lines per SDI frame */

/* Data-Format Flag */
#define DTSDI_SDI_FULL          (0x0001) /* Full SDI frame */
#define DTSDI_SDI_ACTVID        (0x0002) /* Active video only */
#define DTSDI_SDI_10B           (0x0080) /* Set if the file contains 10-bit samples */
#define DTSDI_SDI_HUFFMAN       (0x0200) /* Set if the file contains compressed SDI samples */

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

/* header */
struct dtsdi_header {
    uint32_t magic_code1; /* Magic code 1, identifies file type */
    uint32_t magic_code2; /* Magic code 2, identifies file type */
    uint32_t magic_code3; /* Magic code 3, identifies file type */
    uint8_t  version;     /* Version number of .dtsdi file format */
    uint8_t  type;        /* Type of data */
    uint16_t flag;        /* Additional data-format flags */
};

static FILE *fd_i = NULL;
static char file_i[FILENAME_MAX] = "";
static struct dtsdi_header hdr;
static int output_mode = 8; /* 8(8-bit), 10(10-bit) */
static int show_info = 0;
static int show_vbi = 1;
static int show_active = 0;
static int total_line_cnt; /* depend on 525 or 625 */
static int hbi_cnt_per_line; /* depend on 525 or 625 */
static int act_cnt_per_line = 1440;

static int deal_with_parameter(int argc, char *argv[]);
static int show_help();
static int show_version();

static int parse_header();
static int parse_frame();
static int get_four(uint16_t *sdi);
static int output(uint16_t *dat, int cnt);

int main(int argc, char *argv[])
{
        if(0 != deal_with_parameter(argc, argv)) {
                return -1;
        }

        fd_i = fopen(file_i, "rb");
        if(NULL == fd_i) {
                RPT(RPT_ERR, "open \"%s\" failed", file_i);
                return -1;
        }

        if(0 != parse_header()) {
                return -1;
        }

        parse_frame();

        fclose(fd_i);
        return 0;
}

static int deal_with_parameter(int argc, char *argv[])
{
        int i;
        int dat;

        if(1 == argc) {
                /* no parameter */
                fprintf(stderr, "No binary file to process...\n\n");
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
                        else if(0 == strcmp(argv[i], "-i") ||
                                0 == strcmp(argv[i], "--info")) {
                                show_info = 1;
                        }
                        else if(0 == strcmp(argv[i], "--novbi")) {
                                show_vbi = 0;
                        }
                        else if(0 == strcmp(argv[i], "--active")) {
                                show_active = 1;
                        }
                        else if(0 == strcmp(argv[i], "-m") ||
                                0 == strcmp(argv[i], "--mode")) {
                                i++;
                                if(i >= argc) {
                                        fprintf(stderr, "no parameter for 'mode'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &dat);
                                if(8 == dat || 10 == dat) {
                                        output_mode = dat;
                                }
                                else {
                                        fprintf(stderr,
                                                "bad variable for 'mode': %u(8 or 10), use %u instead!\n",
                                                dat, output_mode);
                                }
                        }
                        else {
                                RPT(RPT_ERR, "Wrong parameter: %s", argv[i]);
                                return -1;
                        }
                }
                else {
                        strcpy(file_i, argv[i]);
                }
        }

        return 0;
}

static int show_help()
{
        puts("'dtsdi' parse .dtsdi file, output to stdout.");
        puts("");
        puts("Usage: dtsdi [OPTION] file [OPTION]");
        puts("");
        puts("Options:");
        puts("");
        puts(" -i, --info               show dtsdi file information, default: do NOT show information");
        puts("     --novbi              do NOT show vbi data, default: show vbi data");
        puts("     --active             show active data, default: do NOT show active data");
        puts(" -m, --mode <m>           output bit mode, 8 or 10, default: 8(-bit)");
        puts("");
        puts(" -h, --help               display this information");
        puts(" -v, --version            display my version");
        puts("");
        puts("Examples:");
        puts("  dtsdi xxx.dtsdi -i");
        puts("  dtsdi xxx.dtsdi -m 10");
        puts("");
        puts("Report bugs to <zhoucheng@tsinghua.org.cn>.");
        return 0;
}

static int show_version()
{
        char str[100];

        sprintf(str, "dtsdi of tools v%s.%s.%s", VER_MAJOR, VER_MINOR, VER_RELEA);
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
        return 0;
}

static int parse_header()
{
        int cnt;

        cnt = fread(&hdr, 1, 16, fd_i); /* file header has 16-byte */
        if(cnt != 16) {
                RPT(RPT_ERR, "read header of \"%s\" failed, cnt == %d != 16", file_i, cnt);
                return -1;
        }
        if(DTSDI_MAGIC_CODE1 != hdr.magic_code1 ||
           DTSDI_MAGIC_CODE2 != hdr.magic_code2 ||
           DTSDI_MAGIC_CODE3 != hdr.magic_code3) {
                RPT(RPT_ERR, "magic_code1(0x%08X) : 0x%08X", DTSDI_MAGIC_CODE1, hdr.magic_code1);
                RPT(RPT_ERR, "magic_code2(0x%08X) : 0x%08X", DTSDI_MAGIC_CODE2, hdr.magic_code2);
                RPT(RPT_ERR, "magic_code3(0x%08X) : 0x%08X", DTSDI_MAGIC_CODE3, hdr.magic_code3);
                return -1;
        }

        if(DTSDI_TYPE_SDI_625 == hdr.type) {
                hbi_cnt_per_line = 280;
                total_line_cnt = 625;
        }
        else {
                hbi_cnt_per_line = 268;
                total_line_cnt = 525;
        }

        if(show_info) {
                fprintf(stdout, "File(dtsdi) version: %d\r\n", hdr.version);
                fprintf(stdout, "Frame: %u", total_line_cnt);
                if(DTSDI_SDI_FULL & hdr.flag) {
                        fprintf(stdout, ", full SDI data");
                }
                else {
                        fprintf(stdout, ", active video only");
                }
                if(0 != (DTSDI_SDI_10B & hdr.flag)) {
                        fprintf(stdout, ", 10-bit style");
                }
                else {
                        fprintf(stdout, ", 8-bit style");
                }
                if(0 != (DTSDI_SDI_HUFFMAN & hdr.flag)) {
                        fprintf(stdout, ", huffman compressed");
                }
                else {
                        fprintf(stdout, ", uncompressed");
                }
                fprintf(stdout, "\r\n");
        }
        return 0;
}

static int parse_frame()
{
        int i;
        uint16_t sdi[4]; /* use low 10-bit only */
        uint16_t dat[1440];
        int line_num;
        int frame_num;

        frame_num = 1;
        line_num = 1;
        while(1) {
                int line;

                /* line head */
                line = line_num;
                if(525 == total_line_cnt) {
                        line += 3;
                        line -= ((line > 525) ? 525 : 0);
                }
                fprintf(stdout, "%4u, %4u, ", frame_num, line);

                /* EAV */
                if(0 != get_four(sdi)) {
                        return -1;
                }
                if(!(0x3FF == sdi[0] &&
                     0x000 == sdi[1] &&
                     0x000 == sdi[2] &&
                     (0x040 & sdi[3]))) { /* H(bit6) == 1 */
                        RPT(RPT_ERR, "bad EAV, exit.");
                        return -1;
                }
                output(sdi, 4);

                /* h-blank data */
                for(i = 0; i < hbi_cnt_per_line; i += 4) {
                        if(0 != get_four(sdi)) {
                                return -1;
                        }
                        dat[i + 0] = sdi[0];
                        dat[i + 1] = sdi[1];
                        dat[i + 2] = sdi[2];
                        dat[i + 3] = sdi[3];
                }
                if(show_vbi) {
                        output(dat, hbi_cnt_per_line);
                }

                /* SAV */
                if(0 != get_four(sdi)) {
                        return -1;
                }
                if(!(0x3FF == sdi[0] &&
                     0x000 == sdi[1] &&
                     0x000 == sdi[2] &&
                     !(0x040 & sdi[3]))) { /* H(bit6) == 0 */
                        RPT(RPT_ERR, "bad SAV, exit.");
                        return -1;
                }
                output(sdi, 4);

                /* active data */
                for(i = 0; i < act_cnt_per_line; i += 4) {
                        if(0 != get_four(sdi)) {
                                return -1;
                        }
                        dat[i + 0] = sdi[0];
                        dat[i + 1] = sdi[1];
                        dat[i + 2] = sdi[2];
                        dat[i + 3] = sdi[3];
                }
                if(show_active) {
                        output(dat, act_cnt_per_line);
                }

                /* line tail */
                fprintf(stdout, "\r\n");
                line_num++;
                if(line_num > total_line_cnt) {
                        line_num = 1;
                        frame_num++;

                        /* 32-bit align in dtsdi */
                        if(525 == total_line_cnt) {
                                fread(dat, 1, 3, fd_i);
                        }
                }
        }
        return 0;
}

static int get_four(uint16_t *sdi)
{
        int cnt;
        uint8_t packed[5];

        cnt = fread(packed, 1, 5, fd_i);
        if(5 != cnt) {
                RPT(RPT_ERR, "data in \"%s\" not enough, got %d only", file_i, cnt);
                return -1;
        }

        /* each 4 x 10-bit be packed into 5 x 8-bit */
        /* 5 x  8-bit: 01234567 01234567 01234567 01234567 01234567 */
        /* 4 x 10-bit: 01234567 89012345 67890123 45678901 23456789 */
        *(sdi + 0) = ((packed[0] & 0xFF) >> 0) | ((packed[1] & 0x03) << 8);
        *(sdi + 1) = ((packed[1] & 0xFC) >> 2) | ((packed[2] & 0x0F) << 6);
        *(sdi + 2) = ((packed[2] & 0xF0) >> 4) | ((packed[3] & 0x3F) << 4);
        *(sdi + 3) = ((packed[3] & 0xC0) >> 6) | ((packed[4] & 0xFF) << 2);
        return 0;
}

static int output(uint16_t *dat, int cnt)
{
        int i;

        if(10 == output_mode) {
                /* 10-bit output mode */
                for(i = 0; i < cnt - 1; i++) {
                        fprintf(stdout, "%03X ", *(dat + i));
                }
                fprintf(stdout, "%03X, ", *(dat + i));
        }
        else {
                /* 8-bit output mode */
                for(i = 0; i < cnt - 1; i++) {
                        fprintf(stdout, "%02X ", *(dat + i) >> 2);
                }
                fprintf(stdout, "%02X, ", *(dat + i) >> 2);
        }
        return 0;
}
