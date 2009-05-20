//=============================================================================
// Name: crc.c
// Purpose: Calculate 16-bit or 32-bit CRC for several 8-bit data
// To build: gcc -std-c99 -c crc.c
//=============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "crc.h"

//=============================================================================
// Variables definition:
//=============================================================================
static uint8_t crc8_table[256];

static uint16_t crc16_table[256] =
{
        0x0000U, 0x1021U, 0x2042U, 0x3063U, 0x4084U, 0x50A5U, 0x60C6U, 0x70E7U,
        0x8108U, 0x9129U, 0xA14AU, 0xB16BU, 0xC18CU, 0xD1ADU, 0xE1CEU, 0xF1EFU,
        0x1231U, 0x0210U, 0x3273U, 0x2252U, 0x52B5U, 0x4294U, 0x72F7U, 0x62D6U,
        0x9339U, 0x8318U, 0xB37BU, 0xA35AU, 0xD3BDU, 0xC39CU, 0xF3FFU, 0xE3DEU,
        0x2462U, 0x3443U, 0x0420U, 0x1401U, 0x64E6U, 0x74C7U, 0x44A4U, 0x5485U,
        0xA56AU, 0xB54BU, 0x8528U, 0x9509U, 0xE5EEU, 0xF5CFU, 0xC5ACU, 0xD58DU,
        0x3653U, 0x2672U, 0x1611U, 0x0630U, 0x76D7U, 0x66F6U, 0x5695U, 0x46B4U,
        0xB75BU, 0xA77AU, 0x9719U, 0x8738U, 0xF7DFU, 0xE7FEU, 0xD79DU, 0xC7BCU,
        0x48C4U, 0x58E5U, 0x6886U, 0x78A7U, 0x0840U, 0x1861U, 0x2802U, 0x3823U,
        0xC9CCU, 0xD9EDU, 0xE98EU, 0xF9AFU, 0x8948U, 0x9969U, 0xA90AU, 0xB92BU,
        0x5AF5U, 0x4AD4U, 0x7AB7U, 0x6A96U, 0x1A71U, 0x0A50U, 0x3A33U, 0x2A12U,
        0xDBFDU, 0xCBDCU, 0xFBBFU, 0xEB9EU, 0x9B79U, 0x8B58U, 0xBB3BU, 0xAB1AU,
        0x6CA6U, 0x7C87U, 0x4CE4U, 0x5CC5U, 0x2C22U, 0x3C03U, 0x0C60U, 0x1C41U,
        0xEDAEU, 0xFD8FU, 0xCDECU, 0xDDCDU, 0xAD2AU, 0xBD0BU, 0x8D68U, 0x9D49U,
        0x7E97U, 0x6EB6U, 0x5ED5U, 0x4EF4U, 0x3E13U, 0x2E32U, 0x1E51U, 0x0E70U,
        0xFF9FU, 0xEFBEU, 0xDFDDU, 0xCFFCU, 0xBF1BU, 0xAF3AU, 0x9F59U, 0x8F78U,
        0x9188U, 0x81A9U, 0xB1CAU, 0xA1EBU, 0xD10CU, 0xC12DU, 0xF14EU, 0xE16FU,
        0x1080U, 0x00A1U, 0x30C2U, 0x20E3U, 0x5004U, 0x4025U, 0x7046U, 0x6067U,
        0x83B9U, 0x9398U, 0xA3FBU, 0xB3DAU, 0xC33DU, 0xD31CU, 0xE37FU, 0xF35EU,
        0x02B1U, 0x1290U, 0x22F3U, 0x32D2U, 0x4235U, 0x5214U, 0x6277U, 0x7256U,
        0xB5EAU, 0xA5CBU, 0x95A8U, 0x8589U, 0xF56EU, 0xE54FU, 0xD52CU, 0xC50DU,
        0x34E2U, 0x24C3U, 0x14A0U, 0x0481U, 0x7466U, 0x6447U, 0x5424U, 0x4405U,
        0xA7DBU, 0xB7FAU, 0x8799U, 0x97B8U, 0xE75FU, 0xF77EU, 0xC71DU, 0xD73CU,
        0x26D3U, 0x36F2U, 0x0691U, 0x16B0U, 0x6657U, 0x7676U, 0x4615U, 0x5634U,
        0xD94CU, 0xC96DU, 0xF90EU, 0xE92FU, 0x99C8U, 0x89E9U, 0xB98AU, 0xA9ABU,
        0x5844U, 0x4865U, 0x7806U, 0x6827U, 0x18C0U, 0x08E1U, 0x3882U, 0x28A3U,
        0xCB7DU, 0xDB5CU, 0xEB3FU, 0xFB1EU, 0x8BF9U, 0x9BD8U, 0xABBBU, 0xBB9AU,
        0x4A75U, 0x5A54U, 0x6A37U, 0x7A16U, 0x0AF1U, 0x1AD0U, 0x2AB3U, 0x3A92U,
        0xFD2EU, 0xED0FU, 0xDD6CU, 0xCD4DU, 0xBDAAU, 0xAD8BU, 0x9DE8U, 0x8DC9U,
        0x7C26U, 0x6C07U, 0x5C64U, 0x4C45U, 0x3CA2U, 0x2C83U, 0x1CE0U, 0x0CC1U,
        0xEF1FU, 0xFF3EU, 0xCF5DU, 0xDF7CU, 0xAF9BU, 0xBFBAU, 0x8FD9U, 0x9FF8U,
        0x6E17U, 0x7E36U, 0x4E55U, 0x5E74U, 0x2E93U, 0x3EB2U, 0x0ED1U, 0x1EF0U
};

static uint32_t crc32_table[256];

//=============================================================================
// Sub-function declare:
//=============================================================================
static void init_crc32_table(uint32_t *table);

//=============================================================================
// Public functions definition:
//=============================================================================
void crc_init()
{
        //init_crc8_table(crc8_table);
        //init_crc16_table(crc16_table);
        init_crc32_table(crc32_table);
}

// polynomial: 0x07(MSB first) ?
// polynomial: 0xE0(LSB first) ?
uint8_t crc8(void *buf, size_t size)
{
        uint8_t crc;
        uint8_t *data = (uint8_t *)buf;

        crc = 0;
        while(size--)
        {
                crc = crc8_table[(crc >> 8 ^ *(data++)) & 0xFF] ^ (crc << 8); // ?
        }

        return crc;
}

// polynomial: 0x1021(MSB first)
// polynomial: 0x8408(LSB first)
uint16_t crc16(void *buf, size_t size)
{
        uint16_t crc;
        uint8_t *data = (uint8_t *)buf;

        crc = 0;
        while(size--)
        {
                crc = crc16_table[(crc >> 8 ^ *(data++)) & 0xFF] ^ (crc << 8);
        }

        return crc;
}

// polynomial: 0x04C11DB7(MSB first)
// polynomial: 0xEDB88320(LSB first)
uint32_t crc32(void *buf, size_t size)
{
        uint32_t crc;
        uint8_t *data = (uint8_t *)buf;

        crc = 0;
        while(size--)
        {
                crc = crc32_table[(crc ^ *(data++)) & 0xFF] ^ (( crc >> 8) & 0x00FFFFFF);
        }

        return crc;
}

uint32_t CRC(void *buf, size_t size, int mode)
{
        int i;
        uint32_t x;
        uint32_t accum;
        uint32_t genpoly;
        uint32_t msb;
        uint8_t *data = (uint8_t *)buf;

        switch(mode)
        {
                case MODE_CRC8:
                        msb = 0x80;
                        genpoly = 0x07;
                        break;
                case MODE_CRC16:
                        msb = 0x8000;
                        genpoly = 0x1021;
                        break;
                default:
                        msb = 0x80000000;
                        genpoly = 0x04C11DB7;
                        break;
        }

        accum = 0;
        while(size--)
        {
                x = *data++;
                x <<= 8;
                for(i = 8; i > 0; i--)
                {
                        if((x ^ accum) & msb)
                        {
                                accum = (accum << 1) ^ genpoly;
                        }
                        else
                        {
                                accum <<= 1;
                        }
                        x <<= 1;
                }
        }
        return accum;
}

#if 0
uint32_t CRC(void *buf, size_t size, int mode)
{
        int bitcount = 0;
        int bitinbyte = 0;
        unsigned short databit;
        unsigned short shiftreg[32];
        //                      0 1 2 3 4 5 6 7 8
        unsigned short g08[] = {1,1,1,0,0,0,0,0,1};
        //                      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6
        unsigned short g16[] = {1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,1};
        //                      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2
        unsigned short g32[] = {1,1,1,0,1,1,0,1,1,0,1,1,1,0,0,0,1,0,0,0,0,0,1,1,0,0,1,0,0,0,0,0,1};

        unsigned short *g;
        int i,nrbits;
        char *data;
        int cnt;
        uint32_t crc;

        switch(mode)
        {
                case  8: g = g08; cnt =  8; break;
                case 16: g = g16; cnt = 16; break;
                default: g = g32; cnt = 32; break;
        }
        /* Initialize shift register's to '1' */
        for(i = 0; i < cnt; i++)
        {
                shiftreg[i] = 1;
        }

        /* Calculate nr of data bits */
        nrbits = ((int) size) * 8;
        data = buf;

        while(bitcount < nrbits)
        {
                /* Fetch bit from bitstream */
                databit = (short int) (*data  & (0x80 >> bitinbyte));
                databit = databit >> (7 - bitinbyte);
                bitinbyte++;
                bitcount++;
                if(bitinbyte == 8)
                {
                        bitinbyte = 0;
                        data++;
                }
                /* Perform the shift and modula 2 addition */
                databit ^= shiftreg[cnt - 1];
                i = cnt - 1;
                while (i != 0)
                {
                        if (g[i])
                        {
                                shiftreg[i] = shiftreg[i-1] ^ databit;
                        }
                        else
                        {
                                shiftreg[i] = shiftreg[i-1];
                        }
                        i--;
                }
                shiftreg[0] = databit;
        }
        /* make CRC an UIMSBF */
        crc = 0;
        for(i = 0; i < cnt; i++)
        {
                crc = (crc << 1) | ((unsigned int) shiftreg[cnt - 1 -i]);
        }

        return crc;
}
#endif
//=============================================================================
// Subfunctions definition:
//=============================================================================
static void init_crc32_table(uint32_t *table)
{
        uint32_t i,j;
        uint32_t crc;

        for (i = 0; i < 256; i++)
        {
                crc = i;
                for (j = 0; j < 8; j++)
                {
                        if (crc & 1)
                        {
                                crc = (crc >> 1) ^ 0xEDB88320;
                        }
                        else
                        {
                                crc >>= 1;
                        }
                }
                table[i] = crc;
        }
}

/*****************************************************************************
 * End
 ****************************************************************************/
