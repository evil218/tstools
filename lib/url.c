//============================================================================
// Name: url.c
// Purpose: URL access
// To build: gcc -std-c99 -c url.c
//============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "url.h"

#define UDP_LENGTH_MAX                  1536
//============================================================================
// Sub-function declare:
//============================================================================
static int udp_open(char *addr, unsigned short port);
static void udp_close(int sock, char *addr);
static size_t udp_read(URL *url);
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
                        // init sockaddr_in length
                        url->sockaddr_in_len = sizeof(url->remote);

                        url->sock = udp_open(url->ip, url->port);
                        if(SOCKET_ERROR == url->sock)
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
                        udp_close(url->sock, url->ip);
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
                                rslt = udp_read(url);
                                //printf("rslt of udp_read() is %d\n", rslt);
                                if(rslt > 0)
                                {
                                        url->ts_cnt += rslt;
                                        url->pbuf = url->buf;
                                }
                        }
                        if(url->ts_cnt >= nobj)
                        {
                                memcpy(buf, url->pbuf, nobj);
                                url->pbuf += nobj;
                                url->ts_cnt -= nobj;
                                rslt = nobj;
                        }
                        else
                        {
                                memcpy(buf, url->pbuf, url->ts_cnt);
                                url->pbuf += url->ts_cnt;
                                url->ts_cnt -= url->ts_cnt;
                                rslt = url->ts_cnt;
                        }
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
static int udp_open(char *addr, unsigned short port)
{
        // build socket
#ifdef MINGW32
        WSADATA wsaData;
        if(WSAStartup(MAKEWORD(1,1), &wsaData) == SOCKET_ERROR)
        {
                printf("WSAStartup error!\n");
                return -1;
        }
#endif

        int sock;
        if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
                printf("open a datagram socket error!\n");
                return sock;
        }

        // nonblock mode
        unsigned long opt = 1;
        ioctlsocket(sock, FIONBIO, &opt);

        // name the socket
        struct sockaddr_in local;
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(port);
        if(bind(sock, (struct sockaddr *)&local, sizeof(struct sockaddr)) < 0)
        {
                //printf("addr: %s\n", inet_ntoa(local.sin_addr));
                //printf("port: %d\n", ntohs(local.sin_port));
                printf("initiate a connection to the socket error!\n");
                return sock;
        }

        // manage multicast
        struct ip_mreq multicast;
        multicast.imr_multiaddr.s_addr = inet_addr(addr);
        if(0xE0 == (multicast.imr_multiaddr.s_net & 0xF0))
        {
                multicast.imr_interface.s_addr = htonl(INADDR_ANY);
                if(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                              (char *)&multicast, sizeof(multicast)) < 0)
                {
                        printf("join multicast membership error!\n");
                }
        }

        return sock;
}

static void udp_close(int sock, char *addr)
{
        struct ip_mreq multicast;

        // manage multicast
        multicast.imr_multiaddr.s_addr = inet_addr(addr);
        if(0xE0 == (multicast.imr_multiaddr.s_net & 0xF0))
        {
                multicast.imr_interface.s_addr = htonl(INADDR_ANY);
                if(setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                              (char *)&multicast, sizeof(multicast)) < 0)
                {
                        printf("quit multicast membership error!\n");
                }
        }

#ifdef MINGW32
        closesocket(sock);
        WSACleanup();
#endif
}

static size_t udp_read(URL *url)
{
        size_t rslt;
        fd_set fds;

        FD_ZERO(&fds);
        // FD_SET(0, &fds); // must be socket handle
        FD_SET(url->sock, &fds);
        if(select(url->sock + 1, &fds, NULL, NULL, NULL) == SOCKET_ERROR)
        {
                int err = WSAGetLastError();

                printf("Select() return SOCKET_ERROR.\n");
                if(WSAENOTSOCK == err)
                {
                        printf("Nonsocket item.\n");
                }
                rslt = 0;
        }
        else if(FD_ISSET(url->sock, &fds))
        {
                rslt = recvfrom(url->sock, url->buf, UDP_LENGTH_MAX, 0,
                                (struct sockaddr *)&(url->remote),
                                &(url->sockaddr_in_len));
                //printf("Recvfrom() got %d-byte.\n", rslt);
        }
        else if(FD_ISSET(0, &fds))
        {
                printf("Key pressed.\n");
                rslt = 0;
        }
        else
        {
                // never reached code
        }

        return rslt;
}

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
