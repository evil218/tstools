/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <stdlib.h>

#include "zlst.h"

/* report level and macro */
#define ERR_LVL (1) /* error, system error */
#define WRN_LVL (2) /* warning, maybe wrong, maybe OK */
#define INF_LVL (3) /* important information */
#define DBG_LVL (4) /* debug information */

#define RPT_ERR(fmt...) do {if(ERR_LVL <= rpt_lvl) {fprintf(stderr, "%s: %d: err: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0)
#define RPT_WRN(fmt...) do {if(WRN_LVL <= rpt_lvl) {fprintf(stderr, "%s: %d: wrn: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0)
#define RPT_INF(fmt...) do {if(INF_LVL <= rpt_lvl) {fprintf(stderr, "%s: %d: inf: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0)
#define RPT_DBG(fmt...) do {if(DBG_LVL <= rpt_lvl) {fprintf(stderr, "%s: %d: dbg: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0)

static int rpt_lvl = WRN_LVL; /* report level: ERR, WRN, INF, DBG */

/* to tail */
void zlst_push(void *PHEAD, void *ZNODE)
{
        struct znode *head;
        struct znode *znode;

        if(!ZNODE) {
                RPT_ERR("push: bad node");
                return;
        }
        znode = (struct znode *)ZNODE;

        if(!PHEAD) {
                RPT_ERR("push: NOT a list");
                return;
        }
        head = *(struct znode **)PHEAD;

        if(!head) {
                RPT_INF("push: into empty list");
                znode->tail = znode;
                znode->next = NULL;
                znode->prev = NULL;
                *(struct znode **)PHEAD = znode;
                return;
        }

        RPT_INF("push: ok");
        znode->next = NULL;
        znode->prev = head->tail;
        head->tail->next = znode; /* head->tail is ok! */
        head->tail = znode;
        return;
}

/* to head */
void zlst_unshift(void *PHEAD, void *ZNODE)
{
        struct znode *head;
        struct znode *znode;

        if(!ZNODE) {
                RPT_ERR("unshift: bad node");
                return;
        }
        znode = (struct znode *)ZNODE;

        if(!PHEAD) {
                RPT_ERR("unshift: NOT a list");
                return;
        }
        head = *(struct znode **)PHEAD;

        if(!head) {
                RPT_INF("unshift: into empty list");
                znode->tail = znode;
                znode->next = NULL;
                znode->prev = NULL;
                *(struct znode **)PHEAD = znode;
                return;
        }

        RPT_INF("unshift: ok");
        znode->tail = head->tail;
        znode->next = head;
        znode->prev = NULL;
        head->prev = znode;
        *(struct znode **)PHEAD = znode;
        return;
}

/* from tail */
/* it's up to the caller to free the node! */
/*@null@*/
void *zlst_pop(void *PHEAD)
{
        struct znode *head;
        struct znode *tail;

        if(!PHEAD) {
                RPT_ERR("pop: NOT a list");
                return NULL;
        }
        head = *(struct znode **)PHEAD;

        if(!head) {
                RPT_INF("pop: from empty list");
                return NULL;
        }
        tail = head->tail;

        if(!(head->next)) {
                RPT_INF("pop: from one node list");
                *(struct znode **)PHEAD = NULL;
                return head;
        }

        if(!(tail->prev)) {
                RPT_ERR("pop: bad list");
                return NULL;
        }

        RPT_INF("pop: ok");
        tail->prev->next = NULL;
        head->tail = tail->prev;
        return tail;
}

/* from head */
/* it's up to the caller to free the node! */
/*@null@*/
void *zlst_shift(void *PHEAD)
{
        struct znode *head;

        if(!PHEAD) {
                RPT_ERR("shift: NOT a list");
                return NULL;
        }
        head = *(struct znode **)PHEAD;

        if(!head) {
                RPT_INF("shift: from empty list");
                return NULL;
        }

        if(!(head->next)) {
                RPT_INF("shift: from one node list");
                *(struct znode **)PHEAD = NULL;
                return head;
        }

        RPT_INF("shift: ok");
        head->next->prev = NULL;
        head->next->tail = head->tail;
        *(struct znode **)PHEAD = head->next;
        return head;
}

/* sort with key, small key first */
/* if not return 0, it's up to the caller to free the uninserted node! */
int zlst_insert(void *PHEAD, void *ZNODE)
{
        struct znode *head;
        struct znode *znode;
        struct znode *x;

        if(!ZNODE) {
                RPT_ERR("insert: bad node");
                return -1;
        }
        znode = (struct znode *)ZNODE;

        if(!PHEAD) {
                RPT_ERR("insert: NOT a list");
                return -1;
        }
        head = *(struct znode **)PHEAD;

        if(!head) {
                RPT_INF("insert: into empty list");
                znode->tail = znode;
                znode->next = NULL;
                znode->prev = NULL;
                *(struct znode **)PHEAD = znode;
                return 0;
        }

        if(head->key > znode->key) {
                RPT_INF("insert: %d as head before %d", znode->key, head->key);
                znode->tail = head->tail;
                znode->next = head;
                znode->prev = NULL;
                head->prev = znode;
                *(struct znode **)PHEAD = znode;
                return 0;
        }

        for(x = head; x; x = x->next) {
                if(x->key == znode->key) {
                        RPT_INF("insert: %d in list already", znode->key);
                        return -1;
                }

                if(x->key > znode->key) {
                        RPT_INF("insert: %d before %d", znode->key, x->key);
                        znode->next = x;
                        znode->prev = x->prev;
                        x->prev->next = znode;
                        x->prev = znode;
                        return 0;
                }
        }

        RPT_INF("insert: %d as tail after %d", znode->key, head->tail->key);
        znode->next = NULL;
        znode->prev = head->tail;
        head->tail->next = znode;
        head->tail = znode;
        return 0;
}

/* it's up to the caller to free the node! */
/*@null@*/
void *zlst_delete(void *PHEAD, /*@dependent@*/ void *ZNODE)
{
        struct znode *head;
        struct znode *znode;

        if(!ZNODE) {
                RPT_ERR("delete: bad node");
                return NULL;
        }
        znode = (struct znode *)ZNODE;

        if(!PHEAD) {
                RPT_ERR("delete: NOT a list");
                return NULL;
        }
        head = *(struct znode **)PHEAD;

        if(!head) {
                RPT_ERR("delete: from an empty list");
                return znode;
        }

        if(!(znode->prev) && !(znode->next)) {
                RPT_INF("delete: from one node list");
                *(struct znode **)PHEAD = NULL;
                return znode;
        }

        if(!(znode->prev) && (NULL != znode->next)) {
                RPT_INF("delete: head node");
                znode->next->tail = znode->tail;
                znode->next->prev = NULL;
                *(struct znode **)PHEAD = znode->next;
                return znode;
        }

        if((NULL != znode->prev) && !(znode->next)) {
                RPT_INF("delete: tail node");
                head->tail = znode->prev;
                znode->prev->next = NULL;
                return znode;
        }

        /* (znode->prev) && (znode->next) */
        RPT_INF("delete: ok");
        znode->prev->next = znode->next;
        znode->next->prev = znode->prev;
        return znode;
}

/*@null@*/
/*@temp@*/
void *zlst_search(void *PHEAD, int key)
{
        struct znode *head;
        struct znode *znode;

        if(!PHEAD) {
                RPT_ERR("search: NOT a list");
                return NULL;
        }
        head = *(struct znode **)PHEAD;

        if(!head) {
                RPT_INF("search: in an empty list");
                return NULL;
        }

        for(znode = head; znode; znode = znode->next) {
                if(znode->key == key) {
                        RPT_INF("search: got %d", key);
                        return znode;
                }
        }

        RPT_INF("search: no %d", key);
        return NULL;
}

void zlst_set_key(void *ZNODE, int key)
{
        struct znode *znode;

        if(!ZNODE) {
                RPT_ERR("set key: bad node");
                return;
        }
        znode = (struct znode *)ZNODE;

        RPT_INF("set key: %d", key);
        znode->key = key;
        return;
}

void zlst_set_name(void *ZNODE, const char *name)
{
        struct znode *znode;

        if(!ZNODE) {
                RPT_ERR("set name: bad node");
                return;
        }
        znode = (struct znode *)ZNODE;

        RPT_INF("set name: %s", name);
        znode->name = name; /* the string should be const */
        return;
}
