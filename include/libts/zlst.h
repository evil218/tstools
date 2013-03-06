/* vim: set tabstop=8 shiftwidth=8:
 * name: zlst
 * funx: Common Bidirection List
 *
 *   *xxxx0;     (HEAD)    _____________     (TAIL)
 *        |     _______   |   _______   |   _______ 
 *        *--->|       |  |  |       |  |  |       |
 *             | tail ----*  | tail  |  |  | tail  |
 *             | next ------>| next ----*->| next --->NULL
 *      NULL<--- prev  |<----- prev  |<----- prev  |
 *             |  key  |     |  key  |     |  key  |
 *             | name  |     | name  |     | name  |
 *   unshift==>|       |     |       |     |       |==>pop
 *     shift<==|   *   |     |   *   |     |   *   |<==push
 *             |_______|     |_______|     |_______|
 *               znode         znode         znode  
 * 
 *       empty  list: *xxxx0 == NULL;
 *       normal list: *xxxx0 == HEAD;
 *
 *       LIFO: stack: push & pop
 *       FIFO: queue: push & shift
 *
 * 2009-05-08, ZHOU Cheng, Init for tstools(referred to the lstLib of VxWorks)
 * 2011-09-18, ZHOU Cheng, Modified for param_xml module
 */

#ifndef _ZLST_H
#define _ZLST_H

#ifdef __cplusplus
extern "C" {
#endif

struct znode { /* list node */
        struct znode *next;
        struct znode *prev;
        struct znode *tail; /* only head->tail is valid! */
        int key; /* for sort list: use zlst_set_key() before zlst_insert()! */
        const char *name; /* for variable type list: use zlst_set_name() before zlst_push()! */
};

/* note: PHEAD will be convert to (struct znode **) type! */
/* note: LNODE will be convert to (struct znode  *) type! */
void *zlst_delete(void *PHEAD, void *LNODE); /* It's up to the caller to free the node! */
void zlst_push(void *PHEAD, void *LNODE);
void zlst_unshift(void *PHEAD, void *LNODE);
void *zlst_pop(void *PHEAD); /* It's up to the caller to free the node! */
void *zlst_shift(void *PHEAD); /* It's up to the caller to free the node! */
void *zlst_insert(void *PHEAD, void *LNODE); /* small key first, it's up to the caller to free the node! */

void *zlst_search(void *PHEAD, int key);
void zlst_set_key(void *LNODE, int key);
void zlst_set_name(void *LNODE, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* _ZLST_H */
