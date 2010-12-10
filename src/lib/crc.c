/* vim: set tabstop=8 shiftwidth=8: */
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

static uint32_t crc32_table[256] =
{
        0x00000000L, 0x77073096L, 0xEE0E612CL, 0x990951BAL,
        0x076DC419L, 0x706AF48FL, 0xE963A535L, 0x9E6495A3L,
        0x0EDB8832L, 0x79DCB8A4L, 0xE0D5E91EL, 0x97D2D988L,
        0x09B64C2BL, 0x7EB17CBDL, 0xE7B82D07L, 0x90BF1D91L,
        0x1DB71064L, 0x6AB020F2L, 0xF3B97148L, 0x84BE41DEL,
        0x1ADAD47DL, 0x6DDDE4EBL, 0xF4D4B551L, 0x83D385C7L,
        0x136C9856L, 0x646BA8C0L, 0xFD62F97AL, 0x8A65C9ECL,
        0x14015C4FL, 0x63066CD9L, 0xFA0F3D63L, 0x8D080DF5L,
        0x3B6E20C8L, 0x4C69105EL, 0xD56041E4L, 0xA2677172L,
        0x3C03E4D1L, 0x4B04D447L, 0xD20D85FDL, 0xA50AB56BL,
        0x35B5A8FAL, 0x42B2986CL, 0xDBBBC9D6L, 0xACBCF940L,
        0x32D86CE3L, 0x45DF5C75L, 0xDCD60DCFL, 0xABD13D59L,
        0x26D930ACL, 0x51DE003AL, 0xC8D75180L, 0xBFD06116L,
        0x21B4F4B5L, 0x56B3C423L, 0xCFBA9599L, 0xB8BDA50FL,
        0x2802B89EL, 0x5F058808L, 0xC60CD9B2L, 0xB10BE924L,
        0x2F6F7C87L, 0x58684C11L, 0xC1611DABL, 0xB6662D3DL,
        0x76DC4190L, 0x01DB7106L, 0x98D220BCL, 0xEFD5102AL,
        0x71B18589L, 0x06B6B51FL, 0x9FBFE4A5L, 0xE8B8D433L,
        0x7807C9A2L, 0x0F00F934L, 0x9609A88EL, 0xE10E9818L,
        0x7F6A0DBBL, 0x086D3D2DL, 0x91646C97L, 0xE6635C01L,
        0x6B6B51F4L, 0x1C6C6162L, 0x856530D8L, 0xF262004EL,
        0x6C0695EDL, 0x1B01A57BL, 0x8208F4C1L, 0xF50FC457L,
        0x65B0D9C6L, 0x12B7E950L, 0x8BBEB8EAL, 0xFCB9887CL,
        0x62DD1DDFL, 0x15DA2D49L, 0x8CD37CF3L, 0xFBD44C65L,
        0x4DB26158L, 0x3AB551CEL, 0xA3BC0074L, 0xD4BB30E2L,
        0x4ADFA541L, 0x3DD895D7L, 0xA4D1C46DL, 0xD3D6F4FBL,
        0x4369E96AL, 0x346ED9FCL, 0xAD678846L, 0xDA60B8D0L,
        0x44042D73L, 0x33031DE5L, 0xAA0A4C5FL, 0xDD0D7CC9L,
        0x5005713CL, 0x270241AAL, 0xBE0B1010L, 0xC90C2086L,
        0x5768B525L, 0x206F85B3L, 0xB966D409L, 0xCE61E49FL,
        0x5EDEF90EL, 0x29D9C998L, 0xB0D09822L, 0xC7D7A8B4L,
        0x59B33D17L, 0x2EB40D81L, 0xB7BD5C3BL, 0xC0BA6CADL,
        0xEDB88320L, 0x9ABFB3B6L, 0x03B6E20CL, 0x74B1D29AL,
        0xEAD54739L, 0x9DD277AFL, 0x04DB2615L, 0x73DC1683L,
        0xE3630B12L, 0x94643B84L, 0x0D6D6A3EL, 0x7A6A5AA8L,
        0xE40ECF0BL, 0x9309FF9DL, 0x0A00AE27L, 0x7D079EB1L,
        0xF00F9344L, 0x8708A3D2L, 0x1E01F268L, 0x6906C2FEL,
        0xF762575DL, 0x806567CBL, 0x196C3671L, 0x6E6B06E7L,
        0xFED41B76L, 0x89D32BE0L, 0x10DA7A5AL, 0x67DD4ACCL,
        0xF9B9DF6FL, 0x8EBEEFF9L, 0x17B7BE43L, 0x60B08ED5L,
        0xD6D6A3E8L, 0xA1D1937EL, 0x38D8C2C4L, 0x4FDFF252L,
        0xD1BB67F1L, 0xA6BC5767L, 0x3FB506DDL, 0x48B2364BL,
        0xD80D2BDAL, 0xAF0A1B4CL, 0x36034AF6L, 0x41047A60L,
        0xDF60EFC3L, 0xA867DF55L, 0x316E8EEFL, 0x4669BE79L,
        0xCB61B38CL, 0xBC66831AL, 0x256FD2A0L, 0x5268E236L,
        0xCC0C7795L, 0xBB0B4703L, 0x220216B9L, 0x5505262FL,
        0xC5BA3BBEL, 0xB2BD0B28L, 0x2BB45A92L, 0x5CB36A04L,
        0xC2D7FFA7L, 0xB5D0CF31L, 0x2CD99E8BL, 0x5BDEAE1DL,
        0x9B64C2B0L, 0xEC63F226L, 0x756AA39CL, 0x026D930AL,
        0x9C0906A9L, 0xEB0E363FL, 0x72076785L, 0x05005713L,
        0x95BF4A82L, 0xE2B87A14L, 0x7BB12BAEL, 0x0CB61B38L,
        0x92D28E9BL, 0xE5D5BE0DL, 0x7CDCEFB7L, 0x0BDBDF21L,
        0x86D3D2D4L, 0xF1D4E242L, 0x68DDB3F8L, 0x1FDA836EL,
        0x81BE16CDL, 0xF6B9265BL, 0x6FB077E1L, 0x18B74777L,
        0x88085AE6L, 0xFF0F6A70L, 0x66063BCAL, 0x11010B5CL,
        0x8F659EFFL, 0xF862AE69L, 0x616BFFD3L, 0x166CCF45L,
        0xA00AE278L, 0xD70DD2EEL, 0x4E048354L, 0x3903B3C2L,
        0xA7672661L, 0xD06016F7L, 0x4969474DL, 0x3E6E77DBL,
        0xAED16A4AL, 0xD9D65ADCL, 0x40DF0B66L, 0x37D83BF0L,
        0xA9BCAE53L, 0xDEBB9EC5L, 0x47B2CF7FL, 0x30B5FFE9L,
        0xBDBDF21CL, 0xCABAC28AL, 0x53B39330L, 0x24B4A3A6L,
        0xBAD03605L, 0xCDD70693L, 0x54DE5729L, 0x23D967BFL,
        0xB3667A2EL, 0xC4614AB8L, 0x5D681B02L, 0x2A6F2B94L,
        0xB40BBE37L, 0xC30C8EA1L, 0x5A05DF1BL, 0x2D02EF8DL
};

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
                case 8:
                        msb = 0x80;
                        genpoly = 0x07;
                        break;
                case 16:
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

uint32_t CRC_for_TS(void *buf, size_t size, int mode)
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
