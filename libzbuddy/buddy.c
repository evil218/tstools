/* vim: set tabstop=8 shiftwidth=8: */
/* for portability, use "%l" instead of "%z" for size_t in this file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memcpy */
#include <stdint.h> /* for uintN_t, etc */

#include "buddy.h"

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

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define IS_POWER_OF_2(x) ((0 == ((x) & ((x) - 1))) && (0 != (x)))

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

/*@null@*/ /*@only@*/ void *buddy_create(int order_max, int order_min)
{
        struct buddy_pool *p;
        size_t tree_size;

        if(order_max > BUDDY_ORDER_MAX) {
                RPTERR("create: bad order_max: %d > %d", order_max, BUDDY_ORDER_MAX);
                return NULL; /* failed */
        }

        if(order_min >= order_max) {
                RPTERR("create: bad order_min: %d >= %d", order_min, order_max);
                return NULL; /* failed */
        }

        if(order_min < 1 || order_max < 1) {
                RPTERR("create: bad order: %d < 1 or %d < 1", order_min, order_max);
                return NULL; /* failed */
        }

        p = (struct buddy_pool *)malloc(sizeof(struct buddy_pool));
        if(NULL == p) {
                RPTERR("create: create buddy pool object failed");
                return NULL; /* failed */
        }

        p->omax = (size_t)order_max;
        p->omin = (size_t)order_min;
        p->size = (size_t)1 << (size_t)order_max;

        tree_size = (1 << (p->omax - p->omin + 1)) - 1;
        p->tree = (uint8_t *)malloc(tree_size); /* FIXME: memalign? */
        if(NULL == p->tree) {
                RPTERR("create: malloc tree(%lu-byte) failed", (unsigned long)tree_size);
                free(p);
                return NULL; /* failed */
        }
        RPTDBG("create: tree: %8lX-byte @ %p", (unsigned long)tree_size, p->tree);

        p->pool = (uint8_t *)malloc(p->size); /* FIXME: memalign? */
        if(NULL == p->pool) {
                RPTERR("create: malloc pool(%lu-byte) failed", (unsigned long)p->size);
                free(p->tree);
                free(p);
                return NULL; /* failed */
        }
        RPTDBG("create: pool: %8lX-byte @ %p, min space: %lX", (unsigned long)p->size, p->pool, (unsigned long)1 << (size_t)order_min);

        p->tree[0] = 0; /* to avoid use malloc() before init() */
        return p;
}

int buddy_destroy(/*@null@*/ /*@only@*/ void *id)
{
        struct buddy_pool *p;
        int rslt = 0;

        p = (struct buddy_pool *)id;
        if(NULL == p) {
                RPTERR("destroy: bad id");
                rslt = -1;
                return rslt;
        }

        if(NULL == p->tree) {
                RPTERR("destroy: bad tree");
                rslt = -2;
        }
        else {
                free(p->tree);
        }

        if(NULL == p->pool) {
                RPTERR("destroy: bad pool");
                rslt = -3;
        }
        else {
                free(p->pool);
        }

        free(p);
        return rslt;
}

int buddy_init(void *id)
{
        struct buddy_pool *p;
        uint8_t *tree;
        size_t size;
        size_t order;

        p= (struct buddy_pool *)id;
        if(NULL == p) {
                RPTERR("init: bad id");
                return -1;
        }
        if(NULL == p->tree) {
                RPTERR("init: bad tree");
                return -1;
        }

        /* init p->tree */
        size = (size_t)1;
        tree = p->tree;
        for(order = p->omax; order >= p->omin; order--) {
                memset(tree, (int)order, size);
                tree += size;
                size <<= 1;
        }
        return 0;
}

int buddy_status(void *id, int enable, const char *hint)
{
        struct buddy_pool *p;
        size_t order;
        size_t tree_size;
        size_t cnt;
        size_t acc;
        size_t i;

        p = (struct buddy_pool *)id;
        if(NULL == p) {
                RPTERR("status: bad id");
                return -1;
        }
        if(NULL == p->tree) {
                RPTERR("status: bad tree");
                return -1;
        }
        if(0 == enable) {
                RPTINF("status: disable: do nothing");
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
                        if((LSUBTREE(i) >= tree_size) || /* no subtree */
                           !(0 == p->tree[LSUBTREE(i)] || 0 == p->tree[RSUBTREE(i)])) { /* not subtree allocated */
                                /* allocated */
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
                if(IS_POWER_OF_2(i + 2) && (0 != cnt)) {
                        fprintf(stderr,"%3lu x 0x%lX, ", (unsigned long)cnt, (unsigned long)1 << order);
#if 0
                        fprintf(stderr, "\n");
#endif
                }
        }
        fprintf(stderr,"(%lu / %lu) used", (unsigned long)acc, (unsigned long)1 << p->omax);
        fprintf(stderr,": %s\n", ((NULL == hint) ? "" : hint));
        return 0;
}

/*@null@*/ /*@dependent@*/ void *buddy_malloc(void *id, size_t size)
{
        struct buddy_pool *p = (struct buddy_pool *)id;
        size_t order;
        size_t i;
        size_t current_order;
        uint8_t *rslt;

        if(NULL == p) {
                RPTERR("malloc: bad id");
                return NULL;
        }
        if(NULL == p->tree) {
                RPTERR("malloc: bad tree");
                return NULL;
        }
        if(NULL == p->pool) {
                RPTERR("malloc: bad pool");
                return NULL;
        }

        if(0 == size) {
                RPTERR("malloc: bad size: 0");
                return NULL;
        }

        /* determine aim order in tree */
        order = MAX(smallest_order(size), p->omin);
        if((size_t)(p->tree[0]) < order) {
                RPTERR("malloc: not enough space in pool");
                return NULL;
        }

        /* search aim node in the tree */
        i = 0; /* index of binary tree array */
        for(current_order = p->omax; current_order > order; current_order--) {
                if(p->tree[LSUBTREE(i)] >= order) {
                        i = LSUBTREE(i);
                }
                else {
                        i = RSUBTREE(i);
                }
        }
        p->tree[i] = 0; /* find the aim node */

        rslt = (p->pool + (i + 1) * (1<<order) - (p->size));
        RPTDBG("malloc:  @ %p %lX %lX", rslt, (unsigned long)1 << order, (unsigned long)size);

        /* modify parent order */
        while(0 != i) {
                i = PARENT(i);
                p->tree[i] = MAX(p->tree[LSUBTREE(i)], p->tree[RSUBTREE(i)]) ;
        }

        return rslt;
}

/*@null@*/ /*@dependent@*/ void *buddy_realloc(void *id, void *ptr, size_t size) /* FIXME: need to be test */
{
        struct buddy_pool *p = (struct buddy_pool *)id;
        size_t offset;
        size_t old_order;
        size_t oi; /* index of binary tree array */
        int found;
        size_t new_order;
        size_t ni; /* index of binary tree array */
        size_t current_order;
        uint8_t *rslt;
        size_t lorder;
        size_t rorder;

        if(NULL == p) {
                RPTERR("realloc: bad id");
                return NULL;
        }
        if(NULL == p->tree) {
                RPTERR("realloc: bad tree");
                return NULL;
        }
        if(NULL == p->pool) {
                RPTERR("realloc: bad pool");
                return NULL;
        }
        if(NULL == ptr) {
                RPTERR("realloc: bad ptr");
                return NULL;
        }
        if(0 == size) {
                RPTERR("realloc: bad size: 0");
                return NULL;
        }

        /* determine offset, then search old node in the tree */
        if((uint8_t *)ptr < p->pool) {
                RPTERR("realloc: bad ptr: %p(%p + %lu), before pool", ptr, p->pool, (unsigned long)p->size);
                return NULL;
        }
        offset = (size_t)((uint8_t *)ptr - p->pool); /* FIXME */
        if(offset >= p->size) {
                RPTERR("realloc: bad ptr: %p(%p + %lu), after pool", ptr, p->pool, (unsigned long)p->size);
                return NULL;
        }

        old_order = p->omin;
        oi = 0;
        found = 0;
        while(offset % (1<<old_order) == 0) {
                oi = (1<<(p->omax - old_order)) - 1 + offset / (1<<old_order);
                if(0 == p->tree[oi]) {
                        found = 1;
                        break;
                }
                old_order++;
        }
        if(0 == found) {
                RPTERR("realloc: bad ptr: %p, illegal node or module bug", ptr);
                return NULL;
        }

        /* determine new_order in tree */
        new_order = MAX(smallest_order(size), p->omin);
        if((size_t)(p->tree[0]) < new_order) {
                RPTERR("realloc: not enough space in pool");
                return NULL;
        }

        /* maybe do not need to realloc */
        if(new_order <= old_order) {
                return ptr;
        }

        /* search new node in the tree */
        ni = 0; /* index of binary tree array */
        for(current_order = p->omax; current_order > new_order; current_order--) {
                if(p->tree[LSUBTREE(ni)] >= new_order) {
                        ni = LSUBTREE(ni);
                }
                else {
                        ni = RSUBTREE(ni);
                }
        }

        rslt = (p->pool + (ni + 1) * (1<<new_order) - (p->size));
        RPTDBG("realloc: @ %p %lX %lX", rslt, (unsigned long)1 << new_order, (unsigned long)size);

        /* modify tree for new */
        p->tree[ni] = 0;
        while(0 != ni) {
                ni = PARENT(ni);
                p->tree[ni] = MAX(p->tree[LSUBTREE(ni)], p->tree[RSUBTREE(ni)]) ;
        }

        /* copy data */
        memmove(rslt, ptr, ((size_t)1 << old_order)); /* FIXME: splint prohibit using memcpy here */

        /* modify tree for old */
        RPTDBG("realloc: @ %p %lX", ptr, (unsigned long)1 << old_order);
        p->tree[oi] = old_order;
        while(0 != oi) {
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

void buddy_free(/*@null@*/ void *id, /*@null@*/ /*@dependent@*/ void *ptr)
{
        struct buddy_pool *p = (struct buddy_pool *)id;
        size_t offset;
        size_t order;
        size_t i; /* index of binary tree array */
        int found;
        size_t lorder;
        size_t rorder;

        if(NULL == p) {
                RPTERR("free: bad id");
                return;
        }
        if(NULL == p->tree) {
                RPTERR("free: bad tree");
                return;
        }
        if(NULL == p->pool) {
                RPTERR("free: bad pool");
                return;
        }
        if(NULL == ptr) {
                RPTERR("free: bad ptr");
                return;
        }

        /* determine offset, then search aim node in the tree */
        if((uint8_t *)ptr < p->pool) {
                RPTERR("realloc: bad ptr: %p(%p + %lu), before pool", ptr, p->pool, (unsigned long)p->size);
                return;
        }
        offset = (size_t)((uint8_t *)ptr - p->pool); /* FIXME */
        if(offset >= p->size) {
                RPTERR("realloc: bad ptr: %p(%p + %lu), after pool", ptr, p->pool, (unsigned long)p->size);
                return;
        }

        order = p->omin;
        i = 0;
        found = 0;
        while(offset % (1<<order) == 0) {
                i = (1<<(p->omax - order)) - 1 + offset / (1<<order);
                if(0 == p->tree[i]) {
                        found = 1;
                        break;
                }
                order++;
        }
        if(0 == found) {
                RPTERR("free: bad ptr: %p, illegal node or module bug", ptr);
                return;
        }

        /* modify tree */
        RPTDBG("free:    @ %p %lX", ptr, (unsigned long)1 << order);
        p->tree[i] = order;
        while(0 != i) {
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

        for(order = 0, mask = (size_t)1; mask < size; order++, mask <<= 1) {};
        return order;
}
