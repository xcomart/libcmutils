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

#if defined(MSWIN)
# define atoll	_atoi64
#endif

CMUTIL_LogDefine("cmutils.nanojson")

CMUTIL_STATIC void CMUTIL_JsonToStringInternal(
        const CMUTIL_Json *json, CMUTIL_String *buf,
        CMBool pretty, int depth);

typedef void (*CMUTIL_JsonValToStrFunc)(CMUTIL_String *str, CMUTIL_String *buf);

CMUTIL_STATIC void CMUTIL_JsonValueNumberToStr(
        CMUTIL_String *data, CMUTIL_String *buf)
{
    CMCall(buf, AddAnother, data);
}

CMUTIL_STATIC void CMUTIL_JsonValueStringToStr(
        CMUTIL_String *data, CMUTIL_String *buf)
{
    const char *sdata = CMCall(data, GetCString);
    CMCall(buf, AddPrint, "\"%s\"", sdata);
}

static CMUTIL_JsonValToStrFunc g_cmutil_jsonvaltostrf[]=
{
    CMUTIL_JsonValueNumberToStr,
    CMUTIL_JsonValueNumberToStr,
    CMUTIL_JsonValueStringToStr,
    CMUTIL_JsonValueNumberToStr,
    CMUTIL_JsonValueNumberToStr
};

typedef struct CMUTIL_JsonValue_Internal {
    CMUTIL_JsonValue		base;
    CMUTIL_String			*data;
    CMUTIL_Mem              *memst;
    CMJsonValueType	type;
    int                     dummy_padder;
} CMUTIL_JsonValue_Internal;

typedef struct CMUTIL_JsonObject_Internal {
    CMUTIL_JsonObject	base;
    CMUTIL_Map			*map;
    CMUTIL_Array        *keys;
    CMUTIL_Mem          *memst;
} CMUTIL_JsonObject_Internal;

typedef struct CMUTIL_JsonArray_Internal {
    CMUTIL_JsonArray	base;
    CMUTIL_Array		*arr;
    CMUTIL_Mem          *memst;
} CMUTIL_JsonArray_Internal;


CMUTIL_STATIC void CMUTIL_JsonIndent(CMUTIL_String *buf, int depth)
{
    for (; depth > 0; depth--)
        CMCall(buf, AddNString, "  ", 2);
}

CMUTIL_STATIC void CMUTIL_JsonValueToStringInternal(
        const CMUTIL_Json *json,
        CMUTIL_String *buf, CMBool pretty, int depth)
{
    const CMUTIL_JsonValue_Internal *ijval =
            (const CMUTIL_JsonValue_Internal*)json;
    g_cmutil_jsonvaltostrf[ijval->type](ijval->data, buf);
    CMUTIL_UNUSED(pretty); CMUTIL_UNUSED(depth);
}

CMUTIL_STATIC void CMUTIL_JsonObjectToStringInternal(
        const CMUTIL_Json *json, CMUTIL_String *buf,
        CMBool pretty, int depth)
{
    uint32_t i;
    const CMUTIL_JsonObject *jobj = (const CMUTIL_JsonObject*)json;
    CMUTIL_StringArray *keys = CMCall(jobj, GetKeys);
    CMCall(buf, AddChar, '{');
    if (CMCall(keys, GetSize) > 0) {
        for (i=0; i<CMCall(keys, GetSize); i++) {
            CMUTIL_String *key = CMCall(keys, GetAt, i);
            const char *skey = CMCall(key, GetCString);
            CMUTIL_Json *item = CMCall(jobj, Get, skey);
            if (i) CMCall(buf, AddChar, ',');
            if (pretty) {
                CMCall(buf, AddChar, '\n');
                CMUTIL_JsonIndent(buf, depth+1);
            }
            CMUTIL_JsonValueStringToStr(key, buf);
            CMCall(buf, AddChar, ':');
            CMUTIL_JsonToStringInternal(item, buf, pretty, depth+1);
        }
        if (pretty) {
            CMCall(buf, AddChar, '\n');
            CMUTIL_JsonIndent(buf, depth);
        }
    }
    CMCall(buf, AddChar, '}');
    CMCall(keys, Destroy);
}

CMUTIL_STATIC void CMUTIL_JsonArrayToStringInternal(
        const CMUTIL_Json *json, CMUTIL_String *buf,
        CMBool pretty, int depth)
{
    const CMUTIL_JsonArray *jarr = (const CMUTIL_JsonArray*)json;
    uint32_t i;
    size_t size = CMCall(jarr, GetSize);
    CMCall(buf, AddChar, '[');
    if (size > 0) {
        for (i=0; i<size; i++) {
            CMUTIL_Json *item = CMCall(jarr, Get, i);
            if (i) CMCall(buf, AddChar, ',');
            CMUTIL_JsonToStringInternal(item, buf, pretty, depth+1);
        }
    }
    CMCall(buf, AddChar, ']');
}

typedef void (*CMUTIL_JsonToStringFunc)(
        const CMUTIL_Json *json,
        CMUTIL_String *buf, CMBool pretty, int depth);

static CMUTIL_JsonToStringFunc g_cmutil_jsontostrf[] = {
    CMUTIL_JsonValueToStringInternal,
    CMUTIL_JsonObjectToStringInternal,
    CMUTIL_JsonArrayToStringInternal
};

CMUTIL_STATIC void CMUTIL_JsonToStringInternal(
        const CMUTIL_Json *json, CMUTIL_String *buf,
        CMBool pretty, int depth)
{
    g_cmutil_jsontostrf[CMCall(json, GetType)](json, buf, pretty, depth);
}

CMUTIL_STATIC void CMUTIL_JsonToString(
        const CMUTIL_Json *json, CMUTIL_String *buf, CMBool pretty)
{
    CMUTIL_JsonToStringInternal(json, buf, pretty, 0);
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonValueClone(const CMUTIL_Json *json)
{
    const CMUTIL_JsonValue_Internal *ival =
            (const CMUTIL_JsonValue_Internal*)json;
    CMUTIL_JsonValue_Internal *res = (CMUTIL_JsonValue_Internal*)
            ival->memst->Alloc(sizeof(CMUTIL_JsonValue_Internal));
    memcpy(res, ival, sizeof(CMUTIL_JsonValue_Internal));
    res->data = CMCall(res->data, Clone);
    return (CMUTIL_Json*)res;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonObjectClone(const CMUTIL_Json *json)
{
    const CMUTIL_JsonObject_Internal *iobj =
            (const CMUTIL_JsonObject_Internal*)json;
    CMUTIL_JsonObject_Internal *res = (CMUTIL_JsonObject_Internal*)
            CMUTIL_JsonObjectCreateInternal(iobj->memst);
    CMUTIL_StringArray *keys = CMCall(iobj->map, GetKeys);
    CMUTIL_Json *val, *dup;
    uint32_t i;
    for (i=0; i<CMCall(keys, GetSize); i++) {
        CMUTIL_String *key = CMCall(keys, GetAt, i);
        const char *skey = CMCall(key, GetCString);
        val = CMCall(&iobj->base, Get, skey);
        dup = CMCall(val, Clone);
        CMCall(&res->base, Put, skey, dup);
    }
    CMCall(keys, Destroy);
    return (CMUTIL_Json*)res;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonArrayClone(const CMUTIL_Json *json)
{
    const CMUTIL_JsonArray_Internal *iarr =
            (const CMUTIL_JsonArray_Internal*)json;
    CMUTIL_JsonArray_Internal *res = (CMUTIL_JsonArray_Internal*)
            CMUTIL_JsonArrayCreateInternal(iarr->memst);
    uint32_t i;
    for (i=0; i<CMCall(iarr->arr, GetSize); i++) {
        CMUTIL_Json *item = CMCall(&iarr->base, Get, i);
        CMUTIL_Json *dup = CMCall(item, Clone);
        CMCall(&res->base, Add, dup);
    }
    return (CMUTIL_Json*)res;
}

static CMUTIL_Json *(*g_cmutil_json_clonefuncs[])(const CMUTIL_Json*) = {
    CMUTIL_JsonValueClone,
    CMUTIL_JsonObjectClone,
    CMUTIL_JsonArrayClone
};

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonClone(const CMUTIL_Json *json)
{
    return g_cmutil_json_clonefuncs[CMCall(json, GetType)](json);
}

CMUTIL_STATIC void CMUTIL_JsonDestroyInternal(void *json) {
    CMUTIL_JsonDestroy(json);
}

CMUTIL_STATIC CMJsonType CMUTIL_JsonValueGetType(const CMUTIL_Json *json)
{
    CMUTIL_UNUSED(json);
    return CMJsonTypeValue;
}

CMUTIL_STATIC void CMUTIL_JsonValueDestroy(CMUTIL_Json *json)
{
    CMUTIL_JsonValue_Internal *ijval = (CMUTIL_JsonValue_Internal*)json;
    if (ijval) {
        CMCall(ijval->data, Destroy);
        ijval->memst->Free(ijval);
    }
}

CMUTIL_STATIC CMJsonValueType CMUTIL_JsonValueGetValueType(
        const CMUTIL_JsonValue *jval)
{
    const CMUTIL_JsonValue_Internal *ijval =
            (const CMUTIL_JsonValue_Internal*)jval;
    return ijval->type;
}

CMUTIL_STATIC int64_t CMUTIL_JsonValueGetLong(const CMUTIL_JsonValue *jval)
{
    const CMUTIL_JsonValue_Internal *ijval =
            (const CMUTIL_JsonValue_Internal*)jval;
    return atoll(CMCall(ijval->data, GetCString));
}

CMUTIL_STATIC double CMUTIL_JsonValueGetDouble(const CMUTIL_JsonValue *jval)
{
    const CMUTIL_JsonValue_Internal *ijval =
            (const CMUTIL_JsonValue_Internal*)jval;
    return atof(CMCall(ijval->data, GetCString));
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_JsonValueGetString(
        const CMUTIL_JsonValue *jval)
{
    const CMUTIL_JsonValue_Internal *ijval =
            (const CMUTIL_JsonValue_Internal*)jval;
    return ijval->data;
}

CMUTIL_STATIC const char *CMUTIL_JsonValueGetCString(
        const CMUTIL_JsonValue *jval)
{
    const CMUTIL_JsonValue_Internal *ijval =
            (const CMUTIL_JsonValue_Internal*)jval;
    return CMCall(ijval->data, GetCString);
}

CMUTIL_STATIC CMBool CMUTIL_JsonValueGetBoolean(
        const CMUTIL_JsonValue *jval)
{
    const CMUTIL_JsonValue_Internal *ijval =
            (const CMUTIL_JsonValue_Internal*)jval;
    const char *sdata = CMCall(ijval->data, GetCString);
    return strchr("1tTyY", *sdata)? CMTrue:CMFalse;
}

CMUTIL_STATIC void CMUTIL_JsonValueSetBase(
        CMUTIL_JsonValue *jval, const char *buf, size_t length,
        CMJsonValueType type)
{
    CMUTIL_JsonValue_Internal *ijval = (CMUTIL_JsonValue_Internal*)jval;
    CMCall(ijval->data, Clear);
    CMCall(ijval->data, AddNString, buf, length);
    ijval->type = type;
}

CMUTIL_STATIC void CMUTIL_JsonValueSetLong(
        CMUTIL_JsonValue *jval, int64_t value)
{
    char buf[50];
    size_t size = (uint32_t)sprintf(buf, PRINT64I, value);
    CMUTIL_JsonValueSetBase(jval, buf, size, CMJsonValueLong);
}

CMUTIL_STATIC void CMUTIL_JsonValueSetDouble(
        CMUTIL_JsonValue *jval, double value)
{
    char buf[50];
    size_t size = (uint32_t)sprintf(buf, "%lf", value);
    CMUTIL_JsonValueSetBase(jval, buf, size, CMJsonValueDouble);
}

CMUTIL_STATIC void CMUTIL_JsonValueSetString(
        CMUTIL_JsonValue *jval, const char *value)
{
    CMUTIL_JsonValueSetBase(
                jval, value, strlen(value), CMJsonValueString);
}

CMUTIL_STATIC void CMUTIL_JsonValueSetBoolean(
        CMUTIL_JsonValue *jval, CMBool value)
{
    const char *sval = value? "true":"false";
    CMUTIL_JsonValueSetBase(
                jval, sval, strlen(sval), CMJsonValueBoolean);
}

CMUTIL_STATIC void CMUTIL_JsonValueSetNull(CMUTIL_JsonValue *jval)
{
    CMUTIL_JsonValueSetBase(jval, "null", 4, CMJsonValueNull);
}

static CMUTIL_JsonValue g_cmutil_jsonvalue = {
    {
        CMUTIL_JsonToString,
        CMUTIL_JsonValueGetType,
        CMUTIL_JsonClone,
        CMUTIL_JsonValueDestroy
    },
    CMUTIL_JsonValueGetValueType,
    CMUTIL_JsonValueGetLong,
    CMUTIL_JsonValueGetDouble,
    CMUTIL_JsonValueGetString,
    CMUTIL_JsonValueGetCString,
    CMUTIL_JsonValueGetBoolean,
    CMUTIL_JsonValueSetLong,
    CMUTIL_JsonValueSetDouble,
    CMUTIL_JsonValueSetString,
    CMUTIL_JsonValueSetBoolean,
    CMUTIL_JsonValueSetNull
};

CMUTIL_STATIC CMUTIL_JsonValue *CMUTIL_JsonValueCreateInternal(
        CMUTIL_Mem *memst)
{
    CMUTIL_JsonValue_Internal *res =
            memst->Alloc(sizeof(CMUTIL_JsonValue_Internal));
    memset(res, 0x0, sizeof(CMUTIL_JsonValue_Internal));
    memcpy(res, &g_cmutil_jsonvalue, sizeof(CMUTIL_JsonValue));
    res->data = CMUTIL_StringCreateInternal(memst, 10, NULL);
    res->memst = memst;
    return (CMUTIL_JsonValue*)res;
}

CMUTIL_JsonValue *CMUTIL_JsonValueCreate()
{
    return CMUTIL_JsonValueCreateInternal(CMUTIL_GetMem());
}


CMUTIL_STATIC CMJsonType CMUTIL_JsonObjectGetType(const CMUTIL_Json *json)
{
    CMUTIL_UNUSED(json);
    return CMJsonTypeObject;
}

CMUTIL_STATIC void CMUTIL_JsonObjectDestroy(CMUTIL_Json *json)
{
    CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)json;
    if (ijobj) {
        if (ijobj->map)
            CMCall(ijobj->map, Destroy);
        if (ijobj->keys)
            CMCall(ijobj->keys, Destroy);
        ijobj->memst->Free(ijobj);
    }
}

CMUTIL_STATIC CMUTIL_StringArray *CMUTIL_JsonObjectGetKeys(
        const CMUTIL_JsonObject *jobj)
{
    const CMUTIL_JsonObject_Internal *ijobj =
            (const CMUTIL_JsonObject_Internal*)jobj;
    return CMCall(ijobj->map, GetKeys);
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonObjectGet(
        const CMUTIL_JsonObject *jobj, const char *key)
{
    const CMUTIL_JsonObject_Internal *ijobj =
            (const CMUTIL_JsonObject_Internal*)jobj;
    return (CMUTIL_Json*)CMCall(ijobj->map, Get, key);
}

#define CMUTIL_JsonObjectGetBody(jobj, key, method, v) do {                 \
    CMUTIL_Json *json = CMUTIL_JsonObjectGet(jobj, key);                    \
    if (json) {                                                             \
        if (CMCall(json, GetType) == CMJsonTypeValue)                  \
            return CMCall((CMUTIL_JsonValue*)json, method );                \
        else                                                                \
            CMLogError("JsonObject item with key '%s' is not a value type.",\
                        key);                                               \
    } else {                                                                \
        CMLogError("JsonObject has no item with key '%s'", key);            \
    }                                                                       \
    return v; } while(0)


CMUTIL_STATIC int64_t CMUTIL_JsonObjectGetLong(
        const CMUTIL_JsonObject *jobj, const char *key)
{
    CMUTIL_JsonObjectGetBody(jobj, key, GetLong, 0);
}

CMUTIL_STATIC double CMUTIL_JsonObjectGetDouble(
        const CMUTIL_JsonObject *jobj, const char *key)
{
    CMUTIL_JsonObjectGetBody(jobj, key, GetDouble, 0.0);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_JsonObjectGetString(
        const CMUTIL_JsonObject *jobj, const char *key)
{
    CMUTIL_JsonObjectGetBody(jobj, key, GetString, NULL);
}

CMUTIL_STATIC const char *CMUTIL_JsonObjectGetCString(
        const CMUTIL_JsonObject *jobj, const char *key)
{
    CMUTIL_JsonObjectGetBody(jobj, key, GetCString, NULL);
}

CMUTIL_STATIC CMBool CMUTIL_JsonObjectGetBoolean(
        const CMUTIL_JsonObject *jobj, const char *key)
{
    CMUTIL_JsonObjectGetBody(jobj, key, GetBoolean, CMFalse);
}

CMUTIL_STATIC void CMUTIL_JsonObjectPut(
        CMUTIL_JsonObject *jobj, const char *key, CMUTIL_Json *json)
{
    CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)jobj;
    json = CMCall(ijobj->map, Put, key, json);
    // destory previous mapped item
    if (json) CMCall(json, Destroy);
    else CMCall(ijobj->keys, Add, CMStrdup(key));
}

#define CMUTIL_JsonObjectPutBody(jobj, key, method, value) do {             \
    CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)jobj;  \
    CMUTIL_JsonValue *jval = CMUTIL_JsonValueCreateInternal(ijobj->memst);  \
    CMCall(jval, method, value);                                            \
    CMCall(jobj, Put, key, (CMUTIL_Json*)jval);                             \
    } while(0)

CMUTIL_STATIC void CMUTIL_JsonObjectPutLong(
        CMUTIL_JsonObject *jobj, const char *key, int64_t value)
{
    CMUTIL_JsonObjectPutBody(jobj, key, SetLong, value);
}

CMUTIL_STATIC void CMUTIL_JsonObjectPutDouble(
        CMUTIL_JsonObject *jobj, const char *key, double value)
{
    CMUTIL_JsonObjectPutBody(jobj, key, SetDouble, value);
}

CMUTIL_STATIC void CMUTIL_JsonObjectPutString(
        CMUTIL_JsonObject *jobj, const char *key, const char *value)
{
    CMUTIL_JsonObjectPutBody(jobj, key, SetString, value);
}

CMUTIL_STATIC void CMUTIL_JsonObjectPutBoolean(
        CMUTIL_JsonObject *jobj, const char *key, CMBool value)
{
    CMUTIL_JsonObjectPutBody(jobj, key, SetBoolean, value);
}

CMUTIL_STATIC void CMUTIL_JsonObjectPutNull(
        CMUTIL_JsonObject *jobj, const char *key)
{
    CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)jobj;
    CMUTIL_JsonValue *jval = CMUTIL_JsonValueCreateInternal(ijobj->memst);
    CMCall(jval, SetNull);
    CMCall(jobj, Put, key, (CMUTIL_Json*)jval);
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonObjectRemove(
        CMUTIL_JsonObject *jobj, const char *key)
{
    CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)jobj;
    uint32_t idx = 0;
    if (CMCall(ijobj->keys, Find, key, &idx))
        CMCall(ijobj->keys, RemoveAt, idx);
    return (CMUTIL_Json*)CMCall(ijobj->map, Remove, key);
}

CMUTIL_STATIC void CMUTIL_JsonObjectDelete(
        CMUTIL_JsonObject *jobj, const char *key)
{
    CMUTIL_Json *item = CMCall(jobj, Remove, key);
    if (item)
        CMCall(item, Destroy);
}

static CMUTIL_JsonObject g_cmutil_jsonobject = {
    {
        CMUTIL_JsonToString,
        CMUTIL_JsonObjectGetType,
        CMUTIL_JsonClone,
        CMUTIL_JsonObjectDestroy
    },
    CMUTIL_JsonObjectGetKeys,
    CMUTIL_JsonObjectGet,
    CMUTIL_JsonObjectGetLong,
    CMUTIL_JsonObjectGetDouble,
    CMUTIL_JsonObjectGetString,
    CMUTIL_JsonObjectGetCString,
    CMUTIL_JsonObjectGetBoolean,
    CMUTIL_JsonObjectPut,
    CMUTIL_JsonObjectPutLong,
    CMUTIL_JsonObjectPutDouble,
    CMUTIL_JsonObjectPutString,
    CMUTIL_JsonObjectPutBoolean,
    CMUTIL_JsonObjectPutNull,
    CMUTIL_JsonObjectRemove,
    CMUTIL_JsonObjectDelete
};

CMUTIL_JsonObject *CMUTIL_JsonObjectCreateInternal(CMUTIL_Mem *memst)
{
    CMUTIL_JsonObject_Internal *res =
            memst->Alloc(sizeof(CMUTIL_JsonObject_Internal));
    memset(res, 0x0, sizeof(CMUTIL_JsonObject_Internal));
    memcpy(res, &g_cmutil_jsonobject, sizeof(CMUTIL_JsonObject));
    res->memst = memst;
    res->map = CMUTIL_MapCreateInternal(
                memst, 256, CMFalse, CMUTIL_JsonDestroyInternal);
    res->keys = CMUTIL_ArrayCreateInternal(
                memst, 10, (int(*)(const void*,const void*))strcmp,
                CMFree, CMFalse);
    return (CMUTIL_JsonObject*)res;
}

CMUTIL_JsonObject *CMUTIL_JsonObjectCreate()
{
    return CMUTIL_JsonObjectCreateInternal(CMUTIL_GetMem());
}



CMUTIL_STATIC CMJsonType CMUTIL_JsonArrayGetType(const CMUTIL_Json *json)
{
    CMUTIL_UNUSED(json);
    return CMJsonTypeArray;
}

CMUTIL_STATIC void CMUTIL_JsonArrayDestroy(CMUTIL_Json *json)
{
    CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)json;
    if (ijarr) {
        if (ijarr->arr) CMCall(ijarr->arr, Destroy);
        ijarr->memst->Free(ijarr);
    }
}

CMUTIL_STATIC size_t CMUTIL_JsonArrayGetSize(const CMUTIL_JsonArray *jarr)
{
    const CMUTIL_JsonArray_Internal *ijarr =
            (const CMUTIL_JsonArray_Internal*)jarr;
    return CMCall(ijarr->arr, GetSize);
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonArrayGet(
        const CMUTIL_JsonArray *jarr, uint32_t index)
{
    const CMUTIL_JsonArray_Internal *ijarr =
            (const CMUTIL_JsonArray_Internal*)jarr;
    if (index < CMCall(ijarr->arr, GetSize))
        return (CMUTIL_Json*)CMCall(ijarr->arr, GetAt, index);
    CMLogErrorS("JsonArray index out of bound(%d) total %d.",
                index, CMCall(ijarr->arr, GetSize));
    return NULL;
}

#define CMUTIL_JsonArrayGetBody(jarr, index, method, v) do {        \
    const CMUTIL_JsonArray_Internal *ijarr =                        \
            (const CMUTIL_JsonArray_Internal*)jarr;                 \
    if (CMCall(ijarr->arr, GetSize) > index) {                      \
        CMUTIL_Json* json = CMCall(jarr, Get, index);               \
        if (CMCall(json, GetType) == CMJsonTypeValue) {             \
            return CMCall((CMUTIL_JsonValue*)json, method);         \
        } else {                                                    \
            CMLogError("item type is not 'Value' type.");           \
        }                                                           \
    } else {                                                        \
        CMLogError("JsonArray index out of bound(%d), total %d.",   \
                index, CMCall(ijarr->arr, GetSize));                \
    } return v;} while(0)

CMUTIL_STATIC int64_t CMUTIL_JsonArrayGetLong(
        const CMUTIL_JsonArray *jarr, uint32_t index)
{
    CMUTIL_JsonArrayGetBody(jarr, index, GetLong, 0);
}

CMUTIL_STATIC double CMUTIL_JsonArrayGetDouble(
        const CMUTIL_JsonArray *jarr, uint32_t index)
{
    CMUTIL_JsonArrayGetBody(jarr, index, GetDouble, 0.0);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_JsonArrayGetString(
        const CMUTIL_JsonArray *jarr, uint32_t index)
{
    CMUTIL_JsonArrayGetBody(jarr, index, GetString, NULL);
}

CMUTIL_STATIC const char *CMUTIL_JsonArrayGetCString(
        const CMUTIL_JsonArray *jarr, uint32_t index)
{
    CMUTIL_JsonArrayGetBody(jarr, index, GetCString, NULL);
}

CMUTIL_STATIC CMBool CMUTIL_JsonArrayGetBoolean(
        const CMUTIL_JsonArray *jarr, uint32_t index)
{
    CMUTIL_JsonArrayGetBody(jarr, index, GetBoolean, CMFalse);
}

CMUTIL_STATIC void CMUTIL_JsonArrayAdd(
        CMUTIL_JsonArray *jarr, CMUTIL_Json *json)
{
    CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)jarr;
    CMCall(ijarr->arr, Add, json);
}

#define CMUTIL_JsonArrayAddBody(jarr, value, method) do{                    \
    CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)jarr;    \
    CMUTIL_JsonValue *json = CMUTIL_JsonValueCreateInternal(ijarr->memst);  \
    CMCall(json, method, value);                                            \
    CMUTIL_JsonArrayAdd(jarr, (CMUTIL_Json*)json); } while(0)

CMUTIL_STATIC void CMUTIL_JsonArrayAddLong(
        CMUTIL_JsonArray *jarr, int64_t value)
{
    CMUTIL_JsonArrayAddBody(jarr, value, SetLong);
}

CMUTIL_STATIC void CMUTIL_JsonArrayAddDouble(
        CMUTIL_JsonArray *jarr, double value)
{
    CMUTIL_JsonArrayAddBody(jarr, value, SetDouble);
}

CMUTIL_STATIC void CMUTIL_JsonArrayAddString(
        CMUTIL_JsonArray *jarr, const char *value)
{
    CMUTIL_JsonArrayAddBody(jarr, value, SetString);
}

CMUTIL_STATIC void CMUTIL_JsonArrayAddBoolean(
        CMUTIL_JsonArray *jarr, CMBool value)
{
    CMUTIL_JsonArrayAddBody(jarr, value, SetBoolean);
}

CMUTIL_STATIC void CMUTIL_JsonArrayAddNull(CMUTIL_JsonArray *jarr)
{
    CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)jarr;
    CMUTIL_JsonValue *json = CMUTIL_JsonValueCreateInternal(ijarr->memst);
    CMCall(json, SetNull);
    CMUTIL_JsonArrayAdd(jarr, (CMUTIL_Json*)json);
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonArrayRemove(
        CMUTIL_JsonArray *jarr, uint32_t index)
{
    CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)jarr;
    return (CMUTIL_Json*)CMCall(ijarr->arr, RemoveAt, index);
}

CMUTIL_STATIC void CMUTIL_JsonArrayDelete(
        CMUTIL_JsonArray *jarr, uint32_t index)
{
    CMUTIL_Json *item = CMCall(jarr, Remove, index);
    CMCall(item, Destroy);
}

static CMUTIL_JsonArray g_cmutil_jsonarray = {
    {
        CMUTIL_JsonToString,
        CMUTIL_JsonArrayGetType,
        CMUTIL_JsonClone,
        CMUTIL_JsonArrayDestroy
    },
    CMUTIL_JsonArrayGetSize,
    CMUTIL_JsonArrayGet,
    CMUTIL_JsonArrayGetLong,
    CMUTIL_JsonArrayGetDouble,
    CMUTIL_JsonArrayGetString,
    CMUTIL_JsonArrayGetCString,
    CMUTIL_JsonArrayGetBoolean,
    CMUTIL_JsonArrayAdd,
    CMUTIL_JsonArrayAddLong,
    CMUTIL_JsonArrayAddDouble,
    CMUTIL_JsonArrayAddString,
    CMUTIL_JsonArrayAddBoolean,
    CMUTIL_JsonArrayAddNull,
    CMUTIL_JsonArrayRemove,
    CMUTIL_JsonArrayDelete
};

CMUTIL_JsonArray *CMUTIL_JsonArrayCreateInternal(CMUTIL_Mem *memst)
{
    CMUTIL_JsonArray_Internal *res =
            memst->Alloc(sizeof(CMUTIL_JsonArray_Internal));
    memset(res, 0x0, sizeof(CMUTIL_JsonArray_Internal));
    memcpy(res, &g_cmutil_jsonarray, sizeof(CMUTIL_JsonArray));
    res->memst = memst;
    res->arr = CMUTIL_ArrayCreateInternal(
                memst, 10, NULL, CMUTIL_JsonDestroyInternal, CMFalse);
    return (CMUTIL_JsonArray*)res;
}

CMUTIL_JsonArray *CMUTIL_JsonArrayCreate()
{
    return CMUTIL_JsonArrayCreateInternal(CMUTIL_GetMem());
}


static const char* JSN_SPACES = " \t\r\n";
static const char* JSN_DELIMS = ":,[]{}";

typedef struct CMUTIL_JsonParser {
    const char *orig, *curr;
    int64_t remain, linecnt;
    CMBool silent;
    CMUTIL_Mem *memst;
} CMUTIL_JsonParser;

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseValue(CMUTIL_JsonParser *pctx);
CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseObject(CMUTIL_JsonParser *pctx);
CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseArray(CMUTIL_JsonParser *pctx);

CMUTIL_STATIC void CMUTIL_JsonParseConsume(
        CMUTIL_JsonParser *pctx, int64_t count)
{
    if (pctx->remain < count)
        count = pctx->remain;

    pctx->remain -= count;
    while (count--) {
        if (*(pctx->curr) == '\n')
            pctx->linecnt++;
        pctx->curr++;
    }
}

CMUTIL_STATIC CMBool CMUTIL_JsonStartsWith(
        CMUTIL_JsonParser *pctx, const char *str)
{
    register const char *p = str, *q = pctx->curr;
    while (*p && *p == *q) {
        p++; q++;
    }
    return *p? CMFalse:CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_JsonSkipSpaces(CMUTIL_JsonParser *pctx)
{
RETRYPOINT:
    while (strchr(JSN_SPACES, *pctx->curr) && pctx->remain > 0)
        CMUTIL_JsonParseConsume(pctx, 1);
    if (pctx->remain > 0) {
        // check comments
        if (*pctx->curr == '/') {
            switch (*(pctx->curr+1)) {
            case '/':	// line comment
                CMUTIL_JsonParseConsume(pctx, 2);
                while (!strchr("\r\n", *pctx->curr) && pctx->remain > 0)
                    CMUTIL_JsonParseConsume(pctx, 1);
                goto RETRYPOINT;
            case '*':	// block comment
                CMUTIL_JsonParseConsume(pctx, 2);
                while (!CMUTIL_JsonStartsWith(pctx, "*/") && pctx->remain > 0)
                    CMUTIL_JsonParseConsume(pctx, 1);
                if (pctx->remain > 0)
                    CMUTIL_JsonParseConsume(pctx, 2);
                goto RETRYPOINT;
            }
        }
    }
    return pctx->remain > 0 ? CMTrue:CMFalse;
}

#define CMUTIL_JSON_PARSE_ERROR(a, msg) do {                                \
    char buf[30] = {0,}; strncat(buf, a->curr, 25); strcat(buf, "...");     \
    if (!a->silent) CMLogError("parse error in line %d before '%s': %s",    \
                a->linecnt, buf, msg); } while(0)

CMUTIL_STATIC CMBool CMUTIL_JsonParseEscape(
        CMUTIL_JsonParser *pctx, CMUTIL_String *sbuf)
{
    char buf[3];
    CMUTIL_JsonParseConsume(pctx, 1);
    if (pctx->remain <= 0) {
        CMUTIL_JSON_PARSE_ERROR(pctx, "unexpected end.");
        return CMFalse;
    }
    switch (*(pctx->curr)) {
    case 't': CMCall(sbuf, AddChar, '\t'); break;
    case 'r': CMCall(sbuf, AddChar, '\r'); break;
    case 'n': CMCall(sbuf, AddChar, '\n'); break;
    case 'u':
        // unicodes are converted to 2 byte
        CMUTIL_StringHexToBytes(buf, pctx->curr+1, 4);
        CMCall(sbuf, AddNString, buf, 2);
        CMUTIL_JsonParseConsume(pctx, 4);
        break;
    default: CMCall(sbuf, AddChar, *(pctx->curr)); break;
    }
    return CMTrue;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseBase(CMUTIL_JsonParser *pctx)
{
    if (CMUTIL_JsonSkipSpaces(pctx)) {
        switch (*(pctx->curr)) {
        case '{': return CMUTIL_JsonParseObject(pctx);
        case '[': return CMUTIL_JsonParseArray(pctx);
        default:  return CMUTIL_JsonParseValue(pctx);
        }
    }
    return NULL;
}

CMUTIL_STATIC CMBool CMUTIL_JsonParseString(
        CMUTIL_JsonParser *pctx, CMUTIL_String **res, CMBool *isconst)
{
    int isend = 0;
    CMUTIL_String *sbuf = NULL;
    int isstquot = 1;

    // remove preceding spaces
    if (!CMUTIL_JsonSkipSpaces(pctx))
        return CMFalse;
    if (*(pctx->curr) != '\"') {
        isstquot = 0;
    } else {
        CMUTIL_JsonParseConsume(pctx, 1);
    }
    sbuf = CMUTIL_StringCreateInternal(pctx->memst, 10, NULL);
    while (pctx->remain > 0 && !isend) {
        if (*(pctx->curr) == '\\') {
            // escape sequence
            if (!CMUTIL_JsonParseEscape(pctx, sbuf)) {
                CMCall(sbuf, Destroy);
                return CMFalse;
            }
        } else if (isstquot && *(pctx->curr) == '\"') {
            // string end
            isend = 1;
        } else if (!isstquot && strchr(JSN_DELIMS, *(pctx->curr))) {
            // string end
            isend = 1;
            break;
        } else {
            CMCall(sbuf, AddChar, *(pctx->curr));
        }
        CMUTIL_JsonParseConsume(pctx, 1);
    }
    if (res)
        *res = sbuf;
    else
        CMCall(sbuf, Destroy);
    if (isconst)
        *isconst = isstquot? CMFalse:CMTrue;
    return CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_JsonParseConst(
        CMUTIL_JsonParser *pctx, CMUTIL_JsonValue *jval, CMUTIL_String *str)
{
    const char *cstr = NULL;
    CMCall(str, SelfTrim);
    cstr = CMCall(str, GetCString);
    if (CMCall(str, GetSize) < 5) {
        if (strcasecmp(cstr, "true") == 0) {
            CMCall(jval, SetBoolean, CMTrue);
            return CMTrue;
        } else if (strcasecmp(cstr, "false") == 0) {
            CMCall(jval, SetBoolean, CMFalse);
            return CMTrue;
        } else if (strcasecmp(cstr, "null") == 0) {
            CMCall(jval, SetNull);
            return CMTrue;
        }
    }
    if (strchr("tTfF", *cstr)) {
        if (strcasecmp(cstr, "true") == 0) {
            CMCall(jval, SetBoolean, CMTrue);
        } else if (strcasecmp(cstr, "false") == 0) {
            CMCall(jval, SetBoolean, CMFalse);
        } else {
            if (!pctx->silent)
                CMUTIL_JSON_PARSE_ERROR(pctx, "cannot parse constant.");
            return CMFalse;
        }
    } else if (strchr("nN", *cstr)) {
        if (strcasecmp(cstr, "null") == 0) {
            CMCall(jval, SetNull);
        } else {
            if (!pctx->silent)
                CMUTIL_JSON_PARSE_ERROR(pctx, "cannot parse constant.");
            return CMFalse;
        }
    } else if (strchr(cstr, '.')) {
        double value = 0.0;
        if (sscanf(cstr, "%lf", &value) != 1) {
            if (!pctx->silent)
                CMUTIL_JSON_PARSE_ERROR(pctx, "cannot parse constant.");
            return CMFalse;
        }
        CMCall(jval, SetDouble, value);
    } else {
        int64_t value = 0;
        if (sscanf(cstr, PRINT64I, &value) != 1) {
            if (!pctx->silent)
                CMUTIL_JSON_PARSE_ERROR(pctx, "cannot parse constant.");
            return CMFalse;
        }
        CMCall(jval, SetLong, value);
    }
    return CMTrue;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseValue(CMUTIL_JsonParser *pctx)
{
    CMUTIL_JsonValue *res = NULL;
    CMUTIL_String *sbuf = NULL;
    CMBool isconst = CMFalse;

    // remove preceding spaces
    if (!CMUTIL_JsonSkipSpaces(pctx))
        return NULL;
    if (!CMUTIL_JsonParseString(pctx, &sbuf, &isconst))
        return NULL;
    res = CMUTIL_JsonValueCreateInternal(pctx->memst);
    if (isconst) {
        if (!CMUTIL_JsonParseConst(pctx, res, sbuf)) {
            CMCall((CMUTIL_Json*)res, Destroy);
            res = NULL;
        }
    } else {
        const char *sval = CMCall(sbuf, GetCString);
        CMCall(res, SetString, sval);
    }
    if (sbuf) CMCall(sbuf, Destroy);
    return (CMUTIL_Json*)res;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseObject(CMUTIL_JsonParser *pctx)
{
    int isend = 0;
    CMUTIL_String *name = NULL;
    CMUTIL_JsonObject *res = NULL;
    CMBool succeeded = CMFalse;

    if (*(pctx->curr) != '{') {
        CMUTIL_JSON_PARSE_ERROR(pctx, "JSON object does not starts with '{'.");
        return NULL;
    }
    CMUTIL_JsonParseConsume(pctx, 1);
    res = CMUTIL_JsonObjectCreateInternal(pctx->memst);
    while (pctx->remain > 0 && !isend) {
        CMUTIL_Json *value = NULL;
        const char *sname = NULL;
        // remove preceeding spaces
        if (!CMUTIL_JsonSkipSpaces(pctx)) {
            CMUTIL_JSON_PARSE_ERROR(pctx, "unexpected end reached.");
            goto ENDPOINT;
        }
        if (*pctx->curr == '}') {
            CMUTIL_JsonParseConsume(pctx, 1);
            isend = 1;
            break;
        }
        // parse name
        if (!CMUTIL_JsonParseString(pctx, &name, NULL)) {
            CMUTIL_JSON_PARSE_ERROR(pctx, "string parsing error.");
            goto ENDPOINT;
        }
        if (*(pctx->curr) != ':') {
            CMUTIL_JSON_PARSE_ERROR(pctx, "value part does not appear.");
            goto ENDPOINT;
        }
        CMUTIL_JsonParseConsume(pctx, 1);

        // parse value part
        value = CMUTIL_JsonParseBase(pctx);
        if (value == NULL) {
            CMUTIL_JSON_PARSE_ERROR(pctx, "value parse error for name '%s'");
            goto ENDPOINT;
        }

        sname = CMCall(name, GetCString);

        CMCall(res, Put, sname, value);
        CMCall(name, Destroy);
        name = NULL;
        value = NULL;

        if (!CMUTIL_JsonSkipSpaces(pctx)) {
            CMUTIL_JSON_PARSE_ERROR(pctx, "unexpected end reached.");
            goto ENDPOINT;
        }
        if (*(pctx->curr) == ',') {
            CMUTIL_JsonParseConsume(pctx, 1);
        } else if (*(pctx->curr) == '}') {
            CMUTIL_JsonParseConsume(pctx, 1);
            isend = 1;
        } else {
            CMUTIL_JSON_PARSE_ERROR(pctx, "unexpected delimiter. "
                                          "must be ',' or '}'.");
            goto ENDPOINT;
        }
    }
    succeeded = CMTrue;
ENDPOINT:
    if (name) CMCall(name, Destroy);
    if (!succeeded && res) {
        CMUTIL_JsonDestroy(res);
        res = NULL;
    }
    return (CMUTIL_Json*)res;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseArray(CMUTIL_JsonParser *pctx)
{
    CMUTIL_JsonArray *res = NULL;
    CMBool succeeded = CMFalse;

    if (*(pctx->curr) != '[') {
        CMUTIL_JSON_PARSE_ERROR(pctx, "JSON array does not starts with '['.");
        return NULL;
    }
    CMUTIL_JsonParseConsume(pctx, 1);
    res = CMUTIL_JsonArrayCreateInternal(pctx->memst);
    while (pctx->remain > 0) {
        CMUTIL_Json *item = NULL;
        // remove preceeding spaces
        if (!CMUTIL_JsonSkipSpaces(pctx)) {
            CMUTIL_JSON_PARSE_ERROR(pctx, "unexpected end reached.");
            goto ENDPOINT;
        }

        if (*(pctx->curr) == ']') {
            // array end.
            CMUTIL_JsonParseConsume(pctx, 1);
            break;
        }

        item = CMUTIL_JsonParseBase(pctx);
        if (item == NULL) {
            //CMUTIL_JSON_PARSE_ERROR(pctx);
            goto ENDPOINT;
        }
        CMCall(res, Add, item);

        if (!CMUTIL_JsonSkipSpaces(pctx)) {
            CMUTIL_JSON_PARSE_ERROR(pctx, "unexpected end reached.");
            goto ENDPOINT;
        }

        if (*(pctx->curr) == ']') {
            // array end.
            CMUTIL_JsonParseConsume(pctx, 1);
            break;
        } else if (*(pctx->curr) == ',') {
            // parse next item
            CMUTIL_JsonParseConsume(pctx, 1);
        } else {
            CMUTIL_JSON_PARSE_ERROR(pctx, "unexpected delimiter. "
                                          "must be ',' or ']'.");
            goto ENDPOINT;
        }
    }
    succeeded = CMTrue;
ENDPOINT:
    if (!succeeded && res) {
        CMUTIL_JsonDestroy(res);
        res = NULL;
    }
    return (CMUTIL_Json*)res;
}

CMUTIL_Json *CMUTIL_JsonParseInternal(
        CMUTIL_Mem *memst, CMUTIL_String *jsonstr, CMBool silent)
{
    CMUTIL_JsonParser parser = {
        CMCall(jsonstr, GetCString),
        CMCall(jsonstr, GetCString),
        (int64_t)CMCall(jsonstr, GetSize),
        0,
        silent,
        memst
    };
    return CMUTIL_JsonParseBase(&parser);
}

CMUTIL_Json *CMUTIL_JsonParse(CMUTIL_String *jsonstr)
{
    return CMUTIL_JsonParseInternal(CMUTIL_GetMem(), jsonstr, CMFalse);
}
