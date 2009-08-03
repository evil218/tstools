/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: udp.h
// Purpose: UDP access
// To build: gcc -std-c99 -c udp.c
//=============================================================================

#ifndef _UDP_H
#define _UDP_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MINGW32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

/*============================================================================
 * Struct Declaration
 ===========================================================================*/
typedef struct
{
        int sock;
        int sockaddr_in_len;
        struct sockaddr_in remote;
}
UDP;

/*============================================================================
 * Public Function Declaration
 ===========================================================================*/
UDP *udp_open(char *addr, unsigned short port);
void udp_close(UDP *udp, char *addr);
size_t udp_read(UDP *udp, char *buf);

#ifdef __cplusplus
}
#endif

#endif // _UDP_H

/*****************************************************************************
 * End
 ****************************************************************************/
