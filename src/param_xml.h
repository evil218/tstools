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
 * 2008-11-20, LI Xin, parse xml file into param struct
 * 2011-09-18, ZHOU Cheng, collate PT_????; modified to avoid "callback funx"
 */

#ifndef _PARAM_XML_H
#define _PARAM_XML_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

/* mask */
#define PT_FMT_MASK (0x00FF) /* BIT[ 7:0] is data format */
#define PT_TYP_MASK (0xFF00) /* BIT[15:8] is data type */

/* parameter type */
#define PT_NULL (0x00000000) /* should be ZERO to avoid endless loop! */
#define PT_SINT (0x00000100) /* signed int, [8,16,32,64]-bit */
#define PT_UINT (0x00000200) /* unsigned int, [8,16,32,64]-bit */
#define PT_FLOT (0x00000300) /* float, double, long double */
#define PT_STRI (0x00000400) /* string zero */
#define PT_ENUM (0x00000500) /* enum, int in struct <---> string in xml */
#define PT_STRU (0x00000600) /* struct */
#define PT_LIST (0x00000700) /* list, should use "zlst" module */
#define PT_VLST (0x00000800) /* like PT_LIST, but each node can be different struct */

/* data format in XML */
#define PT_NULL_ (PT_NULL)
#define PT_SINT_ (PT_SINT)
#define PT_UINTu (PT_UINT | 'u')
#define PT_UINTx (PT_UINT | 'x')
#define PT_UINTX (PT_UINT | 'X')
#define PT_FLOTg (PT_FLOT | 'g')
#define PT_FLOTG (PT_FLOT | 'G')
#define PT_STRI_ (PT_STRI)
#define PT_ENUM_ (PT_ENUM)
#define PT_STRU_ (PT_STRU)
#define PT_LIST_ (PT_LIST)
#define PT_VLST_ (PT_VLST)

/* byte offset of member in structure */
#ifndef OFFSET
#define OFFSET(structure, member) ((int) &(((structure *) 0) -> member))
#endif

/* for enum lookup table */
struct enume {
        const char *key;
        int value;
};

/* parameter description */
struct pdesc {
        int type; /* [PT_SINT, PT_VLST, ...] */
        int index; /* index of current array item, modified by param_xml.c */
        int count; /* array count */
        int offset; /* memory offset from struct head */
        const char *name; /* node name(xml), param name(struct) */
        int size; /* n-byte, sizeof(param) */
        int aux; /* deferent for STRI|ENUM|STRU|LIST|VLST */

        /* callback function, for user expansion */
        int (*toxml)(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
        int (*xmlto)(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
};

/* struct description, for PT_LIST and PT_VLST */
struct tdesc {
        int size; /* n-byte, sizeof(struct xxx) */
        struct pdesc *pdesc; /* each parameter of struct xxx */
        const char *name; /* name of struct xxx */
};

/* module interface, reentrant */
int param2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
int xml2param(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);

#ifdef __cplusplus
}
#endif

#endif /* _PARAM_XML_H */
