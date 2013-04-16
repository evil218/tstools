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
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_pid, PID), "PID", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_pid, type), "type", sizeof(int), 0, NULL, NULL},
        {PT_NULL, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

struct adesc ad_pid[] = {
        {0, 0, sizeof(struct ts_pid), pd_pid, ""},
        {0, 0, 0, NULL, ""} /* end of array */
};

struct pdesc pd_sect[] = {
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, table_id), "table_id", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, section_syntax_indicator), "section_syntax_indicator", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, private_indicator), "private_indicator", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, reserved0), "reserved0", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, section_length), "section_length", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, table_id_extension), "table_id_extension", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, reserved1), "reserved1", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, version_number), "version_number", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, current_next_indicator), "current_next_indicator", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, section_number), "section_number", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, last_section_number), "last_section_number", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_sect, type), "type", sizeof(int), 0, NULL, NULL},
        {PT_NULL, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

struct adesc ad_sect[] = {
        {0, 0, sizeof(struct ts_sect), pd_sect, ""},
        {0, 0, 0, NULL, ""} /* end of array */
};

struct pdesc pd_tabl[] = {
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_tabl, table_id), "table_id", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_tabl, version_number), "version_number", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_tabl, last_section_number), "last_section_number", sizeof(uint8_t), 0, NULL, NULL},
#if 0
        {PT_LIST_, 0, 1, OFFSET(struct ts_tabl, sect0), "sect", sizeof(struct ts_sect *), (int)ad_sect, NULL, NULL},
#endif
        {PT_NULL, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

struct adesc ad_tabl[] = {
        {0, 0, sizeof(struct ts_tabl), pd_tabl, ""},
        {0, 0, 0, NULL, ""} /* end of array */
};

struct adesc ad_es_info[] = {
        {0, OFFSET(struct ts_elem, es_info_len), sizeof(uint8_t), NULL, ""}
};

struct pdesc pd_elem[] = {
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_elem, PID), "PID", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_elem, type), "type", sizeof(int), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_elem, stream_type), "stream_type", sizeof(uint8_t), 0, NULL, NULL},
        {PT_UINTX_XS, 0, 1, OFFSET(struct ts_elem, es_info), "es_info", sizeof(void *), (int)ad_es_info, NULL, NULL},
        {PT_NULL, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

struct adesc ad_elem[] = {
        {0, 0, sizeof(struct ts_elem), pd_elem, ""},
        {0, 0, 0, NULL, ""} /* end of array */
};

struct adesc ad_program_info[] = {
        {0, OFFSET(struct ts_prog, program_info_len), sizeof(uint8_t), NULL, ""}
};

struct adesc ad_service_name[] = {
        {0, OFFSET(struct ts_prog, service_name_len), sizeof(uint8_t), NULL, ""}
};

struct adesc ad_service_provider[] = {
        {0, OFFSET(struct ts_prog, service_provider_len), sizeof(uint8_t), NULL, ""}
};

struct pdesc pd_prog[] = {
        {PT_UINTu_SS, 0, 1, OFFSET(struct ts_prog, program_number), "program_number", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_prog, PMT_PID), "PMT_PID", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX_SS, 0, 1, OFFSET(struct ts_prog, PCR_PID), "PCR_PID", sizeof(uint16_t), 0, NULL, NULL},
        {PT_UINTX_XS, 0, 1, OFFSET(struct ts_prog, program_info), "program_info", sizeof(void *), (int)ad_program_info, NULL, NULL},
        {PT_UINTX_XS, 0, 1, OFFSET(struct ts_prog, service_name), "service_name", sizeof(void *), (int)ad_service_name, NULL, NULL},
        {PT_UINTX_XS, 0, 1, OFFSET(struct ts_prog, service_provider), "service_provider", sizeof(void *), (int)ad_service_provider, NULL, NULL},
        {PT_LIST__XS, 0, 1, OFFSET(struct ts_prog, elem0), "elem", sizeof(struct ts_elem *), (int)ad_elem, NULL, NULL},
        {PT_STRU__SS, 0, 1, OFFSET(struct ts_prog, tabl), "tabl", sizeof(struct ts_tabl), (int)pd_tabl, NULL, NULL},
        {PT_NULL, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

struct adesc ad_prog[] = {
        {0, 0, sizeof(struct ts_prog), pd_prog, ""},
        {0, 0, 0, NULL, ""} /* end of array */
};

struct pdesc pd_ts[] = {
        {PT_UINTu_SS, 0, 1, OFFSET(struct ts_obj, transport_stream_id), "transport_stream_id", sizeof(uint16_t), 0, NULL, NULL},
        {PT_LIST__XS, 0, 1, OFFSET(struct ts_obj, prog0), "prog", sizeof(struct ts_prog *), (int)ad_prog, NULL, NULL},
        {PT_LIST__XS, 0, 1, OFFSET(struct ts_obj, tabl0), "tabl", sizeof(struct ts_tabl *), (int)ad_tabl, NULL, NULL},
        {PT_LIST__XS, 0, 1, OFFSET(struct ts_obj, pid0), "pid", sizeof(struct ts_pid *), (int)ad_pid, NULL, NULL},
        {PT_NULL, 0, 0, 0, "", 0, 0, NULL, NULL} /* PT_NULL means tail of struct pdesc array */
};

#ifdef __cplusplus
}
#endif

#endif /* _TS_DESC_H */
