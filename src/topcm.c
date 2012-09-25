/* vim: set tabstop=8 shiftwidth=8:
 * name: topcm
 * funx: change left-just audio 2 pcm
 * 
 * 2012-09-11, LI WenYan, Init for debug SDI embed audio.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for strcmp, etc */
#include <stdint.h> /* for uint?_t, etc */

#include "version.h"
#include "common.h" /* for RPT() micro */
#include "if.h" /* for next_tag(), etc */

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

#define PKT_BBUF        (256) /* 188 or 204 */
#define PKT_TBUF        (PKT_BBUF * 3 + 10)

#define AUX_ADF_DATA    (0x00FFFF)
#define AUX_HEAD_LEN    (8)
#define AUX_DATA_LEN    (31)
#define LEFT_AUDIO_LEN  (8) /* 2*32bit */
#define AUX_DATA_NUM    (2)
#define MAX_AUDIO_NUM   (10)

#define MODULE_OK       (0)
#define MODULE_ERR      (-1)

enum {
        GOT_WRONG_PKT,
        GOT_RIGHT_PKT,
        GOT_EOF
};

struct pcm_rslt {
        uint8_t pcm[6];
        int pcm_len;
};

struct audio_rslt {
        uint8_t audio_pack[AUX_DATA_LEN * MAX_AUDIO_NUM];
        int audio_len;
};

/* SMPTE291M ancillary data head*/
struct anc_head {
        uint32_t ADF; /* 24-bit */
        uint8_t DID;
        uint8_t DBN;
        uint8_t DC;
};

struct obj {
        int aim_l2p; /* left audio to pcm*/
        int aim_a2p; /* audio pack to pcm*/
        int aim_anc; /* show anc */
        int aim_low_flag; /* get the low 16bit audio flag */
        char esbuf[PKT_TBUF];
        int es_len;
        struct audio_rslt rslt;
};

static struct obj *obj = NULL;
static int get_one_pack(struct obj *obj);
static void aduiopack2pcm32(uint8_t *aduio_data, uint8_t *out_pcm, int low_flag);
static void left64topcm32(uint8_t *in_pcm, uint8_t *out_pcm);
static int get_anc_head(struct anc_head *anc_head, uint8_t *anc_data);
static struct obj *create(int argc, char *argv[]);
static int delete(struct obj *obj);
static void show_help();
static void show_version();

int main(int argc, char *argv[])
{
        int get_rslt;

        obj = create(argc, argv);
        if(NULL == obj)
        {
                exit(EXIT_FAILURE);
        }

        while(GOT_EOF != (get_rslt = get_one_pack(obj))) 
        {
                ;
        }

        delete(obj);
        return 0;
}

static int get_one_pack(struct obj *obj)
{
        int i;
        char *tag;
        char *pt = (char *)(obj->esbuf);
        struct pcm_rslt p_rslt;
        struct audio_rslt *rslt = &obj->rslt;
        struct anc_head ANC_HEAD;
        struct anc_head *anc_head = &ANC_HEAD;
        uint8_t *audio_ptr;

        if(NULL == fgets(obj->esbuf, PKT_TBUF, stdin))
        {
                return GOT_EOF;
        }

        while(0 == next_tag(&tag, &pt))
        {
                if(0 != strcmp(tag, "*es"))
                {
                        continue;
                }

                /* "*es" */
                rslt->audio_len = next_nbyte_hex(rslt->audio_pack, &pt, 188);
                audio_ptr = rslt->audio_pack;

                if(MODULE_OK == get_anc_head(anc_head, audio_ptr))
                {
                        for(i = 0; i < rslt->audio_len; i += AUX_DATA_LEN)
                        {
                                aduiopack2pcm32((audio_ptr + AUX_HEAD_LEN), p_rslt.pcm, obj->aim_low_flag);
                                p_rslt.pcm_len = 4;
                                {
                                        char str[3 * 188 + 3]; /* part of one TS packet */

                                        fprintf(stdout, "*data, ");
                                        b2t(str, p_rslt.pcm, p_rslt.pcm_len);
                                        fprintf(stdout, "%s", str);
                                }
                                if(1 == obj->aim_anc)
                                {
                                        char str[3 * 188 + 3]; /* part of one TS packet */

                                        fprintf(stdout, " *anc, ");
                                        b2t(str, audio_ptr, AUX_DATA_LEN);
                                        fprintf(stdout, "%s", str);
                                }
                                fprintf(stdout, "\n");
                                audio_ptr += AUX_DATA_LEN;
                        }
                }
                else
                {
                        for(i = 0; i < rslt->audio_len; i += LEFT_AUDIO_LEN)
                        {
                                left64topcm32(audio_ptr, p_rslt.pcm);
                                p_rslt.pcm_len = 4;                            
                                {
                                        char str[3 * 188 + 3]; /* part of one TS packet */

                                        fprintf(stdout, "*data, ");
                                        b2t(str, p_rslt.pcm, p_rslt.pcm_len);
                                        fprintf(stdout, "%s\n", str);
                                }
                                audio_ptr += LEFT_AUDIO_LEN;
                        }
                }
        }

        return GOT_RIGHT_PKT;
}

static struct obj *create(int argc, char *argv[])
{
        int i;
        struct obj *obj;

        obj = (struct obj *)malloc(sizeof(struct obj));
        if(NULL == obj) {
                RPT(RPT_ERR, "malloc failed");
                return NULL;
        }

        obj->aim_l2p = 0;
        obj->aim_a2p = 0;
        obj->aim_anc = 0;
        obj->aim_low_flag = 0;

        for(i = 1; i < argc; i++) {
                if('-' == argv[i][0]) {
                        if(0 == strcmp(argv[i], "-a") ||
                           0 == strcmp(argv[i], "--anc"))
                        {
                                obj->aim_anc = 1;
                        }
                        else if(0 == strcmp(argv[i], "-l") ||
                                0 == strcmp(argv[i], "--low"))
                        {
                                obj->aim_low_flag = 1;
                        }
                        else if(0 == strcmp(argv[i], "-h") ||
                                0 == strcmp(argv[i], "--help"))
                        {
                                show_help();
                                exit(EXIT_SUCCESS);
                        }
                        else if(0 == strcmp(argv[i], "-v") ||
                                0 == strcmp(argv[i], "--version"))
                        {
                                show_version();
                                exit(EXIT_SUCCESS);
                        }
                }
                else
                {
                        fprintf(stderr, "Wrong parameter: %s\n", argv[i]);
                        exit(EXIT_FAILURE);
                }
        }

        return obj;
}

static int delete(struct obj *obj)
{
        if(obj) {
                free(obj);
                return 0;
        }
        else {
                return -1;
        }
}

static int get_anc_head(struct anc_head *anc_head, uint8_t *anc_data)
{
        anc_head->ADF = *anc_data++;
        anc_head->ADF <<= 8;    
        anc_head->ADF |= *anc_data++;
        anc_head->ADF <<= 8;    
        anc_head->ADF |= *anc_data++;

        return ((AUX_ADF_DATA == anc_head->ADF) ? MODULE_OK : MODULE_ERR);
}

/* low_flag: 1,get the low 4-20bit; 0,get the high 16bit, big-endine mode */
static void aduiopack2pcm32(uint8_t *aduio_data, uint8_t *out_pcm, int low_flag)
{
        uint8_t *in_data;
        uint8_t *out_data;
        uint32_t aes_data; /* 24-bit audio data */

        in_data = aduio_data;
        out_data = out_pcm;

        aes_data = *(in_data+3)&0x0F; /* high 4-bit no use */
        aes_data <<= 8;
        aes_data |= *(in_data+2);
        aes_data <<= 8;
        aes_data |= *(in_data+1);
        aes_data <<= 8;
        aes_data |= *(in_data+0);
        aes_data >>= 4; /* low 4-bit no use */

        if(1 == low_flag)
        {
                *out_data++ = (aes_data>>12)&0xFF;
                *out_data++ = (aes_data>>4)&0xFF;
        }
        else
        {
                *out_data++ = (aes_data>>16)&0xFF;
                *out_data++ = (aes_data>>8)&0xFF;
        }
        in_data += 4;

        aes_data = *(in_data+3)&0x0F; /* high 4-bit no use */
        aes_data <<= 8;
        aes_data |= *(in_data+2);
        aes_data <<= 8;
        aes_data |= *(in_data+1);
        aes_data <<= 8;
        aes_data |= *(in_data+0);
        aes_data >>= 4; /* low 4-bit no use */

        if(1 == low_flag)
        {
                *out_data++ = (aes_data>>8)&0xFF;
                *out_data++ = (aes_data>>0)&0xFF;
        }
        else
        {
                *out_data++ = (aes_data>>16)&0xFF;
                *out_data++ = (aes_data>>8)&0xFF;
        }

        return;
}

static void left64topcm32(uint8_t *in_pcm, uint8_t *out_pcm)
{
        uint8_t *in_data;
        uint8_t *out_data;

        in_data = in_pcm;
        out_data = out_pcm;

        *out_data++ = *in_data++;
        *out_data++ = *in_data++;

        in_data += 2;
        *out_data++ = *in_data++;
        *out_data++ = *in_data++;
}

static void show_help()
{
        puts("'topcm' get pcm from stdin, analyse, then send the result to stdout.");
        puts("");
        puts("Usage: topcm [OPTION]...");
        puts("");
        puts("Options:");

        puts(" -a, --anc        if stdin data is aux audio, output anc and pcm");
        puts(" -l, --low        if stdin data is aux audio, output 24bit audio low 4-20bit");
        puts("");
        puts(" -h, --help       display this information");
        puts(" -v, --version    display my version");
        puts("");
        puts("Examples:");
        puts("  \"catts xxx.ts | tsana -pid xx -es | topcm | tots xx.pcm\", 2*32bit left audio or anc data to out pcm");
        puts("  \"catts xxx.ts | tsana -pid xx -es | topcm -a \", if stdin data is aux audio, output anc and pcm data");

        puts("");
        puts("Report bugs to <suma.lwy@gmail.com>.");
        return;
}

static void show_version()
{
        char str[100];

        sprintf(str, "topcm of tstools v%s.%s.%s", VER_MAJOR, VER_MINOR, VER_RELEA);
        puts(str);
        sprintf(str, "Build time: %s %s", __DATE__, __TIME__);
        puts(str);
        puts("");
        puts("Copyright (C) 2009,2010,2011,2012 LI WenYan.");
        puts("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>");
        puts("This is free software; contact author for additional information.");
        puts("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR");
        puts("A PARTICULAR PURPOSE.");
        puts("");
        puts("Written by LI WenYan.");
        return;
}
