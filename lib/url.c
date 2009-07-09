//=============================================================================
// Name: url.c
// Purpose: URL access
// To build: gcc -std-c99 -c url.c
//=============================================================================
#include <stdio.h>
#include <stdlib.h>

#include "url.h"

//=============================================================================
// Functions definition:
//=============================================================================
URL *url_open(char *str, char *mode)
{
        URL *url = (URL *)malloc(sizeof(URL));

        url->filename = str;
        if(NULL == (url->fd = fopen(url->filename, mode)))
        {
                printf("Can not open file \"%s\"!\n", url->filename);
                free(url);
                url = NULL;
        }

        return url;
}

int url_close(URL *url)
{
        fclose(url->fd);
        free(url);
        return 0;
}

int url_seek(URL *url, long offset, int origin)
{
        return fseek(url->fd, offset, origin);
}

int url_getc(URL *url)
{
        return fgetc(url->fd);
}

size_t url_read(void *buf, size_t size, size_t nobj, URL *url)
{
        return fread(buf, size, nobj, url->fd);
}

/*****************************************************************************
 * End
 ****************************************************************************/
