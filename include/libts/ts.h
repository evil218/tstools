/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: ts.h
 * funx: analyse ts stream
 * 
 * list: rslt -+-  pid  ->  pid  -> .. ->  pid
 *             |
 *             +-  prog (PMT)  ->  prog (PMT)  -> .. ->  prog (PMT)
 *             |    |     |         |     |               |     |
 *             |  track  sect     track  sect           track  sect
 *             |    |     |         |     |               |     |
 *             |  track  sect     track  sect           track  sect
 *             |    ..    ..        ..    ..              ..    ..
 *             |  track  sect     track  sect           track  sect
 *             |
 *             +- table -> table -> .. -> table
 *                  |        |              |
 *                 sect     sect           sect
 *                  |        |              |
 *                 sect     sect           sect
 *                  ..       ..             .. 
 *                 sect     sect           sect
 */

#ifndef _TS_H
#define _TS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "if.h"
#include "list.h"

#define INFO_LEN_MAX                    (0x0001 << 10) /* pow(2, 10) */
#define SERVER_STR_MAX                  (188) /* max length of server string */

/* TR 101 290 V1.2.1 2001-05 */
struct ts_error {
        /* First priority: necessary for de-codability (basic monitoring) */
        int TS_sync_loss; /* 1.1 */
        int Sync_byte_error; /* 1.2 */
#define ERR_1_3_0 (1<<0)
#define ERR_1_3_1 (1<<1)
#define ERR_1_3_2 (1<<2)
        int PAT_error; /* 1.3 ---- 1.3a of TR 101 290 V1.2.1 2001-05 */
        int Continuity_count_error; /* 1.4 */
#define ERR_1_5_0 (1<<0)
#define ERR_1_5_1 (1<<1)
        int PMT_error; /* 1.5 ---- 1.5a of TR 101 290 V1.2.1 2001-05 */
        int PID_error; /* 1.6 */

        /* Second priority: recommended for continuous or periodic monitoring */
        int Transport_error; /* 2.1 */
        int CRC_error; /* 2.2 */
        /* int PCR_error; 2.3 == (2.3a | 2.3b), invalid for new implementations! */
        int PCR_repetition_error; /* 2.3a */
        int PCR_discontinuity_indicator_error; /* 2.3b */
        int PCR_accuracy_error; /* 2.4 */
        int PTS_error; /* 2.5 */
        int CAT_error; /* 2.6 */

        /* Third priority: application dependant monitoring */
        int NIT_error; /* 3.1 */
        int NIT_actual_error; /* 3.1a */
        int NIT_other_error; /* 3.1b */
        int SI_repetition_error; /* 3.2 */
        int Buffer_error; /* 3.3 */
        int Unreferenced_PID; /* 3.4 */
        int Unreferenced_PID_2; /* 3.4a */
        int SDT_error; /* 3.5 */
        int SDT_actual_error; /* 3.5a */
        int SDT_other_error; /* 3.5b */
        int EIT_error; /* 3.6 */
        int EIT_actual_error; /* 3.6a */
        int EIT_other_error; /* 3.6b */
        int EIT_PF_error; /* 3.6c */
        int RST_error; /* 3.7 */
        int TDT_error; /* 3.8 */
        int Empty_buffer_error; /* 3.9 */
        int Data_delay_error; /* 3.10 */
};

struct ts_psi {
        uint8_t table_id; /* TABLE_ID_TABLE */
        uint8_t section_syntax_indicator; /* 1-bit */
        uint8_t private_indicator; /* 1-bit */
        uint8_t reserved0; /* 2-bit */
        uint16_t section_length; /* 12-bit */
        uint16_t table_id_extension; /* transport_stream_id or program_number */
        uint8_t reserved1; /* 2-bit */
        uint8_t version_number; /* 5-bit */
        uint8_t current_next_indicator; /* 1-bit */
        uint8_t section_number;
        uint8_t last_section_number;

        int check_CRC; /* some table do not need to check CRC_32 */
        int type; /* index of item in PID_TYPE[] */
};

/* node of section list */
struct ts_section {
        struct lnode cvfl; /* common variable for list */

        uint8_t section_number;
        uint16_t section_length;
        uint8_t data[4096];
};

/* node of PSI/SI table list */
struct ts_table {
        struct lnode cvfl; /* common variable for list */

        uint8_t table_id; /* 0x00~0xFF except 0x02 */
        uint8_t version_number;
        uint8_t last_section_number;
        struct ts_section *section0;
        int64_t STC;
};

/* node of track list */
struct ts_track {
        struct lnode cvfl; /* common variable for list */

        /* PID */
        uint16_t PID; /* 13-bit */
        int type; /* PID type index */

        /* stream_type */
        int stream_type;
        const char *sdes; /* stream type short description */
        const char *ldes; /* stream type long description */

        /* es_info */
        int es_info_len;
        uint8_t es_info[INFO_LEN_MAX];

        /* for PTS/DTS mark */
        int64_t PTS; /* last PTS */
        int64_t DTS; /* last DTS */

        int is_pes_align; /* met first PES head */
};

/* node of prog list */
struct ts_prog {
        struct lnode cvfl; /* common variable for list */

        /* PMT section,  */
        int is_parsed;
        struct ts_table table_02; /* table_id = 0x02 */

        /* program information */
        uint16_t PMT_PID; /* 13-bit */
        uint16_t PCR_PID; /* 13-bit */
        uint16_t program_number;
        int program_info_len;
        uint8_t program_info[INFO_LEN_MAX];

        /* service information */
        uint8_t service_name_len;
        uint8_t service_name[SERVER_STR_MAX];
        uint8_t service_provider_len;
        uint8_t service_provider[SERVER_STR_MAX];

        /* tracks */
        struct ts_track *track0;

        /* for STC calc */
        int64_t ADDa; /* PCR packet a: packet address */
        int64_t PCRa; /* PCR packet a: PCR value */
        int64_t ADDb; /* PCR packet b: packet address */
        int64_t PCRb; /* PCR packet b: PCR value */
        int STC_sync; /* true: PCRa and PCRb OK, STC can be calc */
};

/* node of pid list */
struct ts_pid {
        struct lnode cvfl; /* common variable for list */

        /* PID */
        uint16_t PID; /* 13-bit */
        int type; /* PID type index */
        int is_video;
        int is_audio;
        const char *sdes; /* PID type short description */
        const char *ldes; /* PID type long description */

        /* only for PID with PSI/SI */
        int section_idx; /* to index data in section */
        uint8_t section[4416]; /* (184*24=4416), PSI/SI|private <= 1024|4096 */
        int64_t section_interval;
        uint32_t CRC_32;
        uint32_t CRC_32_calc;

         /* point to the node in xxx_list */
        struct ts_prog *prog; /* should be prog0 if does not belong to any program */
        struct ts_track *track; /* should be NULL if not video or audio packet */

        /* for continuity_counter check */
        int is_CC_sync;
        uint8_t CC; /* 4-bit */

        /* for statistic */
        uint32_t cnt; /* packet received from last PCR */
        uint32_t lcnt; /* packet received from PCRa to PCRb */

        int64_t STC; /* last STC */

        /* file for ES dump */
        FILE *fd;
};

/* parse result */
struct ts_rslt {
        int64_t cnt; /* count of this packet, start from 0 */
        struct ts_pkt PKT;
        struct ts_pkt *pkt; /* point to PKT */

        int64_t lCTS; /* for calc dCTS in MTS mode */
        int64_t CTS0;
        int64_t CTS; /* according to clock of real time, MUX or appointed PCR */
        int64_t CTS_base;
        int64_t CTS_ext;

        uint16_t concerned_pid; /* used for PSI parsing */
        uint16_t PID;

        struct ts_pid *pid; /* point to the node in pid_list */

        /* TS information */
        int has_got_transport_stream_id;
        uint16_t transport_stream_id;
        struct ts_prog *prog0; /* program list */
        struct ts_pid *pid0; /* pid list */

        /* PSI/SI table */
        struct ts_psi psi;
        int is_pat_pmt_parsed;
        int is_psi_parse_finished;
        int is_psi_si;
        int has_section;
        struct ts_table *table0; /* PSI/SI table except PMT */

        /* for bit-rate statistic */
        int64_t aim_interval; /* appointed interval */
        int64_t interval; /* time passed from last rate calc */
        int64_t sys_cnt; /* system packet count */
        int64_t psi_cnt; /* psi-si packet count */
        int64_t nul_cnt; /* empty packet count */

        int has_rate; /* new bit-rate ready */
        int64_t last_interval; /* interval from PCRa to PCRb */
        int64_t last_sys_cnt; /* system packet count from PCRa to PCRb */
        int64_t last_psi_cnt; /* psi-si packet count from PCRa to PCRb */
        int64_t last_nul_cnt; /* empty packet count from PCRa to PCRb */

        /* PMT, PCR, VID and AUD has timestamp according to its PCR */
        /* other PID has timestamp according to the PCR in the 1st program */
        int64_t STC; /* according to its PCR */
        int64_t STC_base;
        int16_t STC_ext;

        /* CC */
        int CC_wait;
        int CC_find;
        int CC_lost; /* lost != 0 means CC wrong */

        /* PCR */
        int has_PCR;
        int64_t PCR;
        int64_t PCR_base;
        int16_t PCR_ext;
        int64_t PCR_interval; /* PCR packet arrive time interval */
        int64_t PCR_continuity; /* PCR value interval */
        int64_t PCR_jitter;

        /* PTS */
        int has_PTS;
        int64_t PTS;
        int64_t PTS_interval;
        int64_t PTS_minus_STC;

        /* DTS */
        int has_DTS;
        int64_t DTS;
        int64_t DTS_interval;
        int64_t DTS_minus_STC;

        /* PES */
        uint16_t PES_len;
        uint8_t *PES_buf;

        /* ES */
        uint16_t ES_len;
        uint8_t *ES_buf;

        /* error */
        struct ts_error err;
};

int tsCreate(struct ts_rslt **rslt);
int tsDelete(int id);
int tsParseTS(int id);
int tsParseOther(int id);

#ifdef __cplusplus
}
#endif

#endif /* _TS_H */
