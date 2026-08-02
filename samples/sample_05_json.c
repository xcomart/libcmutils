/*
 * 05 - JSON
 *
 * Shows: building objects and arrays, pretty printing, parsing from a string
 *        and from a file, typed accessors, walking a document and Clone.
 *
 * The parser accepts comments, which is what makes the annotated .jsonc
 * logging configuration in samples/conf/sample_log.jsonc possible.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.json")

static CMUTIL_Json *build_document(void)
{
    CMUTIL_JsonObject *root;
    CMUTIL_JsonObject *nested;
    CMUTIL_JsonArray *array;

    root = CMUTIL_JsonObjectCreate();

    /* Typed mutators cover every scalar, so CMUTIL_JsonValue is rarely
     * needed directly. */
    CMCall(root, PutString, "name", "libcmutils");
    CMCall(root, PutLong, "answer", 42);
    CMCall(root, PutDouble, "ratio", 0.1234);
    CMCall(root, PutBoolean, "enabled", CMTrue);
    CMCall(root, PutNull, "optional");

    array = CMUTIL_JsonArrayCreate();
    CMCall(array, AddLong, 1);
    CMCall(array, AddLong, 2);
    CMCall(array, AddString, "three");
    CMCall(array, AddNull);

    nested = CMUTIL_JsonObjectCreate();
    CMCall(nested, PutString, "host", "127.0.0.1");
    CMCall(nested, PutLong, "port", 9999);

    /* Put/Add take ownership of the child. */
    CMCall(root, Put, "items", (CMUTIL_Json*)array);
    CMCall(root, Put, "server", (CMUTIL_Json*)nested);

    return (CMUTIL_Json*)root;
}

/* Recursive walk over the generic CMUTIL_Json interface. */
static void walk(CMUTIL_Json *json, const char *label, int depth)
{
    char indent[64];
    int pad = depth * 2;

    if (pad > (int)sizeof(indent) - 1) pad = (int)sizeof(indent) - 1;
    memset(indent, ' ', (size_t)pad);
    indent[pad] = 0;

    switch (CMCall(json, GetType)) {
    case CMJsonTypeObject: {
        CMUTIL_JsonObject *obj = (CMUTIL_JsonObject*)json;
        CMUTIL_StringArray *keys = CMCall(obj, GetKeys);
        uint32_t i;
        CMLogInfo("%s%s: object (%u keys)",
                  indent, label, (unsigned)CMCall(keys, GetSize));
        for (i = 0; i < CMCall(keys, GetSize); i++) {
            const char *key = CMCall(keys, GetCString, i);
            CMUTIL_Json *child = CMCall(obj, Get, key);
            walk(child, key, depth + 1);
        }
        CMCall(keys, Destroy);
        break;
    }
    case CMJsonTypeArray: {
        CMUTIL_JsonArray *arr = (CMUTIL_JsonArray*)json;
        uint32_t i;
        CMLogInfo("%s%s: array (%u items)",
                  indent, label, (unsigned)CMCall(arr, GetSize));
        for (i = 0; i < CMCall(arr, GetSize); i++) {
            CMUTIL_Json *child = CMCall(arr, Get, i);
            walk(child, "-", depth + 1);
        }
        break;
    }
    default: {
        CMUTIL_JsonValue *val = (CMUTIL_JsonValue*)json;
        switch (CMCall(val, GetValueType)) {
        case CMJsonValueLong:
            CMLogInfo("%s%s: %" PRId64, indent, label, CMCall(val, GetLong));
            break;
        case CMJsonValueDouble:
            CMLogInfo("%s%s: %f", indent, label, CMCall(val, GetDouble));
            break;
        case CMJsonValueString:
            CMLogInfo("%s%s: \"%s\"", indent, label, CMCall(val, GetCString));
            break;
        case CMJsonValueBoolean:
            CMLogInfo("%s%s: %s", indent, label,
                      CMCall(val, GetBoolean) ? "true" : "false");
            break;
        default:
            CMLogInfo("%s%s: null", indent, label);
            break;
        }
        break;
    }
    }
}

static void sample_build_and_print(void)
{
    CMUTIL_Json *doc;
    CMUTIL_Json *clone;
    CMUTIL_String *out;

    SAMPLE_SECTION("building a document");

    doc = build_document();

    out = CMUTIL_StringCreate();
    CMCall(doc, ToString, out, CMTrue);          /* CMTrue = pretty print */
    CMLogInfo("pretty:\n%s", CMCall(out, GetCString));

    CMCall(out, Clear);
    CMCall(doc, ToString, out, CMFalse);         /* compact */
    CMLogInfo("compact: %s", CMCall(out, GetCString));

    /* Clone is a deep copy. */
    clone = CMCall(doc, Clone);
    CMCall(out, Clear);
    CMCall(clone, ToString, out, CMFalse);
    CMLogInfo("clone  : %s", CMCall(out, GetCString));

    CMCall(out, Destroy);
    /* CMUTIL_JsonDestroy(x) works for every JSON type. */
    CMUTIL_JsonDestroy(clone);
    CMUTIL_JsonDestroy(doc);
}

static void sample_parse_string(void)
{
    const char *text =
        "{"
        "  \"key\": \"value\","
        "  \"list\": [10, 20, 30],"
        "  \"nested\": { \"flag\": true }"
        "}";
    CMUTIL_String *buf;
    CMUTIL_Json *parsed;
    CMUTIL_JsonObject *obj;
    CMUTIL_JsonArray *list;
    CMUTIL_JsonObject *nested;
    CMUTIL_Json *removed;

    SAMPLE_SECTION("parsing from a string");

    buf = CMUTIL_StringCreateEx(0, text);
    parsed = CMUTIL_JsonParse(buf);
    CMCall(buf, Destroy);

    if (parsed == NULL) {
        CMLogError("parse failed");
        return;
    }
    if (CMCall(parsed, GetType) != CMJsonTypeObject) {
        CMLogError("expected an object at the root");
        CMUTIL_JsonDestroy(parsed);
        return;
    }

    obj = (CMUTIL_JsonObject*)parsed;
    CMLogInfo("key    = %s", CMCall(obj, GetCString, "key"));

    list = (CMUTIL_JsonArray*)CMCall(obj, Get, "list");
    CMLogInfo("list[1] = %" PRId64, CMCall(list, GetLong, 1));

    nested = (CMUTIL_JsonObject*)CMCall(obj, Get, "nested");
    CMLogInfo("nested.flag = %s",
              CMCall(nested, GetBoolean, "flag") ? "true" : "false");

    /* Remove hands the child back; Delete destroys it in place.
     * CMUTIL_JsonDestroy is itself a CMCall, so the removed value goes
     * through a temporary rather than being nested into it. */
    removed = CMCall(obj, Remove, "key");
    CMUTIL_JsonDestroy(removed);
    CMCall(obj, Delete, "nested");
    CMLogInfo("after Remove/Delete, key is %s",
              CMCall(obj, Get, "key") ? "still there" : "gone");

    CMUTIL_JsonDestroy(parsed);
}

static void sample_parse_file(void)
{
    const char *path = SAMPLE_DATA("sample_data.json");
    CMUTIL_File *file;
    CMUTIL_String *contents;
    CMUTIL_Json *parsed;

    SAMPLE_SECTION("parsing a file");

    file = CMUTIL_FileCreate(path);
    contents = CMCall(file, GetContents);
    CMCall(file, Destroy);

    if (contents == NULL) {
        CMLogWarn("cannot read %s", path);
        return;
    }

    parsed = CMUTIL_JsonParse(contents);
    CMCall(contents, Destroy);

    if (parsed == NULL) {
        CMLogError("%s is not valid JSON", path);
        return;
    }

    walk(parsed, "root", 0);
    CMUTIL_JsonDestroy(parsed);
}

int main(void)
{
    sample_init();

    sample_build_and_print();
    sample_parse_string();
    sample_parse_file();

    return sample_exit(0);
}
