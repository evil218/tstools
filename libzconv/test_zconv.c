/* vim: set tabstop=8 shiftwidth=8:
 * funx: to test zconv module
 * comp: gcc test.c -L. -lzconv
 */

#include <stdio.h>
#include <stdint.h> /* for uint?_t, etc */

#include "zconv.h"

static uint8_t latin[256] = {
};

void show_latin(int coding, const char *hint);
void show_gb(int X0, int X1, int Y0, int Y1, const char *hint);

void main(void)
{
        int i;
        uint8_t *p;

        p = latin;
#if 1
        for(i = 0x20; i <= 0x7F; i++) {
                *p++ = i;
        }
#endif
        for(i = 0xA0; i <= 0xFF; i++) {
                *p++ = i;
        }
        *p++ = '\0';

        show_gb(0xA1, 0xA1, 0xA9, 0xFE, "GB_B2-CC-1");
        show_gb(0xB0, 0xA1, 0xF7, 0xFE, "GB_B2-CC-2");
        show_gb(0x81, 0x40, 0xA0, 0xFE, "GB_B2-CC-3(do NOT support now)");
        show_gb(0xAA, 0x40, 0xFE, 0xA0, "GB_B2-CC-4(do NOT support now)");
        show_gb(0xA8, 0x40, 0xA9, 0xA0, "GB_B2-CC-5(do NOT support now)");

        show_latin(CODING_ISO6937   , "ISO6937---");
        show_latin(CODING_DVB6937   , "DVB6937---");
        show_latin(CODING_ISO8859_1 , "ISO8859--1");
        show_latin(CODING_ISO8859_2 , "ISO8859--2");
        show_latin(CODING_ISO8859_3 , "ISO8859--3");
        show_latin(CODING_ISO8859_4 , "ISO8859--4");
        show_latin(CODING_ISO8859_5 , "ISO8859--5");
        show_latin(CODING_DVB8859_5 , "DVB8859--5");
        show_latin(CODING_ISO8859_6 , "ISO8859--6");
        show_latin(CODING_ISO8859_7 , "ISO8859--7");
        show_latin(CODING_ISO8859_8 , "ISO8859--8");
        show_latin(CODING_ISO8859_9 , "ISO8859--9");
        show_latin(CODING_ISO8859_10, "ISO8859-10");
        show_latin(CODING_ISO8859_11, "ISO8859-11");
        show_latin(CODING_ISO8859_13, "ISO8859-13");
        show_latin(CODING_ISO8859_14, "ISO8859-14");
        show_latin(CODING_ISO8859_15, "ISO8859-15");
        show_latin(CODING_DVB8859_15, "DVB8859-15");

        return;
}

void show_latin(int coding, const char *hint)
{
        uint8_t str[256 * 3 + 10];

        fprintf(stdout, "%8s: ", hint);
        latin_utf8(latin, str, 256, coding);
        fprintf(stdout, "%s\n", str);
}

void show_gb(int Y0, int X0, int Y1, int X1, const char *hint)
{
        int y, x;
        uint8_t dat[256 * 2];
        uint8_t *p;
        uint8_t str[256 * 10 + 10] = "\0";

        fprintf(stdout, "%s:\n", hint);
        for(y = Y0; y <= Y1; y++)
        {
                fprintf(stdout, "%02X(%02X, %02X): ", y, X0, X1);
                p = dat;
                for(x = X0; x <= X1; x++)
                {
                        if(x == 0x7F)
                        {
                                /* 0x7F break */
                                continue;
                        }
                        if(0xAA <= y && 0xA1 <= x && y <= 0xAF && x <= 0xFE)
                        {
                                /* GB_B2-USR-1 */
                                continue;
                        }
                        if(0xF8 <= y && 0xA1 <= x && y <= 0xFE && x <= 0xFE)
                        {
                                /* GB_B2-USR-2 */
                                continue;
                        }
                        if(0xA1 <= y && 0x40 <= x && y <= 0xA7 && x <= 0xA0)
                        {
                                /* GB_B2-USR-3 */
                                continue;
                        }

                        *p++ = y;
                        *p++ = x;
                }
                *p++ = '\0';

                gb_utf8(dat, str, p - dat);
                fprintf(stdout, "%s\n", str);
        }
        fprintf(stdout, "\n");
}
