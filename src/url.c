/* vim: set tabstop=8 shiftwidth=8:
 * name: url.c
 * funx: URL access
 * 2009-00-00, ZHOU Cheng, init
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> /* for tolower() */

#include "common.h"
#include "url.h"

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

static int parse_url(struct url *url, const char *str);

struct url *url_open(const char *str, char *mode)
{
        struct url *url;

        url = (struct url *)malloc(sizeof(struct url));
        if(NULL == url) {
                RPT(RPT_ERR, "malloc 'struct url' failed");
                exit(-1);
        }
        url->url[0] = '\0';
        url->host = NULL;
        url->port = 0;
        url->path_fname = NULL;

        if(0 != parse_url(url, str)) {
                exit(0);
        }

        switch(url->scheme) {
                case SCH_UDP:
                        url->pbuf = url->buf;
                        url->ts_cnt = 0;
                        url->udp = udp_open(url->host, url->port, mode);
                        if(0 == url->udp) {
                                printf("Socket error!\n");
                                free(url);
                                url = NULL;
                        }
                        break;
                default: /* SCH_FILE */
                        url->fd = fopen(url->path_fname, mode);
                        if(NULL == url->fd) {
                                printf("Can not open file \"%s\"!\n", url->path_fname);
                                free(url);
                                url = NULL;
                        }
                        break;
        }

        return url;
}

int url_close(struct url *url)
{
        if(NULL == url) {
                printf("Bad parameter!\n");
                return -1;
        }

        switch(url->scheme) {
                case SCH_UDP:
                        udp_close(url->udp);
                        break;
                default: /* SCH_FILE */
                        fclose(url->fd);
                        break;
        }

        free(url);
        return 0;
}

int url_seek(struct url *url, long offset, int origin)
{
        int rslt = 0;

        if(NULL == url) {
                printf("Bad parameter!\n");
                return -1;
        }

        switch(url->scheme) {
                case SCH_UDP:
                        /* do nothing! */
                        break;
                default: /* SCH_FILE */
                        rslt = fseek(url->fd, offset, origin);
                        break;
        }

        return rslt;
}

int url_getc(struct url *url)
{
        int rslt;

        if(NULL == url) {
                printf("Bad parameter!\n");
                return -1;
        }

        switch(url->scheme) {
                case SCH_UDP:
                        rslt = 0x47; /* to cheat host */
                        break;
                default: /* SCH_FILE */
                        rslt = fgetc(url->fd);
                        break;
        }

        return rslt;
}

size_t url_read(void *buf, size_t size, size_t nobj, struct url *url)
{
        size_t cobj; /* the number of objects succeeded in reading */
        size_t byte_needed = size * nobj;

        if(NULL == url) {
                printf("Bad parameter!\n");
                return -1;
        }

        switch(url->scheme) {
                case SCH_UDP:
                        if(url->ts_cnt == 0) {
                                size_t rslt;

                                rslt = udp_read(url->udp, url->buf);
                                RPT(RPT_INF, "read %d-byte", rslt);
                                if(rslt > 0) {
                                        url->ts_cnt += rslt;
                                        url->pbuf = url->buf;
                                }
                        }
                        if(url->ts_cnt >= byte_needed) {
                                memcpy(buf, url->pbuf, byte_needed);
                                url->pbuf += byte_needed;
                                url->ts_cnt -= byte_needed;
                                cobj = nobj;
                        }
                        else {
                                memcpy(buf, url->pbuf, url->ts_cnt);
                                url->pbuf += url->ts_cnt;
                                url->ts_cnt -= url->ts_cnt;
                                cobj = url->ts_cnt;
                        }
                        break;
                default: /* SCH_FILE */
                        cobj = fread(buf, size, nobj, url->fd);
                        break;
        }

        return cobj;
}

size_t url_write(const void *buf, size_t size, size_t nobj, struct url *url)
{
        return udp_write(url->udp, buf, size * nobj);
}

#define RFC1738 "[<scheme>://[[<user>[:<password>]@]<host>[:<port>]]][[/<disk>:]*[/<dir>]/<fname>]"
static int parse_url(struct url *url, const char *str)
{
        char *ps; /* source */
        char *pd; /* destination */
        char *rslt = NULL;
        char pattern[MAX_STRING_LENGTH] = "";

        strncpy(url->url, str, MAX_STRING_LENGTH);

        /* pattern */
        for(ps = url->url, pd = pattern; *ps; ps++) {
                if(strchr(":/@", *ps)) {
                        if('*' == *pd) {
                                pd++;
                        }
                        *pd++ = *ps;
                }
                else {
                        *pd = '*';
                }
        }
        RPT(RPT_DBG, "pattern: %s", pattern);

        /* get scheme */
        if(0 == memcmp(pattern, "*://", 4)) {
                strtok(url->url, ":");

                /* lower case */
                for(ps = url->url; *ps; ps++) {
                        *ps = (char)tolower((int)*ps);
                }

                RPT(RPT_DBG, "scheme: %s", url->url);
        }
        else {
                RPT(RPT_DBG, "scheme: file");
                url->scheme = SCH_LFILE;
        }

        /* UDP scheme */
        if(0 == strcmp(url->url, "udp")) {
                url->scheme = SCH_UDP;

                if(0 == memcmp(pattern, "*://*:*", 7)) { /* udp://host:port */
                        rslt = strtok(NULL, ":");
                        url->host = rslt + 2; /* add 2 to pass "//" */
                        RPT(RPT_DBG, "host: %s", url->host);

                        rslt = strtok(NULL, "/");
                        url->port = atoi(rslt);
                        RPT(RPT_DBG, "port: %d", url->port);
                }
                else if(0 == memcmp(pattern, "*://@*:*", 8)) { /* udp://@host:port, VLC only */
                        rslt = strtok(NULL, "@");
                        RPT(RPT_DBG, "prefix: %s", rslt);

                        rslt = strtok(NULL, ":");
                        url->host = rslt;
                        RPT(RPT_DBG, "host: %s", url->host);

                        rslt = strtok(NULL, "/");
                        url->port = atoi(rslt);
                        RPT(RPT_DBG, "port: %d", url->port);
                }
                else if(0 == memcmp(pattern, "*://:*", 6) ||  /* udp://:port */
                        0 == memcmp(pattern, "*://@:*", 7)) { /* udp://@:port, VLC only */
                        rslt = strtok(NULL, ":");
                        RPT(RPT_DBG, "prefix: %s", rslt);
                        url->host = "127.0.0.1";
                        RPT(RPT_DBG, "host: %s", url->host);

                        rslt = strtok(NULL, "/");
                        url->port = atoi(rslt);
                        RPT(RPT_DBG, "port: %d", url->port);
                }
                else {
                        fprintf(stderr, "URL syntax error for UDP scheme!\n");
                        fprintf(stderr, "    " RFC1738 "\n");
                        return -1;
                }
        }
        else if(0 == strcmp(url->url, "file")) {
                /* file:///.../stream.ts */
                /* file:///E:/.../stream.ts */
                url->scheme = SCH_FILE;
                url->path_fname = url->url;
        }

        return 0;
}
