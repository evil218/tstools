/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: ts.c
// Purpose: analyse ts stream
// To build: gcc -std-c99 -c ts.c
//=============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> // for uint?_t, etc
#include <string.h> // for memcpy, etc

#include "error.h"
#include "ts.h"

#define BIT(n)                          (1<<(n))

#define MIN_USER_PID                    0x0020
#define MAX_USER_PID                    0x1FFE

//=============================================================================
// struct definition
//=============================================================================
typedef struct
{
        uint32_t sync_byte:8;
        uint32_t transport_error_indicator:1;
        uint32_t payload_unit_start_indicator:1;
        uint32_t transport_priority:1;
        uint32_t PID:13;
        uint32_t transport_scrambling_control:2;
        uint32_t adaption_field_control:2;
        uint32_t continuity_counter:4;
}
ts_t;

typedef struct
{
        uint32_t adaption_field_length:8;
        uint32_t discontinuity_indicator:1;
        uint32_t random_access_indicator:1;
        uint32_t elementary_stream_priority_indicator:1;
        uint32_t PCR_flag:1;
        uint32_t OPCR_flag:1;
        uint32_t splicing_pointer_flag:1;
        uint32_t transport_private_data_flag:1;
        uint32_t adaption_field_extension_flag:1;
        uint64_t program_clock_reference_base:33;
        uint32_t program_clock_reference_extension:9;
        uint64_t original_program_clock_reference_base:33;
        uint32_t original_program_clock_reference_extension:9;
        uint32_t splice_countdown:8;
        // ...
}
af_t;

typedef struct
{
        uint32_t packet_start_code_prefix:24;
        uint32_t stream_id:8;
        uint32_t PES_packet_length:16;
        uint32_t PES_scrambling_control:2;
        uint32_t PES_priority:1;
        uint32_t data_alignment_indicator:1;
        uint32_t copyright:1;
        uint32_t original_or_copy:1;
        uint32_t PTS_DTS_flags:2;
        uint32_t ESCR_flag:1;
        uint32_t ES_rate_flag:1;
        uint32_t DSM_trick_mode_flag:1;
        uint32_t additional_copy_info_flag:1;
        uint32_t PES_CRC_flag:1;
        uint32_t PES_extension_flag:1;
        uint32_t PES_header_data_length:8;
        uint64_t PTS:33;
        uint64_t DTS:33;
        uint64_t ESCR_base:33;
        uint32_t ESCR_extension:9;
        uint32_t ES_rate:22;
        uint32_t trick_mode_control:3;
        uint32_t field_id:2;
        uint32_t intra_slice_refresh:1;
        uint32_t frequency_truncation:2;
        uint32_t rep_cntrl:5;
        uint32_t additional_copy_info:7;
        uint32_t previous_PES_packet_CRC:16;
        uint32_t PES_private_data_flag:1;
        uint32_t pack_header_field_flag:1;
        uint32_t program_packet_sequence_counter_flag:1;
        uint32_t P_STD_buffer_flag:1;
        uint32_t PES_extension_flag_2:1;
        uint8_t  PES_private_data[16]; // 128-bit
        uint32_t pack_field_length:8;
        uint32_t program_packet_sequence_counter:7;
        uint32_t MPEG1_MPEG2_identifier:1;
        uint32_t original_stuff_length:6;
        uint32_t P_STD_buffer_scale:1;
        uint32_t P_STD_buffer_size:13;
        uint32_t PES_extension_field_length:7;
}
pes_t;

typedef struct
{
        uint32_t pointer_field:8; // 
        uint32_t table_id:8; // TABLE_ID_TABLE
        uint32_t sectin_syntax_indicator:1;
        uint32_t pad0:1; // '0'
        uint32_t reserved0:2;
        uint32_t section_length:12;
        union
        {
                uint32_t idx:16;
                uint32_t transport_stream_id:16;
                uint32_t program_number:16;
        }idx;
        uint32_t reserved1:2;
        uint32_t version_number:5;
        uint32_t current_next_indicator:1;
        uint32_t section_number:8;
        uint32_t last_section_number:8;

        uint32_t CRC3:8; // most significant byte
        uint32_t CRC2:8;
        uint32_t CRC1:8;
        uint32_t CRC0:8; // last significant byte
}
psi_t;

typedef struct
{
        uint16_t min;   // PID range
        uint16_t max;   // PID range
        int dCC;        // delta of CC field: 0 or 1
        char *sdes;     // short description
        char *ldes;     // long description
}
ts_pid_table_t;

typedef struct
{
        uint8_t min;    // table ID range
        uint8_t max;    // table ID range
        char *sdes;     // short description
        char *ldes;     // long description
}
table_id_table_t;

typedef struct
{
        uint8_t *p; // point to rslt.line
        int len;

        ts_t ts;
        af_t af;
        pes_t pes;
        psi_t psi;

        uint32_t pkg_size; // 188 or 204
        int state;

        ts_rslt_t rslt;
}
obj_t;

//=============================================================================
// const definition
//=============================================================================
static const ts_pid_table_t TS_PID_TABLE[] =
{
        /* 0*/{0x0000, 0x0000, 1, " PAT", "program association section"},
        /* 1*/{0x0001, 0x0001, 1, " CAT", "conditional access section"},
        /* 2*/{0x0002, 0x0002, 1, "TSDT", "transport stream description section"},
        /* 3*/{0x0003, 0x000f, 0, "____", "reserved"},
        /* 4*/{0x0010, 0x0010, 1, " NIT", "NIT/ST, network information section"},
        /* 5*/{0x0011, 0x0011, 1, " SDT", "SDT/BAT/ST, service description section"},
        /* 6*/{0x0012, 0x0012, 1, " EIT", "EIT/ST, event information section"},
        /* 7*/{0x0013, 0x0013, 1, " RST", "RST/ST, running status section"},
        /* 8*/{0x0014, 0x0014, 1, " TDT", "TDT/TOT/ST, time data section"},
        /* 9*/{0x0015, 0x0015, 1, "____", "Network Synchroniztion"},
        /*10*/{0x0016, 0x001b, 1, "____", "Reserved for future use"},
        /*11*/{0x001c, 0x001c, 1, "____", "Inband signaling"},
        /*12*/{0x001d, 0x001d, 1, "____", "Measurement"},
        /*13*/{0x001e, 0x001e, 1, " DIT", "discontinuity information section"},
        /*14*/{0x001f, 0x001f, 1, " SIT", "selection information section"},
        /*15*/{0x0020, 0x1ffe, 1, "TBD.", "user define"},
        /*16*/{0x1fff, 0x1fff, 0, "NULL", "empty package"}
};

static const table_id_table_t TABLE_ID_TABLE[] =
{
        /* 0*/{0x00, 0x00, " PAT", "program association section"},
        /* 1*/{0x01, 0x01, " CAT", "conditional access section"},
        /* 2*/{0x02, 0x02, " PMT", "program map section"},
        /* 3*/{0x03, 0x03, "TSDT", "transport stream description section"},
        /* 4*/{0x04, 0x3f, "????", "reserved"},
        /* 5*/{0x40, 0x40, " NIT", "network information section-actual network"},
        /* 6*/{0x41, 0x41, " NIT", "network information section-other network"},
        /* 7*/{0x42, 0x42, " SDT", "service description section-actual transport stream"},
        /* 8*/{0x43, 0x45, "????", "reserved for future use"},
        /* 9*/{0x46, 0x46, " SDT", "service description section-other transport stream"},
        /*10*/{0x47, 0x49, "????", "reserved for future use"},
        /*11*/{0x4a, 0x4a, " BAT", "bouquet association section"},
        /*12*/{0x4b, 0x4d, "????", "reserved for future use"},
        /*13*/{0x4e, 0x4e, " EIT", "event information section-actual transport stream,P/F"},
        /*14*/{0x4f, 0x4f, " EIT", "event information section-other transport stream,P/F"},
        /*15*/{0x50, 0x5f, " EIT", "event information section-actual transport stream,schedule"},
        /*16*/{0x60, 0x6f, " EIT", "event information section-other transport stream,schedule"},
        /*17*/{0x70, 0x70, " TDT", "time data section"},
        /*18*/{0x71, 0x71, " RST", "running status section"},
        /*19*/{0x72, 0x72, "  ST", "stuffing section"},
        /*20*/{0x73, 0x73, " TOT", "time offset section"},
        /*21*/{0x74, 0x7d, "????", "reserved for future use"},
        /*22*/{0x7e, 0x7e, " DIT", "discontinuity information section"},
        /*23*/{0x7f, 0x7f, " SIT", "selection information section"},
        /*24*/{0x80, 0xfe, "????", "user defined"},
        /*25*/{0xff, 0xff, "????", "reserved"}
};

static const char * PAT_PID = " PAT_PID";
static const char * PMT_PID = " PMT_PID";
static const char * VID_PID = " VID_PID";
static const char * VID_PCR = " VID_PCR";
static const char * AUD_PID = " AUD_PID";
static const char * AUD_PCR = " AUD_PCR";
static const char * PCR_PID = " PCR_PID";
static const char * SIT_PID = " SIT_PID";
static const char * UNO_PID = " UNO_PID";
#if 0
static const char * CAT_PID = " CAT_PID";
static const char *TSDT_PID = "TSDT_PID";
static const char * NIT_PID = " NIT_PID";
static const char * SDT_PID = " SDT_PID";
static const char * BAT_PID = " BAT_PID";
static const char * EIT_PID = " EIT_PID";
static const char * TDT_PID = " TDT_PID";
static const char * RST_PID = " RST_PID";
static const char *  ST_PID = "  ST_PID";
static const char * TOT_PID = " TOT_PID";
static const char * DIT_PID = " DIT_PID";
static const char *NULL_PID = "NULL_PID";
#endif

//=============================================================================
// enum definition
//=============================================================================
enum
{
        STATE_NEXT_PAT,
        STATE_NEXT_PMT,
        STATE_NEXT_PKG
};

//=============================================================================
// sub-function declaration
//=============================================================================
static int state_next_pat(obj_t *obj);
static int state_next_pmt(obj_t *obj);
static int state_next_pkg(obj_t *obj);

static int dump_TS(obj_t *obj);   // for debug
static int parse_TS(obj_t *obj);  // ts layer information
static int parse_AF(obj_t *obj);  // adaption_fields information
static int parse_PSI(obj_t *obj); // psi information
static int parse_PES(obj_t *obj); // pes layer information
static int parse_PES_switch(obj_t *obj);
static int parse_PES_detail(obj_t *obj);

static int parse_PAT_load(obj_t *obj);
static int parse_PMT_load(obj_t *obj);

static int search_in_TS_PID_TABLE(uint16_t pid);
static int pids_add(struct LIST *list, ts_pid_t *pids);
static int is_pmt_pid(obj_t *obj);
static int is_unparsed_prog(obj_t *obj);
static int is_all_prog_parsed(obj_t *obj);
static const char *PID_type(uint8_t stream_type);
static ts_pid_t *pids_match(struct LIST *list, uint16_t pid);

//=============================================================================
// public function definition
//=============================================================================
int tsCreate(int pkg_size, ts_rslt_t **rslt)
{
        obj_t *obj;

        obj = (obj_t *)malloc(sizeof(obj_t));
        if(NULL == obj)
        {
                DBG(ERR_MALLOC_FAILED);
                return (int)NULL;
        }

        *rslt = &(obj->rslt);
        (*rslt)->prog_list = list_init();
        (*rslt)->pid_list = list_init();

        obj->pkg_size = pkg_size;
        obj->state = STATE_NEXT_PAT;

        (*rslt)->CC_lost = 0;
        (*rslt)->is_psi_parsed = 0;
        (*rslt)->concerned_pid = 0x0000; // PAT_PID

        return (int)obj;
}

int tsDelete(int id)
{
        obj_t *obj;
        ts_rslt_t *rslt;

        obj = (obj_t *)id;
        if(NULL == obj)
        {
                DBG(ERR_BAD_ID);
                return -ERR_BAD_ID;
        }
        else
        {
                rslt = &(obj->rslt);
                list_free(rslt->prog_list);
                list_free(rslt->pid_list);

                free(obj);

                return 0;
        }
}

int tsParseTS(int id, void *pkg)
{
        obj_t *obj;

        obj = (obj_t *)id;
        if(NULL == obj)
        {
                DBG(ERR_BAD_ID);
                return -ERR_BAD_ID;
        }

        memcpy(obj->rslt.line, pkg, obj->pkg_size);
        obj->p = obj->rslt.line;
        obj->len = obj->pkg_size;

        return parse_TS(obj);
}

int tsParseOther(int id)
{
        obj_t *obj;

        obj = (obj_t *)id;
        if(NULL == obj)
        {
                DBG(ERR_BAD_ID);
                return -ERR_BAD_ID;
        }

        switch(obj->state)
        {
                case STATE_NEXT_PAT:
                        state_next_pat(obj);
                        break;
                case STATE_NEXT_PMT:
                        state_next_pmt(obj);
                        break;
                case STATE_NEXT_PKG:
                default:
                        state_next_pkg(obj);
                        break;
        }

        return 0;
}

//=============================================================================
// sub-function definition
//=============================================================================
static int state_next_pat(obj_t *obj)
{
        ts_t *ts = &(obj->ts);

        if(0x0000 != ts->PID)
        {
                return -1;
        }

        parse_PSI(obj);
        parse_PAT_load(obj);
        obj->state = STATE_NEXT_PMT;

        if(is_all_prog_parsed(obj))
        {
                obj->rslt.is_psi_parsed = 1;
                obj->state = STATE_NEXT_PKG;
        }

        return 0;
}

static int state_next_pmt(obj_t *obj)
{
        if(!is_pmt_pid(obj))
        {
                return -1;
        }

        parse_PSI(obj);
        if(is_unparsed_prog(obj))
        {
                parse_PMT_load(obj);
        }

        if(is_all_prog_parsed(obj))
        {
                obj->rslt.is_psi_parsed = 1;
                obj->state = STATE_NEXT_PKG;
        }

        return 0;
}

static int state_next_pkg(obj_t *obj)
{
        ts_t *ts = &(obj->ts);
        af_t *af = &(obj->af);
        ts_rslt_t *rslt = &(obj->rslt);
        ts_pid_t *pids;

        pids = pids_match(obj->rslt.pid_list, ts->PID);
        if(NULL == pids)
        {
                if(MIN_USER_PID <= ts->PID && ts->PID <= MAX_USER_PID)
                {
                        fprintf(stderr, "unknown PID: 0x%04X\n", ts->PID);
                        dump_TS(obj);
                }
                else
                {
                        //fprintf(stderr, "new PID: 0x%04X\n", ts->PID);
                        //add_reserved_PID(obj);
                }
                return -1;
        }

        // CC
        if(pids->is_CC_sync)
        {
                int lost;

                pids->CC += pids->dCC;
                lost  = (int)ts->continuity_counter;
                lost -= (int)pids->CC;
                if(lost < 0)
                {
                        lost += 16;
                }

                rslt->CC_wait = pids->CC;
                rslt->CC_find = ts->continuity_counter;
                rslt->CC_lost = lost;

                pids->CC = ts->continuity_counter;
        }
        else
        {
                pids->CC = ts->continuity_counter;
                pids->is_CC_sync = 1;

                rslt->CC_wait = pids->CC;
                rslt->CC_find = ts->continuity_counter;
                rslt->CC_lost = 0;
        }

        // PCR
        if(rslt->has_PCR)
        {
                rslt->PCR_base = af->program_clock_reference_base;
                rslt->PCR_ext += af->program_clock_reference_extension;

                rslt->PCR  = rslt->PCR_base;
                rslt->PCR *= 300;
                rslt->PCR += rslt->PCR_ext;
        }

        // PES head & ES data
        if(pids->is_track)
        {
                parse_PES(obj);
        }

        return 0;
}

static int dump_TS(obj_t *obj)
{
        uint32_t i;

        for(i = 0; i < obj->pkg_size; i++)
        {
                fprintf(stderr, " %02X", obj->rslt.line[i]);
        }
        fprintf(stderr, "\n");

        return 0;
}

static int parse_TS(obj_t *obj)
{
        uint8_t dat;
        ts_t *ts = &(obj->ts);
        af_t *af = &(obj->af);
        ts_rslt_t *rslt = &(obj->rslt);

        rslt->has_PCR = 0;
        rslt->has_PTS = 0;
        rslt->has_DTS = 0;

        dat = *(obj->p)++; obj->len--;
        ts->sync_byte = dat;
        if(0x47 != ts->sync_byte)
        {
                fprintf(stderr, "sync_byte(0x47) wrong: %02X\n", dat);
                dump_TS(obj);
                return -1;
        }

        dat = *(obj->p)++; obj->len--;
        ts->transport_error_indicator = (dat & BIT(7)) >> 7;
        ts->payload_unit_start_indicator = (dat & BIT(6)) >> 6;
        ts->transport_priority = (dat & BIT(5)) >> 5;
        ts->PID = dat & 0x1F;

        dat = *(obj->p)++; obj->len--;
        ts->PID <<= 8;
        ts->PID |= dat;
        rslt->pid = ts->PID; // record into rslt struct

        dat = *(obj->p)++; obj->len--;
        ts->transport_scrambling_control = (dat & (BIT(7) | BIT(6))) >> 6;
        ts->adaption_field_control = (dat & (BIT(5) | BIT(4))) >> 4;;
        ts->continuity_counter = dat & 0x0F;

        if(BIT(1) & ts->adaption_field_control)
        {
                parse_AF(obj);
        }
        else
        {
                af->adaption_field_length = 0x00;
        }

        rslt->PES_len = 0; // clear PES length
        rslt->ES_len = 0; // clear ES length
        if(BIT(0) & ts->adaption_field_control)
        {
                // data_byte, PSI or PES
        }

        return 0;
}

static int parse_AF(obj_t *obj)
{
        int i;
        uint8_t dat;
        af_t *af = &(obj->af);
        ts_rslt_t *rslt = &(obj->rslt);

        dat = *(obj->p)++; obj->len--;
        af->adaption_field_length = dat;
        if(0x00 == af->adaption_field_length)
        {
                return 0;
        }

        dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
        af->discontinuity_indicator = (dat & BIT(7)) >> 7;
        af->random_access_indicator = (dat & BIT(6)) >> 6;
        af->elementary_stream_priority_indicator = (dat & BIT(5)) >> 5;
        af->PCR_flag = (dat & BIT(4)) >> 4;
        af->OPCR_flag = (dat & BIT(3)) >> 3;
        af->splicing_pointer_flag = (dat & BIT(2)) >> 2;
        af->transport_private_data_flag = (dat & BIT(1)) >> 1;
        af->adaption_field_extension_flag = (dat & BIT(0)) >> 0;

        if(af->PCR_flag)
        {
                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_base = dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_base <<= 1;
                af->program_clock_reference_base |= ((dat & BIT(7)) >> 7);
                af->program_clock_reference_extension = ((dat & BIT(0)) >> 0);

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->program_clock_reference_extension <<= 8;
                af->program_clock_reference_extension |= dat;

                rslt->has_PCR = 1;
        }
        if(af->OPCR_flag)
        {
                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_base = dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_base <<= 8;
                af->original_program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_base <<= 8;
                af->original_program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_base <<= 8;
                af->original_program_clock_reference_base |= dat;

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_base <<= 1;
                af->original_program_clock_reference_base |= ((dat & BIT(7)) >> 7);
                af->original_program_clock_reference_extension = ((dat & BIT(0)) >> 0);

                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->original_program_clock_reference_extension <<= 8;
                af->original_program_clock_reference_extension |= dat;
        }
        if(af->splicing_pointer_flag)
        {
                dat = *(obj->p)++; obj->len--; af->adaption_field_length--;
                af->splice_countdown = dat;
        }
        if(af->transport_private_data_flag)
        {
                //...
        }
        if(af->adaption_field_extension_flag)
        {
                //...
        }

        // pass stuffing byte
        for(i = af->adaption_field_length; i > 0; i--)
        {
                dat = *(obj->p)++; obj->len--;
        }

        return 0;
}

static int parse_PSI(obj_t *obj)
{
        uint8_t dat;
        psi_t *psi = &(obj->psi);

        dat = *(obj->p)++; obj->len--;
        psi->pointer_field = dat; // special byte before PSI!

        dat = *(obj->p)++; obj->len--;
        psi->table_id = dat;

        dat = *(obj->p)++; obj->len--;
        psi->sectin_syntax_indicator = (dat & BIT(7)) >> 7;
        psi->section_length = dat & 0x0F;

        dat = *(obj->p)++; obj->len--;
        psi->section_length <<= 8;
        psi->section_length |= dat;
        if(psi->section_length > obj->len)
        {
                fprintf(stderr, "PSI_section_length(%d) is too long!\n", psi->section_length);
                return -1;
        }
        // ignore data after CRC of this PSI
        obj->len = psi->section_length;

        dat = *(obj->p)++; obj->len--;
        psi->idx.idx = dat;

        dat = *(obj->p)++; obj->len--;
        psi->idx.idx <<= 8;
        psi->idx.idx |= dat;

        dat = *(obj->p)++; obj->len--;
        psi->version_number = (dat & 0x3E) >> 1;
        psi->current_next_indicator = dat & BIT(0);

        dat = *(obj->p)++; obj->len--;
        psi->section_number = dat;

        dat = *(obj->p)++; obj->len--;
        psi->last_section_number = dat;

        return 0;
}

static int parse_PES(obj_t *obj)
{
        uint8_t dat;
        ts_t *ts = &(obj->ts);
        pes_t *pes = &(obj->pes);
        ts_rslt_t *rslt = &(obj->rslt);

        //fprintf(stderr, "%02X %02X %02X %02X ", rslt->line[4], rslt->line[5], rslt->line[6], rslt->line[7]);

        // record PES data
        rslt->PES_len = obj->len; //fprintf(stderr, "pes len = %d, ", rslt->PES_len);
        rslt->PES_buf = obj->p;

        if(ts->payload_unit_start_indicator)
        {
                // PES head start
                dat = *(obj->p)++; obj->len--;
                pes->packet_start_code_prefix = dat;

                dat = *(obj->p)++; obj->len--;
                pes->packet_start_code_prefix <<= 8;
                pes->packet_start_code_prefix |= dat;

                dat = *(obj->p)++; obj->len--;
                pes->packet_start_code_prefix <<= 8;
                pes->packet_start_code_prefix |= dat;

                if(0x000001 != pes->packet_start_code_prefix)
                {
                        fprintf(stderr, "PES packet start code prefix error(0x%06X)!\n",
                                pes->packet_start_code_prefix);
                        dump_TS(obj);
                        return -1;
                }

                dat = *(obj->p)++; obj->len--;
                pes->stream_id = dat;

                dat = *(obj->p)++; obj->len--;
                pes->PES_packet_length = dat;

                dat = *(obj->p)++; obj->len--;
                pes->PES_packet_length <<= 8;
                pes->PES_packet_length |= dat; // 0x0000 for many video pes
                //fprintf(stderr, "PES_packet_length = %d, ", pes->PES_packet_length);
                //fprintf(stderr, "PES_packet_length = 0x%X, ", pes->PES_packet_length);

                parse_PES_switch(obj);
        }

        rslt->ES_len = obj->len; //fprintf(stderr, "es len = %d\n", rslt->ES_len);
        rslt->ES_buf = obj->p;

        return 0;
}

static int parse_PES_switch(obj_t *obj)
{
        int i;
        uint8_t dat;
        pes_t *pes = &(obj->pes);

        if(0xBE == pes->stream_id) // padding_stream
        {
                // subsequent pes->PES_packet_length data is padding_byte, pass
                if(pes->PES_packet_length > obj->len)
                {
                        fprintf(stderr, "PES_packet_length(%d) for padding_stream is too large!\n",
                                pes->PES_packet_length);
                        return -1;
                }
                for(i = pes->PES_packet_length; i > 0; i--)
                {
                        dat = *(obj->p)++; obj->len--;
                }
        }
        else if(0xBC == pes->stream_id || // program_stream_map
                0xBF == pes->stream_id || // private_stream_2
                0xF0 == pes->stream_id || // ECM
                0xF1 == pes->stream_id || // EMM
                0xFF == pes->stream_id || // program_stream_directory
                0xF2 == pes->stream_id || // DSMCC_stream
                0xF8 == pes->stream_id    // ITU-T Rec. H.222.1 type E stream
        )
        {
                // pes->PES_packet_length data in pes->buf is PES_packet_data_byte
                // record after return
        }
        else
        {
                parse_PES_detail(obj);
        }

        return 0;
}

static int parse_PES_detail(obj_t *obj)
{
        int i;
        int header_data_len;
        uint8_t dat;
        pes_t *pes = &(obj->pes);
        ts_rslt_t *rslt = &(obj->rslt);

        dat = *(obj->p)++; obj->len--;
        pes->PES_scrambling_control = (dat & (BIT(5) | BIT(4))) >> 4;
        pes->PES_priority = (dat & BIT(3)) >> 3;
        pes->data_alignment_indicator = (dat & BIT(2)) >> 2;
        pes->copyright = (dat & BIT(1)) >> 1;
        pes->original_or_copy = (dat & BIT(0)) >> 0;

        dat = *(obj->p)++; obj->len--;
        pes->PTS_DTS_flags = (dat & (BIT(7) | BIT(6))) >> 6;
        pes->ESCR_flag = (dat & BIT(5)) >> 5;
        pes->ES_rate_flag = (dat & BIT(4)) >> 4;
        pes->DSM_trick_mode_flag = (dat & BIT(3)) >> 3;
        pes->additional_copy_info_flag = (dat & BIT(2)) >> 2;
        pes->PES_CRC_flag = (dat & BIT(1)) >> 1;
        pes->PES_extension_flag = (dat & BIT(0)) >> 0;

        dat = *(obj->p)++; obj->len--;
        pes->PES_header_data_length = dat;
        header_data_len = pes->PES_header_data_length;
        if(header_data_len > obj->len)
        {
                fprintf(stderr, "PES_header_data_length(%d) is too long!\n",
                        header_data_len);
                return -1;
        }

        if(0x02 == pes->PTS_DTS_flags) // '10'
        {
                // PTS
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS = (dat & (BIT(3) | BIT(2) | BIT(1))) >> 1;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS <<= 8;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->PTS <<= 7;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS <<= 8;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->PTS <<= 7;
                pes->PTS |= dat;

                rslt->has_PTS = 1;
                rslt->PTS = pes->PTS;
        }
        else if(0x03 == pes->PTS_DTS_flags) // '11'
        {
                // PTS
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS = (dat & (BIT(3) | BIT(2) | BIT(1))) >> 1;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS <<= 8;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->PTS <<= 7;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PTS <<= 8;
                pes->PTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->PTS <<= 7;
                pes->PTS |= dat;

                rslt->has_PTS = 1;
                rslt->PTS = pes->PTS;

                // DTS
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->DTS = (dat & (BIT(3) | BIT(2) | BIT(1))) >> 1;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->DTS <<= 8;
                pes->DTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->DTS <<= 7;
                pes->DTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->DTS <<= 8;
                pes->DTS |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                dat >>= 1;
                pes->DTS <<= 7;
                pes->DTS |= dat;

                rslt->has_DTS = 1;
                rslt->DTS = pes->DTS;
        }
        else if(0x01 == pes->PTS_DTS_flags) // '01'
        {
                fprintf(stderr, "PTS_DTS_flags error!\n");
                dump_TS(obj);
                return -1;
        }
        else
        {
                // no PTS, no DTS, it's OK!
        }

        if(pes->ESCR_flag)
        {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_base = (dat & (BIT(5) | BIT(4) | BIT(3))) >> 3;
                pes->ESCR_base <<= 2;
                pes->ESCR_base |= (dat & (BIT(1) | BIT(0)));

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_base <<= 8;
                pes->ESCR_base |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_base <<= 5;
                pes->ESCR_base |= ((dat & (BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3))) >> 3);
                pes->ESCR_base <<= 2;
                pes->ESCR_base |= (dat & (BIT(1) | BIT(0)));

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_base <<= 8;
                pes->ESCR_base |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_base <<= 5;
                pes->ESCR_base |= ((dat & (BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3))) >> 3);
                pes->ESCR_extension = (dat & (BIT(1) | BIT(0)));

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ESCR_extension <<= 7;
                pes->ESCR_extension |= (dat >> 1);
        }

        if(pes->ES_rate_flag)
        {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ES_rate = (dat & 0x7F);

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ES_rate <<= 8;
                pes->ES_rate |= dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->ES_rate <<= 7;
                pes->ES_rate |= (dat >> 1);
        }

        if(pes->DSM_trick_mode_flag)
        {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->trick_mode_control = (dat & (BIT(7) | BIT(6) | BIT(5))) >> 5;

                if(0x00 == pes->trick_mode_control)
                {
                        // '000', fast_forward
                        pes->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                        pes->intra_slice_refresh = (dat & (BIT(2))) >> 2;
                        pes->frequency_truncation = (dat & (BIT(1) | BIT(0)));
                }
                else if(0x01 == pes->trick_mode_control)
                {
                        // '001', slow_motion
                        pes->rep_cntrl = (dat & 0x1F);
                }
                else if(0x02 == pes->trick_mode_control)
                {
                        // '010', freeze_frame
                        pes->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                }
                else if(0x03 == pes->trick_mode_control)
                {
                        // '011', fast_reverse
                        pes->field_id = (dat & (BIT(4) | BIT(3))) >> 3;
                        pes->intra_slice_refresh = (dat & (BIT(2))) >> 2;
                        pes->frequency_truncation = (dat & (BIT(1) | BIT(0)));
                }
                else if(0x04 == pes->trick_mode_control)
                {
                        // '100', slow_reverse
                        pes->rep_cntrl = (dat & 0x1F);
                }
        }

        if(pes->additional_copy_info_flag)
        {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->additional_copy_info = (dat & 0x7F);
        }

        if(pes->PES_CRC_flag)
        {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->previous_PES_packet_CRC = dat;

                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->previous_PES_packet_CRC <<= 8;
                pes->previous_PES_packet_CRC |= dat;
        }

        if(pes->PES_extension_flag)
        {
                dat = *(obj->p)++; obj->len--; header_data_len--;
                pes->PES_private_data_flag = (dat & (BIT(7))) >> 7;
                pes->pack_header_field_flag = (dat & (BIT(6))) >> 6;
                pes->program_packet_sequence_counter_flag = (dat & (BIT(5))) >> 5;
                pes->P_STD_buffer_flag = (dat & (BIT(4))) >> 4;
                pes->PES_extension_flag_2 = (dat & (BIT(0))) >> 0;

                if(pes->PES_private_data_flag)
                {
                        for(i = 16; i > 0; i--)
                        {
                                dat = *(obj->p)++; obj->len--; header_data_len--;
                                pes->PES_private_data[i] = dat;
                        }
                }

                if(pes->pack_header_field_flag)
                {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->pack_field_length = dat;

                        // pass pack_header()
                        for(i = pes->pack_field_length; i > 0; i--)
                        {
                                dat = *(obj->p)++; obj->len--; header_data_len--;
                        }
                }

                if(pes->program_packet_sequence_counter_flag)
                {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->program_packet_sequence_counter = (dat & 0x7F);

                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->MPEG1_MPEG2_identifier = (dat & (BIT(6))) >> 6;
                        pes->original_stuff_length = (dat & 0x3F);
                }

                if(pes->P_STD_buffer_flag)
                {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->P_STD_buffer_scale = (dat & (BIT(5))) >> 5;
                        pes->P_STD_buffer_size = (dat & 0x1F);

                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->P_STD_buffer_size <<= 8;
                        pes->P_STD_buffer_size |= dat;
                }

                if(pes->PES_extension_flag_2)
                {
                        dat = *(obj->p)++; obj->len--; header_data_len--;
                        pes->PES_extension_field_length = (dat & 0x7F);

                        // pass PES_extension_field
                        for(i = pes->PES_extension_field_length; i > 0; i--)
                        {
                                dat = *(obj->p)++; obj->len--; header_data_len--;
                        }
                }
        }

        // pass stuffing_byte
        for(; header_data_len > 0; header_data_len--)
        {
                dat = *(obj->p)++; obj->len--;
        }

        // subsequent obj->len data is ES data

        return 0;
}

static int parse_PAT_load(obj_t *obj)
{
        int i;
        uint8_t dat;
        psi_t *psi = &(obj->psi);
        ts_rslt_t *rslt = &(obj->rslt);
        ts_prog_t *prog;
        ts_pid_t ts_pid, *pids;

        pids = &ts_pid;

        // add PAT PID
        pids->PID = 0x0000;
        pids->is_track = 0;
        pids->type = PAT_PID;
        i = search_in_TS_PID_TABLE(pids->PID);
        pids->CC = 0;
        pids->dCC = TS_PID_TABLE[i].dCC;
        pids->is_CC_sync = 0;
        pids->sdes = TS_PID_TABLE[i].sdes;
        pids->ldes = TS_PID_TABLE[i].ldes;
        pids_add(obj->rslt.pid_list, pids);

        while(obj->len > 4)
        {
                // add program
                prog = (ts_prog_t *)malloc(sizeof(ts_prog_t));
                if(NULL == prog)
                {
                        DBG(ERR_MALLOC_FAILED);
                        return -ERR_MALLOC_FAILED;
                }
                prog->track = list_init();
                prog->is_parsed = 0;

                dat = *(obj->p)++; obj->len--;
                prog->program_number = dat;

                dat = *(obj->p)++; obj->len--;
                prog->program_number <<= 8;
                prog->program_number |= dat;

                dat = *(obj->p)++; obj->len--;
                prog->PMT_PID = dat & 0x1F;

                dat = *(obj->p)++; obj->len--;
                prog->PMT_PID <<= 8;
                prog->PMT_PID |= dat;

                pids->PID = prog->PMT_PID;
                pids->is_track = 0;
                i = search_in_TS_PID_TABLE(pids->PID);
                pids->CC = 0;
                pids->dCC = TS_PID_TABLE[i].dCC;
                pids->is_CC_sync = 0;
                pids->sdes = TS_PID_TABLE[i].sdes;
                pids->ldes = TS_PID_TABLE[i].ldes;

                if(0 == prog->program_number)
                {
                        // network PID, not a program
                        pids->type = SIT_PID;
                        free(prog);
                }
                else
                {
                        // PMT PID
                        pids->type = PMT_PID;
                        pids->sdes = TABLE_ID_TABLE[2].sdes;
                        pids->ldes = TABLE_ID_TABLE[2].ldes;
                        list_add(rslt->prog_list, (struct NODE *)prog);
                }

                pids_add(rslt->pid_list, pids);
        }

        dat = *(obj->p)++; obj->len--;
        psi->CRC3 = dat;

        dat = *(obj->p)++; obj->len--;
        psi->CRC2 = dat;

        dat = *(obj->p)++; obj->len--;
        psi->CRC1 = dat;

        dat = *(obj->p)++; obj->len--;
        psi->CRC0 = dat;

        if(0 != obj->len)
        {
                printf("PSI load length error!\n");
        }

        //printf("PROG Count: %u\n", list_count(obj->prog));
        //printf("PIDS Count: %u\n", list_count(obj->pids));

        return 0;
}

static int parse_PMT_load(obj_t *obj)
{
        uint16_t i;
        uint8_t dat;
        uint16_t info_length;
        psi_t *psi;
        struct NODE *node;
        ts_prog_t *prog;
        ts_pid_t ts_pid, *pids;
        ts_track_t *track;
        ts_t *ts;

        ts = &(obj->ts);
        psi = &(obj->psi);
        pids = &ts_pid;

        for(node = obj->rslt.prog_list->head; node; node = node->next)
        {
                prog = (ts_prog_t *)node;
                if(psi->idx.program_number == prog->program_number)
                {
                        break;
                }
        }
        if(!node)
        {
                printf("Wrong PID: 0x%04X\n", ts->PID);
                exit(EXIT_FAILURE);
        }

        prog->is_parsed = 1;
        if(0x02 != psi->table_id)
        {
                // SIT or other PSI
                return -1;
        }

        dat = *(obj->p)++; obj->len--;
        prog->PCR_PID = dat & 0x1F;

        dat = *(obj->p)++; obj->len--;
        prog->PCR_PID <<= 8;
        prog->PCR_PID |= dat;

        // add PCR PID
        pids->PID = prog->PCR_PID;
        pids->is_track = 0;
        pids->type = PCR_PID;
        pids->sdes = " PCR";
        pids->ldes = "program clock reference";
        pids->CC = 0;
        pids->dCC = 0;
        pids->is_CC_sync = 1;
        pids_add(obj->rslt.pid_list, pids);

        // program_info_length
        dat = *(obj->p)++; obj->len--;
        info_length = dat & 0x0F;

        dat = *(obj->p)++; obj->len--;
        info_length <<= 8;
        info_length |= dat;

        // record program_info
        prog->program_info_len = info_length;
        prog->program_info_buf = obj->p;
        for(i = 0; i < info_length; i++)
        {
                dat = *(obj->p)++; obj->len--;
        }

        while(obj->len > 4)
        {
                // add track
                track = (ts_track_t *)malloc(sizeof(ts_track_t));
                if(NULL == track)
                {
                        printf("Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }

                dat = *(obj->p)++; obj->len--;
                track->stream_type = dat;

                dat = *(obj->p)++; obj->len--;
                track->PID = dat & 0x1F;

                dat = *(obj->p)++; obj->len--;
                track->PID <<= 8;
                track->PID |= dat;

                // ES_info_length
                dat = *(obj->p)++; obj->len--;
                info_length = dat & 0x0F;

                dat = *(obj->p)++; obj->len--;
                info_length <<= 8;
                info_length |= dat;

                // record es_info
                track->es_info_len = info_length;
                track->es_info_buf = obj->p;
                for(i = 0; i < info_length; i++)
                {
                        dat = *(obj->p)++; obj->len--;
                }

                track->type = PID_type(track->stream_type);
                list_add(prog->track, (struct NODE *)track);

                // add track PID
                pids->PID = track->PID;
                pids->is_track = 1;
                pids->type = track->type;
                if(VID_PID == pids->type)
                {
                        pids->sdes = " VID";
                        pids->ldes = "video package";
                }
                else if(AUD_PID == pids->type)
                {
                        pids->sdes = " AUD";
                        pids->ldes = "audio package";
                }
                else
                {
                        pids->sdes = "UKNO";
                        pids->ldes = "unknown package";
                }
                pids->CC = 0;
                pids->dCC = 1;
                pids->is_CC_sync = 0;
                pids_add(obj->rslt.pid_list, pids);
        }

        return 0;
}

static int search_in_TS_PID_TABLE(uint16_t pid)
{
        int i;
        int count = sizeof(TS_PID_TABLE) / sizeof(ts_pid_table_t);
        //ts_pid_table_t *table;

        //printf("count of TS_PID_TABLE: %d\n", count);
        for(i = 0; i < count; i++)
        {
                //table = &(TS_PID_TABLE[i]);
                if(TS_PID_TABLE[i].min <= pid && pid <= TS_PID_TABLE[i].max)
                {
                        return i;
                }
        }

        return 0; // PAT for wrong search state
}

static int pids_add(struct LIST *list, ts_pid_t *the_pids)
{
        struct NODE *node;
        ts_pid_t *pids;

        for(node = list->head; node; node = node->next)
        {
                pids = (ts_pid_t *)node;
                if(the_pids->PID == pids->PID)
                {
                        if(PCR_PID == pids->type)
                        {
                                if(VID_PID == the_pids->type)
                                {
                                        pids->is_track = 1;
                                        pids->type = VID_PCR;
                                        pids->sdes = "VPCR";
                                        pids->ldes = "video package with pcr information";
                                }
                                else if(AUD_PID == the_pids->type)
                                {
                                        pids->is_track = 1;
                                        pids->type = AUD_PCR;
                                        pids->sdes = "APCR";
                                        pids->ldes = "audio package with pcr information";
                                }
                                else
                                {
                                        // error
                                }
                                pids->dCC = 1;
                                pids->is_CC_sync = 0;
                        }
                        else
                        {
                                // same PID, omit...
                        }
                        return 0;
                }
        }

        pids = (ts_pid_t *)malloc(sizeof(ts_pid_t));
        if(NULL == pids)
        {
                DBG(ERR_MALLOC_FAILED);
                return -ERR_MALLOC_FAILED;
        }

        pids->PID = the_pids->PID;
        pids->is_track = the_pids->is_track;
        pids->type = the_pids->type;
        pids->sdes = the_pids->sdes;
        pids->ldes = the_pids->ldes;
        pids->CC = the_pids->CC;
        pids->dCC = the_pids->dCC;
        pids->is_CC_sync = the_pids->is_CC_sync;

        list_add(list, (struct NODE *)pids);

        return 0;
}

// (find this pid in pid_list) && (its type is PMT_PID)
static int is_pmt_pid(obj_t *obj)
{
        ts_t *ts = &(obj->ts);
        struct NODE *node;
        ts_pid_t *pids;

        for(node = obj->rslt.pid_list->head; node; node = node->next)
        {
                pids = (ts_pid_t *)node;
                if(ts->PID == pids->PID)
                {
                        if(PMT_PID == pids->type)
                        {
                                return 1;
                        }
                        else
                        {
                                return 0;
                        }
                }
        }
        return 0;
}

// (find this program_number in prog_list) && (!is_parsed)
static int is_unparsed_prog(obj_t *obj)
{
        psi_t *psi = &(obj->psi);
        struct NODE *node;
        ts_prog_t *prog;

        for(node = obj->rslt.prog_list->head; node; node = node->next)
        {
                prog = (ts_prog_t *)node;
                if(psi->idx.program_number == prog->program_number)
                {
                        if(0 == prog->is_parsed)
                        {
                                return 1;
                        }
                        else
                        {
                                return 0;
                        }
                }
        }
        return 0;
}

// all PMT_PID in prog_list is parsed
static int is_all_prog_parsed(obj_t *obj)
{
        struct NODE *node;
        ts_prog_t *prog;

        for(node = obj->rslt.prog_list->head; node; node = node->next)
        {
                prog = (ts_prog_t *)node;
                if(     MIN_USER_PID <= prog->PMT_PID &&
                        MAX_USER_PID >= prog->PMT_PID &&
                        0 == prog->is_parsed
                )
                {
                        obj->rslt.concerned_pid = prog->PMT_PID;
                        return 0;
                }
        }
        return 1;
}

static const char *PID_type(uint8_t stream_type)
{
        switch(stream_type)
        {
                case 0x01: // "ISO/IEC 11172-2 (MPEG-1)"
                case 0x02: // "ISO/IEC 13818-2 (MPEG-2)" or
                        //    "ISO/IEC 11172-2 (MPEG-1 parameter limited)"
                case 0x1B: // "ISO/IEC 14496-10(H.264)"
                        return VID_PID;
                case 0x03: // "ISO/IEC 11172-3 (MPEG-1 layer2)"
                case 0x04: // "ISO/IEC 13818-3 (MPEG-2)"
                case 0x06: // "Dolby A52"
                case 0x81: // "Dolby AC3"
                        return AUD_PID;
                default:
                        return UNO_PID; // unknown
        }
}

static ts_pid_t *pids_match(struct LIST *list, uint16_t pid)
{
        struct NODE *node;
        ts_pid_t *pids;


        for(node = list->head; node; node = node->next)
        {
                pids = (ts_pid_t *)node;
                if(pid == pids->PID)
                {
                        return pids;
                }
        }

        return NULL;
}

//=============================================================================
// THE END.
//=============================================================================
