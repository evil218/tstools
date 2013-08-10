/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memcpy */
#include <stdint.h> /* for uintN_t, etc */

#include "buddy.h"

/* report level */
#define RPT_ERR (1) /* error, system error */
#define RPT_WRN (2) /* warning, maybe wrong, maybe OK */
#define RPT_INF (3) /* important information */
#define RPT_DBG (4) /* debug information */

/* report micro */
#define RPT(lvl, fmt...) do \
{ \
        if((int)lvl <= rpt_lvl) \
        { \
                switch(lvl) \
                { \
                        case RPT_ERR: fprintf(stderr, "%s: %d: err: ", __FILE__, __LINE__); break; \
                        case RPT_WRN: fprintf(stderr, "%s: %d: wrn: ", __FILE__, __LINE__); break; \
                        case RPT_INF: fprintf(stderr, "%s: %d: inf: ", __FILE__, __LINE__); break; \
                        case RPT_DBG: fprintf(stderr, "%s: %d: dbg: ", __FILE__, __LINE__); break; \
                        default:      fprintf(stderr, "%s: %d: ???: ", __FILE__, __LINE__); break; \
                } \
                fprintf(stderr, fmt); \
                fprintf(stderr, "\n"); \
        } \
} while (0)

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define IS_POWER_OF_2(x) (!((x) & ((x) - 1)))

#define LSUBTREE(index) (((index) << 1) + 1)
#define RSUBTREE(index) (((index) << 1) + 2)
#define PARENT(index)   ((((index) + 1) >> 1) - 1)

struct buddy_pool
{
        size_t omax; /* max order */
        size_t omin; /* min order */
        uint8_t *tree; /* binary tree, the array to describe the status of pool */
        size_t size; /* pool size: (1 << omax) */
        uint8_t *pool; /* pool buffer */
};

static size_t smallest_order(size_t size);

intptr_t buddy_create(size_t order_max, size_t order_min)
{
        struct buddy_pool *p;
        size_t tree_size;

        if(order_max > BUDDY_ORDER_MAX) {
                RPT(RPT_ERR, "create: bad order_max: %zd > %zd", order_max, BUDDY_ORDER_MAX);
                return 0; /* failed */
        }

        if(order_min >= order_max) {
                RPT(RPT_ERR, "create: bad order_min: %zd >= %zd", order_min, order_max);
                return 0; /* failed */
        }

        if(order_min < 1 || order_max < 1) {
                RPT(RPT_ERR, "create: bad order: %zd < 1 or %zd < 1", order_min, order_max);
                return 0; /* failed */
        }

        p = (struct buddy_pool *)malloc(sizeof(struct buddy_pool));
        if(NULL == p) {
                RPT(RPT_ERR, "create: create buddy pool object failed");
                return 0; /* failed */
        }

        p->omax = order_max;
        p->omin = order_min;
        p->size = ((size_t)1 << order_max);

        tree_size = (1 << (p->omax - p->omin + 1)) - 1;
        p->tree = (uint8_t *)malloc(tree_size); /* FIXME: memalign? */
        if(NULL == p->tree) {
                RPT(RPT_ERR, "create: malloc tree(%zd-byte) failed", tree_size);
                free(p);
                return 0; /* failed */
        }
        RPT(RPT_DBG, "create: tree: %8zX-byte @ %p", tree_size, p->tree);

        p->pool = (uint8_t *)malloc(p->size); /* FIXME: memalign? */
        if(NULL == p->pool) {
                RPT(RPT_ERR, "create: malloc pool(%zd-byte) failed", p->size);
                free(p->tree);
                free(p);
                return 0; /* failed */
        }
        RPT(RPT_DBG, "create: pool: %8zX-byte @ %p, min space: %zX", p->size, p->pool, (size_t)1 << order_min);

        p->tree[0] = 0; /* to avoid use malloc() before init() */
        return (intptr_t)p;
}

int buddy_destroy(intptr_t id)
{
        struct buddy_pool *p;

        p = (struct buddy_pool *)id;
        if(NULL == p) {
                RPT(RPT_ERR, "destroy: bad id");
                return -1;
        }
        if(NULL == p->tree) {
                RPT(RPT_ERR, "destroy: bad tree");
                return -1;
        }
        if(NULL == p->pool) {
                RPT(RPT_ERR, "destroy: bad pool");
                return -1;
        }

        free(p->pool);
        free(p->tree);
        free(p);
        return 0;
}

int buddy_init(intptr_t id)
{
        struct buddy_pool *p;
        size_t tree_size;
        size_t order;
        size_t i;

        p= (struct buddy_pool *)id;
        if(NULL == p) {
                RPT(RPT_ERR, "init: bad id");
                return -1;
        }
        if(NULL == p->tree) {
                RPT(RPT_ERR, "init: bad tree");
                return -1;
        }

        tree_size = (1 << (p->omax - p->omin + 1)) - 1;
        for(order = p->omax + 1, i = 0; i < tree_size; i++) {
                if(IS_POWER_OF_2(i + 1)) {
                        order--;
                }
                p->tree[i] = order;
        }
        return 0;
}

int buddy_status(intptr_t id, int enable, const char *hint)
{
        struct buddy_pool *p;
        size_t order;
        size_t tree_size;
        size_t cnt;
        size_t acc;
        size_t i;

        p = (struct buddy_pool *)id;
        if(NULL == p) {
                RPT(RPT_ERR, "status: bad id");
                return -1;
        }
        if(NULL == p->tree) {
                RPT(RPT_ERR, "status: bad tree");
                return -1;
        }
        if(!enable) {
                RPT(RPT_INF, "status: disable: do nothing");
                return 0;
        }

        order = p->omax + 1;
        tree_size = (1 << (p->omax - p->omin + 1)) - 1;
        cnt = 0;
        acc = 0;
        fprintf(stderr,"buddy: ");
#if 0
        fprintf(stderr,"\n");
#endif
        for(i = 0; i < tree_size; i++) {
                if(IS_POWER_OF_2(i + 1)) {
                        order--;
                        cnt = 0;
                }
                if(0 == p->tree[i]) {
                        if(LSUBTREE(i) >= tree_size || RSUBTREE(i) >= tree_size) {
                                /* no subtree */
                                cnt++;
                                acc += (1 << order);
#if 0
                                uint8_t *buf = (p->pool + (i + 1) * (1<<order) - (p->size));
                                for(size_t x = 0; x <(1 << order); x++) {
                                        fprintf(stderr, "%c", *(buf + x));
                                }
                                fprintf(stderr, "\n");
                                for(size_t x = 0; x <(1 << order); x++) {
                                        fprintf(stderr, "%02X ", *(buf + x));
                                }
                                fprintf(stderr, "\n");
#endif
                        }
                        else {
                                /* depend on subtree */
                                if(0 != p->tree[LSUBTREE(i)] || 0 != p->tree[RSUBTREE(i)]) {
                                        cnt++;
                                        acc += (1 << order);
#if 0
                                        uint8_t *buf = (p->pool + (i + 1) * (1<<order) - (p->size));
                                        for(size_t x = 0; x <(1 << order); x++) {
                                                fprintf(stderr, "%c", *(buf + x));
                                        }
                                        fprintf(stderr, "\n");
                                        for(size_t x = 0; x <(1 << order); x++) {
                                                fprintf(stderr, "%02X ", *(buf + x));
                                        }
                                        fprintf(stderr, "\n");
#endif
                                }
                        }
                }
                if(IS_POWER_OF_2(i + 2) && (0 != cnt)) {
                        fprintf(stderr,"%3zd x 0x%zX, ", cnt, (size_t)1 << order);
#if 0
                        fprintf(stderr, "\n");
#endif
                }
        }
        fprintf(stderr,"(%zu / %zu) used", acc, (size_t)1 << p->omax);
        fprintf(stderr,": %s\n", ((hint) ? hint : ""));
        return 0;
}

void *buddy_malloc(intptr_t id, size_t size)
{
        struct buddy_pool *p = (struct buddy_pool *)id;

        if(NULL == p) {
                RPT(RPT_ERR, "malloc: bad id");
                return NULL;
        }
        if(NULL == p->tree) {
                RPT(RPT_ERR, "malloc: bad tree");
                return NULL;
        }
        if(NULL == p->pool) {
                RPT(RPT_ERR, "malloc: bad pool");
                return NULL;
        }

        if(0 == size) {
                RPT(RPT_ERR, "malloc: bad size: 0");
                return NULL;
        }

        /* determine aim order in tree */
        size_t order = MAX(smallest_order(size), p->omin);
        if((int)(p->tree[0]) < order) {
                RPT(RPT_ERR, "malloc: not enough space in pool");
                return NULL;
        }

        /* search aim node in the tree */
        size_t i = 0; /* index of binary tree array */
        for(size_t current_order = p->omax; current_order > order; current_order--) {
                if(p->tree[LSUBTREE(i)] >= order) {
                        i = LSUBTREE(i);
                }
                else {
                        i = RSUBTREE(i);
                }
        }
        p->tree[i] = 0; /* find the aim node */

        uint8_t *rslt = (p->pool + (i + 1) * (1<<order) - (p->size));
        RPT(RPT_DBG, "malloc:  @ %p %zX %zX", rslt, (size_t)1 << order, size);

        /* modify parent order */
        while(i) {
                i = PARENT(i);
                p->tree[i] = MAX(p->tree[LSUBTREE(i)], p->tree[RSUBTREE(i)]) ;
        }

        return rslt;
}

void *buddy_realloc(intptr_t id, void *ptr, size_t size) /* FIXME: need to be test */
{
        struct buddy_pool *p = (struct buddy_pool *)id;

        if(NULL == p) {
                RPT(RPT_ERR, "realloc: bad id");
                return NULL;
        }
        if(NULL == p->tree) {
                RPT(RPT_ERR, "realloc: bad tree");
                return NULL;
        }
        if(NULL == p->pool) {
                RPT(RPT_ERR, "realloc: bad pool");
                return NULL;
        }
        if(NULL == ptr) {
                RPT(RPT_ERR, "realloc: bad ptr");
                return NULL;
        }
        if(0 == size) {
                RPT(RPT_ERR, "realloc: bad size: 0");
                return NULL;
        }

        /* determine offset, then search old node in the tree */
        size_t offset = (uint8_t *)ptr - p->pool;
        if(offset < 0 || offset >= p->size) {
                RPT(RPT_ERR, "realloc: bad ptr: %p, out of pool", ptr);
                return NULL;
        }

        size_t old_order = p->omin;
        size_t oi = 0; /* index of binary tree array */
        size_t found = 0;
        while(offset % (1<<old_order) == 0) {
                oi = (1<<(p->omax - old_order)) - 1 + offset / (1<<old_order);
                if(0 == p->tree[oi]) {
                        found = 1;
                        break;
                }
                old_order++;
        }
        if(!found) {
                RPT(RPT_ERR, "realloc: bad ptr: %p, illegal node or module bug", ptr);
                return NULL;
        }

        /* determine new_order in tree */
        size_t new_order = MAX(smallest_order(size), p->omin);
        if((int)(p->tree[0]) < new_order) {
                RPT(RPT_ERR, "realloc: not enough space in pool");
                return NULL;
        }

        /* maybe do not need to realloc */
        if(new_order <= old_order) {
                return ptr;
        }

        /* search new node in the tree */
        size_t ni = 0; /* index of binary tree array */
        for(size_t current_order = p->omax; current_order > new_order; current_order--) {
                if(p->tree[LSUBTREE(ni)] >= new_order) {
                        ni = LSUBTREE(ni);
                }
                else {
                        ni = RSUBTREE(ni);
                }
        }

        uint8_t *rslt = (p->pool + (ni + 1) * (1<<new_order) - (p->size));
        RPT(RPT_DBG, "realloc: @ %p %zX %zX", rslt, (size_t)1 << new_order, size);

        /* modify tree for new */
        p->tree[ni] = 0;
        while(ni) {
                ni = PARENT(ni);
                p->tree[ni] = MAX(p->tree[LSUBTREE(ni)], p->tree[RSUBTREE(ni)]) ;
        }

        /* copy data */
        memcpy(rslt, ptr, (1<<old_order));

        /* modify tree for old */
        size_t lorder;
        size_t rorder;
        RPT(RPT_DBG, "realloc: @ %p %zX", ptr, (size_t)1 << old_order);
        p->tree[oi] = old_order;
        while(oi) {
                oi = PARENT(oi) ;
                old_order++;

                lorder = p->tree[LSUBTREE(oi)];
                rorder = p->tree[RSUBTREE(oi)];
                if(lorder == (old_order - 1) &&
                   rorder == (old_order - 1)) {
                        p->tree[oi] = old_order;
                }
                else {
                        p->tree[oi] = MAX(lorder, rorder);
                }
        }

        return rslt;
}

void buddy_free(intptr_t id, void *ptr)
{
        struct buddy_pool *p = (struct buddy_pool *)id;

        if(NULL == p) {
                RPT(RPT_ERR, "free: bad id");
                return;
        }
        if(NULL == p->tree) {
                RPT(RPT_ERR, "free: bad tree");
                return;
        }
        if(NULL == p->pool) {
                RPT(RPT_ERR, "free: bad pool");
                return;
        }
        if(NULL == ptr) {
                RPT(RPT_ERR, "free: bad ptr");
                return;
        }

        /* determine offset, then search aim node in the tree */
        size_t offset = (uint8_t *)ptr - p->pool;
        if(offset < 0 || offset >= p->size) {
                RPT(RPT_ERR, "free: bad ptr: %p, out of pool", ptr);
                return;
        }

        size_t order = p->omin;
        size_t i = 0; /* index of binary tree array */
        int found = 0;
        while(offset % (1<<order) == 0) {
                i = (1<<(p->omax - order)) - 1 + offset / (1<<order);
                if(0 == p->tree[i]) {
                        found = 1;
                        break;
                }
                order++;
        }
        if(!found) {
                RPT(RPT_ERR, "free: bad ptr: %p, illegal node or module bug", ptr);
                return;
        }

        /* modify tree */
        size_t lorder;
        size_t rorder;
        RPT(RPT_DBG, "free:    @ %p %zX", ptr, (size_t)1 << order);
        p->tree[i] = order;
        while(i) {
                i = PARENT(i) ;
                order++;

                lorder = p->tree[LSUBTREE(i)];
                rorder = p->tree[RSUBTREE(i)];
                if(lorder == (order - 1) &&
                   rorder == (order - 1)) {
                        p->tree[i] = order;
                }
                else {
                        p->tree[i] = MAX(lorder, rorder);
                }
        }

        return;
}

/* get the smallest order to cover the size */
static size_t smallest_order(size_t size)
{
        size_t order;
        size_t mask;

        for(order = 0, mask = 1; mask < size; order++, mask <<= 1) {};
        return order;
}
