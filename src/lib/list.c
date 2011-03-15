/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: list.c
// Purpose: Common Bidirection List
// To build: gcc -std-c99 -c list.c
//=============================================================================
#include <stdio.h>
#include <stdlib.h>

#include "list.h"

//=============================================================================
// Functions definition:
//=============================================================================
void list_init(LIST *list)
{
        if(NULL == list)
        {
                return;
        }

        list->head = NULL;
        list->tail = NULL;
        list->count = 0;
        return;
}

void list_free(LIST *list)
{
        if(NULL == list)
        {
                return;
        }

        for(NODE *node = list->head; NULL != node;)
        {
                list->head = node->next;
                free(node);
                node = list->head;
        }
        return;
}

void list_add(LIST *list, NODE *node)
{
        if(NULL == list ||
           NULL == node)
        {
                return;
        }

        node->next = NULL;
        node->prev = list->tail;

        if(NULL == list->tail)
        {
                list->head = node;
                list->tail = node;
        }
        else
        {
                list->tail->next = node;
                list->tail = node;
        }
        list->count++;
        return;
}

void list_del(LIST *list, NODE *node)
{
        if(NULL == list ||
           NULL == node)
        {
                return;
        }

        if(NULL == node->prev)
        {
                list->head = node->next;
        }
        else
        {
                node->prev->next = node->next;
        }

        if(NULL == node->next)
        {
                list->tail = node->prev;
        }
        else
        {
                node->next->prev = node->prev;
        }

        list->count--;
        return;
}

void list_insert_before(LIST *list, NODE *next, NODE *node)
{
        if(NULL == list ||
           NULL == next ||
           NULL == node)
        {
                return;
        }

        node->next = next;
        node->prev = next->prev;

        if(NULL == next->prev)
        {
                list->head = node;
                next->prev = node;
        }
        else
        {
                next->prev->next = node;
                next->prev = node;
        }
        list->count++;
        return;
}

void list_insert_after(LIST *list, NODE *prev, NODE *node)
{
        if(NULL == list ||
           NULL == prev ||
           NULL == node)
        {
                return;
        }

        node->next = prev->next;
        node->prev = prev;

        if(NULL == prev->next)
        {
                list->tail = node;
                prev->next = node;
        }
        else
        {
                prev->next->prev = node;
                prev->next = node;
        }
        list->count++;
        return;
}

void list_insert(LIST *list, NODE *node)
{
        if(NULL == list ||
           NULL == node)
        {
                return;
        }

        for(NODE *x = list->head; x; x = x->next)
        {
                if(x->key == node->key)
                {
                        fprintf(stderr, "node in list already, ignore!\n");
                        return;
                }

                if(x->key > node->key)
                {
                        // find a big one, insert before
                        list_insert_before(list, x, node);
                        return;
                }
        }

        // reach list tail, add
        list_add(list, node);
        return;
}

NODE *list_search(LIST *list, uint32_t key)
{
        if(NULL == list)
        {
                return NULL;
        }

        for(NODE *x = list->head; x; x = x->next)
        {
                if(x->key == key)
                {
                        return x;
                }
        }
        return NULL;
}

int list_cnt(LIST *list)
{
        return (NULL != list) ? list->count : 0;
}

NODE *list_head(LIST *list)
{
        return (NULL != list) ? list->head : NULL;
}

NODE *list_tail(LIST *list)
{
        return (NULL != list) ? list->tail : NULL;
}

NODE *list_next(NODE *node)
{
        return (NULL != node) ? node->next : NULL;
}

NODE *list_prev(NODE *node)
{
        return (NULL != node) ? node->prev : NULL;
}

/*****************************************************************************
 * End
 ****************************************************************************/
