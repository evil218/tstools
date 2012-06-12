/* vim: set tabstop=8 shiftwidth=8:
 * name: udp.c
 * funx: UDP access
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PLATFORM_mingw

#define __USE_GNU /* for 'struct ip_mreq' in CentOS x64 */

# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>                 /* for inet_ntoa(), inet_addr(), etc */
# include <unistd.h>                    /* for close() */
# include <fcntl.h>                     /* for fcntl(), O_NONBLOCK, etc */
# include <sys/select.h>                /* for select(), etc */
#endif

#ifdef PLATFORM_mingw
# include <winsock.h>
# include <sys/types.h>
# include <sys/timeb.h>
#endif

#include "common.h"
#include "udp.h"

#define RPT_LVL         RPT_WRN /* report level: ERR, WRN, INF, DBG */

#define UDP_LENGTH_MAX                  1536

struct udp {
        int sock;
        char addr[32];
        unsigned short port;
};

intptr_t udp_open(char *addr, unsigned short port)
{
        struct udp *udp;

        udp = (struct udp *)malloc(sizeof(struct udp));
        if(NULL == udp) {
                RPT(RPT_ERR, "malloc failed");
                return (intptr_t)NULL;
        }

        strcpy(udp->addr, addr);
        udp->port = port;

#ifndef PLATFORM_mingw
        /* build socket */
        if((udp->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                perror("socket");
                return (intptr_t)NULL;
        }

        /* nonblock mode */
        fcntl(udp->sock, F_SETFL, O_NONBLOCK);

        /* reuse address */
        {
                int reuseaddr = 1; /* nonzero means enable */

                setsockopt(udp->sock, SOL_SOCKET, SO_REUSEADDR,
                           (char *)&reuseaddr, sizeof(int));
        }

        /* name the socket */
        {
                struct sockaddr_in local;

                local.sin_family = AF_INET;
                local.sin_addr.s_addr = htonl(INADDR_ANY);
                local.sin_port = htons(udp->port);

                if(bind(udp->sock, (struct sockaddr *)&local, sizeof(struct sockaddr)) < 0) {
                        perror("bind");
                        fprintf(stderr, "    addr: %s\n", inet_ntoa(local.sin_addr));
                        fprintf(stderr, "    port: %d\n", ntohs(local.sin_port));
                        return (intptr_t)NULL;
                }
        }

        /* manage multicast */
        {
                struct ip_mreq imreq;

                imreq.imr_multiaddr.s_addr = inet_addr(udp->addr);
                if(IN_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
                        imreq.imr_interface.s_addr = htonl(INADDR_ANY);
                        if(setsockopt(udp->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                                      (char *)&imreq, sizeof(imreq)) < 0) {
                                perror("join multicast membership");
                        }
                }
        }
#endif

#ifdef PLATFORM_mingw
        WSADATA wsaData;
        if(WSAStartup(MAKEWORD(1,1), &wsaData) == SOCKET_ERROR)
        {
                printf("WSAStartup error!\n");
                return (intptr_t)NULL;
        }

        // build socket
        if((udp->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
                int err = WSAGetLastError();
                printf("open a datagram socket error: %d!\n", err);
                return (intptr_t)NULL;
        }

        // nonblock mode
        unsigned long opt = 1;
        ioctlsocket(udp->sock, FIONBIO, &opt);

        // reuse address
        int reuseaddr = 1; // nonzero means enable
        setsockopt(udp->sock, SOL_SOCKET, SO_REUSEADDR,
                   (char *)&reuseaddr, sizeof(int));

        // name the socket
        struct sockaddr_in local;
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(port);

        if(bind(udp->sock, (struct sockaddr *)&local, sizeof(struct sockaddr)) < 0)
        {
                int err = WSAGetLastError();
                printf("initiate a connection to the socket error: %d!\n", err);
                //printf("addr: %s\n", inet_ntoa(local.sin_addr));
                //printf("port: %d\n", ntohs(local.sin_port));
                return (intptr_t)NULL;
        }

        // manage multicast
        struct ip_mreq multicast;
        multicast.imr_multiaddr.s_addr = inet_addr(addr);
        if(0xE0 == (multicast.imr_multiaddr.s_net & 0xF0))
        {
                multicast.imr_interface.s_addr = htonl(INADDR_ANY);
                if(setsockopt(udp->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                              (char *)&multicast, sizeof(multicast)) < 0)
                {
                        int err = WSAGetLastError();
                        printf("join multicast membership error: %d!\n", err);
                }
        }
#endif

        return (intptr_t)udp;
}

int udp_close(intptr_t id)
{
        struct udp *udp = (struct udp *)id;

        if(NULL == udp) {
                RPT(RPT_ERR, "bad id");
                return -1;
        }

#ifndef PLATFORM_mingw
        /* manage multicast */
        {
                struct ip_mreq imreq;

                imreq.imr_multiaddr.s_addr = inet_addr(udp->addr);
                if(IN_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
                        imreq.imr_interface.s_addr = htonl(INADDR_ANY);
                        if(setsockopt(udp->sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                                      (char *)&imreq, sizeof(imreq)) < 0) {
                                perror("quit multicast membership");
                        }
                }
        }

        close(udp->sock);
#endif

#ifdef PLATFORM_mingw
        /* manage multicast */
        {
                struct ip_mreq multicast;

                multicast.imr_multiaddr.s_addr = inet_addr(udp->addr);
                if(0xE0 == (multicast.imr_multiaddr.s_net & 0xF0))
                {
                        multicast.imr_interface.s_addr = htonl(INADDR_ANY);
                        if(setsockopt(udp->sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                                      (char *)&multicast, sizeof(multicast)) < 0)
                        {
                                int err = WSAGetLastError();
                                printf("quit multicast membership error: %d!\n", err);
                        }
                }
        }

        closesocket(udp->sock);
        WSACleanup();
#endif
        return 0;
}

size_t udp_read(intptr_t id, char *buf)
{
        struct udp *udp = (struct udp *)id;

        if(NULL == udp) {
                RPT(RPT_ERR, "bad id");
                return -1;
        }

#ifndef PLATFORM_mingw
        size_t rslt = 0;
        fd_set fds;
        struct sockaddr_in remote;
        socklen_t socklen = sizeof(struct sockaddr_in);

        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(udp->sock, &fds);
        if(select(udp->sock + 1, &fds, NULL, NULL, NULL) < 0) {
                perror("select");
        }
        else if(FD_ISSET(udp->sock, &fds)) {
                rslt = recvfrom(udp->sock, buf, UDP_LENGTH_MAX, 0,
                                (struct sockaddr *)&remote,
                                &socklen);
        }
        else if(FD_ISSET(0, &fds)) {
                /*printf("Key pressed.\n"); */
        }
        else {
                fprintf(stderr, "%s:%d: bad branches\n", __FILE__, __LINE__);
        }
#endif

#ifdef PLATFORM_mingw
        size_t rslt = 0;
        fd_set fds;
        struct sockaddr_in remote;
        int socklen = sizeof(struct sockaddr_in);

        FD_ZERO(&fds);
        FD_SET(udp->sock, &fds);
        if(select(udp->sock + 1, &fds, NULL, NULL, NULL) == SOCKET_ERROR)
        {
                printf("Select() return SOCKET_ERROR.\n");
                int err = WSAGetLastError();
                printf("Error No.: %d.\n", err);
                rslt = 0;
        }
        else if(FD_ISSET(udp->sock, &fds))
        {
                rslt = recvfrom(udp->sock, buf, UDP_LENGTH_MAX, 0,
                                (struct sockaddr *)&remote,
                                &socklen);
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
#endif

        return rslt;
}
