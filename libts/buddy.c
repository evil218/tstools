/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"
#include "buddy.h"

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#define IS_POWER_OF_2(x)  (!((x) & ((x) - 1)))
#define LEFT_LEAF(index)  ((index) * 2 + 1)
#define RIGHT_LEAF(index) ((index) * 2 + 2)
#define PARENT(index)     (((index) + 1) / 2 - 1)
#define MAX(a,b)          (((a) > (b)) ? (a) : (b))

struct buddy_pool
{
        uint8_t order_max;
        uint8_t order_min;
        size_t pool_size;
        uint8_t *bh;
        uint8_t *buffer;
};

static unsigned char next_order_of_2(int size);
static int show_status(intptr_t id);

intptr_t buddy_create(int order_max, int order_min)
{
        if(order_max > BUDDY_ORDER_MAX)
        {
                RPT(RPT_ERR, "create: bad order_max: %d > %d", order_max, BUDDY_ORDER_MAX);
                return 0; /* failed */
        }

        if(order_min >= order_max)
        {
                RPT(RPT_ERR, "create: bad order_min: %d >= %d", order_min, order_max);
                return 0; /* failed */
        }

        if(order_min < 1 || order_max < 1)
        {
                RPT(RPT_ERR, "create: bad order: %d < 1 or %d < 1", order_min, order_max);
                return 0; /* failed */
        }

        struct buddy_pool *p = malloc(sizeof(struct buddy_pool));
        if(NULL == p)
        {
                RPT(RPT_ERR, "create: create object failed");
                goto malloc_obj_failed;
        }

        p->order_max = order_max;
        p->order_min = order_min;
        p->pool_size = (1 << order_max);
        int bh_num = (1 << (order_max - order_min)) * 2 - 1;
        int ret = posix_memalign((void **)&(p->bh), 4096, bh_num);
        if(ret < 0 )
        {
                goto malloc_bh_failed;
        }

        ret = posix_memalign((void**)&(p->buffer), 4096, p->pool_size);
        if(ret < 0 )
        {
                goto malloc_buffer_failed;
        }

        buddy_init((intptr_t)p);

        return (intptr_t)p;

malloc_buffer_failed:
        free(p->bh);
malloc_bh_failed :
        free(p);
malloc_obj_failed:
        return 0; /* failed */
}

int buddy_destroy(intptr_t id)
{
        struct buddy_pool *p = (struct buddy_pool *)id;

        assert(p != NULL);
        assert(p->bh != NULL);
        assert(p->buffer != NULL);

        if(RPT_DBG == rpt_lvl) {
                show_status(id);
        }

        free(p->buffer);
        free(p->bh);
        free(p);
        return 0;
}

int buddy_init(intptr_t id)
{
        struct buddy_pool *p = (struct buddy_pool *)id;
        int bh_num = (1 << (p->order_max - p->order_min)) * 2 - 1;
        int current_order = p->order_max + 1;

        for(int i = 0; i < bh_num ; i++)
        {
                if(IS_POWER_OF_2(i + 1)) {
                        current_order--;
                }
                p->bh[i] = current_order;
        }
        return 0;
}

void *buddy_malloc(intptr_t id, size_t NBYTES)
{
        struct buddy_pool *p = (struct buddy_pool *)id;

        uint8_t *ret_ptr = NULL;
        if(NBYTES <= 0)
        {
                return NULL;
        }
        unsigned char order = next_order_of_2(NBYTES);
        if (order < p->order_min)
        {
                order = p->order_min;
        }
        int index = 0;
        if(p->bh[0] < order) /* have no enough space */
        {
                return NULL;
        }

        for(int current_order = p->order_max; current_order != order; current_order--)
        {
                if(p->bh[LEFT_LEAF(index)] >= order)
                        index = LEFT_LEAF(index);
                else
                        index = RIGHT_LEAF(index);
        }

        p->bh[index] = 0;
        ret_ptr = p->buffer + (index+1)*(1<<order)-(p->pool_size);
        while(index)
        {
                index = PARENT(index);
                p->bh[index] = MAX(p->bh[LEFT_LEAF(index)],p->bh[RIGHT_LEAF(index)]) ;
        }

        return ret_ptr;
}

void buddy_free(intptr_t id, void *APTR)
{
        struct buddy_pool *p = (struct buddy_pool *)id;

        assert(p != NULL && p->bh != NULL &&p->buffer != NULL && APTR != NULL) ;
        int offset = (uint8_t *)APTR - p->buffer;
        assert(offset >= 0 && offset < p->pool_size);

        int current_order = p->order_min;
        int current_index = 0;
        int found = 0;
        unsigned char left_order,right_order ;
        while(offset % (1<<current_order) == 0)
        {
                current_index =(1<<(p->order_max - current_order))-1 + offset/(1<<current_order);
                if(p->bh[current_index] == 0)
                {
                        found = 1;
                        break;
                }
                current_order++;
        }

        assert(found == 1);
        p->bh[current_index] = current_order;
        while(current_index)
        {
                current_index = PARENT(current_index) ;
                current_order++;

                left_order = p->bh[LEFT_LEAF(current_index)];
                right_order = p->bh[RIGHT_LEAF(current_index)];
                if(left_order == (current_order -1 ) && right_order == (current_order - 1))
                {
                        p->bh[current_index] = current_order;
                }
                else
                {
                        p->bh[current_index] = MAX(left_order,right_order);
                }
        }

        return;
}

static int show_status(intptr_t id)
{
        struct buddy_pool *p = (struct buddy_pool *)id;

        int i = 0;
        int level = -1;

        for(i = 0; i < 63; i++)
        {
                if(IS_POWER_OF_2(i + 1))
                {
                        level++;
                        fprintf(stderr,"\nlevel (%d):", level);
                }
                fprintf(stderr," %2d", p->bh[i]);
        }

        fprintf(stderr,"\n");
        return 0;
}

static unsigned char next_order_of_2(int size)
{
        unsigned char order = 0;

        while((1<<order) < size) {
                order++;
        }
        return order;
}
