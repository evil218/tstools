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

struct pdesc pd_pid[] = {
        {PT_UINTX, 0, 1, OFFSET(struct ts_pid, PID), "PID", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_pid, type), "type", sizeof(int), 0, NULL, NULL},
        {PT_NULL_, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

struct tdesc td_pid[] = {
        {sizeof(struct ts_pid), pd_pid, ""},
        {0, NULL, ""} /* end of array */
};

struct pdesc pd_sect[] = {
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, table_id), "table_id", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, section_syntax_indicator), "section_syntax_indicator", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, private_indicator), "private_indicator", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, reserved0), "reserved0", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, section_length), "section_length", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, table_id_extension), "table_id_extension", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, reserved1), "reserved1", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, version_number), "version_number", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, current_next_indicator), "current_next_indicator", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, section_number), "section_number", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, last_section_number), "last_section_number", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_sect, type), "type", sizeof(int), 0, NULL, NULL},
        {PT_NULL_, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

struct tdesc td_sect[] = {
        {sizeof(struct ts_sect), pd_sect, ""},
        {0, NULL, ""} /* end of array */
};

struct pdesc pd_tabl[] = {
        {PT_UINTX, 0, 1, OFFSET(struct ts_tabl, table_id), "table_id", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_tabl, version_number), "version_number", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_tabl, last_section_number), "last_section_number", sizeof(uint8_t), 0, NULL, NULL},
#if 0
        {PT_LIST_, 0, 1, OFFSET(struct ts_tabl, sect0), "sect", sizeof(struct ts_sect *), (int)td_sect, NULL, NULL},
#endif
        {PT_NULL_, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

struct tdesc td_tabl[] = {
        {sizeof(struct ts_tabl), pd_tabl, ""},
        {0, NULL, ""} /* end of array */
};

struct pdesc pd_elem[] = {
        {PT_UINTX, 0, 1, OFFSET(struct ts_elem, PID), "PID", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX, 0, 1, OFFSET(struct ts_elem, type), "type", sizeof(int), 0, NULL, NULL},
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
        {PT_STRU_, 0, 1, OFFSET(struct ts_prog, tabl), "tabl", sizeof(struct ts_tabl), (int)pd_tabl, NULL, NULL},
        {PT_NULL_, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

struct tdesc td_prog[] = {
        {sizeof(struct ts_prog), pd_prog, ""},
        {0, NULL, ""} /* end of array */
};

struct pdesc pd_ts[] = {
        {PT_UINTu, 0, 1, OFFSET(struct ts_obj, transport_stream_id), "transport_stream_id", sizeof(uint16_t), 0, NULL, NULL},
        {PT_LIST_, 0, 1, OFFSET(struct ts_obj, prog0), "prog", sizeof(struct ts_prog *), (int)td_prog, NULL, NULL},
        {PT_LIST_, 0, 1, OFFSET(struct ts_obj, tabl0), "tabl", sizeof(struct ts_tabl *), (int)td_tabl, NULL, NULL},
        {PT_LIST_, 0, 1, OFFSET(struct ts_obj, pid0), "pid", sizeof(struct ts_pid *), (int)td_pid, NULL, NULL},
        {PT_NULL_, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

#ifdef __cplusplus
}
#endif

#endif /* _TS_DESC_H */
