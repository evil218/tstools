/* vim: set tabstop=8 shiftwidth=8: */
/*****************************************************************************
 * error.c
 * Error report mechanism
 * ZHOU Cheng, 2009-10-27
 ****************************************************************************/
#include <stdio.h>

#include "error.h"

/*============================================================================
 * Error Strings Declaration
 ===========================================================================*/
static const char *errmsg[] =
{
        "no error",
        "sth. must be wrong",
        "time out",
        "IIC raise SCK failed",
        "no acknowledgement",
        "bad ID",
        "bad command",
        "bad argument",
        "malloc failed",
        "fopen failed",
        "end of file",
        "verify failed",
        "subfunction failed",
        "wrong error number" // last string
};

/*============================================================================
 * Private Functions Declaration
 ===========================================================================*/
static int errno = 0;

/*============================================================================
 * Public Functions Definition
 ===========================================================================*/
void save_err(int err)
{
        errno = err;
        return;
}

void show_err(char *file, int line, int err)
{
        err = (err > ERR_MAX_ERRNO) ? ERR_MAX_ERRNO : err;
        fprintf(stderr, "\"%s\", line %d: %s\r\n", file, line, errmsg[err]);
        return;
}

/*============================================================================
 * Private Functions Definition
 ===========================================================================*/

/*****************************************************************************
 * End
 ****************************************************************************/
