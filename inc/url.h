/* vim: set tabstop=8 shiftwidth=8: */
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

#include "udp.h"

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

        // for PRTCL_FILE
        char *path;
        char *filename;
        FILE *fd;

        // for PRTCL_UDP
        char *ip;
        unsigned short port;
        UDP *udp;

        char buf[8*188]; // for UDP data
        char *pbuf;
        size_t ts_cnt;
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
