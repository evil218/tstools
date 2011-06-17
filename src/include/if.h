/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: if.h
// Purpose: bin <-> txt data convert
// To build: gcc -std-c99 -c if.c
//=============================================================================

#ifndef _IF_H
#define _IF_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Public Function Declaration
 ===========================================================================*/
int b2t(void *tbuf, void *bbuf, int size, char white_space);
int t2b(void *bbuf, void *tbuf);

#ifdef __cplusplus
}
#endif

#endif // _IF_H

/*****************************************************************************
 * End
 ****************************************************************************/
