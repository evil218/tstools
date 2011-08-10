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

#include <stdint.h>                     /* for uintN_t, etc */

typedef struct _ts_pkt_t
{
        uint8_t TS[188]; /* TS data */
        uint8_t *ts; /* NULL means no TS */

        uint8_t RS[16]; /* RS data */
        uint8_t *rs; /* NULL means no RS */

        uint64_t ADDR; /* address of sync-byte in stream */
        uint64_t *addr; /* NULL means no ADDR */

        uint64_t MTS; /* MTS Time Stamp */
        uint64_t *mts; /* NULL means no MTS */

        uint64_t STC; /* System Time Clock */
        uint64_t *stc; /* NULL means no STC */

        uint8_t DATA[256]; /* other data */
        uint8_t *data; /* NULL means no data */
        uint8_t cnt; /* count of data */
}
ts_pkt_t;

int pkt_init(ts_pkt_t *pkt);
int b2t(void *tbuf, ts_pkt_t *pkt);
int t2b(ts_pkt_t *pkt, void *tbuf);

#ifdef __cplusplus
}
#endif

#endif /* _IF_H */
