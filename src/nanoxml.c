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

/*
 * nanoxml.c
 *
 *  Created on: 2014. 7. 30.
 *      Author: comart
 */

#include "functions.h"
#include <stdio.h>

CMUTIL_LogDefine("cmutils.nanoxml")

static char g_cmutil_xml_escape[] = {
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,1,0,0,0,1,1,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,1,
        0,1,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0
};

static char *g_cmutil_xml_escape_str[] = {
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,"&quot;",0,0,0,"&amp;","&apos;",0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,"&lt;",
        0,"&gt;",0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0
};

static struct CMUTIL_EscapePair {
    char *key;
    char *val;
} const g_cmutil_xml_escapes[] = {
        {"quot", "\""},
        {"amp", "&"},
        {"apos", "\'"},
        {"lt", "<"},
        {"gt", ">"},
        {NULL, NULL}
};

typedef struct CMUTIL_XmlNode_Internal {
    CMUTIL_XmlNode  base;
    CMUTIL_String   *tagname;
    CMUTIL_Map      *attributes;
    CMUTIL_Array    *children;
    CMUTIL_XmlNode  *parent;
    CMUTIL_Mem      *memst;
    void            *udata;
    void            (*freef)(void*);
    CMXmlNodeKind   type;
    int             dummy_padder;
} CMUTIL_XmlNode_Internal;

static CMUTIL_Map *g_cmutil_xml_escape_map = NULL;

void CMUTIL_XmlInit()
{
    const struct CMUTIL_EscapePair *pair = g_cmutil_xml_escapes;
    g_cmutil_xml_escape_map = CMUTIL_MapCreateInternal(
                CMUTIL_GetMem(), 10, CMFalse, NULL,
                0.75f);

    while (pair->key) {
        CMCall(g_cmutil_xml_escape_map, Put, pair->key, pair->val, NULL);
        pair++;
    }
}

void CMUTIL_XmlClear()
{
    CMCall(g_cmutil_xml_escape_map, Destroy);
    g_cmutil_xml_escape_map = NULL;
}

CMUTIL_STATIC CMUTIL_StringArray *CMUTIL_XmlGetAttributeNames(
        const CMUTIL_XmlNode *node)
{
    const CMUTIL_XmlNode_Internal *inode =
            (const CMUTIL_XmlNode_Internal*)node;
    return CMCall(inode->attributes, GetKeys);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_XmlGetAttribute(
        const CMUTIL_XmlNode *node, const char *key)
{
    const CMUTIL_XmlNode_Internal *inode =
            (const CMUTIL_XmlNode_Internal*)node;
    return (CMUTIL_String*)CMCall(inode->attributes, Get, key);
}

CMUTIL_STATIC void CMUTIL_XmlSetAttribute(
        CMUTIL_XmlNode *node, const char *key, const char *value)
{
    CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
    CMUTIL_String *nval = CMUTIL_StringCreateInternal(inode->memst, 0, value);
    CMUTIL_String *oldval = CMCall(inode->attributes, Get, key);
    if (oldval) CMCall(oldval, Destroy);
    if (!CMCall(inode->attributes, Put, key, nval, NULL)) {
        CMCall(nval, Destroy);
        CMLogError("CMUTIL_Map Put failed");
    }
}

CMUTIL_STATIC size_t CMUTIL_XmlChildCount(const CMUTIL_XmlNode *node)
{
    const CMUTIL_XmlNode_Internal *inode =
            (const CMUTIL_XmlNode_Internal*)node;
    return CMCall(inode->children, GetSize);
}

CMUTIL_STATIC void CMUTIL_XmlAddChild(
        CMUTIL_XmlNode *node, CMUTIL_XmlNode *child)
{
    CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
    CMCall(inode->children, Add, child, NULL);
    ((CMUTIL_XmlNode_Internal*)child)->parent = node;
}

CMUTIL_STATIC CMUTIL_XmlNode *CMUTIL_XmlChildAt(
        const CMUTIL_XmlNode *node, uint32_t index)
{
    const CMUTIL_XmlNode_Internal *inode = (const CMUTIL_XmlNode_Internal*)node;
    return (CMUTIL_XmlNode*)CMCall(inode->children, GetAt, index);
}

CMUTIL_STATIC CMUTIL_XmlNode *CMUTIL_XmlGetParent(
        const CMUTIL_XmlNode *node)
{
    const CMUTIL_XmlNode_Internal *inode =
            (const CMUTIL_XmlNode_Internal*)node;
    return inode->parent;
}

CMUTIL_STATIC const char *CMUTIL_XmlGetName(const CMUTIL_XmlNode *node)
{
    const CMUTIL_XmlNode_Internal *inode = (const CMUTIL_XmlNode_Internal*)node;
    return CMCall(inode->tagname, GetCString);
}

CMUTIL_STATIC void CMUTIL_XmlSetName(CMUTIL_XmlNode *node, const char *name)
{
    CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
    CMCall(inode->tagname, Clear);
    CMCall(inode->tagname, AddString, name);
}

CMUTIL_STATIC CMXmlNodeKind CMUTIL_XmlGetType(
        const CMUTIL_XmlNode *node)
{
    const CMUTIL_XmlNode_Internal *inode =
            (const CMUTIL_XmlNode_Internal*)node;
    return inode->type;
}

CMUTIL_STATIC void CMUTIL_XmlEscape(
        CMUTIL_String *dest, CMUTIL_String *src)
{
    const uint8_t *p = (const uint8_t*)CMCall(src, GetCString);
    const uint8_t *stpos = p;
    char c;
    while ((c = (char)*p)) {
        if (g_cmutil_xml_escape[(int)c]) {
            char *q;
            if (stpos < p)
                CMCall(dest, AddNString,
                       (const char*)stpos, (uint32_t)(p - stpos));
            q = g_cmutil_xml_escape_str[(int)c];
            if (q)
                CMCall(dest, AddString, q);
            else
                CMCall(dest, AddChar, c);
            stpos = ++p;
        } else {
            p++;
        }
    }
    if (stpos < p)
        CMCall(dest, AddNString, (const char*)stpos, (uint32_t)(p - stpos));
}

CMUTIL_STATIC void CMUTIL_XmlBuildAttribute(
        CMUTIL_String *buffer, const CMUTIL_XmlNode_Internal *inode)
{
    if (CMCall(inode->attributes, GetSize) > 0) {
        uint32_t i;
        size_t len;
        const CMUTIL_Array *pairs = CMCall(inode->attributes, GetPairs);
        len = CMCall(pairs, GetSize);
        for (i=0; i<len; i++) {
            CMUTIL_MapPair *pair = (CMUTIL_MapPair*)CMCall(pairs, GetAt, i);
            const char *skey = CMCall(pair, GetKey);
            CMUTIL_String *sval = (CMUTIL_String*)CMCall(pair, GetValue);
            CMCall(buffer, AddChar, ' ');
            CMCall(buffer, AddString, skey);
            CMCall(buffer, AddString, "=\"");
            CMUTIL_XmlEscape(buffer, sval);
            CMCall(buffer, AddChar, '\"');
        }
    }
}

#define CMUTIL_XML_LINE_END "\r\n"

CMUTIL_STATIC void CMUTIL_XmlToDocumentPrivate(
        CMUTIL_String *buffer,
        const CMUTIL_XmlNode_Internal *inode,
        CMBool beutify,
        uint32_t depth)
{
    uint32_t i;
    if (inode->type == CMXmlNodeText) {
        for (i = 0; i < depth; i++)
            CMCall(buffer, AddString, "    ");
        CMUTIL_XmlEscape(buffer, inode->tagname);
    } else if (inode->type == CMXmlNodeTag) {
        size_t childcnt = CMCall(inode->children, GetSize);
        const char *tname = CMCall(inode->tagname, GetCString);
        size_t namelen = CMCall(inode->tagname, GetSize);
        for (i = 0; i < depth; i++)
            CMCall(buffer, AddString, "    ");
        CMCall(buffer, AddChar, '<');
        CMCall(buffer, AddNString, tname, namelen);
        CMUTIL_XmlBuildAttribute(buffer, inode);

        if (inode->children && childcnt > 0) {
            CMCall(buffer, AddChar, '>');
            CMCall(buffer, AddNString, CMUTIL_XML_LINE_END, 2);
            for (i=0; i<childcnt; i++) {
                CMUTIL_XmlNode_Internal *child = (CMUTIL_XmlNode_Internal*)
                        CMCall(inode->children, GetAt, i);
                CMUTIL_XmlToDocumentPrivate(buffer, child, beutify, depth + 1);
                CMCall(buffer, AddNString, CMUTIL_XML_LINE_END, 2);
            }
            for (i = 0; i < depth; i++)
                CMCall(buffer, AddChar, '\t');
            CMCall(buffer, AddNString, "</", 2);
            CMCall(buffer, AddNString, tname, namelen);
            CMCall(buffer, AddChar, '>');
        }
        else {
            CMCall(buffer, AddString, "/>");
        }
    }
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_XmlToDocument(
        const CMUTIL_XmlNode *node, CMBool beutify)
{
    const CMUTIL_XmlNode_Internal *inode = (const CMUTIL_XmlNode_Internal*)node;
    CMUTIL_String *res = CMUTIL_StringCreateInternal(
                    inode->memst, 64,
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
    CMUTIL_XmlToDocumentPrivate(res, inode, beutify, 0);
    return res;
}

CMUTIL_STATIC void CMUTIL_XmlSetUserData(
        CMUTIL_XmlNode *node, void *udata, void (*freef)(void*))
{
    CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
    inode->udata = udata;
    inode->freef = freef;
}

CMUTIL_STATIC void *CMUTIL_XmlGetUserData(const CMUTIL_XmlNode *node)
{
    const CMUTIL_XmlNode_Internal *inode =
            (const CMUTIL_XmlNode_Internal*)node;
    return inode->udata;
}

CMUTIL_STATIC void CMUTIL_XmlDestroy(CMUTIL_XmlNode *node)
{
    CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
    if (inode) {
        CMUTIL_Array *children = inode->children;
        if (inode->attributes)
            CMCall(inode->attributes, Destroy);
        if (inode->tagname)
            CMCall(inode->tagname, Destroy);
        if (inode->udata && inode->freef)
            inode->freef(inode->udata);
        inode->memst->Free(inode);

        if (children)
            CMCall(children, Destroy);
    }
}

static CMUTIL_XmlNode g_cmutil_xmlnode = {
    CMUTIL_XmlGetAttributeNames,
    CMUTIL_XmlGetAttribute,
    CMUTIL_XmlSetAttribute,
    CMUTIL_XmlChildCount,
    CMUTIL_XmlAddChild,
    CMUTIL_XmlChildAt,
    CMUTIL_XmlGetParent,
    CMUTIL_XmlGetName,
    CMUTIL_XmlSetName,
    CMUTIL_XmlGetType,
    CMUTIL_XmlToDocument,
    CMUTIL_XmlSetUserData,
    CMUTIL_XmlGetUserData,
    CMUTIL_XmlDestroy
};

CMUTIL_STATIC void CMUTIL_XmlStringDestroyer(void *data)
{
    CMCall((CMUTIL_String*)data, Destroy);
}

CMUTIL_STATIC void CMUTIL_XmlNodeDestroyer(void *data)
{
    CMCall((CMUTIL_XmlNode*)data, Destroy);
}

CMUTIL_XmlNode *CMUTIL_XmlNodeCreateWithLenInternal(
        CMUTIL_Mem *memst,
        CMXmlNodeKind type, const char *tagname, size_t namelen)
{
    CMUTIL_XmlNode_Internal *res =
            memst->Alloc(sizeof(CMUTIL_XmlNode_Internal));
    memset(res, 0x0, sizeof(CMUTIL_XmlNode_Internal));
    memcpy(res, &g_cmutil_xmlnode, sizeof(CMUTIL_XmlNode));
    res->type = type;
    res->memst = memst;
    res->tagname = CMUTIL_StringCreateInternal(memst, namelen, NULL);
    CMCall(res->tagname, AddNString, tagname, namelen);
    res->attributes = CMUTIL_MapCreateInternal(
                memst, 50, CMFalse,
                CMUTIL_XmlStringDestroyer, 0.75f);
    res->children = CMUTIL_ArrayCreateInternal(
                memst, 5, NULL,
                CMUTIL_XmlNodeDestroyer, CMFalse);
    return (CMUTIL_XmlNode*)res;
}

CMUTIL_XmlNode *CMUTIL_XmlNodeCreateInternal(
        CMUTIL_Mem *memst, CMXmlNodeKind type, const char *tagname)
{
    return CMUTIL_XmlNodeCreateWithLenInternal(
                memst, type, tagname, strlen(tagname));
}

CMUTIL_XmlNode *CMUTIL_XmlNodeCreate(
        CMXmlNodeKind type, const char *tagname)
{
    return CMUTIL_XmlNodeCreateInternal(CMUTIL_GetMem(), type, tagname);
}

CMUTIL_XmlNode *CMUTIL_XmlNodeCreateWithLen(
        CMXmlNodeKind type, const char *tagname, size_t namelen)
{
    return CMUTIL_XmlNodeCreateWithLenInternal(
                CMUTIL_GetMem(), type, tagname, namelen);
}

static const char g_cmutil_spaces[]=" \t\r\n";
static const char g_cmutil_delims[]=" \t\r\n<>&=/\"\'";

typedef struct CMUTIL_XmlParseCtx {
    const char *pos;
    int64_t          remain;
    CMUTIL_Array    *stack;
    CMUTIL_CSConv   *cconv;
    CMUTIL_Mem      *memst;
    char            encoding[20];
    int             dummy_padder;
} CMUTIL_XmlParseCtx;

#define DO_BOOL(...) do { if (!(__VA_ARGS__)) { \
    char __buf[50] = {0,};    \
    strncat(__buf, ctx->pos, 40); strcat(__buf, "..."); \
    CMLogError("xml parse failed near '%s'", __buf);  \
    return CMFalse; } } while(0)
#define DO_OBJ(...) do { if (!(__VA_ARGS__)) { \
    char __buf[50] = {0,};\
    strncat(__buf, ctx->pos, 40); strcat(__buf, "..."); \
    CMLogError("xml parse failed near '%s'", __buf);  \
    return NULL; } } while(0)

CMUTIL_STATIC CMBool CMUTIL_XmlUnescape(
        CMUTIL_String *dest, CMUTIL_String *src,
        CMUTIL_XmlParseCtx *ctx)
{
    const char *p = CMCall(src, GetCString), *q, *stpos;
    stpos = p;
    while ((q = strchr(p, '&'))) {
        const char *r = strchr(q+1, ';');
        if (r && (r-q) < 20) {
            char *v, keybuf[50] = {0,};
            if (q > p)
                CMCall(dest, AddNString, p, (uint64_t)(q-p));
            strncat(keybuf, q+1, (uint64_t)(r-q-1));
            v = (char*)CMCall(g_cmutil_xml_escape_map, Get, keybuf);
            if (v)
                CMCall(dest, AddString, v);
            else
                return CMFalse;
            p = r + 1;
        } else {
            return CMFalse;
        }
    }
    if (p && *p) {
        size_t len = CMCall(src, GetSize) - (uint64_t)(p - stpos);
        CMCall(dest, AddNString, p, len);
    }

    if (ctx->cconv) {
        // convert encoding to utf-8
        CMUTIL_String *ndst = CMCall(ctx->cconv, Forward, dest);
        size_t len = CMCall(ndst, GetSize);
        const char *p = CMCall(ndst, GetCString);
        CMCall(dest, Clear);
        CMCall(dest, AddNString, p, len);
        CMCall(ndst, Destroy);
    }

    return CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_XmlSkipSpaces(CMUTIL_XmlParseCtx *ctx)
{
    register const char *p = ctx->pos;
    while (ctx->remain > 0 && *p && strchr(g_cmutil_spaces, *p)) {
        p++; ctx->remain--;
    }
    if (ctx->remain >= 0) {
        ctx->pos = p;
        return CMTrue;
    } else {
        return CMFalse;
    }
}

CMUTIL_STATIC CMBool CMUTIL_XmlNextSub(
        char *buf, CMUTIL_XmlParseCtx *ctx, size_t len)
{
    if (ctx->remain >= (int64_t)len) {
        if (buf) {
            *buf = 0x0;
            strncat(buf, ctx->pos, len);
        }
        ctx->pos += len;
        ctx->remain -= len;
        return CMTrue;
    } else {
        return CMFalse;
    }
}

CMUTIL_STATIC CMBool CMUTIL_XmlNextToken(
        char *buf, CMUTIL_XmlParseCtx *ctx)
{
    register const char *p = ctx->pos;
    register char *q = buf;
    while (ctx->remain > 0 && *p && !strchr(g_cmutil_delims, *p)) {
        *q++ = *p++; ctx->remain--;
    }
    if (ctx->remain >= 0) {
        *q = 0x0;
        ctx->pos = p;
        return CMTrue;
    } else {
        return CMFalse;
    }
}

CMUTIL_STATIC CMBool CMUTIL_XmlParseHeader(CMUTIL_XmlParseCtx *ctx)
{
    char buf[20];
    DO_BOOL(CMUTIL_XmlSkipSpaces(ctx));
    DO_BOOL(CMUTIL_XmlNextSub(buf, ctx, 2));
    if (strcmp(buf, "<?") == 0) {
        DO_BOOL(CMUTIL_XmlSkipSpaces(ctx));
        DO_BOOL(CMUTIL_XmlNextToken(buf, ctx));
        // 'buf' may contain 'xml'
        DO_BOOL(CMUTIL_XmlSkipSpaces(ctx));
        while (*(ctx->pos) != '?') {
            CMBool isenc;
            DO_BOOL(CMUTIL_XmlNextToken(buf, ctx));
            isenc = strcmp(buf, "encoding") == 0?
                    CMTrue:CMFalse;
            DO_BOOL(CMUTIL_XmlNextSub(NULL, ctx, 2));
            DO_BOOL(CMUTIL_XmlNextToken(buf, ctx));
            if (isenc) {
                const char *p = strchr(ctx->pos, '?');
                if (!p) return CMFalse;
                strcpy(ctx->encoding, buf);
                CMUTIL_XmlNextSub(NULL, ctx, (uint64_t)(p - ctx->pos));
            } else {
                CMUTIL_XmlNextSub(NULL, ctx, 1);
            }
        }
        DO_BOOL(CMUTIL_XmlNextSub(buf, ctx, 2));
        return CMTrue;
    } else {
        CMLogErrorS("%s", "xml header is invalid");
        return CMFalse;
    }
}

CMUTIL_STATIC CMBool CMUTIL_XmlNextChar(
        CMUTIL_XmlParseCtx *ctx, char *c)
{
    if (ctx->remain > 0) {
        *c = *(ctx->pos++);
        ctx->remain--;
        return CMTrue;
    } else {
        return CMFalse;
    }
}

CMUTIL_STATIC CMBool CMUTIL_XmlStartsWith(
        const char *a, const char *b)
{
    while (*b && *a == *b) {
        a++; b++;
    }
    return *b? CMFalse:CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_XmlParseAttributes(CMUTIL_XmlParseCtx *ctx)
{
    CMUTIL_XmlNode *node =
            (CMUTIL_XmlNode*)CMCall(ctx->stack, Top);
    CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
    DO_BOOL(CMUTIL_XmlSkipSpaces(ctx));
    while (!strchr("/>", *(ctx->pos))) {
        char attname[1024], attvalue[1024], openc;
        const char *p;
        CMUTIL_String *bfval, *afval;
        DO_BOOL(CMUTIL_XmlNextToken(attname, ctx));
        DO_BOOL(CMUTIL_XmlSkipSpaces(ctx));

        // may current ctx->pos indicates '='
        if ('=' == *(ctx->pos)) {
            DO_BOOL(CMUTIL_XmlNextSub(NULL, ctx, 1));
            DO_BOOL(CMUTIL_XmlSkipSpaces(ctx));
            DO_BOOL(CMUTIL_XmlNextChar(ctx, &openc));
            p = strchr(ctx->pos, openc);
            if (!p) {
                CMLogErrorS("invalid xml");
                return CMFalse;
            }
            *attvalue = 0x0;
            strncat(attvalue, ctx->pos, (uint64_t)(p - ctx->pos));
            *(attvalue + (uint64_t)(p - ctx->pos)) = 0x0;
            DO_BOOL(CMUTIL_XmlNextSub(NULL, ctx, (uint64_t)(p - ctx->pos + 1)));
        } else {
            // assume attribute value is 'true' if there is no value part.
            strcpy(attvalue, "true");
        }
        bfval = CMUTIL_StringCreateInternal(ctx->memst, 0, attvalue);
        afval = CMUTIL_StringCreateInternal(
                    ctx->memst, CMCall(bfval, GetSize), NULL);
        CMUTIL_XmlUnescape(afval, bfval, ctx);
        CMCall(bfval, Destroy); bfval = NULL;
        CMCall(inode->attributes, Put, attname, afval, (void**)&bfval);
        if (bfval) CMCall(bfval, Destroy);
        DO_BOOL(CMUTIL_XmlSkipSpaces(ctx));
    }
    return CMTrue;
}

CMUTIL_STATIC CMUTIL_XmlNode *CMUTIL_XmlParseNode(
        CMUTIL_XmlParseCtx *ctx, CMBool *isClose)
{
    char tagname[128];
    CMUTIL_XmlNode *child = NULL;
    CMUTIL_XmlNode *parent =
            (CMUTIL_XmlNode*)CMCall(ctx->stack, Top);
PARSE_NEXT:
    DO_OBJ(CMUTIL_XmlSkipSpaces(ctx));
    if (*(ctx->pos) == '<') {
        const char *p;
        char c;
        DO_OBJ(CMUTIL_XmlNextChar(ctx, &c));
        switch (*(ctx->pos)) {
        case '/':
            // parse close tag
            p = strchr(ctx->pos, '>');
            if (p) {
                CMUTIL_XmlNextSub(NULL, ctx, (uint64_t)(p - ctx->pos + 1));
                if (isClose)
                    *isClose = CMTrue;
                return parent;
            } else {
                CMLogErrorS("invalid xml: invalid close tag");
                return NULL;
            }
        case '!':
            // cdata or ignored.
            DO_OBJ(CMUTIL_XmlNextChar(ctx, &c));
            if (CMUTIL_XmlStartsWith(ctx->pos, "[CDATA[")) {
                // create text node
                CMUTIL_XmlNextSub(NULL, ctx, 7);
                p = strstr(ctx->pos, "]]>");
                if (!p) return NULL;
                if (parent) {
                    CMUTIL_String *bfval, *afval;
                    bfval = CMUTIL_StringCreateInternal(ctx->memst, 10, NULL);
                    CMCall(bfval, AddNString,
                            ctx->pos, (uint64_t)(p - ctx->pos));
                    afval = CMUTIL_StringCreateInternal(
                            ctx->memst, CMCall(bfval, GetSize), NULL);
                    CMUTIL_XmlUnescape(afval, bfval, ctx);
                    CMCall(bfval, Destroy);
                    child = CMUTIL_XmlNodeCreateWithLenInternal(
                                ctx->memst, CMXmlNodeText,
                                CMCall(afval, GetCString),
                                CMCall(afval, GetSize));
                    CMCall(afval, Destroy);
                    CMCall(parent, AddChild, child);
                    CMUTIL_XmlNextSub(NULL, ctx, (uint64_t)(p - ctx->pos + 3));
                    return child;
                } else {
                    CMLogErrorS("invalid xml: CDATA with no parent");
                    return NULL;
                }
            } else {
                // ignore contents.
                const char *p = strchr(ctx->pos+1, '>');
                if (!p) {
                    CMLogErrorS("invalid xml: no close tag");
                    return NULL;
                }
                CMUTIL_XmlNextSub(NULL, ctx, (uint64_t)(p - ctx->pos + 1));
                goto PARSE_NEXT;
            }
        default:
            // may be normal node
            DO_OBJ(CMUTIL_XmlNextToken(tagname, ctx));
            child = CMUTIL_XmlNodeCreateInternal(
                        ctx->memst, CMXmlNodeTag, tagname);
            if (parent)
                CMCall(parent, AddChild, child);
            CMCall(ctx->stack, Push, child);
            DO_OBJ(CMUTIL_XmlParseAttributes(ctx));
            if (CMUTIL_XmlStartsWith(ctx->pos, "/>")) {
                // no children
                CMUTIL_XmlNextSub(NULL, ctx, 2);
                CMCall(ctx->stack, Pop);
                return child;
            } else if (*(ctx->pos) == '>') {
                CMBool closed = CMFalse;
                CMUTIL_XmlNextSub(NULL, ctx, 1);
                while (!closed)
                    if (!CMUTIL_XmlParseNode(ctx, &closed))
                        return NULL;
                CMCall(ctx->stack, Pop);
                return child;
            } else {
                CMCall(child, Destroy);
                CMLogErrorS("invalid xml");
                return NULL;
            }
        }
    } else if (parent) {
        // text node
        CMUTIL_String *bfval, *afval;
        const char *p = strchr (ctx->pos, '<');
        size_t textlen = (uint64_t)(p - ctx->pos);
        bfval = CMUTIL_StringCreateInternal(ctx->memst, 10, NULL);
        CMCall(bfval, AddNString, ctx->pos, textlen);
        afval = CMUTIL_StringCreateInternal(
                    ctx->memst, CMCall(bfval, GetSize), NULL);
        CMUTIL_XmlUnescape(afval, bfval, ctx);
        CMCall(bfval, Destroy);
        child = CMUTIL_XmlNodeCreateWithLenInternal(
                        ctx->memst, CMXmlNodeText,
                        CMCall(afval, GetCString),
                        CMCall(afval, GetSize));
        CMCall(afval, Destroy);
        CMCall(parent, AddChild, child);
        CMUTIL_XmlNextSub(NULL, ctx, textlen);
        return child;
    } else {
        CMLogErrorS("invalid xml");
        return NULL;
    }
}

CMUTIL_XmlNode *CMUTIL_XmlParseStringInternal(
        CMUTIL_Mem *memst, const char *xmlstr, size_t len)
{
    CMUTIL_XmlParseCtx ctx;
    memset(&ctx, 0x0, sizeof(CMUTIL_XmlParseCtx));
    ctx.pos = xmlstr;
    ctx.remain = (int64_t)len;
    ctx.memst = memst;
    strcpy(ctx.encoding, "UTF-8");
    if (CMUTIL_XmlParseHeader(&ctx)) {
        CMUTIL_XmlNode *res, *bottom;
        if (*(ctx.encoding) && strcasecmp(ctx.encoding, "UTF-8") != 0)
            ctx.cconv = CMUTIL_CSConvCreateInternal(
                        memst, ctx.encoding, "UTF-8");
        ctx.stack = CMUTIL_ArrayCreateInternal(memst, 3, NULL, NULL, CMFalse);
        res = CMUTIL_XmlParseNode(&ctx, NULL);
        bottom = (CMUTIL_XmlNode*)CMCall(ctx.stack, Bottom);
        if (bottom) {
            CMLogErrorS("invalid xml: stack not empty");
            CMCall(bottom, Destroy);
            res = NULL;
        }
        if (ctx.cconv)
            CMCall(ctx.cconv, Destroy);
        CMCall(ctx.stack, Destroy);
        return res;
    } else {
        CMLogErrorS("invalid xml: invalid header");
        return NULL;
    }
}

CMUTIL_XmlNode *CMUTIL_XmlParseString(const char *xmlstr, size_t len)
{
    return CMUTIL_XmlParseStringInternal(CMUTIL_GetMem(), xmlstr, len);
}

CMUTIL_XmlNode *CMUTIL_XmlParseInternal(
        CMUTIL_Mem *memst, CMUTIL_String *str)
{
    return CMUTIL_XmlParseStringInternal(
                memst, CMCall(str, GetCString),
                CMCall(str, GetSize));
}

CMUTIL_XmlNode *CMUTIL_XmlParse(CMUTIL_String *str)
{
    return CMUTIL_XmlParseInternal(CMUTIL_GetMem(), str);
}

CMUTIL_XmlNode *CMUTIL_XmlParseFileInternal(
        CMUTIL_Mem *memst, const char *fpath)
{
    FILE *f;
    char buffer[1024];
    f = fopen(fpath, "rb");
    if (f) {
        CMUTIL_String *fcont = CMUTIL_StringCreateInternal(memst, 1024, NULL);
        CMUTIL_XmlNode *res = NULL;
        size_t rsz;
        while ((rsz = fread(buffer, 1, 1024, f)))
            CMCall(fcont, AddNString, buffer, rsz);
        res = CMUTIL_XmlParseInternal(memst, fcont);
        CMCall(fcont, Destroy);
        return res;
    } else {
        CMLogErrorS("cannot read file: %s", fpath);
        return NULL;
    }
}

CMUTIL_XmlNode *CMUTIL_XmlParseFile(const char *fpath)
{
    return CMUTIL_XmlParseFileInternal(CMUTIL_GetMem(), fpath);
}

typedef struct CMUTIL_XmlToJsonCtx {
    CMUTIL_Array *stack;
    CMUTIL_Mem *memst;
} CMUTIL_XmlToJsonCtx;

CMUTIL_STATIC CMUTIL_Json *CMUTIL_XmlToJsonInternal(
        CMUTIL_XmlNode *node, CMUTIL_XmlToJsonCtx *ctx)
{
    uint32_t i, ccnt;
    CMUTIL_Json *prev;
    CMUTIL_Json *res = NULL;
    CMUTIL_JsonObject *parent;
    CMUTIL_StringArray *attrnames;
    CMBool isattr = CMFalse;
    const char *name = CMCall(node, GetName);
    if (CMCall(ctx->stack, GetSize) == 0) {
        parent = CMUTIL_JsonObjectCreateInternal(ctx->memst);
        CMCall(ctx->stack, Push, parent);
    } else {
        parent = (CMUTIL_JsonObject*)CMCall(ctx->stack, Top);
    }

    // set attributes to json object
    attrnames = CMCall(node, GetAttributeNames);
    if (attrnames != NULL && CMCall(attrnames, GetSize) > 0) {
        CMUTIL_JsonObject *ores = CMUTIL_JsonObjectCreateInternal(ctx->memst);
        for (i=0; i<CMCall(attrnames, GetSize); i++) {
            const CMUTIL_String *aname = CMCall(attrnames, GetAt, i);
            const char *sname = CMCall(aname, GetCString);
            CMUTIL_String *value = CMCall(node, GetAttribute, sname);
            const char *svalue = CMCall(value, GetCString);
            prev = CMCall(ores, Get, sname);
            if (prev) {
                if (CMCall(prev, GetType) == CMJsonTypeArray) {
                    CMCall((CMUTIL_JsonArray*)prev, AddString, svalue);
                } else {
                    CMUTIL_JsonArray *arr =
                            CMUTIL_JsonArrayCreateInternal(ctx->memst);
                    prev = CMCall(ores, Remove, sname);
                    CMCall(arr, Add, prev);
                    CMCall(arr, AddString, svalue);
                    CMCall(ores, Put, sname, (CMUTIL_Json*)arr);
                }
            } else {
                CMCall(ores, PutString, sname, svalue);
            }
        }
        res = (CMUTIL_Json*)ores;
    }
    if (CMCall(node, ChildCount) == 1) {
        CMUTIL_XmlNode *child = CMCall(node, ChildAt, 0);
        if (CMXmlNodeText == CMCall(child, GetType)) {
            const char *text = CMCall(child, GetName);
            text = CMUTIL_StrTrim((char*)text);
            if (strlen(text) > 0) {
                CMUTIL_JsonValue *val = CMUTIL_JsonValueCreate();
                CMCall(val, SetString, text);
                if (res == NULL) {
                    res = (CMUTIL_Json*)val;
                } else {
                    CMCall((CMUTIL_JsonObject*)res, Put, "content", (CMUTIL_Json*)val);
                }
                isattr = CMTrue;
            }
        }
    }
    if (res == NULL)
        res = (CMUTIL_Json*)CMUTIL_JsonObjectCreate();
    if (attrnames)
        CMCall(attrnames, Destroy);

    ccnt = (uint32_t)CMCall(node, ChildCount);
    if (ccnt > 0 && !isattr) {
        // set children to json object
        CMCall(ctx->stack, Push, res);
        for (i=0; i<ccnt; i++)
            CMUTIL_XmlToJsonInternal(CMCall(node, ChildAt, i), ctx);
        CMCall(ctx->stack, Pop);
    }
    prev = CMCall(parent, Get, name);
    if (prev) {
        if (CMCall(prev, GetType) == CMJsonTypeArray) {
            CMCall((CMUTIL_JsonArray*)prev, Add, res);
        } else {
            CMUTIL_JsonArray *arr =
                    CMUTIL_JsonArrayCreateInternal(ctx->memst);
            prev = CMCall(parent, Remove, name);
            CMCall(arr, Add, prev);
            CMCall(arr, Add, res);
            CMCall(parent, Put, name, (CMUTIL_Json*)arr);
        }
    } else {
        CMCall(parent, Put, name, res);
    }
    return (CMUTIL_Json*)res;
}

CMUTIL_Json *CMUTIL_XmlToJson(CMUTIL_XmlNode *node)
{
    CMUTIL_Json *res;
    CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
    CMUTIL_XmlToJsonCtx ctx;
    ctx.stack = CMUTIL_ArrayCreateInternal(
                inode->memst, 5, NULL, NULL, CMFalse);
    ctx.memst = inode->memst;
    CMUTIL_XmlToJsonInternal(node, &ctx);
    res = (CMUTIL_Json*)CMCall(ctx.stack, Pop);
    CMCall(ctx.stack, Destroy);
    return res;
}
