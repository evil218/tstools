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

/* report level and macro */
#define ERR_LVL (1) /* error, system error */
#define WRN_LVL (2) /* warning, maybe wrong, maybe OK */
#define INF_LVL (3) /* important information */
#define DBG_LVL (4) /* debug information */

#define RPTERR(fmt...) do {if(ERR_LVL <= rpt_lvl) {fprintf(stderr, "%s: %d: err: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0)
#define RPTWRN(fmt...) do {if(WRN_LVL <= rpt_lvl) {fprintf(stderr, "%s: %d: wrn: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0)
#define RPTINF(fmt...) do {if(INF_LVL <= rpt_lvl) {fprintf(stderr, "%s: %d: inf: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0)
#define RPTDBG(fmt...) do {if(DBG_LVL <= rpt_lvl) {fprintf(stderr, "%s: %d: dbg: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0)

#ifdef __cplusplus
}
#endif

#endif /* _COMMON_H */
