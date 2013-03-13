/* vim: set tabstop=8 shiftwidth=8:
 * name: ts.h
 * funx: analyse ts stream
 * 
 *  obj: +-  pid  ->  pid  -> .. ->  pid
 *       |
 *       +-  prog (PMT)  ->  prog (PMT)  -> .. ->  prog (PMT)
 *       |    |     |         |     |               |     |
 *       |   elem  sect      elem  sect            elem  sect
 *       |    |     |         |     |               |     |
 *       |   elem  sect      elem  sect            elem  sect
 *       |    ..    ..        ..    ..              ..    ..
 *       |   elem  sect      elem  sect            elem  sect
 *       |
 *       +- table -> table -> .. -> table
 *            |        |              |
 *           sect     sect           sect
 *            |        |              |
 *           sect     sect           sect
 *            ..       ..             ..
 *           sect     sect           sect
 */

#ifndef _TS_H
#define _TS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zlst.h"

#define STC_BASE_MS  (90)        /* 90 clk == 1(ms) */
#define STC_BASE_1S  (90 * 1000) /* do NOT use 1e3 */
#define STC_BASE_OVF (1LL << 33) /* 0x0200000000 */

#define STC_US  (27)                 /* 27 clk == 1(us) */
#define STC_MS  (27 * 1000)          /* do NOT use 1e3  */
#define STC_1S  (27 * 1000 * 1000)   /* do NOT use 1e3  */
#define STC_OVF (STC_BASE_OVF * 300) /* 2576980377600 */

#define MTS_US  (27)               /* 27 clk == 1(us) */
#define MTS_MS  (27 * 1000)        /* do NOT use 1e3  */
#define MTS_1S  (27 * 1000 * 1000) /* do NOT use 1e3  */
#define MTS_OVF (1<<30)            /* 0x40000000 */

#define INFO_LEN_MAX (1<<10) /* pow(2, 10) */
#define SERVER_STR_MAX (188) /* max length of server string */

/* TR 101 290 V1.2.1 2001-05 */
struct ts_err {
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

/* TS head */
struct ts_tsh {
        uint8_t sync_byte;
        uint8_t transport_error_indicator; /* 1-bit */
        uint8_t payload_unit_start_indicator; /* 1-bit */
        uint8_t transport_priority; /* 1-bit */
        uint16_t PID; /* 13-bit */
        uint8_t transport_scrambling_control; /* 2-bit */
        uint8_t adaption_field_control; /* 2-bit */
        uint8_t continuity_counter; /* 4-bit */
};

/* Adaption Field */
struct ts_af {
        uint8_t adaption_field_length;
        uint8_t discontinuity_indicator; /* 1-bit */
        uint8_t random_access_indicator; /* 1-bit */
        uint8_t elementary_stream_priority_indicator; /* 1-bit */
        uint8_t PCR_flag; /* 1-bit */
        uint8_t OPCR_flag; /* 1-bit */
        uint8_t splicing_pointer_flag; /* 1-bit */
        uint8_t transport_private_data_flag; /* 1-bit */
        uint8_t adaption_field_extension_flag; /* 1-bit */
        int64_t program_clock_reference_base; /* 33-bit */
        int16_t program_clock_reference_extension; /* 9-bit */
        int64_t original_program_clock_reference_base; /* 33-bit */
        int16_t original_program_clock_reference_extension; /* 9-bit */
        uint8_t splice_countdown;
        uint8_t transport_private_data_length;
        uint8_t private_data_byte[256];
        uint8_t adaption_field_extension_length;
        /* ... */
};

/* PES head */
struct ts_pesh {
        uint32_t packet_start_code_prefix; /* 24-bit */
        uint8_t stream_id;
        uint16_t PES_packet_length;
        uint8_t PES_scrambling_control; /* 2-bit */
        uint8_t PES_priority; /* 1-bit */
        uint8_t data_alignment_indicator; /* 1-bit */
        uint8_t copyright; /* 1-bit */
        uint8_t original_or_copy; /* 1-bit */
        uint8_t PTS_DTS_flags; /* 2-bit */
        uint8_t ESCR_flag; /* 1-bit */
        uint8_t ES_rate_flag; /* 1-bit */
        uint8_t DSM_trick_mode_flag; /* 1-bit */
        uint8_t additional_copy_info_flag; /* 1-bit */
        uint8_t PES_CRC_flag; /* 1-bit */
        uint8_t PES_extension_flag; /* 1-bit */
        uint8_t PES_header_data_length;
        int64_t PTS; /* 33-bit */
        int64_t DTS; /* 33-bit */
        int64_t ESCR_base; /* 33-bit */
        int16_t ESCR_extension; /* 9-bit */
        uint32_t ES_rate; /* 22-bit */
        uint8_t trick_mode_control; /* 3-bit */
        uint8_t field_id; /* 2-bit */
        uint8_t intra_slice_refresh; /* 1-bit */
        uint8_t frequency_truncation; /* 2-bit */
        uint8_t rep_cntrl; /* 5-bit */
        uint8_t additional_copy_info; /* 7-bit */
        uint16_t previous_PES_packet_CRC;
        uint8_t PES_private_data_flag; /* 1-bit */
        uint8_t pack_header_field_flag; /* 1-bit */
        uint8_t program_packet_sequence_counter_flag; /* 1-bit */
        uint8_t P_STD_buffer_flag; /* 1-bit */
        uint8_t PES_extension_flag_2; /* 1-bit */
        uint8_t  PES_private_data[16]; /* 128-bit */
        uint8_t pack_field_length;
        uint8_t program_packet_sequence_counter; /* 7-bit */
        uint8_t MPEG1_MPEG2_identifier; /* 1-bit */
        uint8_t original_stuff_length; /* 6-bit */
        uint8_t P_STD_buffer_scale; /* 1-bit */
        uint16_t P_STD_buffer_size; /* 13-bit */
        uint8_t PES_extension_field_length; /* 7-bit */
};

/* section head */
struct ts_sech {
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
struct ts_sect {
        struct znode cvfl; /* common variable for list */

        /* section head */
        uint8_t section_number;
        uint16_t section_length;

        /* section info */
        uint8_t data[4096];
        int check_CRC; /* bool, some table do not need to check CRC_32 */
        int type; /* index of item in PID_TYPE[] */
};

/* node of PSI/SI table list */
struct ts_table {
        struct znode cvfl; /* common variable for list */

        uint8_t table_id; /* 0x00~0xFF except 0x02 */
        uint8_t version_number;
        uint8_t last_section_number;
        struct ts_sect *sect0;
        int64_t STC;
};

/* node of elementary list */
struct ts_elem {
        struct znode cvfl; /* common variable for list */

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

/* node of program list */
struct ts_prog {
        struct znode cvfl; /* common variable for list */

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

        /* elementary stream list */
        struct ts_elem *elem0;

        /* for STC calc */
        int64_t ADDa; /* PCR packet a: packet address */
        int64_t PCRa; /* PCR packet a: PCR value */
        int64_t ADDb; /* PCR packet b: packet address */
        int64_t PCRb; /* PCR packet b: PCR value */
        int STC_sync; /* true: PCRa and PCRb OK, STC can be calc */
};

/* node of pid list */
struct ts_pid {
        struct znode cvfl; /* common variable for list */

        /* PID */
        uint16_t PID; /* 13-bit */
        int type; /* PID type index */
        int is_video;
        int is_audio;
        const char *sdes; /* PID type short description */
        const char *ldes; /* PID type long description */

        /* only for PID with PSI/SI */
        int sect_idx; /* to index data in section */
        uint8_t sect_data[4416]; /* (184*24=4416), PSI/SI|private <= 1024|4096 */
        int64_t sect_interval;
        uint32_t CRC_32;
        uint32_t CRC_32_calc;

         /* point to the node in xxx_list */
        struct ts_prog *prog; /* should be prog0 if does not belong to any program */
        struct ts_elem *elem; /* should be NULL if not video or audio packet */

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

/* parse input and output */
struct ts_obj {
        /* <input> */

        /* information about one packet, tell me as more as you can :-) */
        uint8_t TS[188]; /* TS data */
        uint8_t RS[16]; /* RS data */
        long long int ADDR; /* address of sync-byte(unit: byte) */
        int64_t MTS; /* MTS Time Stamp */
        int64_t CTS; /* according to clock of real time, MUX or appointed PCR */

        /* NULL means the item is absent */
        uint8_t *ts; /* point to TS packet */
        uint8_t *rs; /* NULL or point to RS data */
        long long int *addr; /* NULL or point to ADDR */
        int64_t *mts; /* NULL or point to MTS */
        int64_t *cts; /* NULL or point to CTS */

        /* </input> */

        /* <output> */

        /* CTS */
        int64_t CTS_base;
        int64_t CTS_ext;
        int64_t lCTS; /* for calc dCTS in MTS mode */
        int64_t CTS0;

        /* STC */
        /* PMT, PCR, VID and AUD has timestamp according to its PCR */
        /* other PID has timestamp according to the PCR of program0 */
        int64_t *stc; /* NULL or point to STC */
        int64_t STC;
        int64_t STC_base;
        int16_t STC_ext;

        /* CC */
        int CC_wait;
        int CC_find;
        int CC_lost; /* lost != 0 means CC wrong */

        /* AF */
        uint8_t *AF; /* NULL or point to adaptation_fields */
        int AF_len;

        /* PCR */
        int64_t *pcr; /* NULL or point to PCR */
        int64_t PCR;
        int64_t PCR_base;
        int16_t PCR_ext;
        int64_t PCR_interval; /* PCR packet arrive time interval */
        int64_t PCR_continuity; /* PCR value interval */
        int64_t PCR_jitter;

        /* PES */
        uint8_t *PES; /* NULL or point to PES fragment */
        int PES_len;

        /* PTS */
        int64_t *pts; /* NULL or point to PTS */
        int64_t PTS;
        int64_t PTS_interval;
        int64_t PTS_minus_STC;

        /* DTS */
        int64_t *dts; /* NULL or point to DTS */
        int64_t DTS;
        int64_t DTS_interval;
        int64_t DTS_minus_STC;

        /* ES */
        uint8_t *ES; /* NULL or point to ES fragment */
        int ES_len;

        uint16_t concerned_pid; /* used for PSI parsing */
        uint16_t PID;

        struct ts_pid *pid; /* point to the node in pid_list */

        /* TS information */
        long long int cnt; /* count of this packet in this stream, start from 0 */
        struct ts_tsh tsh; /* info about ts head of this packet */
        struct ts_af af; /* info about af of this packet */
        struct ts_pesh pesh; /* info about pesh of this packet */
        struct ts_pid *pid0; /* pid list of this stream */

        /* PSI/SI table */
        uint16_t transport_stream_id;
        int has_got_transport_stream_id;
        struct ts_sech sech; /* info about section head before this packet */
        int is_pat_pmt_parsed;
        int is_psi_parse_finished;
        int is_psi_si;
        int has_sect;
        struct ts_prog *prog0; /* program list of this stream */
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

        /* error */
        struct ts_err err;

        /* </output> */

        /* special variables for ts object */
        int state;
        intptr_t mp; /* id of buddy memory pool, for list malloc and free */
        int need_pes_align; /* 0: dot't need; 1: need PES align */
        int is_verbose; /* 0: shut up; 1: report key step */

        /* special variables for packet analyse */
        uint8_t *cur; /* point to the current data in rslt.TS[] */
        uint8_t *tail; /* point to the next data after rslt.TS[] */
};

struct ts_obj *ts_create(intptr_t mp);
int ts_destroy(struct ts_obj *ts);

int ts_init(struct ts_obj *ts);
int ts_parse_tsh(struct ts_obj *ts);
int ts_parse_tsb(struct ts_obj *ts);

/* calculate timestamp:
 *      t0: [0, ovf);
 *      t1: [0, ovf);
 *      td: [-hovf, +hovf)
 *      ovf: overflow, (ovf > 0 and ovf is even)
 */

/* return: t1 = t0 + td */
int64_t timestamp_add(int64_t t0, int64_t td, int64_t ovf);

/* return: td = t1 - t0 */
int64_t timestamp_diff(int64_t t1, int64_t t0, int64_t ovf);

#ifdef __cplusplus
}
#endif

#endif /* _TS_H */
