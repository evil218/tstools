/* vim: set tabstop=8 shiftwidth=8:
 * name: dtsdi.c
 * funx: analyse ".dtsdi" file(header frame frame ...)
 * 2012-09-15, ZHOU Cheng, init
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> /* for uint?_t, etc */

#include "tstool_config.h" /* for VERSION_STR, REVISION */
#include "common.h" /* for RPTERR(), RPTWRN(), RPTINF(), RPTDBG() */

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

static int rpt_lvl = WRN_LVL; /* report level: ERR, WRN, INF, DBG */

/* header */
struct dtsdi_header {
        uint32_t magic_code1; /* Magic code 1, identifies file type */
        uint32_t magic_code2; /* Magic code 2, identifies file type */
        uint32_t magic_code3; /* Magic code 3, identifies file type */
        uint8_t  version;     /* Version number of .dtsdi file format */
        uint8_t  type;        /* Type of data */
        uint16_t flag;        /* Additional data-format flags */
};

struct dtsdi_header_ext {
        uint8_t ext[8]; /* I do not known the meaning of these 8-byte */
};

#define DT_PASS (0) /* do not show these data */
#define DT_DATA (1) /* show these data directly */
#define DT_INFO (2) /* parse and show the info of these data */

static FILE *fd_i = NULL;
static char file_i[FILENAME_MAX] = "";
static struct dtsdi_header hdr;
static struct dtsdi_header_ext hdr_ext;
static uint8_t packed[1440 / 4 * 5]; /* support SD only */
static uint16_t sdi[1440]; /* use low 10-bit only */
static int output_mode = 10; /* 8-bit(8)!10-bit(10) */
static int show_inf = DT_PASS; /* pass(0)!info(2) */
static int show_vbi = DT_PASS; /* pass(0)!data(1)!info(2) */
static int show_act = DT_PASS; /* pass(0)!data(1) */
static int total_line_cnt; /* depend on 525 or 625 */
static int hbi_cnt_per_line; /* depend on 525 or 625 */
static int act_cnt_per_line = 1440;

static int deal_with_parameter(int argc, char *argv[]);
static int show_help();
static int show_version();

static int parse_header();
static int parse_frame();
static int get_data(uint16_t *sdi, int CNT);
static int show_msb(uint16_t *dat, int cnt);
static int show_hbi(uint16_t *dat, int cnt);
static int show_anc(uint16_t *dat, int cnt);

int main(int argc, char *argv[])
{
        if(0 != deal_with_parameter(argc, argv)) {
                return -1;
        }

        fd_i = fopen(file_i, "rb");
        if(NULL == fd_i) {
                RPTERR("open \"%s\" failed", file_i);
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
                                0 == strcmp(argv[i], "--inf")) {
                                show_inf = DT_INFO;
                        }
                        else if(0 == strcmp(argv[i], "--hbd")) {
                                show_vbi = DT_DATA;
                        }
                        else if(0 == strcmp(argv[i], "--hbi")) {
                                show_vbi = DT_INFO;
                        }
                        else if(0 == strcmp(argv[i], "-a") ||
                                0 == strcmp(argv[i], "--act")) {
                                show_act = DT_DATA;
                        }
                        else if(0 == strcmp(argv[i], "-m") ||
                                0 == strcmp(argv[i], "--mod")) {
                                i++;
                                if(i >= argc) {
                                        fprintf(stderr, "no parameter for 'mod'!\n");
                                        return -1;
                                }
                                sscanf(argv[i], "%i" , &dat);
                                if(8 == dat || 10 == dat) {
                                        output_mode = dat;
                                }
                                else {
                                        fprintf(stderr,
                                                "bad variable for 'mod': %u(8 or 10), use %u instead!\n",
                                                dat, output_mode);
                                }
                        }
                        else {
                                RPTERR("Wrong parameter: %s", argv[i]);
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
        fprintf(stdout,
                "'dtsdi' parse .dtsdi file, output to stdout.\n"
                "\n"
                "Usage: dtsdi [OPTION] file [OPTION]\n"
                "\n"
                "Options:\n"
                "\n"
                "  -i, --inf                show dtsdi file information, default: do NOT show information\n"
                "      --hbd                show hbi data, default: do NOT show hbi data\n"
                "      --hbi                parse and show the info of  hbi data, default: do NOT show hbi data\n"
                "  -a, --act                show active data, default: do NOT show active data\n"
                "  -m, --mod <m>           output bit mode, 8 or 10, default: 10(-bit)\n"
                "\n"
                "  -h, --help               display this information\n"
                "  -v, --version            display my version\n"
                "\n"
                "Examples:\n"
                "  dtsdi xxx.dtsdi -i\n"
                "  dtsdi xxx.dtsdi -m 8\n"
                "\n"
                "Report bugs to <zhoucheng@tsinghua.org.cn>.\n");
        return 0;
}

static int show_version()
{
        fprintf(stdout,
                "dtsdi of tstools v%s (%s)\n"
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
        return 0;
}

static int parse_header()
{
        int cnt;

        cnt = fread(&hdr, 1, 16, fd_i); /* file header has 16-byte */
        if(cnt != 16) {
                RPTERR("read header of \"%s\" failed, cnt == %d != 16", file_i, cnt);
                return -1;
        }
        if(DTSDI_MAGIC_CODE1 != hdr.magic_code1 ||
           DTSDI_MAGIC_CODE2 != hdr.magic_code2 ||
           DTSDI_MAGIC_CODE3 != hdr.magic_code3) {
                RPTERR("magic_code1(0x%08X) : 0x%08X", DTSDI_MAGIC_CODE1, hdr.magic_code1);
                RPTERR("magic_code2(0x%08X) : 0x%08X", DTSDI_MAGIC_CODE2, hdr.magic_code2);
                RPTERR("magic_code3(0x%08X) : 0x%08X", DTSDI_MAGIC_CODE3, hdr.magic_code3);
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

        /* header_ext of version 1 */
        if(DTSDI_FMT_VERSION != hdr.version) {
                cnt = fread(&hdr_ext, 1, 8, fd_i); /* header of version 1 has additional 8-byte */
                if(cnt != 8) {
                        RPTERR("read header_ext of \"%s\" failed, cnt == %d != 8", file_i, cnt);
                        return -1;
                }
        }

        if(DT_INFO == show_inf) {
                fprintf(stdout, "DTSDI: version %d", hdr.version);
                fprintf(stdout, ", frame: %u", total_line_cnt);
                fprintf(stdout, ", %s", (DTSDI_SDI_FULL & hdr.flag) ? "full SDI data" : "active video only");
                fprintf(stdout, ", %s", (DTSDI_SDI_10B & hdr.flag) ? "10-bit style" : "8-bit style");
                fprintf(stdout, ", %s", (DTSDI_SDI_HUFFMAN & hdr.flag) ? "huffman compressed" : "uncompressed");

                if(DTSDI_FMT_VERSION != hdr.version) {
                        int i;

                        fprintf(stdout, ", header_ext:");
                        for(i = 0; i < sizeof(hdr_ext); i++) {
                                fprintf(stdout, " %02X", hdr_ext.ext[i]);
                        }
                }
                fprintf(stdout, "\r\n");
        }
        return 0;
}

static int parse_frame()
{
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
                if(0 != get_data(sdi, 4)) {
                        return -1;
                }
                if(!((0x3FF == sdi[0]) &&
                     (0x000 == sdi[1]) &&
                     (0x000 == sdi[2]) &&
                     (0x040 & sdi[3]))) { /* H(bit6) == 1 */
                        RPTERR("bad EAV");
                        return -1;
                }
                show_msb(sdi, 4);

                /* h-blank data */
                if(0 != get_data(sdi, hbi_cnt_per_line)) {
                        return -1;
                }
                if(DT_DATA == show_vbi) {
                        show_hbi(sdi, hbi_cnt_per_line);
                }
                if(DT_INFO == show_vbi) {
                        show_anc(sdi, hbi_cnt_per_line);
                }

                /* SAV */
                if(0 != get_data(sdi, 4)) {
                        return -1;
                }
                if(!((0x3FF == sdi[0]) &&
                     (0x000 == sdi[1]) &&
                     (0x000 == sdi[2]) &&
                     !(0x040 & sdi[3]))) { /* H(bit6) == 0 */
                        RPTERR("bad SAV");
                        return -1;
                }
                show_msb(sdi, 4);

                /* active data */
                if(0 != get_data(sdi, act_cnt_per_line)) {
                        return -1;
                }
                if(DT_DATA == show_act) {
                        show_msb(sdi, act_cnt_per_line);
                }

                /* line tail */
                fprintf(stdout, "\r\n");
                line_num++;
                if(line_num > total_line_cnt) {
                        line_num = 1;
                        frame_num++;

                        /* each frame is 32-bit align in dtsdi */
                        if(525 == total_line_cnt) {
                                uint8_t dat[3];

                                fread(dat, 1, 3, fd_i);
                        }
                }
        }
        return 0;
}

static int get_data(uint16_t *sdi, int CNT)
{
        int i;
        int cnt;
        uint8_t *pack;

        if(0 != (0x03 & CNT) || CNT > 1440) {
                RPTERR("not 32-bit align or bigger than 1440");
                return -1;
        }
        cnt = CNT + (CNT >> 2); /* "5 / 4" */

        i = fread(packed, 1, cnt, fd_i);
        if(i != cnt) {
                RPTERR("data in \"%s\" not enough, got %d only", file_i, i);
                return -1;
        }

        pack = packed;
        for(i = 0; i < CNT; i += 4) {
                /* each 4 x 10-bit be packed into 5 x 8-bit */
                /* 5 x  8-bit: 01234567 01234567 01234567 01234567 01234567 */
                /* 4 x 10-bit: 01234567 89012345 67890123 45678901 23456789 */
                *sdi++ = ((*(pack + 0) & 0x00FF) >> 0) | ((*(pack + 1) & 0x0003) << 8);
                *sdi++ = ((*(pack + 1) & 0x00FC) >> 2) | ((*(pack + 2) & 0x000F) << 6);
                *sdi++ = ((*(pack + 2) & 0x00F0) >> 4) | ((*(pack + 3) & 0x003F) << 4);
                *sdi++ = ((*(pack + 3) & 0x00C0) >> 6) | ((*(pack + 4) & 0x00FF) << 2);
                pack += 5;
        }
        return 0;
}

static int show_msb(uint16_t *dat, int cnt)
{
        int i;

        if(10 == output_mode) {
                /* 10-bit output mode */
                for(i = 0; i < cnt - 1; i++) {
                        fprintf(stdout, "%03X ", *dat++);
                }
                fprintf(stdout, "%03X, ", *dat++);
        }
        else {
                /* 8-bit output mode */
                for(i = 0; i < cnt - 1; i++) {
                        fprintf(stdout, "%02X ", *dat++ >> 2);
                }
                fprintf(stdout, "%02X, ", *dat++ >> 2);
        }
        return 0;
}

static int show_hbi(uint16_t *dat, int cnt)
{
        int i;

        if(10 == output_mode) {
                /* 10-bit output mode */
                for(i = 0; i < cnt - 1; i++) {
                        fprintf(stdout, "%03X ", *dat++);
                }
                fprintf(stdout, "%03X, ", *dat++);
        }
        else {
                /* 8-bit output mode */
                for(i = 0; i < cnt - 1; i++) {
                        fprintf(stdout, "%02X ", *dat++ && 0xFF);
                }
                fprintf(stdout, "%02X, ", *dat++ && 0xFF);
        }
        return 0;
}

static int show_anc(uint16_t *dat, int cnt)
{
        uint16_t *end = dat + cnt;
        uint16_t DID;
        uint16_t DC;
        uint16_t CS;

        while(dat < end) {
                if(0x000 == *(dat + 0) &&
                   0x3FF == *(dat + 1) &&
                   0x3FF == *(dat + 2)) {
                        dat += 3;
                        DID = *dat++;
                        fprintf(stdout, "DID %03X, ", DID);
                        fprintf(stdout, "%s %03X, ", (0x80 & DID) ? "DBN" : "SDID", *dat++);
                        DC = *dat++;
                        DC &= 0xFF;
                        fprintf(stdout, "DC %3d, ", DC);
                        dat += DC;
                        CS = *dat++;
                        fprintf(stdout, "CS %3d, ", CS);
                }
                else {
                        dat++;
#if 0
                        fprintf(stdout, "%02X, ", *dat++ >> 2);
#endif
                }
        }

        return 0;
}
