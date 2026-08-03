/*
 * 06 - XML
 *
 * Shows: parsing from a file and from a string, walking the DOM, reading and
 *        writing attributes, building a document by hand, serializing with
 *        ToDocument and converting to the JSON model with CMUTIL_XmlToJson.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.xml")

static void walk(const CMUTIL_XmlNode *node, int depth)
{
    char indent[64];
    int pad = depth * 2;
    uint32_t i;

    if (pad > (int)sizeof(indent) - 1) pad = (int)sizeof(indent) - 1;
    memset(indent, ' ', (size_t)pad);
    indent[pad] = 0;

    if (CMCall(node, GetType) == CMXmlNodeText) {
        /* For a text node the "name" is the text itself. */
        CMLogInfo("%stext: \"%s\"", indent, CMCall(node, GetName));
        return;
    }

    {
        CMUTIL_StringArray *attrs = CMCall(node, GetAttributeNames);
        CMUTIL_String *line = CMUTIL_StringCreate();
        uint32_t nattr = (uint32_t)CMCall(attrs, GetSize);
        uint32_t nchild = (uint32_t)CMCall(node, ChildCount);
        /* Never nest CMCall inside another CMCall's argument list. */
        const char *name = CMCall(node, GetName);

        CMCall(line, AddPrint, "%s<%s>", indent, name);
        for (i = 0; i < nattr; i++) {
            const char *key = CMCall(attrs, GetCString, i);
            /* GetAttribute hands back the node's own CMUTIL_String - read
             * it, do not destroy it. */
            CMUTIL_String *value = CMCall(node, GetAttribute, key);
            const char *text = CMCall(value, GetCString);
            CMCall(line, AddPrint, " %s=\"%s\"", key, text);
        }
        CMCall(line, AddPrint, " (%u children)", (unsigned)nchild);
        CMLogInfo("%s", CMCall(line, GetCString));

        CMCall(line, Destroy);
        CMCall(attrs, Destroy);

        for (i = 0; i < nchild; i++) {
            CMUTIL_XmlNode *child = CMCall(node, ChildAt, i);
            walk(child, depth + 1);
        }
    }
}

static void sample_parse_string(void)
{
    CMUTIL_String *src;
    CMUTIL_XmlNode *root;
    CMUTIL_String *doc;

    SAMPLE_SECTION("parsing a string");

    src = CMUTIL_StringCreateEx(0,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<config env=\"dev\">"
            "<server host=\"127.0.0.1\" port=\"9999\"/>"
            "<name>sample</name>"
            "</config>");

    /* CMUTIL_XmlParseString(str, len) takes a plain char* instead. */
    root = CMUTIL_XmlParse(src);
    CMCall(src, Destroy);

    if (root == NULL) {
        CMLogError("parse failed");
        return;
    }

    walk(root, 0);

    doc = CMCall(root, ToDocument, CMTrue);      /* CMTrue = indented */
    CMLogInfo("serialized:\n%s", CMCall(doc, GetCString));
    CMCall(doc, Destroy);

    CMCall(root, Destroy);
}

static void sample_parse_file(void)
{
    const char *path = SAMPLE_DATA("sample_data.xml");
    CMUTIL_XmlNode *root;
    CMUTIL_Json *json;
    CMUTIL_String *out;

    SAMPLE_SECTION("parsing a file and converting to JSON");

    root = CMUTIL_XmlParseFile(path);
    if (root == NULL) {
        CMLogWarn("cannot parse %s", path);
        return;
    }

    walk(root, 0);

    /* Tags become objects, repeated tags become arrays, attributes and text
     * become members. */
    json = CMUTIL_XmlToJson(root);
    out = CMUTIL_StringCreate();
    CMCall(json, ToString, out, CMTrue);
    CMLogInfo("as JSON:\n%s", CMCall(out, GetCString));

    CMCall(out, Destroy);
    CMUTIL_JsonDestroy(json);
    CMCall(root, Destroy);
}

static void sample_build(void)
{
    CMUTIL_XmlNode *root;
    CMUTIL_XmlNode *item;
    CMUTIL_XmlNode *text;
    CMUTIL_String *doc;

    SAMPLE_SECTION("building a document by hand");

    root = CMUTIL_XmlNodeCreate(CMXmlNodeTag, "catalog");
    CMCall(root, SetAttribute, "version", "1");

    item = CMUTIL_XmlNodeCreate(CMXmlNodeTag, "item");
    CMCall(item, SetAttribute, "id", "42");

    text = CMUTIL_XmlNodeCreate(CMXmlNodeText, "a sample entry");

    /* AddChild takes ownership: destroying the root destroys the tree. */
    CMCall(item, AddChild, text);
    CMCall(root, AddChild, item);

    {
        /* Portable code keeps a call out of the receiver position: only
         * where CMUTIL_CALL_SINGLE_EVAL is 1 is the receiver guaranteed to
         * be evaluated once. */
        CMUTIL_XmlNode *parent = CMCall(item, GetParent);
        CMLogInfo("item's parent is <%s>", CMCall(parent, GetName));
    }

    doc = CMCall(root, ToDocument, CMTrue);
    CMLogInfo("built:\n%s", CMCall(doc, GetCString));

    CMCall(doc, Destroy);
    CMCall(root, Destroy);
}

int main(void)
{
    sample_init();

    sample_parse_string();
    sample_parse_file();
    sample_build();

    return sample_exit(0);
}
