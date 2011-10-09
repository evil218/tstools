/* vim: set tabstop=8 shiftwidth=8: */
#include <stdio.h>
#include <string.h>
#include "atsc_mh_tcp.h"

#define TCP_LEN (184) /* 188 -4 */
#define TRELLIS_CODE_LEN (12) /* 12 */
#define TX_DATA_NUM (16) /* 20 */
#define TCP_ECC_LEN (20) /* 20 */
#define BIT(n)   (1<<n)

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
        uint8_t syn_time_stamp_base[3];
        uint8_t max_delay_base[3];
        uint16_t network_identifier_pattern; /* 12 */
        uint8_t syn_time_stamp_extension;  /* 1 */
        uint8_t max_delay_base_extension;  /* 1 */
        uint16_t packet_number; /* 10 */
        uint32_t adjusted_gps_seconds_count;
        uint8_t tx_group_number;
        tx_data_t tx_data[TX_DATA_NUM];
        uint32_t AT_frame_number;
        uint32_t atsc_time_displacement;/* 25-7 */
        uint8_t reserved[32];
        uint8_t TCP_ECC[TCP_ECC_LEN];
}tcp_pata_t;

tcp_pata_t tcp_param; 

void show_tcp(uint8_t *ts_pack)
{
        int i;
        uint8_t *tcp_data = (uint8_t *)(ts_pack + 5);
        int tcpLen = sizeof(tcp_param);
        //uint16_t network_identifier_pattern;

        memset(&tcp_param, 0, tcpLen);
        tcp_param.sig_data.packet_frame_number = (*tcp_data)>>3;
        tcp_param.sig_data.mode = ( (*tcp_data)&BIT(2) )>>2;
        tcp_data++;

        for(i = 0; i<TRELLIS_CODE_LEN; i++)
        {
                tcp_param.trellis_code_state[i] = *tcp_data++;
        }


        tcp_param.syn_time_stamp_base[0] = *tcp_data++;
        tcp_param.syn_time_stamp_base[1] = *tcp_data++;
        tcp_param.syn_time_stamp_base[2] = *tcp_data++;

        tcp_param.max_delay_base[0] = *tcp_data++;
        tcp_param.max_delay_base[1] = *tcp_data++;
        tcp_param.max_delay_base[2] = *tcp_data++;

        tcp_param.network_identifier_pattern = *tcp_data++;
        tcp_param.network_identifier_pattern <<= 4;
        tcp_param.network_identifier_pattern |= (*tcp_data>>4);

        tcp_param.syn_time_stamp_extension = ((*tcp_data)&BIT(3))>>3;
        tcp_param.max_delay_base_extension = ((*tcp_data)&BIT(2))>>2;

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

        for(i = 0; i<TX_DATA_NUM; i++)
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

        for(i = 0; i<TCP_ECC_LEN; i++)
        {
                tcp_param.TCP_ECC[i] = *tcp_data++;
        }

        printf(" #sig_data:\n");
        printf("    packet_frame_number:%d, mode:%d\n",tcp_param.sig_data.packet_frame_number, tcp_param.sig_data.mode);

        printf(" #trellis_code_state: ");
        for(i = 0; i<TRELLIS_CODE_LEN; i++)
        {
                if( i%8 == 0 )
                {
                        printf("\n");
                }
                printf(" 0x%02x ", tcp_param.trellis_code_state[i]);
        }

        printf("\n #syn_time_stamp_base:0x%02x%02x%02x\n", 
               tcp_param.syn_time_stamp_base[0],
               tcp_param.syn_time_stamp_base[1],
               tcp_param.syn_time_stamp_base[2]);

        printf(" #max_delay_base:0x%02x%02x%02x\n", 
               tcp_param.max_delay_base[0],
               tcp_param.max_delay_base[1],
               tcp_param.max_delay_base[2]);

        printf(" #network_identifier_pattern:%d\n", tcp_param.network_identifier_pattern);
        printf(" #syn_time_stamp_extension:%d\n", tcp_param.syn_time_stamp_extension);
        printf(" #max_delay_base_extension:%d\n", tcp_param.max_delay_base_extension);
        printf(" #packet_number:0x%04x\n", tcp_param.packet_number);

        printf(" #adjusted_gps_seconds_count:%d\n", tcp_param.adjusted_gps_seconds_count);
        printf(" #tx_group_number:%d\n", tcp_param.tx_group_number);

#if 0
        printf(" #tx_data:");
        for(i = 0; i<TX_DATA_NUM; i++)
        {
                printf("   tx_data: \n", i);
                printf(" tx_address:0x%03X\n", tcp_param.tx_data[i].tx_address);
                printf(" tx_identifier_level:%d\n", tcp_param.tx_data[i].tx_identifier_level);
                printf(" tx_data_inhibit:%d\n", tcp_param.tx_data[i].tx_data_inhibit);
                printf(" tx_time_offset:0x%04X\n", tcp_param.tx_data[i].tx_time_offset);
                printf(" tx_power:0x%03X\n", tcp_param.tx_data[i].tx_power);
        }
#endif
        printf(" #AT_frame_number:%d\n", tcp_param.AT_frame_number);
        printf(" #atsc_time_displacement:%d\n", tcp_param.atsc_time_displacement);

        printf("\n #TCP_ECC:");
        for(i = 0; i<TCP_ECC_LEN; i++)
        {
                if( i%8 == 0 )
                {
                        printf("\n");
                }
                printf(" 0x%02X ", tcp_param.TCP_ECC[i]);
        }
        printf("\n");
}
