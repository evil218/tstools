/* vim: set tabstop=8 shiftwidth=8:
 * name: atsc_mh_tcp
 * funx: parse each field of ATSC M/H TCP packet
 *
 * 2011-10-09, LI WenYan, Init for debug ATSC M/H MUX card
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h> /* for uint?_t, etc */

#include "common.h"

#define TRELLIS_CODE_LEN                (12) /* bytes */
#define TX_DATA_NUM                     (16) /* groups */
#define TCP_ECC_LEN                     (20) /* bytes */
#define BIT(n)                          (1<<n)

typedef struct _signaling_data_t
{
        uint8_t packet_frame_number; /* 5 */
        uint8_t mode; /* 1 */
}sig_data_t;

typedef struct _tx_data_t
{
        uint16_t tx_address; /* 12 */
        uint16_t tx_identifier_level;/* 3  */
        uint16_t tx_data_inhibit; /* 1 */
        uint16_t tx_time_offset; /* 16 */
        uint16_t tx_power; /* 12 */
}tx_data_t;

typedef struct _tcp_para_t
{
        sig_data_t sig_data;
        uint8_t trellis_code_state[TRELLIS_CODE_LEN];
        uint32_t syn_time_stamp;//uint8_t syn_time_stamp_base[3];
        uint32_t max_delay;//uint8_t max_delay_base[3];
        uint16_t network_identifier_pattern; /* 12 */
        //uint8_t syn_time_stamp_extension;  /* 1 */
        //uint8_t max_delay_extension;  /* 1 */
        uint16_t packet_number; /* 10 */
        uint32_t adjusted_gps_seconds_count;
        uint8_t tx_group_number;
        tx_data_t tx_data[TX_DATA_NUM];
        uint32_t AT_frame_number;
        uint32_t atsc_time_displacement;/* 25-7 */
        uint8_t reserved[32];
        uint8_t TCP_ECC[TCP_ECC_LEN];
}tcp_pata_t;

void atsc_mh_tcp(uint8_t *ts_pack, int is_color)
{
        int i;
        char *yellow_on = "";
        char *color_off = "";
        uint8_t *tcp_data = (uint8_t *)(ts_pack + 5);
        tcp_pata_t tcp_param; 
        int tcpLen = sizeof(tcp_pata_t);
        uint32_t syn_time_stamp_extension;
        uint32_t max_delay_extension;

        if(is_color)
        {
                yellow_on = FYELLOW;
                color_off = NONE;
        }

        //fprintf(stdout, "%s", obj->tbak);
        memset(&tcp_param, 0, tcpLen);
        tcp_param.sig_data.packet_frame_number = (*tcp_data)>>3;
        tcp_param.sig_data.mode = ( (*tcp_data)&BIT(2) )>>2;
        tcp_data++;

        for(i = 0; i < TRELLIS_CODE_LEN; i++)
        {
                tcp_param.trellis_code_state[i] = *tcp_data++;
        }

        tcp_param.syn_time_stamp = *tcp_data++;
        tcp_param.syn_time_stamp <<= 8;
        tcp_param.syn_time_stamp |= *tcp_data++;
        tcp_param.syn_time_stamp <<= 8;
        tcp_param.syn_time_stamp |= *tcp_data++;

        tcp_param.max_delay = *tcp_data++;
        tcp_param.max_delay <<= 8;
        tcp_param.max_delay |= *tcp_data++;
        tcp_param.max_delay <<= 8;
        tcp_param.max_delay |= *tcp_data++;

        tcp_param.network_identifier_pattern = *tcp_data++;
        tcp_param.network_identifier_pattern <<= 4;
        tcp_param.network_identifier_pattern |= (*tcp_data>>4);

        syn_time_stamp_extension = ((*tcp_data)&BIT(3))>>3;
        tcp_param.syn_time_stamp |= syn_time_stamp_extension<<24;
        max_delay_extension = ((*tcp_data)&BIT(2))>>2;
        tcp_param.max_delay |= max_delay_extension<<24;

        tcp_param.packet_number = (*tcp_data)&0x03;
        tcp_param.packet_number <<= 8;
        tcp_data++;
        tcp_param.packet_number |= *tcp_data++;

        tcp_param.adjusted_gps_seconds_count = *tcp_data++;
        tcp_param.adjusted_gps_seconds_count <<= 8;
        tcp_param.adjusted_gps_seconds_count |= *tcp_data++;
        tcp_param.adjusted_gps_seconds_count <<= 8;
        tcp_param.adjusted_gps_seconds_count |= *tcp_data++;
        tcp_param.adjusted_gps_seconds_count <<= 8;
        tcp_param.adjusted_gps_seconds_count |= *tcp_data++;

        tcp_param.tx_group_number = *tcp_data++;

        for(i = 0; i < TX_DATA_NUM; i++)
        {
                tcp_param.tx_data[i].tx_address = *tcp_data++; 
                tcp_param.tx_data[i].tx_address <<= 4;
                tcp_param.tx_data[i].tx_address |= (*tcp_data>>4);

                tcp_param.tx_data[i].tx_identifier_level = (*tcp_data>>1)&0x07;
                tcp_param.tx_data[i].tx_data_inhibit = (*tcp_data)&BIT(0);
                tcp_data++;

                tcp_param.tx_data[i].tx_time_offset = *tcp_data++;
                tcp_param.tx_data[i].tx_time_offset <<= 8;
                tcp_param.tx_data[i].tx_time_offset |= *tcp_data++;

                tcp_param.tx_data[i].tx_power = *tcp_data++; 
                tcp_param.tx_data[i].tx_power <<= 4;
                tcp_param.tx_data[i].tx_power |= (*tcp_data>>4);
                tcp_data++;
        }

        tcp_param.AT_frame_number = *tcp_data++;
        tcp_param.AT_frame_number <<= 8;
        tcp_param.AT_frame_number |= *tcp_data++;
        tcp_param.AT_frame_number <<= 8;
        tcp_param.AT_frame_number |= *tcp_data++;
        tcp_param.AT_frame_number <<= 8;
        tcp_param.AT_frame_number |= *tcp_data++;

        tcp_param.atsc_time_displacement = *tcp_data++;
        tcp_param.atsc_time_displacement <<= 8;
        tcp_param.atsc_time_displacement |= *tcp_data++;
        tcp_param.atsc_time_displacement <<= 8;
        tcp_param.atsc_time_displacement |= *tcp_data++;
        tcp_param.atsc_time_displacement <<= 8;
        tcp_param.atsc_time_displacement |= *tcp_data++;
        tcp_param.atsc_time_displacement >>= 7;

        tcp_data += 32; /* reserved part(263/8=) */

        for(i = 0; i < TCP_ECC_LEN; i++)
        {
                tcp_param.TCP_ECC[i] = *tcp_data++;
        }

        // FIXME because pack_frame_num is 20,turn to be 0,and mode is also always 0. not print.
        /*fprintf(stdout, "\n #sig_data=>pack_frame_num,%s%02X%s, mode,%s%02X%s ",
          yellow_on,
          tcp_param.sig_data.packet_frame_number, 
          color_off,
          yellow_on,
          tcp_param.sig_data.mode,
          color_off);
          */


        fprintf(stdout, "Syn_time_stamp, %s0x%08X%s, ", 
                yellow_on,
                tcp_param.syn_time_stamp,
                color_off);

        fprintf(stdout, "Max_delay, %s0x%08X%s, ", 
                yellow_on, 
                tcp_param.max_delay, 
                color_off);

        // FIXME fixed 0yet not print
        /* fprintf(stdout, " #network_pattern, %s0x%04X%s\n", 
           yellow_on, 
           tcp_param.network_identifier_pattern, 
           color_off);
           fprintf(stdout, "pack_num, %s0x%04X%s, ", 
           yellow_on, 
           tcp_param.packet_number,
           color_off);
           */      

        fprintf(stdout, "Adj_gpsSec_cnt, %s0x%08X%s, ",
                yellow_on,
                tcp_param.adjusted_gps_seconds_count,
                color_off);
        //fprintf(stdout, " #tx_group_num,0x%02X ", tcp_param.tx_group_number);


        fprintf(stdout,"AT_frame_number, %s0x%08X%s, ", 
                yellow_on,
                tcp_param.AT_frame_number,
                color_off);
        fprintf(stdout,"Atsc_time, %s0x%08X%s, ", 
                yellow_on,
                tcp_param.atsc_time_displacement,
                color_off);

#if 0
        fprintf(stdout," #tx_data: ");
        for(i = 0; i < TX_DATA_NUM; i++)
        {
                fprintf(stdout,"  tx_data: \n", i);
                fprintf(stdout,"tx_address:%04X\n", tcp_param.tx_data[i].tx_address);
                fprintf(stdout,"tx_identifier_level:%04X\n", tcp_param.tx_data[i].tx_identifier_level);
                fprintf(stdout,"tx_data_inhibit:%04X\n", tcp_param.tx_data[i].tx_data_inhibit);
                fprintf(stdout,"tx_time_offset:%04X\n", tcp_param.tx_data[i].tx_time_offset);
                fprintf(stdout,"tx_power:%04X\n", tcp_param.tx_data[i].tx_power);
        }
#endif

#if 0
        fprintf(stdout," #trellis_code_state: ");
        for(i = 0; i<TRELLIS_CODE_LEN; i++)
        {
                fprintf(stdout,"%s%02X%s ", 
                        yellow_on,
                        tcp_param.trellis_code_state[i],
                        color_off);
        }
        fprintf(stdout,"\n");

        fprintf(stdout," #TCP_ECC: ");
        for(i = 0; i < TCP_ECC_LEN; i++)
        {
                fprintf(stdout,"%s%02X%s ", 
                        yellow_on,
                        tcp_param.TCP_ECC[i],
                        color_off);
        }
#endif   
        fprintf(stdout,"\n");
}
