
#include "functions.h"

#if defined(MSWIN)
# define atoll	_atoi64
#endif

CMUTIL_LogDefine("cmutils.nanojson")

CMUTIL_STATIC void CMUTIL_JsonToStringInternal(
		CMUTIL_Json *json, CMUTIL_String *buf, CMUTIL_Bool pretty, int depth);

typedef void (*CMUTIL_JsonValToStrFunc)(CMUTIL_String *str, CMUTIL_String *buf);

CMUTIL_STATIC void CMUTIL_JsonValueNumberToStr(
		CMUTIL_String *data, CMUTIL_String *buf)
{
	CMUTIL_CALL(buf, AddAnother, data);
}

CMUTIL_STATIC void CMUTIL_JsonValueStringToStr(
		CMUTIL_String *data, CMUTIL_String *buf)
{
	const char *sdata = CMUTIL_CALL(data, GetCString);
	CMUTIL_CALL(buf, AddPrint, "\"%s\"", sdata);
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
	CMUTIL_JsonValueType	type;
	CMUTIL_Mem_st			*memst;
} CMUTIL_JsonValue_Internal;

typedef struct CMUTIL_JsonObject_Internal {
	CMUTIL_JsonObject	base;
	CMUTIL_Map			*map;
	CMUTIL_Mem_st		*memst;
} CMUTIL_JsonObject_Internal;

typedef struct CMUTIL_JsonArray_Internal {
	CMUTIL_JsonArray	base;
	CMUTIL_Array		*arr;
	CMUTIL_Mem_st		*memst;
} CMUTIL_JsonArray_Internal;


CMUTIL_STATIC void CMUTIL_JsonIndent(CMUTIL_String *buf, int depth)
{
	for (; depth > 0; depth--)
		CMUTIL_CALL(buf, AddNString, "  ", 2);
}

CMUTIL_STATIC void CMUTIL_JsonValueToStringInternal(
		CMUTIL_Json *json, CMUTIL_String *buf, CMUTIL_Bool pretty, int depth)
{
	CMUTIL_JsonValue_Internal *ijval = (CMUTIL_JsonValue_Internal*)json;
	g_cmutil_jsonvaltostrf[ijval->type](ijval->data, buf);
	CMUTIL_UNUSED(pretty); CMUTIL_UNUSED(depth);
}

CMUTIL_STATIC void CMUTIL_JsonObjectToStringInternal(
		CMUTIL_Json *json, CMUTIL_String *buf, CMUTIL_Bool pretty, int depth)
{
	int i;
	CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
	CMUTIL_StringArray *keys = CMUTIL_CALL(jobj, GetKeys);
	CMUTIL_CALL(buf, AddChar, '{');
	if (CMUTIL_CALL(keys, GetSize) > 0) {
		for (i=0; i<CMUTIL_CALL(keys, GetSize); i++) {
			CMUTIL_String *key = CMUTIL_CALL(keys, GetAt, i);
			const char *skey = CMUTIL_CALL(key, GetCString);
			CMUTIL_Json *item = CMUTIL_CALL(jobj, Get, skey);
			if (i) CMUTIL_CALL(buf, AddChar, ',');
			if (pretty) {
				CMUTIL_CALL(buf, AddChar, '\n');
				CMUTIL_JsonIndent(buf, depth+1);
			}
			CMUTIL_JsonValueStringToStr(key, buf);
			CMUTIL_CALL(buf, AddChar, ':');
			CMUTIL_JsonToStringInternal(item, buf, pretty, depth+1);
		}
		if (pretty) {
			CMUTIL_CALL(buf, AddChar, '\n');
			CMUTIL_JsonIndent(buf, depth);
		}
	}
	CMUTIL_CALL(buf, AddChar, '}');
	CMUTIL_CALL(keys, Destroy);
}

CMUTIL_STATIC void CMUTIL_JsonArrayToStringInternal(
		CMUTIL_Json *json, CMUTIL_String *buf, CMUTIL_Bool pretty, int depth)
{
	CMUTIL_JsonArray *jarr = (CMUTIL_JsonArray*)json;
	int i, size = CMUTIL_CALL(jarr, GetSize);
	CMUTIL_CALL(buf, AddChar, '[');
	if (size > 0) {
		for (i=0; i<size; i++) {
			CMUTIL_Json *item = CMUTIL_CALL(jarr, Get, i);
			if (i) CMUTIL_CALL(buf, AddChar, ',');
			CMUTIL_JsonToStringInternal(item, buf, pretty, depth+1);
		}
	}
	CMUTIL_CALL(buf, AddChar, ']');
}

typedef void (*CMUTIL_JsonToStringFunc)(
		CMUTIL_Json *json, CMUTIL_String *buf, CMUTIL_Bool pretty, int depth);

static CMUTIL_JsonToStringFunc g_cmutil_jsontostrf[] = {
	CMUTIL_JsonValueToStringInternal,
	CMUTIL_JsonObjectToStringInternal,
	CMUTIL_JsonArrayToStringInternal
};

CMUTIL_STATIC void CMUTIL_JsonToStringInternal(
		CMUTIL_Json *json, CMUTIL_String *buf, CMUTIL_Bool pretty, int depth)
{
	g_cmutil_jsontostrf[CMUTIL_CALL(json, GetType)](json, buf, pretty, depth);
}

CMUTIL_STATIC void CMUTIL_JsonToString(
		CMUTIL_Json *json, CMUTIL_String *buf, CMUTIL_Bool pretty)
{
	return CMUTIL_JsonToStringInternal(json, buf, pretty, 0);
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonValueClone(CMUTIL_Json *json)
{
	CMUTIL_JsonValue_Internal *ival = (CMUTIL_JsonValue_Internal*)json;
	CMUTIL_JsonValue_Internal *res = (CMUTIL_JsonValue_Internal*)
			ival->memst->Alloc(sizeof(CMUTIL_JsonValue_Internal));
	memcpy(res, ival, sizeof(CMUTIL_JsonValue_Internal));
	res->data = CMUTIL_CALL(res->data, Clone);
	return (CMUTIL_Json*)res;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonObjectClone(CMUTIL_Json *json)
{
	CMUTIL_JsonObject_Internal *iobj = (CMUTIL_JsonObject_Internal*)json;
	CMUTIL_JsonObject_Internal *res = (CMUTIL_JsonObject_Internal*)
			CMUTIL_JsonObjectCreateInternal(iobj->memst);
	CMUTIL_StringArray *keys = CMUTIL_CALL(iobj->map, GetKeys);
	CMUTIL_Json *val, *dup;
	int i;
	for (i=0; i<CMUTIL_CALL(keys, GetSize); i++) {
		CMUTIL_String *key = CMUTIL_CALL(keys, GetAt, i);
		const char *skey = CMUTIL_CALL(key, GetCString);
		val = CMUTIL_CALL(&iobj->base, Get, skey);
		dup = CMUTIL_CALL(val, Clone);
		CMUTIL_CALL(&res->base, Put, skey, dup);
	}
	CMUTIL_CALL(keys, Destroy);
	return (CMUTIL_Json*)res;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonArrayClone(CMUTIL_Json *json)
{
	CMUTIL_JsonArray_Internal *iarr = (CMUTIL_JsonArray_Internal*)json;
	CMUTIL_JsonArray_Internal *res = (CMUTIL_JsonArray_Internal*)
			CMUTIL_JsonArrayCreateInternal(iarr->memst);
	int i;
	for (i=0; i<CMUTIL_CALL(iarr->arr, GetSize); i++) {
		CMUTIL_Json *item = CMUTIL_CALL(&iarr->base, Get, i);
		CMUTIL_Json *dup = CMUTIL_CALL(item, Clone);
		CMUTIL_CALL(&res->base, Add, dup);
	}
	return (CMUTIL_Json*)res;
}

static CMUTIL_Json *(*g_cmutil_json_clonefuncs[])() = {
	CMUTIL_JsonValueClone,
	CMUTIL_JsonObjectClone,
	CMUTIL_JsonArrayClone
};

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonClone(CMUTIL_Json *json)
{
	return g_cmutil_json_clonefuncs[CMUTIL_CALL(json, GetType)](json);
}

CMUTIL_STATIC void CMUTIL_JsonDestroyInternal(void *json) {
	CMUTIL_JsonDestroy(json);
}

CMUTIL_STATIC CMUTIL_JsonType CMUTIL_JsonValueGetType(CMUTIL_Json *json)
{
	CMUTIL_UNUSED(json);
	return CMUTIL_JsonTypeValue;
}

CMUTIL_STATIC void CMUTIL_JsonValueDestroy(CMUTIL_Json *json)
{
	CMUTIL_JsonValue_Internal *ijval = (CMUTIL_JsonValue_Internal*)json;
	if (ijval) {
		CMUTIL_CALL(ijval->data, Destroy);
		ijval->memst->Free(ijval);
	}
}

CMUTIL_STATIC CMUTIL_JsonValueType CMUTIL_JsonValueGetValueType(
		CMUTIL_JsonValue *jval)
{
	CMUTIL_JsonValue_Internal *ijval = (CMUTIL_JsonValue_Internal*)jval;
	return ijval->type;
}

CMUTIL_STATIC int64 CMUTIL_JsonValueGetLong(CMUTIL_JsonValue *jval)
{
	CMUTIL_JsonValue_Internal *ijval = (CMUTIL_JsonValue_Internal*)jval;
	return atoll(CMUTIL_CALL(ijval->data, GetCString));
}

CMUTIL_STATIC double CMUTIL_JsonValueGetDouble(CMUTIL_JsonValue *jval)
{
	CMUTIL_JsonValue_Internal *ijval = (CMUTIL_JsonValue_Internal*)jval;
	return atof(CMUTIL_CALL(ijval->data, GetCString));
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_JsonValueGetString(CMUTIL_JsonValue *jval)
{
	CMUTIL_JsonValue_Internal *ijval = (CMUTIL_JsonValue_Internal*)jval;
	return ijval->data;
}

CMUTIL_STATIC const char *CMUTIL_JsonValueGetCString(CMUTIL_JsonValue *jval)
{
	CMUTIL_JsonValue_Internal *ijval = (CMUTIL_JsonValue_Internal*)jval;
	return CMUTIL_CALL(ijval->data, GetCString);
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_JsonValueGetBoolean(CMUTIL_JsonValue *jval)
{
	CMUTIL_JsonValue_Internal *ijval = (CMUTIL_JsonValue_Internal*)jval;
	const char *sdata = CMUTIL_CALL(ijval->data, GetCString);
	return strchr("1tTyY", *sdata)? CMUTIL_True:CMUTIL_False;
}

CMUTIL_STATIC void CMUTIL_JsonValueSetBase(
		CMUTIL_JsonValue *jval, const char *buf, int length,
		CMUTIL_JsonValueType type)
{
	CMUTIL_JsonValue_Internal *ijval = (CMUTIL_JsonValue_Internal*)jval;
	CMUTIL_CALL(ijval->data, Clear);
	CMUTIL_CALL(ijval->data, AddNString, buf, length);
	ijval->type = type;
}

CMUTIL_STATIC void CMUTIL_JsonValueSetLong(CMUTIL_JsonValue *jval, int64 value)
{
	char buf[50];
	int size = sprintf(buf, PRINT64I, value);
	CMUTIL_JsonValueSetBase(jval, buf, size, CMUTIL_JsonValueLong);
}

CMUTIL_STATIC void CMUTIL_JsonValueSetDouble(
		CMUTIL_JsonValue *jval, double value)
{
	char buf[50];
	int size = sprintf(buf, "%lf", value);
	CMUTIL_JsonValueSetBase(jval, buf, size, CMUTIL_JsonValueDouble);
}

CMUTIL_STATIC void CMUTIL_JsonValueSetString(
		CMUTIL_JsonValue *jval, const char *value)
{
	CMUTIL_JsonValueSetBase(jval, value, strlen(value), CMUTIL_JsonValueString);
}

CMUTIL_STATIC void CMUTIL_JsonValueSetBoolean(
		CMUTIL_JsonValue *jval, CMUTIL_Bool value)
{
	const char *sval = value? "true":"false";
	CMUTIL_JsonValueSetBase(jval, sval, strlen(sval), CMUTIL_JsonValueBoolean);
}

CMUTIL_STATIC void CMUTIL_JsonValueSetNull(CMUTIL_JsonValue *jval)
{
	CMUTIL_JsonValueSetBase(jval, "null", 4, CMUTIL_JsonValueNull);
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
		CMUTIL_Mem_st *memst)
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
	return CMUTIL_JsonValueCreateInternal(__CMUTIL_Mem);
}


CMUTIL_STATIC CMUTIL_JsonType CMUTIL_JsonObjectGetType(CMUTIL_Json *json)
{
	CMUTIL_UNUSED(json);
	return CMUTIL_JsonTypeObject;
}

CMUTIL_STATIC void CMUTIL_JsonObjectDestroy(CMUTIL_Json *json)
{
	CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)json;
	if (ijobj) {
		if (ijobj->map)
			CMUTIL_CALL(ijobj->map, Destroy);
		ijobj->memst->Free(ijobj);
	}
}

CMUTIL_STATIC CMUTIL_StringArray *CMUTIL_JsonObjectGetKeys(
		CMUTIL_JsonObject *jobj)
{
	CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)jobj;
	return CMUTIL_CALL(ijobj->map, GetKeys);
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonObjectGet(
		CMUTIL_JsonObject *jobj, const char *key)
{
	CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)jobj;
	return (CMUTIL_Json*)CMUTIL_CALL(ijobj->map, Get, key);
}

#define CMUTIL_JsonObjectGetBody(jobj, key, method, v) do {					\
	CMUTIL_Json *json = CMUTIL_JsonObjectGet(jobj, key);					\
	if (json) {																\
		if (CMUTIL_CALL(json, GetType) == CMUTIL_JsonTypeValue)				\
			return CMUTIL_CALL((CMUTIL_JsonValue*)json, method );			\
		else																\
			CMLogErrorS("JsonObject item with key '%s' is not a value type.",\
						key);												\
	} else {																\
		CMLogErrorS("JsonObject has no item with key '%s'", key);			\
	}																		\
	return v; } while(0)


CMUTIL_STATIC int64 CMUTIL_JsonObjectGetLong(
		CMUTIL_JsonObject *jobj, const char *key)
{
	CMUTIL_JsonObjectGetBody(jobj, key, GetLong, 0);
}

CMUTIL_STATIC double CMUTIL_JsonObjectGetDouble(
		CMUTIL_JsonObject *jobj, const char *key)
{
	CMUTIL_JsonObjectGetBody(jobj, key, GetDouble, 0.0);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_JsonObjectGetString(
		CMUTIL_JsonObject *jobj, const char *key)
{
	CMUTIL_JsonObjectGetBody(jobj, key, GetString, NULL);
}

CMUTIL_STATIC const char *CMUTIL_JsonObjectGetCString(
		CMUTIL_JsonObject *jobj, const char *key)
{
	CMUTIL_JsonObjectGetBody(jobj, key, GetCString, NULL);
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_JsonObjectGetBoolean(
		CMUTIL_JsonObject *jobj, const char *key)
{
	CMUTIL_JsonObjectGetBody(jobj, key, GetBoolean, CMUTIL_False);
}

CMUTIL_STATIC void CMUTIL_JsonObjectPut(
		CMUTIL_JsonObject *jobj, const char *key, CMUTIL_Json *json)
{
	CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)jobj;
	json = CMUTIL_CALL(ijobj->map, Put, key, json);
	// destory previous mapped item
	if (json) CMUTIL_CALL(json, Destroy);
}

#define CMUTIL_JsonObjectPutBody(jobj, key, method, value) do {				\
	CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)jobj;	\
	CMUTIL_JsonValue *jval = CMUTIL_JsonValueCreateInternal(ijobj->memst);	\
	CMUTIL_CALL(jval, method, value);										\
	CMUTIL_CALL(jobj, Put, key, (CMUTIL_Json*)jval);						\
	} while(0)

CMUTIL_STATIC void CMUTIL_JsonObjectPutLong(
		CMUTIL_JsonObject *jobj, const char *key, int64 value)
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
		CMUTIL_JsonObject *jobj, const char *key, CMUTIL_Bool value)
{
	CMUTIL_JsonObjectPutBody(jobj, key, SetBoolean, value);
}

CMUTIL_STATIC void CMUTIL_JsonObjectPutNull(
		CMUTIL_JsonObject *jobj, const char *key)
{
	CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)jobj;
	CMUTIL_JsonValue *jval = CMUTIL_JsonValueCreateInternal(ijobj->memst);
	CMUTIL_CALL(jval, SetNull);
	CMUTIL_CALL(jobj, Put, key, (CMUTIL_Json*)jval);
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonObjectRemove(
		CMUTIL_JsonObject *jobj, const char *key)
{
	CMUTIL_JsonObject_Internal *ijobj = (CMUTIL_JsonObject_Internal*)jobj;
	return (CMUTIL_Json*)CMUTIL_CALL(ijobj->map, Remove, key);
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
	CMUTIL_JsonObjectRemove
};

CMUTIL_JsonObject *CMUTIL_JsonObjectCreateInternal(CMUTIL_Mem_st *memst)
{
	CMUTIL_JsonObject_Internal *res =
			memst->Alloc(sizeof(CMUTIL_JsonObject_Internal));
	memset(res, 0x0, sizeof(CMUTIL_JsonObject_Internal));
	memcpy(res, &g_cmutil_jsonobject, sizeof(CMUTIL_JsonObject));
	res->memst = memst;
	res->map = CMUTIL_MapCreateInternal(memst, 256, CMUTIL_JsonDestroyInternal);
	return (CMUTIL_JsonObject*)res;
}

CMUTIL_JsonObject *CMUTIL_JsonObjectCreate()
{
	return CMUTIL_JsonObjectCreateInternal(__CMUTIL_Mem);
}



CMUTIL_STATIC CMUTIL_JsonType CMUTIL_JsonArrayGetType(CMUTIL_Json *json)
{
	CMUTIL_UNUSED(json);
	return CMUTIL_JsonTypeArray;
}

CMUTIL_STATIC void CMUTIL_JsonArrayDestroy(CMUTIL_Json *json)
{
	CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)json;
	if (ijarr) {
		if (ijarr->arr) CMUTIL_CALL(ijarr->arr, Destroy);
		ijarr->memst->Free(ijarr);
	}
}

CMUTIL_STATIC int CMUTIL_JsonArrayGetSize(CMUTIL_JsonArray *jarr)
{
	CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)jarr;
	return CMUTIL_CALL(ijarr->arr, GetSize);
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonArrayGet(
		CMUTIL_JsonArray *jarr, int index)
{
	CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)jarr;
	if (index < CMUTIL_CALL(ijarr->arr, GetSize))
		return (CMUTIL_Json*)CMUTIL_CALL(ijarr->arr, GetAt, index);
	CMLogErrorS("JsonArray index out of bound(%d) total %d.",
				index, CMUTIL_CALL(ijarr->arr, GetSize));
	return NULL;
}

#define CMUTIL_JsonArrayGetBody(jarr, index, method, v) do {				\
	CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)jarr;	\
	if (CMUTIL_CALL(ijarr->arr, GetSize) > index) {							\
		CMUTIL_Json* json = CMUTIL_CALL(jarr, Get, index);					\
		if (CMUTIL_CALL(json, GetType) == CMUTIL_JsonTypeValue) {			\
			return CMUTIL_CALL((CMUTIL_JsonValue*)json, method);			\
		} else {															\
			CMLogErrorS("item type is not 'Value' type.");					\
		}																	\
	} else {																\
		CMLogErrorS("JsonArray index out of bound(%d), total %d.",			\
				index, CMUTIL_CALL(ijarr->arr, GetSize));					\
	} return v;} while(0)

CMUTIL_STATIC int64 CMUTIL_JsonArrayGetLong(CMUTIL_JsonArray *jarr, int index)
{
	CMUTIL_JsonArrayGetBody(jarr, index, GetLong, 0);
}

CMUTIL_STATIC double CMUTIL_JsonArrayGetDouble(
		CMUTIL_JsonArray *jarr, int index)
{
	CMUTIL_JsonArrayGetBody(jarr, index, GetDouble, 0.0);
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_JsonArrayGetString(
		CMUTIL_JsonArray *jarr, int index)
{
	CMUTIL_JsonArrayGetBody(jarr, index, GetString, NULL);
}

CMUTIL_STATIC const char *CMUTIL_JsonArrayGetCString(
		CMUTIL_JsonArray *jarr, int index)
{
	CMUTIL_JsonArrayGetBody(jarr, index, GetCString, NULL);
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_JsonArrayGetBoolean(
		CMUTIL_JsonArray *jarr, int index)
{
	CMUTIL_JsonArrayGetBody(jarr, index, GetBoolean, CMUTIL_False);
}

CMUTIL_STATIC void CMUTIL_JsonArrayAdd(
		CMUTIL_JsonArray *jarr, CMUTIL_Json *json)
{
	CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)jarr;
	CMUTIL_CALL(ijarr->arr, Add, json);
}

#define CMUTIL_JsonArrayAddBody(jarr, value, method)	do{					\
	CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)jarr;	\
	CMUTIL_JsonValue *json = CMUTIL_JsonValueCreateInternal(ijarr->memst);	\
	CMUTIL_CALL(json, method, value);										\
	CMUTIL_JsonArrayAdd(jarr, (CMUTIL_Json*)json); } while(0)

CMUTIL_STATIC void CMUTIL_JsonArrayAddLong(
		CMUTIL_JsonArray *jarr, int64 value)
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
		CMUTIL_JsonArray *jarr, CMUTIL_Bool value)
{
	CMUTIL_JsonArrayAddBody(jarr, value, SetBoolean);
}

CMUTIL_STATIC void CMUTIL_JsonArrayAddNull(CMUTIL_JsonArray *jarr)
{
	CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)jarr;
	CMUTIL_JsonValue *json = CMUTIL_JsonValueCreateInternal(ijarr->memst);
	CMUTIL_CALL(json, SetNull);
	CMUTIL_JsonArrayAdd(jarr, (CMUTIL_Json*)json);
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonArrayRemove(
		CMUTIL_JsonArray *jarr, int index)
{
	CMUTIL_JsonArray_Internal *ijarr = (CMUTIL_JsonArray_Internal*)jarr;
	return (CMUTIL_Json*)CMUTIL_CALL(ijarr->arr, RemoveAt, index);
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
	CMUTIL_JsonArrayRemove
};

CMUTIL_JsonArray *CMUTIL_JsonArrayCreateInternal(CMUTIL_Mem_st *memst)
{
	CMUTIL_JsonArray_Internal *res =
			memst->Alloc(sizeof(CMUTIL_JsonArray_Internal));
	memset(res, 0x0, sizeof(CMUTIL_JsonArray_Internal));
	memcpy(res, &g_cmutil_jsonarray, sizeof(CMUTIL_JsonArray));
	res->memst = memst;
	res->arr = CMUTIL_ArrayCreateInternal(
				memst, 10, NULL, CMUTIL_JsonDestroyInternal);
	return (CMUTIL_JsonArray*)res;
}

CMUTIL_JsonArray *CMUTIL_JsonArrayCreate()
{
	return CMUTIL_JsonArrayCreateInternal(__CMUTIL_Mem);
}


static const char* JSN_SPACES = " \t\r\n";
static const char* JSN_DELIMS = ":,[]{}";

typedef struct CMUTIL_JsonParser {
	const char *orig, *curr;
	int remain, linecnt;
	CMUTIL_Bool silent;
	CMUTIL_Mem_st *memst;
} CMUTIL_JsonParser;

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseValue(CMUTIL_JsonParser *pctx);
CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseObject(CMUTIL_JsonParser *pctx);
CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseArray(CMUTIL_JsonParser *pctx);

CMUTIL_STATIC void CMUTIL_JsonParseConsume(CMUTIL_JsonParser *pctx, int count)
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

CMUTIL_STATIC CMUTIL_Bool CMUTIL_JsonStartsWith(
		CMUTIL_JsonParser *pctx, const char *str)
{
	register const char *p = str, *q = pctx->curr;
	while (*p && *p == *q) p++, q++;
	return *p? CMUTIL_False:CMUTIL_True;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_JsonSkipSpaces(CMUTIL_JsonParser *pctx)
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
	return pctx->remain > 0 ? CMUTIL_True:CMUTIL_False;
}

#define CMUTIL_JSON_PARSE_ERROR(a, msg) do {								\
	char buf[30] = {0,}; strncat(buf, a->curr, 25); strcat(buf, "...");		\
	if (!a->silent) CMLogErrorS("parse error in line %d before '%s': %s",	\
				a->linecnt, buf, msg); } while(0)

CMUTIL_STATIC CMUTIL_Bool CMUTIL_JsonParseEscape(
		CMUTIL_JsonParser *pctx, CMUTIL_String *sbuf)
{
	char buf[3];
	CMUTIL_JsonParseConsume(pctx, 1);
	if (pctx->remain <= 0) {
		CMUTIL_JSON_PARSE_ERROR(pctx, "unexpected end.");
		return CMUTIL_False;
	}
	switch (*(pctx->curr)) {
	case 't': CMUTIL_CALL(sbuf, AddChar, '\t'); break;
	case 'r': CMUTIL_CALL(sbuf, AddChar, '\r'); break;
	case 'n': CMUTIL_CALL(sbuf, AddChar, '\n'); break;
	case 'u':
		// unicodes are converted to 2 byte
		CMUTIL_StringHexToBytes(buf, pctx->curr+1, 4);
		CMUTIL_CALL(sbuf, AddNString, buf, 2);
		CMUTIL_JsonParseConsume(pctx, 4);
		break;
	default: CMUTIL_CALL(sbuf, AddChar, *(pctx->curr)); break;
	}
	return CMUTIL_True;
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

CMUTIL_STATIC CMUTIL_Bool CMUTIL_JsonParseString(
		CMUTIL_JsonParser *pctx, CMUTIL_String **res, CMUTIL_Bool *isconst)
{
	int isend = 0;
	CMUTIL_String *sbuf = NULL;
	int isstquot = 1;

	// remove preceding spaces
	if (!CMUTIL_JsonSkipSpaces(pctx))
		return CMUTIL_False;
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
				CMUTIL_CALL(sbuf, Destroy);
				return CMUTIL_False;
			}
		} else if (isstquot && *(pctx->curr) == '\"') {
			// string end
			isend = 1;
		} else if (!isstquot && strchr(JSN_DELIMS, *(pctx->curr))) {
			// string end
			isend = 1;
			break;
		} else {
			CMUTIL_CALL(sbuf, AddChar, *(pctx->curr));
		}
		CMUTIL_JsonParseConsume(pctx, 1);
	}
	if (res)
		*res = sbuf;
	else
		CMUTIL_CALL(sbuf, Destroy);
	if (isconst)
		*isconst = isstquot? CMUTIL_False:CMUTIL_True;
	return CMUTIL_True;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_JsonParseConst(
		CMUTIL_JsonParser *pctx, CMUTIL_JsonValue *jval, CMUTIL_String *str)
{
	const char *cstr = CMUTIL_CALL(str, GetCString);
	cstr = CMUTIL_StrTrim((char*)cstr);
	if (CMUTIL_CALL(str, GetSize) < 5) {
		if (strcasecmp(cstr, "true") == 0) {
			CMUTIL_CALL(jval, SetBoolean, CMUTIL_True);
			return CMUTIL_True;
		} else if (strcasecmp(cstr, "false") == 0) {
			CMUTIL_CALL(jval, SetBoolean, CMUTIL_False);
			return CMUTIL_True;
		} else if (strcasecmp(cstr, "null") == 0) {
			CMUTIL_CALL(jval, SetNull);
			return CMUTIL_True;
		}
	}
	if (strchr("tTfF", *cstr)) {
		if (strcasecmp(cstr, "true") == 0) {
			CMUTIL_CALL(jval, SetBoolean, CMUTIL_True);
		} else if (strcasecmp(cstr, "false") == 0) {
			CMUTIL_CALL(jval, SetBoolean, CMUTIL_False);
		} else {
			if (!pctx->silent) CMLogError("cannot parse constant '%s'", cstr);
			return CMUTIL_False;
		}
	} else if (strchr("nN", *cstr)) {
		if (strcasecmp(cstr, "null") == 0) {
			CMUTIL_CALL(jval, SetNull);
		} else {
			if (!pctx->silent) CMLogError("cannot parse constant '%s'", cstr);
			return CMUTIL_False;
		}
	} else if (strchr(cstr, '.')) {
		double value = 0.0;
		if (sscanf(cstr, "%lf", &value) != 1) {
			if (!pctx->silent) CMLogError("cannot parse constant '%s'", cstr);
			return CMUTIL_False;
		}
		CMUTIL_CALL(jval, SetDouble, value);
	} else {
		int64 value = 0;
		if (sscanf(cstr, PRINT64I, &value) != 1) {
			if (!pctx->silent) CMLogError("cannot parse constant '%s'", cstr);
			return CMUTIL_False;
		}
		CMUTIL_CALL(jval, SetLong, value);
	}
	return CMUTIL_True;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseValue(CMUTIL_JsonParser *pctx)
{
	CMUTIL_JsonValue *res = NULL;
	CMUTIL_String *sbuf = NULL;
	CMUTIL_Bool isconst = CMUTIL_False;

	// remove preceding spaces
	if (!CMUTIL_JsonSkipSpaces(pctx))
		return NULL;
	if (!CMUTIL_JsonParseString(pctx, &sbuf, &isconst))
		return NULL;
	res = CMUTIL_JsonValueCreateInternal(pctx->memst);
	if (isconst) {
		if (!CMUTIL_JsonParseConst(pctx, res, sbuf)) {
			CMUTIL_CALL((CMUTIL_Json*)res, Destroy);
			res = NULL;
		}
	} else {
		const char *sval = CMUTIL_CALL(sbuf, GetCString);
		CMUTIL_CALL(res, SetString, sval);
	}
	if (sbuf) CMUTIL_CALL(sbuf, Destroy);
	return (CMUTIL_Json*)res;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseObject(CMUTIL_JsonParser *pctx)
{
	int isend = 0;
	CMUTIL_String *name = NULL;
	CMUTIL_JsonObject *res = NULL;
	CMUTIL_Bool succeeded = CMUTIL_False;

	if (*(pctx->curr) != '{') {
		CMUTIL_JSON_PARSE_ERROR(pctx, "JSON object does not starts with '{'");
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

		sname = CMUTIL_CALL(name, GetCString);

		CMUTIL_CALL(res, Put, sname, value);
		CMUTIL_CALL(name, Destroy);
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
	succeeded = CMUTIL_True;
ENDPOINT:
	if (name) CMUTIL_CALL(name, Destroy);
	if (!succeeded && res) {
		CMUTIL_JsonDestroy(res);
		res = NULL;
	}
	return (CMUTIL_Json*)res;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_JsonParseArray(CMUTIL_JsonParser *pctx)
{
	CMUTIL_JsonArray *res = NULL;
	CMUTIL_Bool succeeded = CMUTIL_False;

	if (*(pctx->curr) != '[') {
		CMUTIL_JSON_PARSE_ERROR(pctx, "JSON array does not starts with '['");
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
		CMUTIL_CALL(res, Add, item);

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
	succeeded = CMUTIL_True;
ENDPOINT:
	if (!succeeded && res) {
		CMUTIL_JsonDestroy(res);
		res = NULL;
	}
	return (CMUTIL_Json*)res;
}

CMUTIL_Json *CMUTIL_JsonParseInternal(
		CMUTIL_Mem_st *memst, CMUTIL_String *jsonstr, CMUTIL_Bool silent)
{
	CMUTIL_JsonParser parser = {
		CMUTIL_CALL(jsonstr, GetCString),
		CMUTIL_CALL(jsonstr, GetCString),
		CMUTIL_CALL(jsonstr, GetSize),
		0,
		silent,
		memst
	};
	return CMUTIL_JsonParseBase(&parser);
}

CMUTIL_Json *CMUTIL_JsonParse(CMUTIL_String *jsonstr)
{
	return CMUTIL_JsonParseInternal(__CMUTIL_Mem, jsonstr, CMUTIL_False);
}
