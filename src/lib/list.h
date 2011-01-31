/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: list.h
// Purpose: Common Bidirection List
// To build: gcc -std-c99 -c list.c
//=============================================================================

#ifndef _LIST_H
#define _LIST_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Struct Declaration
 ===========================================================================*/
typedef struct _NODE
{
        struct _NODE *next;
        struct _NODE *prev;
}
NODE;

typedef struct _LIST
{
        NODE *head;
        NODE *tail;

        int count;
}
LIST;

/*============================================================================
 * Public Function Declaration
 ===========================================================================*/
void list_init(LIST *list);
void list_free(LIST *list);

void list_add(LIST *list, NODE *node); // to the end of list
void list_insert_before(LIST *list, NODE *next, NODE *node);
void list_insert_after(LIST *list, NODE *prev, NODE *node);
void list_del(LIST *list, NODE *node);
int list_count(LIST *list);

NODE *list_head(LIST *list);
NODE *list_tail(LIST *list);
NODE *list_next(NODE *node);
NODE *list_prev(NODE *node);

#ifdef __cplusplus
}
#endif

#endif // _LIST_H

/*****************************************************************************
 * End
 ****************************************************************************/
