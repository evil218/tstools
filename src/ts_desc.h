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
        {0, 0, 1, PT_UINTX_SS(struct ts_pid, PID, uint16_t), "PID", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_pid, type, int), "type", NULL, 0},
        {0, 0, 0, PT_NULL, "", NULL, 0} /* PT_NULL means tail of struct pdesc array */
};

struct pdesc pd_sect[] = {
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, table_id, uint8_t), "table_id", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, section_syntax_indicator, uint8_t), "section_syntax_indicator", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, private_indicator, uint8_t), "private_indicator", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, reserved0, uint8_t), "reserved0", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, section_length, uint16_t), "section_length", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, table_id_extension, uint16_t), "table_id_extension", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, reserved1, uint8_t), "reserved1", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, version_number, uint8_t), "version_number", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, current_next_indicator, uint8_t), "current_next_indicator", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, section_number, uint8_t), "section_number", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, last_section_number, uint8_t), "last_section_number", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_sect, type, int), "type", NULL, 0},
        {0, 0, 0, PT_NULL, "", NULL, 0} /* PT_NULL means tail of struct pdesc array */
};

struct pdesc pd_tabl[] = {
        {0, 0, 1, PT_UINTX_SS(struct ts_tabl, table_id, uint8_t), "table_id", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_tabl, version_number, uint8_t), "version_number", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_tabl, last_section_number, uint8_t), "last_section_number", NULL, 0},
#if 0
        {0, 0, 1, PT_LIST_(struct ts_tabl, sect0), "sect", sizeof(struct ts_sect *), (int)ad_sect, NULL, NULL},
#endif
        {0, 0, 0, PT_NULL, "", NULL, 0} /* PT_NULL means tail of struct pdesc array */
};

struct pdesc pd_elem[] = {
        {0, 0, 1, PT_UINTX_SS(struct ts_elem, PID, uint16_t), "PID", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_elem, type, int), "type", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_elem, stream_type, uint8_t), "stream_type", NULL, 0},
        {0, 0, 1, PT_UINTX_XS(struct ts_elem, es_info, es_info_len, uint8_t), "es_info", NULL, 0},
        {0, 0, 0, PT_NULL, "", NULL, 0} /* PT_NULL means tail of struct pdesc array */
};

struct pdesc pd_prog[] = {
        {0, 0, 1, PT_UINTu_SS(struct ts_prog, program_number, uint16_t), "program_number", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_prog, PMT_PID, uint16_t), "PMT_PID", NULL, 0},
        {0, 0, 1, PT_UINTX_SS(struct ts_prog, PCR_PID, uint16_t), "PCR_PID", NULL, 0},
        {0, 0, 1, PT_UINTX_XS(struct ts_prog, program_info, program_info_len, uint8_t), "program_info", NULL, 0},
        {0, 0, 1, PT_UINTX_XS(struct ts_prog, service_name, service_name_len, uint8_t), "service_name", NULL, 0},
        {0, 0, 1, PT_UINTX_XS(struct ts_prog, service_provider, service_provider_len, uint8_t), "service_provider", NULL, 0},
        {0, 0, 1, PT_LIST__XS(struct ts_prog, elem0, struct ts_elem), "elem", pd_elem, 0},
        {0, 0, 1, PT_STRU__SS(struct ts_prog, tabl, struct ts_tabl), "tabl", pd_tabl, 0},
        {0, 0, 0, PT_NULL, "", NULL, 0} /* PT_NULL means tail of struct pdesc array */
};

struct pdesc pd_ts[] = {
        {0, 0, 1, PT_UINTu_SS(struct ts_obj, transport_stream_id, uint16_t), "transport_stream_id", NULL, 0},
        {0, 0, 1, PT_LIST__XS(struct ts_obj, prog0, struct ts_prog), "prog", pd_prog, 0},
        {0, 0, 1, PT_LIST__XS(struct ts_obj, tabl0, struct ts_tabl), "tabl", pd_tabl, 0},
        {0, 0, 1, PT_LIST__XS(struct ts_obj, pid0, struct ts_pid), "pid", pd_pid, 0},
        {0, 0, 0, PT_NULL, "", NULL, 0} /* PT_NULL means tail of struct pdesc array */
};

#ifdef __cplusplus
}
#endif

#endif /* _TS_DESC_H */
