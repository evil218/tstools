/* vim: set tabstop=8 shiftwidth=8: */
//============================================================================
// Name: udp.c
// Purpose: UDP access
// To build: gcc -std-c99 -c udp.c
//============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __USE_GNU

#include "udp.h"

#define UDP_LENGTH_MAX                  1536
//============================================================================
// Sub-function declare:
//============================================================================

//============================================================================
// Functions definition:
//============================================================================
UDP *udp_open(char *addr, unsigned short port)
{
        UDP *udp;

        udp = (UDP *)malloc(sizeof(UDP));
        if(NULL == udp)
        {
                perror("malloc");
                return NULL;
        }

        // build socket
        if((udp->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
                perror("socket");
                return NULL;
        }

        // nonblock mode
        fcntl(udp->sock, F_SETFL, O_NONBLOCK);

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
                perror("bind");
                fprintf(stderr, "    addr: %s\n", inet_ntoa(local.sin_addr));
                fprintf(stderr, "    port: %d\n", ntohs(local.sin_port));
                return NULL;
        }

        // manage multicast
        struct ip_mreq imreq;
        imreq.imr_multiaddr.s_addr = inet_addr(addr);
        if(IN_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr)))
        {
                imreq.imr_interface.s_addr = htonl(INADDR_ANY);
                if(setsockopt(udp->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                              (char *)&imreq, sizeof(imreq)) < 0)
                {
                        perror("join multicast membership");
                }
        }

        return udp;
}

void udp_close(UDP *udp, char *addr)
{
        struct ip_mreq imreq;

        // manage multicast
        imreq.imr_multiaddr.s_addr = inet_addr(addr);
        if(IN_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr)))
        {
                imreq.imr_interface.s_addr = htonl(INADDR_ANY);
                if(setsockopt(udp->sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                              (char *)&imreq, sizeof(imreq)) < 0)
                {
                        perror("quit multicast membership");
                }
        }

        close(udp->sock);
}

size_t udp_read(UDP *udp, char *buf)
{
        size_t rslt = 0;
        fd_set fds;
        socklen_t socklen = sizeof(struct sockaddr_in);

        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(udp->sock, &fds);
        if(select(udp->sock + 1, &fds, NULL, NULL, NULL) < 0)
        {
                perror("select");
        }
        else if(FD_ISSET(udp->sock, &fds))
        {
                rslt = recvfrom(udp->sock, buf, UDP_LENGTH_MAX, 0,
                                (struct sockaddr *)&udp->remote,
                                &socklen);
        }
        else if(FD_ISSET(0, &fds))
        {
                //printf("Key pressed.\n");
        }
        else
        {
                fprintf(stderr, "%s:%d: bad branches\n", __FILE__, __LINE__);
        }

        return rslt;
}

/*****************************************************************************
 * End
 ****************************************************************************/
