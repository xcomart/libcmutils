/*
                      GNU General Public License
    libcmutils is a bunch of commonly used utility functions for multiplatform.
    Copyright (C) 2016 Dennis Soungjin Park <xcomart@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "functions.h"

typedef enum CMUTIL_ConfType {
    ConfItem_Comment,
    ConfItem_Pair
} CMUTIL_ConfType;

typedef struct CMUTIL_ConfItem {
    char			*key;
    char			*comment;
    CMUTIL_ConfType	type;
    uint32_t			index;
    CMUTIL_Mem      *memst;
} CMUTIL_ConfItem;

typedef struct CMUTIL_Config_Internal {
    CMUTIL_Map		*confs;
    CMUTIL_Array	*sequence;
    CMUTIL_Map		*revconf;
    int             maxkeylen;
    int             dummy_padder;
    CMUTIL_Mem      *memst;
} CMUTIL_Config_Internal;

CMUTIL_STATIC char *NextTermBefore(
        char *dest, char *line, const char *delim, char escapechar)
{
    register char *p = line;
    register char *q = dest;
    while (*p && (!strchr(delim, *p) || *(p - 1) == escapechar)) *q++ = *p++;
    *q = 0x0;
    return p;
}

//CMUTIL_STATIC char *TrimRightEx(char *in, const char *spaces, char *lastchr)
//{
//	char *p = in, *last = NULL;
//	while (*p) {
//		if (!strchr(spaces, *p))
//			last = p + 1;
//	}
//	*last = 0x0;
//	if (lastchr)
//		*lastchr = *(last - 1);
//	return in;
//}

CMUTIL_STATIC int AppendTrimmedLine(CMUTIL_String *rstr, char *in, char *spaces)
{
    int iscont = 0;
    char *p = in, *last = NULL;
    while (*p) {
        if (!strchr(spaces, *p))
            last = p + 1;
    }
    if (*(last - 1) == '\\') {
        iscont = 1;
        last--;
    }
    *last = 0x0;
    if (last > in)
        CMCall(rstr, AddNString, in, (size_t)(last - in));
    return iscont;

}

CMUTIL_STATIC void ConfItemDestroy(void *data)
{
    if (data) {
        CMUTIL_ConfItem *item = (CMUTIL_ConfItem*)data;
        if (item->comment)
            item->memst->Free(item->comment);
        if (item->key)
            item->memst->Free(item->key);
        item->memst->Free(item);
    }
}

CMUTIL_STATIC void CMUTIL_ConfigDestroy(CMUTIL_Config *conf)
{
    if (conf) {
        CMUTIL_Config_Internal *iconf = (CMUTIL_Config_Internal*)conf;
        if (iconf->confs)
            CMCall(iconf->confs, Destroy);

        if (iconf->sequence)
            CMCall(iconf->sequence, Destroy);

        if (iconf->revconf)
            CMCall(iconf->revconf, Destroy);

        iconf->memst->Free(iconf);
    }
}

#define DOFAILED	if (!*p) do { \
        failed = 1; ConfItemDestroy(item); goto FAILED; } while(0)

void CMUTIL_ConfigSave(const CMUTIL_Config *conf, const char *confpath)
{
    if (conf) {
        const CMUTIL_Config_Internal *iconf =
                (const CMUTIL_Config_Internal*)conf;
        FILE *f = NULL;
        char fmt[50];
        uint32_t i;
        f = fopen(confpath, "wb");
        for (i = 0; i < CMCall(iconf->sequence, GetSize); i++) {
            CMUTIL_ConfItem *item = (CMUTIL_ConfItem*)CMCall(
                    iconf->sequence, GetAt, i);
            if (item->type == ConfItem_Pair) {
                char *v = (char*)CMCall(iconf->confs, Get, item->key);
                if (item->comment) {
                    sprintf(fmt, "%%%ds = %%s %%s\n", iconf->maxkeylen);
                    fprintf(f, fmt, item->key, v, item->comment);
                }
                else {
                    sprintf(fmt, "%%%ds = %%s\n", iconf->maxkeylen);
                    fprintf(f, fmt, item->key, v);
                }
            }
            else {
                if (item->comment)
                    fprintf(f, "%s\n", item->comment);
                else
                    fprintf(f, "\n");
            }
        }
        fclose(f);
    }
}

const char *CMUTIL_ConfigGet(const CMUTIL_Config *conf, const char *key)
{
    const CMUTIL_Config_Internal *iconf = (const CMUTIL_Config_Internal*)conf;
    return (char*)CMCall(iconf->confs, Get, key);
}

void CMUTIL_ConfigSet(CMUTIL_Config *conf, const char *key, const char *value)
{
    CMUTIL_Config_Internal *iconf = (CMUTIL_Config_Internal*)conf;
    char *prev = (char*)CMCall(
                iconf->confs, Put, key, iconf->memst->Strdup(value));
    if (prev) {
        iconf->memst->Free(prev);
    }
    else {
        CMUTIL_ConfItem *item =
                (CMUTIL_ConfItem*)iconf->memst->Alloc(sizeof(CMUTIL_ConfItem));
        memset(item, 0x0, sizeof(CMUTIL_ConfItem));
        item->key = iconf->memst->Strdup(key);
        item->type = ConfItem_Pair;
        item->memst = iconf->memst;
        CMCall(iconf->sequence, Add, item);
        CMCall(iconf->revconf, Put, item->key, item);
    }
}

long CMUTIL_ConfigGetLong(const CMUTIL_Config *conf, const char *key)
{
    return atol(CMCall(conf, Get, key));
}

void CMUTIL_ConfigSetLong(CMUTIL_Config *conf, const char *key, long value)
{
    char buf[20];
    sprintf(buf, "%ld", value);
    CMCall(conf, Set, key, buf);
}

double CMUTIL_ConfigGetDouble(const CMUTIL_Config *conf, const char *key)
{
    return atof(CMCall(conf, Get, key));
}

void CMUTIL_ConfigSetDouble(CMUTIL_Config *conf, const char *key, double value)
{
    char buf[30];
    sprintf(buf, "%f", value);
    CMCall(conf, Set, key, buf);
}

static CMUTIL_Config g_cmutil_config = {
    CMUTIL_ConfigSave,
    CMUTIL_ConfigGet,
    CMUTIL_ConfigSet,
    CMUTIL_ConfigGetLong,
    CMUTIL_ConfigSetLong,
    CMUTIL_ConfigGetDouble,
    CMUTIL_ConfigSetDouble,
    CMUTIL_ConfigDestroy
};

CMUTIL_Config *CMUTIL_ConfigCreateInternal(CMUTIL_Mem *memst)
{
    CMUTIL_Config_Internal *res =
        (CMUTIL_Config_Internal*)memst->Alloc(sizeof(CMUTIL_Config_Internal));
    memset(res, 0x0, sizeof(CMUTIL_Config_Internal));
    memcpy(res, &g_cmutil_config, sizeof(CMUTIL_Config));
    res->memst = memst;
    res->confs = CMUTIL_MapCreateInternal(
                memst, CMUTIL_MAP_DEFAULT, CMFalse, memst->Free);
    res->sequence = CMUTIL_ArrayCreateInternal(
                memst, 100, NULL, ConfItemDestroy);
    res->revconf = CMUTIL_MapCreateInternal(
                memst, CMUTIL_MAP_DEFAULT, CMFalse, NULL);
    return (CMUTIL_Config*)res;
}

CMUTIL_Config *CMUTIL_ConfigCreate()
{
    return CMUTIL_ConfigCreateInternal(CMUTIL_GetMem());
}

CMUTIL_Config *CMUTIL_ConfigLoadInternal(
        CMUTIL_Mem *memst, const char *fconf)
{
    FILE *f = fopen(fconf, "rb");
    CMUTIL_Config_Internal *res = NULL;
    int failed = 0;

    if (f) {
//		int cont = 0;
        char line[4096];
        CMUTIL_String *str = CMUTIL_StringCreate();
        res = (CMUTIL_Config_Internal*)CMUTIL_ConfigCreateInternal(memst);

        while (fgets(line, 4096, f)) {
            CMUTIL_ConfItem *item = NULL;
            char *p = NULL;

            if (AppendTrimmedLine(str, line, " \r\t\n"))
                continue;

            item = (CMUTIL_ConfItem*)memst->Alloc(sizeof(CMUTIL_ConfItem));
            memset(res, 0x0, sizeof(CMUTIL_ConfItem));
            res->memst = memst;

            p = (char*)CMCall(str, GetCString);
            p = CMUTIL_StrSkipSpaces(p, SPACES);
            switch (*p) {
            case '#': case '\0':
                item->type = ConfItem_Comment;
                if (*p)
                    item->comment = memst->Strdup(CMUTIL_StrRTrim(p));
                break;
            default:
            {
                int keylen = 0;
                char *v = NULL;
                char name[1024], value[2048];
                p = NextTermBefore(name, p, " =\t", '\\');
                DOFAILED;
                p = CMUTIL_StrSkipSpaces(p, " =\t");
                DOFAILED;
                p = NextTermBefore(value, p, "#\n", '\\');
                v = CMUTIL_StrTrim(value);
                if (*p == '#')
                    item->comment = memst->Strdup(CMUTIL_StrRTrim(p));
                CMCall(res->confs, Put, name, memst->Strdup(v));
                item->key = memst->Strdup(name);
                keylen = (int)strlen(item->key);
                if (res->maxkeylen < keylen)
                    res->maxkeylen = keylen;
                item->type = ConfItem_Pair;
                CMCall(res->revconf, Put, item->key, item);
            }
            }

            item->index = (uint32_t)CMCall(res->sequence, GetSize);
            CMCall(res->sequence, Add, item);
            CMCall(str, Clear);
        }
    FAILED:
        if (str)
            CMCall(str, Destroy);
        fclose(f);
    }

    if (failed && res) {
        CMCall((CMUTIL_Config*)res, Destroy);
        res = NULL;
    }

    return (CMUTIL_Config*)res;
}

CMUTIL_Config *CMUTIL_ConfigLoad(const char *fconf)
{
    return CMUTIL_ConfigLoadInternal(CMUTIL_GetMem(), fconf);
}
