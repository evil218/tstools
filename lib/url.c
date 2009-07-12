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

        // for sock
        WORD wVersionRequested;
        WSADATA wsaData;
        int length;
        unsigned long opt;
        struct sockaddr_in local;
        int err;
        int rdata;

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
                        // build socket
                        wVersionRequested = MAKEWORD(1,1);
                        err = WSAStartup(wVersionRequested, &wsaData);
                        if(err != 0)
                        {
                                exit(0);
                        }
                        url->sock = socket(AF_INET, SOCK_DGRAM, 0);
                        err = GetLastError();
                        if(url->sock < 0)
                        {
                                perror("open a datagram socket error!");
                                exit(0);
                        }
                        // nonblock mode
                        opt = 1;
                        //ioctlsocket(url->sock, FIONBIO, &opt);
                        // name the socket
                        local.sin_family = AF_INET;
                        local.sin_addr.s_addr = htonl(INADDR_ANY);
                        local.sin_port = htons(url->port);
                        printf("bind: port %d %d\n", url->port, ntohs(local.sin_port));
                        rdata = bind(url->sock, (struct sockaddr *)&local, sizeof(struct sockaddr));
                        err = GetLastError();
                        if(rdata < 0)
                        {
                                perror("initiate a connection to the socket error!");
                                exit(0);
                        }
                        // find the port and print it
                        length = sizeof(local);
                        rdata = getsockname(url->sock, (struct sockaddr *)&local, &length);
                        err = GetLastError();
                        if(rdata < 0)
                        {
                                perror("getting socket name error!");
                                exit(0);
                        }
#if 0
                        url->remote.sin_family = AF_INET;
                        url->remote.sin_addr.s_addr = inet_addr(url->ip);
                        url->remote.sin_port = 0;
#endif
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
                        closesocket(url->sock);
                        WSACleanup();
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
        int rslt;

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
        size_t rslt;
        int length;

        if(NULL == url)
        {
                printf("Bad parameter!\n");
                return -1;
        }

        switch(url->protocol)
        {
                case PRTCL_UDP:
                        printf("need %d-byte\n", nobj);
                        length = sizeof(url->remote);
                        rslt = recvfrom(url->sock, buf, nobj, 0,
                                        (struct sockaddr *)&(url->remote), &length);
                        //printf("wait: ip %s\n", inet_ntoa(url->remote.sin_addr.s_addr));
                        printf("port: %d\n", ntohs(url->remote.sin_port));
                        printf("got %d-byte\n", rslt);
                        break;
                default: // PRTCL_FILE
                        rslt = fread(buf, size, nobj, url->fd);
                        break;
        }

        return rslt;
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
