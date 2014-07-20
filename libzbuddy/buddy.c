/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memcpy */
//#include <pthread.h>
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
        //pthread_mutex_t mux;
        uint8_t maxo; /* maximum order */
        uint8_t mino; /* minimum order */
        size_t tree_size; /* tree size */
        uint8_t *tree; /* binary tree, the array to describe the status of pool */
        size_t pool_size; /* pool size */
        uint8_t *pool; /* pool buffer */

        /* for efficiency of report() */
        int level;
        size_t offset;
        /*@dependent@*/ uint8_t *buf;
        size_t x;
};

struct inf {
        uint8_t order; /* value of binary tree node */
        size_t index; /* index of binary tree array */
        size_t offset; /* pointer offset from p->pool */
};

static void report(struct buddy_obj *p, size_t i, uint8_t order, size_t *acc);
static void init_tree(struct buddy_obj *p);
static int size2inf(struct buddy_obj *p, size_t size, struct inf *inf);
static int ptr2inf(struct buddy_obj *p, uint8_t *ptr, struct inf *inf);

static void allocate_node(struct buddy_obj *p, size_t i);
static void free_node(struct buddy_obj *p, size_t i, uint8_t order);

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

        //(void)pthread_mutex_init(&p->mux, NULL);
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

        //(void)pthread_mutex_lock(&p->mux);
        free(p->tree);
        free(p->pool);
        //(void)pthread_mutex_destroy(&p->mux);
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

        //(void)pthread_mutex_lock(&p->mux);
        init_tree(p);
        //(void)pthread_mutex_unlock(&p->mux);
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

        //(void)pthread_mutex_lock(&p->mux);
        p->level = level;

        acc = 0;
        report(p, 0, p->maxo, &acc);

        fprintf(stderr, "%s", (BUDDY_REPORT_TOTAL == level && 0 != acc) ? "\n" : "");
        fprintf(stderr, "buddy: (%zu / %zu) used: %s\n",
                acc, (size_t)1 << p->maxo, ((NULL == hint) ? "" : hint));
        //(void)pthread_mutex_unlock(&p->mux);
        return 0;
}

/* The malloc() function allocates size bytes and returns a pointer to the allocated memory.
 * The memory is not initialized.
 * If size is 0, then malloc() returns NULL.
 */
void *buddy_malloc(void *id, size_t size)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
        struct inf new;

        if(NULL == p) {
                RPTERR("malloc: bad id");
                return NULL;
        }

        if(0 == size) {
                RPTWRN("malloc: bad size: 0");
                return NULL;
        }

        //(void)pthread_mutex_lock(&p->mux);
        if(0 != size2inf(p, size, &new)) {
                return NULL;
        }
        allocate_node(p, new.index); /* modify parent node */
        //(void)pthread_mutex_unlock(&p->mux);
        RPTDBG("malloc:  @ 0x%zX, space: 0x%zX, size: 0x%zX",
               new.offset, (size_t)1 << new.order, size);
        return (p->pool + new.offset);
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
        struct inf old;
        struct inf new;
        uint8_t *ptr_n;

        if(NULL == p) {
                RPTERR("realloc: bad id");
                return NULL;
        }
        //(void)pthread_mutex_unlock(&p->mux);
        if(0 == size) {
                RPTERR("realloc: bad size: 0");
                return NULL;
        }
        if(NULL == ptr) {
                if(0 != size2inf(p, size, &new)) {
                        return NULL;
                }
                allocate_node(p, new.index); /* modify parent node */
                RPTDBG("malloc:  @ 0x%zX, space: 0x%zX, size: 0x%zX",
                       new.offset, (size_t)1 << new.order, size);
                return (p->pool + new.offset);
        }

        if(0 != ptr2inf(p, (uint8_t *)ptr, &old)) {
                return NULL;
        }
        if(0 != size2inf(p, size, &new)) {
                return NULL;
        }

        /* maybe do not need to realloc */
        if(new.order <= old.order) {
                return ptr;
        }

        free_node(p, old.index, old.order); /* modify parent node */
        allocate_node(p, new.index); /* modify parent node */

        /* copy data */
        ptr_n = p->pool + new.offset;
        memmove(ptr_n, ptr, (size_t)1 << old.order); /* FIXME: splint prohibit using memcpy here */

        //(void)pthread_mutex_unlock(&p->mux);
        RPTDBG("realloc: @ 0x%zX, space: 0x%zX -> @ 0x%zX, space: 0x%zX, size: 0x%zX",
               old.offset, (size_t)1 << old.order,
               new.offset, (size_t)1 << new.order, size);
        return ptr_n;
}

/* The free() function frees the memory space pointed to by ptr,
 * which must have been returned by a previous call to malloc(), calloc() or realloc().
 * Otherwise, or if free(ptr) has already been called before, undefined behavior occurs.
 * If ptr is NULL, no operation is performed.
 */
void buddy_free(void *id, void *ptr)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
        struct inf old;

        if(NULL == p) {
                RPTERR("free: bad id");
                return;
        }
        if(NULL == ptr) {
                RPTWRN("free: free empty ptr, do nothing");
                return;
        }

        //(void)pthread_mutex_lock(&p->mux);
        if(0 != ptr2inf(p, (uint8_t *)ptr, &old)) {
                return;
        }
        free_node(p, old.index, old.order); /* modify parent node */
        //(void)pthread_mutex_unlock(&p->mux);
        RPTDBG("free:    @ 0x%zX, space: 0x%zX", old.offset, (size_t)1 << old.order);
        return;
}

/* recursion function:
 *      :-) speed: auto ignore subtree if current node is allocated
 *      :-( space: need many stack space if (maxo - mino) is big
 */
static void report(struct buddy_obj *p, size_t i, uint8_t order, size_t *acc)
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
                *acc += ((size_t)1 << order);

                if(BUDDY_REPORT_TOTAL == p->level) {
                        fprintf(stderr, "%u ", (unsigned int)order);
                }
                else if(BUDDY_REPORT_DETAIL == p->level) {
                        p->offset = (i + 1) * (1<<order) - p->pool_size;
                        fprintf(stderr, "%3u: 0x%zX: ", (unsigned int)order, p->offset);

                        p->buf = p->pool + p->offset;
                        for(p->x = 0; p->x <((size_t)1 << order); p->x++) {
                                fprintf(stderr, "%02X ", (unsigned int)*(p->buf)++);
                        }
                        fprintf(stderr, "\n");
                }
        }
        else { /* 0 != p->tree[i] */
                if(FBTL(i) < p->tree_size) {
                        report(p, FBTL(i), order - 1, acc);
                        report(p, FBTR(i), order - 1, acc);
                }
        }
        return;
}

static void init_tree(struct buddy_obj *p)
{
        uint8_t *tree = p->tree;
        size_t size;
        uint8_t order; /* current order */

        size = (size_t)1;
        for(order = p->maxo; order >= p->mino; order--) {
                memset(tree, (int)order, size);
                tree += size;
                size <<= 1;
        }
        return;
}

/*
 * size -> order -> index -> offset
 *                    +----> "tree"
 */
static int size2inf(struct buddy_obj *p, size_t size, struct inf *inf)
{
        size_t mask;
        uint8_t order; /* temp order */

        /* the smallest order to cover the size */
        inf->order = 0;
        for(mask = (size_t)1; mask < size; mask <<= 1) {
                inf->order++;
        }
        inf->order = MAX(inf->order, p->mino);
        if(inf->order > p->tree[0]) {
                RPTERR("not enough space in pool");
                return -1;
        }

        /* order to index */
        inf->index = 0; /* from root */
        for(order = p->maxo; order > inf->order; order--) {
                if(p->tree[FBTL(inf->index)] >= inf->order) {
                        inf->index = FBTL(inf->index);
                }
                else {
                        inf->index = FBTR(inf->index);
                }
        }

        /* index to offset */
        inf->offset = (inf->index + 1) * ((size_t)1 << inf->order) - p->pool_size;
        return 0;
}

/*
 * ptr -> offset ---> index ---> "tree"
 *                \-> order -/
 */
static int ptr2inf(struct buddy_obj *p, uint8_t *ptr, struct inf *inf)
{
        /* determine offset */
        if(ptr < p->pool) {
                RPTERR("bad ptr: %p, before pool(%p + %zu)", ptr, p->pool, p->pool_size);
                return -1;
        }
        inf->offset = (size_t)(ptr - p->pool);
        if(inf->offset >= p->pool_size) {
                RPTERR("bad ptr: %p, after pool(%p + %zu)", ptr, p->pool, p->pool_size);
                return -1;
        }

        /* offset to index and order */
        /*      9
         *      8       8
         *      7   7   7   7
         *      6 6 6 6 6 6 6 6
         */
        inf->order = p->mino;
        while(inf->offset % ((size_t)1 << inf->order) == 0) { /* possible order */
                inf->index = (1<<(p->maxo - inf->order)) - 1 + inf->offset / (1<<inf->order);
                if(0 == p->tree[inf->index]) {
                        return 0; /* fine the node and the order */
                }
                inf->order++; /* maybe bigger space */
        }
        RPTERR("bad ptr: %p, illegal node", ptr);
        return -1;
}

static void allocate_node(struct buddy_obj *p, size_t i)
{
        p->tree[i] = 0; /* means it is allocated */
        while(0 != i) {
                i = FBTP(i);
                p->tree[i] = MAX(p->tree[FBTL(i)], p->tree[FBTR(i)]) ;
        }
}

static void free_node(struct buddy_obj *p, size_t i, uint8_t order)
{
        uint8_t order_l; /* left order */
        uint8_t order_r; /* right order */

        p->tree[i] = order; /* means it is freed */
        while(0 != i) {
                i = FBTP(i);
                order++;

                order_l = p->tree[FBTL(i)];
                order_r = p->tree[FBTR(i)];
                if(order_l == (order - 1) &&
                   order_r == (order - 1)) {
                        p->tree[i] = order; /* merge */
                }
                else {
                        p->tree[i] = MAX(order_l, order_r);
                }
        }
}
