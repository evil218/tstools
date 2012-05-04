/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: if.c
 * funx: packet struct <-> txt data convert
 * To build: gcc -std-c99 -c if.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "if.h"

/* for function to_byte() */
#define NEOL (+1) /* normal end of line */
#define UEOL (-1) /* unexpected end of line */
#define BDWS (-2) /* bad white space */

/* for high speed txt to bin convert */
static const uint8_t t2b_table_h[256] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 00-90 */
        0x00, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* A0-F0 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* a0-f0 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t t2b_table_l[256] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 00-09 */
        0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0A-0F */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0a-0f */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* for high-speed bin to txt convert */
static const char *b2t_table[256] = {
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

/* for high-speed bin to txt convert */
static const char b2t_string[] = "0123456789ABCDEF";

static int uint2txt(uint64_t uint, char **text);
static int byte2txt(uint8_t byte, char **text);
static int txt2byte(uint8_t *byte, char **text);
static int next_tag(char **tag, char **text);
static int next_uint(uint64_t *uint, char **text);

int pkt_init(struct ts_pkt *pkt)
{
        if(pkt) {
                pkt->ts = NULL;
                pkt->rs = NULL;
                pkt->addr = NULL;
                pkt->mts = NULL;
                pkt->stc = NULL;
                pkt->data = NULL;
                return 0;
        }
        else {
                return -1;
        }
}

int b2t(void *tbuf, struct ts_pkt *pkt)
{
        int i;
        char *text = (char *)tbuf;
        uint8_t *byte;

        /* TS */
        if(NULL != pkt->ts) {
                /* tag */
                *text++ = 't';
                *text++ = 's';
                *text++ = ',';

                /* data */
                byte = pkt->ts;
                byte2txt(*byte++, &text);
                for(i = 1; i < 188; i++) {
                        *text++ = ' ';
                        byte2txt(*byte++, &text);
                }
                *text++ = ',';
        }

        /* RS */
        if(NULL != pkt->rs) {
                /* tag */
                *text++ = 'r';
                *text++ = 's';
                *text++ = ',';

                /* data */
                byte = pkt->rs;
                byte2txt(*byte++, &text);
                for(i = 1; i < 16; i++) {
                        *text++ = ' ';
                        byte2txt(*byte++, &text);
                }
                *text++ = ',';
        }

        /* DATA */
        if(NULL != pkt->data) {
                /* tag */
                *text++ = 'd';
                *text++ = 'a';
                *text++ = 't';
                *text++ = 'a';
                *text++ = ',';

                /* data */
                byte = pkt->data;
                byte2txt(*byte++, &text);
                for(i = 1; i < pkt->cnt; i++) {
                        *text++ = ' ';
                        byte2txt(*byte++, &text);
                }
                *text++ = ',';
        }

        /* ADDR */
        if(NULL != pkt->addr) {
                /* tag */
                *text++ = 'a';
                *text++ = 'd';
                *text++ = 'd';
                *text++ = 'r';
                *text++ = ',';

                /* address */
                uint2txt(pkt->ADDR, &text);
                *text++ = ',';
        }

        /* MTS */
        if(NULL != pkt->mts) {
                /* tag */
                *text++ = 'm';
                *text++ = 't';
                *text++ = 's';
                *text++ = ',';

                /* stc */
                uint2txt(pkt->MTS, &text);
                *text++ = ',';
        }

        /* STC */
        if(NULL != pkt->stc) {
                /* tag */
                *text++ = 's';
                *text++ = 't';
                *text++ = 'c';
                *text++ = ',';

                /* stc */
                uint2txt(pkt->STC, &text);
                *text++ = ',';
        }

        *text = '\0';

        return 0;
}

int t2b(struct ts_pkt *pkt, void *tbuf)
{
        int i;
        char *tag;
        uint8_t *pb;
        char *pt = (char *)tbuf;

        pkt_init(pkt);

        while(0 == next_tag(&tag, &pt)) {
                if(0 == strcmp(tag, "ts")) {
                        pb = pkt->TS;
                        for(i = 0; i < 188; i++) {
                                txt2byte(pb, &pt);
                                pb++;
                        }
                        pkt->ts = pkt->TS;
                }
                else if(0 == strcmp(tag, "rs")) {
                        pb = pkt->RS;
                        for(i = 0; i < 16; i++) {
                                txt2byte(pb, &pt);
                                pb++;
                        }
                        pkt->rs = pkt->RS;
                }
                else if(0 == strcmp(tag, "addr")) {
                        if(0 != next_uint(&(pkt->ADDR), &pt)) {
                                return -1;
                        }
                        pkt->addr = &(pkt->ADDR);
                }
                else if(0 == strcmp(tag, "mts")) {
                        if(0 != next_uint(&(pkt->MTS), &pt)) {
                                return -1;
                        }
                        pkt->mts = &(pkt->MTS);
                }
                else if(0 == strcmp(tag, "stc")) {
                        if(0 != next_uint(&(pkt->STC), &pt)) {
                                return -1;
                        }
                        pkt->stc = &(pkt->STC);
                }
                else if(0 == strcmp(tag, "data")) {
                        pb = pkt->DATA;
                        for(pkt->cnt = 0; pkt->cnt < 256; pkt->cnt++) {
                                if(0 != txt2byte(pb, &pt)) {
                                        break;
                                }
                                pb++;
                        }
                        pkt->data = pkt->DATA;
                }
                else {
                        return -1;
                }
        }

        return 0;
}

/* support 8-byte max */
static int uint2txt(uint64_t uint, char **text)
{
        int i;
        int shift;
        uint64_t mask;

        /* init */
        i = 16;
        shift = 60;
        mask = (uint64_t)0x0F;
        mask <<= shift;

        /* right move and check */
        while(i > 1) { /* at least 1-char */
                if(uint & mask) {
                        break;
                }
                i--;
                mask >>= 4;
                shift -= 4;
        }

        /* data */
        while(i > 0) {
                *(*text)++ = b2t_string[(uint & mask) >> shift];
                i--;
                mask >>= 4;
                shift -= 4;
        }

        return 0;
}

static int byte2txt(uint8_t byte, char **text)
{
        const char *ch;

        ch = b2t_table[byte];
        *(*text)++ = *ch++;
        *(*text)++ = *ch;

        return 0;
}

static int txt2byte(uint8_t *byte, char **text)
{
        uint8_t h, l, s;

        h = *(*text);
        if('\0' == h || 0x0A == h || 0x0D == h) {
                return NEOL;
        }
        (*text)++;

        l = *(*text);
        if('\0' == l || 0x0A == l || 0x0D == l) {
                fprintf(stderr, "unexpected end of line: 0x%02X\n", (int)l);
                return UEOL;
        }
        (*text)++;

        s = *(*text);
        if('\0' == s || 0x0A == s || 0x0D == s) {
                return NEOL;
        }
        else if((('0' <= s) && (s <= '9')) ||
                (('A' <= s) && (s <= 'F')) ||
                (('a' <= s) && (s <= 'f'))) {
                fprintf(stderr, "Bad white space: 0x%02X\n", (int)s);
                return BDWS;
        }
        else {
                /* other white space, it is OK */
        }
        (*text)++;

        *byte = (t2b_table_h[h] | t2b_table_l[l]);
        return 0;
}

static int next_tag(char **tag, char **text)
{
        char ch;

        while(1) {
                ch = *(*text);
                if('\0' == ch || 0x0A == ch || 0x0D == ch) {
                        /* normal EOL */
                        *(*text) = '\0';
                        return NEOL;
                }
                else if((('A' <= ch) && (ch <= 'Z')) ||
                        (('a' <= ch) && (ch <= 'z'))) {
                        /* tag head */
                        *tag = *text;
                        (*text)++;
                        break;
                }
                else {
                        /* white space */
                        (*text)++;
                }
        }
        while(1) {
                ch = *(*text);
                if((('0' <= ch) && (ch <= '9')) ||
                   (('A' <= ch) && (ch <= 'Z')) ||
                   (('a' <= ch) && (ch <= 'z'))) {
                        /* tag */
                        (*text)++;
                }
                else if('\0' == ch || 0x0A == ch || 0x0D == ch) {
                        /* unexpected EOL */
                        *(*text) = '\0';
                        return UEOL;
                }
                else {
                        /* tag tail */
                        *(*text)++ = '\0';
                        return 0;
                }
        }
}

static int next_uint(uint64_t *uint, char **text)
{
        char ch;

        *uint = 0;
        while(1) {
                ch = *(*text);
                if(('0' <= ch) && (ch <= '9')) {
                        *uint <<= 4;
                        *uint += (ch - '0');
                }
                else if(('A' <= ch) && (ch <= 'F')) {
                        *uint <<= 4;
                        *uint += (ch - 'A' + 10);
                }
                else if(('a' <= ch) && (ch <= 'f')) {
                        *uint <<= 4;
                        *uint += (ch - 'a' + 10);
                }
                else if('\0' == ch || 0x0A == ch || 0x0D == ch) {
                        /* EOL */
                        return 0;
                }
                else {
                        /* white space */
                        (*text)++;
                        return 0;
                }
                (*text)++;
        }
}
