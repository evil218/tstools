/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: UTF_GB
 * funx: UTF-8, UTF-16 and GB code convert
 * 2011-02-10, ZHOU Cheng, init
 */

#ifndef _UTF_GB_H
#define _UTF_GB_H

#ifdef __cplusplus
extern "C" {
#endif

/* cnt: max byte count of source if no '\0' */
/* be: little-endien(0); big-endien(~0) */

int utf8_gb(const char *utf8, char *gb, size_t cnt);
int gb_utf8(const char *gb, char *utf8, size_t cnt);

int utf16_gb(const uint16_t *utf16, char *gb, size_t cnt, int be);
int gb_utf16(const char *gb, uint16_t *utf16, size_t cnt, int be);

int utf8_utf16(const char *utf8, uint16_t *utf16, size_t cnt, int be);
int utf16_utf8(const uint16_t *utf16, char *utf8, size_t cnt, int be);

#ifdef __cplusplus
}
#endif

#endif /* _UTF_GB_H */
