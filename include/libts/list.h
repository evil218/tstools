/* vim: set tabstop=8 shiftwidth=8:
 * name: list
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
 *               lnode         lnode         lnode  
 * 
 *       empty  list: *xxxx0 == NULL;
 *       normal list: *xxxx0 == HEAD;
 *
 *       LIFO: stack: push & pop
 *       FIFO: queue: push & shift
 *
 * 2009-05-08, ZHOU Cheng, Init for TStools(referred to the lstLib of VxWorks)
 * 2011-09-18, ZHOU Cheng, Modified for param_xml module
 */

#ifndef _LIST_H
#define _LIST_H

#ifdef __cplusplus
extern "C" {
#endif

struct lnode { /* list node */
        struct lnode *next;
        struct lnode *prev;
        struct lnode *tail; /* only head->tail is valid! */
        int key; /* for sort list: use list_set_key() before list_insert()! */
        const char *name; /* for variable type list: use list_set_name() before list_push()! */
};

/* note: PHEAD will be convert to (struct lnode **) type! */
/* note: LNODE will be convert to (struct lnode  *) type! */
void list_free(void *PHEAD);
void list_delete(void *PHEAD, void *LNODE);
void list_push(void *PHEAD, void *LNODE);
void list_unshift(void *PHEAD, void *LNODE);
void *list_pop(void *PHEAD);   /* It's up to the caller to free the lnode with free()! */
void *list_shift(void *PHEAD); /* It's up to the caller to free the lnode with free()! */
void list_insert(void *PHEAD, void *LNODE); /* small key first */

void *list_search(void *PHEAD, int key);
void list_set_key(void *LNODE, int key);
void list_set_name(void *LNODE, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* _LIST_H */
