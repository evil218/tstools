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

/* note: if buddy_obj is OK, we trust tree and pool pointer below, and do not check them */
struct buddy_obj
{
        uint8_t maxo; /* maximum order */
        uint8_t mino; /* minimum order */
        size_t tree_size; /* tree size */
        uint8_t *tree; /* binary tree, the array to describe the status of pool */
        size_t pool_size; /* pool size */
        uint8_t *pool; /* pool buffer */

        /* for efficiency of report() */
        int level;
        size_t offset;
        uint8_t *buf;
        size_t x;
};

static void report(struct buddy_obj *p, size_t i, uint8_t order_c, size_t *acc);
static void init_tree(struct buddy_obj *p);
static uint8_t smallest_order(size_t size);
static size_t order2index(struct buddy_obj *p, uint8_t order_n, /*@out@*/ size_t *offset);
static void allocate_node(struct buddy_obj *p, size_t i);
static size_t offset2index(struct buddy_obj *p, size_t offset, /*@out@*/ uint8_t *the_order);
static void free_node(struct buddy_obj *p, size_t i, uint8_t order_c);

void *buddy_create(int maxo, int mino)
{
        struct buddy_obj *p;

        if(!((1 <= mino) && (mino < maxo) && (maxo <= BUDDY_ORDER_MAX))) {
                RPTERR("create: !(1 <= %d < %d <= %d)", mino, maxo, BUDDY_ORDER_MAX);
                return NULL; /* failed */
        }

        p = (struct buddy_obj *)malloc(sizeof(struct buddy_obj));
        if(NULL == p) {
                RPTERR("create: create buddy pool object failed");
                return NULL; /* failed */
        }

        p->maxo = (uint8_t)maxo;
        p->mino = (uint8_t)mino;
        p->pool_size = ((size_t)1 << (p->maxo));
        p->tree_size = ((size_t)1 << (p->maxo - p->mino + 1)) - 1; /* minus 1 is important */

        p->tree = (uint8_t *)malloc(p->tree_size); /* FIXME: memalign? */
        if(NULL == p->tree) {
                RPTERR("create: malloc tree(%zu-byte) failed", p->tree_size);
                free(p);
                return NULL; /* failed */
        }
        RPTDBG("create: tree: 0x%zX-byte @ %p", p->tree_size, p->tree);

        p->pool = (uint8_t *)malloc(p->pool_size); /* FIXME: memalign? */
        if(NULL == p->pool) {
                RPTERR("create: malloc pool(%zu-byte) failed", p->pool_size);
                free(p->tree);
                free(p);
                return NULL; /* failed */
        }
        RPTDBG("create: pool: 0x%zX-byte @ %p, min space: 0x%zX",
               p->pool_size, p->pool, (size_t)1 << p->mino);

        init_tree(p); /* to splint: tree and pool is OK now, why warning me here? */
        return p;
}

int buddy_destroy(void *id)
{
        struct buddy_obj *p = (struct buddy_obj *)id;

        if(NULL == p) {
                RPTERR("destroy: bad id");
                return -1;
        }

        free(p->tree);
        free(p->pool);
        free(p);
        return 0;
}

int buddy_init(void *id)
{
        struct buddy_obj *p = (struct buddy_obj *)id;

        if(NULL == p) {
                RPTERR("init: bad id");
                return -1;
        }

        init_tree(p);
        return 0;
}

int buddy_report(void *id, int level, const char *hint)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
        size_t acc;

        if(BUDDY_REPORT_NONE == level) {
                return 0; /* do nothing */
        }

        if(NULL == p) {
                RPTERR("report: bad id");
                return -1;
        }
        p->level = level;

        acc = 0;
        report(p, 0, p->maxo, &acc);

        fprintf(stderr, "%s", (BUDDY_REPORT_TOTAL == level && 0 != acc) ? "\n" : "");
        fprintf(stderr, "buddy: (%zu / %zu) used: %s\n",
                acc, (size_t)1 << p->maxo, ((NULL == hint) ? "" : hint));
        return 0;
}

/* The malloc() function allocates size bytes and returns a pointer to the allocated memory.
 * The memory is not initialized.
 * If size is 0, then malloc() returns NULL.
 */
void *buddy_malloc(void *id, size_t size)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
        uint8_t order_n; /* new order */
        size_t i;
        size_t offset;

        if(NULL == p) {
                RPTERR("malloc: bad id");
                return NULL;
        }

        if(0 == size) {
                RPTWRN("malloc: bad size: 0");
                return NULL;
        }

        /* determine new order in tree */
        order_n = MAX(smallest_order(size), p->mino);
        if(p->tree[0] < order_n) {
                RPTERR("malloc: not enough space in pool");
                return NULL;
        }

        i = order2index(p, order_n, &offset);
        allocate_node(p, i); /* modify parent node */
        RPTDBG("malloc:  @ 0x%zX, space: 0x%zX, size: 0x%zX", offset, (size_t)1 << order_n, size);
        return (p->pool + offset);
}

/* The calloc() function allocates memory for an array of nmemb elements of size bytes each and
 * returns a pointer to the allocated memory.
 * The memory is set to zero.
 * If nmemb or size is 0, then calloc() returns either NULL,
 * or a unique pointer value that can later be successfully passed to free().
 */
#if 0
void *buddy_calloc(void *id, size_t nmemb, size_t size)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
}
#endif

/* The realloc() function changes the size of the memory block pointed to by ptr to size bytes.
 * The contents will be unchanged in the range from the start of the region up to the minimum of the old and new sizes.
 * If the new size is larger than the old size, the added memory will not be initialized.
 * If ptr is NULL, then the call is equivalent to malloc(size), for all values of size;
 * if size is equal to zero, and ptr is not NULL, then the call is equivalent to free(ptr).
 * Unless ptr is NULL, it must have been returned by an earlier call to malloc(), calloc() or realloc().
 * If the area pointed to was moved, a free(ptr) is done.
 */
void *buddy_realloc(void *id, void *ptr, size_t size)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
        size_t offset_o; /* old offset */
        uint8_t order_o; /* old order */
        size_t index_o; /* old index, index of binary tree array */
        size_t offset_n; /* new offset */
        uint8_t order_n; /* new order */
        size_t index_n; /* new index, index of binary tree array */
        uint8_t *rslt;

        if(NULL == p) {
                RPTERR("realloc: bad id");
                return NULL;
        }
        if(0 == size) {
                RPTERR("realloc: bad size: 0");
                return NULL;
        }
        if(NULL == ptr) {
                /* determine new order in tree */
                order_n = MAX(smallest_order(size), p->mino);
                if(p->tree[0] < order_n) {
                        RPTERR("malloc: not enough space in pool");
                        return NULL;
                }

                index_n = order2index(p, order_n, &offset_n);
                allocate_node(p, index_n); /* modify parent node */
                RPTDBG("malloc:  @ 0x%zX, space: 0x%zX, size: 0x%zX",
                       offset_n, (size_t)1 << order_n, size);

                return (p->pool + offset_n);
        }

        /* determine offset_o */
        if((uint8_t *)ptr < p->pool) {
                RPTERR("realloc: bad ptr: %p, before pool(%p + %zu)", ptr, p->pool, p->pool_size);
                return NULL;
        }
        offset_o = (size_t)((uint8_t *)ptr - p->pool); /* FIXME: Assignment of int to size_t */
        if(offset_o >= p->pool_size) {
                RPTERR("realloc: bad ptr: %p, after pool(%p + %zu)", ptr, p->pool, p->pool_size);
                return NULL;
        }

        index_o = offset2index(p, offset_o, &order_o);
        if(index_o == p->tree_size) {
                RPTERR("realloc: bad ptr: %p, illegal node or module bug", ptr);
                return NULL;
        }

        /* determine order_n in tree */
        order_n = MAX(smallest_order(size), p->mino);
        if(p->tree[0] < order_n) {
                RPTERR("realloc: not enough space in pool");
                return NULL;
        }

        /* maybe do not need to realloc */
        if(order_n <= order_o) {
                return ptr;
        }

        /* free old space */
        free_node(p, index_o, order_o); /* modify parent node */

        index_n = order2index(p, order_n, &offset_n);
        allocate_node(p, index_n); /* modify parent node */
        RPTDBG("realloc: @ 0x%zX, space: 0x%zX -> @ 0x%zX, space: 0x%zX, size: 0x%zX",
               offset_o, (size_t)1 << order_o,
               offset_n, (size_t)1 << order_n, size);

        /* copy data */
        rslt = p->pool + offset_n;
        memmove(rslt, ptr, (size_t)1 << order_o); /* FIXME: splint prohibit using memcpy here */

        return rslt;
}

/* The free() function frees the memory space pointed to by ptr,
 * which must have been returned by a previous call to malloc(), calloc() or realloc().
 * Otherwise, or if free(ptr) has already been called before, undefined behavior occurs.
 * If ptr is NULL, no operation is performed.
 */
void buddy_free(void *id, void *ptr)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
        size_t offset;
        uint8_t order_c; /* current order */
        size_t i; /* index of binary tree array */

        if(NULL == p) {
                RPTERR("free: bad id");
                return;
        }
        if(NULL == ptr) {
                RPTWRN("free: free empty ptr, do nothing");
                return;
        }

        /* determine offset */
        if((uint8_t *)ptr < p->pool) {
                RPTERR("free: bad ptr: %p, before pool(%p + %zu)", ptr, p->pool, p->pool_size);
                return;
        }
        offset = (size_t)((uint8_t *)ptr - p->pool); /* FIXME: assignment of int to size_t */
        if(offset >= p->pool_size) {
                RPTERR("free: bad ptr: %p, after pool(%p + %zu)", ptr, p->pool, p->pool_size);
                return;
        }

        i = offset2index(p, offset, &order_c);
        if(i == p->tree_size) {
                RPTERR("free: bad ptr: %p, illegal node or module bug", ptr);
                return;
        }

        RPTDBG("free:    @ 0x%zX, space: 0x%zX", offset, (size_t)1 << order_c);
        free_node(p, i, order_c); /* modify parent node */
        return;
}

/* recursion function:
 *      :-) speed: auto ignore subtree if current node is allocated
 *      :-( space: need many stack space if (maxo - mino) is big
 */
static void report(struct buddy_obj *p, size_t i, uint8_t order_c, size_t *acc)
{
        /* for efficiency:
         *      move local variables into buddy_obj;
         *      do NOT check parameter 'i';
         *      do NOT check wrong tree;
         *
         * cases:
         *      P L R | action     | memo
         *      ------+------------+-----------------
         *      0 - - | accumulate | allocated leaf
         *      0 0 0 | recursion  | branch
         *      0 l 0 | -          | wrong tree
         *      0 0 r | -          | wrong tree
         *      0 l r | accumulate | allocated branch
         *      p - - | return     | free leaf
         *      p ? ? | recursion  | branch
         */
        if((0 == p->tree[i]) &&
           ((FBTL(i) >= p->tree_size) || (0 != p->tree[FBTL(i)] && 0 != p->tree[FBTR(i)]))) {
                *acc += ((size_t)1 << order_c);

                if(BUDDY_REPORT_TOTAL == p->level) {
                        fprintf(stderr, "%u ", (unsigned int)order_c);
                }
                else if(BUDDY_REPORT_DETAIL == p->level) {
                        p->offset = (i + 1) * (1<<order_c) - p->pool_size;
                        fprintf(stderr, "%3u: 0x%zX: ", (unsigned int)order_c, p->offset);

                        p->buf = p->pool + p->offset;
                        for(p->x = 0; p->x <((size_t)1 << order_c); p->x++) {
                                fprintf(stderr, "%02X ", (unsigned int)*(p->buf)++);
                        }
                        fprintf(stderr, "\n");
                }
        }
        else { /* 0 != p->tree[i] */
                if(FBTL(i) < p->tree_size) {
                        report(p, FBTL(i), order_c - 1, acc);
                        report(p, FBTR(i), order_c - 1, acc);
                }
        }
        return;
}

static void init_tree(struct buddy_obj *p)
{
        uint8_t *tree = p->tree;
        size_t size;
        uint8_t order_c; /* current order */

        size = (size_t)1;
        for(order_c = p->maxo; order_c >= p->mino; order_c--) {
                memset(tree, (int)order_c, size);
                tree += size;
                size <<= 1;
        }
        return;
}

static uint8_t smallest_order(size_t size)
{
        uint8_t order_c; /* current order */
        size_t mask;

        order_c = 0;
        for(mask = (size_t)1; mask < size; mask <<= 1) {
                order_c++;
        }
        return order_c; /* the smallest order to cover the size */
}

static size_t order2index(struct buddy_obj *p, uint8_t order_n, size_t *offset)
{
        size_t i; /* index of binary tree array */
        uint8_t order_c; /* current order */

        i = 0; /* from root */
        for(order_c = p->maxo; order_c > order_n; order_c--) {
                if(p->tree[FBTL(i)] >= order_n) {
                        i = FBTL(i);
                }
                else {
                        i = FBTR(i);
                }
        }
        *offset = (i + 1) * (1<<order_n) - p->pool_size;
        return i;
}

static void allocate_node(struct buddy_obj *p, size_t i)
{
        p->tree[i] = 0; /* 0 means it is allocated */
        while(0 != i) {
                i = FBTP(i);
                p->tree[i] = MAX(p->tree[FBTL(i)], p->tree[FBTR(i)]) ;
        }
}

static size_t offset2index(struct buddy_obj *p, size_t offset, uint8_t *the_order)
{
        size_t i; /* index of binary tree array */
        uint8_t order_c; /* current order */

        /*      9
         *      8       8
         *      7   7   7   7
         *      6 6 6 6 6 6 6 6
         */
        order_c = p->mino;
        while(offset % (1<<order_c) == 0) { /* possible order */
                i = (1<<(p->maxo - order_c)) - 1 + offset / (1<<order_c); /* corresponding index */
                if(0 == p->tree[i]) {
                        *the_order = order_c;
                        return i; /* find the node */
                }
                order_c++; /* maybe bigger space */
        }

        *the_order = 0; /* for splint */
        return p->tree_size; /* offset2index failed */
}

static void free_node(struct buddy_obj *p, size_t i, uint8_t order_c)
{
        uint8_t order_l; /* left order */
        uint8_t order_r; /* right order */

        p->tree[i] = order_c; /* current order means it is free */
        while(0 != i) {
                i = FBTP(i);
                order_c++;

                order_l = p->tree[FBTL(i)];
                order_r = p->tree[FBTR(i)];
                if(order_l == (order_c - 1) &&
                   order_r == (order_c - 1)) {
                        p->tree[i] = order_c; /* merge */
                }
                else {
                        p->tree[i] = MAX(order_l, order_r);
                }
        }
}
