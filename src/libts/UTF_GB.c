/* vim: set tabstop=8 shiftwidth=8: */

#include <stdio.h>
#include <stdint.h> /* for uint?_t, etc */

#include "UTF_GB.h"

#define DFLT_UCS                        (0x00D7) /* look like 'X' */
#define DFLT_GB                         (0xA1C1) /* look like 'X' */

static const uint16_t GB2UCS[] = {
#include "G2U.h" /* big file, only be included here */
};

static const uint16_t UCS2GB[] = {
#include "U2G.h" /* big file, only be included here */
};

static int utf8_to_ucs4(const char **utf8, uint32_t *ucs4);
static void ucs4_to_utf8(uint32_t ucs4, char **utf8);

static void ucs4_to_utf16(uint32_t ucs4, uint16_t **utf16, int endian);
static void utf16_to_ucs4(const uint16_t **utf16, uint32_t *ucs4, int endian);

static uint16_t half_search(uint16_t dat, uint16_t dflt, int hi, const uint16_t *p);

int utf8_gb(const char *utf8, char *gb, size_t cnt)
{
        int wc = 0; /* word count */
        const char *putf = utf8;
        uint32_t ucs4; /* UCS-4 data */
        uint16_t utf16; /* UTF-16 data */
        uint16_t gb2; /*  GB 2-byte data */
        int max = sizeof(GB2UCS) / 4 - 1; /* high index */

        while(cnt > 0) {
                cnt -= utf8_to_ucs4(&putf, &ucs4);
                if(0x00000000 != (ucs4 & 0xFFFF0000)) {
                        fprintf(stderr, "UCS data(0x%08X) is bigger than 0xFFFF!\n", ucs4);
                        break;
                }
                utf16 = (uint16_t)ucs4;

                if(utf16 == 0x0000) {
                        break;
                }
                else if(utf16 <= 0x007F) {
                        *gb++ = (char)utf16;
                }
                else {
                        gb2 = half_search(utf16, DFLT_GB, max, UCS2GB);
                        *gb++ = (char)(gb2 >> 8);
                        *gb++ = (char)(gb2 >> 0);
                }
                wc++;
        }
        *gb = '\0';

        return wc;
}

int gb_utf8(const char *gb, char *utf8, size_t cnt)
{
        int wc = 0; /* word count */
        char *putf = utf8;
        uint32_t ucs4; /* UCS-4 data */
        uint16_t ucs2; /* UCS-2 data */
        uint16_t gb2; /*  GB 2-byte data */
        int max = sizeof(GB2UCS) / 4 - 1; /* high index */

        while(cnt > 0) {
                gb2 = *gb++;
                if(gb2 == 0x0000) {
                        break;
                }
                else if(gb2 <= 0x007F) {
                        *putf++ = (char)gb2;
                        cnt -= 1;
                }
                else {
                        gb2 <<= 8;
                        gb2 |= (uint16_t)(uint8_t)(*gb++);
                        ucs2 = half_search(gb2, DFLT_UCS, max, GB2UCS);
                        ucs4 = (uint32_t)ucs2;
                        ucs4_to_utf8(ucs4, &putf);
                        cnt -= 2;
                }
                wc++;
        }
        *putf = 0x00;

        return wc;
}

int utf16_gb(const uint16_t *utf16, char *gb, size_t cnt, int endian)
{
        int wc = 0; /* word count */
        uint16_t utf_16; /* UTF-16 data */
        uint16_t gb2; /* GB 2-byte data */
        int max = sizeof(UCS2GB) / 4 - 1; /* high index */

        while(cnt > 0) {
                utf_16 = *utf16++;
                utf_16 = (BIG_ENDIAN == endian) ? be16toh(utf_16) : le16toh(utf_16);

                if((0xD800 <= utf_16) && (utf_16 <= 0xDFFF)) {
                        fprintf(stderr, "Unsupported UTF-16 data(0x%04X)!\n", utf_16);
                        break;
                }

                if(utf_16 == 0x0000) {
                        break;
                }
                else if(utf_16 <= 0x007F) {
                        *gb++ = (char)utf_16;
                        cnt -= 2;
                }
                else {
                        gb2 = half_search(utf_16, DFLT_GB, max, UCS2GB);
                        *gb++ = (char)(gb2 >> 8);
                        *gb++ = (char)(gb2 >> 0);
                        cnt -= 2;
                }
                wc++;
        }
        *gb = '\0';

        return wc;
}

int gb_utf16(const char *gb, uint16_t *utf16, size_t cnt, int endian)
{
        int wc = 0; /* word count */
        uint16_t gb2; /*  GB 2-byte data */
        uint16_t utf_16; /* UTF-16 data */
        int max = sizeof(GB2UCS) / 4 - 1; /* high index */

        while(cnt > 0) {
                gb2 = (uint16_t)*gb++;
                if(gb2 == 0x0000) {
                        break;
                }
                else if(gb2 <= 0x007F) {
                        utf_16 = gb2;
                        *utf16++ = (BIG_ENDIAN == endian) ? htobe16(utf_16) : htole16(utf_16);
                        cnt -= 1;
                }
                else {
                        gb2 <<= 8;
                        gb2 |= (uint16_t)(uint8_t)(*gb++);
                        utf_16 = half_search(gb2, DFLT_UCS, max, GB2UCS);
                        *utf16++ = (BIG_ENDIAN == endian) ? htobe16(utf_16) : htole16(utf_16);
                        cnt -= 2;
                }
                wc++;
        }
        *utf16 = 0x0000;

        return wc;
}

int utf8_utf16(const char *utf8, uint16_t *utf16, size_t cnt, int endian)
{
        int wc = 0; /* word count */
        const char *putf = utf8;
        uint16_t *putf16 = utf16;
        uint32_t ucs4; /* UCS-4 data */

        while(cnt > 0) {
                cnt -= utf8_to_ucs4(&putf, &ucs4);
                if(0x00000000 == ucs4) {
                        break;
                }
                ucs4_to_utf16(ucs4, &putf16, endian);
                wc++;
        }
        *putf16 = 0x0000;

        return wc;
}

int utf16_utf8(const uint16_t *utf16, char *utf8, size_t cnt, int endian)
{
        int wc = 0; /* word count */
        const uint16_t *putf16 = utf16;
        char *putf8 = utf8;
        uint32_t ucs4; /* UCS-4 data */

        while(cnt > 0) {
                utf16_to_ucs4(&putf16, &ucs4, endian);
                if(0x00000000 == ucs4) {
                        break;
                }
                ucs4_to_utf8(ucs4, &putf8);
                cnt -= 2;
                wc++;
        }
        *putf8 = 0x00;

        return wc;
}

static int utf8_to_ucs4(const char **utf8, uint32_t *ucs4)
{
        size_t cnt;
        uint8_t head; /* UTF-8 head data */
        uint8_t tail; /* UTF-8 tail data, maybe 0~5-byte */
        int tc; /* tail count, the number of tail data */
        uint32_t ucs_4; /* UCS-4 data */

        /* head */
        head = *(*utf8)++;
        if(head <= 0x7F) {
                tc = 0;
                ucs_4 = head; /* 0xxx xxxx */
        }
        else if((0xC0 <= head) && (head <= 0xDF)) {
                tc = 1;
                ucs_4 = head & 0x1F; /* 110x xxxx */
        }
        else if(head <= 0xEF) {
                tc = 2;
                ucs_4 = head & 0x0F; /* 1110 xxxx */
        }
        else if(head <= 0xF7) {
                tc = 3;
                ucs_4 = head & 0x07; /* 1111 0xxx */
        }
        else if(head <= 0xFB) {
                tc = 4;
                ucs_4 = head & 0x03; /* 1111 10xx */
        }
        else if(head <= 0xFD) {
                tc = 5;
                ucs_4 = head & 0x01; /* 1111 110x */
        }
        else {
                /* incorrect UTF-8 head: 0xFE or 0xFF or 10xx xxxx */
                tc = 0;
                ucs_4 = DFLT_UCS;
        }
        cnt = tc + 1;

        /* tail */
        for(; tc != 0; tc--) {
                tail = *(*utf8)++;
                if((tail & 0xC0) == 0x80) {
                        ucs_4 <<= 6;
                        ucs_4 |= (tail & 0x3F); /* 10xx xxxx */
                }
                else {
                        (*utf8)--; /* bad tail, maybe next UTF-8 head */
                        ucs_4 = DFLT_UCS;
                        break;
                }
        }

        *ucs4 = ucs_4;
        return cnt;
}

static void ucs4_to_utf8(uint32_t ucs4, char **utf8)
{
        int shft; /* shift ucs4 to get n-bit data */

        /* head */
        if(ucs4 <= 0x0000007F) {
                shft = 0;
                *(*utf8)++ = (uint8_t)(ucs4 >> shft); /* 0xxx xxxx */
        }
        else if(ucs4 <= 0x000007FF) {
                shft = 6;
                *(*utf8)++ = (0xC0 | (uint8_t)(ucs4 >> shft)); /* 110x xxxx */
        }
        else if(ucs4 <= 0x0000FFFF) {
                shft = 12;
                *(*utf8)++ = (0xE0 | (uint8_t)(ucs4 >> shft)); /* 1110 xxxx */
        }
        else if(ucs4 <= 0x001FFFFF) {
                shft = 18;
                *(*utf8)++ = (0xF0 | (uint8_t)(ucs4 >> shft)); /* 1111 0xxx */
        }
        else if(ucs4 <= 0x03FFFFFF) {
                shft = 24;
                *(*utf8)++ = (0xF8 | (uint8_t)(ucs4 >> shft)); /* 1111 10xx */
        }
        else if(ucs4 <= 0x7FFFFFFF) {
                shft = 30;
                *(*utf8)++ = (0xFC | (uint8_t)(ucs4 >> shft)); /* 1111 110x */
        }
        else {
                /* bad UCS-4 data, ignore */
                shft = 0;
                *(*utf8)++ = 0x00;
        }

        /* tail */
        for(shft -= 6; shft >= 0; shft -= 6) {
                *(*utf8)++ = (uint8_t)(0x80 | (0x3F & (ucs4 >> shft))); /* 10xx xxxx; */
        }

        return;
}

static void ucs4_to_utf16(uint32_t ucs4, uint16_t **utf16, int endian)
{
        uint16_t utf_16; /* UTF-16 data */

        if(ucs4 <= 0x0000FFFF) {
                utf_16 = (uint16_t)ucs4;
                *(*utf16)++ = (BIG_ENDIAN == endian) ? htobe16(utf_16) : htole16(utf_16);
        }
        else if(ucs4 <= 0x0010FFFF) {
                ucs4 -= 0x00010000;
                utf_16 = (uint16_t)(0xD800 + ucs4 / 0x0400);
                *(*utf16)++ = (BIG_ENDIAN == endian) ? htobe16(utf_16) : htole16(utf_16);
                utf_16 = (uint16_t)(0xDC00 + ucs4 % 0x0400);
                *(*utf16)++ = (BIG_ENDIAN == endian) ? htobe16(utf_16) : htole16(utf_16);
        }
        else {
                fprintf(stderr,
                        "UCS-4 data(0x%08X) is bigger than 0x0010FFFF, s"
                        "no UTF-16 mapping is defined!\n", ucs4);
        }

        return;
}

static void utf16_to_ucs4(const uint16_t **utf16, uint32_t *ucs4, int endian)
{
        uint16_t utf16_x;
        uint16_t utf16_y;
        uint32_t ucs_4;

        utf16_x = *(*utf16)++;
        utf16_x = (BIG_ENDIAN == endian) ? be16toh(utf16_x) : le16toh(utf16_x);
        if((utf16_x <= 0xD7FF) || (0xE000 <= utf16_x)) {
                ucs_4 = (uint32_t)utf16_x;
        }
        else if((0xDC00 <= utf16_x) && (utf16_x <= 0xDFFF)) {
                utf16_y = *(*utf16)++;
                utf16_y = (BIG_ENDIAN == endian) ? be16toh(utf16_y) : le16toh(utf16_y);
                if((0xDC00 <= utf16_y) && (utf16_y <= 0xDFFF)) {
                        ucs_4 = (utf16_x - 0xD800);
                        ucs_4 <<= 10;
                        ucs_4 += (utf16_y - 0xDC00);
                        ucs_4 += 0x00010000;
                }
                else {
                        fprintf(stderr,
                                "Bad UTF-16 y data(0x%04X)!\n", utf16_y);
                        (*utf16)--; /* maybe next UTF-16 x data */
                        ucs_4 = DFLT_UCS;
                }
        }
        else {
                fprintf(stderr,
                        "Bad UTF-16 x data(0x%04X)!\n", utf16_x);
                ucs_4 = DFLT_UCS;
        }

        *ucs4 = ucs_4;
        return;
}

static uint16_t half_search(uint16_t key, uint16_t dflt, int hi, const uint16_t *hash)
{
        int i;
        int li = 0;
        const uint16_t *p;
        uint16_t value;
        uint16_t rslt;

        while(1) {
                if(li > hi) {
                        /* mismatch, use default data */
                        rslt = dflt;
                        break;
                }

                i = (li + hi) >> 1; /* half */
                p = (hash + (i << 1)); /* 2-data per item(key, value) */
                value = *p;
                if(key == value) {
                        rslt = *(p + 1);
                        break;
                }
                else if(key > value) {
                        li = i + 1;
                }
                else { //(key < value)
                        hi = i - 1;
                }
        }

        return rslt;
}
