/* vim: set tabstop=8 shiftwidth=8:
 * name: common.h
 * funx: common definition
 */

#ifndef _COMMON_H
#define _COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#define LINE_LENGTH_MAX                 (32767) /* max line length in txt file */
#define TSTOOLS_VERSION                 "1.0.0"

/* ANSI control for printf */
#define NONE                            "\033[00m" /* recover all */
#define LIGHT                           "\033[01m" /* high light colour */

#define FGRAY                           "\033[30m"
#define FRED                            "\033[31m"
#define FGREEN                          "\033[32m"
#define FYELLOW                         "\033[33m"
#define FBLUE                           "\033[34m"
#define FPURPLE                         "\033[35m"
#define FCYAN                           "\033[36m"
#define FWHITE                          "\033[37m"

#define BGRAY                           "\033[40m"
#define BRED                            "\033[41m"
#define BGREEN                          "\033[42m"
#define BYELLOW                         "\033[43m"
#define BBLUE                           "\033[44m"
#define BPURPLE                         "\033[45m"
#define BCYAN                           "\033[46m"
#define BWHITE                          "\033[47m"

#define CLRSCR                          "\033[2J" /* clear screan */
#define CLRLIN                          "\033[K" /* clear to line end */

/* report level */
#define RPT_ERR         (1) // error, system error
#define RPT_WRN         (2) // warning, maybe wrong, maybe OK
#define RPT_INF         (3) // important information
#define RPT_DBG         (4) // debug information

/* report micro */
#define RPT(lvl, ...) \
        do { \
                if (lvl <= RPT_LVL) { \
                        fprintf(stderr, "\"%s\", line %d: ",__FILE__, __LINE__); \
                        fprintf(stderr, __VA_ARGS__); \
                        fprintf(stderr, "\n"); \
                } \
        } while (0)

#ifdef __cplusplus
}
#endif

#endif /* _COMMON_H */
