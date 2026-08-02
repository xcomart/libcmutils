/*
 * 04 - Text and binary buffers
 *
 * Shows: CMUTIL_String (append / insert / transform), CMUTIL_StringArray,
 *        the in-place char* helpers, CMUTIL_ByteBuffer and CMUTIL_CSConv.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.strings")

/* AddVPrint is the va_list form of AddPrint - useful for wrappers. */
static void append_formatted(CMUTIL_String *str, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    CMCall(str, AddVPrint, fmt, args);
    va_end(args);
}

static void sample_string(void)
{
    CMUTIL_String *str;
    CMUTIL_String *other;

    SAMPLE_SECTION("CMUTIL_String");

    /* CMUTIL_StringCreateEx(capacity, initial_text); 0 = default capacity. */
    str = CMUTIL_StringCreateEx(0, "test");

    CMCall(str, AddString, "-appended");
    CMCall(str, AddChar, '!');
    CMCall(str, AddPrint, " %d", 12);
    append_formatted(str, " (%s)", "vprint");
    CMLogInfo("after appends : %s", CMCall(str, GetCString));

    CMCall(str, InsertString, "**", 4);
    CMCall(str, InsertPrint, 0, "[%03d] ", 7);
    CMLogInfo("after inserts : %s", CMCall(str, GetCString));

    /* CutTailOff keeps the first n bytes. */
    CMCall(str, CutTailOff, 10);
    CMLogInfo("after CutTailOff(10): %s", CMCall(str, GetCString));

    /*
     * The Self* variants mutate in place; the others return a new object
     * that the caller owns.
     */
    other = CMCall(str, ToUpper);
    CMLogInfo("ToUpper       : %s (original still %s)",
              CMCall(other, GetCString), CMCall(str, GetCString));
    CMCall(other, Destroy);

    CMCall(str, SelfToUpper);
    CMLogInfo("SelfToUpper   : %s", CMCall(str, GetCString));

    other = CMCall(str, Substring, 1, 4);
    CMLogInfo("Substring(1,4): %s", CMCall(other, GetCString));
    CMCall(other, Destroy);

    other = CMCall(str, Replace, "0", "#");
    CMLogInfo("Replace(0,#)  : %s", CMCall(other, GetCString));
    CMCall(other, Destroy);

    other = CMCall(str, Clone);
    CMLogInfo("Clone         : %s (%u bytes)",
              CMCall(other, GetCString), (unsigned)CMCall(other, GetSize));
    CMCall(other, Destroy);

    CMCall(str, Clear);
    CMCall(str, AddString, "  \t padded \r\n ");
    CMCall(str, SelfTrim);
    CMLogInfo("SelfTrim      : [%s]", CMCall(str, GetCString));

    CMCall(str, Destroy);
}

static void sample_string_array(void)
{
    CMUTIL_StringArray *parts;
    CMUTIL_StringArray *fixed;
    CMUTIL_String *joined;
    CMUTIL_String *removed;
    uint32_t i;

    SAMPLE_SECTION("CMUTIL_StringArray");

    /* Split on any of the delimiter characters; empty tokens are dropped. */
    parts = CMUTIL_StringSplit("alpha:;beta:;gamma", ":;");
    for (i = 0; i < CMCall(parts, GetSize); i++)
        CMLogInfo("part[%u] = %s", (unsigned)i, CMCall(parts, GetCString, i));

    /* Both the object and the C string form exist for every mutator. */
    CMCall(parts, AddCString, "delta");
    CMCall(parts, InsertAtCString, "zero", 0);

    /* SetAt/SetAtCString return the element they replaced, and the caller
     * owns it from then on - dropping it on the floor leaks. */
    removed = CMCall(parts, SetAtCString, "ZERO", 0);
    CMLogInfo("replaced: %s", CMCall(removed, GetCString));
    CMCall(removed, Destroy);

    /* RemoveAt hands the CMUTIL_String object over the same way. */
    removed = CMCall(parts, RemoveAt, 0);
    CMLogInfo("removed: %s", CMCall(removed, GetCString));
    CMCall(removed, Destroy);

    joined = CMUTIL_StringCreate();
    CMCall(parts, PrintTo, joined);
    CMLogInfo("PrintTo: %s", CMCall(joined, GetCString));
    CMCall(joined, Destroy);

    /* The array owns its elements. */
    CMCall(parts, Destroy);

    /* A literal array, NULL-terminated for you by the macro. */
    fixed = CMUTIL_StringArrayCreateWith("one", "two", "three");
    CMLogInfo("CreateWith size=%u last=%s",
              (unsigned)CMCall(fixed, GetSize),
              CMCall(fixed, GetCString, 2));
    CMCall(fixed, Destroy);
}

static void sample_raw_helpers(void)
{
    char buf[64];
    char token[32];
    const char *p;
    uint8_t bytes[8];
    int nbytes;
    int i;
    CMUTIL_String *hex;

    SAMPLE_SECTION("in-place char* helpers");

    /* These modify the input and return a pointer into it. */
    strcpy(buf, "   padded   ");
    CMLogInfo("StrTrim  : [%s]", CMUTIL_StrTrim(buf));

    strcpy(buf, "   padded   ");
    CMLogInfo("StrLTrim : [%s]", CMUTIL_StrLTrim(buf));

    strcpy(buf, "   padded   ");
    CMLogInfo("StrRTrim : [%s]", CMUTIL_StrRTrim(buf));

    strcpy(buf, "key=value;next");
    p = CMUTIL_StrNextToken(token, sizeof(token), buf, "=;");
    CMLogInfo("StrNextToken: [%s], stopped at '%c'", token, *p);

    strcpy(buf, "\r\n\t value");
    CMLogInfo("StrSkipSpaces: [%s]", CMUTIL_StrSkipSpaces(buf, " \t\r\n"));

    /* Hex decoding into a caller-supplied buffer. */
    nbytes = CMUTIL_StringHexToBytes(bytes, "cafebabe", 8);
    hex = CMUTIL_StringCreate();
    for (i = 0; i < nbytes; i++)
        CMCall(hex, AddPrint, "%02x ", bytes[i]);
    CMLogInfo("HexToBytes(\"cafebabe\") -> %d bytes: %s",
              nbytes, CMCall(hex, GetCString));
    CMCall(hex, Destroy);
}

static void sample_byte_buffer(void)
{
    CMUTIL_ByteBuffer *buf;
    uint8_t *bytes;
    uint32_t len;

    SAMPLE_SECTION("CMUTIL_ByteBuffer");

    /* The currency of the socket and HTTP APIs. */
    buf = CMUTIL_ByteBufferCreateEx(16);

    CMCall(buf, AddByte, 'c');
    CMCall(buf, AddBytes, (const uint8_t*)"test", 4);
    /* offset 6, length 5 of "hello world" -> "world" */
    CMCall(buf, AddBytesPart, (const uint8_t*)"hello world", 6, 5);
    CMCall(buf, InsertBytesAt, 0, (const uint8_t*)">> ", 3);

    bytes = CMCall(buf, GetBytes);
    len = (uint32_t)CMCall(buf, GetSize);
    CMLogInfo("content: %.*s (size=%u capacity=%u)",
              (int)len, (const char*)bytes, (unsigned)len,
              (unsigned)CMCall(buf, GetCapacity));
    CMLogInfo("byte at 3: '%c'", CMCall(buf, GetAt, 3));

    CMCall(buf, ShrinkTo, 5);
    bytes = CMCall(buf, GetBytes);
    len = (uint32_t)CMCall(buf, GetSize);
    CMLogInfo("after ShrinkTo(5): %.*s", (int)len, (const char*)bytes);

    CMCall(buf, Clear);
    CMLogInfo("after Clear: size=%u", (unsigned)CMCall(buf, GetSize));

    CMCall(buf, Destroy);
}

static void sample_charset(void)
{
    CMUTIL_CSConv *conv;
    CMUTIL_String *source;
    CMUTIL_String *converted;
    CMUTIL_String *back;

    SAMPLE_SECTION("CMUTIL_CSConv - iconv wrapper");

    /* Forward converts from the first charset to the second, Backward
     * goes the other way. Creation fails when the platform iconv does not
     * know one of the names. */
    conv = CMUTIL_CSConvCreate("UTF-8", "EUC-KR");
    if (conv == NULL) {
        CMLogWarn("EUC-KR is not available on this platform, skipping");
        return;
    }

    source = CMUTIL_StringCreateEx(0, "\355\225\234\352\270\200 test"); /* UTF-8 */
    converted = CMCall(conv, Forward, source);
    back = CMCall(conv, Backward, converted);

    CMLogInfo("utf-8 %u bytes -> euc-kr %u bytes -> utf-8 %u bytes",
              (unsigned)CMCall(source, GetSize),
              (unsigned)CMCall(converted, GetSize),
              (unsigned)CMCall(back, GetSize));
    CMLogInfo("round trip: %s", CMCall(back, GetCString));

    CMCall(back, Destroy);
    CMCall(converted, Destroy);
    CMCall(source, Destroy);
    CMCall(conv, Destroy);
}

int main(void)
{
    sample_init();

    sample_string();
    sample_string_array();
    sample_raw_helpers();
    sample_byte_buffer();
    sample_charset();

    return sample_exit(0);
}
