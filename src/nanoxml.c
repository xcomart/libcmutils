
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
} g_cmutil_xml_escapes[] = {
		{"quot", "\""},
		{"amp", "&"},
		{"apos", "\'"},
		{"lt", "<"},
		{"gt", ">"},
		{NULL, NULL}
};

typedef struct CMUTIL_XmlNode_Internal {
	CMUTIL_XmlNode		base;
	CMUTIL_String		*tagname;
	CMUTIL_XmlNodeKind	type;
	CMUTIL_Map			*attributes;
	CMUTIL_Array		*children;
	CMUTIL_XmlNode		*parent;
	CMUTIL_Mem_st		*memst;
	void				*udata;
	void				(*freef)(void*);
} CMUTIL_XmlNode_Internal;

static CMUTIL_Map *g_cmutil_xml_escape_map = NULL;

void CMUTIL_XmlInit()
{
	struct CMUTIL_EscapePair *pair = g_cmutil_xml_escapes;
	g_cmutil_xml_escape_map = CMUTIL_MapCreateInternal(__CMUTIL_Mem, 10, NULL);

	while (pair->key) {
		CMUTIL_CALL(g_cmutil_xml_escape_map, Put, pair->key, pair->val);
		pair++;
	}
}

void CMUTIL_XmlClear()
{
	CMUTIL_CALL(g_cmutil_xml_escape_map, Destroy);
	g_cmutil_xml_escape_map = NULL;
}

CMUTIL_STATIC CMUTIL_StringArray *CMUTIL_XmlGetAttributeNames(
		CMUTIL_XmlNode *node)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	return CMUTIL_CALL(inode->attributes, GetKeys);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_XmlGetAttribute(
		CMUTIL_XmlNode *node, const char *key)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	return (CMUTIL_String*)CMUTIL_CALL(inode->attributes, Get, key);
}

CMUTIL_STATIC void CMUTIL_XmlSetAttribute(
		CMUTIL_XmlNode *node, const char *key, const char *value)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	CMUTIL_CALL(inode->attributes, Put, key,
				CMUTIL_StringCreateInternal(inode->memst, 10, value));
}

CMUTIL_STATIC int CMUTIL_XmlChildCount(CMUTIL_XmlNode *node)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	return CMUTIL_CALL(inode->children, GetSize);
	return 0;
}

CMUTIL_STATIC void CMUTIL_XmlAddChild(
		CMUTIL_XmlNode *node, CMUTIL_XmlNode *child)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	CMUTIL_CALL(inode->children, Add, child);
	((CMUTIL_XmlNode_Internal*)child)->parent = node;
}

CMUTIL_STATIC CMUTIL_XmlNode *CMUTIL_XmlChildAt(
		CMUTIL_XmlNode *node, int index)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	return (CMUTIL_XmlNode*)CMUTIL_CALL(inode->children, GetAt, index);
}

CMUTIL_STATIC CMUTIL_XmlNode *CMUTIL_XmlGetParent(
		CMUTIL_XmlNode *node)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	return inode->parent;
}

CMUTIL_STATIC const char *CMUTIL_XmlGetName(CMUTIL_XmlNode *node)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	return CMUTIL_CALL(inode->tagname, GetCString);
}

CMUTIL_STATIC void CMUTIL_XmlSetName(CMUTIL_XmlNode *node, const char *name)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	CMUTIL_CALL(inode->tagname, Clear);
	CMUTIL_CALL(inode->tagname, AddString, name);
}

CMUTIL_STATIC CMUTIL_XmlNodeKind CMUTIL_XmlGetType(CMUTIL_XmlNode *node)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	return inode->type;
}

CMUTIL_STATIC void CMUTIL_XmlEscape(
		CMUTIL_String *dest, CMUTIL_String *src)
{
	const unsigned char *p = (const unsigned char*)CMUTIL_CALL(src, GetCString);
	const unsigned char *stpos = p;
	char c;
	while ((c = (char)*p)) {
		if (g_cmutil_xml_escape[(int)c]) {
			char *q;
			if (stpos < p)
				CMUTIL_CALL(dest, AddNString, (char*)stpos, (int)(p - stpos));
			q = g_cmutil_xml_escape_str[(int)c];
			if (q)
				CMUTIL_CALL(dest, AddString, q);
			else
				CMUTIL_CALL(dest, AddChar, c);
			stpos = ++p;
		} else {
			p++;
		}
	}
	if (stpos > p)
		CMUTIL_CALL(dest, AddNString, (char*)stpos, (int)(p - stpos));
}

CMUTIL_STATIC void CMUTIL_XmlBuildAttribute(
		CMUTIL_String *buffer, CMUTIL_XmlNode_Internal *inode)
{
	if (CMUTIL_CALL(inode->attributes, GetSize) > 0) {
		int i, len;
		CMUTIL_StringArray *keys = CMUTIL_CALL(inode->attributes, GetKeys);
		len = CMUTIL_CALL(keys, GetSize);
		for (i=0; i<len; i++) {
			CMUTIL_String *key = CMUTIL_CALL(keys, GetAt, i);
			const char *skey = CMUTIL_CALL(key, GetCString);
			CMUTIL_CALL(buffer, AddChar, ' ');
			CMUTIL_CALL(buffer, AddAnother, key);
			CMUTIL_CALL(buffer, AddString, "=\"");
			CMUTIL_XmlEscape(buffer, (CMUTIL_String*)
					CMUTIL_CALL(inode->attributes, Get, skey));
			CMUTIL_CALL(buffer, AddChar, '\"');
		}
		if (keys)
			CMUTIL_CALL(keys, Destroy);
	}
}

#define CMUTIL_XML_LINE_END		"\r\n"

CMUTIL_STATIC void CMUTIL_XmlToDocumentPrivate(
		CMUTIL_String *buffer,
		CMUTIL_XmlNode_Internal *inode,
		CMUTIL_Bool beutify,
		int depth)
{
	if (inode->type == CMUTIL_XmlNodeText) {
		CMUTIL_XmlEscape(buffer, inode->tagname);
	} else if (inode->type == CMUTIL_XmlNodeTag) {
		int i, childcnt = CMUTIL_CALL(inode->children, GetSize);
		const char *tname = CMUTIL_CALL(inode->tagname, GetCString);
		int namelen = CMUTIL_CALL(inode->tagname, GetSize);
		for (i = 0; i < depth; i++)
			CMUTIL_CALL(buffer, AddChar, '\t');
		CMUTIL_CALL(buffer, AddChar, '<');
		CMUTIL_CALL(buffer, AddNString, tname, namelen);
		CMUTIL_XmlBuildAttribute(buffer, inode);

		if (inode->children && childcnt > 0) {
			CMUTIL_CALL(buffer, AddChar, '>');
			CMUTIL_CALL(buffer, AddNString, CMUTIL_XML_LINE_END, 2);
			for (i=0; i<childcnt; i++) {
				CMUTIL_XmlNode_Internal *child = (CMUTIL_XmlNode_Internal*)
						CMUTIL_CALL(inode->children, GetAt, i);
				CMUTIL_XmlToDocumentPrivate(buffer, child, beutify, depth + 1);
				CMUTIL_CALL(buffer, AddNString, CMUTIL_XML_LINE_END, 2);
			}
			for (i = 0; i < depth; i++)
				CMUTIL_CALL(buffer, AddChar, '\t');
			CMUTIL_CALL(buffer, AddNString, "</", 2);
			CMUTIL_CALL(buffer, AddNString, tname, namelen);
			CMUTIL_CALL(buffer, AddChar, '>');
		}
		else {
			CMUTIL_CALL(buffer, AddString, "/>");
		}
	}
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_XmlToDocument(
		CMUTIL_XmlNode *node, CMUTIL_Bool beutify)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
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

CMUTIL_STATIC void *CMUTIL_XmlGetUserData(CMUTIL_XmlNode *node)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	return inode->udata;
}

CMUTIL_STATIC void CMUTIL_XmlDestroy(CMUTIL_XmlNode *node)
{
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	if (inode) {
		CMUTIL_Array *children = inode->children;
		if (inode->attributes)
			CMUTIL_CALL(inode->attributes, Destroy);
		if (inode->tagname)
			CMUTIL_CALL(inode->tagname, Destroy);
		if (inode->udata && inode->freef)
			inode->freef(inode->udata);
		inode->memst->Free(inode);

		if (children)
			CMUTIL_CALL(children, Destroy);
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
	CMUTIL_CALL((CMUTIL_String*)data, Destroy);
}

CMUTIL_STATIC void CMUTIL_XmlNodeDestroyer(void *data)
{
	CMUTIL_CALL((CMUTIL_XmlNode*)data, Destroy);
}

CMUTIL_XmlNode *CMUTIL_XmlNodeCreateWithLenInternal(
		CMUTIL_Mem_st *memst,
		CMUTIL_XmlNodeKind type, const char *tagname, int namelen)
{
	CMUTIL_XmlNode_Internal *res =
			memst->Alloc(sizeof(CMUTIL_XmlNode_Internal));
	memset(res, 0x0, sizeof(CMUTIL_XmlNode_Internal));
	memcpy(res, &g_cmutil_xmlnode, sizeof(CMUTIL_XmlNode));
	res->type = type;
	res->memst = memst;
	res->tagname = CMUTIL_StringCreateInternal(memst, namelen, NULL);
	CMUTIL_CALL(res->tagname, AddNString, tagname, namelen);
	res->attributes = CMUTIL_MapCreateInternal(
				memst, 50, CMUTIL_XmlStringDestroyer);
	res->children = CMUTIL_ArrayCreateInternal(
				memst, 5, NULL, CMUTIL_XmlNodeDestroyer);
	return (CMUTIL_XmlNode*)res;
}

CMUTIL_XmlNode *CMUTIL_XmlNodeCreateInternal(
		CMUTIL_Mem_st *memst, CMUTIL_XmlNodeKind type, const char *tagname)
{
	return CMUTIL_XmlNodeCreateWithLenInternal(
				memst, type, tagname, (int)strlen(tagname));
}

CMUTIL_XmlNode *CMUTIL_XmlNodeCreate(
		CMUTIL_XmlNodeKind type, const char *tagname)
{
	return CMUTIL_XmlNodeCreateInternal(__CMUTIL_Mem, type, tagname);
}

CMUTIL_XmlNode *CMUTIL_XmlNodeCreateWithLen(
		CMUTIL_XmlNodeKind type, const char *tagname, int namelen)
{
	return CMUTIL_XmlNodeCreateWithLenInternal(
				__CMUTIL_Mem, type, tagname, namelen);
}

static const char g_cmutil_spaces[]=" \t\r\n";
static const char g_cmutil_delims[]=" \t\r\n<>&=/\"\'";

typedef struct CMUTIL_XmlParseCtx {
	const char *pos;
	char encoding[20];
	int remain;
	CMUTIL_Array *stack;
	CMUTIL_CSConv *cconv;
	CMUTIL_Mem_st *memst;
} CMUTIL_XmlParseCtx;

#define DO_BOOL(...) do { if (!(__VA_ARGS__)) {	\
	char buf[50] = {0,};\
	strncat(buf, ctx->pos, 40); strcat(buf, "...");	\
	CMLogErrorS("xml parse failed near '%s'", buf);	\
	return CMUTIL_False; } } while(0)
#define DO_OBJ(...) do { if (!(__VA_ARGS__)) { \
	char buf[50] = {0,};\
	strncat(buf, ctx->pos, 40); strcat(buf, "...");	\
	CMLogErrorS("xml parse failed near '%s'", buf);	\
	return NULL; } } while(0)

CMUTIL_STATIC CMUTIL_Bool CMUTIL_XmlUnescape(
		CMUTIL_String *dest, CMUTIL_String *src,
		CMUTIL_XmlParseCtx *ctx)
{
	const char *p = CMUTIL_CALL(src, GetCString), *q, *stpos;
	stpos = p;
	while ((q = strchr(p, '&'))) {
		const char *r = strchr(q+1, ';');
		if (r && (r-q) < 20) {
			char *v, keybuf[50] = {0,};
			if (q > p)
				CMUTIL_CALL(dest, AddNString, p, (int)(q-p));
			strncat(keybuf, q+1, r-q-1);
			v = (char*)CMUTIL_CALL(g_cmutil_xml_escape_map, Get, keybuf);
			if (v)
				CMUTIL_CALL(dest, AddString, v);
			else
				return CMUTIL_False;
			p = r + 1;
		} else {
			return CMUTIL_False;
		}
	}
	if (p && *p) {
		int len = (int)(CMUTIL_CALL(src, GetSize) - (p - stpos));
		CMUTIL_CALL(dest, AddNString, p, len);
	}

	if (ctx->cconv) {
		// convert encoding to utf-8
		CMUTIL_String *ndst = CMUTIL_CALL(ctx->cconv, Forward, dest);
		int len = CMUTIL_CALL(ndst, GetSize);
		const char *p = CMUTIL_CALL(ndst, GetCString);
		CMUTIL_CALL(dest, Clear);
		CMUTIL_CALL(dest, AddNString, p, len);
		CMUTIL_CALL(ndst, Destroy);
	}

	return CMUTIL_True;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_XmlSkipSpaces(CMUTIL_XmlParseCtx *ctx)
{
	register const char *p = ctx->pos;
	while (ctx->remain > 0 && *p && strchr(g_cmutil_spaces, *p))
		p++, ctx->remain--;
	if (ctx->remain >= 0) {
		ctx->pos = p;
		return CMUTIL_True;
	} else {
		return CMUTIL_False;
	}
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_XmlNextSub(
		char *buf, CMUTIL_XmlParseCtx *ctx, int len)
{
	if (ctx->remain >= len) {
		if (buf) {
			*buf = 0x0;
			strncat(buf, ctx->pos, len);
		}
		ctx->pos += len;
		ctx->remain -= len;
		return CMUTIL_True;
	} else {
		return CMUTIL_False;
	}
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_XmlNextToken(
		char *buf, CMUTIL_XmlParseCtx *ctx)
{
	register const char *p = ctx->pos;
	register char *q = buf;
	while (ctx->remain > 0 && *p && !strchr(g_cmutil_delims, *p))
		*q++ = *p++, ctx->remain--;
	if (ctx->remain >= 0) {
		*q = 0x0;
		ctx->pos = p;
		return CMUTIL_True;
	} else {
		return CMUTIL_False;
	}
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_XmlParseHeader(CMUTIL_XmlParseCtx *ctx)
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
			CMUTIL_Bool isenc;
			DO_BOOL(CMUTIL_XmlNextToken(buf, ctx));
			isenc = strcmp(buf, "encoding") == 0?
					CMUTIL_True:CMUTIL_False;
			DO_BOOL(CMUTIL_XmlNextSub(NULL, ctx, 2));
			DO_BOOL(CMUTIL_XmlNextToken(buf, ctx));
			if (isenc) {
				const char *p = strchr(ctx->pos, '?');
				if (!p) return CMUTIL_False;
				strcpy(ctx->encoding, buf);
				CMUTIL_XmlNextSub(NULL, ctx, (int)(p - ctx->pos));
			} else {
				CMUTIL_XmlNextSub(NULL, ctx, 1);
			}
		}
		DO_BOOL(CMUTIL_XmlNextSub(buf, ctx, 2));
		return CMUTIL_True;
	} else {
		CMLogErrorS("%s", "xml header is invalid");
		return CMUTIL_False;
	}
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_XmlNextChar(
		CMUTIL_XmlParseCtx *ctx, char *c)
{
	if (ctx->remain > 0) {
		*c = *(ctx->pos++);
		ctx->remain--;
		return CMUTIL_True;
	} else {
		return CMUTIL_False;
	}
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_XmlStartsWith(
		const char *a, const char *b)
{
	while (*b && *a == *b) a++, b++;
	return *b? CMUTIL_False:CMUTIL_True;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_XmlParseAttributes(CMUTIL_XmlParseCtx *ctx)
{
	CMUTIL_XmlNode *node =
			(CMUTIL_XmlNode*)CMUTIL_CALL(ctx->stack, Top);
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	DO_BOOL(CMUTIL_XmlSkipSpaces(ctx));
	while (!strchr("/>", *(ctx->pos))) {
		char attname[1024], attvalue[1024], openc;
		const char *p;
		CMUTIL_String *bfval, *afval;
		DO_BOOL(CMUTIL_XmlNextToken(attname, ctx));
		DO_BOOL(CMUTIL_XmlSkipSpaces(ctx));

		// may current ctx->pos indicates '='
		if ('=' == *(ctx->pos))	{
			DO_BOOL(CMUTIL_XmlNextSub(NULL, ctx, 1));
			DO_BOOL(CMUTIL_XmlSkipSpaces(ctx));
			DO_BOOL(CMUTIL_XmlNextChar(ctx, &openc));
			p = strchr(ctx->pos, openc);
			if (!p) {
				CMLogErrorS("invalid xml");
				return CMUTIL_False;
			}
			*attvalue = 0x0;
			strncat(attvalue, ctx->pos, p - ctx->pos);
			DO_BOOL(CMUTIL_XmlNextSub(NULL, ctx, (int)(p - ctx->pos + 1)));
		} else {
			// assume attribute value is 'true' if there is no value part.
			strcpy(attvalue, "true");
		}
		bfval = CMUTIL_StringCreateInternal(ctx->memst, 10, attvalue);
		afval = CMUTIL_StringCreateInternal(
					ctx->memst, CMUTIL_CALL(bfval, GetSize), NULL);
		CMUTIL_XmlUnescape(afval, bfval, ctx);
		CMUTIL_CALL(inode->attributes, Put, attname, afval);
		CMUTIL_CALL(bfval, Destroy);
		DO_BOOL(CMUTIL_XmlSkipSpaces(ctx));
	}
	return CMUTIL_True;
}

CMUTIL_STATIC CMUTIL_XmlNode *CMUTIL_XmlParseNode(
		CMUTIL_XmlParseCtx *ctx, CMUTIL_Bool *isClose)
{
	char tagname[128];
	CMUTIL_XmlNode *child = NULL;
	CMUTIL_XmlNode *parent =
			(CMUTIL_XmlNode*)CMUTIL_CALL(ctx->stack, Top);
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
				CMUTIL_XmlNextSub(NULL, ctx, (int)(p - ctx->pos + 1));
				if (isClose)
					*isClose = CMUTIL_True;
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
					CMUTIL_CALL(bfval, AddNString,
							ctx->pos, (int)(p - ctx->pos));
					afval = CMUTIL_StringCreateInternal(
							ctx->memst, CMUTIL_CALL(bfval, GetSize), NULL);
					CMUTIL_XmlUnescape(afval, bfval, ctx);
					CMUTIL_CALL(bfval, Destroy);
					child = CMUTIL_XmlNodeCreateWithLenInternal(
								ctx->memst, CMUTIL_XmlNodeText,
								CMUTIL_CALL(afval, GetCString),
								CMUTIL_CALL(afval, GetSize));
					CMUTIL_CALL(afval, Destroy);
					CMUTIL_CALL(parent, AddChild, child);
					CMUTIL_XmlNextSub(NULL, ctx, (int)(p - ctx->pos + 3));
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
				CMUTIL_XmlNextSub(NULL, ctx, (int)(p - ctx->pos + 1));
				goto PARSE_NEXT;
			}
			break;
		default:
			// may be normal node
			DO_OBJ(CMUTIL_XmlNextToken(tagname, ctx));
			child = CMUTIL_XmlNodeCreateInternal(
						ctx->memst, CMUTIL_XmlNodeTag, tagname);
			if (parent)
				CMUTIL_CALL(parent, AddChild, child);
			CMUTIL_CALL(ctx->stack, Push, child);
			DO_OBJ(CMUTIL_XmlParseAttributes(ctx));
			if (CMUTIL_XmlStartsWith(ctx->pos, "/>")) {
				// no children
				CMUTIL_XmlNextSub(NULL, ctx, 2);
				CMUTIL_CALL(ctx->stack, Pop);
				return child;
			} else if (*(ctx->pos) == '>') {
				CMUTIL_Bool closed = CMUTIL_False;
				CMUTIL_XmlNextSub(NULL, ctx, 1);
				while (!closed)
					if (!CMUTIL_XmlParseNode(ctx, &closed))
						return NULL;
				CMUTIL_CALL(ctx->stack, Pop);
				return child;
			} else {
				CMUTIL_CALL(child, Destroy);
				CMLogErrorS("invalid xml");
				return NULL;
			}
			break;
		}
	} else if (parent) {
		// text node
		CMUTIL_String *bfval, *afval;
		const char *p = strchr (ctx->pos, '<');
		int textlen = (int)(p - ctx->pos);
		bfval = CMUTIL_StringCreateInternal(ctx->memst, 10, NULL);
		CMUTIL_CALL(bfval, AddNString, ctx->pos, textlen);
		afval = CMUTIL_StringCreateInternal(
					ctx->memst, CMUTIL_CALL(bfval, GetSize), NULL);
		CMUTIL_XmlUnescape(afval, bfval, ctx);
		CMUTIL_CALL(bfval, Destroy);
		child = CMUTIL_XmlNodeCreateWithLenInternal(
						ctx->memst, CMUTIL_XmlNodeText,
						CMUTIL_CALL(afval, GetCString),
						CMUTIL_CALL(afval, GetSize));
		CMUTIL_CALL(afval, Destroy);
		CMUTIL_CALL(parent, AddChild, child);
		CMUTIL_XmlNextSub(NULL, ctx, textlen);
		return child;
	} else {
		CMLogErrorS("invalid xml");
		return NULL;
	}
}

CMUTIL_XmlNode *CMUTIL_XmlParseStringInternal(
		CMUTIL_Mem_st *memst, const char *xmlstr, int len)
{
	CMUTIL_XmlParseCtx ctx;
	memset(&ctx, 0x0, sizeof(CMUTIL_XmlParseCtx));
	ctx.pos = xmlstr;
	ctx.remain = len;
	ctx.memst = memst;
	strcpy(ctx.encoding, "UTF-8");
	if (CMUTIL_XmlParseHeader(&ctx)) {
		CMUTIL_XmlNode *res, *bottom;
		if (*(ctx.encoding) && strcmp(ctx.encoding, "UTF-8") != 0)
			ctx.cconv = CMUTIL_CSConvCreateInternal(
						memst, ctx.encoding, "UTF-8");
		ctx.stack = CMUTIL_ArrayCreateInternal(memst, 3, NULL, NULL);
		res = CMUTIL_XmlParseNode(&ctx, NULL);
		bottom = (CMUTIL_XmlNode*)CMUTIL_CALL(ctx.stack, Bottom);
		if (bottom) {
			CMLogErrorS("invalid xml: stack not empty");
			CMUTIL_CALL(bottom, Destroy);
			res = NULL;
		}
		if (ctx.cconv)
			CMUTIL_CALL(ctx.cconv, Destroy);
		CMUTIL_CALL(ctx.stack, Destroy);
		return res;
	} else {
		CMLogErrorS("invalid xml: invalid header");
		return NULL;
	}
}

CMUTIL_XmlNode *CMUTIL_XmlParseString(const char *xmlstr, int len)
{
	return CMUTIL_XmlParseStringInternal(__CMUTIL_Mem, xmlstr, len);
}

CMUTIL_XmlNode *CMUTIL_XmlParseInternal(
		CMUTIL_Mem_st *memst, CMUTIL_String *str)
{
	return CMUTIL_XmlParseStringInternal(
				memst, CMUTIL_CALL(str, GetCString),
				CMUTIL_CALL(str, GetSize));
}

CMUTIL_XmlNode *CMUTIL_XmlParse(CMUTIL_String *str)
{
	return CMUTIL_XmlParseInternal(__CMUTIL_Mem, str);
}

CMUTIL_XmlNode *CMUTIL_XmlParseFileInternal(
		CMUTIL_Mem_st *memst, const char *fpath)
{
	FILE *f;
	char buffer[1024];
	f = fopen(fpath, "rb");
	if (f) {
		CMUTIL_String *fcont = CMUTIL_StringCreateInternal(memst, 1024, NULL);
		CMUTIL_XmlNode *res = NULL;
		size_t rsz;
		while ((rsz = fread(buffer, 1, 1024, f)))
			CMUTIL_CALL(fcont, AddNString, buffer, (int)rsz);
		res = CMUTIL_XmlParseInternal(memst, fcont);
		CMUTIL_CALL(fcont, Destroy);
		return res;
	} else {
		CMLogErrorS("cannot read file: %s", fpath);
		return NULL;
	}
}

CMUTIL_XmlNode *CMUTIL_XmlParseFile(const char *fpath)
{
	return CMUTIL_XmlParseFileInternal(__CMUTIL_Mem, fpath);
}

typedef struct CMUTIL_XmlToJsonCtx {
	CMUTIL_Array *stack;
	CMUTIL_Mem_st *memst;
} CMUTIL_XmlToJsonCtx;

CMUTIL_STATIC CMUTIL_Json *CMUTIL_XmlToJsonInternal(
		CMUTIL_XmlNode *node, CMUTIL_XmlToJsonCtx *ctx)
{
	int i, ccnt;
	CMUTIL_Json *prev;
	CMUTIL_Json *res;
	CMUTIL_JsonObject *parent;
	CMUTIL_StringArray *attrnames;
	CMUTIL_Bool isattr = CMUTIL_False;
	const char *name = CMUTIL_CALL(node, GetName);
	if (CMUTIL_CALL(ctx->stack, GetSize) == 0) {
		parent = CMUTIL_JsonObjectCreateInternal(ctx->memst);
		CMUTIL_CALL(ctx->stack, Push, parent);
	} else {
		parent = (CMUTIL_JsonObject*)CMUTIL_CALL(ctx->stack, Top);
	}

	// set attributes to json object
	attrnames = CMUTIL_CALL(node, GetAttributeNames);
	if (attrnames != NULL && CMUTIL_CALL(attrnames, GetSize) > 0) {
		CMUTIL_JsonObject *ores = CMUTIL_JsonObjectCreateInternal(ctx->memst);
		for (i=0; i<CMUTIL_CALL(attrnames, GetSize); i++) {
			CMUTIL_String *aname = CMUTIL_CALL(attrnames, GetAt, i);
			const char *sname = CMUTIL_CALL(aname, GetCString);
			CMUTIL_String *value = CMUTIL_CALL(node, GetAttribute, sname);
			const char *svalue = CMUTIL_CALL(value, GetCString);
			prev = CMUTIL_CALL(ores, Get, sname);
			if (prev) {
				if (CMUTIL_CALL(prev, GetType) == CMUTIL_JsonTypeArray) {
					CMUTIL_CALL((CMUTIL_JsonArray*)prev, AddString, svalue);
				} else {
					CMUTIL_JsonArray *arr =
							CMUTIL_JsonArrayCreateInternal(ctx->memst);
					prev = CMUTIL_CALL(ores, Remove, sname);
					CMUTIL_CALL(arr, Add, prev);
					CMUTIL_CALL(arr, AddString, svalue);
					CMUTIL_CALL(ores, Put, sname, (CMUTIL_Json*)arr);
				}
			} else {
				CMUTIL_CALL(ores, PutString, sname, svalue);
			}
		}
		res = (CMUTIL_Json*)ores;
	}
	if (CMUTIL_CALL(node, ChildCount) == 1) {
		CMUTIL_XmlNode *child = CMUTIL_CALL(node, ChildAt, 0);
		if (CMUTIL_XmlNodeText == CMUTIL_CALL(child, GetType)) {
			const char *text = CMUTIL_CALL(child, GetName);
			text = CMUTIL_StrTrim((char*)text);
			if (strlen(text) > 0) {
				CMUTIL_JsonValue *val = CMUTIL_JsonValueCreate();
				CMUTIL_CALL(val, SetString, text);
				//if ()
				res = (CMUTIL_Json*)val;
				isattr = CMUTIL_True;
			}
		}
	}
	if (res == NULL)
		res = (CMUTIL_Json*)CMUTIL_JsonObjectCreate();
	if (attrnames)
		CMUTIL_CALL(attrnames, Destroy);

	ccnt = CMUTIL_CALL(node, ChildCount);
	if (ccnt > 0 && !isattr) {
		// set children to json object
		CMUTIL_CALL(ctx->stack, Push, res);
		for (i=0; i<ccnt; i++)
			CMUTIL_XmlToJsonInternal(CMUTIL_CALL(node, ChildAt, i), ctx);
		CMUTIL_CALL(ctx->stack, Pop);
	}
	prev = CMUTIL_CALL(parent, Get, name);
	if (prev) {
		if (CMUTIL_CALL(prev, GetType) == CMUTIL_JsonTypeArray) {
			CMUTIL_CALL((CMUTIL_JsonArray*)prev, Add, res);
		} else {
			CMUTIL_JsonArray *arr =
					CMUTIL_JsonArrayCreateInternal(ctx->memst);
			prev = CMUTIL_CALL(parent, Remove, name);
			CMUTIL_CALL(arr, Add, prev);
			CMUTIL_CALL(arr, Add, res);
			CMUTIL_CALL(parent, Put, name, (CMUTIL_Json*)arr);
		}
	} else {
		CMUTIL_CALL(parent, Put, name, res);
	}
	return (CMUTIL_Json*)res;
}

CMUTIL_Json *CMUTIL_XmlToJson(CMUTIL_XmlNode *node)
{
	CMUTIL_Json *res;
	CMUTIL_XmlNode_Internal *inode = (CMUTIL_XmlNode_Internal*)node;
	CMUTIL_XmlToJsonCtx ctx;
	ctx.stack = CMUTIL_ArrayCreateInternal(inode->memst, 5, NULL, NULL);
	ctx.memst = inode->memst;
	CMUTIL_XmlToJsonInternal(node, &ctx);
	res = (CMUTIL_Json*)CMUTIL_CALL(ctx.stack, Pop);
	CMUTIL_CALL(ctx.stack, Destroy);
	return res;
}
