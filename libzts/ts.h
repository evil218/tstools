/* vim: set tabstop=8 shiftwidth=8:
 * name: ts.h
 * funx: analyse ts stream
 * 
 * obj:  +-  pid         ->  pid         -> .. ->  pid
 *       |    |               |
 *       |   pkt             pkt
 *       |    |               |
 *       |   pkt             pkt
 *       |    ..              ..
 *       |   pkt             pkt
 *       |
 *       +-  tabl        ->  tabl        -> .. ->  tabl
 *       |    |               |                     |
 *       |   sect            sect                  sect
 *       |    |               |                     |
 *       |   sect            sect                  sect
 *       |    ..              ..                    ..
 *       |   sect            sect                  sect
 *       |
 *       +-  prog (PMT)  ->  prog (PMT)  -> .. ->  prog (PMT)
 *            |     |         |     |               |     |
 *           elem  sect      elem  sect            elem  sect
 *            |               |                     |
 *           elem            elem                  elem
 *            ..              ..                    ..
 *           elem            elem                  elem
 *
 * pid  list: sorted by PID
 * tabl list: sorted by table_id
 * sect list: sorted by section_number
 * prog list: sorted by program_number
 * elem list: unsorted, just use the order in PMT
 *
 * 2009-04-01, ZHOU Cheng, Init for tstools
 */

#ifndef _TS_H
#define _TS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zlst.h" /* for "struct znode" */

#define STC_BASE_MS  (90)        /* 90 clk == 1(ms) */
#define STC_BASE_1S  (90 * 1000) /* do NOT use 1e3 */
#define STC_BASE_OVF (((int64_t)1) << 33) /* 0x0200000000 */

#define STC_US  (27)                 /* 27 clk == 1(us) */
#define STC_MS  (27 * 1000)          /* do NOT use 1e3  */
#define STC_1S  (27 * 1000 * 1000)   /* do NOT use 1e3  */
#define STC_OVF (STC_BASE_OVF * 300) /* 2576980377600 */

#define ATS_US  (27)               /* 27 clk == 1(us) */
#define ATS_MS  (27 * 1000)        /* do NOT use 1e3  */
#define ATS_1S  (27 * 1000 * 1000) /* do NOT use 1e3  */
#define ATS_OVF (1<<30)            /* 0x40000000 */

#define TS_PKT_SIZE (188)
#define INFO_LEN_MAX (1<<10) /* uint10_t, max length of es_info or program_info */
#define SERVER_STR_MAX (1<<8) /* uint8_t, max length of server string */

/* TS packet type */
#define TS_TMSK_BASE    (0x00FF) /* BIT[7:0]: base type mask */
#define TS_TMSK_PCR     (0x0100) /* BIT[8:8]: 0(without PCR), 1(with PCR) */

#define TS_TYPE_ANY     (0x0000)
#define TS_TYPE_UNO     (0x0000)
#define TS_TYPE_NUL     (0x0001)
#define TS_TYPE_RSV     (0x0002)
#define TS_TYPE_BAD     (0x0003)

#define TS_TYPE_PAT     (0x0010)
#define TS_TYPE_CAT     (0x0011)
#define TS_TYPE_TSDT    (0x0012)
#define TS_TYPE_NIT     (0x0013)
#define TS_TYPE_ST      (0x0014)
#define TS_TYPE_SDT     (0x0015)
#define TS_TYPE_BAT     (0x0016)
#define TS_TYPE_EIT     (0x0017)
#define TS_TYPE_RST     (0x0018)
#define TS_TYPE_TDT     (0x0019)
#define TS_TYPE_TOT     (0x001A)
#define TS_TYPE_NS      (0x001B)
#define TS_TYPE_INB     (0x001C)
#define TS_TYPE_MSU     (0x001D)
#define TS_TYPE_DIT     (0x001E)
#define TS_TYPE_SIT     (0x001F)
#define TS_TYPE_EMM     (0x0020)

#define TS_TYPE_USR     (0x00C0)
#define TS_TYPE_PMT     (0x00C1)
#define TS_TYPE_VID     (0x00C2)
#define TS_TYPE_AUD     (0x00C3)
#define TS_TYPE_ECM     (0x00C4)

#define TS_TYPE_PCR     (TS_TYPE_USR | TS_TMSK_PCR)
#define TS_TYPE_VIDP    (TS_TYPE_VID | TS_TMSK_PCR)
#define TS_TYPE_AUDP    (TS_TYPE_AUD | TS_TMSK_PCR)
#define TS_TYPE_NULP    (TS_TYPE_NUL | TS_TMSK_PCR)

#define WITH_PCR(x)     (((x) & TS_TMSK_PCR ) == TS_TMSK_PCR)
#define IS_TYPE(t, x)   (((x) & TS_TMSK_BASE) == (t))

/* TR 101 290 V1.2.1 2001-05 and other stream errors */
struct ts_err {
        int has_level1_error; /* check veriables of "First priority" for detail information */
        int has_level2_error; /* check veriables of "Second priority" for detail information */
        int has_level3_error; /* check veriables of "Third priority" for detail information */
        int has_other_error; /* other stream errors */

        /* First priority: necessary for de-codability (basic monitoring) */
        int TS_sync_loss; /* 1.1 */
        int Sync_byte_error; /* 1.2 */
#define ERR_1_3_0 (1<<0) /* PAT occur more than 500ms */
#define ERR_1_3_1 (1<<1) /* table_id error in PID 0x0000 */
#define ERR_1_3_2 (1<<2) /* PAT Scrambling_control_field is not 00 */
        int PAT_error; /* 1.3 ---- 1.3a of TR 101 290 V1.2.1 2001-05 */
        int Continuity_count_error; /* 1.4 */
#define ERR_1_5_0 (1<<0)
#define ERR_1_5_1 (1<<1)
        int PMT_error; /* 1.5 ---- 1.5a of TR 101 290 V1.2.1 2001-05 */
        int PID_error; /* 1.6 FIXME: not supported */

        /* Second priority: recommended for continuous or periodic monitoring */
        int Transport_error; /* 2.1 */
        int CRC_error; /* 2.2 */
        /* int PCR_error; 2.3 == (2.3a | 2.3b), invalid for new implementations! */
        int PCR_repetition_error; /* 2.3a */
        int PCR_discontinuity_indicator_error; /* 2.3b */
        int PCR_accuracy_error; /* 2.4 */
        int PTS_error; /* 2.5 ---- repetition_error */
#define ERR_2_6_0 (1<<0) /* scrambling program without CAT */
#define ERR_2_6_1 (1<<1) /* table_id error in PID 0x0001 */
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

        /* other errors */
        int adaption_field_control_error; /* 00 */
        int wild_pcr_packet; /* no program use this pcr packet */
        int normal_section_length_error; /* bad normal section length */
        int private_section_length_error; /* bad private section length */
        int pat_pid_error; /* pat table not in 0x0000 */
        int cat_pid_error; /* cat table not in 0x0001 */
        int pmt_pid_error; /* pmt table not in pmt pid of pat */
        int nit_pid_error; /* nit table not in 0x0010 */
        int sdt_pid_error; /* sdt table not in 0x0011 */
        int descriptor_error; /* wrong descriptor */
        int program_info_length_error; /* program_info_length too big */
        int es_info_length_error; /* es_info_length too big */
        int table_id_extension_error; /* table_id_extension != transport_stream_id */
        int pes_pid_error; /* pid of pes is psi/si */
        int pes_elem_error; /* pid of pes is not es in pmt */
        int pes_start_code_error; /* pes start code not 0x000001 */
        int pes_packet_length_error; /* pes_packet_length is too large */
        int pes_header_length_error; /* pes_header_length is too large */
        int pts_dts_flags_error; /* pts_dts_flags is 01 */
        int pmt_section_number_error; /* pmt section_number|last_section_number not 0x00 */
#define ERR_4_0_0 (1<<0) /* PAT changed */
#define ERR_4_0_1 (1<<1) /* CAT changed */
#define ERR_4_0_2 (1<<2) /* PMT changed */
        int section_crc32_error; /* sect->CRC_32 changed */
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
        uint8_t private_data_byte[183];
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
        uint8_t PES_private_data[16]; /* 128-bit */
        uint8_t pack_field_length;
        uint8_t program_packet_sequence_counter; /* 7-bit */
        uint8_t MPEG1_MPEG2_identifier; /* 1-bit */
        uint8_t original_stuff_length; /* 6-bit */
        uint8_t P_STD_buffer_scale; /* 1-bit */
        uint16_t P_STD_buffer_size; /* 13-bit */
        uint8_t PES_extension_field_length; /* 7-bit */
};

/* node of CA descriptor list */
struct ts_ca {
        struct znode cvfl; /* common variable for list */

        uint16_t CA_system_ID;
        uint16_t CA_PID;
};

/* node of section list */
struct ts_sect {
        struct znode cvfl; /* common variable for list */

        /* section data */
        /*@temp@*/
        uint8_t *section; /* point to the section buffer: 3 + section_length */

        /* section info */
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
        uint32_t CRC_32; /* for check section modification */

        int check_CRC; /* bool, some table do not need to check CRC_32 */
        int type; /* TS_TYPE_xxx */
};

/* node of PSI/SI table list */
struct ts_tabl {
        struct znode cvfl; /* common variable for list */

        /*@temp@*/
        struct ts_sect *sect0; /* section list of this table */
        uint8_t table_id; /* 0x00~0xFF */
        uint8_t version_number;
        uint8_t last_section_number;
        int64_t STC; /* for pid->sect_interval */
};

/* node of elementary list */
struct ts_elem {
        struct znode cvfl; /* common variable for list */

        uint16_t PID; /* 13-bit */
        int type; /* TS_TYPE_xxx */
        uint8_t stream_type;
        int es_info_len;
        /*@temp@*/
        uint8_t *es_info; /* point to NULL if len is 0 */
        struct ts_ca *ca0; /* CA descriptor list */

        /* for PTS/DTS mark */
        int64_t PTS; /* last PTS, for obj->PTS_interval */
        int64_t DTS; /* last DTS, for obj->DTS_interval */
        int64_t STC; /* STC of last PTS, for err->PTS_error */

        int is_pes_align; /* met first PES head */
};

/* node of program list */
struct ts_prog {
        struct znode cvfl; /* common variable for list */

        uint16_t PMT_PID; /* 13-bit */
        uint16_t PCR_PID; /* 13-bit */
        uint16_t program_number;
        int program_info_len;
        /*@temp@*/
        uint8_t *program_info; /* point to NULL if len is 0 */
        struct ts_ca *ca0; /* CA descriptor list */
        int service_name_len;
        /*@temp@*/
        uint8_t *service_name; /* point to NULL if len is 0 */
        int service_provider_len;
        /*@temp@*/
        uint8_t *service_provider; /* point to NULL if len is 0 */

        /* elementary stream list */
        /*@temp@*/
        struct ts_elem *elem0; /* PMT: elem list of this prog */

        /* PMT table */
        int is_parsed;
        struct ts_tabl tabl; /* table_id is 0x02 */

        /* for STC calc */
        int64_t ADDa; /* PCR packet a: packet address */
        int64_t PCRa; /* PCR packet a: PCR value */
        int64_t ADDb; /* PCR packet b: packet address */
        int64_t PCRb; /* PCR packet b: PCR value */
        int is_STC_sync; /* true: PCRa and PCRb OK, STC can be calc */
};

/* node of packet list, for ts2sect() or sect2ts() */
struct ts_pkt {
        struct znode cvfl; /* common variable for list */

        uint8_t pkt[TS_PKT_SIZE];
        uint8_t payload_unit_start_indicator; /* 1-bit */
        int payload_size;
};

/* node of pid list */
struct ts_pid {
        struct znode cvfl; /* common variable for list */

        uint16_t PID; /* 13-bit */
        int type; /* TS_TYPE_xxx */

        /* pay attention: PCR PID maybe belongs to many prog!!! */
        /*@temp@*/
        struct ts_prog *prog; /* should be prog0 if does not belong to any program */
        /*@temp@*/
        struct ts_elem *elem; /* should be NULL if not video or audio packet */

        /* for continuity_counter check */
        int is_CC_sync;
        uint8_t CC; /* 4-bit */

        /* for statistic */
        uint32_t cnt; /* packet received from last PCR */
        uint32_t lcnt; /* packet received from PCRa to PCRb */
        uint32_t cnt_es; /* es byte received from last PCR */
        uint32_t lcnt_es; /* es byte received from PCRa to PCRb */
        uint32_t cnt_es_from_pesh; /* es byte received from last PES head */
        uint32_t cnt_es_of_last_pes; /* es byte received of last PES */

        /* only for PID with PSI/SI */
        /*@temp@*/
        struct ts_pkt *pkt0; /* packets of a section */
        int payload_total; /* accumulate size_of_payload packet by packet */
        int has_new_sech; /* true, if second section head in pkt list */
        int sech3_idx; /* 0~3, 3 means sech3 is OK */
        uint8_t sech3[3]; /* collect first 3-byte to get section_length */
        uint8_t table_id; /* TABLE_ID_TABLE */
        uint16_t section_length; /* 12-bit */
};

/* input: information about one packet, tell me as more as you can :-) */
struct ts_ipt {
        uint8_t TS[TS_PKT_SIZE]; /* TS data */
        uint8_t RS[16]; /* RS data */
        int64_t ADDR; /* address of sync-byte(unit: byte) */
        int64_t ATS; /* arrive timestamp, SONY style */
        int64_t CTS; /* receive timestamp */

        /* 0 means corresponding data can not be used */
        int has_ts; /* data in TS[] is OK */
        int has_rs; /* data in RS[] is OK */
        int has_addr; /* data of ADDR is OK */
        int has_ats; /* data of ATS is OK */
        int has_cts; /* data of CTS is OK */
};

/* configurations for libts */
struct ts_cfg {
        int need_cc;   /* not 0: parse CC */
        int need_af;   /* not 0: parse AF(PCR) */
        int need_timestamp; /* not 0: calculate CTS, STC,  */
        int need_psi;  /* not 0: parse PSI */
        int need_si;   /* not 0: parse SI */
        int need_pes;  /* not 0: parse PES head(PTS, DTS) */
        int need_pes_align; /* not 0: ignore data before first PES head */
        int need_statistic; /* not 0: need statistic information */
};

/* object about one transfer stream */
struct ts_obj {
        struct ts_ipt ipt; /* input */
        struct ts_cfg cfg; /* config */

        /* CTS */
        int64_t CTS; /* according to clock of real time, MUX or prog0->PCR */
        int64_t CTS_base;
        int64_t CTS_ext;
        int64_t lATS; /* last ATS: for calc dCTS(for CTS) in ATS mode */
        int64_t CTS0; /* start time of each statistic interval */

        /* STC */
        int64_t STC; /* timestamp according to prog->PCR(or prog0->PCR if pkt has no prog) */
        int64_t STC_base;
        int16_t STC_ext;

        /* CC */
        int CC_wait;
        int CC_find;
        int CC_lost; /* lost != 0 means CC wrong */

        /* AF */
        /*@temp@*/
        uint8_t *AF; /* point to adaptation_fields in ipt.TS[] */
        int AF_len; /* 0 means no AF */

        /* PCR */
        int has_pcr;
        int64_t PCR;
        int64_t PCR_base;
        int16_t PCR_ext;
        int64_t PCR_repetition; /* PCR packet arrive time interval */
        int64_t PCR_continuity; /* PCR value interval */
        int64_t PCR_jitter; /* PCR - STC */

        /* PES */
        /*@temp@*/
        uint8_t *PES; /* point to PES fragment in ipt.TS[] */
        int PES_len; /* 0 means no PES */

        /* PTS */
        int has_pts;
        int64_t PTS;
        int64_t PTS_repetition; /* PTS packet arrive time interval */
        int64_t PTS_continuity; /* PTS value interval */
        int64_t PTS_minus_STC;

        /* DTS */
        int has_dts;
        int64_t DTS;
        int64_t DTS_continuity;
        int64_t DTS_minus_STC;

        /* ES */
        /*@temp@*/
        uint8_t *ES; /* point to ES fragment in ipt.TS[] */
        int ES_len; /* 0 means no ES */

        uint16_t concerned_pid; /* used for PSI parsing */
        uint16_t PID;

        /*@temp@*/
        struct ts_pid *pid; /* point to the node in pid_list */

        /* TS information */
        int64_t ADDR; /* address of sync-byte(unit: byte) */
        int64_t cnt; /* count of this packet in this stream, start from 0 */

        struct ts_tsh tsh; /* info about ts head of this packet */
        struct ts_af af; /* info about af of this packet */
        struct ts_pesh pesh; /* info about pesh of this packet */
        /*@temp@*/
        struct ts_pid *pid0; /* pid list of this stream */

        /* PSI/SI table */
        uint16_t transport_stream_id;
        int has_got_transport_stream_id;
        /*@temp@*/
        struct ts_sect *sect; /* point to the node in sect_list */
        int64_t sect_interval;
        uint32_t CRC_32;
        uint32_t CRC_32_calc;
        int is_pat_pmt_parsed;
        int is_psi_si_parsed;
        int is_psi_si;
        /*@temp@*/
        struct ts_prog *prog0; /* PAT: program list of this stream */
        /*@temp@*/
        struct ts_tabl *tabl0; /* PSI/SI table except PMT */
        struct ts_ca *ca0; /* CAT: CA descriptor list of this stream */

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

        int has_ess; /* es size of last PES packet ready */

        /* for CAT_error */
        int has_scrambling; /* meet PID with scrambling */
        int has_CAT; /* meet CAT */

        /* error */
        int has_err; /* check err struct for detail information */
        struct ts_err err;

        /* special variables for ts object */
        int state;
        /*@temp@*/
        void *mp; /* id of buddy memory pool, for list malloc and free */

        /* special variables for packet analyse */
        /*@temp@*/
        uint8_t *cur; /* point to the current data in TS[] */
        /*@temp@*/
        uint8_t *tail; /* point to the next data after TS[] */
};

/*@only@*/
/*@null@*/
struct ts_obj *ts_create(/*@null@*/ void *mp);
int ts_destroy(/*@only@*/ /*@null@*/ struct ts_obj *obj);

/* cmd */
#define TS_INIT         (0) /* init object for new application */
#define TS_SCFG         (1) /* set ts_cfg to object */
#define TS_TIDY         (2) /* tidy wild pointer in object */
int ts_ioctl(struct ts_obj *obj, int cmd, void *arg);

int ts_parse_tsh(struct ts_obj *obj);
int ts_parse_tsb(struct ts_obj *obj);

uint32_t ts_crc(void *buf, size_t size, int mode);

/* calculate timestamp:
 *      t0: [0, ovf);
 *      t1: [0, ovf);
 *      td: [-hovf, +hovf)
 *      ovf: overflow, (ovf > 0 and ovf is even)
 */

/* return: t1 = t0 + td */
int64_t ts_timestamp_add(int64_t t0, int64_t td, int64_t ovf);

/* return: td = t1 - t0 */
int64_t ts_timestamp_diff(int64_t t1, int64_t t0, int64_t ovf);

#ifdef __cplusplus
}
#endif

#endif /* _TS_H */
