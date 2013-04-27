/* vim: set tabstop=8 shiftwidth=8:
 * name: param_xml
 * funx: common information for parameter xml convertor
 *          _______             _______                        _____
 *         |       | param2xml |       | xmlSaveFormatFileEnc |     |
 *         | param |---------->| xnode |--------------------->| XML |
 *         |       |     *     |       |                      |     |
 *         |       |<--------- |       |<---------------------|     |
 *         |_______| xml2param |_______|     xmlParseFile     |_____|
 *             A         A         A                             A
 *             |         |         |                             |
 *       (struct tree)   |    (xnode tree)                   (XML file)
 *                       |
 *                  (pdesc tree): parameter descriptive tree
 * 
 * 2011-09-18, ZHOU Cheng, collate PT_????; modified to avoid "callback funx"
 * 2008-11-20, LI Xin, parse xml file into param struct
 */

#ifndef _PARAM_XML_H
#define _PARAM_XML_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h> /* for intptr_t, int64_t, PRIX64, etc */

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

/* mask and mark */
#define PT_TYP_MASK (0xF000) /* basic type */
#define PT_ACS_MASK (0x0800) /* access mode */
#define PT_CNT_MASK (0x0400) /* array count mode */
#define PT_FMT_MASK (0x000F) /* text format */

#define PT_TYP(x) ((x) & PT_TYP_MASK)
#define PT_ACS(x) ((x) & PT_ACS_MASK)
#define PT_CNT(x) ((x) & PT_CNT_MASK)
#define PT_FMT(x) ((x) & PT_FMT_MASK)

/* basic type */
#define PT_TYP_NULL (0x0000) /* should be ZERO to avoid endless loop! */
#define PT_TYP_SINT (0x1000) /* signed int, [8,16,32,64]-bit */
#define PT_TYP_UINT (0x2000) /* unsigned int, [8,16,32,64]-bit */
#define PT_TYP_FLOT (0x3000) /* float, double, long double */
#define PT_TYP_STRI (0x4000) /* string zero */
#define PT_TYP_ENUM (0x5000) /* enum, int in struct <---> string in xml */
#define PT_TYP_STRU (0x6000) /* struct */
#define PT_TYP_LIST (0x7000) /* list, should use "zlst" module */
#define PT_TYP_VLST (0x8000) /* like PT_LIST, but each node can be different struct */

/* access mode */
#define PT_ACS_S (0x0000) /* static: direct access */
#define PT_ACS_X (0x0800) /* dynamic buffer(need xmlMalloc and xmlFree) */

/* array count mode */
#define PT_CNT_S (0x0000) /* static: pdesc->count */
#define PT_CNT_X (0x0400) /* dynamic: another parameter */

/* text format */
#define PT_FMT_u (0x0001) /* %u */
#define PT_FMT_x (0x0002) /* %x */
#define PT_FMT_X (0x0003) /* %X */

#define PT_TYP_UINTu (PT_TYP_UINT | PT_FMT_u)
#define PT_TYP_UINTx (PT_TYP_UINT | PT_FMT_x)
#define PT_TYP_UINTX (PT_TYP_UINT | PT_FMT_X)

/* data format in XML */
#define PT_NULL     (PT_TYP_NULL) /* should be ZERO to avoid endless loop! */

#define PT_SINT__SS (PT_TYP_SINT  | PT_ACS_S | PT_CNT_S) /*   intN_t  a[20]; %d */
#define PT_UINTu_SS (PT_TYP_UINTu | PT_ACS_S | PT_CNT_S) /*  uintN_t  a[20]; %u */
#define PT_UINTx_SS (PT_TYP_UINTx | PT_ACS_S | PT_CNT_S) /*  uintN_t  a[20]; %x */
#define PT_UINTX_SS (PT_TYP_UINTX | PT_ACS_S | PT_CNT_S) /*  uintN_t  a[20]; %X */
#define PT_FLOT__SS (PT_TYP_FLOT  | PT_ACS_S | PT_CNT_S) /*    float  a[20]; */
#define PT_STRI__SS (PT_TYP_STRI  | PT_ACS_S | PT_CNT_S) /*     char  a[20][max_size] */
#define PT_ENUM__SS (PT_TYP_ENUM  | PT_ACS_S | PT_CNT_S) /*      int  a[20]; */
#define PT_STRU__SS (PT_TYP_STRU  | PT_ACS_S | PT_CNT_S) /* struct x  a[20]; */

#define PT_SINT__XS (PT_TYP_SINT  | PT_ACS_X | PT_CNT_S) /*   intN_t *a[20]; %d, int a_len[20]; */
#define PT_UINTu_XS (PT_TYP_UINTu | PT_ACS_X | PT_CNT_S) /*  uintN_t *a[20]; %u, int a_len[20]; */
#define PT_UINTx_XS (PT_TYP_UINTx | PT_ACS_X | PT_CNT_S) /*  uintN_t *a[20]; %x, int a_len[20]; */
#define PT_UINTX_XS (PT_TYP_UINTX | PT_ACS_X | PT_CNT_S) /*  uintN_t *a[20]; %X, int a_len[20]; */
#define PT_FLOT__XS (PT_TYP_FLOT  | PT_ACS_X | PT_CNT_S) /*    float *a[20]; */
#define PT_STRU__XS (PT_TYP_STRU  | PT_ACS_X | PT_CNT_S) /* struct x *a[20];     int a_len[20]; */
#define PT_LIST__XS (PT_TYP_LIST  | PT_ACS_X | PT_CNT_S) /* struct x *a[20]; */
#define PT_VLST__XS (PT_TYP_VLST  | PT_ACS_X | PT_CNT_S) /*     void *a[20]; */

/* byte offset of member in structure */
#ifndef OFFSET
#define OFFSET(structure, member) ((intptr_t)&(((structure *)0)->member))
#endif

/* for enum lookup table */
struct enume {
        const char *key;
        int value;
};

/* auxiliary description */
struct adesc {
        intptr_t aoffset; /* array count parameter offset */
        intptr_t boffset; /* buffer count parameter offset */
        intptr_t size; /* n-byte, sizeof(one param) */
        struct pdesc *pdesc; /* each parameter of struct xxx */
        const char *name; /* name of struct xxx */
};

/* parameter description */
struct pdesc {
        int type; /* [PT_SINT, PT_VLST, ...] */
        int index; /* index of current array item, modified by param_xml.c */
        int count; /* array count */
        intptr_t offset; /* memory offset from struct head */
        const char *name; /* node name(xml), param name(struct) */
        intptr_t size; /* n-byte, sizeof(param) */
        intptr_t aux; /* data or pointer to auxiliary description */

        /* callback function, for user expansion */
        int (*toxml)(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
        int (*xmlto)(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
};

/* module interface, reentrant */
int param2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
int xml2param(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);

#ifdef __cplusplus
}
#endif

#endif /* _PARAM_XML_H */
