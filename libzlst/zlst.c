/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <stdlib.h>

#include "zlst.h"

/* report level and macro */
#define RPT_ERR (1) /* error, system error */
#define RPT_WRN (2) /* warning, maybe wrong, maybe OK */
#define RPT_INF (3) /* important information */
#define RPT_DBG (4) /* debug information */

#ifdef S_SPLINT_S /* FIXME */
#define RPTERR(fmt...) do {if(RPT_ERR <= rpt_lvl) {fprintf(stderr, "%s: %d: err: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0 == 1)
#define RPTWRN(fmt...) do {if(RPT_WRN <= rpt_lvl) {fprintf(stderr, "%s: %d: wrn: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0 == 1)
#define RPTINF(fmt...) do {if(RPT_INF <= rpt_lvl) {fprintf(stderr, "%s: %d: inf: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0 == 1)
#define RPTDBG(fmt...) do {if(RPT_DBG <= rpt_lvl) {fprintf(stderr, "%s: %d: dbg: ", __FILE__, __LINE__); fprintf(stderr, fmt); fprintf(stderr, "\n");}} while(0 == 1)
#else
#define RPTERR(fmt, ...) do {if(RPT_ERR <= rpt_lvl) {fprintf(stderr, "%s: %d: err: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);}} while(0 == 1)
#define RPTWRN(fmt, ...) do {if(RPT_WRN <= rpt_lvl) {fprintf(stderr, "%s: %d: wrn: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);}} while(0 == 1)
#define RPTINF(fmt, ...) do {if(RPT_INF <= rpt_lvl) {fprintf(stderr, "%s: %d: inf: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);}} while(0 == 1)
#define RPTDBG(fmt, ...) do {if(RPT_DBG <= rpt_lvl) {fprintf(stderr, "%s: %d: dbg: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);}} while(0 == 1)
#endif

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

void *zlst_push(zhead_t *PHEAD, void *ZNODE)
{
        struct znode *head;
        struct znode *tail; /* old tail */
        struct znode *znode;

        if(NULL == ZNODE) {
                RPTERR("push: bad node");
                return ZNODE;
        }
        znode = (struct znode *)ZNODE;

        if(NULL == PHEAD) {
                RPTERR("push: NOT a list");
                return ZNODE;
        }
        head = *PHEAD;

        if(NULL == head) {
                RPTINF("push: into empty list");
                znode->tail = znode;
                znode->next = NULL;
                znode->prev = NULL;
                *PHEAD = znode;
                return NULL;
        }

        tail = head->tail;
        if(NULL == tail || NULL != tail->next) {
                RPTERR("push: bad head->tail");
                return ZNODE;
        }

        RPTINF("push: to tail");
        znode->next = NULL;
        znode->prev = tail;
        head->tail = znode;
        tail->next = znode;
        return NULL;
}

void *zlst_unshift(zhead_t *PHEAD, void *ZNODE)
{
        struct znode *head; /* old head */
        struct znode *znode;

        if(NULL == ZNODE) {
                RPTERR("unshift: bad node");
                return ZNODE;
        }
        znode = (struct znode *)ZNODE;

        if(NULL == PHEAD) {
                RPTERR("unshift: NOT a list");
                return ZNODE;
        }
        head = *PHEAD;

        if(NULL == head) {
                RPTINF("unshift: into empty list");
                znode->tail = znode;
                znode->next = NULL;
                znode->prev = NULL;
                *PHEAD = znode;
                return NULL;
        }

        RPTINF("unshift: to head");
        *PHEAD = znode;
        znode->tail = head->tail;
        znode->next = head;
        znode->prev = NULL;
        head->prev = znode;
        return NULL;
}

void *zlst_pop(zhead_t *PHEAD)
{
        struct znode *head;
        struct znode *tail; /* old tail */
        struct znode *rslt; /* old tail */

        if(NULL == PHEAD) {
                RPTERR("pop: NOT a list");
                return NULL;
        }
        head = *PHEAD;

        if(NULL == head) {
                RPTINF("pop: empty list");
                return NULL;
        }

        if(NULL == head->next) {
                RPTINF("pop: last node");
                *PHEAD = NULL;
                return head;
        }

        tail = head->tail;
        if(NULL == tail || NULL == tail->prev) {
                RPTERR("pop: bad tail");
                return NULL;
        }

        RPTINF("pop: from tail");
        rslt = tail->prev->next; /* rslt becomes owned */
        tail->prev->next = NULL;
        head->tail = tail->prev;
        return rslt;
}

void *zlst_shift(zhead_t *PHEAD)
{
        struct znode *head;
        struct znode *scnd; /* second node */

        if(NULL == PHEAD) {
                RPTERR("shift: NOT a list");
                return NULL;
        }
        head = *PHEAD;

        if(NULL == head) {
                RPTINF("shift: empty list");
                return NULL;
        }

        scnd = head->next;
        if(NULL == scnd) {
                RPTINF("shift: last node");
                *PHEAD = NULL;
                return head;
        }

        RPTINF("shift: from head");
        scnd->prev = NULL;
        scnd->tail = head->tail;
        *PHEAD = scnd;
        return head;
}

/* sort with key, small key first */
void *zlst_insert(zhead_t *PHEAD, void *ZNODE)
{
        struct znode *head;
        struct znode *znode;
        struct znode *prev;
        struct znode *x;
        struct znode *tail;

        if(NULL == ZNODE) {
                RPTERR("insert: bad node");
                return ZNODE;
        }
        znode = (struct znode *)ZNODE;

        if(NULL == PHEAD) {
                RPTERR("insert: NOT a list");
                return ZNODE;
        }
        head = *PHEAD;

        if(NULL == head) {
                RPTINF("insert: empty list");
                znode->tail = znode;
                znode->next = NULL;
                znode->prev = NULL;
                *PHEAD = znode;
                return NULL;
        }

        if(head->key > znode->key) {
                RPTINF("insert: %d before %d as head", znode->key, head->key);
                *PHEAD = znode;
                znode->tail = head->tail;
                znode->next = head;
                znode->prev = NULL;
                head->prev = znode;
                return NULL;
        }

        for(x = head->next; NULL != x; x = x->next) {
                if(x->key == znode->key) {
                        RPTINF("insert: %d in list already", znode->key);
                        return ZNODE;
                }

                if(x->key > znode->key) {
                        prev = x->prev;
                        if(NULL == prev) {
                                RPTERR("insert: bad x node");
                                return ZNODE;
                        }

                        RPTINF("insert: %d before %d", znode->key, x->key);
                        znode->next = x;
                        znode->prev = prev;
                        prev->next = znode;
                        x->prev = znode;
                        return NULL;
                }
        }

        tail = head->tail;
        if(NULL == tail) {
                RPTERR("insert: bad head");
                return ZNODE;
        }

        RPTINF("insert: %d after %d as tail", znode->key, tail->key);
        znode->next = NULL;
        znode->prev = tail;
        tail->next = znode;
        head->tail = znode;
        return NULL;
}

void *zlst_delete(zhead_t *PHEAD, void *ZNODE)
{
        struct znode *head;
        struct znode *znode;

        if(NULL == ZNODE) {
                RPTERR("delete: bad node");
                return NULL;
        }
        znode = (struct znode *)ZNODE;

        if(NULL == PHEAD) {
                RPTERR("delete: NOT a list");
                return NULL;
        }
        head = *PHEAD;

        if(NULL == head) {
                RPTERR("delete: empty list");
                return NULL;
        }

        if(NULL == znode->prev) {
                if(head != znode) {
                        RPTERR("delete: bad arg");
                        return NULL;
                }
                if(NULL == znode->next) {
                        RPTINF("delete: last node");
                        *PHEAD = NULL;
                        return head;
                }
                else {
                        RPTINF("delete: head node");
                        znode->next->tail = znode->tail;
                        znode->next->prev = NULL;
                        *PHEAD = znode->next;
                        return head;
                }
        }
        else { /* (NULL != znode->prev) */
                struct znode *rslt = znode->prev->next; /* node to be free */

                if(NULL == znode->next) {
                        if(head->tail != znode) {
                                RPTERR("delete: bad arg");
                                return NULL;
                        }
                        RPTINF("delete: tail node");
                        head->tail = znode->prev;
                        znode->prev->next = NULL;
                        return rslt;
                }
                else {
                        RPTINF("delete: middle node");
                        znode->prev->next = znode->next;
                        znode->next->prev = znode->prev;
                        return rslt;
                }
        }
}

void *zlst_search(zhead_t *PHEAD, int key)
{
        struct znode *head;
        struct znode *znode;

        if(NULL == PHEAD) {
                RPTERR("search: NOT a list");
                return NULL;
        }
        head = *PHEAD;

        if(NULL == head) {
                RPTINF("search: empty list");
                return NULL;
        }

        for(znode = head; NULL != znode; znode = znode->next) {
                if(znode->key == key) {
                        RPTINF("search: got %d", key);
                        return znode;
                }
        }

        RPTINF("search: no %d", key);
        return NULL;
}

void zlst_set_key(void *ZNODE, int key)
{
        struct znode *znode;

        if(NULL == ZNODE) {
                RPTERR("set key: bad node");
                return;
        }
        znode = (struct znode *)ZNODE;

        RPTINF("set key: %d", key);
        znode->key = key;
        return;
}

void zlst_set_name(void *ZNODE, const char *name)
{
        struct znode *znode;

        if(NULL == ZNODE) {
                RPTERR("set name: bad node");
                return;
        }
        znode = (struct znode *)ZNODE;

        if(NULL == name) {
                RPTWRN("set name: null name");
        }
        else {
                RPTINF("set name: %s", name);
        }
        znode->name = name; /* the string should be const */
        return;
}
