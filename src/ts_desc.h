/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: pdesc.h
 * funx: parameter descriptive tree
 */

#ifndef _TS_DESC_H
#define _TS_DESC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "param_xml.h"
#include "ts.h"

struct pdesc pd_elem[] = {
        {PT_UINTX, 0, 1, OFFSET(struct ts_elem, PID), "PID", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_elem, stream_type), "stream_type", sizeof(uint8_t), 0, NULL, NULL},
        {PT_NULL_, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

struct tdesc td_elem[] = {
        {sizeof(struct ts_elem), pd_elem, ""},
        {0, NULL, ""} /* end of array */
};

struct pdesc pd_prog[] = {
        {PT_UINTu, 0, 1, OFFSET(struct ts_prog, program_number), "program_number", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_prog, PMT_PID), "PMT_PID", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_prog, PCR_PID), "PCR_PID", sizeof(uint16_t), 0, NULL, NULL},
        {PT_LIST_, 0, 1, OFFSET(struct ts_prog, elem0), "elem", sizeof(struct ts_elem *), (int)td_elem, NULL, NULL},
        {PT_NULL_, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

struct tdesc td_prog[] = {
        {sizeof(struct ts_prog), pd_prog, ""},
        {0, NULL, ""} /* end of array */
};

struct pdesc pd_ts[] = {
        {PT_UINTu, 0, 1, OFFSET(struct ts_obj, transport_stream_id), "transport_stream_id", sizeof(uint16_t), 0, NULL, NULL},
        {PT_LIST_, 0, 1, OFFSET(struct ts_obj, prog0), "prog", sizeof(struct ts_prog *), (int)td_prog, NULL, NULL},
        {PT_NULL_, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

#ifdef __cplusplus
}
#endif

#endif /* _TS_DESC_H */
