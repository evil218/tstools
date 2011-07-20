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

/* tag */
#define TAG_TS                          (0x01)
#define TAG_RS                          (0x02)
#define TAG_SRC                         (0x03)
#define TAG_ADDR                        (0x04)
#define TAG_CTS                         (0x05)
#define TAG_DATA                        (0x06)

/* length */
#define LENGTH_TS                       (188)
#define LENGTH_RS                       (16)
#define LENGTH_SRC                      (1)

typedef struct _ts_pkt_t
{
        uint8_t TS[188]; /* TS data */
        uint8_t *ts; /* NULL means no TS */

        uint8_t RS[16]; /* RS data */
        uint8_t *rs; /* NULL means no RS */

        uint8_t SRC; /* source information */
        uint8_t *src; /* NULL means no SRC */

        uint64_t ADDR; /* address of sync-byte in stream */
        uint64_t *addr; /* NULL means no ADDR */

        uint64_t CTS_overflow; /* maximum CTS plus 1 */
        uint64_t CTS; /* Capture Time Stamp */
        uint64_t *cts; /* NULL means no CTS */

        uint8_t DATA[256]; /* other data */
        uint8_t *data; /* NULL means no data */
        uint8_t cnt; /* count of data */
}
ts_pkt_t;

int b2t(void *tbuf, ts_pkt_t *pkt, char ws); /* ws: white_space */
int t2b(ts_pkt_t *pkt, void *tbuf);

#ifdef __cplusplus
}
#endif

#endif /* _IF_H */
