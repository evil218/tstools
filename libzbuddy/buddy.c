/* vim: set tabstop=8 shiftwidth=8: */
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

#define FBTL(idx) (((idx) << 1) + 1)        /* full binary tree left subnode index */
#define FBTR(idx) (((idx) << 1) + 2)        /* full binary tree right subnode index */
#define FBTP(idx) ((((idx) + 1) >> 1) - 1)  /* full binary tree parent node index */

struct buddy_obj
{
        uint8_t omax; /* max order */
        uint8_t omin; /* min order */
        size_t tree_size; /* tree size */
        /*@null@*/ uint8_t *tree; /* binary tree, the array to describe the status of pool */
        size_t pool_size; /* pool size */
        /*@null@*/ uint8_t *pool; /* pool buffer */
};

static void report(struct buddy_obj *p, size_t i, size_t order, size_t *acc, int level);
static size_t smallest_order(size_t size);

void *buddy_create(int order_max, int order_min)
{
        struct buddy_obj *p;

        if(order_max > BUDDY_ORDER_MAX) {
                RPTERR("create: bad order_max: %d > %d", order_max, BUDDY_ORDER_MAX);
                return NULL; /* failed */
        }

        if(order_min >= order_max) {
                RPTERR("create: bad order_min: %d >= %d", order_min, order_max);
                return NULL; /* failed */
        }

        if(order_min < 1) {
                RPTERR("create: bad order: %d < 1", order_min);
                return NULL; /* failed */
        }

        p = (struct buddy_obj *)malloc(sizeof(struct buddy_obj));
        if(NULL == p) {
                RPTERR("create: create buddy pool object failed");
                return NULL; /* failed */
        }

        p->omax = (uint8_t)order_max;
        p->omin = (uint8_t)order_min;
        p->pool_size = ((size_t)1 << (p->omax));
        p->tree_size = ((size_t)1 << (p->omax - p->omin + 1)) - 1;

        p->tree = (uint8_t *)malloc(p->tree_size); /* FIXME: memalign? */
        if(NULL == p->tree) {
                RPTERR("create: malloc tree(%zu-byte) failed", p->tree_size);
                free(p);
                return NULL; /* failed */
        }
        RPTDBG("create: tree: %zX-byte @ %p", p->tree_size, p->tree);

        p->pool = (uint8_t *)malloc(p->pool_size); /* FIXME: memalign? */
        if(NULL == p->pool) {
                RPTERR("create: malloc pool(%zu-byte) failed", p->pool_size);
                free(p->tree);
                free(p);
                return NULL; /* failed */
        }
        RPTDBG("create: pool: %zX-byte @ %p, min space: %zX",
               p->pool_size, p->pool, (size_t)1 << p->omin);

        buddy_init(p);
        return p;
}

int buddy_destroy(void *id)
{
        struct buddy_obj *p;
        int rslt = 0;

        p = (struct buddy_obj *)id;
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
        struct buddy_obj *p;
        uint8_t *tree;
        size_t size;
        size_t order;

        p= (struct buddy_obj *)id;
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

int buddy_report(void *id, int level, const char *hint)
{
        struct buddy_obj *p;
        size_t acc;

        if(BUDDY_REPORT_NONE == level) {
                RPTINF("status: disable: do nothing");
                return 0;
        }

        p = (struct buddy_obj *)id;
        if(NULL == p) {
                RPTERR("status: bad id");
                return -1;
        }
        if(NULL == p->tree) {
                RPTERR("status: bad tree");
                return -1;
        }

        acc = 0;
        report(p, 0, p->omax, &acc, level); /* note: report do NOT check the 2ed parameter */

        fprintf(stderr, "%s", (BUDDY_REPORT_TOTAL == level && 0 != acc) ? "\n" : "");
        fprintf(stderr, "buddy: (%zu / %zu) used: %s\n",
                acc, (size_t)1 << p->omax, ((NULL == hint) ? "" : hint));
        return 0;
}

void *buddy_malloc(void *id, size_t size)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
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
                if(p->tree[FBTL(i)] >= order) {
                        i = FBTL(i);
                }
                else {
                        i = FBTR(i);
                }
        }
        p->tree[i] = 0; /* find the aim node */

        rslt = (p->pool + (i + 1) * (1<<order) - (p->pool_size));
        RPTDBG("malloc:  @ %p %zX %zX", rslt, (size_t)1 << order, size);

        /* modify parent order */
        while(0 != i) {
                i = FBTP(i);
                p->tree[i] = MAX(p->tree[FBTL(i)], p->tree[FBTR(i)]) ;
        }

        return rslt;
}

void *buddy_realloc(void *id, void *ptr, size_t size) /* FIXME: need to be test */
{
        struct buddy_obj *p = (struct buddy_obj *)id;
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

        /* determine offset */
        if((uint8_t *)ptr < p->pool) {
                RPTERR("realloc: bad ptr: %p(%p + %zu), before pool", ptr, p->pool, p->pool_size);
                return NULL;
        }
        offset = (size_t)((uint8_t *)ptr - p->pool); /* FIXME */
        if(offset >= p->pool_size) {
                RPTERR("realloc: bad ptr: %p(%p + %zu), after pool", ptr, p->pool, p->pool_size);
                return NULL;
        }

        /*      9
         *      8       8
         *      7   7   7   7
         *      6 6 6 6 6 6 6 6
         */
        /* search aim node in the tree */
        found = 0;
        old_order = p->omin;
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
                if(p->tree[FBTL(ni)] >= new_order) {
                        ni = FBTL(ni);
                }
                else {
                        ni = FBTR(ni);
                }
        }

        rslt = (p->pool + (ni + 1) * (1<<new_order) - (p->pool_size));
        RPTDBG("realloc: @ %p %zX %zX", rslt, (size_t)1 << new_order, size);

        /* modify tree for new */
        p->tree[ni] = 0;
        while(0 != ni) {
                ni = FBTP(ni);
                p->tree[ni] = MAX(p->tree[FBTL(ni)], p->tree[FBTR(ni)]) ;
        }

        /* copy data */
        memmove(rslt, ptr, (size_t)1 << old_order); /* FIXME: splint prohibit using memcpy here */

        /* modify tree for old */
        RPTDBG("realloc: @ %p %zX", ptr, (size_t)1 << old_order);
        p->tree[oi] = old_order;
        while(0 != oi) {
                oi = FBTP(oi) ;
                old_order++;

                lorder = p->tree[FBTL(oi)];
                rorder = p->tree[FBTR(oi)];
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

void buddy_free(void *id, void *ptr)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
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

        /* determine offset */
        if((uint8_t *)ptr < p->pool) {
                RPTERR("free: bad ptr: %p(%p + %zu), before pool", ptr, p->pool, p->pool_size);
                return;
        }
        offset = (size_t)((uint8_t *)ptr - p->pool); /* FIXME */
        if(offset >= p->pool_size) {
                RPTERR("free: bad ptr: %p(%p + %zu), after pool", ptr, p->pool, p->pool_size);
                return;
        }

        /*      9
         *      8       8
         *      7   7   7   7
         *      6 6 6 6 6 6 6 6
         */
        /* search aim node in the tree */
        found = 0;
        order = p->omin;
        while(offset % (1<<order) == 0) { /* possible order */
                i = (1<<(p->omax - order)) - 1 + offset / (1<<order); /* corresponding index */
                if(0 == p->tree[i]) {
                        found = 1;
                        break;
                }
                order++; /* maybe bigger space */
        }
        if(0 == found) {
                RPTERR("free: bad ptr: %p, illegal node or module bug", ptr);
                return;
        }

        /* modify tree */
        RPTDBG("free:    @ %p %zX", ptr, (size_t)1 << order);
        p->tree[i] = order;
        while(0 != i) {
                i = FBTP(i) ;
                order++;

                lorder = p->tree[FBTL(i)];
                rorder = p->tree[FBTR(i)];
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

static void report(struct buddy_obj *p, size_t i, size_t order, size_t *acc, int level)
{
        if(0 == p->tree[i] && /* order is zero */
           ((FBTL(i) >= p->tree_size) || /* no subtree */
            !(0 == p->tree[FBTL(i)] || 0 == p->tree[FBTR(i)]))) { /* not subtree allocated */
                /* allocated */
                *acc += ((size_t)1 << order);
                if(BUDDY_REPORT_TOTAL == level) {
                        fprintf(stderr, "%zu ", order);
                }
                else if(BUDDY_REPORT_DETAIL == level) {
                        uint8_t *buf;
                        size_t x;

                        buf = (p->pool + (i + 1) * (1<<order) - (p->pool_size));
                        fprintf(stderr, "%2zu: %p: ", order, buf);
                        for(x = 0; x <((size_t)1 << order); x++) {
                                fprintf(stderr, "%02X ", *buf++);
                        }
                        fprintf(stderr, "\n");
                }
        }
        else {
                if(FBTL(i) < p->tree_size) { /* to reduce recursion times observably */
                        report(p, FBTL(i), order - 1, acc, level);
                        report(p, FBTR(i), order - 1, acc, level);
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
