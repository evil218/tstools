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
        int  protocol; // PRTCL_XXX
        char *path;
        char *filename;
        char *ip;
        char *port;
        FILE *fd;
}
URL;

/*============================================================================
 * Public Function Declaration
 ===========================================================================*/
URL *url_open(char *str, char *mode);
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
