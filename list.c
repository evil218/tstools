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
struct LIST *list_init()
{
        struct LIST *list;

        list = (struct LIST *)malloc(sizeof(struct LIST));
        if(NULL == list)
        {
                printf("Malloc memory for list failure!\n");
                exit(EXIT_FAILURE);
        }

        list->head = NULL;
        list->tail = NULL;
        list->count = 0;

        return list;
}

void list_free(struct LIST *list)
{
        struct NODE *node;

        node = list->head;
        while(NULL != node)
        {
                list->head = node->next;
                free(node); // assume that system will free allocated size
                node = list->head;
        }
        free(list);
}

void list_add(struct LIST *list, struct NODE *node)
{
        if(NULL == list || NULL == node)
        {
                return;
        }
        else
        {
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
        }
}

void list_del(struct LIST *list, struct NODE *node)
{
        if(NULL == list || NULL == node)
        {
                return;
        }
        else
        {
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
        }
}

int list_count(struct LIST *list)
{
        if(NULL == list)
        {
                return 0;
        }
        else
        {
                return list->count;
        }
}

struct NODE *list_head(struct LIST *list)
{
        if(NULL == list)
        {
                return NULL;
        }
        else
        {
                return list->head;
        }
}

struct NODE *list_tail(struct LIST *list)
{
        if(NULL == list)
        {
                return NULL;
        }
        else
        {
                return list->tail;
        }
}

struct NODE *list_next(struct NODE *node)
{
        if(NULL == node)
        {
                return NULL;
        }
        else
        {
                return node->next;
        }
}

struct NODE *list_prev(struct NODE *node)
{
        if(NULL == node)
        {
                return NULL;
        }
        else
        {
                return node->prev;
        }
}

/*****************************************************************************
 * End
 ****************************************************************************/
