/*
 * vim: set tabstop=8 shiftwidth=8:
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

/* report micro */
#define RPT_EMERG       (1) // 紧急：系统崩溃前最后一句话
#define RPT_ALERT       (2) // 报告：必须立即采取措施
#define RPT_CRIT        (3) // 临界：严重的硬件或软件操作失败
#define RPT_ERR         (4) // 错误：硬件错误
#define RPT_WARNING     (5) // 警告：可能出现问题的情况
#define RPT_NOTICE      (6) // 提醒：正常但又重要的条件
#define RPT_INFO        (7) // 提示：驱动程序启动、硬件信息
#define RPT_DBG         (8) // 调试：运行中的各个步骤

#define rpt(level, ...) \
        do { \
                if (level <= RPT_LEVEL) { \
                        fprintf(stderr, "\"%s\", line %d: ",__FILE__, __LINE__); \
                        fprintf(stderr, __VA_ARGS__); \
                        fprintf(stderr, "\n"); \
                } \
        } while (0)

#ifdef __cplusplus
}
#endif

#endif /* _COMMON_H */
