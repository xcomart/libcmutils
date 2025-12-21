//
// Created by 박성진 on 25. 12. 12.
//

#include <string.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.string")

void add_vprint(CMUTIL_String *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    CMCall(str, AddVPrint, fmt, ap);
    va_end(ap);
}

int main() {
    int ir = -1;
    CMUTIL_Init(CMUTIL_MEM_TYPE);

    CMUTIL_StringArray *sarr = NULL;
    CMUTIL_Iterator *iter = NULL;
    CMUTIL_ByteBuffer *bbuf = NULL;

    //////////////////////////////////////////////////////////////////////
    // CMUTIL_String tests

    CMUTIL_String *another = NULL;
    CMUTIL_String *str = CMUTIL_StringCreate();
    CMLogInfo("CMUTIL_String test start =============================");
    ASSERT((str != NULL), "CMUTIL_StringCreate");

    CMCall(str, AddString, "test");
    ASSERT(strcmp(CMCall(str, GetCString), "test") == 0, "AddString");

    CMCall(str, AddNString, "test", 2);
    ASSERT(strcmp(CMCall(str, GetCString), "testte") == 0, "AddNString");

    CMCall(str, AddChar, 'T');
    ASSERT(strcmp(CMCall(str, GetCString), "testteT") == 0, "AddChar");

    CMCall(str, Clear);
    ASSERT(strlen(CMCall(str, GetCString)) == 0, "Clear");

    CMCall(str, AddPrint, "%d", 12);
    ASSERT(strcmp(CMCall(str, GetCString), "12") == 0, "AddPrint");

    add_vprint(str, "%d", 12);
    ASSERT(strcmp(CMCall(str, GetCString), "1212") == 0, "AddVPrint");

    CMCall(str, Clear);
    another = CMUTIL_StringCreate();
    CMCall(another, AddString, "test");
    CMCall(str, AddAnother, another);
    ASSERT(strcmp(CMCall(str, GetCString), "test") == 0, "AddAnother");

    CMCall(str, InsertString, "st", 2);
    ASSERT(strcmp(CMCall(str, GetCString), "testst") == 0, "InsertString");

    CMCall(str, InsertNString, "stoa", 2, 2);
    ASSERT(strcmp(CMCall(str, GetCString), "teststst") == 0, "InsertNString");

    CMCall(str, InsertPrint, 2, "%d", 12);
    ASSERT(strcmp(CMCall(str, GetCString), "te12ststst") == 0, "InsertPrint");

    CMCall(str, CutTailOff, 6);
    ASSERT(strcmp(CMCall(str, GetCString), "te12") == 0, "CutTailOff");

    if (another) CMCall(another, Destroy); another = NULL;
    another = CMCall(str, Substring, 1, 2);
    ASSERT(strcmp(CMCall(another, GetCString), "e1") == 0, "Substring");
    ASSERT(strcmp(CMCall(str, GetCString), "te12") == 0, "Substring");

    if (another) CMCall(another, Destroy); another = NULL;
    CMCall(str, InsertString, "S", 2);
    another = CMCall(str, ToLower);
    ASSERT(strcmp(CMCall(another, GetCString), "tes12") == 0, "ToLower");
    ASSERT(strcmp(CMCall(str, GetCString), "teS12") == 0, "ToLower");

    CMCall(str, SelfToLower);
    ASSERT(strcmp(CMCall(str, GetCString), "tes12") == 0, "SelfToLower");

    if (another) CMCall(another, Destroy); another = NULL;
    another = CMCall(str, ToUpper);
    ASSERT(strcmp(CMCall(another, GetCString), "TES12") == 0, "ToUpper");
    ASSERT(strcmp(CMCall(str, GetCString), "tes12") == 0, "ToUpper");

    CMCall(str, SelfToUpper);
    ASSERT(strcmp(CMCall(str, GetCString), "TES12") == 0, "SelfToUpper");

    if (another) CMCall(another, Destroy); another = NULL;
    CMCall(str, AddString, "TES12");
    another = CMCall(str, Replace, "TES", "tes");
    ASSERT(strcmp(CMCall(another, GetCString), "tes12tes12") == 0, "Replace");
    ASSERT(strcmp(CMCall(str, GetCString), "TES12TES12") == 0, "Replace");

    ASSERT(CMCall(str, GetSize) == 10, "GetSize");

    if (another) CMCall(another, Destroy); another = NULL;
    another = CMCall(str, Clone);
    ASSERT(strcmp(CMCall(str, GetCString), CMCall(another, GetCString)) == 0, "Clone");

    CMCall(str, Clear);
    CMCall(str, AddString, "  test \t\n ");
    CMCall(str, SelfTrim);
    ASSERT(strcmp(CMCall(str, GetCString), "test") == 0, "SelfTrim");


    //////////////////////////////////////////////////////////////////////
    // CMUTIL_StringArray tests
    CMLogInfo("CMUTIL_StringArray test start =============================");

    sarr = CMUTIL_StringArrayCreate();
    ASSERT(sarr != NULL, "CMUTIL_StringArrayCreate");
    CMCall(sarr, Add, CMUTIL_StringCreateEx(10, "first"));
    ASSERT(CMCall(sarr, GetSize) == 1, "StringArray Add");
    ASSERT(strcmp(CMCall(sarr, GetCString, 0), "first") == 0, "StringArray Add/GetCString validation");

    CMCall(sarr, AddCString, "second");
    ASSERT(CMCall(sarr, GetSize) == 2, "StringArray AddCString");
    ASSERT(strcmp(CMCall(sarr, GetCString, 1), "second") == 0, "StringArray AddCString/GetCString validation");

    CMCall(sarr, InsertAt, CMUTIL_StringCreateEx(10, "1/2"), 0);
    ASSERT(CMCall(sarr, GetSize) == 3, "StringArray InsertAt front");
    ASSERT(strcmp(CMCall(sarr, GetCString, 0), "1/2") == 0, "StringArray InsertAt front validation");

    CMCall(sarr, InsertAt, CMUTIL_StringCreateEx(10, "third"), 3);
    ASSERT(CMCall(sarr, GetSize) == 4, "StringArray InsertAt last");
    ASSERT(strcmp(CMCall(sarr, GetCString, 3), "third") == 0, "StringArray InsertAt last validation");

    CMCall(sarr, InsertAtCString, "1/4", 0);
    ASSERT(CMCall(sarr, GetSize) == 5, "StringArray InsertAtCString front");
    ASSERT(strcmp(CMCall(sarr, GetCString, 0), "1/4") == 0, "StringArray InsertAtCString front validation");

    CMCall(sarr, InsertAtCString, "fourth", 5);
    ASSERT(CMCall(sarr, GetSize) == 6, "StringArray InsertAtCString last");
    ASSERT(strcmp(CMCall(sarr, GetCString, 5), "fourth") == 0, "StringArray InsertAtCString last validation");

    CMCall(str, Destroy); str = NULL;
    str = CMCall(sarr, RemoveAt, 0);
    ASSERT(CMCall(sarr, GetSize) == 5, "StringArray RemoveAt");
    ASSERT(strcmp(CMCall(str, GetCString), "1/4") == 0, "StringArray RemoveAt validation");

    CMCall(another, Destroy); another = NULL;
    another = CMCall(sarr, SetAt, str, 0); str = NULL;
    ASSERT(CMCall(sarr, GetSize) == 5, "StringArray SetAt");
    ASSERT(strcmp(CMCall(another, GetCString), "1/2") == 0, "StringArray SetAt validation");

    str = CMCall(sarr, SetAtCString, "quarter", 0);
    ASSERT(CMCall(sarr, GetSize) == 5, "StringArray SetAtCString");
    ASSERT(strcmp(CMCall(str, GetCString), "1/4") == 0, "StringArray SetAtCString validation");

    CMCall(str, Destroy); str = NULL;
    const CMUTIL_String *cstr = CMCall(sarr, GetAt, 0);
    ASSERT(strcmp(CMCall(cstr, GetCString), "quarter") == 0, "StringArray GetAt validation");

    ASSERT(strcmp(CMCall(sarr, GetCString, 0), "quarter") == 0, "StringArray GetCString");

    iter = CMCall(sarr, Iterator);
    ASSERT(iter != NULL, "StringArray Iterator");
    int i = 0;
    for (; i < CMCall(sarr, GetSize) && CMCall(iter, HasNext); i++) {
        CMUTIL_String *item = CMCall(iter, Next);
        ASSERT(strcmp(CMCall(item, GetCString), CMCall(sarr, GetCString, i)) == 0, "StringArray Iterator validation");
    }
    ASSERT(i == CMCall(sarr, GetSize) && !CMCall(iter, HasNext), "StringArray Iterator count");

    CMLogInfo("StringArray's GetSize is tested in other tests.");

    char testbuf[1024] = "   test   ";

    char *r = CMUTIL_StrRTrim(testbuf);
    ASSERT(strcmp(r, "   test") == 0 && r == testbuf, "CMUTIL_StrRTrim");

    strcpy(testbuf, "   test   ");
    r = CMUTIL_StrLTrim(testbuf);
    ASSERT(strcmp(r, "test   ") == 0 && r == testbuf, "CMUTIL_StrLTrim");

    strcpy(testbuf, "   test   ");
    r = CMUTIL_StrTrim(testbuf);
    ASSERT(strcmp(r, "test") == 0 && r == testbuf, "CMUTIL_StrTrim");

    strcpy(testbuf, "asdf;qwer:[]");
    char destbuf[20];
    const char *p = CMUTIL_StrNextToken(destbuf, 20, testbuf, ";:");
    ASSERT(strcmp(destbuf, "asdf") == 0 && *p == ';', "CMUTIL_StrNextToken");
    p = CMUTIL_StrNextToken(destbuf, 20, p+1, ";:");
    ASSERT(strcmp(destbuf, "qwer") == 0 && *p == ':', "CMUTIL_StrNextToken");

    strcpy(testbuf, "\r\n \t test");
    p = CMUTIL_StrSkipSpaces(testbuf, " \t\r\n");
    ASSERT(strcmp(p, "test") == 0, "CMUTIL_StrSkipSpaces");

    CMCall(sarr, Destroy); sarr = NULL;
    sarr = CMUTIL_StringSplit("asdf:;qwer:;1234;:zxcv", ":;");
    ASSERT(sarr != NULL && CMCall(sarr, GetSize) == 3, "CMUTIL_StringSplit");


    //////////////////////////////////////////////////////////////////////
    // CMUTIL_ByteBuffer tests
    CMLogInfo("CMUTIL_ByteBuffer test start =============================");

    bbuf = CMUTIL_ByteBufferCreate();
    ASSERT(bbuf != NULL, "CMUTIL_ByteBufferCreate");

    CMUTIL_ByteBuffer *rb = CMCall(bbuf, AddByte, 'c');
    ASSERT(rb == bbuf && CMCall(rb, GetSize) == 1 && CMCall(rb, GetAt, 0) == 'c',
        "ByteBuffer AddByte");
    rb = CMCall(bbuf, AddBytes, (uint8_t*)"test", 4);
    ASSERT(rb == bbuf && CMCall(rb, GetSize) == 5 && strncmp((char*)CMCall(rb, GetBytes), "ctest", 5) == 0,"ByteBuffer AddBytes");

    rb = CMCall(bbuf, AddBytesPart, (uint8_t*)"hello world", 6, 5);
    ASSERT(rb == bbuf && CMCall(rb, GetSize) == 10 && strncmp((char*)CMCall(rb, GetBytes), "ctestworld", 10) == 0,"ByteBuffer AddBytesPart");


    ir = 0;
END_POINT:
    if (bbuf) CMCall(bbuf, Destroy);
    if (iter) CMCall(iter, Destroy);
    if (sarr) CMCall(sarr, Destroy);
    if (str) CMCall(str, Destroy);
    if (another) CMCall(another, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}