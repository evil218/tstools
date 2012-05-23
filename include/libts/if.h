/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: if.h
 * funx: packet struct <-> txt data convert
 */

#ifndef _IF_H
#define _IF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h> /* for uintN_t, etc */

struct ts_pkt {
        /* NULL means the item is absent */
        uint8_t *ip; /* IP packet */
        uint8_t *rtp; /* RTP packet */
        uint8_t *rtcp; /* RTCP packet */
        uint8_t *tsh; /* TS head */
        uint8_t *ts; /* TS packet */
        uint8_t *rs; /* RS data for TS packet */
        uint8_t *af; /* AF data, adoptint field */
        uint8_t *pesh; /* PES head */
        uint8_t *pes; /* PES fragment */
        uint8_t *es; /* ES fragment */
        uint8_t *sec; /* section data */

        uint8_t *bg; /*  */
        uint8_t *date;
        uint8_t *time;
        uint64_t *mts;
        uint8_t *cts;
        uint64_t *stc;
        uint64_t *addr;
        uint8_t *pcr;
        uint8_t *pts;
        uint8_t *rate;
        uint8_t *rats;
        uint8_t *ratp;
        uint8_t *err;
        uint8_t *si;
        uint8_t *atscmhtcp;

        uint8_t *data;
        uint8_t cnt; /* count of data */

        uint8_t TS[188]; /* TS data */
        uint8_t RS[16]; /* RS data */
        uint64_t ADDR; /* address of sync-byte in stream */
        uint64_t MTS; /* MTS Time Stamp */
        uint64_t STC; /* System Time Clock */
        uint8_t DATA[256]; /* other data */
};

int b2t(char *DST, const uint8_t *PTR, int len);
int next_tag(char **tag, char **text);
int next_nbyte_hex(uint8_t *byte, char **text, int max);
int next_nuint_hex(uint64_t *uint, char **text, int max);

#ifdef __cplusplus
}
#endif

#endif /* _IF_H */
