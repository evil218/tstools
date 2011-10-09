/* vim: set tabstop=8 shiftwidth=8:
 * name: atsc_mh_tcp
 * funx: parse each field of ATSC M/H TCP packet
 *
 * 2011-10-09, LI WenYan, Init for debug ATSC M/H MUX card
 */

#ifndef _ATSC_MH_TCP_H
#define _ATSC_MH_TCP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h> /* for uintN_t, etc */

void show_tcp(uint8_t *ts);

#ifdef __cplusplus
}
#endif

#endif /* _ATSC_MH_TCP_H */
