/* vim: set tabstop=8 shiftwidth=8:
 * name: buddy.h
 * funx: buddy memory pool, to avoid malloc and free from OS frequently
 *
 * tree vs pool:
 *
 *      6             5             5             5             6
 *      5   5         4   5         3   5         4   5         5   5
 *      4 4 4 4       3 4 4 4       3 0 4 4       4 0 4 4       4 4 4 4
 *      33333333      03333333      03333333      33333333      33333333
 *
 *       init        (2^3)-byte    (2^4)-byte    (2^3)-byte    (2^4)-byte
 *      status        allocted      allocted      free          free
 *
 * 2013-03-09, ZHOU Cheng, modularized
 * 2012-11-02, manuscola.bean@gmail.com, optimized from https://github.com/wuwenbin/buddy2
 */

#ifndef BUDDY_H
#define BUDDY_H

#ifdef __cplusplus
extern "C" {
#endif

#define BUDDY_ORDER_MAX (int)(8 * sizeof(size_t))

/* for 'level' parameter of buddy_report() */
#define BUDDY_REPORT_NONE       (0)
#define BUDDY_REPORT_TOTAL      (1)
#define BUDDY_REPORT_DETAIL     (2)

/*@null@*/ /*@only@*/ void *buddy_create(int order_max, int order_min);
int buddy_destroy(/*@null@*/ /*@only@*/ void *id);
int buddy_init(/*@null@*/ void *id); /* buddy_create() has buddy_init() function */
int buddy_report(/*@null@*/ void *id, int level, const char *hint); /* for debug */

/*@null@*/ /*@dependent@*/ void *buddy_malloc(/*@null@*/ void *id, size_t size);
/*@null@*/ /*@dependent@*/ void *buddy_realloc(/*@null@*/ void *id, void *ptr, size_t size);
/*@null@*/ /*@dependent@*/ void *buddy_calloc(/*@null@*/ void *id, size_t nmemb, size_t size);
void buddy_free(/*@null@*/ void *id, /*@null@*/ /*@dependent@*/ void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* BUDDY_H */
