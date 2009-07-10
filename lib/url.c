//============================================================================
// Name: url.c
// Purpose: URL access
// To build: gcc -std-c99 -c url.c
//============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "url.h"

//============================================================================
// Sub-function declare:
//============================================================================
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
        url->path[0] = '\0';
        url->filename[0] = '\0';
        url->ip[0] = '\0';

        parse_url(url, str);

        switch(url->protocol)
        {
                case PRTCL_UDP:
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
static void parse_url(URL *url, const char *str)
{
        //printf("%s\n", str);
        if(0 == memcmp(str, "udp", 3))
        {
                strcpy(url->ip, str);
                url->port = atoi(str);
                url->protocol = PRTCL_UDP;
                printf("%s:%d\n", url->ip, url->port);
                exit(0);
        }
        else
        {
                strcpy(url->filename, str);
                url->protocol = PRTCL_FILE;
        }
}

/*****************************************************************************
 * End
 ****************************************************************************/
