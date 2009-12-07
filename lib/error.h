/* vim: set tabstop=8 shiftwidth=8: */
/*****************************************************************************
 * error.h
 * Error report mechanism
 * ZHOU Cheng, 2009-10-27
 ****************************************************************************/
#ifndef _ERROR_H
#define _ERROR_H

/*============================================================================
 * Error Number Declaration
 ===========================================================================*/
enum
{
        ERR_NO_ERROR = 0,
        ERR_TIME_OUT,
        ERR_IIC_RAISE_SCK_FAILED,
        ERR_NACK,
        ERR_BAD_ID,
        ERR_BAD_CMD,
        ERR_BAD_ARG,
        ERR_MALLOC_FAILED,
        ERR_EOF,
        ERR_VERIFY_FAILED,
        ERR_SUBFUXN_FAILED,
        ERR_MAX_ERRNO                   // for count ERR number only
};

/*============================================================================
 * Error Micro Declaration
 ===========================================================================*/
#define DBG(err)                                                        \
        do                                                              \
{                                                                       \
        save_err((err));                                                \
        show_err(__FILE__, __LINE__, (err));                            \
}while(0)

/*============================================================================
 * Public Functions Declaration
 ===========================================================================*/
void save_err(int err);
void show_err(char *file, int line, int err);

#endif

/*****************************************************************************
 * End
 ****************************************************************************/
