/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* for uintN_t, etc */

#include "common.h"
#include "buddy.h"

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define IS_POWER_OF_2(x) (!((x) & ((x) - 1)))

#define LSUBTREE(index) (((index) << 1) + 1)
#define RSUBTREE(index) (((index) << 1) + 2)
#define PARENT(index)   ((((index) + 1) >> 1) - 1)

struct buddy_pool
{
        int omax; /* max order */
        int omin; /* min order */
        uint8_t *tree; /* binary tree, the array to describe the status of pool */
        size_t size; /* pool size: (1 << omax) */
        uint8_t *pool; /* pool buffer */
};

static int smallest_order(size_t size);

intptr_t buddy_create(int order_max, int order_min)
{
        if(order_max > BUDDY_ORDER_MAX) {
                RPT(RPT_ERR, "create: bad order_max: %d > %d", order_max, BUDDY_ORDER_MAX);
                return 0; /* failed */
        }

        if(order_min >= order_max) {
                RPT(RPT_ERR, "create: bad order_min: %d >= %d", order_min, order_max);
                return 0; /* failed */
        }

        if(order_min < 1 || order_max < 1) {
                RPT(RPT_ERR, "create: bad order: %d < 1 or %d < 1", order_min, order_max);
                return 0; /* failed */
        }

        struct buddy_pool *p = (struct buddy_pool *)malloc(sizeof(struct buddy_pool));
        if(NULL == p) {
                RPT(RPT_ERR, "create: create buddy pool object failed");
                return 0; /* failed */
        }

        p->omax = order_max;
        p->omin = order_min;
        p->size = (1 << order_max);

        size_t tree_size = (1 << (p->omax - p->omin + 1)) - 1;
        p->tree = (uint8_t *)malloc(tree_size); /* FIXME: memalign? */
        if(NULL == p->tree) {
                RPT(RPT_ERR, "create: malloc tree(%d-byte) failed", tree_size);
                free(p);
                return 0; /* failed */
        }
        RPT(RPT_DBG, "create: tree: %8lX-byte @ %8lX", (unsigned long)tree_size, (unsigned long)(p->tree));

        p->pool = (uint8_t *)malloc(p->size); /* FIXME: memalign? */
        if(NULL == p->pool) {
                RPT(RPT_ERR, "create: malloc pool(%d-byte) failed", p->size);
                free(p->tree);
                free(p);
                return 0; /* failed */
        }
        RPT(RPT_DBG, "create: pool: %8lX-byte @ %8lX", (unsigned long)p->size, (unsigned long)(p->pool));

        p->tree[0] = 0; /* to avoid use malloc() before init() */
        return (intptr_t)p;
}

int buddy_destroy(intptr_t id)
{
        struct buddy_pool *p = (struct buddy_pool *)id;

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

        if(RPT_DBG == rpt_lvl) {
                /* show tree(pool status):
                 *      level   tree_array
                 *        0     0
                 *        1     1 2
                 *        2     3 4 5 6
                 *        3     7 8 9 A B C D E
                 *        .     . . .
                 */
                for(int level = -1, i = 0; i < 63; i++) {
                        if(IS_POWER_OF_2(i + 1)) {
                                fprintf(stderr,"\nlevel (%d):", ++level);
                        }
                        fprintf(stderr," %2d", p->tree[i]);
                }
                fprintf(stderr,"\n");
        }

        free(p->pool);
        free(p->tree);
        free(p);
        return 0;
}

int buddy_init(intptr_t id)
{
        struct buddy_pool *p = (struct buddy_pool *)id;

        if(NULL == p) {
                RPT(RPT_ERR, "init: bad id");
                return -1;
        }
        if(NULL == p->tree) {
                RPT(RPT_ERR, "init: bad tree");
                return -1;
        }

        size_t tree_size = (1 << (p->omax - p->omin + 1)) - 1;
        for(int order = p->omax + 1, i = 0; i < tree_size; i++) {
                if(IS_POWER_OF_2(i + 1)) {
                        order--;
                }
                p->tree[i] = order;
        }
        return 0;
}

void *buddy_malloc(intptr_t id, size_t NBYTES)
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

        if(NBYTES <= 0) {
                RPT(RPT_ERR, "malloc: bad NBYTES: %d", NBYTES);
                return NULL;
        }

        /* determine aim order in tree */
        int order = MAX(smallest_order(NBYTES), p->omin);
        if((int)(p->tree[0]) < order) {
                RPT(RPT_ERR, "malloc: not enough space in pool");
                return NULL;
        }

        /* search aim node in the tree */
        int i = 0; /* index of binary tree array */
        for(int current_order = p->omax; current_order > order; current_order--) {
                if(p->tree[LSUBTREE(i)] >= order) {
                        i = LSUBTREE(i);
                }
                else {
                        i = RSUBTREE(i);
                }
        }
        p->tree[i] = 0; /* find the aim node */

        uint8_t *rslt = (p->pool + (i + 1) * (1<<order) - (p->size));
        RPT(RPT_DBG, "malloc:  got %8lX/%8lX @ %8lX from pool",
            (unsigned long)NBYTES, (unsigned long)1<<order, (unsigned long)rslt);

        /* modify parent order */
        while(i) {
                i = PARENT(i);
                p->tree[i] = MAX(p->tree[LSUBTREE(i)], p->tree[RSUBTREE(i)]) ;
        }

        return rslt;
}

void buddy_free(intptr_t id, void *APTR)
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
        if(NULL == APTR) {
                RPT(RPT_ERR, "free: bad APTR");
                return;
        }

        /* determine offset */
        size_t offset = (uint8_t *)APTR - p->pool;
        if(offset < 0 || offset >= p->size) {
                RPT(RPT_ERR, "free: bad APTR: %lX", (unsigned long)APTR);
                return;
        }

        /* search aim node in the tree */
        int order = p->omin;
        int i = 0; /* index of binary tree array */
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
                RPT(RPT_ERR, "free: bad APTR: %lX, illegal node or module bug", (unsigned long)APTR);
                return;
        }
        p->tree[i] = order;
        RPT(RPT_DBG, "free: return         /%8lX @ %8lX to pool",
            (unsigned long)1<<order, (unsigned long)APTR);

        /* modify parent order */
        int lorder;
        int rorder;
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
static int smallest_order(size_t size)
{
        int order;
        size_t mask;

        for(order = 0, mask = 1; mask < size; order++, mask <<= 1) {};
        return order;
}
