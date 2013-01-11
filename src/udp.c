/* vim: set tabstop=8 shiftwidth=8:
 * name: udp.c
 * funx: UDP access
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_mingw
#       include <winsock.h>
#       define IN_CLASSD(a)    ((((long)(a)) & 0xF0000000) == 0xE0000000)
#       define IN_MULTICAST(a) IN_CLASSD(a)
typedef int socklen_t;
#else /* unix-like PLATFORM */
#       include <sys/types.h>
#       include <sys/socket.h>
#       include <netinet/in.h>
#       include <arpa/inet.h> /* for inet_ntoa(), inet_addr(), etc */
#       include <unistd.h> /* for close() */
#       include <fcntl.h> /* for fcntl(), O_NONBLOCK, etc */
#       include <sys/select.h> /* for select(), etc */
#       include <errno.h>
#       define __USE_GNU /* for 'struct ip_mreq' in CentOS x64 */
#endif

#include "common.h"
#include "udp.h"

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

#define UDP_LENGTH_MAX (1536)

struct udp {
        int sock;
        struct sockaddr_in remote;
        socklen_t socklen;

        char addr[32];
};

static int report(const char *str);

intptr_t udp_open(char *addr, unsigned short port, char *mode)
{
        struct udp *udp;

        udp = (struct udp *)malloc(sizeof(struct udp));
        if(NULL == udp) {
                RPT(RPT_ERR, "malloc failed");
                return (intptr_t)NULL;
        }

        strcpy(udp->addr, addr);

#ifdef PLATFORM_mingw
        WSADATA wsaData;
        if(WSAStartup(MAKEWORD(1,1), &wsaData) == SOCKET_ERROR)
        {
                RPT(RPT_ERR, "WSAStartup error");
                return (intptr_t)NULL;
        }
#endif

        /* build socket */
        if((udp->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                report("socket failed");
                return (intptr_t)NULL;
        }

        /* nonblock mode */
#ifdef PLATFORM_mingw
        {
                unsigned long opt = 1;
                ioctlsocket(udp->sock, FIONBIO, &opt);
        }
#else
        {
                fcntl(udp->sock, F_SETFL, O_NONBLOCK);
        }
#endif

        /* reuse address */
        {
                int reuseaddr = 1; /* nonzero means enable */

                setsockopt(udp->sock, SOL_SOCKET, SO_REUSEADDR,
                           (char *)&reuseaddr, sizeof(int));
        }

        /* name the socket */
        {
                struct sockaddr_in local;

                memset(&local, 0, sizeof(local)); /* for some special compile environment */
                local.sin_family = AF_INET;
                local.sin_addr.s_addr = htonl(INADDR_ANY);
                local.sin_port = ('r' == mode[0]) ? htons(port) : 0;

                if(bind(udp->sock, (struct sockaddr *)&local, sizeof(struct sockaddr)) < 0) {
                        report("bind failed");
                        fprintf(stderr, "addr: %s, port: %d\n",
                            inet_ntoa(local.sin_addr),
                            ntohs(local.sin_port));
                        return (intptr_t)NULL;
                }
        }

        /* set the remote */
        if('w' == mode[0])
        {
                udp->remote.sin_family = AF_INET;
                udp->remote.sin_addr.s_addr = inet_addr(addr);
                udp->remote.sin_port = htons(port);
        }

        /* manage multicast */
        {
                struct ip_mreq imreq;

                imreq.imr_multiaddr.s_addr = inet_addr(addr);
                if(IN_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
                        imreq.imr_interface.s_addr = htonl(INADDR_ANY);
                        if(setsockopt(udp->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                                      (char *)&imreq, sizeof(imreq)) < 0) {
                                report("join multicast membership failed");
                        }
                }
        }

        udp->socklen = sizeof(struct sockaddr_in);
        return (intptr_t)udp;
}

int udp_close(intptr_t id)
{
        struct udp *udp = (struct udp *)id;

        if(NULL == udp) {
                RPT(RPT_ERR, "bad id");
                return -1;
        }

        /* manage multicast */
        {
                struct ip_mreq imreq;

                imreq.imr_multiaddr.s_addr = inet_addr(udp->addr);
                if(IN_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
                        imreq.imr_interface.s_addr = htonl(INADDR_ANY);
                        if(setsockopt(udp->sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                                      (char *)&imreq, sizeof(imreq)) < 0) {
                                report("quit multicast membership failed");
                        }
                }
        }

        /* close socket*/
#ifdef PLATFORM_mingw
        closesocket(udp->sock);
        WSACleanup();
#else
        close(udp->sock);
#endif

        free(udp);
        return 0;
}

size_t udp_read(intptr_t id, void *buf)
{
        struct udp *udp = (struct udp *)id;

        if(NULL == udp) {
                RPT(RPT_ERR, "bad id");
                return -1;
        }

        size_t rslt = 0;
        fd_set fds;

        FD_ZERO(&fds);
        FD_SET(udp->sock, &fds);
        if(select(udp->sock + 1, &fds, NULL, NULL, NULL) < 0) {
                report("select failed");
                return 0;
        }

        if(FD_ISSET(udp->sock, &fds)) {
                rslt = recvfrom(udp->sock, buf, UDP_LENGTH_MAX, 0,
                                (struct sockaddr *)&(udp->remote),
                                &(udp->socklen));
        }

        return rslt;
}

size_t udp_write(intptr_t id, const void *buf, int len)
{
        struct udp *udp = (struct udp *)id;

        if(NULL == udp) {
                RPT(RPT_ERR, "bad id");
                return -1;
        }

        size_t rslt = 0;
        rslt = sendto(udp->sock, buf, len, 0,
                        (struct sockaddr *)&(udp->remote),
                        udp->socklen);

        return rslt;
}

static int report(const char *str)
{
        int err;

#ifdef PLATFORM_mingw
        err = WSAGetLastError();
#else
        err = errno;
#endif
        RPT(RPT_ERR, "%s, errno: %d", str, err);
        return 0;
}
