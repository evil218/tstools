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
        int dCC;        // delta of CC field: 0 or 1
        char *sdes;     // short description
        char *ldes;     // long description
}
pid_type_table_t;

typedef struct
{
        uint16_t min;   // PID range
        uint16_t max;   // PID range
        int     type;   // index of item in PID_TYPE_TABLE[]
}
ts_pid_table_t;

typedef struct
{
        uint8_t min;    // table ID range
        uint8_t max;    // table ID range
        int     type;   // index of item in PID_TYPE_TABLE[]
}
table_id_table_t;

typedef struct
{
        int is_first_pkg;

        uint8_t *p; // point to rslt.line
        int len;

        ts_t ts;
        af_t af;
        pes_t pes;
        psi_t psi;

        uint32_t pkg_size; // 188 or 204
        int state;

        int is_pes_align; // met first PES head

        ts_rslt_t rslt;
}
obj_t;

//=============================================================================
// const definition
//=============================================================================
enum PID_TYPE
{
        // should be synchronize with PID_TYPE_TABLE[]!
        PAT_PID,
        CAT_PID,
        TSDT_PID,
        RSV_PID,
        NIT_PID,
        ST_PID,
        SDT_PID,
        BAT_PID,
        EIT_PID,
        RST_PID,
        TDT_PID,
        TOT_PID,
        NS_PID,
        INB_PID,
        MSU_PID,
        DIT_PID,
        SIT_PID,
        USR_PID,
        PMT_PID,
        VID_PID,
        VID_PCR,
        AUD_PID,
        AUD_PCR,
        PCR_PID,
        NULL_PID,
        UNO_PID,
        BAD_PID
};

static const pid_type_table_t PID_TYPE_TABLE[] =
{
        // should be synchronize with enum PID_TYPE!
        {1, " PAT_PID", "program association section"},
        {1, " CAT_PID", "conditional access section"},
        {1, "TSDT_PID", "transport stream description section"},
        {0, " RSV_PID", "reserved"},
        {1, " NIT_PID", "network information section"},
        {1, "  ST_PID", "stuffing section"},
        {1, " SDT_PID", "service description section"},
        {1, " BAT_PID", "bouquet association section"},
        {1, " EIT_PID", "event information section"},
        {1, " RST_PID", "running status section"},
        {1, " TDT_PID", "time data section"},
        {1, " TOT_PID", "time offset section"},
        {1, "  NS_PID", "Network Synchroniztion"},
        {1, " INB_PID", "Inband signaling"},
        {1, " MSU_PID", "Measurement"},
        {1, " DIT_PID", "discontinuity information section"},
        {1, " SIT_PID", "selection information section"},
        {1, " USR_PID", "user define"},
        {1, " PMT_PID", "program map section"},
        {1, " VID_PID", "video package"},
        {1, " VID_PCR", "video package with PCR"},
        {1, " AUD_PID", "audio package"},
        {1, " AUD_PCR", "audio package with PCR"},
        {0, " PCR_PID", "program counter reference"},
        {0, "NULL_PID", "empty package"},
        {1, " UNO_PID", "unknown"},
        {0, " BAD_PID", "illegal"}
};

static const ts_pid_table_t TS_PID_TABLE[] =
{
        /* 0*/{0x0000, 0x0000,  PAT_PID},
        /* 1*/{0x0001, 0x0001,  CAT_PID},
        /* 2*/{0x0002, 0x0002, TSDT_PID},
        /* 3*/{0x0003, 0x000f,  RSV_PID},
        /* 4*/{0x0010, 0x0010,  NIT_PID}, // NIT/ST
        /* 5*/{0x0011, 0x0011,  SDT_PID}, // SDT/BAT/ST
        /* 6*/{0x0012, 0x0012,  EIT_PID}, // EIT/ST
        /* 7*/{0x0013, 0x0013,  RST_PID}, // RST/ST
        /* 8*/{0x0014, 0x0014,  TDT_PID}, // TDT/TOT/ST
        /* 9*/{0x0015, 0x0015,   NS_PID},
        /*10*/{0x0016, 0x001b,  RSV_PID},
        /*11*/{0x001c, 0x001c,  INB_PID},
        /*12*/{0x001d, 0x001d,  MSU_PID},
        /*13*/{0x001e, 0x001e,  DIT_PID},
        /*14*/{0x001f, 0x001f,  SIT_PID},
        /*15*/{0x0020, 0x1ffe,  USR_PID},
        /*16*/{0x1fff, 0x1fff, NULL_PID},
        /*17*/{0x2000, 0xffff,  BAD_PID}
};

static const table_id_table_t TABLE_ID_TABLE[] =
{
        /* 0*/{0x00, 0x00,  PAT_PID},
        /* 1*/{0x01, 0x01,  CAT_PID},
        /* 2*/{0x02, 0x02,  PMT_PID},
        /* 3*/{0x03, 0x03, TSDT_PID},
        /* 4*/{0x04, 0x3f,  RSV_PID},
        /* 5*/{0x40, 0x40,  NIT_PID}, // actual network
        /* 6*/{0x41, 0x41,  NIT_PID}, // other network
        /* 7*/{0x42, 0x42,  SDT_PID}, // actual transport stream
        /* 8*/{0x43, 0x45,  RSV_PID},
        /* 9*/{0x46, 0x46,  SDT_PID}, // other transport stream
        /*10*/{0x47, 0x49,  RSV_PID},
        /*11*/{0x4a, 0x4a,  BAT_PID},
        /*12*/{0x4b, 0x4d,  RSV_PID},
        /*13*/{0x4e, 0x4e,  EIT_PID}, // actual transport stream,P/F"},
        /*14*/{0x4f, 0x4f,  EIT_PID}, // other transport stream,P/F"},
        /*15*/{0x50, 0x5f,  EIT_PID}, // actual transport stream,schedule"},
        /*16*/{0x60, 0x6f,  EIT_PID}, // other transport stream,schedule"},
        /*17*/{0x70, 0x70,  TDT_PID},
        /*18*/{0x71, 0x71,  RST_PID},
        /*19*/{0x72, 0x72,   ST_PID},
        /*20*/{0x73, 0x73,  TOT_PID},
        /*21*/{0x74, 0x7d,  RSV_PID},
        /*22*/{0x7e, 0x7e,  DIT_PID},
        /*23*/{0x7f, 0x7f,  SIT_PID},
        /*24*/{0x80, 0xfe,  USR_PID},
        /*25*/{0xff, 0xff,  RSV_PID}
};

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
static int parse_PES_head_switch(obj_t *obj);
static int parse_PES_head_detail(obj_t *obj);

static int parse_PAT_load(obj_t *obj);
static int parse_PMT_load(obj_t *obj);

static int search_in_TS_PID_TABLE(uint16_t pid, int *type);
static ts_pid_t *add_to_pid_list(struct LIST *list, ts_pid_t *pids);
static ts_pid_t *add_new_pid(obj_t *obj);
static int is_pmt_pid(obj_t *obj);
static int is_unparsed_prog(obj_t *obj);
static int is_all_prog_parsed(obj_t *obj);
static int track_type(ts_track_t *track);
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

        obj->is_first_pkg = 1;
        obj->pkg_size = pkg_size;
        obj->state = STATE_NEXT_PAT;
        obj->is_pes_align = 1; // PES align(0: need; 1: dot't need)

        (*rslt)->time = 0;
        (*rslt)->addr = 0;
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

        // calculate address
        if(obj->is_first_pkg)
        {
                obj->is_first_pkg = 0;
                obj->rslt.addr = 0;
        }
        else
        {
                obj->rslt.addr += obj->pkg_size;
        }

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
        ts_pid_t *pids = rslt->pids;

        // CC
        if(pids->is_CC_sync)
        {
                int lost;

                if(BIT(0) & ts->adaption_field_control)
                {
                        // increase CC only for packet with load
                        pids->CC += pids->dCC;
                }
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
                rslt->PCR_ext  = af->program_clock_reference_extension;

                rslt->PCR  = rslt->PCR_base;
                rslt->PCR *= 300;
                rslt->PCR += rslt->PCR_ext;
        }

        // PES head & ES data
        if(pids->track)
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

        // init rslt
        rslt->pids = NULL;
        rslt->has_PCR = 0;
        rslt->has_PTS = 0;
        rslt->has_DTS = 0;
        rslt->PES_len = 0; // clear PES length
        rslt->ES_len = 0; // clear ES length

        // begin
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

        if(BIT(0) & ts->adaption_field_control)
        {
                // data_byte, PSI or PES
        }

        rslt->pid = ts->PID; // record into rslt struct
        rslt->pids = pids_match(obj->rslt.pid_list, rslt->pid);
        if(NULL == rslt->pids)
        {
                rslt->pids = add_new_pid(obj);
        }
        rslt->pids->count++;

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

        // record PES data
        if(obj->is_pes_align)
        {
                rslt->PES_len = obj->len;
                rslt->PES_buf = obj->p;
        }

        if(ts->payload_unit_start_indicator)
        {
                obj->is_pes_align = 1;
                rslt->PES_len = obj->len;
                rslt->PES_buf = obj->p;

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

                parse_PES_head_switch(obj);
        }

        if(obj->is_pes_align)
        {
                rslt->ES_len = obj->len;
                rslt->ES_buf = obj->p;
        }

        return 0;
}

static int parse_PES_head_switch(obj_t *obj)
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
                parse_PES_head_detail(obj);
        }

        return 0;
}

static int parse_PES_head_detail(obj_t *obj)
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
        uint8_t dat;
        psi_t *psi = &(obj->psi);
        ts_rslt_t *rslt = &(obj->rslt);
        ts_prog_t *prog;
        ts_pid_t ts_pid, *pids = &ts_pid;

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
                search_in_TS_PID_TABLE(pids->PID, &(pids->type));
                pids->count = 0;
                pids->track = NULL;
                pids->CC = 0;
                pids->is_CC_sync = 0;
                pids->dCC = PID_TYPE_TABLE[pids->type].dCC;
                pids->sdes = PID_TYPE_TABLE[pids->type].sdes;
                pids->ldes = PID_TYPE_TABLE[pids->type].ldes;

                if(0 == prog->program_number)
                {
                        // network PID, not a program
                        free(prog);
                }
                else
                {
                        pids->type = PMT_PID;
                        pids->dCC = PID_TYPE_TABLE[pids->type].dCC;
                        pids->sdes = PID_TYPE_TABLE[pids->type].sdes;
                        pids->ldes = PID_TYPE_TABLE[pids->type].ldes;
                        list_add(rslt->prog_list, (struct NODE *)prog);
                }

                add_to_pid_list(rslt->pid_list, pids);
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
                fprintf(stderr, "PSI load length error!\n");
        }

        return 0;
}

static int parse_PMT_load(obj_t *obj)
{
        uint16_t i;
        uint8_t dat;
        uint16_t info_length;
        struct NODE *node;
        ts_prog_t *prog;
        ts_pid_t ts_pid, *pids = &ts_pid;
        ts_track_t *track;
        ts_t *ts = &(obj->ts);
        psi_t *psi = &(obj->psi);

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
                fprintf(stderr, "Wrong PID: 0x%04X\n", ts->PID);
                exit(EXIT_FAILURE);
        }

        prog->is_parsed = 1;

        dat = *(obj->p)++; obj->len--;
        prog->PCR_PID = dat & 0x1F;

        dat = *(obj->p)++; obj->len--;
        prog->PCR_PID <<= 8;
        prog->PCR_PID |= dat;

        // add PCR PID
        pids->PID = prog->PCR_PID;
        pids->count = 0;
        pids->track = NULL;
        pids->type = PCR_PID;
        pids->CC = 0;
        pids->is_CC_sync = 1;
        pids->dCC = PID_TYPE_TABLE[pids->type].dCC;
        pids->sdes = PID_TYPE_TABLE[pids->type].sdes;
        pids->ldes = PID_TYPE_TABLE[pids->type].ldes;
        add_to_pid_list(obj->rslt.pid_list, pids);

        // program_info_length
        dat = *(obj->p)++; obj->len--;
        info_length = dat & 0x0F;

        dat = *(obj->p)++; obj->len--;
        info_length <<= 8;
        info_length |= dat;

        // record program_info
        if(info_length > INFO_LEN_MAX)
        {
                fprintf(stderr, "PID(0x%04X): program_info_length(%d) too big!\n",
                        ts->PID, info_length);
        }
        prog->program_info_len = info_length;
        for(i = 0; i < info_length; i++)
        {
                dat = *(obj->p)++; obj->len--;
                prog->program_info[i] = dat;
        }

        while(obj->len > 4)
        {
                // add track
                track = (ts_track_t *)malloc(sizeof(ts_track_t));
                if(NULL == track)
                {
                        fprintf(stderr, "Malloc memory failure!\n");
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
                if(info_length > INFO_LEN_MAX)
                {
                        fprintf(stderr, "PID(0x%04X): ES_info_length(%d) too big!\n",
                                track->PID, info_length);
                }
                track->es_info_len = info_length;
                for(i = 0; i < info_length; i++)
                {
                        dat = *(obj->p)++; obj->len--;
                        track->es_info[i] = dat;
                }

                track_type(track);
                list_add(prog->track, (struct NODE *)track);

                // add track PID
                pids->PID = track->PID;
                pids->count = 0;
                pids->track = track;
                pids->type = track->type;
                pids->CC = 0;
                pids->is_CC_sync = 0;
                pids->dCC = PID_TYPE_TABLE[pids->type].dCC;
                pids->sdes = PID_TYPE_TABLE[pids->type].sdes;
                pids->ldes = PID_TYPE_TABLE[pids->type].ldes;
                add_to_pid_list(obj->rslt.pid_list, pids);
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
                fprintf(stderr, "PSI load length error!\n");
        }

        return 0;
}

static int search_in_TS_PID_TABLE(uint16_t pid, int *type)
{
        int i;
        int count = sizeof(TS_PID_TABLE) / sizeof(ts_pid_table_t);

        for(i = 0; i < count; i++)
        {
                if(TS_PID_TABLE[i].min <= pid && pid <= TS_PID_TABLE[i].max)
                {
                        *type = TS_PID_TABLE[i].type;
                        return 0;
                }
        }

        *type = UNO_PID;
        return -1; // illegal PID value
}

static ts_pid_t *add_new_pid(obj_t *obj)
{
        ts_pid_t ts_pid, *pids;
        ts_t *ts = &(obj->ts);
        ts_rslt_t *rslt = &(obj->rslt);

        pids = &ts_pid;

        pids->PID = rslt->pid;
        search_in_TS_PID_TABLE(pids->PID, &(pids->type));
        pids->track = NULL;
        pids->count = 1;
        pids->dCC = PID_TYPE_TABLE[pids->type].dCC;
        if(STATE_NEXT_PKG == obj->state)
        {
                pids->CC = ts->continuity_counter - pids->dCC;
        }
        else
        {
                pids->CC = ts->continuity_counter;
        }
        pids->is_CC_sync = 1;
        pids->sdes = PID_TYPE_TABLE[pids->type].sdes;
        pids->ldes = PID_TYPE_TABLE[pids->type].ldes;

        return add_to_pid_list(rslt->pid_list, pids);
}

static ts_pid_t *add_to_pid_list(struct LIST *list, ts_pid_t *the_pids)
{
        struct NODE *node;
        ts_pid_t *pids;

        for(node = list->head; node; node = node->next)
        {
                pids = (ts_pid_t *)node;
                if(pids->PID == the_pids->PID)
                {
                        if(PCR_PID == pids->type)
                        {
                                pids->track = the_pids->track;
                                if(VID_PID == the_pids->type)
                                {
                                        pids->type = VID_PCR;
                                }
                                else if(AUD_PID == the_pids->type)
                                {
                                        pids->type = AUD_PCR;
                                }
                                else
                                {
                                        pids->type = the_pids->type;
                                        fprintf(stderr, "bad element PID(0x%04X)!\n", the_pids->PID);
                                }
                                pids->count = the_pids->count;
                                pids->CC = the_pids->CC;
                                pids->is_CC_sync = the_pids->is_CC_sync;
                                pids->dCC = PID_TYPE_TABLE[pids->type].dCC;
                                pids->sdes = PID_TYPE_TABLE[pids->type].sdes;
                                pids->ldes = PID_TYPE_TABLE[pids->type].ldes;
                        }
                        else
                        {
                                // update item information
                                pids->track = the_pids->track;
                                pids->type = the_pids->type;
                                pids->count = the_pids->count;
                                pids->CC = the_pids->CC;
                                pids->is_CC_sync = the_pids->is_CC_sync;
                                pids->dCC = the_pids->dCC;
                                pids->sdes = the_pids->sdes;
                                pids->ldes = the_pids->ldes;
                        }
                        return pids;
                }

                if(pids->PID > the_pids->PID)
                {
                        pids = (ts_pid_t *)malloc(sizeof(ts_pid_t));
                        if(NULL == pids)
                        {
                                DBG(ERR_MALLOC_FAILED);
                                return NULL;
                        }

                        pids->PID = the_pids->PID;
                        pids->track = the_pids->track;
                        pids->type = the_pids->type;
                        pids->count = the_pids->count;
                        pids->CC = the_pids->CC;
                        pids->is_CC_sync = the_pids->is_CC_sync;
                        pids->dCC = the_pids->dCC;
                        pids->sdes = the_pids->sdes;
                        pids->ldes = the_pids->ldes;

                        list_insert_before(list, node, (struct NODE *)pids);
                        return pids;
                }

                if(NULL == node->next)
                {
                        pids = (ts_pid_t *)malloc(sizeof(ts_pid_t));
                        if(NULL == pids)
                        {
                                DBG(ERR_MALLOC_FAILED);
                                return NULL;
                        }

                        pids->PID = the_pids->PID;
                        pids->track = the_pids->track;
                        pids->type = the_pids->type;
                        pids->count = the_pids->count;
                        pids->CC = the_pids->CC;
                        pids->is_CC_sync = the_pids->is_CC_sync;
                        pids->dCC = the_pids->dCC;
                        pids->sdes = the_pids->sdes;
                        pids->ldes = the_pids->ldes;

                        list_add(list, (struct NODE *)pids);
                        return pids;
                }

                if(((ts_pid_t *)(node->next))->PID > the_pids->PID)
                {
                        pids = (ts_pid_t *)malloc(sizeof(ts_pid_t));
                        if(NULL == pids)
                        {
                                DBG(ERR_MALLOC_FAILED);
                                return NULL;
                        }

                        pids->PID = the_pids->PID;
                        pids->track = the_pids->track;
                        pids->type = the_pids->type;
                        pids->count = the_pids->count;
                        pids->CC = the_pids->CC;
                        pids->is_CC_sync = the_pids->is_CC_sync;
                        pids->dCC = the_pids->dCC;
                        pids->sdes = the_pids->sdes;
                        pids->ldes = the_pids->ldes;

                        list_insert_after(list, node, (struct NODE *)pids);
                        return pids;
                }
        }

        pids = (ts_pid_t *)malloc(sizeof(ts_pid_t));
        if(NULL == pids)
        {
                DBG(ERR_MALLOC_FAILED);
                return NULL;
        }

        pids->PID = the_pids->PID;
        pids->track = the_pids->track;
        pids->type = the_pids->type;
        pids->count = the_pids->count;
        pids->CC = the_pids->CC;
        pids->is_CC_sync = the_pids->is_CC_sync;
        pids->dCC = the_pids->dCC;
        pids->sdes = the_pids->sdes;
        pids->ldes = the_pids->ldes;

        list_add(list, (struct NODE *)pids);
        return pids;
}

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

static int is_all_prog_parsed(obj_t *obj)
{
        struct NODE *node;
        ts_prog_t *prog;

        for(node = obj->rslt.prog_list->head; node; node = node->next)
        {
                prog = (ts_prog_t *)node;
                if(0 == prog->is_parsed)
                {
                        obj->rslt.concerned_pid = prog->PMT_PID;
                        return 0;
                }
        }
        return 1;
}

static int track_type(ts_track_t *track)
{
        switch(track->stream_type)
        {
                case 0x00:
                        track->type = USR_PID;
                        track->sdes = "Reserved";
                        track->ldes = "ITU-T|ISO/IEC Reserved";
                        break;
                case 0x01:
                        track->type = VID_PID;
                        track->sdes = "MPEG-1";
                        track->ldes = "ISO/IEC 11172-2 Video";
                        break;
                case 0x02:
                        track->type = VID_PID;
                        track->sdes = "MPEG-2";
                        track->ldes = "ITU-T Rec.H.262|ISO/IEC 13818-2 Video or MPEG-1 parameter limited";
                        break;
                case 0x03:
                        track->type = AUD_PID;
                        track->sdes = "MPEG-1";
                        track->ldes = "ISO/IEC 11172-3 Audio";
                        break;
                case 0x04:
                        track->type = AUD_PID;
                        track->sdes = "MPEG-2";
                        track->ldes = "ISO/IEC 13818-3 Audio";
                        break;
                case 0x05:
                        track->type = AUD_PID;
                        track->sdes = "MPEG-2";
                        track->ldes = "ITU-T Rec.H.222.0|ISO/IEC 13818-1 private_sections";
                        break;
                case 0x06:
                        track->type = AUD_PID;
                        track->sdes = "AC3";
                        track->ldes = "Dolby Digital DVB";
                        break;
                case 0x07:
                        track->type = AUD_PID;
                        track->sdes = "MHEG";
                        track->ldes = "ISO/IEC 13522 MHEG";
                        break;
                case 0x08:
                        track->type = AUD_PID;
                        track->sdes = "DSM-CC";
                        track->ldes = "ITU-T Rec.H.222.0|ISO/IEC 13818-1 Annex A DSM-CC";
                        break;
                case 0x09:
                        track->type = AUD_PID;
                        track->sdes = "H.222.1";
                        track->ldes = "ITU-T Rec.H.222.1";
                        break;
                case 0x0A:
                        track->type = AUD_PID;
                        track->sdes = "MPEG2 type A";
                        track->ldes = "ISO/IEC 13818-6 type A";
                        break;
                case 0x0B:
                        track->type = AUD_PID;
                        track->sdes = "MPEG2 type B";
                        track->ldes = "ISO/IEC 13818-6 type B";
                        break;
                case 0x0C:
                        track->type = AUD_PID;
                        track->sdes = "MPEG2 type C";
                        track->ldes = "ISO/IEC 13818-6 type C";
                        break;
                case 0x0D:
                        track->type = AUD_PID;
                        track->sdes = "MPEG2 type D";
                        track->ldes = "ISO/IEC 13818-6 type D";
                        break;
                case 0x0E:
                        track->type = AUD_PID;
                        track->sdes = "auxiliary";
                        track->ldes = "ITU-T Rec.H.222.0|ISO/IEC 13818-1 auxiliary";
                        break;
                case 0x0F:
                        track->type = AUD_PID;
                        track->sdes = "AAC ADTS";
                        track->ldes = "ISO/IEC 13818-7 Audio with ADTS transport syntax";
                        break;
                case 0x10:
                        track->type = VID_PID;
                        track->sdes = "MPEG-4";
                        track->ldes = "ISO/IEC 14496-2 Visual";
                        break;
                case 0x11:
                        track->type = AUD_PID;
                        track->sdes = "AAC LATM";
                        track->ldes = "ISO/IEC 14496-3 Audio with LATM transport syntax";
                        break;
                case 0x12:
                        track->type = AUD_PID;
                        track->sdes = "MPEG-4";
                        track->ldes = "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets";
                        break;
                case 0x13:
                        track->type = AUD_PID;
                        track->sdes = "MPEG-4";
                        track->ldes = "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC 14496_sections";
                        break;
                case 0x14:
                        track->type = AUD_PID;
                        track->sdes = "MPEG-2";
                        track->ldes = "ISO/IEC 13818-6 Synchronized Download Protocol";
                        break;
                case 0x15:
                        track->type = AUD_PID;
                        track->sdes = "MPEG-2";
                        track->ldes = "Metadata carried in PES packets";
                        break;
                case 0x16:
                        track->type = AUD_PID;
                        track->sdes = "MPEG-2";
                        track->ldes = "Metadata carried in metadata_sections";
                        break;
                case 0x17:
                        track->type = AUD_PID;
                        track->sdes = "MPEG-2";
                        track->ldes = "Metadata carried in ISO/IEC 13818-6 Data Carousel";
                        break;
                case 0x18:
                        track->type = AUD_PID;
                        track->sdes = "MPEG-2";
                        track->ldes = "Metadata carried in ISO/IEC 13818-6 Object Carousel";
                        break;
                case 0x19:
                        track->type = AUD_PID;
                        track->sdes = "MPEG-2";
                        track->ldes = "Metadata carried in ISO/IEC 13818-6 Synchronized Dowload Protocol";
                        break;
                case 0x1A:
                        track->type = AUD_PID;
                        track->sdes = "IPMP";
                        track->ldes = "IPMP stream(ISO/IEC 13818-11, MPEG-2 IPMP)";
                        break;
                case 0x1B:
                        track->type = VID_PID;
                        track->sdes = "H.264";
                        track->ldes = "ITU-T Rec.H.264|ISO/IEC 14496-10 Video";
                        break;
                case 0x42:
                        track->type = VID_PID;
                        track->sdes = "AVS";
                        track->ldes = "Advanced Video Standard";
                        break;
                case 0x7F:
                        track->type = AUD_PID;
                        track->sdes = "IPMP";
                        track->ldes = "IPMP stream";
                        break;
                case 0x81:
                        track->type = AUD_PID;
                        track->sdes = "AC3";
                        track->ldes = "Dolby Digital ATSC";
                        break;
                default:
                        track->type = UNO_PID;
                        track->sdes = "";
                        track->ldes = "";
                        break;
        }
        return 0;
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
