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
        uint32_t reserved:6;
        uint32_t program_clock_reference_extension:9;
}
af_t;

typedef struct
{
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
        uint8_t line[204]; // one TS package
        uint8_t *p; // point to current data in line
        int left_length;

        ts_t ts;
        af_t af;
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

#if 0
static const char *pid_type_str[] =
{
        " PAT_PID",
        " CAT_PID",
        " PMT_PID",
        "TSDT_PID",
        " NIT_PID",
        " SDT_PID",
        " BAT_PID",
        " EIT_PID",
        " TDT_PID",
        " RST_PID",
        " ST_PID",
        " TOT_PID",
        " DIT_PID",
        " SIT_PID",
        " PCR_PID",
        " VID_PID",
        " VID_PCR",
        " AUD_PID",
        " AUD_PCR",
        "NULL_PID",
        " UNO_PID"
};
#endif

//=============================================================================
// enum definition
//=============================================================================
enum pid_type
{
        PAT_PID,
        CAT_PID,
        PMT_PID,
        TSDT_PID,
        NIT_PID,
        SDT_PID,
        BAT_PID,
        EIT_PID,
        TDT_PID,
        RST_PID,
        ST_PID,
        TOT_PID,
        DIT_PID,
        SIT_PID,
        PCR_PID,
        VID_PID,
        VID_PCR,
        AUD_PID,
        AUD_PCR,
        NULL_PID,
        UNO_PID
};

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

static int parse_TS(obj_t *obj);  // ts layer information
static int parse_AF(obj_t *obj);  // adaption_fields information
static int parse_PSI(obj_t *obj); // psi information

static int parse_PAT_load(obj_t *obj);
static int parse_PMT_load(obj_t *obj);

static int search_in_TS_PID_TABLE(uint16_t pid);
static int pids_add(struct LIST *list, ts_pid_t *pids);
static int is_pmt_pid(obj_t *obj);
static int is_unparsed_prog(obj_t *obj);
static int is_all_prog_parsed(obj_t *obj);
static int PID_type(uint8_t stream_type);
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

int tsParseTS(int id, const char *pkg)
{
        obj_t *obj;

        obj = (obj_t *)id;
        if(NULL == obj)
        {
                DBG(ERR_BAD_ID);
                return -ERR_BAD_ID;
        }

        memcpy(obj->line, pkg, obj->pkg_size);
        obj->p = obj->line;

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

        // CC
        pids = pids_match(obj->rslt.pid_list, ts->PID);
        if(NULL == pids)
        {
                if(MIN_USER_PID <= ts->PID && ts->PID <= MAX_USER_PID)
                {
                        fprintf(stderr, "unknown PID: 0x%04X\n", ts->PID);
                }
                else
                {
                        // reserved PID
                }
        }
        else if(pids->is_CC_sync)
        {
                pids->CC += pids->dCC;
                if(pids->CC != ts->continuity_counter)
                {
                        int lost;

                        lost = (int)ts->continuity_counter;
                        lost -= (int)pids->CC;
                        if(lost < 0)
                        {
                                lost += 16;
                        }

                        rslt->CC.wait = pids->CC;
                        rslt->CC.find = ts->continuity_counter;
                        rslt->CC.lost = lost;

                        pids->CC = ts->continuity_counter;
                }
        }
        else
        {
                pids->CC = ts->continuity_counter;
                pids->is_CC_sync = 1;
        }

        // PCR
        if(rslt->PCR.is_pcr_pid)
        {
                rslt->PCR.pcr_base = af->program_clock_reference_base;
                rslt->PCR.pcr_ext += af->program_clock_reference_extension;

                rslt->PCR.pcr  = rslt->PCR.pcr_base;
                rslt->PCR.pcr *= 300;
                rslt->PCR.pcr += rslt->PCR.pcr_ext;
        }

        return 0;
}

static int parse_TS(obj_t *obj)
{
        uint8_t dat;
        ts_t *ts = &(obj->ts);
        ts_rslt_t *rslt = &(obj->rslt);

        rslt->PCR.is_pcr_pid = 0;

        dat = *(obj->p)++;
        ts->sync_byte = dat;
        if(0x47 != ts->sync_byte)
        {
                uint32_t i;
                fprintf(stderr, "sync_byte(0x47) wrong: \n%02X", dat);
                for(i = 1; i < obj->pkg_size; i++)
                {
                        fprintf(stderr, " %02X", *(obj->p)++);
                }
                fprintf(stderr, "\n");
                return -1;
        }

        dat = *(obj->p)++;
        ts->transport_error_indicator = (dat & BIT(7)) >> 7;
        ts->payload_unit_start_indicator = (dat & BIT(6)) >> 6;
        ts->transport_priority = (dat & BIT(5)) >> 5;
        ts->PID = dat & 0x1F;

        dat = *(obj->p)++;
        ts->PID <<= 8;
        ts->PID |= dat;
        rslt->pid = ts->PID; // record into rslt struct

        dat = *(obj->p)++;
        ts->transport_scrambling_control = (dat & (BIT(7) | BIT(6))) >> 6;
        ts->adaption_field_control = (dat & (BIT(5) | BIT(4))) >> 4;;
        ts->continuity_counter = dat & 0x0F;

        if(BIT(1) & ts->adaption_field_control)
        {
                parse_AF(obj);
        }

        if(BIT(0) & ts->adaption_field_control)
        {
                // data_byte
                (obj->p)++;
        }

        return 0;
}

static int parse_AF(obj_t *obj)
{
        uint8_t dat;
        af_t *af = &(obj->af);
        ts_rslt_t *rslt = &(obj->rslt);

        dat = *(obj->p)++;
        af->adaption_field_length = dat;
        if(0x00 == af->adaption_field_length)
        {
                return 0;
        }

        dat = *(obj->p)++;
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
                dat = *(obj->p)++;
                af->program_clock_reference_base = dat;

                dat = *(obj->p)++;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++;
                af->program_clock_reference_base <<= 8;
                af->program_clock_reference_base |= dat;

                dat = *(obj->p)++;
                af->program_clock_reference_base <<= 1;
                af->program_clock_reference_base |= ((dat & BIT(7)) >> 7);
                af->program_clock_reference_extension = ((dat & BIT(0)) >> 0);

                dat = *(obj->p)++;
                af->program_clock_reference_extension <<= 8;
                af->program_clock_reference_extension |= dat;

                rslt->PCR.is_pcr_pid = 1;
        }

        return 0;
}

static int parse_PSI(obj_t *obj)
{
        uint8_t dat;
        psi_t *psi = &(obj->psi);

        dat = *(obj->p)++;
        psi->table_id = dat;

        dat = *(obj->p)++;
        psi->sectin_syntax_indicator = (dat & BIT(7)) >> 7;
        psi->section_length = dat & 0x0F;

        dat = *(obj->p)++;
        psi->section_length <<= 8;
        psi->section_length |= dat;
        obj->left_length = psi->section_length;

        dat = *(obj->p)++; obj->left_length--;
        psi->idx.idx = dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->idx.idx <<= 8;
        psi->idx.idx |= dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->version_number = (dat & 0x3E) >> 1;
        psi->current_next_indicator = dat & BIT(0);

        dat = *(obj->p)++; obj->left_length--;
        psi->section_number = dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->last_section_number = dat;

        return 0;
}

static int parse_PAT_load(obj_t *obj)
{
        int i;
        uint8_t dat;
        psi_t *psi = &(obj->psi);
        ts_rslt_t *rslt = &(obj->rslt);
        ts_prog_t *prog;
        ts_pid_t *pids;

        while(obj->left_length > 4)
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

                dat = *(obj->p)++; obj->left_length--;
                prog->program_number = dat;

                dat = *(obj->p)++; obj->left_length--;
                prog->program_number <<= 8;
                prog->program_number |= dat;

                dat = *(obj->p)++; obj->left_length--;
                prog->PMT_PID = dat & 0x1F;

                dat = *(obj->p)++; obj->left_length--;
                prog->PMT_PID <<= 8;
                prog->PMT_PID |= dat;

                // add PID
                pids = (ts_pid_t *)malloc(sizeof(ts_pid_t));
                if(NULL == pids)
                {
                        printf("Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }
                pids->PID = prog->PMT_PID;
                i = search_in_TS_PID_TABLE(pids->PID);
                pids->CC = 0;
                pids->dCC = TS_PID_TABLE[i].dCC;
                pids->is_CC_sync = 0;
                pids->sdes = TS_PID_TABLE[i].sdes;
                pids->ldes = TS_PID_TABLE[i].ldes;
                pids_add(rslt->pid_list, pids);

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
        }

        dat = *(obj->p)++; obj->left_length--;
        psi->CRC3 = dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->CRC2 = dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->CRC1 = dat;

        dat = *(obj->p)++; obj->left_length--;
        psi->CRC0 = dat;

        if(0 != obj->left_length)
        {
                printf("PSI load length error!\n");
        }

        //printf("PROG Count: %u\n", list_count(obj->prog));
        //printf("PIDS Count: %u\n", list_count(obj->pids));

        return 0;
}

static int parse_PMT_load(obj_t *obj)
{
        uint8_t dat;
        uint16_t info_length;
        psi_t *psi;
        struct NODE *node;
        ts_prog_t *prog;
        ts_pid_t *pids;
        ts_track_t *track;
        ts_t *ts;

        ts = &(obj->ts);
        psi = &(obj->psi);

        node = obj->rslt.prog_list->head;
        while(node)
        {
                prog = (ts_prog_t *)node;
                if(psi->idx.program_number == prog->program_number)
                {
                        break;
                }
                node = node->next;
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

        dat = *(obj->p)++; obj->left_length--;
        prog->PCR_PID = dat & 0x1F;

        dat = *(obj->p)++; obj->left_length--;
        prog->PCR_PID <<= 8;
        prog->PCR_PID |= dat;

        // add PCR PID
        pids = (ts_pid_t *)malloc(sizeof(ts_pid_t));
        if(NULL == pids)
        {
                printf("Malloc memory failure!\n");
                exit(EXIT_FAILURE);
        }
        pids->PID = prog->PCR_PID;
        pids->type = PCR_PID;
        pids->sdes = " PCR";
        pids->ldes = "program clock reference";
        pids->CC = 0;
        pids->dCC = 0;
        pids->is_CC_sync = 1;
        pids_add(obj->rslt.pid_list, pids);

        // program_info_length
        dat = *(obj->p)++; obj->left_length--;
        info_length = dat & 0x0F;

        dat = *(obj->p)++; obj->left_length--;
        info_length <<= 8;
        info_length |= dat;

        while(info_length-- > 0)
        {
                // omit descriptor here
                dat = *(obj->p)++; obj->left_length--;
        }

        while(obj->left_length > 4)
        {
                // add track
                track = (ts_track_t *)malloc(sizeof(ts_track_t));
                if(NULL == track)
                {
                        printf("Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }

                dat = *(obj->p)++; obj->left_length--;
                track->stream_type = dat;

                dat = *(obj->p)++; obj->left_length--;
                track->PID = dat & 0x1F;

                dat = *(obj->p)++; obj->left_length--;
                track->PID <<= 8;
                track->PID |= dat;

                // ES_info_length
                dat = *(obj->p)++; obj->left_length--;
                info_length = dat & 0x0F;

                dat = *(obj->p)++; obj->left_length--;
                info_length <<= 8;
                info_length |= dat;

                while(info_length-- > 0)
                {
                        // omit descriptor here
                        dat = *(obj->p)++; obj->left_length--;
                }

                track->type = PID_type(track->stream_type);
                list_add(prog->track, (struct NODE *)track);

                // add track PID
                pids = (ts_pid_t *)malloc(sizeof(ts_pid_t));
                if(NULL == pids)
                {
                        printf("Malloc memory failure!\n");
                        exit(EXIT_FAILURE);
                }
                pids->PID = track->PID;
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

        node = list->head;
        while(node)
        {
                pids = (ts_pid_t *)node;
                if(the_pids->PID == pids->PID)
                {
                        if(PCR_PID == pids->type)
                        {
                                if(VID_PID == the_pids->type)
                                {
                                        pids->type = VID_PCR;
                                        pids->sdes = "VPCR";
                                        pids->ldes = "video package with pcr information";
                                }
                                else if(AUD_PID == the_pids->type)
                                {
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
                node = node->next;
        }

        list_add(list, (struct NODE *)the_pids);

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

static int PID_type(uint8_t stream_type)
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
                case 0x81: // "AC3"
                        return AUD_PID;
                default:
                        return UNO_PID; // unknown
        }
}

static ts_pid_t *pids_match(struct LIST *list, uint16_t pid)
{
        struct NODE *node;
        ts_pid_t *pids;

        node = list->head;
        while(node)
        {
                pids = (ts_pid_t *)node;
                if(pid == pids->PID)
                {
                        return pids;
                }
                node = node->next;
        }

        return NULL;
}

#if 0
static void show_TS(obj_t *obj)
{
        ts_t *ts = obj->ts;

        printf("TS:\n");
        printf("  0x%02X  : sync_byte\n", ts->sync_byte);
        printf("  %c     : transport_error_indicator\n", (ts->transport_error_indicator) ? '1' : '0');
        printf("  %c     : payload_unit_start_indicator\n", (ts->payload_unit_start_indicator) ? '1' : '0');
        printf("  %c     : transport_priority\n", (ts->transport_priority) ? '1' : '0');
        printf("  0x%04X: PID\n", ts->PID);
        printf("  0b%s  : transport_scrambling_control\n", printb(ts->transport_scrambling_control, 2));
        printf("  0b%s  : adaption_field_control\n", printb(ts->adaption_field_control, 2));
        printf("  0x%X   : continuity_counter\n", ts->continuity_counter);
}

static void show_PSI(obj_t *obj)
{
        uint32_t idx;
        psi_t *psi = obj->psi;

        printf("0x0000: PAT_PID\n");
        //printf("    0x%04X: section_length\n", psi->section_length);
        printf("    0x%04X: transport_stream_id\n", psi->idx.idx);

        idx = 0;
#if 0
        while(idx < psi->program_cnt)
        {
                if(0x0000 == psi->program[idx].program_number)
                {
                        printf("    0x%04X: network_program_number\n",
                               psi->program[idx].program_number);
                        printf("        0x%04X: network_PID\n",
                               psi->program[idx].PID);
                }
                else
                {
                        printf("    0x%04X: program_number\n",
                               psi->program[idx].program_number);
                        printf("        0x%04X: PMT_PID\n",
                               psi->program[idx].PID);
                }
                idx++;
        }
#endif
}

static void show_PMT(obj_t *obj)
{
        uint32_t i;
        uint32_t idx;
        psi_t *psi = obj->psi;
        struct PMT *pmt;

        for(idx = 0; idx < psi->program_cnt; idx++)
        {
                pmt = obj->pmt[idx];
                if(0x0000 == pmt->program_number)
                {
                        // network information
                        printf("0x%04X: network_PID\n",
                               pmt->PID);
                        //printf("    omit...\n");
                }
                else
                {
                        // normal program
                        printf("0x%04X: PMT_PID\n",
                               pmt->PID);
                        //printf("    0x%04X: section_length\n",
                        //       pmt->section_length);
                        printf("    0x%04X: program_number\n",
                               pmt->program_number);
                        printf("    0x%04X: PCR_PID\n",
                               pmt->PCR_PID);
                        //printf("    0x%04X: program_info_length\n",
                        //       pmt->program_info_length);
                        if(0 != pmt->program_info_length)
                        {
                                //printf("        omit...\n");
                        }

                        i = 0;
                        while(i < pmt->pid_cnt)
                        {
                                printf("    0x%04X: %s\n",
                                       pmt->pid[i].PID,
                                       PID_TYPE_STR[stream_type(pmt->pid[i].stream_type)]);
                                printf("        0x%04X: %s\n",
                                       pmt->pid[i].stream_type,
                                       stream_type_str(pmt->pid[i].stream_type));
                                //printf("        0x%04X: ES_info_length\n",
                                //       pmt->pid[i].ES_info_length);
                                if(0 != pmt->pid[i].ES_info_length)
                                {
                                        //printf("            omit...\n");
                                }

                                i++;
                        }
                }
        }
}

static char *printb(uint32_t x, int bit_cnt)
{
        static char str[64];

        char *p = str;
        uint32_t mask;

        if(bit_cnt < 1 || bit_cnt > 32)
        {
                *p++ = 'e';
                *p++ = 'r';
                *p++ = 'r';
                *p++ = 'o';
                *p++ = 'r';
        }
        else
        {
                mask = (1 << (bit_cnt - 1));
                while(mask)
                {
                        *p++ = (x & mask) ? '1' : '0';
                        mask >>= 1;
                }
        }
        *p = '\0';

        return str;
}
#endif
//=============================================================================
// THE END.
//=============================================================================
