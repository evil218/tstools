/* vim: set tabstop=8 shiftwidth=8: */
#include <string.h> /* for memcpy() only */
#include <float.h> /* for FLT_DIG, DBL_DIG, LDBL_DIG, etc */
#include <ctype.h> /* for isspace() */

#include "zlst.h"
#include "param_xml.h"

#ifdef HAVE_STDINT_H
#include <stdint.h> /* for uint32_t, etc */
#endif

#ifdef _DEBUG
#define dbg(level, ...) \
        do { \
                if (level >= 0) { \
                        fprintf(stderr, "\"%s\", line %d: ", __FILE__, __LINE__); \
                        fprintf(stderr, __VA_ARGS__); \
                        fprintf(stderr, "\n"); \
                } \
        } while (0)
#else /* NDEBUG */
#define dbg(level, ...)
#endif /* NDEBUG */

#ifdef DBL_DIG
#ifdef LDBL_DIG
#if DBL_DIG != LDBL_DIG /* in system like VxWorks5.5, "double" is equal to "long double" */
#ifdef  HAVE_STRTOLD
#define SUPPORT_LONG_DOUBLE
#endif /* HAVE_STRTOLD */
#endif /* DBL_DIG != LDBL_DIG */
#endif /* LDBL_DIG */
#endif /* DBL_DIG */

#if 1
#define MORE_IDX /* more "idx" attribute in XML */
#endif

#define CTNT_LENGTH (512) /* size of str_ctnt[] */
#define DATA_SPACE (' ') /* space char for PT_SINT, PT_UINT and PT_FLOT */

static const xmlChar xStrIdx[] = "idx";
static const xmlChar xStrTyp[] = "typ";

static int sint2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int uint2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int flot2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int stri2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int enum2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int stru2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int list2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int vlst2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);

static int xml2sint(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int xml2uint(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int xml2flot(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int xml2stri(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int xml2enum(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int xml2stru(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int xml2list(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);
static int xml2vlst(void *mem_base, xmlNode *xnode, struct pdesc *pdesc);

/* for speed reason, define sint2str micro and uint2str micro here */
#define UINT64_MAX_DEC_LENGTH 20 /* UINT64_MAX is 1.8e+19 level */

#define sint2str(STR, sdat, type, i, str) \
        do { \
                type udat; \
                \
                /* sign */ \
                if(sdat < 0) { \
                        *STR++ = '-'; \
                        sdat = -sdat; /* INT_MIN = -INT_MIN */ \
                } \
                udat = *((type *)&sdat); \
                \
                /* convert into str[] */ \
                for(i = 0; i < UINT64_MAX_DEC_LENGTH; i++) { \
                        str[i] = (char)(udat % 10) + '0'; \
                        udat /= 10; \
                        if(!udat) {break;} \
                } \
                \
                /* move into STR[] */ \
                for(; i >= 0; i--) { \
                        *STR++ = str[i]; \
                } \
                *STR = '\0'; \
        } while(0)

#define uint2str(STR, udat, FORMAT, i, str) \
        do { \
                /* convert into str[], optimized with 3-case */ \
                switch(FORMAT) { \
                case 'x': \
                        *STR++ = '0'; \
                        *STR++ = 'x'; \
                        for(i = 0; i < UINT64_MAX_DEC_LENGTH; i++) { \
                                str[i] = (char)(udat & 0x0F); \
                                str[i] += ((str[i] <= 9) ?  '0' : ('a' - 10)); \
                                udat >>= 4; \
                                if(!udat) {break;} \
                        } \
                break; \
                case 'X': \
                        *STR++ = '0'; \
                        *STR++ = 'x'; \
                        for(i = 0; i < UINT64_MAX_DEC_LENGTH; i++) { \
                                str[i] = (char)(udat & 0x0F); \
                                str[i] += ((str[i] <= 9) ?  '0' : ('A' - 10)); \
                                udat >>= 4; \
                                if(!udat) {break;} \
                        } \
                break; \
                default: /* 'u' or others */ \
                        for(i = 0; i < UINT64_MAX_DEC_LENGTH; i++) { \
                                str[i] = (char)(udat % 10) + '0'; \
                                udat /= 10; \
                                if(!udat) {break;} \
                        } \
                break; \
                } \
                \
                /* move into STR[] */ \
                for(; i >= 0; i--) { \
                        *STR++ = str[i]; \
                } \
                *STR = '\0'; \
        } while(0)

#ifndef INTMAX_MAX
typedef int64_t intmax_t;
typedef uint64_t uintmax_t;
#endif /* !INTMAX_MAX */

static const intmax_t intmax_max = +9223372036854775807LL;
static const uintmax_t uintmax_imax = +9223372036854775807ULL;
static const uintmax_t uintmax_imin = +9223372036854775808ULL;
static const uintmax_t uintmax_umax = 18446744073709551615ULL;

static  intmax_t strtoimax(const char *nptr, char **endptr, int base);
static uintmax_t strtoumax(const char *nptr, char **endptr, int base);

/* module interface */
int param2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        struct pdesc *cur_pdesc;

        for(cur_pdesc = pdesc; cur_pdesc->type; cur_pdesc++) {
                if(cur_pdesc->toxml) {
                        dbg(1, "param2xml: toxml");
                        cur_pdesc->toxml(mem_base, xnode, cur_pdesc);
                        continue;
                }
                switch(cur_pdesc->type & PT_TYP_MASK) {
                        case PT_SINT: sint2xml(mem_base, xnode, cur_pdesc); break;
                        case PT_UINT: uint2xml(mem_base, xnode, cur_pdesc); break;
                        case PT_FLOT: flot2xml(mem_base, xnode, cur_pdesc); break;
                        case PT_STRI: stri2xml(mem_base, xnode, cur_pdesc); break;
                        case PT_ENUM: enum2xml(mem_base, xnode, cur_pdesc); break;
                        case PT_STRU: stru2xml(mem_base, xnode, cur_pdesc); break;
                        case PT_LIST: list2xml(mem_base, xnode, cur_pdesc); break;
                        case PT_VLST: vlst2xml(mem_base, xnode, cur_pdesc); break;
                        default: dbg(1, "param2xml: bad type(0x%X)", cur_pdesc->type); break;
                }
        }
        return 0;
}

int xml2param(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        xmlNode *sub_xnode;
        struct pdesc *cur_pdesc;

        /* clear pdesc->index */
        /* FIXME: if the pdesc array be used in subnode of the xnode, pdesc->index will be wrong! */
        for(cur_pdesc = pdesc; cur_pdesc->type; cur_pdesc++) {
                cur_pdesc->index = 0;
        }

        for(sub_xnode = xnode->xmlChildrenNode; sub_xnode; sub_xnode = sub_xnode->next) {

                /* search cur_pdesc */
                for(cur_pdesc = pdesc; cur_pdesc->type; cur_pdesc++) {
                        if(xmlStrEqual((xmlChar *)(cur_pdesc->name), sub_xnode->name)) {
                                break;
                        }
                }
                if(!(cur_pdesc->type)) {
                        /* fprintf(stderr, "no 0x%X->%s\n", (int)sub_xnode, (char *)sub_xnode->name); */
                        /* FIXME: why many sub_xnode has name "text"? */
                        continue;
                }

                if(cur_pdesc->xmlto) {
                        dbg(1, "xml2param: xmlto");
                        cur_pdesc->xmlto(mem_base, sub_xnode, cur_pdesc);
                        continue;
                }
                switch(cur_pdesc->type & PT_TYP_MASK) {
                        case PT_SINT: xml2sint(mem_base, sub_xnode, cur_pdesc); break;
                        case PT_UINT: xml2uint(mem_base, sub_xnode, cur_pdesc); break;
                        case PT_FLOT: xml2flot(mem_base, sub_xnode, cur_pdesc); break;
                        case PT_STRI: xml2stri(mem_base, sub_xnode, cur_pdesc); break;
                        case PT_ENUM: xml2enum(mem_base, sub_xnode, cur_pdesc); break;
                        case PT_STRU: xml2stru(mem_base, sub_xnode, cur_pdesc); break;
                        case PT_LIST: xml2list(mem_base, sub_xnode, cur_pdesc); break;
                        case PT_VLST: xml2vlst(mem_base, sub_xnode, cur_pdesc); break;
                        default: dbg(1, "xml2param: bad type(0x%X)", cur_pdesc->type); break;
                }
        }
        return 0;
}

/* subfunctions */
static int sint2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        int i;
        char *p;
        char *mem = (char *)mem_base + pdesc->offset;
        int count = pdesc->count;
        xmlNode *sub_xnode;
        char str_ctnt[CTNT_LENGTH];

        int data_str_idx; /* in sint2str micro: index of data_str */
        char data_str[UINT64_MAX_DEC_LENGTH]; /* in sint2str micro: string of one data */

        i = 0;
        while(i < count) {
                int n; /* n-data per line */

                sub_xnode = xmlNewNode(NULL, (const xmlChar *)pdesc->name);
                if(!sub_xnode) {
                        return -1;
                }

                if(i) {
                        char str_idx[10];

                        sprintf(str_idx, "%d", i);
                        xmlNewProp(sub_xnode, xStrIdx, (const xmlChar *)str_idx);
                }

                switch(pdesc->size) {
                        case sizeof(int8_t):
                                for(p = str_ctnt, n = 0; (i < count) && (n < 16); i++, n++) {
                                        int8_t  sdat = *(((int8_t *)mem) + i);
                                        if(n) {*p++ = DATA_SPACE;}
                                        sint2str(p, sdat, uint8_t, data_str_idx, data_str);
                                }
                                break;
                        case sizeof(int16_t):
                                for(p = str_ctnt, n = 0; (i < count) && (n < 16); i++, n++) {
                                        int16_t  sdat = *(((int16_t *)mem) + i);
                                        if(n) {*p++ = DATA_SPACE;}
                                        sint2str(p, sdat, uint16_t, data_str_idx, data_str);
                                }
                                break;
                        case sizeof(int32_t):
                                for(p = str_ctnt, n = 0; (i < count) && (n < 8); i++, n++) {
                                        int32_t  sdat = *(((int32_t *)mem) + i);
                                        if(n) {*p++ = DATA_SPACE;}
                                        sint2str(p, sdat, uint32_t, data_str_idx, data_str);
                                }
                                break;
                        case sizeof(int64_t):
                                for(p = str_ctnt, n = 0; (i < count) && (n < 4); i++, n++) {
                                        int64_t  sdat = *(((int64_t *)mem) + i);
                                        if(n) {*p++ = DATA_SPACE;}
                                        sint2str(p, sdat, uint64_t, data_str_idx, data_str);
                                }
                                break;
                        default:
                                sprintf(str_ctnt, "unsupported data size(%d)", pdesc->size);
                                i = count; /* stop converter */
                                break;
                }

                dbg(1, "sint2xml: %s", str_ctnt);
                xmlAddChild(xnode, sub_xnode);
                xmlNodeAddContent(sub_xnode, (const xmlChar *)str_ctnt);
        }
        return 0;
}

static int uint2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        int i;
        char *p;
        char fmt = (char)(pdesc->type & PT_FMT_MASK);
        char *mem = (char *)mem_base + pdesc->offset;
        int count = pdesc->count;
        xmlNode *sub_xnode;
        char str_ctnt[CTNT_LENGTH];

        int data_str_idx; /* in uint2str micro: index of data_str */
        char data_str[UINT64_MAX_DEC_LENGTH]; /* in uint2str micro: string of one data */

        i = 0;
        while(i < count) {
                int n; /* n-data per line */

                sub_xnode = xmlNewNode(NULL, (const xmlChar *)pdesc->name);
                if(!sub_xnode) {
                        return -1;
                }

                if(i) {
                        char str_idx[10];

                        sprintf(str_idx, "%d", i);
                        xmlNewProp(sub_xnode, xStrIdx, (const xmlChar *)str_idx);
                }

                switch(pdesc->size) {
                        case sizeof(uint8_t):
                                for(p = str_ctnt, n = 0; (i < count) && (n < 16); i++, n++) {
                                        uint8_t udat = *(((uint8_t *)mem) + i);
                                        if(n) {*p++ = DATA_SPACE;}
                                        uint2str(p, udat, fmt, data_str_idx, data_str);
                                }
                                break;
                        case sizeof(uint16_t):
                                for(p = str_ctnt, n = 0; (i < count) && (n < 16); i++, n++) {
                                        uint16_t udat = *(((uint16_t *)mem) + i);
                                        if(n) {*p++ = DATA_SPACE;}
                                        uint2str(p, udat, fmt, data_str_idx, data_str);
                                }
                                break;
                        case sizeof(uint32_t):
                                for(p = str_ctnt, n = 0; (i < count) && (n < 8); i++, n++) {
                                        uint32_t udat = *(((uint32_t *)mem) + i);
                                        if(n) {*p++ = DATA_SPACE;}
                                        uint2str(p, udat, fmt, data_str_idx, data_str);
                                }
                                break;
                        case sizeof(uint64_t):
                                for(p = str_ctnt, n = 0; (i < count) && (n < 4); i++, n++) {
                                        uint64_t udat = *(((uint64_t *)mem) + i);
                                        if(n) {*p++ = DATA_SPACE;}
                                        uint2str(p, udat, fmt, data_str_idx, data_str);
                                }
                                break;
                        default:
                                sprintf(str_ctnt, "unsupported data size(%d)", pdesc->size);
                                i = count; /* stop converter */
                                break;
                }

                dbg(1, "uint2xml: %s", str_ctnt);
                xmlAddChild(xnode, sub_xnode);
                xmlNodeAddContent(sub_xnode, (const xmlChar *)str_ctnt);
        }
        return 0;
}

static int flot2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        int i;
        char *p;
        char *fmt; /* float or double */
        char *lfmt; /* long double */
        char *mem = (char *)mem_base + pdesc->offset;
        int count = pdesc->count;
        xmlNode *sub_xnode;
        char str_ctnt[CTNT_LENGTH];

        /* fmt and lfmt, use [gG] to auto choose [fF] or [eE] format */
        if('G' == (pdesc->type & PT_FMT_MASK)) {
                        fmt = "%.*G";
                        lfmt = "%.*LG";
        }
        else {
                        fmt = "%.*g";
                        lfmt = "%.*Lg";
        }

        i = 0;
        while(i < count) {
                int n; /* n-data per line */

                sub_xnode = xmlNewNode(NULL, (const xmlChar *)pdesc->name);
                if(!sub_xnode) {
                        return -1;
                }

                if(i) {
                        char str_idx[10];

                        sprintf(str_idx, "%d", i);
                        xmlNewProp(sub_xnode, xStrIdx, (const xmlChar *)str_idx);
                }

                switch(pdesc->size) {
                        case sizeof(float):
                                for(p = str_ctnt, n = 0; (i < count) && (n < 8); i++, n++) {
                                        if(n) {*p++ = DATA_SPACE;}
                                        p += sprintf(p, fmt, FLT_DIG, *(((float *)mem) + i));
                                }
                                break;
                        case sizeof(double):
                                for(p = str_ctnt, n = 0; (i < count) && (n < 4); i++, n++) {
                                        if(n) {*p++ = DATA_SPACE;}
                                        p += sprintf(p, fmt, DBL_DIG, *(((double *)mem) + i));
                                }
                                break;
#ifdef SUPPORT_LONG_DOUBLE
                        case sizeof(long double):
                                for(p = str_ctnt, n = 0; (i < count) && (n < 4); i++, n++) {
                                        if(n) {*p++ = DATA_SPACE;}
                                        fmt = "g";
                                        p += sprintf(p, lfmt, LDBL_DIG, *(((long double *)mem) + i));
                                }
                                break;
#endif
                        default:
                                sprintf(str_ctnt, "unsupported data size(%d)", pdesc->size);
                                i = count; /* stop converter */
                                break;
                }

                dbg(1, "flot2xml: %s", str_ctnt);
                xmlAddChild(xnode, sub_xnode);
                xmlNodeAddContent(sub_xnode, (const xmlChar *)str_ctnt);
        }
        return 0;
}

static int stri2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        int i;
        char *mem = (char *)mem_base + pdesc->offset;
        int count = pdesc->count;
        xmlNode *sub_xnode;

        for(i = 0; i < count; i++, mem += pdesc->size) {
                sub_xnode = xmlNewNode(NULL, (const xmlChar *)pdesc->name);
                if(!sub_xnode) {
                        return -1;
                }

#ifdef MORE_IDX
                if(i) {
                        char str_idx[10];

                        sprintf(str_idx, "%d", i);
                        xmlNewProp(sub_xnode, xStrIdx, (const xmlChar *)str_idx);
                }
#endif

                dbg(1, "stri2xml: %s", mem);
                xmlAddChild(xnode, sub_xnode);
                xmlNodeAddContent(sub_xnode, (const xmlChar *)mem);
        }
        return 0;
}

static int enum2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        int i;
        char *mem = (char *)mem_base + pdesc->offset;
        int count = pdesc->count;
        xmlNode *sub_xnode;
        struct enume *enum_item;

        for(i = 0; i < count; i++, mem += pdesc->size) {
                sub_xnode = xmlNewNode(NULL, (const xmlChar *)pdesc->name);
                if(!sub_xnode) {
                        return -1;
                }

#ifdef MORE_IDX
                if(i) {
                        char str_idx[10];

                        sprintf(str_idx, "%d", i);
                        xmlNewProp(sub_xnode, xStrIdx, (const xmlChar *)str_idx);
                }
#endif

                /* search value in struct enume array */
                for(enum_item = (struct enume *)(pdesc->aux); enum_item->key; enum_item++) {
                        if(*((int *)mem) == enum_item->value) {
                                break;
                        }
                }
                if(!enum_item->key) {
                        enum_item = (struct enume *)(pdesc->aux);
                }

                dbg(1, "enum2xml: %s", enum_item->key);
                xmlAddChild(xnode, sub_xnode);
                xmlNodeAddContent(sub_xnode, (const xmlChar *)(enum_item->key));
        }
        return 0;
}

static int stru2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        int i;
        char *mem = (char *)mem_base + pdesc->offset;
        int count = pdesc->count;
        xmlNode *sub_xnode;

        for(i = 0; i < count; i++, mem += pdesc->size) {
                sub_xnode = xmlNewNode(NULL, (const xmlChar *)pdesc->name);
                if(!sub_xnode) {
                        return -1;
                }

#ifdef MORE_IDX
                if(i) {
                        char str_idx[10];

                        sprintf(str_idx, "%d", i);
                        xmlNewProp(sub_xnode, xStrIdx, (const xmlChar *)str_idx);
                }
#endif

                dbg(1, "stru2xml[%d]:", i);
                param2xml(mem, sub_xnode, (struct pdesc *)(pdesc->aux));
                xmlAddChild(xnode, sub_xnode);
        }
        return 0;
}

static int list2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        int i;
        char *mem = (char *)mem_base + pdesc->offset;
        int count = pdesc->count;
        xmlNode *sub_xnode;

        for(i = 0; i < count; i++, mem += pdesc->size) {
                int sub_i;
                struct znode *list;

                sub_xnode = xmlNewNode(NULL, (const xmlChar *)pdesc->name);
                if(!sub_xnode) {
                        return -1;
                }

#ifdef MORE_IDX
                if(i) {
                        char str_idx[10];

                        sprintf(str_idx, "%d", i);
                        xmlNewProp(sub_xnode, xStrIdx, (const xmlChar *)str_idx);
                }
#endif

                dbg(1, "list2xml[%d]:", i);
                for(list = *(struct znode **)mem, sub_i = 0; list; list = list->next, sub_i++) {
                        xmlNode *sub_sub_xnode;
                        struct tdesc *tdesc = (struct tdesc *)(pdesc->aux);

                        if(!tdesc) {
                                continue;
                        }

                        sub_sub_xnode = xmlNewNode(NULL, (const xmlChar *)pdesc->name);
                        if(!sub_sub_xnode) {
                                return -1;
                        }

#ifdef MORE_IDX
                        if(sub_i) {
                                char str_idx[10];

                                sprintf(str_idx, "%d", sub_i);
                                xmlNewProp(sub_sub_xnode, xStrIdx, (const xmlChar *)str_idx);
                        }
#endif

                        dbg(1, "node[%d]:", sub_i);
                        param2xml(list, sub_sub_xnode, tdesc->pdesc);
                        xmlAddChild(sub_xnode, sub_sub_xnode);
                }
                xmlAddChild(xnode, sub_xnode);
        }
        return 0;
}

static int vlst2xml(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        int i;
        char *mem = (char *)mem_base + pdesc->offset;
        int count = pdesc->count;
        xmlNode *sub_xnode;

        for(i = 0; i < count; i++, mem += pdesc->size) {
                int sub_i;
                struct znode *list;

                sub_xnode = xmlNewNode(NULL, (const xmlChar *)pdesc->name);
                if(!sub_xnode) {
                        return -1;
                }

#ifdef MORE_IDX
                if(i) {
                        char str_idx[10];

                        sprintf(str_idx, "%d", i);
                        xmlNewProp(sub_xnode, xStrIdx, (const xmlChar *)str_idx);
                }
#endif

                dbg(1, "vlst2xml[%d]:", i);
                for(list = *(struct znode **)mem, sub_i = 0; list; list = list->next, sub_i++) {
                        xmlNode *sub_sub_xnode;
                        struct tdesc *tdesc;

                        /* search in tdesc array */
                        if(!(pdesc->aux)) {
                                continue;
                        }
                        for(tdesc = (struct tdesc *)(pdesc->aux); tdesc->name; tdesc++) {
                                if(xmlStrEqual((xmlChar *)(tdesc->name),
                                               (xmlChar *)(list->name))) {
                                        break;
                                }
                        }
                        if(!(tdesc->name)) {
                                tdesc = (struct tdesc *)(pdesc->aux);
                        }

                        /* got tdesc */
                        sub_sub_xnode = xmlNewNode(NULL, (const xmlChar *)pdesc->name);
                        if(!sub_sub_xnode) {
                                return -1;
                        }

#ifdef MORE_IDX
                        if(sub_i) {
                                char str_idx[10];

                                sprintf(str_idx, "%d", sub_i);
                                xmlNewProp(sub_sub_xnode, xStrIdx, (const xmlChar *)str_idx);
                        }
#endif

                        dbg(1, "node[%d]:", sub_i);
                        xmlNewProp(sub_sub_xnode, xStrTyp, (const xmlChar *)list->name);
                        param2xml(list, sub_sub_xnode, tdesc->pdesc);
                        xmlAddChild(sub_xnode, sub_sub_xnode);
                }
                xmlAddChild(xnode, sub_xnode);
        }
        return 0;
}

static int xml2sint(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        xmlChar *idx;
        xmlChar *ctnt;
        char *p;
        char *endp;
        char *mem = (char *)mem_base + pdesc->offset;

        /* adjust pdesc->index, calc mem */
        idx = xmlGetProp(xnode, xStrIdx);
        if(idx) {
                pdesc->index = atoi((char *)idx);
                xmlFree(idx);
        }
        if(pdesc->index >= pdesc->count) {
                dbg(1, "xml2sint: idx(%d) >= count(%d), ignore", pdesc->index, pdesc->count);
                return -1;
        }
        mem += (pdesc->index * pdesc->size);

        /* get sint one by one */
        ctnt = xmlNodeGetContent(xnode);
        if(!ctnt) {
                return -1;
        }
        p = (char *)ctnt;
        switch(pdesc->size) {
                case sizeof(int8_t):
                        {
                                int8_t data;
                                int8_t *cur_mem = (int8_t *)mem;

                                while(1) {
                                        data = (int8_t)strtoimax(p, &endp, 0);
                                        if(p == endp) {break;}
                                        dbg(1, "xml2sint[%d]: %d", pdesc->index, data);
                                        *cur_mem++ = data;
                                        pdesc->index++;
                                        p = endp;
                                }
                        }
                        break;
                case sizeof(int16_t):
                        {
                                int16_t data;
                                int16_t *cur_mem = (int16_t *)mem;

                                while(1) {
                                        data = (int16_t)strtoimax(p, &endp, 0);
                                        if(p == endp) {break;}
                                        dbg(1, "xml2sint[%d]: %d", pdesc->index, data);
                                        *cur_mem++ = data;
                                        pdesc->index++;
                                        p = endp;
                                }
                        }
                        break;
                case sizeof(int32_t):
                        {
                                int32_t data;
                                int32_t *cur_mem = (int32_t *)mem;

                                while(1) {
                                        data = (int32_t)strtoimax(p, &endp, 0);
                                        if(p == endp) {break;}
                                        dbg(1, "xml2sint[%d]: %d", pdesc->index, data);
                                        *cur_mem++ = data;
                                        pdesc->index++;
                                        p = endp;
                                }
                        }
                        break;
                case sizeof(int64_t):
                        {
                                int64_t data;
                                int64_t *cur_mem = (int64_t *)mem;

                                while(1) {
                                        data = (int64_t)strtoimax(p, &endp, 0);
                                        if(p == endp) {break;}
                                        dbg(1, "xml2sint[%d]: %lld", pdesc->index, data);
                                        *cur_mem++ = data;
                                        pdesc->index++;
                                        p = endp;
                                }
                        }
                        break;
                default:
                        dbg(1, "xml2sint: bad data size(%d)", pdesc->size);
                        break;
        }
        xmlFree(ctnt);
        return 0;
}

static int xml2uint(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        xmlChar *idx;
        xmlChar *ctnt;
        char *p;
        char *endp;
        char *mem = (char *)mem_base + pdesc->offset;

        /* adjust pdesc->index, calc mem */
        idx = xmlGetProp(xnode, xStrIdx);
        if(idx) {
                pdesc->index = atoi((char *)idx);
                xmlFree(idx);
        }
        if(pdesc->index >= pdesc->count) {
                dbg(1, "xml2uint: idx(%d) >= count(%d), ignore", pdesc->index, pdesc->count);
                return -1;
        }
        mem += (pdesc->index * pdesc->size);

        /* get uint one by one */
        ctnt = xmlNodeGetContent(xnode);
        if(!ctnt) {
                return -1;
        }
        p = (char *)ctnt;
        switch(pdesc->size) {
                case sizeof(uint8_t):
                        {
                                uint8_t data;
                                uint8_t *cur_mem = (uint8_t *)mem;

                                while(1) {
                                        data = (uint8_t)strtoumax(p, &endp, 0);
                                        if(p == endp) {break;}
                                        dbg(1, "xml2uint[%d]: 0x%X", pdesc->index, data);
                                        *cur_mem++ = data;
                                        pdesc->index++;
                                        p = endp;
                                }
                        }
                        break;
                case sizeof(uint16_t):
                        {
                                uint16_t data;
                                uint16_t *cur_mem = (uint16_t *)mem;

                                while(1) {
                                        data = (uint16_t)strtoumax(p, &endp, 0);
                                        if(p == endp) {break;}
                                        dbg(1, "xml2uint[%d]: 0x%X", pdesc->index, data);
                                        *cur_mem++ = data;
                                        pdesc->index++;
                                        p = endp;
                                }
                        }
                        break;
                case sizeof(uint32_t):
                        {
                                uint32_t data;
                                uint32_t *cur_mem = (uint32_t *)mem;

                                while(1) {
                                        data = (uint32_t)strtoumax(p, &endp, 0);
                                        if(p == endp) {break;}
                                        dbg(1, "xml2uint[%d]: 0x%X", pdesc->index, data);
                                        *cur_mem++ = data;
                                        pdesc->index++;
                                        p = endp;
                                }
                        }
                        break;
                case sizeof(uint64_t):
                        {
                                uint64_t data;
                                uint64_t *cur_mem = (uint64_t *)mem;

                                while(1) {
                                        data = (uint64_t)strtoumax(p, &endp, 0);
                                        if(p == endp) {break;}
                                        dbg(1, "xml2uint[%d]: 0x%llX", pdesc->index, data);
                                        *cur_mem++ = data;
                                        pdesc->index++;
                                        p = endp;
                                }
                        }
                        break;
                default:
                        dbg(1, "xml2uint: bad data size(%d)", pdesc->size);
                        break;
        }
        xmlFree(ctnt);
        return 0;
}

static int xml2flot(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        xmlChar *idx;
        xmlChar *ctnt;
        char *p;
        char *endp;
        char *mem = (char *)mem_base + pdesc->offset;

        /* adjust pdesc->index, calc mem */
        idx = xmlGetProp(xnode, xStrIdx);
        if(idx) {
                pdesc->index = atoi((char *)idx);
                xmlFree(idx);
        }
        if(pdesc->index >= pdesc->count) {
                dbg(1, "xml2flot: idx(%d) >= count(%d), ignore", pdesc->index, pdesc->count);
                return -1;
        }
        mem += (pdesc->index * pdesc->size);

        /* get float one by one */
        ctnt = xmlNodeGetContent(xnode);
        if(!ctnt) {
                return -1;
        }
        p = (char *)ctnt;
        switch(pdesc->size) {
                case sizeof(float):
                        {
                                float data;
                                float *cur_mem = (float *)mem;

                                while(1) {
                                        data = (float)strtod(p, &endp); /* strtof() is seldom */
                                        if(p == endp) {break;}
                                        dbg(1, "xml2flot[%d]: %.*e", pdesc->index, FLT_DIG + 1, data);
                                        *cur_mem++ = data;
                                        pdesc->index++;
                                        p = endp;
                                };
                        }
                        break;
                case sizeof(double):
                        {
                                double data;
                                double *cur_mem = (double *)mem;

                                while(1) {
                                        data = strtod(p, &endp);
                                        if(p == endp) {break;}
                                        dbg(1, "xml2flot[%d]: %.*e", pdesc->index, DBL_DIG + 1, data);
                                        *cur_mem++ = data;
                                        pdesc->index++;
                                        p = endp;
                                };
                        }
                        break;
#ifdef SUPPORT_LONG_DOUBLE
                case sizeof(long double):
                        {
                                long double data;
                                long double *cur_mem = (long double *)mem;

                                while(1) {
                                        data = strtold(p, &endp);
                                        if(p == endp) {break;}
                                        dbg(1, "xml2flot[%d]: %.*Le", pdesc->index, LDBL_DIG + 1, data);
                                        *cur_mem++ = data;
                                        pdesc->index++;
                                        p = endp;
                                };
                        }
                        break;
#endif
                default:
                        dbg(1, "xml2flot: bad data size(%d)", pdesc->size);
                        break;
        }
        xmlFree(ctnt);
        return 0;
}

static int xml2stri(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        xmlChar *idx;
        xmlChar *ctnt;
        int len;
        char *mem = (char *)mem_base + pdesc->offset;

        /* adjust pdesc->index, calc mem */
        idx = xmlGetProp(xnode, xStrIdx);
        if(idx) {
                pdesc->index = atoi((char *)idx);
                xmlFree(idx);
        }
        if(pdesc->index >= pdesc->count) {
                dbg(1, "xml2stri: idx(%d) >= count(%d), ignore", pdesc->index, pdesc->count);
                return -1;
        }
        mem += (pdesc->index * pdesc->size);

        /* get string */
        ctnt = xmlNodeGetContent(xnode);
        if(!ctnt) {
                return -1;
        }
        len = xmlStrlen(ctnt) + 1; /* with '\0' */
        len = ((len > pdesc->size) ? pdesc->size : len);
        memcpy(mem, ctnt, len);
        xmlFree(ctnt);
        dbg(1, "xml2stri[%d]: %s", pdesc->index, (char *)mem);
        pdesc->index++;
        return 0;
}

static int xml2enum(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        xmlChar *idx;
        xmlChar *ctnt;
        char *mem = (char *)mem_base + pdesc->offset;
        struct enume *enum_item;

        /* adjust pdesc->index, calc mem */
        idx = xmlGetProp(xnode, xStrIdx);
        if(idx) {
                pdesc->index = atoi((char *)idx);
                xmlFree(idx);
        }
        if(pdesc->index >= pdesc->count) {
                dbg(1, "xml2enum: idx(%d) >= count(%d), ignore", pdesc->index, pdesc->count);
                return -1;
        }
        mem += (pdesc->index * pdesc->size);

        /* search key in struct enume array */
        ctnt = xmlNodeGetContent(xnode);
        if(!ctnt) {
                return -1;
        }
        for(enum_item = (struct enume *)(pdesc->aux); enum_item->key; enum_item++) {
                if(xmlStrEqual(ctnt, (xmlChar *)(enum_item->key))) {
                        break;
                }
        }
        xmlFree(ctnt);

        /* get enum */
        *(int *)mem = enum_item->value;
        dbg(1, "xml2enum[%d]: %d(\"%s\")", pdesc->index, *(int *)mem, enum_item->key);
        pdesc->index++;
        return 0;
}

static int xml2stru(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        xmlChar *idx;
        char *mem = (char *)mem_base + pdesc->offset;

        /* adjust pdesc->index, calc mem */
        idx = xmlGetProp(xnode, xStrIdx);
        if(idx) {
                pdesc->index = atoi((char *)idx);
                xmlFree(idx);
        }
        if(pdesc->index >= pdesc->count) {
                dbg(1, "xml2stru: idx(%d) >= count(%d), ignore", pdesc->index, pdesc->count);
                return -1;
        }
        mem += (pdesc->index * pdesc->size);

        /* get struct */
        dbg(1, "xml2stru[%d]:", pdesc->index);
        pdesc->index++;
        xml2param(mem, xnode, (struct pdesc *)(pdesc->aux));
        return 0;
}

static int xml2list(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        xmlChar *idx;
        char *mem = (char *)mem_base + pdesc->offset;
        struct znode *list;
        xmlNode *sub_xnode;

        /* adjust pdesc->index, calc mem */
        idx = xmlGetProp(xnode, xStrIdx);
        if(idx) {
                pdesc->index = atoi((char *)idx);
                xmlFree(idx);
        }
        if(pdesc->index >= pdesc->count) {
                dbg(1, "xml2list: idx(%d) >= count(%d), ignore", pdesc->index, pdesc->count);
                return -1;
        }
        mem += (pdesc->index * pdesc->size);

        /* get list */
        dbg(1, "xml2list[%d]:", pdesc->index);
        pdesc->index++;
        if(*(struct znode **)mem) {
                dbg(1, "xml2list: not an empty list");
                return -1;
        };
        for(sub_xnode = xnode->xmlChildrenNode; sub_xnode; sub_xnode = sub_xnode->next) {
                struct tdesc *tdesc;

                if(!xmlStrEqual((xmlChar *)(pdesc->name), sub_xnode->name)) {
                        /* FIXME: why many sub_xnode has name "text"? */
                        continue;
                }
                if(!(pdesc->aux)) {
                        continue;
                }
                tdesc = (struct tdesc *)(pdesc->aux);
                if(!tdesc) {
                        continue;
                }
                if(!(tdesc->name)) {
                        continue;
                }

                /* add list node with tdesc */
                list = (struct znode *)xmlMalloc(tdesc->size);
                if(!list) {
                        dbg(1, "malloc znode failed, ignore");
                        continue;
                }
                zlst_push(mem, list);
                xml2param(list, sub_xnode, tdesc->pdesc);
        }

        return 0;
}

static int xml2vlst(void *mem_base, xmlNode *xnode, struct pdesc *pdesc)
{
        xmlChar *idx;
        char *mem = (char *)mem_base + pdesc->offset;
        struct znode *list;
        xmlNode *sub_xnode;

        /* adjust pdesc->index, calc mem */
        idx = xmlGetProp(xnode, xStrIdx);
        if(idx) {
                pdesc->index = atoi((char *)idx);
                xmlFree(idx);
        }
        if(pdesc->index >= pdesc->count) {
                dbg(1, "xml2vlst: idx(%d) >= count(%d), ignore", pdesc->index, pdesc->count);
                return -1;
        }
        mem += (pdesc->index * pdesc->size);

        /* get vlst */
        dbg(1, "xml2vlst[%d]:", pdesc->index);
        pdesc->index++;
        if(*(struct znode **)mem) {
                dbg(1, "xml2vlst: not an empty list");
                return -1;
        };
        for(sub_xnode = xnode->xmlChildrenNode; sub_xnode; sub_xnode = sub_xnode->next) {
                struct tdesc *tdesc;
                xmlChar *type_name;

                if(!xmlStrEqual((xmlChar *)(pdesc->name), sub_xnode->name)) {
                        /* FIXME: why many sub_xnode has name "text"? */
                        continue;
                }
                if(!(pdesc->aux)) {
                        continue;
                }

                /* search in tdesc array */
                type_name = xmlGetProp(sub_xnode, xStrTyp);
                if(!type_name) {
                        continue;
                }
                for(tdesc = (struct tdesc *)(pdesc->aux); tdesc->name; tdesc++) {
                        if(xmlStrEqual((xmlChar *)(tdesc->name), type_name)) {
                                break;
                        }
                }
                xmlFree(type_name);
                if(!(tdesc->name)) {
                        continue;
                }

                /* add list node with tdesc */
                list = (struct znode *)xmlMalloc(tdesc->size);
                if(!list) {
                        dbg(1, "malloc znode failed, ignore");
                        continue;
                }
                zlst_set_name(list, tdesc->name);
                zlst_push(mem, list);
                xml2param(list, sub_xnode, tdesc->pdesc);
        }

        return 0;
}

/* low level functions */

#if 0
static int ftostr(double data, char *str)
{
        int i, j, k;
        long temp, tempoten;
        char intpart[20], dotpart[20];

        if(data < 0) {
                str[0] = '-';
                data = -data;
        }
        else {
                str[0] = '+';
        }

        temp = (long)data;
        i = 0;
        tempoten = temp / 10;
        while(tempoten != 0) {
                intpart[i] = temp - 10 * tempoten + 48;
                temp = tempoten;
                tempoten = temp / 10;
                i++;
        }
        intpart[i] = temp + 48;


        data = data - (long)data;
        for(j = 0; j < 12; j++) {
                dotpart[j] = (int)(data * 10) + 48;
                data = data * 10.0;
                data = data - (long)data;
        }

        for(k = 1; k <= i + 1; k++) str[k] = intpart[i + 1 - k];
        str[i + 2] = '.';
        for(k = i + 3; k < i + j + 3; k++) str[k] = dotpart[k - i - 3];
        str[i + j + 3] = 0x0D;

        return i+j+4;
}
#endif

static intmax_t strtoimax(const char *nptr, char **endptr, int base)
{
        const char *s;
        char c;
        int neg;
        intmax_t acc;
        int any;
        uintmax_t cutoff;
        int cutlim;

        acc = 0;
        any = 0;

        /* remove white space at head */
        s = nptr;
        do {
                c = *s++;
                if(DATA_SPACE == c) {c = ' ';}
        }while(isspace((unsigned char)c));

        /* deal with '+' or '-' */
        if(c == '-') {
                neg = 1;
                c = *s++;
        }
        else{
                neg = 0;
                if(c == '+') {c = *s++;}
        }

        /* base */
        if((base == 0 || base == 16) &&
           c == '0' &&
           (*s == 'x' || *s == 'X') &&
           ((s[1] >= '0' && s[1] <= '9') ||
            (s[1] >= 'A' && s[1] <= 'F') ||
            (s[1] >= 'a' && s[1] <= 'f'))) {
                c = s[1];
                s += 2;
                base = 16;
        }
        if(base == 0) {
                base = ((c == '0') ? 8 : 10);
        }
        if(base < 2 || base > 36) {
                goto noconv; /* bad base */
        }

        /* use cutoff and cutlim to judge overflow */
        cutoff = (neg ? uintmax_imin : uintmax_imax);
        cutlim = cutoff % base;
        cutoff /= base;

        for(; ; c = *s++) {
                /* get one number */
                if     (c >= '0' && c <= '9') {c -= '0';}
                else if(c >= 'A' && c <= 'Z') {c -= ('A' - 10);}
                else if(c >= 'a' && c <= 'z') {c -= ('a' - 10);}
                else                          {break;}

                if(c >= base) {
                        break; /* bad number, stop */
                }

                if(any < 0 ||
                   acc > cutoff ||
                   (acc == cutoff && c > cutlim)) {
                        any = -1; /* overflow */
                }
                else {
                        any = 1;
                        acc *= base;
                        acc += c;
                }
        }

        if(any < 0) {
                /* overflow */
                acc = intmax_max;
                acc += (neg ? 1 : 0); /* intmax_min */
        }
        else if(!any) {
noconv:
                any = 0; /* invalid data */
        }
        else if(neg) {
                acc = -acc;
        }

        if (endptr != NULL) {
                *endptr = (char *)(any ? s - 1 : nptr);
        }

        return (acc);
}

static uintmax_t strtoumax(const char *nptr, char **endptr, int base)
{
        const char *s;
        char c;
        uintmax_t acc;
        int any;
        uintmax_t cutoff;
        int cutlim;

        acc = 0;
        any = 0;

        /* remove white space at head */
        s = nptr;
        do {
                c = *s++;
                if(DATA_SPACE == c) {c = ' ';}
        }while(isspace((unsigned char)c));

        /* deal with '+' or '-' */
        if(c == '-') {
                goto noconv; /* bad base */
        }
        else if(c == '+') {
                c = *s++;
        }

        /* base */
        if((base == 0 || base == 16) &&
           c == '0' &&
           (*s == 'x' || *s == 'X') &&
           ((s[1] >= '0' && s[1] <= '9') ||
            (s[1] >= 'A' && s[1] <= 'F') ||
            (s[1] >= 'a' && s[1] <= 'f'))) {
                c = s[1];
                s += 2;
                base = 16;
        }
        if(base == 0) {
                base = ((c == '0') ? 8 : 10);
        }
        if(base < 2 || base > 36) {
                goto noconv; /* bad base */
        }

        /* use cutoff and cutlim to judge overflow */
        cutoff = uintmax_umax;
        cutlim = cutoff % base;
        cutoff /= base;

        for(; ; c = *s++) {
                /* get one number */
                if     (c >= '0' && c <= '9') {c -= '0';}
                else if(c >= 'A' && c <= 'Z') {c -= ('A' - 10);}
                else if(c >= 'a' && c <= 'z') {c -= ('a' - 10);}
                else                          {break;}

                if(c >= base) {
                        break; /* bad number, stop */
                }

                if(any < 0 ||
                   acc > cutoff ||
                   (acc == cutoff && c > cutlim)) {
                        any = -1; /* overflow */
                }
                else {
                        any = 1;
                        acc *= base;
                        acc += c;
                }
        }

        if(any < 0) {
                acc = uintmax_umax; /* overflow */
        }
        else if(!any) {
noconv:
                any = 0; /* invalid data */
        }

        if (endptr != NULL) {
                *endptr = (char *)(any ? s - 1 : nptr);
        }

        return (acc);
}
