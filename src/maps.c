
#include "functions.h"

//*****************************************************************************
// CMUTIL_Map implementation
//*****************************************************************************

typedef struct CMUTIL_MapItem {
    char	*key;
    void	*value;
} CMUTIL_MapItem;

typedef struct CMUTIL_Map_Internal {
    CMUTIL_Map		base;
    CMUTIL_Array	**buckets;
    int				bucketsize;
    int				size;
    CMUTIL_Array	*keyset;
    void			(*freecb)(void*);
    CMUTIL_Mem_st	*memst;
} CMUTIL_Map_Internal;

CMUTIL_STATIC int CMUTIL_MapHash(const char *in, int bucketsz)
{
    register const char *p = in;
    unsigned h=0, g=0;
    while (*p) {
        h=(h<<4)+(*p);
        if((g=h&0xf0000000) != 0) {
            h ^= (g>>24);
            h ^= g;
        }
        p++;
    }
    return (h%bucketsz);
}

CMUTIL_STATIC int CMUTIL_MapComparator(const void *a, const void *b)
{
    const CMUTIL_MapItem *ia = (const CMUTIL_MapItem*)a;
    const CMUTIL_MapItem *ib = (const CMUTIL_MapItem*)b;

    return strcmp(ia->key, ib->key);
}

CMUTIL_STATIC void *CMUTIL_MapPut(
        CMUTIL_Map *map, const char* key, void* value)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    int index = CMUTIL_MapHash(key, imap->bucketsize);
    CMUTIL_Array *bucket = imap->buckets[index];
    CMUTIL_MapItem *item = imap->memst->Alloc(sizeof(CMUTIL_MapItem));
    CMUTIL_MapItem *ires = NULL;
    void *res = NULL;

    memset(item, 0x0, sizeof(CMUTIL_MapItem));

    if (!bucket) {
        bucket = CMUTIL_ArrayCreateInternal(
                    imap->memst, 5, CMUTIL_MapComparator, NULL);
        imap->buckets[index] = bucket;
    }

    item->key = imap->memst->Strdup(key);
    item->value = value;
    ires = CMUTIL_CALL(bucket, Add, item);
    CMUTIL_CALL(imap->keyset, Add, item->key);
    if (ires) {
        res = ires->value;
        imap->memst->Free(ires->key);
        imap->memst->Free(ires);
    } else {
        imap->size++;
    }

    return res;
}

CMUTIL_STATIC void CMUTIL_MapPutAll(
        CMUTIL_Map *map, CMUTIL_Map *src)
{
    CMUTIL_StringArray *keys = CMUTIL_CALL(src, GetKeys);
    int i;
    for (i=0; i<CMUTIL_CALL(keys, GetSize); i++) {
        CMUTIL_String *skey = CMUTIL_CALL(keys, GetAt, i);
        const char *key = CMUTIL_CALL(skey, GetCString);
        void *val = CMUTIL_CALL(src, Get, key);
        CMUTIL_CALL(map, Put, key, val);
    }
    CMUTIL_CALL(keys, Destroy);
}

CMUTIL_STATIC void *CMUTIL_MapGet(CMUTIL_Map *map, const char* key)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    int index = CMUTIL_MapHash(key, imap->bucketsize);
    CMUTIL_Array *bucket = imap->buckets[index];
    void *res = NULL;

    if (bucket) {
        CMUTIL_MapItem *ires = NULL;
        CMUTIL_MapItem item = {(char*)key, NULL};
        int idx = 0;
        ires = CMUTIL_CALL(bucket, Find, &item, &idx);
        if (ires)
            res = ires->value;
    }

    return res;
}

CMUTIL_STATIC void *CMUTIL_MapRemove(CMUTIL_Map *map, const char* key)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    int index = CMUTIL_MapHash(key, imap->bucketsize);
    CMUTIL_Array *bucket = imap->buckets[index];
    void *res = NULL;

    if (bucket) {
        CMUTIL_MapItem *ires = NULL;
        CMUTIL_MapItem item = {(char*)key, NULL};
        ires = (CMUTIL_MapItem*)CMUTIL_CALL(bucket, Remove, &item);
        if (ires) {
            CMUTIL_CALL(imap->keyset, Remove, key);
            res = ires->value;
            imap->memst->Free(ires->key);
            imap->memst->Free(ires);
        }

        if (res)
            imap->size--;
    }

    return res;
}

CMUTIL_STATIC CMUTIL_StringArray *CMUTIL_MapGetKeys(CMUTIL_Map *map)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    CMUTIL_StringArray *res = CMUTIL_StringArrayCreateInternal(
            imap->memst, CMUTIL_CALL(imap->keyset, GetSize));
    int i;
    for (i=0; i<CMUTIL_CALL(imap->keyset, GetSize); i++) {
        const char *key = (const char*)CMUTIL_CALL(imap->keyset, GetAt, i);
        CMUTIL_String *skey = CMUTIL_StringCreateInternal(
                    imap->memst, 20, key);
        CMUTIL_CALL(res, Add, skey);
    }

    return res;
}

CMUTIL_STATIC int CMUTIL_MapGetSize(CMUTIL_Map *map)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    return imap->size;
}

typedef struct CMUTIL_MapIter_st {
    CMUTIL_Iterator		base;
    CMUTIL_Map_Internal	*imap;
    CMUTIL_StringArray	*keys;
    CMUTIL_Iterator		*keyiter;
} CMUTIL_MapIter_st;

CMUTIL_STATIC CMUTIL_Bool CMUTIL_MapIterHasNext(CMUTIL_Iterator *iter)
{
    CMUTIL_MapIter_st *iiter = (CMUTIL_MapIter_st*)iter;
    return CMUTIL_CALL(iiter->keyiter, HasNext);
}

CMUTIL_STATIC void *CMUTIL_MapIterNext(CMUTIL_Iterator *iter)
{
    CMUTIL_MapIter_st *iiter = (CMUTIL_MapIter_st*)iter;
    CMUTIL_String *key = (CMUTIL_String*)CMUTIL_CALL(iiter->keyiter, Next);
    if (key) {
        const char *skey = CMUTIL_CALL(key, GetCString);
        return CMUTIL_CALL((CMUTIL_Map*)iiter->imap, Get, skey);
    }
    return NULL;
}

CMUTIL_STATIC void CMUTIL_MapIterDestroy(CMUTIL_Iterator *iter)
{
    CMUTIL_MapIter_st *iiter = (CMUTIL_MapIter_st*)iter;
    CMUTIL_CALL(iiter->keyiter, Destroy);
    CMUTIL_CALL(iiter->keys, Destroy);
    iiter->imap->memst->Free(iiter);
}

static CMUTIL_Iterator g_cmutil_map_iterator = {
    CMUTIL_MapIterHasNext,
    CMUTIL_MapIterNext,
    CMUTIL_MapIterDestroy
};

CMUTIL_STATIC CMUTIL_Iterator *CMUTIL_MapIterator(CMUTIL_Map *map)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    CMUTIL_MapIter_st *res = imap->memst->Alloc(sizeof(CMUTIL_MapIter_st));
    memset(res, 0x0, sizeof(CMUTIL_MapIter_st));
    memcpy(res, &g_cmutil_map_iterator, sizeof(CMUTIL_Iterator));
    res->imap = imap;
    res->keys = CMUTIL_CALL(map, GetKeys);
    res->keyiter = CMUTIL_CALL(res->keys, Iterator);

    return (CMUTIL_Iterator*)res;
}

CMUTIL_STATIC void CMUTIL_MapClearBase(CMUTIL_Map *map, CMUTIL_Bool freedata)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    int i, j;

    for (i=0; i<imap->bucketsize; i++) {
        CMUTIL_Array *bucket = imap->buckets[i];
        if (bucket) {
            for (j=0; j<CMUTIL_CALL(bucket, GetSize); j++) {
                CMUTIL_MapItem *item =
                        (CMUTIL_MapItem*)CMUTIL_CALL(bucket, GetAt, j);
                imap->memst->Free(item->key);
                if (imap->freecb && freedata)
                    imap->freecb(item->value);
                imap->memst->Free(item);
            }
            bucket->Clear(bucket);
        }
    }

    CMUTIL_CALL(imap->keyset, Clear);
    imap->size = 0;
}

CMUTIL_STATIC void CMUTIL_MapClear(CMUTIL_Map *map)
{
    CMUTIL_MapClearBase(map, CMUTIL_True);
}

CMUTIL_STATIC void CMUTIL_MapClearLink(CMUTIL_Map *map)
{
    CMUTIL_MapClearBase(map, CMUTIL_False);
}

CMUTIL_STATIC void CMUTIL_MapDestroy(CMUTIL_Map *map)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    int i, j;

    for (i=0; i<imap->bucketsize; i++) {
        CMUTIL_Array *bucket = imap->buckets[i];
        if (bucket) {
            for (j=0; j<CMUTIL_CALL(bucket, GetSize); j++) {
                CMUTIL_MapItem *item =
                        (CMUTIL_MapItem*)CMUTIL_CALL(bucket, GetAt, j);
                imap->memst->Free(item->key);
                if (imap->freecb)
                    imap->freecb(item->value);
                imap->memst->Free(item);
            }
            CMUTIL_CALL(bucket, Destroy);
        }
    }
    CMUTIL_CALL(imap->keyset, Destroy);
    imap->memst->Free(imap->buckets);
    imap->memst->Free(imap);
}

CMUTIL_STATIC void CMUTIL_MapPrintTo(CMUTIL_Map *map, CMUTIL_String *out)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    int i, j;

    CMUTIL_CALL(out, AddChar, '{');
    for (i=0; i<imap->bucketsize; i++) {
        CMUTIL_Array *bucket = imap->buckets[i];
        if (bucket) {
            for (j=0; j<CMUTIL_CALL(bucket, GetSize); j++) {
                CMUTIL_MapItem *item =
                        (CMUTIL_MapItem*)CMUTIL_CALL(bucket, GetAt, j);
                if (CMUTIL_CALL(out, GetSize) > 1)
                    CMUTIL_CALL(out, AddChar, ',');
                CMUTIL_CALL(out, AddString, item->key);
            }
            CMUTIL_CALL(bucket, Destroy);
        }
    }
    CMUTIL_CALL(out, AddChar, '}');
    CMUTIL_CALL(imap->keyset, Destroy);
    imap->memst->Free(imap->buckets);
    imap->memst->Free(imap);
}

static CMUTIL_Map g_cmutil_map = {
    CMUTIL_MapPut,
    CMUTIL_MapPutAll,
    CMUTIL_MapGet,
    CMUTIL_MapRemove,
    CMUTIL_MapGetKeys,
    CMUTIL_MapGetSize,
    CMUTIL_MapIterator,
    CMUTIL_MapClear,
    CMUTIL_MapClearLink,
    CMUTIL_MapDestroy,
    CMUTIL_MapPrintTo
};

CMUTIL_Map *CMUTIL_MapCreateInternal(
        CMUTIL_Mem_st *memst, int bucketsize, void(*freecb)(void*))
{
    CMUTIL_Map_Internal *imap = memst->Alloc(sizeof(CMUTIL_Map_Internal));
    memset(imap, 0x0, sizeof(CMUTIL_Map_Internal));

    memcpy(imap, &g_cmutil_map, sizeof(CMUTIL_Map));
    imap->buckets = memst->Alloc(sizeof(CMUTIL_Array)*bucketsize);
    memset(imap->buckets, 0x0, sizeof(CMUTIL_Array)*bucketsize);
    imap->bucketsize = bucketsize;
    imap->keyset = CMUTIL_ArrayCreateInternal(
                memst, bucketsize, (int(*)(const void*,const void*))strcmp,
                NULL);
    imap->memst = memst;
    imap->freecb = freecb;

    return (CMUTIL_Map*)imap;
}

CMUTIL_Map *CMUTIL_MapCreateEx(int bucketsize, void(*freecb)(void*))
{
    return CMUTIL_MapCreateInternal(__CMUTIL_Mem, bucketsize, freecb);
}
