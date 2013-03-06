/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "mpool.h"

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

/* memory pool structure:
 *
 *      -----------------------------------------
 *      A            A                           A
 *      +--head      +--next                     +--tail
 */
struct obj {
        uint8_t *head; /* 1st byte of memory pool, malloc from OS */
        uint8_t *next; /* next buffer to alloc */
        uint8_t *tail; /* next byte of memory pool tail */
};

int mp_create(size_t NBYTES)
{
        struct obj *p;
        
        p = (struct obj *)malloc(sizeof(struct obj));
        if(NULL == p)
        {
                RPT(RPT_ERR, "mp_create: create object failed");
                return (int)NULL;
        }

        p->head = (uint8_t *)malloc(NBYTES);
        if(NULL == p->head)
        {
                RPT(RPT_ERR, "mp_create: malloc %d-byte from OS failed", NBYTES);
                return (int)NULL;
        }

        p->tail = p->head + NBYTES;
        p->next = p->tail; /* to avoid use mp_alloc() before mp_init() */

        RPT(RPT_INF, "mp_create: malloc %d-byte from OS", NBYTES);
        return (int)p;
}

int mp_init(int id)
{
        struct obj *p = (struct obj *)id;

        if(NULL == p)
        {
                RPT(RPT_ERR, "mp_init: wrong id");
                return -1;
        }

        p->next = p->head;
        RPT(RPT_INF, "mp_init: %6.2f%%: %d = %d + %d",
            100.0 * (p->next - p->head) / (p->tail - p->head),
            p->tail - p->head,
            p->next - p->head,
            p->tail - p->next);
        return 0;
}

int mp_status(int id, size_t *used, size_t *left)
{
        struct obj *p = (struct obj *)id;

        if(NULL == p)
        {
                RPT(RPT_ERR, "mp_status: wrong id");
                return -1;
        }

        *used = p->next - p->head;
        *left = p->tail - p->next;
        return 0;
}

void *mp_alloc(int id, size_t NBYTES)
{
        struct obj *p = (struct obj *)id;
        uint8_t *rslt;
        size_t remain;

        if(NULL == p)
        {
                RPT(RPT_ERR, "mp_alloc: wrong id");
                return (void *)NULL;
        }

        remain = p->tail - p->next;
        if(NBYTES > remain) {
                RPT(RPT_ERR, "mp_alloc: alloc %d-byte failed, %d/%d-byte left",
                    NBYTES, remain, p->tail - p->head);
                return (void *)NULL;
        }

        rslt = p->next;
        p->next += NBYTES;

        RPT(RPT_INF, "mp_alloc: %6.2f%%: %d = %d + %d, %d-byte allocated",
            100.0 * (p->next - p->head) / (p->tail - p->head),
            p->tail - p->head,
            p->next - p->head,
            p->tail - p->next,
            NBYTES);
        return (void *)rslt;
}

void mp_free(int id, void *APTR)
{
        struct obj *p = (struct obj *)id;

        if(NULL == p)
        {
                RPT(RPT_ERR, "mp_free: wrong id");
                return;
        }

        RPT(RPT_DBG, "mp_free: I am fake");
        return;
}

int mp_destroy(int id)
{
        struct obj *p = (struct obj *)id;

        if(NULL == p)
        {
                RPT(RPT_ERR, "mp_destory: wrong id");
                return -1;
        }

        if(p->head) {
                free(p->head);
                RPT(RPT_INF, "mp_destroy: free %d-byte to OS", p->tail - p->head);
        }
        free(p);
        return 0;
}
