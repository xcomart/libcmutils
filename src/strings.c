
#include "functions.h"

#include <ctype.h>
#include <iconv.h>

CMUTIL_LogDefine("cmutil.string")

//*****************************************************************************
// CMUTIL_String implementation
//*****************************************************************************

typedef struct CMUTIL_String_Internal CMUTIL_String_Internal;
struct CMUTIL_String_Internal {
    CMUTIL_String	base;
    char			*data;
    int				capacity;
    int				size;
    CMUTIL_Mem_st	*memst;
};

CMUTIL_STATIC void CMUTIL_StringCheckSize(
        CMUTIL_String_Internal *str, int insize)
{
    int reqsz = (str->size + insize + 1);
    if (str->capacity < reqsz) {
        int newcap = str->capacity * 2;
        while (newcap < reqsz) newcap *= 2;
        str->data = str->memst->Realloc(str->data, newcap);
        str->capacity = newcap;
    }
}

CMUTIL_STATIC int CMUTIL_StringAddString(CMUTIL_String *string,
        const char *tobeadded)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    register const char *p = tobeadded;

    while (*p) {
        CMUTIL_StringCheckSize(istr, 1);
        istr->data[istr->size++] = *p++;
    }
    istr->data[istr->size] = 0x0;
    return istr->size;
}

CMUTIL_STATIC int CMUTIL_StringAddNString(CMUTIL_String *string,
        const char *tobeadded, int size)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    CMUTIL_StringCheckSize(istr, size);
    memcpy(istr->data + istr->size, tobeadded, size);
    istr->size += size;
    istr->data[istr->size] = 0x0;
    return istr->size;
}

CMUTIL_STATIC int CMUTIL_StringAddChar(CMUTIL_String *string, char tobeadded)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;

    CMUTIL_StringCheckSize(istr, 1);
    istr->data[istr->size++] = tobeadded;

    istr->data[istr->size] = 0x0;
    return istr->size;
}

CMUTIL_STATIC int CMUTIL_StringAddPrint(
        CMUTIL_String *string, const char *fmt, ...)
{
    va_list args;
    int ir;
    va_start(args, fmt);
    ir = CMUTIL_CALL(string, AddVPrint, fmt, args);
    va_end(args);
    return ir;
}

CMUTIL_STATIC int CMUTIL_StringAddVPrint(
        CMUTIL_String *string, const char *fmt, va_list args)
{
    char buf[4096];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    return CMUTIL_CALL(string, AddNString, buf, len);
}

CMUTIL_STATIC int CMUTIL_StringAddAnother(
        CMUTIL_String *string, CMUTIL_String *tobeadded)
{
    const char *str = CMUTIL_CALL(tobeadded, GetCString);
    int len = CMUTIL_CALL(tobeadded, GetSize);
    return CMUTIL_CALL(string, AddNString, str, len);
}

CMUTIL_STATIC int CMUTIL_StringInsertString(CMUTIL_String *string,
        const char *tobeadded, int at)
{
    int size = (int)strlen((const char*)tobeadded);
    return CMUTIL_CALL(string, InsertNString, tobeadded, at, size);
}

CMUTIL_STATIC int CMUTIL_StringInsertNString(CMUTIL_String *string,
        const char *tobeadded, int at, int size)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    CMUTIL_StringCheckSize(istr, size);
    memmove(istr->data+at+size, istr->data+at, istr->size-at);
    memcpy(istr->data+at, tobeadded, size);
    istr->size += size;
    return istr->size;
}

CMUTIL_STATIC int CMUTIL_StringInsertPrint(
        CMUTIL_String *string, int idx, const char *fmt, ...)
{
    va_list args;
    int ir;
    va_start(args, fmt);
    ir = CMUTIL_CALL(string, InsertVPrint, idx, fmt, args);
    va_end(args);
    return ir;
}

CMUTIL_STATIC int CMUTIL_StringInsertVPrint(
        CMUTIL_String *string, int idx, const char *fmt, va_list args)
{
    char buf[4096];
    int len, ir;
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    ir = CMUTIL_CALL(string, InsertNString, buf, idx, len);
    return ir;
}

CMUTIL_STATIC int CMUTIL_StringInsertAnother(
        CMUTIL_String *string, int idx, CMUTIL_String *tobeadded)
{
    const char *str = CMUTIL_CALL(tobeadded, GetCString);
    int len = CMUTIL_CALL(tobeadded, GetSize);
    return CMUTIL_CALL(string, InsertNString, str, idx, len);
}

CMUTIL_STATIC void CMUTIL_StringCutTailOff(
        CMUTIL_String *string, int length)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    if (istr->size > length)
        istr->size -= length;
    else
        istr->size = 0;
    *(istr->data + istr->size) = 0x0;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringSubstring(
    CMUTIL_String *string, int offset, int length)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    CMUTIL_String *res = NULL;
    if (offset < istr->size) {
        int len = length;
        if ((offset + length) > istr->size)
            len = istr->size - offset;
        res = CMUTIL_StringCreateInternal(istr->memst, len, NULL);
        CMUTIL_CALL(res, AddNString, istr->data + offset, len);
    }
    return res;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringToLower(
    CMUTIL_String *string)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    CMUTIL_String_Internal *res =
            (CMUTIL_String_Internal*)CMUTIL_StringCreateInternal(
                istr->memst, istr->size, NULL);
    register char *p = res->data, *q = istr->data;
    while (*q)
        *p++ = tolower(*q++);
    *p = 0x0;
    res->size = istr->size;
    return (CMUTIL_String*)res;
}

CMUTIL_STATIC void CMUTIL_StringSelfToUpper(CMUTIL_String *str)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)str;
    register char *p = istr->data;
    while (*p) {
        *p = toupper(*p);
        p++;
    }
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringToUpper(
    CMUTIL_String *string)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    CMUTIL_String_Internal *res =
        (CMUTIL_String_Internal*)CMUTIL_StringCreateInternal(
                istr->memst, istr->size, NULL);
    register char *p = res->data, *q = istr->data;
    while (*q)
        *p++ = toupper(*q++);
    *p = 0x0;
    res->size = istr->size;
    return (CMUTIL_String*)res;
}

CMUTIL_STATIC void CMUTIL_StringSelfToLower(CMUTIL_String *str)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)str;
    register char *p = istr->data;
    while (*p) {
        *p = tolower(*p);
        p++;
    }
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringReplace(
        CMUTIL_String *string, const char *needle, const char *alter)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    CMUTIL_String *res;
    if (!needle || !alter) {
        CMLogErrorS("invalid argument.");
        return NULL;
    }
    res = CMUTIL_StringCreateInternal(istr->memst, istr->capacity, NULL);
    if (res) {
        const char *prv, *cur;
        int nlen = (int)strlen(needle);
        prv = istr->data;
        cur = strstr(prv, needle);
        while (cur) {
            if (cur - prv)
                CMUTIL_CALL(res, AddNString, prv, (int)(cur-prv));
            CMUTIL_CALL(res, AddString, alter);
            cur += nlen; prv = cur;
            cur = strstr(prv, needle);
        }
        CMUTIL_CALL(res, AddString, prv);
    } else {
        CMLogErrorS("not enough memory.");
    }
    return res;
}

CMUTIL_STATIC int CMUTIL_StringGetSize(CMUTIL_String *string)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    return istr->size;
}

CMUTIL_STATIC const char *CMUTIL_StringGetCString(CMUTIL_String *string)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    return istr->data;
}

CMUTIL_STATIC void CMUTIL_StringClear(CMUTIL_String *string)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    *(istr->data) = 0x0;
    istr->size = 0;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringClone(CMUTIL_String *string)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    int size = CMUTIL_CALL(string, GetSize);
    const char *str = CMUTIL_CALL(string, GetCString);
    CMUTIL_String *res =
            CMUTIL_StringCreateInternal(istr->memst, size, NULL);
    CMUTIL_CALL(res, AddNString, str, size);
    return res;
}

CMUTIL_STATIC void CMUTIL_StringDestroy(CMUTIL_String *string)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    if (istr) {
        if (istr->data)
            istr->memst->Free(istr->data);
        istr->memst->Free(istr);
    }
}

static CMUTIL_String g_cmutil_string = {
    CMUTIL_StringAddString,
    CMUTIL_StringAddNString,
    CMUTIL_StringAddChar,
    CMUTIL_StringAddPrint,
    CMUTIL_StringAddVPrint,
    CMUTIL_StringAddAnother,
    CMUTIL_StringInsertString,
    CMUTIL_StringInsertNString,
    CMUTIL_StringInsertPrint,
    CMUTIL_StringInsertVPrint,
    CMUTIL_StringInsertAnother,
    CMUTIL_StringCutTailOff,
    CMUTIL_StringSubstring,
    CMUTIL_StringToLower,
    CMUTIL_StringSelfToLower,
    CMUTIL_StringToUpper,
    CMUTIL_StringSelfToUpper,
    CMUTIL_StringReplace,
    CMUTIL_StringGetSize,
    CMUTIL_StringGetCString,
    CMUTIL_StringClear,
    CMUTIL_StringClone,
    CMUTIL_StringDestroy
};

CMUTIL_String *CMUTIL_StringCreateInternal(
        CMUTIL_Mem_st *memst,
        int initcapacity,
        const char *initcontent)
{
    CMUTIL_String_Internal *istr = memst->Alloc(sizeof(CMUTIL_String_Internal));
    memset(istr, 0x0, sizeof(CMUTIL_String_Internal));
    memcpy(istr, &g_cmutil_string, sizeof(CMUTIL_String));

    istr->data = memst->Alloc(initcapacity+1);
    istr->capacity = initcapacity+1;
    *(istr->data) = 0x0;
    istr->memst = memst;

    if (initcontent)
        CMUTIL_CALL((CMUTIL_String*)istr, AddString, initcontent);
    return (CMUTIL_String*)istr;
}

CMUTIL_String *CMUTIL_StringCreateEx(
        int initcapacity,
        const char *initcontent)
{
    return CMUTIL_StringCreateInternal(
                __CMUTIL_Mem, initcapacity, initcontent);
}




//*****************************************************************************
// CMUTIL_StringArray implementation
//*****************************************************************************

typedef struct CMUTIL_StringArray_Internal {
    CMUTIL_StringArray	base;
    CMUTIL_Array		*array;
    CMUTIL_Mem_st		*memst;
} CMUTIL_StringArray_Internal;

CMUTIL_STATIC void CMUTIL_StringArrayAdd(
        CMUTIL_StringArray *array, CMUTIL_String *string)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    CMUTIL_CALL(iarray->array, Add, string);
}

CMUTIL_STATIC void CMUTIL_StringArrayAddCString(
        CMUTIL_StringArray *array, const char *string)
{
    CMUTIL_String *data = CMUTIL_StringCreateEx(64, string);
    CMUTIL_CALL(array, Add, data);
}

CMUTIL_STATIC void CMUTIL_StringArrayInsertAt(
        CMUTIL_StringArray *array, CMUTIL_String *string, int index)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    CMUTIL_CALL(iarray->array, InsertAt, string, index);
}

CMUTIL_STATIC void CMUTIL_StringArrayInsertAtCString(
        CMUTIL_StringArray *array, const char *string, int index)
{
    CMUTIL_String *data = CMUTIL_StringCreateEx(64, string);
    CMUTIL_CALL(array, InsertAt, data, index);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringArrayRemoveAt(
        CMUTIL_StringArray *array, int index)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    return (CMUTIL_String*)CMUTIL_CALL(iarray->array, RemoveAt, index);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringArraySetAt(
        CMUTIL_StringArray *array, CMUTIL_String *string, int index)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    return CMUTIL_CALL(iarray->array, SetAt, string, index);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringArraySetAtCString(
        CMUTIL_StringArray *array, const char *string, int index)
{
    CMUTIL_String *data = CMUTIL_StringCreateEx(64, string);
    return CMUTIL_CALL(array, SetAt, data, index);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringArrayGetAt(
        CMUTIL_StringArray *array, int index)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    return (CMUTIL_String*)CMUTIL_CALL(iarray->array, GetAt, index);
}

CMUTIL_STATIC const char *CMUTIL_StringArrayGetCString(
        CMUTIL_StringArray *array, int index)
{
    CMUTIL_String *v = CMUTIL_CALL(array, GetAt, index);
    return CMUTIL_CALL(v, GetCString);
}

CMUTIL_STATIC int CMUTIL_StringArrayGetSize(
        CMUTIL_StringArray *array)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    return CMUTIL_CALL(iarray->array, GetSize);
}

CMUTIL_STATIC CMUTIL_Iterator *CMUTIL_StringArrayIterator(
        CMUTIL_StringArray *array)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    return CMUTIL_CALL(iarray->array, Iterator);
}

CMUTIL_STATIC void CMUTIL_StringArrayDestroy(
        CMUTIL_StringArray *array)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    CMUTIL_Iterator *iter = CMUTIL_CALL(array, Iterator);
    while (CMUTIL_CALL(iter, HasNext)) {
        CMUTIL_String *item = (CMUTIL_String*)CMUTIL_CALL(iter, Next);
        CMUTIL_CALL(item, Destroy);
    }
    CMUTIL_CALL(iter, Destroy);
    CMUTIL_CALL(iarray->array, Destroy);
    iarray->memst->Free(iarray);
}

static CMUTIL_StringArray g_cmutil_stringarray = {
    CMUTIL_StringArrayAdd,
    CMUTIL_StringArrayAddCString,
    CMUTIL_StringArrayInsertAt,
    CMUTIL_StringArrayInsertAtCString,
    CMUTIL_StringArrayRemoveAt,
    CMUTIL_StringArraySetAt,
    CMUTIL_StringArraySetAtCString,
    CMUTIL_StringArrayGetAt,
    CMUTIL_StringArrayGetCString,
    CMUTIL_StringArrayGetSize,
    CMUTIL_StringArrayIterator,
    CMUTIL_StringArrayDestroy
};

CMUTIL_StringArray *CMUTIL_StringArrayCreateInternal(
        CMUTIL_Mem_st *memst, int initcapacity)
{
    CMUTIL_StringArray_Internal *res =
            memst->Alloc(sizeof(CMUTIL_StringArray_Internal));
    memset(res, 0x0, sizeof(CMUTIL_StringArray_Internal));
    memcpy(res, &g_cmutil_stringarray, sizeof(CMUTIL_StringArray));
    res->memst=  memst;
    res->array = CMUTIL_ArrayCreateInternal(memst, initcapacity, NULL, NULL);
    return (CMUTIL_StringArray*)res;
}

CMUTIL_StringArray *CMUTIL_StringArrayCreateEx(int initcapacity)
{
    return CMUTIL_StringArrayCreateInternal(__CMUTIL_Mem, initcapacity);
}



//*****************************************************************************
// CMUTIL_CSConv implementation
//*****************************************************************************

typedef struct CMUTIL_CSConv_Internal {
    CMUTIL_CSConv		base;
    char				*frcs;
    char				*tocs;
    CMUTIL_Mem_st		*memst;
} CMUTIL_CSConv_Internal;

CMUTIL_STATIC CMUTIL_String *CMUTIL_CSConvConv(
        CMUTIL_Mem_st *memst, CMUTIL_String *instr,
        const char *frcs, const char *tocs)
{
    CMUTIL_String *res = NULL;

    if (instr) {
        iconv_t icv;
        size_t insz, outsz, osz, ressz;
        char *obuf, *osave, *src;


        outsz = ressz = 0;
        obuf = osave = NULL;
        src = (char*)CMUTIL_CALL(instr, GetCString);
        insz = CMUTIL_CALL(instr, GetSize);

        icv = iconv_open(tocs, frcs);

        osz = outsz = insz * 2;
        if (outsz > 0) {
            res = CMUTIL_StringCreateInternal(memst, (int)outsz, NULL);
            obuf = (char*)CMUTIL_CALL(res, GetCString);

#if defined(SUNOS)
            ressz = iconv(icv, (const char**)&src, &insz, &obuf, &outsz);
#else
            ressz = iconv(icv, &src, &insz, &obuf, &outsz);
#endif
            if (ressz == (size_t) -1) {
                CMUTIL_CALL(res, Destroy);
                res = NULL;
            } else {
                CMUTIL_String_Internal *pres = (CMUTIL_String_Internal*)res;
                if (ressz == 0) {
                    pres->size = (int)(osz - outsz);
                } else {
                    pres->size = (int)ressz;
                }
                *(pres->data + pres->size) = 0x0;
            }
        }
    }
    return res;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_CSConvForward(
        CMUTIL_CSConv *conv, CMUTIL_String *instr)
{
    CMUTIL_CSConv_Internal *iconv = (CMUTIL_CSConv_Internal*)conv;
    return CMUTIL_CSConvConv(iconv->memst, instr, iconv->frcs, iconv->tocs);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_CSConvBackward(
        CMUTIL_CSConv *conv, CMUTIL_String *instr)
{
    CMUTIL_CSConv_Internal *iconv = (CMUTIL_CSConv_Internal*)conv;
    return CMUTIL_CSConvConv(iconv->memst, instr, iconv->tocs, iconv->frcs);
}

CMUTIL_STATIC void CMUTIL_CSConvDestroy(CMUTIL_CSConv *conv)
{
    if (conv) {
        CMUTIL_CSConv_Internal *iconv = (CMUTIL_CSConv_Internal*)conv;
        if (iconv->frcs)
            iconv->memst->Free(iconv->frcs);
        if (iconv->tocs)
            iconv->memst->Free(iconv->tocs);
        iconv->memst->Free(iconv);
    }
}

static const CMUTIL_CSConv g_cmutil_csconv = {
        CMUTIL_CSConvForward,
        CMUTIL_CSConvBackward,
        CMUTIL_CSConvDestroy
};

CMUTIL_CSConv *CMUTIL_CSConvCreateInternal(
        CMUTIL_Mem_st *memst, const char *fromcs, const char *tocs)
{
    CMUTIL_CSConv_Internal *res = memst->Alloc(sizeof(CMUTIL_CSConv_Internal));
    memset(res, 0x0, sizeof(CMUTIL_CSConv_Internal));
    memcpy(res, &g_cmutil_csconv, sizeof(CMUTIL_CSConv_Internal));
    res->frcs = memst->Strdup(fromcs);
    res->tocs = memst->Strdup(tocs);
    res->memst = memst;
    return (CMUTIL_CSConv*)res;
}

CMUTIL_CSConv *CMUTIL_CSConvCreate(const char *fromcs, const char *tocs)
{
    return CMUTIL_CSConvCreateInternal(__CMUTIL_Mem, fromcs, tocs);
}


///////////////////////////////////////////////////////////////////////////////
// Stand-alone functions.

char* CMUTIL_StrRTrim(char *inp)
{
    int len;
    register char *p = NULL;
    if (!inp)
        return NULL;
    len = (int)strlen(inp);
    p = inp + len - 1;
    while (strchr(SPACES, *p) && (p > inp))
        p--;
    *(p+1) = 0x0;
    return inp;
}

char* CMUTIL_StrLTrim(char *inp)
{
    register char *p, *q;
    if (!inp)
        return NULL;
    p = q = inp;
    while (strchr(SPACES, *q) && *q)
        q++;
    while (*q)
        *p++ = *q++;
    *p = 0x0;
    return inp;
}

char* CMUTIL_StrTrim(char *inp)
{
    /* do rtrim first for performance */
    CMUTIL_StrRTrim(inp);
    return CMUTIL_StrLTrim(inp);
}

CMUTIL_StringArray *CMUTIL_StringSplitInternal(
        CMUTIL_Mem_st *memst, const char *haystack, const char *needle)
{
    char buf[4096];
    CMUTIL_StringArray *res = NULL;
    if (haystack && needle) {
        register const char *p, *q, *r;
        int needlelen, nfail = 0;
        res = CMUTIL_StringArrayCreateInternal(memst, 5);
        if (res) {
            CMUTIL_String *str = NULL;
            needlelen = (int)strlen(needle);
            p = haystack;
            /* find needle */
            q = strstr(p, needle);
            while (q) {
                strncpy(buf, p, q-p);
                *(buf+(q-p)) = 0x0;
                r = CMUTIL_StrTrim(buf);
                str = CMUTIL_StringCreateInternal(memst, (int)strlen(r), r);
                CMUTIL_CALL(res, Add, str);
                /* skip needle */
                p = q + needlelen;
                /* find next needle */
                q = strstr(p, needle);
            }
            if (!nfail) {
                /* add last one */
                strcpy(buf, p);
                r = CMUTIL_StrTrim(buf);
                str = CMUTIL_StringCreateInternal(memst, (int)strlen(r), r);
                CMUTIL_CALL(res, Add, str);
            } else {
                CMUTIL_CALL(res, Destroy);
                res = NULL;
            }
        }
    }
    return res;
}

CMUTIL_StringArray *CMUTIL_StringSplit(const char *haystack, const char *needle)
{
    return CMUTIL_StringSplitInternal(__CMUTIL_Mem, haystack, needle);
}

const char *CMUTIL_StrNextToken(
    char *dest, size_t buflen, const char *src, const char *delim)
{
    register const char *p = src;
    register char *q = dest, *l = dest + buflen;
    while (*p && !strchr(delim, *p) && q < l) *q++ = *p++;
    *q = 0x0;
    return p;
}

char *CMUTIL_StrSkipSpaces(char *line, const char *spaces)
{
    register char *p = line;
    while (*p && strchr(spaces, *p)) p++;
    return p;
}

CMUTIL_STATIC int CMUTIL_StringHexChar(char inp)
{
    inp = toupper((int)inp);
    if (inp >= 'A' && inp <= 'F')
        return 10 + (inp - 'A');
    else if (inp >= '0' && inp <= '9')
        return inp - '0';
    else
        return -1;
}

int CMUTIL_StringHexToBytes(char *dest, const char *src, int len)
{
    register char *p;
    register const char* hexstr = src;
    int isodd = len % 2;
    p = dest;

    /* every 2 hex characters are converted to one byte */
    while (len > 0) {
        if (isodd) {
            // pad first byte to zero when given source has odd number.
            /* upper 4 bits */
            *p = 0;
            /* lower 4 bits */
            *p |= (CMUTIL_StringHexChar(*hexstr++) & 0xF);
            len -= 1; p++;
            isodd = 0;
        } else {
            if (*hexstr) {
                /* upper 4 bits */
                *p = ((CMUTIL_StringHexChar(*hexstr++) & 0xF) << 4);

                /* lower 4 bits */
                *p |= (CMUTIL_StringHexChar(*hexstr++) & 0xF);
            } else {
                *p = (char)0xFF;	// padding with 0xFF
            }
            len -= 2; p++;
        }
    }
    /* return number of result bytes */
    return (int)(p - dest);
}

/* end of file */
