/* vim: set tabstop=8 shiftwidth=8:
 * name: url.h
 * funx: URL access
 * 2009-00-00, ZHOU Cheng, init
 */

#ifndef _URL_H
#define _URL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h> /* for uint?_t, etc */

#include "udp.h"

#define MAX_STRING_LENGTH 256

enum scheme {
        SCH_UDP,  /* udp://... */
        SCH_FILE, /* file://... */
        SCH_LFILE /* local file, without "file://" scheme prefix */
};

struct url {
        char url[MAX_STRING_LENGTH];

        /* part of URL */
        int  scheme; /* SCH_XXX */
        char *user;
        char *password;
        char *host;
        uint16_t port;
        char *disk;
        char *path_fname;

        /* id */
        FILE *fd;
        intptr_t udp;

        /* data buffer */
        char buf[8*188]; /* for UDP data */
        char *pbuf;
        size_t ts_cnt;
};

struct url *url_open(const char *str, char *mode);
int url_close(struct url *url);
int url_seek(struct url *url, long offset, int origin);
int url_getc(struct url *url);
size_t url_read(void *buf, size_t size, size_t nobj, struct url *url);

#ifdef __cplusplus
}
#endif

#endif /* _URL_H */
