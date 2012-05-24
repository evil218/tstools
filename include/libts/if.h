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

int b2t(char *DST, const uint8_t *PTR, int len);
int next_tag(char **tag, char **text);
int next_nbyte_hex(uint8_t *byte, char **text, int max);
int next_nuint_hex(int64_t *sint, char **text, int max);

#ifdef __cplusplus
}
#endif

#endif /* _IF_H */
