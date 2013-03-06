/* vim: set tabstop=8 shiftwidth=8:
 * name: mpool.h
 * funx: memory pool module, to avoid malloc and free from OS frequently
 */

#ifndef _MPOOL_H
#define _MPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

int mp_create(size_t NBYTES);
int mp_init(int id);
void *mp_alloc(int id, size_t NBYTES);
void mp_free(int id, void *APTR); /* fake functio, memory pool never free memorys */
int mp_status(int id, size_t *used, size_t *left); /* report memory pool status */
int mp_destroy(int id);

#ifdef __cplusplus
}
#endif

#endif /* _MPOOL_H */
