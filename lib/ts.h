/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: ts.h
// Purpose: analyse ts stream
// To build: gcc -std-c99 -c ts.c
//=============================================================================

#ifndef _TS_H
#define _TS_H

#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Struct definition:
//=============================================================================
typedef struct
{
        struct NODE *next;
        struct NODE *prev;

        uint16_t PID:13;
        int is_track; // is video or audio package
        const char *type; // pid_type string
        const char *sdes; // short description
        const char *ldes; // long description

        // for check continuity_counter
        uint32_t CC:4;
        uint32_t dCC:4; // 0 or 1
        int is_CC_sync;
}
ts_pid_t; // unit of pid list

typedef struct
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t PMT_PID:13;
        uint32_t PCR_PID:13;
        uint32_t program_number:16;
        uint16_t program_info_len;
        uint8_t *program_info_buf;

        struct LIST *track; // track list
        int is_parsed;
}
ts_prog_t; // unit of prog list

typedef struct
{
        struct NODE *next;
        struct NODE *prev;

        int stream_type;
        uint32_t PID:13;
        const char *type; // pid_type string
        uint16_t es_info_len;
        uint8_t *es_info_buf;
}
ts_track_t; // unit of track list

typedef struct
{
        // PSI, SI and other TS information
        int is_psi_parsed;
        struct LIST *prog_list;
        struct LIST *pid_list;

        // information about current package
        uint8_t line[204]; // current TS package
        uint16_t concerned_pid; // used for PSI parsing
        uint16_t pid;

        int CC_wait;
        int CC_find;
        int CC_lost; // lost != 0 means CC wrong

        int has_PCR;
        uint64_t PCR_base;
        uint16_t PCR_ext;
        uint64_t PCR;

        int has_PTS;
        uint64_t PTS;

        int has_DTS;
        uint64_t DTS;

        // PES data info in this TS
        uint16_t PES_len;
        uint8_t *PES_buf;

        // ES data info in this TS
        uint16_t ES_len;
        uint8_t *ES_buf;
}
ts_rslt_t; // parse result

//============================================================================
// public function declaration
//============================================================================
int tsCreate(int pkg_size, ts_rslt_t **rslt);
int tsDelete(int id);
int tsParseTS(int id, void *pkg);
int tsParseOther(int id);

#ifdef __cplusplus
}
#endif

#endif // _TS_H

/*****************************************************************************
 * End
 ****************************************************************************/
