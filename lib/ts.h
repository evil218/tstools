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

#define INFO_LEN_MAX                    (0x0001 << 10) // pow(2, 10)
#define SERVER_STR_MAX                  (188) // max length of server string

//=============================================================================
// Struct definition:
//=============================================================================
typedef struct
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t PMT_PID:13;
        uint32_t PCR_PID:13;
        uint32_t program_number:16;
        int program_info_len;
        uint8_t program_info[INFO_LEN_MAX];
        int server_name_len;
        uint8_t server_name[SERVER_STR_MAX];
        int server_provider_len;
        uint8_t server_provider[SERVER_STR_MAX];

        struct LIST *track; // track list
        int is_parsed;

        // for STC calc
        //      STCx - PCRb   ADDx - ADDb
        //      ----------- = -----------
        //      PCRb - PCRa   ADDb - ADDa
        uint64_t ADDa, ADDb;
        uint64_t PCRa, PCRb;
}
ts_prog_t; // unit of prog list

typedef struct
{
        struct NODE *next;
        struct NODE *prev;

        uint32_t PID:13;
        int type; // PID type index
        int stream_type;
        const char *sdes; // stream short description
        const char *ldes; // stream long description
        int es_info_len;
        uint8_t es_info[INFO_LEN_MAX];

        uint64_t PTS_last;
        uint64_t DTS_last;
}
ts_track_t; // unit of track list

typedef struct
{
        struct NODE *next;
        struct NODE *prev;

        uint16_t PID:13;
        int type; // pid type index
        const char *sdes; // short description
        const char *ldes; // long description

        uint32_t count; // packet number received, for statistic

        ts_prog_t *prog; // for PMT, PCR and PES package or NULL
        ts_track_t *track; // for video or audio package or NULL

        // for check continuity_counter
        uint32_t CC:4;
        uint32_t dCC:4; // 0 or 1
        int is_CC_sync;
}
ts_pid_t; // unit of pid list

typedef struct
{
        // PSI, SI and other TS information
        int is_psi_parsed;
        struct LIST *prog_list;
        struct LIST *pid_list;

        // information about current package
        uint64_t addr; // address of this package
        uint8_t line[204]; // current TS package

        uint16_t concerned_pid; // used for PSI parsing
        uint16_t pid;

        ts_pid_t *pids; // point to item in pid_list

        int CC_wait;
        int CC_find;
        int CC_lost; // lost != 0 means CC wrong

        // PMT, PCR, VID and AUD has timestamp according to its PCR
        uint64_t STC; // System Time Clock of this package
        uint64_t STC_base;
        uint16_t STC_ext;

        int has_PCR;
        uint64_t PCR;
        uint64_t PCR_base;
        uint16_t PCR_ext;
        double PCR_interval; // (ms)
        double PCR_jitter; // (us)

        int has_PTS;
        uint64_t PTS;
        double PTS_interval; // (ms)
        double PTS_minus_STC; // (ms)

        int has_DTS;
        uint64_t DTS;
        double DTS_interval; // (ms)
        double DTS_minus_STC; // (ms)

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
