/* vim: set tabstop=8 shiftwidth=8:
 * name: zconv(zhoucheng convert)
 * funx: Latin, UTF-8, UTF-16, UTF-16BE and GBxxxxx buffer convert
 * 2013-12-19, ZHOU Cheng, add latin to utf-8 support for dvb string
 * 2011-02-10, ZHOU Cheng, init as UTF_GB module
 */

#ifndef _ZCONV_H
#define _ZCONV_H

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

#define CODING_DVB6937_P  (0)  /* Latin alphabet(Super ASCII) + Euro symbol(U+20AC) */
#define CODING_DVB8859_1  (1)  /* West European */
#define CODING_DVB8859_2  (2)  /* East European */
#define CODING_DVB8859_3  (3)  /* South European */
#define CODING_DVB8859_4  (4)  /* North and North-East European */
#define CODING_DVB8859_5  (5)  /* Latin/Cyrillic */
#define CODING_DVB8859_6  (6)  /* Latin/Arabic */
#define CODING_DVB8859_7  (7)  /* Latin/Greek */
#define CODING_DVB8859_8  (8)  /* Latin/Hebrew */
#define CODING_DVB8859_9  (9)  /* West European & Turkish */
#define CODING_DVB8859_10 (10) /* North European */
#define CODING_DVB8859_11 (11) /* Thai */
#define CODING_DVB8859_13 (13) /* Baltic */
#define CODING_DVB8859_14 (14) /* Celtic */
#define CODING_DVB8859_15 (15) /* West European */

#define CODING_UTF8     (20) /* UTF8 */
#define CODING_UTF16BE  (21) /* UTF16, big endian */
#define CODING_KSX1001  (22) /* Korean Character Set */
#define CODING_GB2312   (23) /* GB-2312-1980 */
#define CODING_BIG5     (24) /* BIG5 subset of ISO10646 */
#define CODING_BBC      (25) /* encoding_type_ID: BBC */
#define CODING_MTFSB    (26) /* encoding_type_ID: Malaysian Technical Standard Forum Bhd */

#define CODING_RESERVED (99) /* reserved */

/* brief        convert string between latin, utf8, utf16, utf16be and gb
 * param        cnt     max byte count of source if no '\0'
 *              latin   latin buffer, '\0' end or cnt-byte
 *              coding  CODING_xxxx of latin charsets
 *              endian  BIG_ENDIAN or LITTLE_ENDIAN for utf16 data
 *              utf8    utf8 buffer, '\0' end or cnt-byte
 *              utf16   utf16 buffer, 0x0000 end or cnt-byte, BE or LE
 *              gb      gbxxxxx buffer, '\0' end or cnt-byte
 * return       the number of words the fxn succeeded in converting
 */

int latin_utf8(const uint8_t *latin, char *utf8, size_t cnt, int coding);

int utf8_gb(const char *utf8, char *gb, size_t cnt);
int gb_utf8(const char *gb, char *utf8, size_t cnt);

int utf16_gb(const uint16_t *utf16, char *gb, size_t cnt, int endian);
int gb_utf16(const char *gb, uint16_t *utf16, size_t cnt, int endian);

int utf8_utf16(const char *utf8, uint16_t *utf16, size_t cnt, int endian);
int utf16_utf8(const uint16_t *utf16, char *utf8, size_t cnt, int endian);

#ifdef __cplusplus
}
#endif

#endif /* _ZCONV_H */
