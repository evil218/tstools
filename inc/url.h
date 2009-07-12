//=============================================================================
// Name: url.h
// Purpose: URL access
// To build: gcc -std-c99 -c url.c
//=============================================================================

#ifndef _URL_H
#define _URL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "winsock.h"

#define MAX_STRING_LENGTH 256

/*============================================================================
 * Struct Declaration
 ===========================================================================*/
enum
{
        PRTCL_UDP,
        PRTCL_FILE
};

typedef struct
{
        char url[MAX_STRING_LENGTH];
        int  protocol; // PRTCL_XXX

        char *path;
        char *filename;
        FILE *fd;

        char *ip;
        unsigned short port;
        int  sock;
        struct sockaddr_in remote;
}
URL;

/*============================================================================
 * Public Function Declaration
 ===========================================================================*/
URL *url_open(const char *str, char *mode);
int url_close(URL *url);
int url_seek(URL *url, long offset, int origin);
int url_getc(URL *url);
size_t url_read(void *buf, size_t size, size_t nobj, URL *url);

#ifdef __cplusplus
}
#endif

#endif // _URL_H

/*****************************************************************************
 * End
 ****************************************************************************/
