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
        // First priority: necessary for de-codability (basic monitoring)
        int TS_sync_loss; // 1.1
        int Sync_byte_error; // 1.2
        int PAT_error; // 1.3
        int PAT_error_2; // 1.3.a
        int Continuity_count_error; // 1.4
        int PMT_error; // 1.5
        int PMT_error_2; // 1.5.a
        int PID_error; // 1.6

        // Second priority: recommended for continuous or periodic monitoring
        int Transport_error; // 2.1
        int CRC_error; // 2.2
        int PCR_error; // 2.3
        int PCR_repetition_error; // 2.3a
        int PCR_discontinuity_indicator_error; // 2.3b
        int PCR_accuracy_error; // 2.4
        int PTS_error; // 2.5
        int CAT_error; // 2.6

        // Third priority: application dependant monitoring
        int NIT_error; // 3.1
        int NIT_actual_error; // 3.1.a
        int NIT_other_error; // 3.1.b
        int SI_repetition_error; // 3.2
        int Buffer_error; // 3.3
        int Unreferenced_PID; // 3.4
        int Unreferenced_PID_2; // 3.4.a
        int SDT_error; // 3.5
        int SDT_actual_error; // 3.5.a
        int SDT_other_error; // 3.5.b
        int EIT_error; // 3.6
        int EIT_actual_error; // 3.6.a
        int EIT_other_error; // 3.6.b
        int EIT_PF_error; // 3.6.c
        int RST_error; // 3.7
        int TDT_error; // 3.8
        int Empty_buffer_error; // 3.9
        int Data_delay_error; // 3.10
}
ts_error_t; // TR 101 290 V1.2.1 2001-05

typedef struct
{
        // error
        ts_error_t err;

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
        int64_t PCR_interval;
        int64_t PCR_jitter;

        int has_PTS;
        uint64_t PTS;
        int64_t PTS_interval;
        int64_t PTS_minus_STC;

        int has_DTS;
        uint64_t DTS;
        int64_t DTS_interval;
        int64_t DTS_minus_STC;

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
