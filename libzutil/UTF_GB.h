/* vim: set tabstop=8 shiftwidth=8:
 * name: UTF_GB
 * funx: UTF-8, UTF-16, UTF-16BE and GBxxxxx buffer convert
 * 2011-02-10, ZHOU Cheng, init
 */

#ifndef _UTF_GB_H
#define _UTF_GB_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIG_ENDIAN
# define BIG_ENDIAN 4321
#endif
#ifndef LITTLE_ENDIAN
# define LITTLE_ENDIAN 1234
#endif

#ifndef __BYTE_ORDER
# define __BYTE_ORDER LITTLE_ENDIAN /* WARNING: depend on your system! */

#if __BYTE_ORDER == LITTLE_ENDIAN

#define htobe16(x) ((x >> 8) | (x << 8))
#define be16toh(x) ((x >> 8) | (x << 8))
#define htole16(x) (x)
#define le16toh(x) (x)

#endif /* __BYTE_ORDER == LITTLE_ENDIAN*/

#if __BYTE_ORDER == BIG_ENDIAN

#define htobe16(x) (x)
#define be16toh(x) (x)
#define htole16(x) ((x >> 8) | (x << 8))
#define le16toh(x) ((x >> 8) | (x << 8))

#endif /* __BYTE_ORDER == BIG_ENDIAN*/

#endif

/* brief        convert string between utf8, utf16, utf16be and gb
 * param        cnt     max byte count of source if no '\0'
 *              endian  BIG_ENDIAN or LITTLE_ENDIAN for utf16 data
 *              utf8    utf8 buffer, '\0' end or cnt-byte
 *              utf16   utf16 buffer, 0x0000 end or cnt-byte, BE or LE
 *              gb      gbxxxxx buffer, '\0' end or cnt-byte
 * return       the number of words the fxn succeeded in converting
 */

int utf8_gb(const char *utf8, char *gb, size_t cnt);
int gb_utf8(const char *gb, char *utf8, size_t cnt);

int utf16_gb(const uint16_t *utf16, char *gb, size_t cnt, int endian);
int gb_utf16(const char *gb, uint16_t *utf16, size_t cnt, int endian);

int utf8_utf16(const char *utf8, uint16_t *utf16, size_t cnt, int endian);
int utf16_utf8(const uint16_t *utf16, char *utf8, size_t cnt, int endian);

#ifdef __cplusplus
}
#endif

#endif /* _UTF_GB_H */
