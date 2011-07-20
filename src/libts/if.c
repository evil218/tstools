/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: if.c
 * funx: packet struct <-> txt data convert
 * To build: gcc -std-c99 -c if.c
 */

#include <stdio.h>
#include <stdlib.h>

#include "libts/error.h"
#include "libts/if.h"

/* for function to_byte() */
#define NEOL (+1) /* normal end of line */
#define UEOL (-1) /* unexpected end of line */
#define BDWS (-2) /* bad white space */

/* for high speed txt to bin convert */
static const uint8_t t2b_table_h[256] =
{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x0X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x1X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x2X */
        0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x3X */
        0x00, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x4X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x5X */
        0x00, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x6X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x7X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x8X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x9X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xAX */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xBX */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xCX */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xDX */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xEX */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* 0xFX */
};
static const uint8_t t2b_table_l[256] =
{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x0X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x1X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x2X */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x3X */
        0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x4X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x5X */
        0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x6X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x7X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x8X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x9X */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xAX */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xBX */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xCX */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xDX */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xEX */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* 0xFX */
};

/* for high-speed bin to txt convert */
static const char *b2t_table[256] =
{
        "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B", "0C", "0D", "0E", "0F",
        "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "1A", "1B", "1C", "1D", "1E", "1F",
        "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "2A", "2B", "2C", "2D", "2E", "2F",
        "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3A", "3B", "3C", "3D", "3E", "3F",
        "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4A", "4B", "4C", "4D", "4E", "4F",
        "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "5A", "5B", "5C", "5D", "5E", "5F",
        "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6A", "6B", "6C", "6D", "6E", "6F",
        "70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "7A", "7B", "7C", "7D", "7E", "7F",
        "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F",
        "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9A", "9B", "9C", "9D", "9E", "9F",
        "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF",
        "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "BA", "BB", "BC", "BD", "BE", "BF",
        "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA", "CB", "CC", "CD", "CE", "CF",
        "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF",
        "E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF",
        "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "FA", "FB", "FC", "FD", "FE", "FF"
};

static int uint_len(uint64_t uint);
static int uint2txt(uint64_t uint, char **text, char ws);
static int to_text(uint8_t byte, char **text);
static int to_byte(uint8_t *byte, char **text);

int b2t(void *tbuf, ts_pkt_t *pkt, char ws)
{
        int i;
        char *text = (char *)tbuf;
        uint8_t len;
        uint8_t *byte;

        /* TS */
        if(NULL != pkt->ts)
        {
                /* tag */
                to_text(TAG_TS, &text);
                *text++ = ws;

                /* length */
                to_text(LENGTH_TS, &text);
                *text++ = ',';

                /* data */
                byte = pkt->ts;
                to_text(*byte++, &text);
                for(i = 1; i < LENGTH_TS; i++)
                {
                        *text++ = ws;
                        to_text(*byte++, &text);
                }
                *text++ = ',';
        }

        /* RS */
        if(NULL != pkt->rs)
        {
                /* tag */
                to_text(TAG_RS, &text);
                *text++ = ws;

                /* length */
                to_text(LENGTH_RS, &text);
                *text++ = ',';

                /* data */
                byte = pkt->rs;
                to_text(*byte++, &text);
                for(i = 1; i < LENGTH_RS; i++)
                {
                        *text++ = ws;
                        to_text(*byte++, &text);
                }
                *text++ = ',';
        }

        /* ADDR */
        if(NULL != pkt->addr)
        {
                /* tag */
                to_text(TAG_ADDR, &text);
                *text++ = ws;

                /* length & data */
                uint2txt(pkt->ADDR, &text, ws);
                *text++ = ',';
        }

        /* CTS */
        if(NULL != pkt->cts)
        {
                /* tag */
                to_text(TAG_CTS, &text);
                *text++ = ws;

                /* length */
                len = 2;
                len += uint_len(pkt->CTS_overflow);
                len += uint_len(pkt->CTS);
                to_text(len, &text);
                *text++ = ',';

                /* length & data */
                uint2txt(pkt->CTS_overflow, &text, ws);
                *text++ = ',';

                /* length & data */
                uint2txt(pkt->CTS, &text, ws);
                *text++ = ',';
        }

        /* DATA */
        if(NULL != pkt->data)
        {
                /* tag */
                to_text(TAG_DATA, &text);
                *text++ = ws;

                /* length */
                to_text(pkt->cnt, &text);
                *text++ = ',';

                /* data */
                byte = pkt->data;
                to_text(*byte++, &text);
                for(i = 1; i < pkt->cnt; i++)
                {
                        *text++ = ws;
                        to_text(*byte++, &text);
                }
                *text++ = ',';
        }

        *text = '\0';

        return 0;
}

int t2b(ts_pkt_t *pkt, void *tbuf)
{
        int i;
        int exit;
        uint8_t tag;
        uint8_t len;
        uint8_t byte;
        uint8_t *pb;
        char *pt = (char *)tbuf;

        pkt->ts = NULL;
        pkt->rs = NULL;
        pkt->src = NULL;
        pkt->addr = NULL;
        pkt->cts = NULL;
        pkt->data = NULL;

        exit = 0;
        while(0 == exit)
        {
                exit = to_byte(&tag, &pt);
                exit = to_byte(&len, &pt);
                if(0 != exit)
                {
                        break;
                }
                switch(tag)
                {
                        case TAG_TS:
                                pb = pkt->TS;
                                for(i = 0; i < len; i++)
                                {
                                        exit = to_byte(pb, &pt);
                                        pb++;
                                }
                                pkt->ts = pkt->TS;
                                break;
                        case TAG_RS:
                                pb = pkt->RS;
                                for(i = 0; i < len; i++)
                                {
                                        exit = to_byte(pb, &pt);
                                        pb++;
                                }
                                pkt->rs = pkt->RS;
                                break;
                        case TAG_DATA:
                                pb = pkt->DATA;
                                for(i = 0; i < len; i++)
                                {
                                        exit = to_byte(pb, &pt);
                                        pb++;
                                }
                                pkt->cnt = len;
                                pkt->data = pkt->DATA;
                                break;
                        case TAG_SRC:
                                pb = &(pkt->SRC);
                                for(i = 0; i < len; i++)
                                {
                                        exit = to_byte(pb, &pt);
                                        pb++;
                                }
                                pkt->src = &(pkt->SRC);
                                break;
                        case TAG_ADDR:
                                pkt->ADDR = 0;
                                for(i = 0; i < len; i++)
                                {
                                        exit = to_byte(&byte, &pt);
                                        pkt->ADDR <<= 8;
                                        pkt->ADDR |= byte;
                                }
                                pkt->addr = &(pkt->ADDR);
                                break;
                        case TAG_CTS:
                                /* overflow */
                                exit = to_byte(&len, &pt);
                                pkt->CTS_overflow = 0;
                                for(i = 0; i < len; i++)
                                {
                                        exit = to_byte(&byte, &pt);
                                        pkt->CTS_overflow <<= 8;
                                        pkt->CTS_overflow |= byte;
                                }

                                /* CTS */
                                exit = to_byte(&len, &pt);
                                pkt->CTS = 0;
                                for(i = 0; i < len; i++)
                                {
                                        exit = to_byte(&byte, &pt);
                                        pkt->CTS <<= 8;
                                        pkt->CTS |= byte;
                                }
                                pkt->cts = &(pkt->CTS);
                                break;
                        default:
                                exit = 1;
                                break;
                }
        }

        exit = (NEOL == exit) ? 0 : exit;
        return exit;
}

/* support 8-byte max */
static int uint_len(uint64_t uint)
{
        int i;
        int shift;
        uint64_t mask;

        /* init */
        i = 8;
        shift = 56;
        mask = (uint64_t)0xFF;
        mask <<= shift;

        /* right move and check */
        while(i > 1) /* at least 1-byte */
        {
                if(uint & mask) break;

                i--;
                mask >>= 8;
                shift -= 8;
        }

        return i;
}

/* support 8-byte max */
static int uint2txt(uint64_t uint, char **text, char ws)
{
        int i;
        int shift;
        uint64_t mask;

        /* len */
        i = uint_len(uint);
        shift = 8 * (i - 1);
        mask = (uint64_t)0xFF;
        mask <<= shift;

        to_text(i, text);
        *(*text)++ = ',';

        /* data */
        to_text(((uint & mask) >> shift), text);

        i--;
        mask >>= 8;
        shift -= 8;

        while(i > 0)
        {
                *(*text)++ = ws;
                to_text(((uint & mask) >> shift), text);

                i--;
                mask >>= 8;
                shift -= 8;
        }

        return 0;
}

static int to_text(uint8_t byte, char **text)
{
        const char *ch;

        ch = b2t_table[byte];
        *(*text)++ = *ch++;
        *(*text)++ = *ch;

        return 0;
}

static int to_byte(uint8_t *byte, char **text)
{
        uint8_t h, l, s;

        h = *(*text);
        if('\0' == h || 0x0A == h || 0x0D == h)
        {
                return NEOL;
        }
        (*text)++;

        l = *(*text);
        if('\0' == l || 0x0A == l || 0x0D == l)
        {
                fprintf(stderr, "unexpected end of line: 0x%02X\n", (int)l);
                return UEOL;
        }
        (*text)++;

        s = *(*text);
        if('\0' == s || 0x0A == s || 0x0D == s)
        {
                return NEOL;
        }
        else if((('0' <= s) && (s <= '9')) ||
                (('A' <= s) && (s <= 'F')) ||
                (('a' <= s) && (s <= 'f')))
        {
                fprintf(stderr, "Bad white space: 0x%02X\n", (int)s);
                return BDWS;
        }
        else
        {
                /* other white space, it is OK */
        }
        (*text)++;

        *byte = (t2b_table_h[h] | t2b_table_l[l]);
        return 0;
}
