/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memcpy */
#include <pthread.h>
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
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define POW2(x) ((size_t)1 << (x))

#define FBTL(idx) (((idx) << 1) + 1)        /* full binary tree left subnode index */
#define FBTR(idx) (((idx) << 1) + 2)        /* full binary tree right subnode index */
#define FBTP(idx) ((((idx) + 1) >> 1) - 1)  /* full binary tree parent node index */

/* note: if buddy_obj is OK, we trust tree and pool pointer below, and do not check them */
struct buddy_obj
{
        pthread_mutex_t mux;
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

struct node {
        uint8_t order; /* value of binary tree node */
        size_t size; /* POW2(order) */
        size_t offset; /* pointer offset from p->pool */
        uint8_t *ptr; /* p->pool + offset, NULL means bad node */
        size_t index; /* index of binary tree array */
};

static void report(struct buddy_obj *p, size_t i, uint8_t order, size_t *acc);
static void init_tree(struct buddy_obj *p);
static int siz2nod(struct buddy_obj *p, size_t size, struct node *nod);
static int ptr2nod(struct buddy_obj *p, uint8_t *ptr, struct node *nod);
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
        p->pool_size = POW2(p->maxo);
        p->tree_size = POW2(p->maxo - p->mino + 1) - 1; /* minus 1 is important */

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
               p->pool_size, p->pool, POW2(p->mino));

        (void)pthread_mutex_init(&p->mux, NULL);
        (void)pthread_mutex_lock(&p->mux);
        init_tree(p); /* FIXME: tree and pool is OK now, why splint warning me here? */
        (void)pthread_mutex_unlock(&p->mux);
        return p;
}

int buddy_destroy(void *id)
{
        struct buddy_obj *p = (struct buddy_obj *)id;

        if(NULL == p) {
                RPTERR("destroy: bad id");
                return -1;
        }

        (void)pthread_mutex_lock(&p->mux);
        free(p->tree);
        free(p->pool);
        (void)pthread_mutex_destroy(&p->mux);
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

        (void)pthread_mutex_lock(&p->mux);
        init_tree(p);
        (void)pthread_mutex_unlock(&p->mux);
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

        (void)pthread_mutex_lock(&p->mux);
        p->level = level;
        acc = 0;
        report(p, 0, p->maxo, &acc);
        (void)pthread_mutex_unlock(&p->mux);

        fprintf(stderr, "%s", (BUDDY_REPORT_TOTAL == level && 0 != acc) ? "\n" : "");
        fprintf(stderr, "buddy: (%zu / %zu) used: %s\n",
                acc, p->pool_size, ((NULL == hint) ? "" : hint));
        return 0;
}

/* The malloc() function allocates size bytes and returns a pointer to the allocated memory.
 * The memory is not initialized.
 * If size is 0, then malloc() returns NULL.
 */
void *buddy_malloc(void *id, size_t size)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
        struct node new;

        if(NULL == p) {
                RPTERR("malloc: bad id");
                return NULL;
        }

        if(0 == size) {
                RPTWRN("malloc: bad size: 0");
                return NULL;
        }

        (void)pthread_mutex_lock(&p->mux);
        siz2nod(p, size, &new);
        if(new.ptr) {
                allocate_node(p, new.index); /* modify parent node */
        }
        (void)pthread_mutex_unlock(&p->mux);

        if(!new.ptr) {
                return NULL;
        }

        RPTDBG("malloc:  @ 0x%zX, space: 0x%zX, size: 0x%zX",
               new.offset, POW2(new.order), size);
        return new.ptr;
}

/* The calloc() function allocates memory for an array of nmemb elements of size bytes each and
 * returns a pointer to the allocated memory.
 * The memory is set to zero.
 * If nmemb or size is 0, then calloc() returns NULL.
 */
void *buddy_calloc(void *id, size_t nmemb, size_t size)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
        struct node new;
        size_t total_size;

        if(NULL == p) {
                RPTERR("calloc: bad id");
                return NULL;
        }

        total_size = nmemb * size;
        if(0 == total_size) {
                RPTWRN("calloc: nmemb or size is 0");
                return NULL;
        }

        (void)pthread_mutex_lock(&p->mux);
        siz2nod(p, total_size, &new);
        if(new.ptr) {
                allocate_node(p, new.index); /* modify parent node */
                memset(new.ptr, 0, total_size); /* set to zero */
        }
        (void)pthread_mutex_unlock(&p->mux);

        if(!new.ptr) {
                return NULL;
        }

        RPTDBG("calloc:  @ 0x%zX, space: 0x%zX, size: 0x%zX * 0x%zX",
               new.offset, POW2(new.order), nmemb, size);
        return new.ptr;
}

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
        struct node old;
        struct node new;

        if(NULL == p) {
                RPTERR("realloc: bad id");
                return NULL;
        }

        if(NULL == ptr) {
                if(0 == size) {
                        RPTERR("realloc: ptr is NULL, size is 0");
                        return NULL;
                }

                /* malloc */
                (void)pthread_mutex_lock(&p->mux);
                siz2nod(p, size, &new);
                if(new.ptr) {
                        allocate_node(p, new.index); /* modify parent node */
                }
                (void)pthread_mutex_unlock(&p->mux);

                if(!new.ptr) {
                        return NULL;
                }

                RPTDBG("malloc:  @ 0x%zX, space: 0x%zX, size: 0x%zX",
                       new.offset, POW2(new.order), size);
                return new.ptr;
        }

        if(0 == size) {
                /* free */
                (void)pthread_mutex_lock(&p->mux);
                ptr2nod(p, (uint8_t *)ptr, &old);
                if(old.ptr) {
                        free_node(p, old.index, old.order); /* modify parent node */
                }
                (void)pthread_mutex_unlock(&p->mux);

                if(!old.ptr) {
                        return NULL;
                }

                RPTDBG("free:    @ 0x%zX, space: 0x%zX",
                       old.offset, POW2(old.order));
                return NULL;
        }

        /* realloc */
        (void)pthread_mutex_lock(&p->mux);
        ptr2nod(p, (uint8_t *)ptr, &old);
        siz2nod(p, size, &new);
        if(old.ptr &&
           new.ptr &&
           new.order != old.order) {
                /* modify parent node */
                allocate_node(p, new.index);
                free_node(p, old.index, old.order);

                /* copy data */
                memcpy(new.ptr, ptr, POW2(MIN(old.order, new.order))); /* FIXME: memcpy() better than memmove() here */
        }
        (void)pthread_mutex_unlock(&p->mux);

        if(!old.ptr ||
           !new.ptr) {
                return NULL;
        }
        if(new.order == old.order) {
                /* do not need to realloc */
                return ptr;
        }

        RPTDBG("realloc: @ 0x%zX, space: 0x%zX -> @ 0x%zX, space: 0x%zX, size: 0x%zX",
               old.offset, POW2(old.order),
               new.offset, POW2(new.order), size);
        return new.ptr;
}

/* The free() function frees the memory space pointed to by ptr,
 * which must have been returned by a previous call to malloc(), calloc() or realloc().
 * Otherwise, or if free(ptr) has already been called before, undefined behavior occurs.
 * If ptr is NULL, no operation is performed.
 */
void buddy_free(void *id, void *ptr)
{
        struct buddy_obj *p = (struct buddy_obj *)id;
        struct node old;

        if(NULL == p) {
                RPTERR("free: bad id");
                return;
        }
        if(NULL == ptr) {
                RPTWRN("free: empty ptr, do nothing");
                return;
        }

        (void)pthread_mutex_lock(&p->mux);
        ptr2nod(p, (uint8_t *)ptr, &old);
        if(old.ptr) {
                free_node(p, old.index, old.order); /* modify parent node */
        }
        (void)pthread_mutex_unlock(&p->mux);

        if(!old.ptr) {
                return;
        }

        RPTDBG("free:    @ 0x%zX, space: 0x%zX", old.offset, POW2(old.order));
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
                *acc += POW2(order);

                if(BUDDY_REPORT_TOTAL == p->level) {
                        fprintf(stderr, "%u ", (unsigned int)order);
                }
                else if(BUDDY_REPORT_DETAIL == p->level) {
                        p->offset = ((i + 1) << order) - p->pool_size;
                        fprintf(stderr, "%3u: 0x%zX: ", (unsigned int)order, p->offset);

                        p->buf = p->pool + p->offset;
                        for(p->x = 0; p->x < POW2(order); p->x++) {
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
 */
static int siz2nod(struct buddy_obj *p, size_t size, struct node *nod)
{
        uint8_t order; /* temp order */

        nod->ptr = NULL;

        /* the smallest order to cover the size */
        nod->order = p->mino;
        nod->size = POW2(nod->order);
        while(nod->size < size) {
                nod->order++;
                nod->size <<= 1;
        }
        if(nod->order > p->tree[0]) {
                RPTERR("not enough space in pool for 0x%zX-byte", size);
                return -1;
        }

        /* order to index */
        nod->index = 0; /* from root */
        for(order = p->maxo; order > nod->order; order--) {
                if(p->tree[FBTL(nod->index)] >= nod->order) {
                        nod->index = FBTL(nod->index);
                }
                else {
                        nod->index = FBTR(nod->index);
                }
        }

        /* index to offset */
        nod->offset = ((nod->index + 1) << nod->order) - p->pool_size;
        nod->ptr = p->pool + nod->offset;
        return 0;
}

/*
 * ptr -> offset -+-> index
 *                +-> order
 */
static int ptr2nod(struct buddy_obj *p, uint8_t *ptr, struct node *nod)
{
        nod->ptr = NULL;

        /* determine offset */
        if(ptr < p->pool) {
                RPTERR("bad ptr: %p, before pool(%p + %zu)", ptr, p->pool, p->pool_size);
                return -1;
        }
        nod->offset = (size_t)(ptr - p->pool);
        if(nod->offset >= p->pool_size) {
                RPTERR("bad ptr: %p, after pool(%p + %zu)", ptr, p->pool, p->pool_size);
                return -1;
        }

        /* offset to (index and order) */
        /*      9
         *      8       8
         *      7   7   7   7
         *      6 6 6 6 6 6 6 6
         */
        nod->order = p->mino;
        while(nod->offset == ((nod->offset >> nod->order) << nod->order)) { /* possible order */
                nod->index = ((p->pool_size + nod->offset) >> nod->order) - 1;
                if(0 == p->tree[nod->index]) {
                        nod->size = POW2(nod->order);
                        nod->ptr = ptr;
                        return 0; /* fine the node and the order */
                }
                nod->order++; /* maybe bigger space */
        }
        RPTERR("bad ptr: %p, illegal node in pool", ptr);
        return -1;
}

/* modify tree to allocate the node */
static void allocate_node(struct buddy_obj *p, size_t i)
{
        uint8_t ol; /* left order */
        uint8_t or; /* right order */

        p->tree[i] = 0; /* means it is allocated */
        while(0 != i) {
                i = FBTP(i);
                ol = p->tree[FBTL(i)];
                or = p->tree[FBTR(i)];
                p->tree[i] = MAX(ol, or);
        }
}

/* modify tree to free the node */
static void free_node(struct buddy_obj *p, size_t i, uint8_t order)
{
        uint8_t ol; /* left order */
        uint8_t or; /* right order */

        p->tree[i] = order; /* means it is freed */
        while(0 != i) {
                i = FBTP(i);
                order++;

                ol = p->tree[FBTL(i)];
                or = p->tree[FBTR(i)];
                if(ol == (order - 1) &&
                   or == (order - 1)) {
                        p->tree[i] = order; /* merge */
                }
                else {
                        p->tree[i] = MAX(ol, or);
                }
        }
}
