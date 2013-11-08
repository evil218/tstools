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

#ifndef ZLST_H
#define ZLST_H

#ifdef __cplusplus
extern "C" {
#endif

/*@abstract@*/ struct znode {
        /*@null@*/ /*@owned@*/ struct znode *next;
        /*@null@*/ /*@dependent@*/ struct znode *prev;
        /*@null@*/ /*@dependent@*/ struct znode *tail; /* only head->tail is valid! */
        /*@null@*/ /*@dependent@*/ const char *name; /* for variable type list: use zlst_set_name() to assign */
        int key; /* for sort list: use zlst_insert()! */
};

typedef /*@null@*/ /*@owned@*/ struct znode *zhead_t; /* point to the head of a list */

/* note:
 *      PHEAD will be convert to (struct znode **) type
 *      ZNODE will be convert to (struct znode  *) type
 *      It's up to the caller to free the valid return node(zlst do NOT known your free function)
 */
/*@null@*/ /*@owned@*/ /*@observer@*/ void *zlst_push(/*@null@*/ zhead_t *PHEAD, /*@null@*/ /*@owned@*/ void *ZNODE);
/*@null@*/ /*@owned@*/ /*@observer@*/ void *zlst_unshift(/*@null@*/ zhead_t *PHEAD, /*@null@*/ /*@owned@*/ void *ZNODE);
/*@null@*/ /*@owned@*/ void *zlst_pop(/*@null@*/ zhead_t *PHEAD);
/*@null@*/ /*@owned@*/ void *zlst_shift(/*@null@*/ zhead_t *PHEAD);
/*@null@*/ /*@owned@*/ /*@observer@*/ void *zlst_insert(/*@null@*/ zhead_t *PHEAD, /*@null@*/ /*@owned@*/ void *ZNODE, int key); /* small key first */
/*@null@*/ /*@owned@*/ void *zlst_delete(/*@null@*/ zhead_t *PHEAD, /*@null@*/ /*@dependent@*/ void *ZNODE);

/*@null@*/ /*@dependent@*/ void *zlst_search(/*@null@*/ zhead_t *PHEAD, int key);
void zlst_set_key(/*@null@*/ void *ZNODE, int key);
void zlst_set_name(/*@null@*/ void *ZNODE, const /*@null@*/ /*@dependent@*/ char *name);

#ifdef __cplusplus
}
#endif

#endif /* ZLST_H */
