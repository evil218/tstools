/* vim: set tabstop=8 shiftwidth=8: */
//============================================================================
// Name: url.c
// Purpose: URL access
// To build: gcc -std-c99 -c url.c
//============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "url.h"

//============================================================================
// Sub-function declare:
//============================================================================
static void regcpy(char *des, const char *src);
static void parse_url(URL *url, const char *str);

//============================================================================
// Functions definition:
//============================================================================
URL *url_open(const char *str, char *mode)
{
        URL *url;

        url = (URL *)malloc(sizeof(URL));
        if(NULL == url)
        {
                printf("Alloc %d-byte failed!\n", sizeof(URL));
                exit(-1);
        }
        url->url[0] = '\0';
        url->path = NULL;
        url->filename = NULL;
        url->ip = NULL;

        parse_url(url, str);

        switch(url->protocol)
        {
                case PRTCL_UDP:
                        url->pbuf = url->buf;
                        url->ts_cnt = 0;
                        url->udp = udp_open(url->ip, url->port);
                        if(0 == url->udp)
                        {
                                printf("Socket error!\n");
                                free(url);
                                url = NULL;
                        }
                        break;
                default: // PRTCL_FILE
                        url->fd = fopen(url->filename, mode);
                        if(NULL == url->fd)
                        {
                                printf("Can not open file \"%s\"!\n", url->filename);
                                free(url);
                                url = NULL;
                        }
                        break;
        }

        return url;
}

int url_close(URL *url)
{
        if(NULL == url)
        {
                printf("Bad parameter!\n");
                return -1;
        }

        switch(url->protocol)
        {
                case PRTCL_UDP:
                        udp_close(url->udp);
                        break;
                default: // PRTCL_FILE
                        fclose(url->fd);
                        break;
        }

        free(url);
        return 0;
}

int url_seek(URL *url, long offset, int origin)
{
        int rslt = 0;

        if(NULL == url)
        {
                printf("Bad parameter!\n");
                return -1;
        }

        switch(url->protocol)
        {
                case PRTCL_UDP:
                        // do nothing!
                        break;
                default: // PRTCL_FILE
                        rslt = fseek(url->fd, offset, origin);
                        break;
        }

        return rslt;
}

int url_getc(URL *url)
{
        int rslt;

        if(NULL == url)
        {
                printf("Bad parameter!\n");
                return -1;
        }

        switch(url->protocol)
        {
                case PRTCL_UDP:
                        rslt = 0x47; // to cheat host
                        break;
                default: // PRTCL_FILE
                        rslt = fgetc(url->fd);
                        break;
        }

        return rslt;
}

size_t url_read(void *buf, size_t size, size_t nobj, URL *url)
{
        size_t cobj; // the number of objects succeeded in reading
        size_t byte_needed = size * nobj;

        if(NULL == url)
        {
                printf("Bad parameter!\n");
                return -1;
        }

        switch(url->protocol)
        {
                case PRTCL_UDP:
                        if(url->ts_cnt == 0)
                        {
                                size_t rslt;

                                rslt = udp_read(url->udp, url->buf);
                                //printf("rslt of udp_read() is %d\n", rslt);
                                if(rslt > 0)
                                {
                                        url->ts_cnt += rslt;
                                        url->pbuf = url->buf;
                                }
                        }
                        if(url->ts_cnt >= byte_needed)
                        {
                                memcpy(buf, url->pbuf, byte_needed);
                                url->pbuf += byte_needed;
                                url->ts_cnt -= byte_needed;
                                cobj = nobj;
                        }
                        else
                        {
                                memcpy(buf, url->pbuf, url->ts_cnt);
                                url->pbuf += url->ts_cnt;
                                url->ts_cnt -= url->ts_cnt;
                                cobj = url->ts_cnt;
                        }
                        break;
                default: // PRTCL_FILE
                        cobj = fread(buf, size, nobj, url->fd);
                        break;
        }

        return cobj;
}

//============================================================================
// Subfunctions definition:
//============================================================================
static void regcpy(char *des, const char *src)
{
        char ch;

        do
        {
                ch = *src++;
                if(' ' != ch)
                {
                        *des++ = ch;
                }
        }while('\0' != ch);
}

static void parse_url(URL *url, const char *str)
{
        int i;

        regcpy(url->url, str);
        if(0 == memcmp(url->url, "udp://", 6))
        {
                i = 7;
                url->ip = url->url + i;
                while(':' != *(url->url + i)) {i++;}
                *(url->url + i++) = '\0';
                url->port = atoi(url->url + i);
                url->protocol = PRTCL_UDP;
        }
        else
        {
                url->filename = url->url;
                url->protocol = PRTCL_FILE;
        }
}

/*****************************************************************************
 * End
 ****************************************************************************/
