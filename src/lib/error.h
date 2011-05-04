/* vim: set tabstop=8 shiftwidth=8: */
/*****************************************************************************
 * error.h
 * Error report mechanism
 * ZHOU Cheng, 2009-10-27
 ****************************************************************************/
#ifndef _ERROR_H
#define _ERROR_H

/*============================================================================
 * enum Declaration
 ===========================================================================*/
enum error_number
{
        ERR_NO_ERROR = 0,
        ERR_OTHER = 1,
        ERR_TIME_OUT,
        ERR_IIC_RAISE_SCK_FAILED,
        ERR_NACK,
        ERR_BAD_ID,
        ERR_BAD_CMD,
        ERR_BAD_ARG,
        ERR_BAD_CASE,
        ERR_MALLOC_FAILED,
        ERR_FOPEN_FAILED,
        ERR_EOF,
        ERR_VERIFY_FAILED,
        ERR_SUBFUXN_FAILED,
        ERR_WRONG_ERRNO // should be the last enum!
};

/*============================================================================
 * Error Micro Declaration
 ===========================================================================*/
#define DBG(err, ...)                                                       \
        do                                                                  \
{                                                                           \
        show_err(__FILE__, __LINE__, (err));                                \
        fprintf(stderr, __VA_ARGS__);                                       \
}while(0)

/*============================================================================
 * Public Functions Declaration
 ===========================================================================*/
void show_err(const char *file, int line, int err);

#endif // _ERROR_H

/*****************************************************************************
 * End
 ****************************************************************************/
