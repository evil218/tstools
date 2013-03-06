/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "zlst.h"

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

void *zlst_delete(void *PHEAD, void *LNODE)
{
        struct znode **phead;
        struct znode *znode;

        if(!PHEAD) {
                RPT(RPT_INF, "delete: PHEAD is NULL");
                return NULL;
        }
        phead = (struct znode **)PHEAD;

        if(!LNODE) {
                RPT(RPT_INF, "delete: LNODE is NULL");
                return NULL;
        }
        znode = (struct znode *)LNODE;

        if(znode->prev) {
                znode->prev->next = znode->next;
        } else {
                RPT(RPT_INF, "head-");
                if((*phead)->next) {
                        (*phead)->next->prev = NULL;
                        (*phead)->next->tail = (*phead)->tail;
                        *phead = (*phead)->next;
                } else {
                        RPT(RPT_INF, "last-");
                        *phead = NULL;
                }
        }

        if(znode->next) {
                znode->next->prev = znode->prev;
        } else {
                RPT(RPT_INF, "tail-");
                if(znode->prev) {
                        znode->prev->next = NULL;
                        (*phead)->tail = znode->prev;
                } else {
                        RPT(RPT_INF, "last-");
                        *phead = NULL;
                }
        }

        RPT(RPT_INF, "free");
        return znode;
}

/* to tail */
void zlst_push(void *PHEAD, void *LNODE)
{
        struct znode **phead;
        struct znode *znode;

        if(!PHEAD) {
                RPT(RPT_INF, "push: PHEAD is NULL");
                return;
        }
        phead = (struct znode **)PHEAD;

        if(!LNODE) {
                RPT(RPT_INF, "push: LNODE is NULL");
                return;
        }
        znode = (struct znode *)LNODE;
        znode->next = NULL;

        if(*phead) {
                RPT(RPT_INF, "push 0x%X into 0x%X", (int)znode, (int)phead);
                znode->prev = (*phead)->tail;
                (*phead)->tail->next = znode; /* (*phead)->tail is ok! */
        } else {
                RPT(RPT_INF, "push 0x%X into 0x%X(empty)", (int)znode, (int)phead);
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
                RPT(RPT_INF, "unshift: PHEAD is NULL");
                return;
        }
        phead = (struct znode **)PHEAD;

        if(!LNODE) {
                RPT(RPT_INF, "unshift: LNODE is NULL");
                return;
        }
        znode = (struct znode *)LNODE;
        znode->prev = NULL;

        if(*phead) {
                RPT(RPT_INF, "head-");
                znode->next = (*phead);
                znode->tail = (*phead)->tail;
                (*phead)->prev = znode;
        } else {
                RPT(RPT_INF, "empty-");
                znode->next = NULL;
                znode->tail = znode;
        }

        *phead = znode;

        RPT(RPT_INF, "unshift");
        return;
}

/* from tail, it's up to the caller to free the znode with free()! */
void *zlst_pop(void *PHEAD)
{
        struct znode **phead;
        struct znode *tail;

        if(!PHEAD) {
                RPT(RPT_INF, "pop: PHEAD is NULL");
                return NULL;
        }
        phead = (struct znode **)PHEAD;
        if(!(*phead)) {
                RPT(RPT_INF, "pop: *PHEAD is NULL");
                return NULL;
        }
        tail = (*phead)->tail;

        if(!tail) {
                RPT(RPT_INF, "pop: no tail node to pop");
                return NULL;
        }

        if(tail->prev) {
                tail->prev->next = NULL;
                (*phead)->tail = tail->prev;
        } else {
                RPT(RPT_INF, "last-");
                *phead = NULL;
        }

        RPT(RPT_INF, "pop");
        return tail;
}

/* from head, it's up to the caller to free the znode with free()! */
void *zlst_shift(void *PHEAD)
{
        struct znode **phead;
        struct znode *head;

        if(!PHEAD) {
                RPT(RPT_INF, "shift: PHEAD is NULL");
                return NULL;
        }
        phead = (struct znode **)PHEAD;
        head = *phead;

        if(!head) {
                RPT(RPT_INF, "shift: no head node to shift");
                return NULL;
        }

        if(head->next) {
                head->next->prev = NULL;
                head->next->tail = head->tail;
                *phead = head->next;
        } else {
                RPT(RPT_INF, "last-");
                *phead = NULL;
        }

        RPT(RPT_INF, "shift");
        return head;
}

/* sort with key, small key first */
void *zlst_insert(void *PHEAD, void *LNODE)
{
        struct znode **phead;
        struct znode *znode;
        struct znode *x;

        if(!PHEAD) {
                RPT(RPT_INF, "insert: PHEAD is NULL");
                return NULL;
        }
        phead = (struct znode **)PHEAD;

        if(!LNODE) {
                RPT(RPT_INF, "insert: LNODE is NULL");
                return NULL;
        }
        znode = (struct znode *)LNODE;

        if(!*phead) {
                RPT(RPT_INF, "insert 0x%X into 0x%X(empty)", (int)znode, (int)phead);
                znode->next = NULL;
                znode->prev = NULL;
                *phead = znode;
                (*phead)->tail = znode;
                return NULL;
        }

        for(x = *phead; x; x = x->next) {
                if(x->key == znode->key) {
                        RPT(RPT_INF, "insert: key(%d) in list already, free new node", znode->key);
                        return znode;
                }

                if(x->key > znode->key) {
                        RPT(RPT_INF, "insert 0x%X into 0x%X before 0x%X", (int)znode, (int)phead, (int)x);
                        znode->next = x;
                        znode->prev = x->prev;

                        if(x->prev) {
                                x->prev->next = znode;
                                x->prev = znode;
                        } else {
                                RPT(RPT_INF, "as head-");
                                znode->tail = (*phead)->tail;
                                *phead = znode;
                                x->prev = znode;
                        }

                        RPT(RPT_INF, "add");
                        return NULL;
                }
        }

        RPT(RPT_INF, "insert 0x%X into 0x%X(as tail)", (int)znode, (int)phead);
        zlst_push(phead, znode);
        return NULL;
}

void *zlst_search(void *PHEAD, int key)
{
        struct znode **phead;
        struct znode *znode;

        if(!PHEAD) {
                RPT(RPT_INF, "search: PHEAD is NULL");
                return NULL;
        }
        phead = (struct znode **)PHEAD;

        for(znode = *phead; znode; znode = znode->next) {
                if(znode->key == key) {
                        RPT(RPT_INF, "search: 0x%X->key is %d", (int)znode, key);
                        return znode;
                }
        }

        RPT(RPT_INF, "search: no %d", key);
        return NULL;
}

void zlst_set_key(void *LNODE, int key)
{
        struct znode *znode;

        if(!LNODE) {
                RPT(RPT_INF, "set key: LNODE is NULL");
                return;
        }
        znode = (struct znode *)LNODE;

        RPT(RPT_INF, "set key: %d", key);
        znode->key = key;
        return;
}

void zlst_set_name(void *LNODE, const char *name)
{
        struct znode *znode;

        if(!LNODE) {
                RPT(RPT_INF, "set name: LNODE is NULL");
                return;
        }
        znode = (struct znode *)LNODE;

        RPT(RPT_INF, "set name: %s", name);
        znode->name = name;
        return;
}
