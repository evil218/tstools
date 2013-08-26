/* vim: set tabstop=8 shiftwidth=8:
 * name: buddy.h
 * funx: buddy memory pool, to avoid malloc and free from OS frequently
 *
 * tree vs pool:
 *
 *         7          6             0
 *       6   6      0   6         0   0
 *      5 5 5 5    5 5 5 5       5 5 5 5
 *
 *       init     (2^6)-byte   2x(2^6)-byte
 *      status     allocted      allocted
 *
 * 2013-03-09, ZHOU Cheng, modularized
 * 2012-11-02, manuscola.bean@gmail.com, optimized from https://github.com/wuwenbin/buddy2
 */

#ifndef _BUDDY_H
#define _BUDDY_H

#ifdef __cplusplus
extern "C" {
#endif

#define BUDDY_ORDER_MAX (8 * sizeof(size_t))

/*@only@*/
/*@null@*/
void *buddy_create(size_t order_max, size_t order_min);
int buddy_destroy(/*@only@*/ /*@null@*/ void *id);
int buddy_init(void *id);
int buddy_status(void *id, int enable, const char *hint); /* for debug */

/*@dependent@*/
/*@null@*/
void *buddy_malloc(void *id, size_t size);
/*@dependent@*/
/*@null@*/
void *buddy_realloc(void *id, void *ptr, size_t size); /* need to be test */
void buddy_free(/*@null@*/ void *id, /*@dependent@*/ /*@null@*/ void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* _BUDDY_H */
