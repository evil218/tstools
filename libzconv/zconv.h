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

#define CODING_ISO6937    (0x0000) /* Latin alphabet(Super ASCII) */
#define CODING_ISO8859_1  (0x0001) /* West European */
#define CODING_ISO8859_2  (0x0002) /* East European */
#define CODING_ISO8859_3  (0x0003) /* South European */
#define CODING_ISO8859_4  (0x0004) /* North and North-East European */
#define CODING_ISO8859_5  (0x0005) /* Latin/Cyrillic */
#define CODING_ISO8859_6  (0x0006) /* Latin/Arabic */
#define CODING_ISO8859_7  (0x0007) /* Latin/Greek */
#define CODING_ISO8859_8  (0x0008) /* Latin/Hebrew */
#define CODING_ISO8859_9  (0x0009) /* West European & Turkish */
#define CODING_ISO8859_10 (0x000A) /* North European */
#define CODING_ISO8859_11 (0x000B) /* Thai */
#define CODING_ISO8859_13 (0x000D) /* Baltic */
#define CODING_ISO8859_14 (0x000E) /* Celtic */
#define CODING_ISO8859_15 (0x000F) /* West European */

#define CODING_DVB6937    (0x1000) /* ISO6937: add 0xA4(U+20AC) */
#define CODING_DVB8859_1  (0x1001) /* FIXME */
#define CODING_DVB8859_2  (0x1002) /* FIXME */
#define CODING_DVB8859_3  (0x1003) /* FIXME */
#define CODING_DVB8859_4  (0x1004) /* FIXME */
#define CODING_DVB8859_5  (0x1005) /* ISO8859_5: 0xB9(U+0419 -> U+04E4), 0xD9(U+0439 -> U+04E5) */
#define CODING_DVB8859_6  (0x1006) /* ISO8859_6 */
#define CODING_DVB8859_7  (0x1007) /* ISO8859_7: omit 0xA4, 0xA5, 0xAA */
#define CODING_DVB8859_8  (0x1008) /* ISO8859_8: omit 0xFD, 0xFE */
#define CODING_DVB8859_9  (0x1009) /* ISO8859_9 */
#define CODING_DVB8859_10 (0x100A) /* ISO8859_10 */
#define CODING_DVB8859_11 (0x100B) /* ISO8859_11 */
#define CODING_DVB8859_13 (0x100D) /* ISO8859_13 */
#define CODING_DVB8859_14 (0x100E) /* ISO8859_14 */
#define CODING_DVB8859_15 (0x100F) /* ISO8859_15: 0xA4(U+20AC -> U+00A4), 0xB1(U+00B1 -> U+20AC) */

#define CODING_UTF8     (0x0020) /* UTF8 */
#define CODING_UTF16BE  (0x0021) /* UTF16, big endian */
#define CODING_KSX1001  (0x0022) /* Korean Character Set */
#define CODING_GB2312   (0x0023) /* GB-2312-1980 */
#define CODING_BIG5     (0x0024) /* BIG5 subset of ISO10646 */
#define CODING_BBC      (0x0025) /* encoding_type_ID: BBC */
#define CODING_MTFSB    (0x0026) /* encoding_type_ID: Malaysian Technical Standard Forum Bhd */

#define CODING_RESERVED (0x0099) /* reserved */

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
