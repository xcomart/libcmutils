/*
MIT License

Copyright (c) 2020 Dennis Soungjin Park<xcomart@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#include "functions.h"

#include <ctype.h>

#if defined(_MSC_VER)
static struct CMUTIL_CSPair {
    char    *csname;
    int     cpno;
} g_msvc_cspiars[] = {
     {"IBM037",37}  // IBM EBCDIC US-Canada
    ,{"IBM437",437}  // OEM United States
    ,{"IBM500",500}  // IBM EBCDIC International
    ,{"ASMO-708",708}  // Arabic (ASMO 708)
    ,{"DOS-720",720}  // Arabic (Transparent ASMO); Arabic (DOS)
    ,{"IBM737",737}  // OEM Greek (formerly 437G); Greek (DOS)
    ,{"IBM775",775}  // OEM Baltic; Baltic (DOS)
    ,{"IBM850",850}  // OEM Multilingual Latin 1; Western European (DOS)
    ,{"IBM852",852}  // OEM Latin 2; Central European (DOS)
    ,{"IBM855",855}  // OEM Cyrillic (primarily Russian)
    ,{"IBM857",857}  // OEM Turkish; Turkish (DOS)
    ,{"IBM00858",858}  // OEM Multilingual Latin 1 + Euro symbol
    ,{"IBM860",860}  // OEM Portuguese; Portuguese (DOS)
    ,{"IBM861",861}  // OEM Icelandic; Icelandic (DOS)
    ,{"DOS-862",862}  // OEM Hebrew; Hebrew (DOS)
    ,{"IBM863",863}  // OEM French Canadian; French Canadian (DOS)
    ,{"IBM864",864}  // OEM Arabic; Arabic (864)
    ,{"IBM865",865}  // OEM Nordic; Nordic (DOS)
    ,{"CP866",866}  // OEM Russian; Cyrillic (DOS)
    ,{"IBM869",869}  // OEM Modern Greek; Greek, Modern (DOS)
    ,{"IBM870",870}  // IBM EBCDIC Multilingual/ROECE (Latin 2); IBM EBCDIC Multilingual Latin 2
    ,{"WINDOWS-874",874}  // ANSI/OEM Thai (ISO 8859-11); Thai (Windows)
    ,{"CP875",875}  // IBM EBCDIC Greek Modern
    ,{"SHIFT_JIS",932}  // ANSI/OEM Japanese; Japanese (Shift-JIS)
    ,{"GB2312",936}  // ANSI/OEM Simplified Chinese (PRC, Singapore); Chinese Simplified (GB2312)
    ,{"KS_C_5601-1987",949}  // ANSI/OEM Korean (Unified Hangul Code)
    ,{"CP949",949}  // ANSI/OEM Korean (Unified Hangul Code)
    ,{"BIG5",950}  // ANSI/OEM Traditional Chinese (Taiwan; Hong Kong SAR, PRC); Chinese Traditional (Big5)
    ,{"IBM1026",1026}  // IBM EBCDIC Turkish (Latin 5)
    ,{"IBM01047",1047}  // IBM EBCDIC Latin 1/Open System
    ,{"IBM01140",1140}  // IBM EBCDIC US-Canada (037 + Euro symbol); IBM EBCDIC (US-Canada-Euro)
    ,{"IBM01141",1141}  // IBM EBCDIC Germany (20273 + Euro symbol); IBM EBCDIC (Germany-Euro)
    ,{"IBM01142",1142}  // IBM EBCDIC Denmark-Norway (20277 + Euro symbol); IBM EBCDIC (Denmark-Norway-Euro)
    ,{"IBM01143",1143}  // IBM EBCDIC Finland-Sweden (20278 + Euro symbol); IBM EBCDIC (Finland-Sweden-Euro)
    ,{"IBM01144",1144}  // IBM EBCDIC Italy (20280 + Euro symbol); IBM EBCDIC (Italy-Euro)
    ,{"IBM01145",1145}  // IBM EBCDIC Latin America-Spain (20284 + Euro symbol); IBM EBCDIC (Spain-Euro)
    ,{"IBM01146",1146}  // IBM EBCDIC United Kingdom (20285 + Euro symbol); IBM EBCDIC (UK-Euro)
    ,{"IBM01147",1147}  // IBM EBCDIC France (20297 + Euro symbol); IBM EBCDIC (France-Euro)
    ,{"IBM01148",1148}  // IBM EBCDIC International (500 + Euro symbol); IBM EBCDIC (International-Euro)
    ,{"IBM01149",1149}  // IBM EBCDIC Icelandic (20871 + Euro symbol); IBM EBCDIC (Icelandic-Euro)
    ,{"UTF-16",1200}  // Unicode UTF-16, little endian byte order (BMP of ISO 10646); available only to managed applications
    ,{"UNICODEFFFE",1201}  // Unicode UTF-16, big endian byte order; available only to managed applications
    ,{"WINDOWS-1250",1250}  // ANSI Central European; Central European (Windows)
    ,{"WINDOWS-1251",1251}  // ANSI Cyrillic; Cyrillic (Windows)
    ,{"WINDOWS-1252",1252}  // ANSI Latin 1; Western European (Windows)
    ,{"WINDOWS-1253",1253}  // ANSI Greek; Greek (Windows)
    ,{"WINDOWS-1254",1254}  // ANSI Turkish; Turkish (Windows)
    ,{"WINDOWS-1255",1255}  // ANSI Hebrew; Hebrew (Windows)
    ,{"WINDOWS-1256",1256}  // ANSI Arabic; Arabic (Windows)
    ,{"WINDOWS-1257",1257}  // ANSI Baltic; Baltic (Windows)
    ,{"WINDOWS-1258",1258}  // ANSI/OEM Vietnamese; Vietnamese (Windows)
    ,{"JOHAB",1361}  // Korean (Johab)
    ,{"MACINTOSH",10000}  // MAC Roman; Western European (Mac)
    ,{"X-MAC-JAPANESE",10001}  // Japanese (Mac)
    ,{"X-MAC-CHINESETRAD",10002}  // MAC Traditional Chinese (Big5); Chinese Traditional (Mac)
    ,{"X-MAC-KOREAN",10003}  // Korean (Mac)
    ,{"X-MAC-ARABIC",10004}  // Arabic (Mac)
    ,{"X-MAC-HEBREW",10005}  // Hebrew (Mac)
    ,{"X-MAC-GREEK",10006}  // Greek (Mac)
    ,{"X-MAC-CYRILLIC",10007}  // Cyrillic (Mac)
    ,{"X-MAC-CHINESESIMP",10008}  // MAC Simplified Chinese (GB 2312); Chinese Simplified (Mac)
    ,{"X-MAC-ROMANIAN",10010}  // Romanian (Mac)
    ,{"X-MAC-UKRAINIAN",10017}  // Ukrainian (Mac)
    ,{"X-MAC-THAI",10021}  // Thai (Mac)
    ,{"X-MAC-CE",10029}  // MAC Latin 2; Central European (Mac)
    ,{"X-MAC-ICELANDIC",10079}  // Icelandic (Mac)
    ,{"X-MAC-TURKISH",10081}  // Turkish (Mac)
    ,{"X-MAC-CROATIAN",10082}  // Croatian (Mac)
    ,{"UTF-32",12000}  // Unicode UTF-32, little endian byte order; available only to managed applications
    ,{"UTF-32BE",12001}  // Unicode UTF-32, big endian byte order; available only to managed applications
    ,{"X-CHINESE_CNS",20000}  // CNS Taiwan; Chinese Traditional (CNS)
    ,{"X-CP20001",20001}  // TCA Taiwan
    ,{"X_CHINESE-ETEN",20002}  // Eten Taiwan; Chinese Traditional (Eten)
    ,{"X-CP20003",20003}  // IBM5550 Taiwan
    ,{"X-CP20004",20004}  // TeleText Taiwan
    ,{"X-CP20005",20005}  // Wang Taiwan
    ,{"X-IA5",20105}  // IA5 (IRV International Alphabet No. 5, 7-bit); Western European (IA5)
    ,{"X-IA5-GERMAN",20106}  // IA5 German (7-bit)
    ,{"X-IA5-SWEDISH",20107}  // IA5 Swedish (7-bit)
    ,{"X-IA5-NORWEGIAN",20108}  // IA5 Norwegian (7-bit)
    ,{"US-ASCII",20127}  // US-ASCII (7-bit)
    ,{"X-CP20261",20261}  // T.61
    ,{"X-CP20269",20269}  // ISO 6937 Non-Spacing Accent
    ,{"IBM273",20273}  // IBM EBCDIC Germany
    ,{"IBM277",20277}  // IBM EBCDIC Denmark-Norway
    ,{"IBM278",20278}  // IBM EBCDIC Finland-Sweden
    ,{"IBM280",20280}  // IBM EBCDIC Italy
    ,{"IBM284",20284}  // IBM EBCDIC Latin America-Spain
    ,{"IBM285",20285}  // IBM EBCDIC United Kingdom
    ,{"IBM290",20290}  // IBM EBCDIC Japanese Katakana Extended
    ,{"IBM297",20297}  // IBM EBCDIC France
    ,{"IBM420",20420}  // IBM EBCDIC Arabic
    ,{"IBM423",20423}  // IBM EBCDIC Greek
    ,{"IBM424",20424}  // IBM EBCDIC Hebrew
    ,{"X-EBCDIC-KOREANEXTENDED",20833}  // IBM EBCDIC Korean Extended
    ,{"IBM-THAI",20838}  // IBM EBCDIC Thai
    ,{"KOI8-R",20866}  // Russian (KOI8-R); Cyrillic (KOI8-R)
    ,{"IBM871",20871}  // IBM EBCDIC Icelandic
    ,{"IBM880",20880}  // IBM EBCDIC Cyrillic Russian
    ,{"IBM905",20905}  // IBM EBCDIC Turkish
    ,{"IBM00924",20924}  // IBM EBCDIC Latin 1/Open System (1047 + Euro symbol)
    ,{"EUC-JP",20932}  // Japanese (JIS 0208-1990 and 0212-1990)
    ,{"X-CP20936",20936}  // Simplified Chinese (GB2312); Chinese Simplified (GB2312-80)
    ,{"X-CP20949",20949}  // Korean Wansung
    ,{"CP1025",21025}  // IBM EBCDIC Cyrillic Serbian-Bulgarian
    ,{"KOI8-U",21866}  // Ukrainian (KOI8-U); Cyrillic (KOI8-U)
    ,{"ISO-8859-1",28591}  // ISO 8859-1 Latin 1; Western European (ISO)
    ,{"ISO-8859-2",28592}  // ISO 8859-2 Central European; Central European (ISO)
    ,{"ISO-8859-3",28593}  // ISO 8859-3 Latin 3
    ,{"ISO-8859-4",28594}  // ISO 8859-4 Baltic
    ,{"ISO-8859-5",28595}  // ISO 8859-5 Cyrillic
    ,{"ISO-8859-6",28596}  // ISO 8859-6 Arabic
    ,{"ISO-8859-7",28597}  // ISO 8859-7 Greek
    ,{"ISO-8859-8",28598}  // ISO 8859-8 Hebrew; Hebrew (ISO-Visual)
    ,{"ISO-8859-9",28599}  // ISO 8859-9 Turkish
    ,{"ISO-8859-13",28603}  // ISO 8859-13 Estonian
    ,{"ISO-8859-15",28605}  // ISO 8859-15 Latin 9
    ,{"X-EUROPA",29001}  // Europa 3
    ,{"ISO-8859-8-I",38598}  // ISO 8859-8 Hebrew; Hebrew (ISO-Logical)
    ,{"ISO-2022-JP",50220}  // ISO 2022 Japanese with no halfwidth Katakana; Japanese (JIS)
    ,{"CSISO2022JP",50221}  // ISO 2022 Japanese with halfwidth Katakana; Japanese (JIS-Allow 1 byte Kana)
    ,{"ISO-2022-JP",50222}  // ISO 2022 Japanese JIS X 0201-1989; Japanese (JIS-Allow 1 byte Kana - SO/SI)
    ,{"ISO-2022-KR",50225}  // ISO 2022 Korean
    ,{"X-CP50227",50227}  // ISO 2022 Simplified Chinese; Chinese Simplified (ISO 2022)
    ,{"EUC-JP",51932}  // EUC Japanese
    ,{"EUC-CN",51936}  // EUC Simplified Chinese; Chinese Simplified (EUC)
    ,{"EUC-KR",51949}  // EUC Korean
    ,{"EUCKR",51949}  // EUC Korean
    ,{"HZ-GB-2312",52936}  // HZ-GB2312 Simplified Chinese; Chinese Simplified (HZ)
    ,{"GB18030",54936}  // Windows XP and later: GB18030 Simplified Chinese (4 byte); Chinese Simplified (GB18030)
    ,{"X-ISCII-DE",57002}  // ISCII Devanagari
    ,{"X-ISCII-BE",57003}  // ISCII Bangla
    ,{"X-ISCII-TA",57004}  // ISCII Tamil
    ,{"X-ISCII-TE",57005}  // ISCII Telugu
    ,{"X-ISCII-AS",57006}  // ISCII Assamese
    ,{"X-ISCII-OR",57007}  // ISCII Odia
    ,{"X-ISCII-KA",57008}  // ISCII Kannada
    ,{"X-ISCII-MA",57009}  // ISCII Malayalam
    ,{"X-ISCII-GU",57010}  // ISCII Gujarati
    ,{"X-ISCII-PA",57011}  // ISCII Punjabi
    ,{"UTF-7",65000}  // Unicode (UTF-7)
    ,{"UTF-8",65001}  // Unicode (UTF-8)
    ,{"UTF8",65001}  // Unicode (UTF-8)
    ,{NULL, 0}
};
static CMUTIL_Map *g_cmutil_csmap = NULL;
#else
# include <iconv.h>
#endif

CMUTIL_LogDefine("cmutil.string")

//*****************************************************************************
// CMUTIL_String implementation
//*****************************************************************************

void CMUTIL_StringBaseInit()
{
#if defined(_MSC_VER)
    struct CMUTIL_CSPair *pair = NULL;
    g_cmutil_csmap = CMUTIL_MapCreateEx(CMUTIL_MAP_DEFAULT, CMTrue, NULL);
    pair = g_msvc_cspiars;
    while (pair->csname) {
        CMCall(g_cmutil_csmap, Put, pair->csname, pair);
        pair++;
    }
#endif
}

void CMUTIL_StringBaseClear()
{
#if defined(_MSC_VER)
    CMCall(g_cmutil_csmap, Destroy);
#endif
}



typedef struct CMUTIL_String_Internal CMUTIL_String_Internal;
struct CMUTIL_String_Internal {
    CMUTIL_String   base;
    char            *data;
    size_t          capacity;
    size_t          size;
    CMUTIL_Mem      *memst;
};

CMUTIL_STATIC void CMUTIL_StringCheckSize(
        CMUTIL_String_Internal *str, size_t insize)
{
    size_t reqsz = (str->size + insize + 1);
    if (str->capacity < reqsz) {
        size_t newcap = str->capacity * 2;
        while (newcap < reqsz) newcap *= 2;
        str->data = str->memst->Realloc(str->data, newcap);
        str->capacity = newcap;
    }
}

CMUTIL_STATIC ssize_t CMUTIL_StringAddString(CMUTIL_String *string,
        const char *tobeadded)
{
    if (tobeadded) {
        CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
        register const char *p = tobeadded;

        while (*p) {
            CMUTIL_StringCheckSize(istr, 1);
            istr->data[istr->size++] = *p++;
        }
        istr->data[istr->size] = 0x0;
        return (ssize_t)istr->size;
    }
    CMLogErrorS("invalid argument. %s", tobeadded == NULL ? "NULL" : "");
    return -1;
}

CMUTIL_STATIC ssize_t CMUTIL_StringAddNString(CMUTIL_String *string,
        const char *tobeadded, size_t size)
{
    if (tobeadded && size > 0) {
        CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
        CMUTIL_StringCheckSize(istr, size);
        memcpy(istr->data + istr->size, tobeadded, size);
        istr->size += size;
        istr->data[istr->size] = 0x0;
        return (ssize_t)istr->size;
    }
    CMLogErrorS("invalid argument. %s, size: %u",
        tobeadded == NULL ? "NULL" : "", size);
    return -1;
}

CMUTIL_STATIC ssize_t CMUTIL_StringAddChar(CMUTIL_String *string, char tobeadded)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;

    CMUTIL_StringCheckSize(istr, 1);
    istr->data[istr->size++] = tobeadded;

    istr->data[istr->size] = 0x0;
    return (ssize_t)istr->size;
}

CMUTIL_STATIC ssize_t CMUTIL_StringAddPrint(
        CMUTIL_String *string, const char *fmt, ...)
{
    va_list args;
    ssize_t ir;
    va_start(args, fmt);
    ir = CMCall(string, AddVPrint, fmt, args);
    va_end(args);
    return ir;
}

CMUTIL_STATIC ssize_t CMUTIL_StringAddVPrint(
        CMUTIL_String *string, const char *fmt, va_list args)
{
    if (fmt) {
        char buf[4096];
        ssize_t len = vsnprintf(buf, sizeof(buf), fmt, args);
        return CMCall(string, AddNString, buf, len);
    }
    CMLogErrorS("invalid argument. %s", fmt == NULL ? "NULL" : "");
    return -1;
}

CMUTIL_STATIC ssize_t CMUTIL_StringAddAnother(
        CMUTIL_String *string, const CMUTIL_String *tobeadded)
{
    if (tobeadded) {
        const char *str = CMCall(tobeadded, GetCString);
        const size_t len = CMCall(tobeadded, GetSize);
        return CMCall(string, AddNString, str, len);
    }
    CMLogErrorS("invalid argument. %s", tobeadded == NULL ? "NULL" : "");
    return -1;
}

CMUTIL_STATIC ssize_t CMUTIL_StringInsertString(CMUTIL_String *string,
        const char *tobeadded, uint32_t at)
{
    size_t size = strlen((const char*)tobeadded);
    return CMCall(string, InsertNString, tobeadded, at, size);
}

CMUTIL_STATIC ssize_t CMUTIL_StringInsertNString(CMUTIL_String *string,
        const char *tobeadded, uint32_t at, size_t size)
{
    if (tobeadded && size > 0) {
        CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
        CMUTIL_StringCheckSize(istr, size);
        memmove(istr->data+at+size, istr->data+at, istr->size-(size_t)at+1);
        memcpy(istr->data+at, tobeadded, size);
        istr->size += size;
        return (ssize_t)istr->size;
    }
    CMLogErrorS("invalid argument. %s, size: %u",
        tobeadded == NULL ? "NULL" : "", size);
    return -1;
}

CMUTIL_STATIC ssize_t CMUTIL_StringInsertPrint(
        CMUTIL_String *string, uint32_t idx, const char *fmt, ...)
{
    va_list args;
    ssize_t ir;
    va_start(args, fmt);
    ir = CMCall(string, InsertVPrint, idx, fmt, args);
    va_end(args);
    return ir;
}

CMUTIL_STATIC ssize_t CMUTIL_StringInsertVPrint(
        CMUTIL_String *string, uint32_t idx, const char *fmt, va_list args)
{
    if (fmt) {
        char buf[4096];
        const ssize_t len = vsnprintf(buf, sizeof(buf), fmt, args);
        const ssize_t ir = CMCall(string, InsertNString, buf, idx, len);
        return ir;
    }
    CMLogErrorS("invalid argument. %s", fmt == NULL ? "NULL" : "");
    return -1;
}

CMUTIL_STATIC ssize_t CMUTIL_StringInsertAnother(
        CMUTIL_String *string, uint32_t idx, CMUTIL_String *tobeadded)
{
    const char *str = CMCall(tobeadded, GetCString);
    const ssize_t len = CMCall(tobeadded, GetSize);
    return CMCall(string, InsertNString, str, idx, len);
}

CMUTIL_STATIC void CMUTIL_StringCutTailOff(
        CMUTIL_String *string, size_t length)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    if (istr->size > length)
        istr->size -= length;
    else
        istr->size = 0;
    *(istr->data + istr->size) = 0x0;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringSubstring(
    const CMUTIL_String *string, uint32_t offset, size_t length)
{
    const CMUTIL_String_Internal *istr = (const CMUTIL_String_Internal*)string;
    CMUTIL_String *res = NULL;
    if (offset < istr->size) {
        size_t len = length;
        if ((offset + length) > istr->size)
            len = istr->size - offset;
        res = CMUTIL_StringCreateInternal(istr->memst, len, NULL);
        CMCall(res, AddNString, istr->data + offset, len);
    }
    return res;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringToLower(
    const CMUTIL_String *string)
{
    const CMUTIL_String_Internal *istr = (const CMUTIL_String_Internal*)string;
    CMUTIL_String_Internal *res =
            (CMUTIL_String_Internal*)CMUTIL_StringCreateInternal(
                istr->memst, istr->size, NULL);
    if (res) {
        register char *p = res->data, *q = istr->data;
        while (*q)
            *p++ = (char)tolower(*q++);
        *p = 0x0;
        res->size = istr->size;
    }
    return (CMUTIL_String*)res;
}

CMUTIL_STATIC void CMUTIL_StringSelfToUpper(CMUTIL_String *str)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)str;
    register char *p = istr->data;
    while (*p) {
        *p = (char)toupper(*p);
        p++;
    }
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringToUpper(
    const CMUTIL_String *string)
{
    const CMUTIL_String_Internal *istr = (const CMUTIL_String_Internal*)string;
    CMUTIL_String_Internal *res =
        (CMUTIL_String_Internal*)CMUTIL_StringCreateInternal(
                istr->memst, istr->size, NULL);
    if (res) {
        register char *p = res->data, *q = istr->data;
        while (*q)
            *p++ = (char)toupper(*q++);
        *p = 0x0;
        res->size = istr->size;
    }
    return (CMUTIL_String*)res;
}

CMUTIL_STATIC void CMUTIL_StringSelfToLower(CMUTIL_String *str)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)str;
    register char *p = istr->data;
    while (*p) {
        *p = (char)tolower(*p);
        p++;
    }
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringReplace(
        const CMUTIL_String *string, const char *needle, const char *alter)
{
    const CMUTIL_String_Internal *istr =
            (const CMUTIL_String_Internal*)string;
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
                CMCall(res, AddNString, prv, (size_t)(cur-prv));
            CMCall(res, AddString, alter);
            cur += nlen; prv = cur;
            cur = strstr(prv, needle);
        }
        CMCall(res, AddString, prv);
    } else {
        CMLogErrorS("not enough memory.");
    }
    return res;
}

CMUTIL_STATIC size_t CMUTIL_StringGetSize(const CMUTIL_String *string)
{
    const CMUTIL_String_Internal *istr = (const CMUTIL_String_Internal*)string;
    return istr->size;
}

CMUTIL_STATIC const char *CMUTIL_StringGetCString(const CMUTIL_String *string)
{
    const CMUTIL_String_Internal *istr = (const CMUTIL_String_Internal*)string;
    return istr->data;
}

CMUTIL_STATIC int CMUTIL_StringGetChar(const CMUTIL_String *string, uint32_t idx)
{
    const CMUTIL_String_Internal *istr = (const CMUTIL_String_Internal*)string;
    if (idx < istr->size)
        return (int)istr->data[idx];
    return -1;
}

CMUTIL_STATIC void CMUTIL_StringClear(CMUTIL_String *string)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    *(istr->data) = 0x0;
    istr->size = 0;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringClone(const CMUTIL_String *string)
{
    const CMUTIL_String_Internal *istr = (const CMUTIL_String_Internal*)string;
    size_t size = CMCall(string, GetSize);
    const char *str = CMCall(string, GetCString);
    CMUTIL_String *res =
            CMUTIL_StringCreateInternal(istr->memst, size, NULL);
    CMCall(res, AddNString, str, size);
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

CMUTIL_STATIC void CMUTIL_StringSelfTrim(CMUTIL_String *string)
{
    CMUTIL_String_Internal *istr = (CMUTIL_String_Internal*)string;
    if (istr) {

        // right trim
        size_t len = istr->size;
        register char *p = istr->data + len - 1;
        while (strchr(SPACES, *p) && (p > istr->data)) {
            p--; len--;
        }
        *(p+1) = 0x0;

        // left trim
        p = istr->data;
        while (strchr(SPACES, *p) && *p) {
            p++; len--;
        }

        // move to front of buffer
        if (p > istr->data)
            memmove(istr->data, p, len+1);

        // size reset
        istr->size = len;
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
    CMUTIL_StringGetChar,
    CMUTIL_StringClear,
    CMUTIL_StringClone,
    CMUTIL_StringDestroy,
    CMUTIL_StringSelfTrim
};

CMUTIL_String *CMUTIL_StringCreateInternal(
        CMUTIL_Mem *memst,
        size_t initcapacity,
        const char *initcontent)
{
    CMUTIL_String_Internal *istr = memst->Alloc(sizeof(CMUTIL_String_Internal));
    size_t capacity = initcapacity;
    const size_t len = initcontent ? strlen(initcontent) : 0;
    memset(istr, 0x0, sizeof(CMUTIL_String_Internal));
    memcpy(istr, &g_cmutil_string, sizeof(CMUTIL_String));

    if (capacity == 0) {
        if (initcontent) {
            capacity = len;
        } else {
            CMLogError("initcapacty is zero and initcontent is null");
            memst->Free(istr);
            return NULL;
        }
    }
    istr->data = memst->Alloc(capacity+1);
    istr->capacity = capacity+1;
    *(istr->data) = 0x0;
    istr->memst = memst;

    if (initcontent && len > 0)
        CMCall((CMUTIL_String*)istr, AddNString, initcontent, len);
    return (CMUTIL_String*)istr;
}

CMUTIL_String *CMUTIL_StringCreateEx(
        size_t initcapacity,
        const char *initcontent)
{
    return CMUTIL_StringCreateInternal(
                CMUTIL_GetMem(), initcapacity, initcontent);
}




//*****************************************************************************
// CMUTIL_StringArray implementation
//*****************************************************************************

typedef struct CMUTIL_StringArray_Internal {
    CMUTIL_StringArray  base;
    CMUTIL_Array        *array;
    CMUTIL_Mem          *memst;
} CMUTIL_StringArray_Internal;

CMUTIL_STATIC void CMUTIL_StringArrayAdd(
        CMUTIL_StringArray *array, CMUTIL_String *string)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    CMCall(iarray->array, Add, string);
}

CMUTIL_STATIC void CMUTIL_StringArrayAddCString(
        CMUTIL_StringArray *array, const char *string)
{
    CMUTIL_String *data = CMUTIL_StringCreateEx(0, string);
    CMCall(array, Add, data);
}

CMUTIL_STATIC CMBool CMUTIL_StringArrayInsertAt(
        CMUTIL_StringArray *array, CMUTIL_String *string, uint32_t index)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    return CMCall(iarray->array, InsertAt, string, index) == NULL ?
            CMFalse:CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_StringArrayInsertAtCString(
        CMUTIL_StringArray *array, const char *string, uint32_t index)
{
    CMUTIL_String *data = CMUTIL_StringCreateEx(0, string);
    if (CMCall(array, InsertAt, data, index)) {
        return CMTrue;
    }
    CMCall(data, Destroy);
    return CMFalse;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringArrayRemoveAt(
        CMUTIL_StringArray *array, uint32_t index)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    return (CMUTIL_String*)CMCall(iarray->array, RemoveAt, index);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringArraySetAt(
        CMUTIL_StringArray *array, CMUTIL_String *string, uint32_t index)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    return (CMUTIL_String*)CMCall(iarray->array, SetAt, string, index);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_StringArraySetAtCString(
        CMUTIL_StringArray *array, const char *string, uint32_t index)
{
    CMUTIL_String *data = CMUTIL_StringCreateEx(0, string);
    CMUTIL_String *res = (CMUTIL_String*)CMCall(array, SetAt, data, index);
    if (res == NULL) CMCall(data, Destroy);
    return res;
}

CMUTIL_STATIC const CMUTIL_String *CMUTIL_StringArrayGetAt(
        const CMUTIL_StringArray *array, uint32_t index)
{
    const CMUTIL_StringArray_Internal *iarray =
            (const CMUTIL_StringArray_Internal*)array;
    return (CMUTIL_String*)CMCall(iarray->array, GetAt, index);
}

CMUTIL_STATIC const char *CMUTIL_StringArrayGetCString(
        const CMUTIL_StringArray *array, uint32_t index)
{
    const CMUTIL_String *v = CMCall(array, GetAt, index);
    if (v) return CMCall(v, GetCString);
    return NULL;
}

CMUTIL_STATIC size_t CMUTIL_StringArrayGetSize(
        const CMUTIL_StringArray *array)
{
    const CMUTIL_StringArray_Internal *iarray =
            (const CMUTIL_StringArray_Internal*)array;
    return CMCall(iarray->array, GetSize);
}

CMUTIL_STATIC CMUTIL_Iterator *CMUTIL_StringArrayIterator(
        const CMUTIL_StringArray *array)
{
    const CMUTIL_StringArray_Internal *iarray =
            (const CMUTIL_StringArray_Internal*)array;
    return CMCall(iarray->array, Iterator);
}

CMUTIL_STATIC void CMUTIL_StringArrayDestroy(
        CMUTIL_StringArray *array)
{
    CMUTIL_StringArray_Internal *iarray = (CMUTIL_StringArray_Internal*)array;
    while (CMCall(iarray->array, GetSize) > 0) {
        CMUTIL_String *item = (CMUTIL_String*)CMCall(iarray->array, Pop);
        CMCall(item, Destroy);
    }
    CMCall(iarray->array, Destroy);
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
        CMUTIL_Mem *memst, size_t initcapacity)
{
    CMUTIL_StringArray_Internal *res =
            memst->Alloc(sizeof(CMUTIL_StringArray_Internal));
    memset(res, 0x0, sizeof(CMUTIL_StringArray_Internal));
    memcpy(res, &g_cmutil_stringarray, sizeof(CMUTIL_StringArray));
    res->memst=  memst;
    res->array = CMUTIL_ArrayCreateInternal(
                memst, initcapacity, NULL, NULL, CMFalse);
    return (CMUTIL_StringArray*)res;
}

CMUTIL_StringArray *CMUTIL_StringArrayCreateEx(size_t initcapacity)
{
    return CMUTIL_StringArrayCreateInternal(CMUTIL_GetMem(), initcapacity);
}



//*****************************************************************************
// CMUTIL_CSConv implementation
//*****************************************************************************

typedef struct CMUTIL_CSConv_Internal {
    CMUTIL_CSConv   base;
    char            *frcs;
    char            *tocs;
    CMUTIL_Mem      *memst;
} CMUTIL_CSConv_Internal;

CMUTIL_STATIC CMUTIL_String *CMUTIL_CSConvConv(
        CMUTIL_Mem *memst, CMUTIL_String *instr,
        const char *frcs, const char *tocs)
{
    CMUTIL_String *res = NULL;

    if (instr) {
#if defined(_MSC_VER)
        struct CMUTIL_CSPair *pair1, *pair2;
        pair1 = CMCall(g_cmutil_csmap, Get, frcs);
        pair2 = CMCall(g_cmutil_csmap, Get, tocs);
        if (pair1 && pair2) {
            // convert mbcs to widechar and convert widechar to mbcs
            WCHAR *wstr = NULL;
            int cvsize;
            int size = CMCall(instr, GetSize);
            char *rbuf = NULL;
            wstr = memst->Alloc(sizeof(WCHAR) * size);
            cvsize = MultiByteToWideChar(
                        pair1->cpno, 0, CMCall(instr, GetCString),
                        size, wstr, size);
            if (cvsize > 0) {
                rbuf = memst->Alloc(cvsize * 4);
                cvsize = WideCharToMultiByte(
                            pair2->cpno, 0, wstr, cvsize, rbuf, cvsize*4,
                            NULL, NULL);
            }
            if (cvsize > 0) {
                CMUTIL_String_Internal *pres = NULL;
                res = CMUTIL_StringCreateInternal(memst, cvsize, NULL);
                pres = (CMUTIL_String_Internal*)res;
                memcpy(pres->data, rbuf, cvsize);
                *(pres->data + cvsize) = 0x0;
                pres->size = cvsize;
            }
        }
#else
        iconv_t icv;
        size_t insz, outsz, osz, ressz;
        char *obuf, *src;


        outsz = ressz = 0;
        obuf = NULL;
        src = (char*)CMCall(instr, GetCString);
        insz = (size_t)CMCall(instr, GetSize);

        icv = iconv_open(tocs, frcs);

        osz = outsz = insz * 2;
        if (outsz > 0) {
            res = CMUTIL_StringCreateInternal(memst, outsz, NULL);
            obuf = (char*)CMCall(res, GetCString);

#if defined(SUNOS)
            ressz = iconv(icv, (const char**)&src, &insz, &obuf, &outsz);
#else
            ressz = iconv(icv, &src, &insz, &obuf, &outsz);
#endif
            if (ressz == (size_t) -1) {
                CMCall(res, Destroy);
                res = NULL;
            } else {
                CMUTIL_String_Internal *pres = (CMUTIL_String_Internal*)res;
                if (ressz == 0) {
                    pres->size = osz - outsz;
                } else {
                    pres->size = ressz;
                }
                *(pres->data + pres->size) = 0x0;
            }
        }
#endif
    }
    return res;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_CSConvForward(
        const CMUTIL_CSConv *conv, CMUTIL_String *instr)
{
    const CMUTIL_CSConv_Internal *iconv =
            (const CMUTIL_CSConv_Internal*)conv;
    return CMUTIL_CSConvConv(iconv->memst, instr, iconv->frcs, iconv->tocs);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_CSConvBackward(
        const CMUTIL_CSConv *conv, CMUTIL_String *instr)
{
    const CMUTIL_CSConv_Internal *iconv =
            (const CMUTIL_CSConv_Internal*)conv;
    return CMUTIL_CSConvConv(
        iconv->memst, instr, iconv->tocs, iconv->frcs);
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
        CMUTIL_Mem *memst, const char *fromcs, const char *tocs)
{
    CMUTIL_CSConv_Internal *res = memst->Alloc(sizeof(CMUTIL_CSConv_Internal));
    memset(res, 0x0, sizeof(CMUTIL_CSConv_Internal));
    memcpy(res, &g_cmutil_csconv, sizeof(CMUTIL_CSConv));
    res->frcs = memst->Strdup(fromcs);
    res->tocs = memst->Strdup(tocs);
    res->memst = memst;
    return (CMUTIL_CSConv*)res;
}

CMUTIL_CSConv *CMUTIL_CSConvCreate(const char *fromcs, const char *tocs)
{
    return CMUTIL_CSConvCreateInternal(CMUTIL_GetMem(), fromcs, tocs);
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
        CMUTIL_Mem *memst, const char *haystack, const char *needle)
{
    char buf[4096];
    CMUTIL_StringArray *res = NULL;
    if (haystack && needle) {
        register const char *p, *q, *r;
        int needlelen;
        res = CMUTIL_StringArrayCreateInternal(memst, 5);
        if (res) {
            CMUTIL_String *str = NULL;
            needlelen = (int)strlen(needle);
            p = haystack;
            /* find needle */
            q = strstr(p, needle);
            while (q) {
                strncpy(buf, p, (uint32_t)(q-p));
                *(buf+(q-p)) = 0x0;
                r = CMUTIL_StrTrim(buf);
                str = CMUTIL_StringCreateInternal(memst, strlen(r), r);
                CMCall(res, Add, str);
                /* skip needle */
                p = q + needlelen;
                /* find next needle */
                q = strstr(p, needle);
            }
            /* add last one */
            strcpy(buf, p);
            r = CMUTIL_StrTrim(buf);
            str = CMUTIL_StringCreateInternal(memst, strlen(r), r);
            CMCall(res, Add, str);
        }
    }
    return res;
}

CMUTIL_StringArray *CMUTIL_StringSplit(const char *haystack, const char *needle)
{
    return CMUTIL_StringSplitInternal(CMUTIL_GetMem(), haystack, needle);
}

const char *CMUTIL_StrNextToken(
    char *dest, size_t buflen, const char *src, const char *delim)
{
    if (buflen > 0 && src && dest && delim) {
        const register char *p = src;
        register char *q = dest;
        const register char *l = dest + buflen - 1; // save one for null character
        while (*p && !strchr(delim, *p) && q < l) *q++ = *p++;
        *q = 0x0;
        return p;
    }
    return NULL;
}

const char *CMUTIL_StrSkipSpaces(const char *line, const char *spaces)
{
    register const char *p = line;
    while (*p && strchr(spaces, *p)) p++;
    return p;
}

CMUTIL_STATIC int CMUTIL_StringHexChar(const int inp)
{
    const int ic = toupper(inp);
    if (ic >= 'A' && ic <= 'F')
        return 10 + (ic - 'A');
    if (ic >= '0' && ic <= '9')
        return ic - '0';
    return -1;
}

int CMUTIL_StringHexToBytes(uint8_t *dest, const char *src, int len)
{
    register const char* hexstr = src;
    int isodd = len % 2;
    register uint8_t* p = dest;

    if (isodd) {
        // pad the first byte to zero when the given length is an odd number.
        int ctob = CMUTIL_StringHexChar(*hexstr++);
        if (ctob < 0) return -1;
        /* upper 4 bits */
        *p = 0;
        /* lower 4 bits */
        *p |= (ctob & 0xF);
        len -= 1; p++;
        isodd = 0;
    }
    /* every 2 hex characters are converted to one byte */
    while (len > 0) {
        if (*hexstr) {
            int ctob = CMUTIL_StringHexChar(*hexstr++);
            if (ctob < 0) return -1;
            /* upper 4 bits */
            *p = ((ctob & 0xF) << 4);

            ctob = CMUTIL_StringHexChar(*hexstr++);
            if (ctob < 0) return -1;
            /* lower 4 bits */
            *p |= (ctob & 0xF);
        } else {
            *p = (uint8_t)0xFF;    // padding with 0xFF
        }
        len -= 2; p++;
    }
    /* return number of result bytes */
    return (int)(p - dest);
}

typedef struct CMUTIL_ByteBuffer_Internal {
    CMUTIL_ByteBuffer   base;
    uint8_t             *buffer;
    CMUTIL_Mem          *memst;
    size_t              size;
    size_t              capacity;
} CMUTIL_ByteBuffer_Internal;

CMUTIL_STATIC void CMUTIL_ByteBufferCheckSize(
        CMUTIL_ByteBuffer_Internal *bbi,
        uint32_t insize)
{
    size_t reqsz = bbi->size + insize;
    if (bbi->capacity < reqsz) {
        size_t ncapa = bbi->capacity * 2;
        while (ncapa < reqsz)
            ncapa *= 2;
        bbi->memst->Realloc(bbi->buffer, ncapa);
        bbi->capacity = ncapa;
    }
}

CMUTIL_STATIC CMUTIL_ByteBuffer *CMUTIL_ByteBufferAddByte(
        CMUTIL_ByteBuffer *buffer,
        uint8_t b)
{
    return CMCall(buffer, AddBytes, &b, 1);
}

CMUTIL_STATIC CMUTIL_ByteBuffer *CMUTIL_ByteBufferAddBytes(
        CMUTIL_ByteBuffer *buffer,
        const uint8_t *bytes,
        uint32_t length)
{
    if (bytes && length > 0) {
        CMUTIL_ByteBuffer_Internal *bbi = (CMUTIL_ByteBuffer_Internal*)buffer;
        CMUTIL_ByteBufferCheckSize(bbi, length);
        memcpy(bbi->buffer + bbi->size, bytes, length);
        bbi->size += length;
        return buffer;
    }
    CMLogErrorS("invalid parameter bytes: %s, length: %u",
        bytes == NULL ? "NULL" : "not NULL", length);
    return NULL;
}

CMUTIL_STATIC CMUTIL_ByteBuffer *CMUTIL_ByteBufferAddBytesPart(
        CMUTIL_ByteBuffer *buffer,
        const uint8_t *bytes,
        uint32_t offset,
        uint32_t length)
{
    if (bytes && length > 0) {
        CMUTIL_ByteBuffer_Internal *bbi = (CMUTIL_ByteBuffer_Internal*)buffer;
        CMUTIL_ByteBufferCheckSize(bbi, length);
        memcpy(bbi->buffer + bbi->size, bytes + offset, length);
        bbi->size += length;
        return buffer;
    }
    CMLogErrorS("invalid parameter bytes: %s, length: %u",
        bytes == NULL ? "NULL" : "not NULL", length);
    return NULL;
}

CMUTIL_STATIC CMUTIL_ByteBuffer *CMUTIL_ByteBufferInsertByteAt(
        CMUTIL_ByteBuffer *buffer,
        uint32_t index,
        uint8_t b)
{
    return CMCall(buffer, InsertBytesAt, index, &b, 1);
}

CMUTIL_STATIC CMUTIL_ByteBuffer *CMUTIL_ByteBufferInsertBytesAt(
        CMUTIL_ByteBuffer *buffer,
        const uint32_t index,
        const uint8_t *bytes,
        uint32_t length)
{
    if (bytes && length > 0) {
        CMUTIL_ByteBuffer_Internal *bbi = (CMUTIL_ByteBuffer_Internal*)buffer;
        if (index > bbi->size) {
            CMLogErrorS("index out of bounds: size - %u, request - %u",
                        (uint32_t)bbi->size, index);
            return NULL;
        }
        if (index == bbi->size)
            return CMCall(buffer, AddBytes, bytes, length);
        CMUTIL_ByteBufferCheckSize(bbi, length);
        memmove(bbi->buffer + index + length,
                bbi->buffer + index,
                bbi->size - index);
        memcpy(bbi->buffer + index, bytes, length);
        bbi->size += length;
        return buffer;
    }
    CMLogErrorS("invalid parameter bytes: %s, length: %u",
        bytes == NULL ? "NULL" : "not NULL", length);
    return NULL;
}

CMUTIL_STATIC int CMUTIL_ByteBufferGetAt(
        const CMUTIL_ByteBuffer *buffer,
        uint32_t index)
{
    CMUTIL_ByteBuffer_Internal *bbi = (CMUTIL_ByteBuffer_Internal*)buffer;
    if (index > bbi->size) {
        CMLogErrorS("index out of bounds: size - %u, request - %u",
                    (uint32_t)bbi->size, index);
        return -1;
    }
    return *(bbi->buffer + index);
}

CMUTIL_STATIC size_t CMUTIL_ByteBufferGetSize(
        const CMUTIL_ByteBuffer *buffer)
{
    CMUTIL_ByteBuffer_Internal *bbi = (CMUTIL_ByteBuffer_Internal*)buffer;
    return bbi->size;
}

CMUTIL_STATIC uint8_t *CMUTIL_ByteBufferGetBytes(
        CMUTIL_ByteBuffer *buffer)
{
    CMUTIL_ByteBuffer_Internal *bbi = (CMUTIL_ByteBuffer_Internal*)buffer;
    return bbi->buffer;
}

CMUTIL_STATIC CMBool CMUTIL_ByteBufferShrinkTo(
        CMUTIL_ByteBuffer *buffer,
        size_t size)
{
    CMUTIL_ByteBuffer_Internal *bbi = (CMUTIL_ByteBuffer_Internal*)buffer;
    if (bbi->capacity < size) {
        CMLogErrorS("out of bound(buffer capacity: %lu, size: %lu)",
                bbi->capacity, size);
        return CMFalse;
    }
    bbi->size = size;
    return CMTrue;
}

CMUTIL_STATIC size_t CMUTIL_ByteBufferGetCapacity(
        const CMUTIL_ByteBuffer *buffer)
{
    const CMUTIL_ByteBuffer_Internal *bbi =
            (const CMUTIL_ByteBuffer_Internal*)buffer;
    return bbi->capacity;
}

CMUTIL_STATIC void CMUTIL_ByteBufferDestroy(
        CMUTIL_ByteBuffer *buffer)
{
    CMUTIL_ByteBuffer_Internal *bbi = (CMUTIL_ByteBuffer_Internal*)buffer;
    if (bbi) {
        if (bbi->buffer)
            bbi->memst->Free(bbi->buffer);
        bbi->memst->Free(bbi);
    }
}

CMUTIL_STATIC void CMUTIL_ByteBufferClear(
        CMUTIL_ByteBuffer *buffer)
{
    CMUTIL_ByteBuffer_Internal *bbi = (CMUTIL_ByteBuffer_Internal*)buffer;
    bbi->size = 0;
}

static CMUTIL_ByteBuffer g_cmutil_bytebuffer = {
    CMUTIL_ByteBufferAddByte,
    CMUTIL_ByteBufferAddBytes,
    CMUTIL_ByteBufferAddBytesPart,
    CMUTIL_ByteBufferInsertByteAt,
    CMUTIL_ByteBufferInsertBytesAt,
    CMUTIL_ByteBufferGetAt,
    CMUTIL_ByteBufferGetSize,
    CMUTIL_ByteBufferGetBytes,
    CMUTIL_ByteBufferShrinkTo,
    CMUTIL_ByteBufferGetCapacity,
    CMUTIL_ByteBufferDestroy,
    CMUTIL_ByteBufferClear
};

CMUTIL_ByteBuffer *CMUTIL_ByteBufferCreateInternal(
        CMUTIL_Mem  *memst,
        size_t initcapacity)
{
    CMUTIL_ByteBuffer_Internal *res =
            memst->Alloc(sizeof(CMUTIL_ByteBuffer_Internal));
    memset(res, 0x0, sizeof(CMUTIL_ByteBuffer_Internal));
    memcpy(res, &g_cmutil_bytebuffer, sizeof(CMUTIL_ByteBuffer));
    res->memst = memst;
    res->buffer = memst->Alloc(sizeof(uint8_t) * initcapacity);
    res->capacity = initcapacity;
    return (CMUTIL_ByteBuffer*)res;
}

CMUTIL_ByteBuffer *CMUTIL_ByteBufferCreateEx(
        size_t initcapacity)
{
    return CMUTIL_ByteBufferCreateInternal(CMUTIL_GetMem(), initcapacity);
}

/* end of file */
