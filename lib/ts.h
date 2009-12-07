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
        int type; // enum pid_type
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

        uint32_t PID:13;
        int stream_type;
        int type; // enum pid_type
}
ts_track_t; // unit of track list

typedef struct
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t program_number:16;
        uint32_t PMT_PID:13;
        uint32_t PCR_PID:13;

        struct LIST *track; // track list
        int is_parsed;
}
ts_prog_t; // unit of prog list

typedef struct
{
        // PSI, SI and other TS information
        struct LIST *prog_list;
        struct LIST *pid_list;

        // information about current package
        int is_psi_parsed;
        uint16_t concerned_pid; // used for PSI parsing
        uint16_t pid;

        struct
        {
                int wait;
                int find;
                int lost; // lost != 0 means CC wrong
        } CC;

        struct
        {
                int is_pcr_pid;
                uint64_t pcr_base;
                uint16_t pcr_ext;
                uint64_t pcr;
        } PCR;
}
ts_rslt_t; // parse result

//============================================================================
// public function declaration
//============================================================================
int tsCreate(int pkg_size, ts_rslt_t **rslt);
int tsDelete(int id);
int tsParseTS(int id, const char *pkg);
int tsParseOther(int id);

#ifdef __cplusplus
}
#endif

#endif // _TS_H

/*****************************************************************************
 * End
 ****************************************************************************/
