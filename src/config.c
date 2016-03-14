
#include "functions.h"

typedef enum CMUTIL_ConfType {
	ConfItem_Comment,
	ConfItem_Pair
} CMUTIL_ConfType;

typedef struct CMUTIL_ConfItem {
	char			*key;
	char			*comment;
	CMUTIL_ConfType	type;
	int				index;
	CMUTIL_Mem_st	*memst;
} CMUTIL_ConfItem;

typedef struct CMUTIL_Config_Internal {
	CMUTIL_Map		*confs;
	CMUTIL_Array	*sequence;
	CMUTIL_Map		*revconf;
	int maxkeylen;
	CMUTIL_Mem_st	*memst;
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
		CMUTIL_CALL(rstr, AddNString, in, (int)(last - in));
	return iscont;

}

CMUTIL_STATIC void CNConfItemFree(void *data)
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
			CMUTIL_CALL(iconf->confs, Destroy);

		if (iconf->sequence)
			CMUTIL_CALL(iconf->sequence, Destroy);

		if (iconf->revconf)
			CMUTIL_CALL(iconf->revconf, Destroy);

		iconf->memst->Free(iconf);
	}
}

#define DOFAILED	if (!*p) do { \
		failed = 1; CNConfItemFree(item); goto FAILED; } while(0)

void CMUTIL_ConfigSave(CMUTIL_Config *conf, const char *confpath)
{
	if (conf) {
		CMUTIL_Config_Internal *iconf = (CMUTIL_Config_Internal*)conf;
		FILE *f = NULL;
		char fmt[50];
		int i;
		f = fopen(confpath, "wb");
		for (i = 0; i < CMUTIL_CALL(iconf->sequence, GetSize); i++) {
			CMUTIL_ConfItem *item = (CMUTIL_ConfItem*)CMUTIL_CALL(
					iconf->sequence, GetAt, i);
			if (item->type == ConfItem_Pair) {
				char *v = (char*)CMUTIL_CALL(iconf->confs, Get, item->key);
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

const char *CMUTIL_ConfigGet(CMUTIL_Config *conf, const char *key)
{
	CMUTIL_Config_Internal *iconf = (CMUTIL_Config_Internal*)conf;
	return (char*)CMUTIL_CALL(iconf->confs, Get, key);
}

void CMUTIL_ConfigSet(CMUTIL_Config *conf, const char *key, const char *value)
{
	CMUTIL_Config_Internal *iconf = (CMUTIL_Config_Internal*)conf;
	char *prev = (char*)CMUTIL_CALL(
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
		CMUTIL_CALL(iconf->sequence, Add, item);
		CMUTIL_CALL(iconf->revconf, Put, item->key, item);
	}
}

long CMUTIL_ConfigGetLong(CMUTIL_Config *conf, const char *key)
{
	return atol(CMUTIL_CALL(conf, Get, key));
}

void CMUTIL_ConfigSetLong(CMUTIL_Config *conf, const char *key, long value)
{
	char buf[20];
	sprintf(buf, "%ld", value);
	CMUTIL_CALL(conf, Set, key, buf);
}

double CMUTIL_ConfigGetDouble(CMUTIL_Config *conf, const char *key)
{
	return atof(CMUTIL_CALL(conf, Get, key));
}

void CMUTIL_ConfigSetDouble(CMUTIL_Config *conf, const char *key, double value)
{
	char buf[30];
	sprintf(buf, "%f", value);
	CMUTIL_CALL(conf, Set, key, buf);
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

CMUTIL_Config *CMUTIL_ConfigCreateInternal(CMUTIL_Mem_st *memst)
{
	CMUTIL_Config_Internal *res =
		(CMUTIL_Config_Internal*)memst->Alloc(sizeof(CMUTIL_Config_Internal));
	memset(res, 0x0, sizeof(CMUTIL_Config_Internal));
	memcpy(res, &g_cmutil_config, sizeof(CMUTIL_Config));
	res->memst = memst;
	res->confs = CMUTIL_MapCreateInternal(
				memst, CMUTIL_MAP_DEFAULT, memst->Free);
	res->sequence = CMUTIL_ArrayCreateInternal(
				memst, 100, NULL, CNConfItemFree);
	res->revconf = CMUTIL_MapCreateInternal(memst, CMUTIL_MAP_DEFAULT, NULL);
	return (CMUTIL_Config*)res;
}

CMUTIL_Config *CMUTIL_ConfigCreate()
{
	return CMUTIL_ConfigCreateInternal(__CMUTIL_Mem);
}

CMUTIL_Config *CMUTIL_ConfigLoadInternal(
		CMUTIL_Mem_st *memst, const char *fconf)
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

			p = (char*)CMUTIL_CALL(str, GetCString);
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
				CMUTIL_CALL(res->confs, Put, name, memst->Strdup(v));
				item->key = memst->Strdup(name);
				keylen = (int)strlen(item->key);
				if (res->maxkeylen < keylen)
					res->maxkeylen = keylen;
				item->type = ConfItem_Pair;
				CMUTIL_CALL(res->revconf, Put, item->key, item);
			}
			}

			item->index = CMUTIL_CALL(res->sequence, GetSize);
			CMUTIL_CALL(res->sequence, Add, item);
			CMUTIL_CALL(str, Clear);
		}
	FAILED:
		if (str)
			CMUTIL_CALL(str, Destroy);
		fclose(f);
	}

	if (failed && res) {
		CMUTIL_CALL((CMUTIL_Config*)res, Destroy);
		res = NULL;
	}

	return (CMUTIL_Config*)res;
}

CMUTIL_Config *CMUTIL_ConfigLoad(const char *fconf)
{
	return CMUTIL_ConfigLoadInternal(__CMUTIL_Mem, fconf);
}
