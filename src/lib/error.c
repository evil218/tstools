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
        "no ACK",
        "bad ID",
        "bad command",
        "bad argument",
        "bad case entry",
        "malloc failed",
        "fopen failed",
        "end of file",
        "verify failed",
        "^^^^", // subfunction failed
        "wrong errno" // should be the last one
};

/*============================================================================
 * Public Functions Definition
 ===========================================================================*/
void show_err(const char *file, int line, int err)
{
        err = (err > ERR_WRONG_ERRNO) ? ERR_WRONG_ERRNO : err;
        fprintf(stderr, "\"%s\", line %d: %s, ", file, line, errmsg[err]);
        return;
}

/*****************************************************************************
 * End
 ****************************************************************************/
