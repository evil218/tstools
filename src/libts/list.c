/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: list.c
 * funx: Common Bidirection List
 */

#include <stdio.h>
#include <stdlib.h>

#include "libts/list.h"

static void list_insert_before(LIST *list, NODE *next, NODE *node);

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
        NODE *node;

        if(NULL == list)
        {
                return;
        }

        for(node = list->head; NULL != node;)
        {
                list->head = node->next;
                free(node);
                node = list->head;
        }
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
                /* head node */
                list->head = node->next;
        }
        else
        {
                node->prev->next = node->next;
        }

        if(NULL == node->next)
        {
                /* tail node */
                list->tail = node->prev;
        }
        else
        {
                node->next->prev = node->prev;
        }

        list->count--;
        return;
}

void list_add(LIST *list, NODE *node, uint32_t key)
{
        if(NULL == list ||
           NULL == node)
        {
                return;
        }

        node->key = key;
        node->next = NULL;
        node->prev = list->tail;

        if(NULL == list->tail)
        {
                /* empty list */
                list->head = node;
                list->tail = node;
        }
        else
        {
                /* normal list */
                list->tail->next = node;
                list->tail = node;
        }
        list->count++;
        return;
}

void list_insert(LIST *list, NODE *node, uint32_t key)
{
        NODE *x;

        if(NULL == list ||
           NULL == node)
        {
                return;
        }

        node->key = key;
        for(x = list->head; x; x = x->next)
        {
                if(x->key == key)
                {
                        /* find a node with the same key in list */
                        free(node);
#if 0
                        fprintf(stderr, "node in list already, ignore!\n");
#endif
                        return;
                }

                if(x->key > key)
                {
                        /* find a big one, insert before */
                        list_insert_before(list, x, node);
                        return;
                }
        }

        /* reach list tail, add */
        list_add(list, node, key);
        return;
}

NODE *list_search(LIST *list, uint32_t key)
{
        NODE *x;

        if(NULL == list)
        {
                return NULL;
        }

        for(x = list->head; x; x = x->next)
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

static void list_insert_before(LIST *list, NODE *next, NODE *node)
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
                /* head node */
                list->head = node;
                next->prev = node;
        }
        else
        {
                /* normal node */
                next->prev->next = node;
                next->prev = node;
        }
        list->count++;
        return;
}
