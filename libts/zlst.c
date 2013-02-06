/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <stdlib.h>

#include "zlst.h"

#if 0
#define DEBUG /* print detail info. to debug this module */
#endif

#ifdef DEBUG
#define dbg(level, ...) \
        do { \
                if (level >= 0) { \
                        fprintf(stderr, "\"%s\", line %d: ", __FILE__, __LINE__); \
                        fprintf(stderr, __VA_ARGS__); \
                        fprintf(stderr, "\n"); \
                } \
        } while (0)
#else
#define dbg(level, ...)
#endif /* DEBUG */

void zlst_free(void *PHEAD)
{
        struct znode **phead;
        struct znode *znode;

        if(!PHEAD) {
                dbg(1, "free: PHEAD is NULL");
                return;
        }
        phead = (struct znode **)PHEAD;

        znode = *phead;
        while(znode) {
                struct znode *temp;

                /* dbg(1, "free: 0x%X(key=%d)", (int)znode, znode->key); */
                dbg(1, "free: 0x%X", (int)znode);
                temp = znode->next;
                free(znode);
                znode = temp;
        }
        *phead = NULL;

        return;
}

void zlst_delete(void *PHEAD, void *LNODE)
{
        struct znode **phead;
        struct znode *znode;

        if(!PHEAD) {
                dbg(1, "delete: PHEAD is NULL");
                return;
        }
        phead = (struct znode **)PHEAD;

        if(!LNODE) {
                dbg(1, "delete: LNODE is NULL");
                return;
        }
        znode = (struct znode *)LNODE;

        if(znode->prev) {
                znode->prev->next = znode->next;
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

        if(znode->next) {
                znode->next->prev = znode->prev;
        } else {
                dbg(1, "tail-");
                if(znode->prev) {
                        znode->prev->next = NULL;
                        (*phead)->tail = znode->prev;
                } else {
                        dbg(1, "last-");
                        *phead = NULL;
                }
        }

        dbg(1, "free");
        free(znode);
        return;
}

/* to tail */
void zlst_push(void *PHEAD, void *LNODE)
{
        struct znode **phead;
        struct znode *znode;

        if(!PHEAD) {
                dbg(1, "push: PHEAD is NULL");
                return;
        }
        phead = (struct znode **)PHEAD;

        if(!LNODE) {
                dbg(1, "push: LNODE is NULL");
                return;
        }
        znode = (struct znode *)LNODE;
        znode->next = NULL;

        if(*phead) {
                dbg(1, "push 0x%X into 0x%X", (int)znode, (int)phead);
                znode->prev = (*phead)->tail;
                (*phead)->tail->next = znode; /* (*phead)->tail is ok! */
        } else {
                dbg(1, "push 0x%X into 0x%X(empty)", (int)znode, (int)phead);
                znode->prev = NULL;
                *phead = znode;
        }

        (*phead)->tail = znode;
        return;
}

/* to head */
void zlst_unshift(void *PHEAD, void *LNODE)
{
        struct znode **phead;
        struct znode *znode;

        if(!PHEAD) {
                dbg(1, "unshift: PHEAD is NULL");
                return;
        }
        phead = (struct znode **)PHEAD;

        if(!LNODE) {
                dbg(1, "unshift: LNODE is NULL");
                return;
        }
        znode = (struct znode *)LNODE;
        znode->prev = NULL;

        if(*phead) {
                dbg(1, "head-");
                znode->next = (*phead);
                znode->tail = (*phead)->tail;
                (*phead)->prev = znode;
        } else {
                dbg(1, "empty-");
                znode->next = NULL;
                znode->tail = znode;
        }

        *phead = znode;

        dbg(1, "unshift");
        return;
}

/* from tail, it's up to the caller to free the znode with free()! */
void *zlst_pop(void *PHEAD)
{
        struct znode **phead;
        struct znode *tail;

        if(!PHEAD) {
                dbg(1, "pop: PHEAD is NULL");
                return NULL;
        }
        phead = (struct znode **)PHEAD;
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

/* from head, it's up to the caller to free the znode with free()! */
void *zlst_shift(void *PHEAD)
{
        struct znode **phead;
        struct znode *head;

        if(!PHEAD) {
                dbg(1, "shift: PHEAD is NULL");
                return NULL;
        }
        phead = (struct znode **)PHEAD;
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
void zlst_insert(void *PHEAD, void *LNODE)
{
        struct znode **phead;
        struct znode *znode;
        struct znode *x;

        if(!PHEAD) {
                dbg(1, "insert: PHEAD is NULL");
                return;
        }
        phead = (struct znode **)PHEAD;

        if(!LNODE) {
                dbg(1, "insert: LNODE is NULL");
                return;
        }
        znode = (struct znode *)LNODE;

        if(!*phead) {
                dbg(1, "insert 0x%X into 0x%X(empty)", (int)znode, (int)phead);
                znode->next = NULL;
                znode->prev = NULL;
                *phead = znode;
                (*phead)->tail = znode;
                return;
        }

        for(x = *phead; x; x = x->next) {
                if(x->key == znode->key) {
                        dbg(1, "insert: key(%d) in list already, free new node", znode->key);
                        free(znode);
                        return;
                }

                if(x->key > znode->key) {
                        dbg(1, "insert 0x%X into 0x%X before 0x%X", (int)znode, (int)phead, (int)x);
                        znode->next = x;
                        znode->prev = x->prev;

                        if(x->prev) {
                                x->prev->next = znode;
                                x->prev = znode;
                        } else {
                                dbg(1, "as head-");
                                znode->tail = (*phead)->tail;
                                *phead = znode;
                                x->prev = znode;
                        }

                        dbg(1, "add");
                        return;
                }
        }

        dbg(1, "insert 0x%X into 0x%X(as tail)", (int)znode, (int)phead);
        zlst_push(phead, znode);
        return;
}

void *zlst_search(void *PHEAD, int key)
{
        struct znode **phead;
        struct znode *znode;

        if(!PHEAD) {
                dbg(1, "search: PHEAD is NULL");
                return NULL;
        }
        phead = (struct znode **)PHEAD;

        for(znode = *phead; znode; znode = znode->next) {
                if(znode->key == key) {
                        dbg(1, "search: 0x%X->key is %d", (int)znode, key);
                        return znode;
                }
        }

        dbg(1, "search: no %d", key);
        return NULL;
}

void zlst_set_key(void *LNODE, int key)
{
        struct znode *znode;

        if(!LNODE) {
                dbg(1, "set key: LNODE is NULL");
                return;
        }
        znode = (struct znode *)LNODE;

        dbg(1, "set key: %d", key);
        znode->key = key;
        return;
}

void zlst_set_name(void *LNODE, const char *name)
{
        struct znode *znode;

        if(!LNODE) {
                dbg(1, "set name: LNODE is NULL");
                return;
        }
        znode = (struct znode *)LNODE;

        dbg(1, "set name: %s", name);
        znode->name = name;
        return;
}
