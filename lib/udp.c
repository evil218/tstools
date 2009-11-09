/* vim: set tabstop=8 shiftwidth=8: */
//============================================================================
// Name: udp.c
// Purpose: UDP access
// To build: gcc -std-c99 -c udp.c
//============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
                printf("Alloc %d-byte failed!\n", sizeof(UDP));
                return NULL;
        }

        // init sockaddr_in length
        udp->sockaddr_in_len = sizeof(udp->remote);

        // build socket
        if((udp->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
                printf("open a datagram socket error!\n");
                return NULL;
        }

        // nonblock mode
        //unsigned long opt = 1;
        //ioctlsocket(udp->sock, FIONBIO, &opt);

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
                printf("initiate a connection to the socket error!\n");
                //printf("addr: %s\n", inet_ntoa(local.sin_addr));
                //printf("port: %d\n", ntohs(local.sin_port));
                return NULL;
        }

        // manage multicast
        struct ip_mreq multicast;
        multicast.imr_multiaddr.s_addr = inet_addr(addr);
        //printf("s_addr is 0x%08X\n", (int)(multicast.imr_multiaddr.s_addr));
        if(0x000000E0 == (multicast.imr_multiaddr.s_addr & 0x000000F0))
        {
                multicast.imr_interface.s_addr = htonl(INADDR_ANY);
                if(setsockopt(udp->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                              (char *)&multicast, sizeof(multicast)) < 0)
                {
                        printf("join multicast membership error!\n");
                }
        }

        return udp;
}

void udp_close(UDP *udp, char *addr)
{
        struct ip_mreq multicast;

        // manage multicast
        multicast.imr_multiaddr.s_addr = inet_addr(addr);
        if(0x000000E0 == (multicast.imr_multiaddr.s_addr & 0x000000F0))
        {
                multicast.imr_interface.s_addr = htonl(INADDR_ANY);
                if(setsockopt(udp->sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                              (char *)&multicast, sizeof(multicast)) < 0)
                {
                        printf("quit multicast membership error!\n");
                }
        }

        close(udp->sock);
}

size_t udp_read(UDP *udp, char *buf)
{
        size_t rslt = 0;
        fd_set fds;

        FD_ZERO(&fds);
        FD_SET(0, &fds); // must be socket handle
        FD_SET(udp->sock, &fds);
        if(select(udp->sock + 1, &fds, NULL, NULL, NULL) < 0)
        {
                printf("select() returns error.\n");
        }
        else if(FD_ISSET(udp->sock, &fds))
        {
                rslt = recvfrom(udp->sock, buf, UDP_LENGTH_MAX, 0,
                                (struct sockaddr *)&udp->remote,
                                &udp->sockaddr_in_len);
                //printf("Recvfrom() got %d-byte.\n", rslt);
        }
        else if(FD_ISSET(0, &fds))
        {
                printf("Key pressed.\n");
        }
        else
        {
                // never reached code
        }

        return rslt;
}

/*****************************************************************************
 * End
 ****************************************************************************/
