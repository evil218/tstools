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
struct NODE
{
        struct NODE *next;
        struct NODE *prev;
};

struct LIST
{
        struct NODE *head;
        struct NODE *tail;

        int count;
};

/*============================================================================
 * Public Function Declaration
 ===========================================================================*/
struct LIST *list_init();
void list_free(struct LIST *list);

void list_add(struct LIST *list, struct NODE *node); // to the end of list
void list_insert_before(struct LIST *list, struct NODE *next, struct NODE *node);
void list_insert_after(struct LIST *list, struct NODE *prev, struct NODE *node);
void list_del(struct LIST *list, struct NODE *node);
int list_count(struct LIST *list);

struct NODE *list_head(struct LIST *list);
struct NODE *list_tail(struct LIST *list);
struct NODE *list_next(struct NODE *node);
struct NODE *list_prev(struct NODE *node);

#ifdef __cplusplus
}
#endif

#endif // _LIST_H

/*****************************************************************************
 * End
 ****************************************************************************/
