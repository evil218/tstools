/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <stdlib.h>

#include "list.h"

#if 0
#define DEBUG /* print detail info. to debug this module */
#endif

#ifdef DEBUG
#define dbg(level, ...) \
        do { \
                if (level >= 0) { \
                        fprintf(stderr, "\"%s\", line %d: ",__FILE__, __LINE__); \
                        fprintf(stderr, __VA_ARGS__); \
                        fprintf(stderr, "\n"); \
                } \
        } while (0)
#else
#define dbg(level, ...)
#endif /* DEBUG */

void list_free(void *PHEAD)
{
        struct lnode **phead;
        struct lnode *lnode;

        if(!PHEAD) {
                dbg(1, "free: PHEAD is NULL");
                return;
        }
        phead = (struct lnode **)PHEAD;

        lnode = *phead;
        while(lnode) {
                struct lnode *temp;

                //dbg(1, "free: 0x%X(key=%d)", (int)lnode, lnode->key);
                dbg(1, "free: 0x%X", (int)lnode);
                temp = lnode->next;
                free(lnode);
                lnode = temp;
        }
        *phead = NULL;

        return;
}

void list_delete(void *PHEAD, void *LNODE)
{
        struct lnode **phead;
        struct lnode *lnode;

        if(!PHEAD) {
                dbg(1, "delete: PHEAD is NULL");
                return;
        }
        phead = (struct lnode **)PHEAD;

        if(!LNODE) {
                dbg(1, "delete: LNODE is NULL");
                return;
        }
        lnode = (struct lnode *)LNODE;

        if(lnode->prev) {
                lnode->prev->next = lnode->next;
        } else {
                dbg(1, "head-");
                if((*phead)->next) {
                        (*phead)->next->prev = NULL;
                        (*phead)->next->tail = (*phead)->tail;
                        *phead = (*phead)->next;
                } else {
                        dbg(1, "last-");
                        *phead = NULL;
                }
        }

        if(lnode->next) {
                lnode->next->prev = lnode->prev;
        } else {
                dbg(1, "tail-");
                if(lnode->prev) {
                        lnode->prev->next = NULL;
                        (*phead)->tail = lnode->prev;
                } else {
                        dbg(1, "last-");
                        *phead = NULL;
                }
        }

        dbg(1, "free");
        free(lnode);
        return;
}

/* to tail */
void list_push(void *PHEAD, void *LNODE)
{
        struct lnode **phead;
        struct lnode *lnode;

        if(!PHEAD) {
                dbg(1, "push: PHEAD is NULL");
                return;
        }
        phead = (struct lnode **)PHEAD;

        if(!LNODE) {
                dbg(1, "push: LNODE is NULL");
                return;
        }
        lnode = (struct lnode *)LNODE;
        lnode->next = NULL;

        if(*phead) {
                dbg(1, "push 0x%X into 0x%X", (int)lnode, (int)phead);
                lnode->prev = (*phead)->tail;
                (*phead)->tail->next = lnode; /* (*phead)->tail is ok! */
        } else {
                dbg(1, "push 0x%X into 0x%X(empty)", (int)lnode, (int)phead);
                lnode->prev = NULL;
                *phead = lnode;
        }

        (*phead)->tail = lnode;
        return;
}

/* to head */
void list_unshift(void *PHEAD, void *LNODE)
{
        struct lnode **phead;
        struct lnode *lnode;

        if(!PHEAD) {
                dbg(1, "unshift: PHEAD is NULL");
                return;
        }
        phead = (struct lnode **)PHEAD;

        if(!LNODE) {
                dbg(1, "unshift: LNODE is NULL");
                return;
        }
        lnode = (struct lnode *)LNODE;
        lnode->prev = NULL;

        if(*phead) {
                dbg(1, "head-");
                lnode->next = (*phead);
                lnode->tail = (*phead)->tail;
                (*phead)->prev = lnode;
        } else {
                dbg(1, "empty-");
                lnode->next = NULL;
                lnode->tail = lnode;
        }

        *phead = lnode;

        dbg(1, "unshift");
        return;
}

/* from tail, it's up to the caller to free the lnode with free()! */
void *list_pop(void *PHEAD)
{
        struct lnode **phead;
        struct lnode *tail;

        if(!PHEAD) {
                dbg(1, "pop: PHEAD is NULL");
                return NULL;
        }
        phead = (struct lnode **)PHEAD;
        tail = (*phead)->tail;

        if(!tail) {
                dbg(1, "pop: no tail node to pop");
                return NULL;
        }

        if(tail->prev) {
                tail->prev->next = NULL;
                (*phead)->tail = tail->prev;
        } else {
                dbg(1, "last-");
                *phead = NULL;
        }

        dbg(1, "pop");
        return tail;
}

/* from head, it's up to the caller to free the lnode with free()! */
void *list_shift(void *PHEAD)
{
        struct lnode **phead;
        struct lnode *head;

        if(!PHEAD) {
                dbg(1, "shift: PHEAD is NULL");
                return NULL;
        }
        phead = (struct lnode **)PHEAD;
        head = *phead;

        if(!head) {
                dbg(1, "shift: no head node to shift");
                return NULL;
        }

        if(head->next) {
                head->next->prev = NULL;
                head->next->tail = head->tail;
                *phead = head->next;
        } else {
                dbg(1, "last-");
                *phead = NULL;
        }

        dbg(1, "shift");
        return head;
}

/* sort with key, small key first */
void list_insert(void *PHEAD, void *LNODE)
{
        struct lnode **phead;
        struct lnode *lnode;
        struct lnode *x;

        if(!PHEAD) {
                dbg(1, "insert: PHEAD is NULL");
                return;
        }
        phead = (struct lnode **)PHEAD;

        if(!LNODE) {
                dbg(1, "insert: LNODE is NULL");
                return;
        }
        lnode = (struct lnode *)LNODE;

        if(!*phead) {
                dbg(1, "insert 0x%X into 0x%X(empty)", (int)lnode, (int)phead);
                lnode->next = NULL;
                lnode->prev = NULL;
                *phead = lnode;
                (*phead)->tail = lnode;
                return;
        }

        for(x = *phead; x; x = x->next) {
                if(x->key == lnode->key) {
                        dbg(1, "insert: key(%d) in list already, free new node", lnode->key);
                        free(lnode);
                        return;
                }

                if(x->key > lnode->key) {
                        dbg(1, "insert 0x%X into 0x%X before 0x%X", (int)lnode, (int)phead, (int)x);
                        lnode->next = x;
                        lnode->prev = x->prev;

                        if(x->prev) {
                                x->prev->next = lnode;
                                x->prev = lnode;
                        } else {
                                dbg(1, "as head-");
                                lnode->tail = (*phead)->tail;
                                *phead = lnode;
                                x->prev = lnode;
                        }

                        dbg(1, "add");
                        return;
                }
        }

        dbg(1, "insert 0x%X into 0x%X(as tail)", (int)lnode, (int)phead);
        list_push(phead, lnode);
        return;
}

void *list_search(void *PHEAD, int key)
{
        struct lnode **phead;
        struct lnode *lnode;

        if(!PHEAD) {
                dbg(1, "search: PHEAD is NULL");
                return NULL;
        }
        phead = (struct lnode **)PHEAD;

        for(lnode = *phead; lnode; lnode = lnode->next) {
                if(lnode->key == key) {
                        dbg(1, "search: 0x%X->key is %d", (int)lnode, key);
                        return lnode;
                }
        }

        dbg(1, "search: no %d", key);
        return NULL;
}

void list_set_key(void *LNODE, int key)
{
        struct lnode *lnode;

        if(!LNODE) {
                dbg(1, "set key: LNODE is NULL");
                return;
        }
        lnode = (struct lnode *)LNODE;

        dbg(1, "set key: %d", key);
        lnode->key = key;
        return;
}

void list_set_name(void *LNODE, const char *name)
{
        struct lnode *lnode;

        if(!LNODE) {
                dbg(1, "set name: LNODE is NULL");
                return;
        }
        lnode = (struct lnode *)LNODE;

        dbg(1, "set name: %s", name);
        lnode->name = name;
        return;
}
