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

#include "functions.h"

CMUTIL_LogDefine("cmutil.maps")

//*****************************************************************************
// CMUTIL_Map implementation
//*****************************************************************************

typedef struct CMUTIL_MapItem {
    char    *key;
    void    *value;
} CMUTIL_MapItem;

typedef struct CMUTIL_MapCItem {
    const char  *key;
    void        *value;
} CMUTIL_MapCItem;

typedef struct CMUTIL_Map_Internal {
    CMUTIL_Map      base;
    CMUTIL_Array    **buckets;
    size_t          size;
    CMUTIL_Array    *keyset;
    void            (*freecb)(void*);
    CMUTIL_Mem      *memst;
    CMBool          isucase;
    uint32_t        bucketsize;
} CMUTIL_Map_Internal;

CMUTIL_STATIC uint32_t CMUTIL_MapHash(const char *in, uint32_t bucketsz)
{
    register const char *p = in;
    unsigned h=0, g=0;
    while (*p) {
        h=(h<<4)+(uint32_t)(*p);
        if((g=h&0xf0000000) != 0) {
            h ^= (g>>24);
            h ^= g;
        }
        p++;
    }
    return h%bucketsz;
}

CMUTIL_STATIC uint32_t CMUTIL_MapHashUpper(const char *in, uint32_t bucketsz)
{
    register const char *p = in;
    unsigned h=0, g=0;
    while (*p) {
        h=(h<<4)+(uint32_t)toupper(*p);
        if((g=h&0xf0000000) != 0) {
            h ^= (g>>24);
            h ^= g;
        }
        p++;
    }
    return h%bucketsz;
}

CMUTIL_STATIC void CMUTIL_MapToUpper(char *in)
{
    while (*in) {
        *in = (char)toupper(*in);
        in++;
    }
}

CMUTIL_STATIC int CMUTIL_MapComparator(const void *a, const void *b)
{
    const CMUTIL_MapItem *ia = (const CMUTIL_MapItem*)a;
    const CMUTIL_MapItem *ib = (const CMUTIL_MapItem*)b;

    return strcmp(ia->key, ib->key);
}

CMUTIL_STATIC CMUTIL_MapItem *CMUTIL_MapPutBase(
        CMUTIL_Map_Internal *imap, const char* key, void* value, void **prev)
{
    uint32_t index = imap->isucase?
                CMUTIL_MapHashUpper(key, imap->bucketsize) :
                CMUTIL_MapHash(key, imap->bucketsize);
    CMUTIL_Array *bucket = imap->buckets[index];
    CMUTIL_MapItem *item = imap->memst->Alloc(sizeof(CMUTIL_MapItem));
    CMUTIL_MapItem *ires = NULL;
    *prev = NULL;

    memset(item, 0x0, sizeof(CMUTIL_MapItem));

    if (!bucket) {
        bucket = CMUTIL_ArrayCreateInternal(
                    imap->memst, 5, CMUTIL_MapComparator, NULL, CMTrue);
        imap->buckets[index] = bucket;
    }

    item->key = imap->memst->Strdup(key);
    if (imap->isucase)
        CMUTIL_MapToUpper(item->key);
    item->value = value;
    ires = CMCall(bucket, Add, item);
    if (ires) {
        *prev = ires->value;
        CMUTIL_MapItem tv = {item->key, NULL};
        CMCall(imap->keyset, Remove, &tv);
        imap->memst->Free(ires->key);
        imap->memst->Free(ires);
    } else {
        imap->size++;
    }
    return item;
}

CMUTIL_STATIC void *CMUTIL_MapPut(
        CMUTIL_Map *map, const char* key, void* value)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    void *res = NULL;
    CMUTIL_MapItem *item = CMUTIL_MapPutBase(imap, key, value, &res);
    if (item)
        CMCall(imap->keyset, Add, item->key);
    return res;
}

CMUTIL_STATIC void CMUTIL_MapPutAll(
        CMUTIL_Map *map, const CMUTIL_Map *src)
{
    CMUTIL_StringArray *keys = CMCall(src, GetKeys);
    uint32_t i;
    for (i=0; i<CMCall(keys, GetSize); i++) {
        CMUTIL_String *skey = CMCall(keys, GetAt, i);
        const char *key = CMCall(skey, GetCString);
        void *val = CMCall(src, Get, key);
        CMCall(map, Put, key, val);
    }
    CMCall(keys, Destroy);
}

CMUTIL_STATIC void *CMUTIL_MapGet(const CMUTIL_Map *map, const char* key)
{
    const CMUTIL_Map_Internal *imap = (const CMUTIL_Map_Internal*)map;
    uint32_t index = imap->isucase?
                CMUTIL_MapHashUpper(key, imap->bucketsize) :
                CMUTIL_MapHash(key, imap->bucketsize);
    CMUTIL_Array *bucket = imap->buckets[index];
    void *res = NULL;

    if (bucket) {
        CMUTIL_MapCItem item = {key, NULL};
        uint32_t idx = 0;
        CMUTIL_MapItem *ires = NULL;
        if (imap->isucase) {
            char *skey = imap->memst->Strdup(key);
            CMUTIL_MapToUpper(skey);
            item.key = skey;
            ires = CMCall(bucket, Find, &item, &idx);
            imap->memst->Free(skey);
        } else {
            ires = CMCall(bucket, Find, &item, &idx);
        }

        if (ires)
            res = ires->value;
    }

    return res;
}

CMUTIL_STATIC void *CMUTIL_MapRemove(CMUTIL_Map *map, const char* key)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    uint32_t index = imap->isucase?
                CMUTIL_MapHashUpper(key, imap->bucketsize) :
                CMUTIL_MapHash(key, imap->bucketsize);
    CMUTIL_Array *bucket = imap->buckets[index];
    void *res = NULL;

    if (bucket) {
        CMUTIL_MapItem *ires = NULL;
        CMUTIL_MapCItem item = {key, NULL};
        if (imap->isucase) {
            char *skey = imap->memst->Strdup(key);
            CMUTIL_MapToUpper(skey);
            item.key = skey;
            ires = (CMUTIL_MapItem*)CMCall(bucket, Remove, &item);
            imap->memst->Free(skey);
        } else {
            ires = (CMUTIL_MapItem*)CMCall(bucket, Remove, &item);
        }
        if (ires) {
            CMCall(imap->keyset, Remove, key);
            res = ires->value;
            imap->memst->Free(ires->key);
            imap->memst->Free(ires);
        }

        if (res)
            imap->size--;
    }

    return res;
}

CMUTIL_STATIC CMUTIL_StringArray *CMUTIL_MapGetKeys(const CMUTIL_Map *map)
{
    const CMUTIL_Map_Internal *imap = (const CMUTIL_Map_Internal*)map;
    CMUTIL_StringArray *res = CMUTIL_StringArrayCreateInternal(
            imap->memst, CMCall(imap->keyset, GetSize));
    uint32_t i;
    for (i=0; i<CMCall(imap->keyset, GetSize); i++) {
        const char *key = (const char*)CMCall(imap->keyset, GetAt, i);
        CMUTIL_String *skey = CMUTIL_StringCreateInternal(
                    imap->memst, 20, key);
        CMCall(res, Add, skey);
    }

    return res;
}

CMUTIL_STATIC size_t CMUTIL_MapGetSize(const CMUTIL_Map *map)
{
    const CMUTIL_Map_Internal *imap = (const CMUTIL_Map_Internal*)map;
    return imap->size;
}

typedef struct CMUTIL_MapIter_st {
    CMUTIL_Iterator             base;
    const CMUTIL_Map_Internal	*imap;
    CMUTIL_StringArray          *keys;
    CMUTIL_Iterator             *keyiter;
} CMUTIL_MapIter_st;

CMUTIL_STATIC CMBool CMUTIL_MapIterHasNext(const CMUTIL_Iterator *iter)
{
    const CMUTIL_MapIter_st *iiter = (const CMUTIL_MapIter_st*)iter;
    return CMCall(iiter->keyiter, HasNext);
}

CMUTIL_STATIC void *CMUTIL_MapIterNext(CMUTIL_Iterator *iter)
{
    CMUTIL_MapIter_st *iiter = (CMUTIL_MapIter_st*)iter;
    CMUTIL_String *key = (CMUTIL_String*)CMCall(iiter->keyiter, Next);
    if (key) {
        const char *skey = CMCall(key, GetCString);
        return CMCall((const CMUTIL_Map*)iiter->imap, Get, skey);
    }
    return NULL;
}

CMUTIL_STATIC void CMUTIL_MapIterDestroy(CMUTIL_Iterator *iter)
{
    CMUTIL_MapIter_st *iiter = (CMUTIL_MapIter_st*)iter;
    CMCall(iiter->keyiter, Destroy);
    CMCall(iiter->keys, Destroy);
    iiter->imap->memst->Free(iiter);
}

static CMUTIL_Iterator g_cmutil_map_iterator = {
    CMUTIL_MapIterHasNext,
    CMUTIL_MapIterNext,
    CMUTIL_MapIterDestroy
};

CMUTIL_STATIC CMUTIL_Iterator *CMUTIL_MapIterator(const CMUTIL_Map *map)
{
    const CMUTIL_Map_Internal *imap = (const CMUTIL_Map_Internal*)map;
    CMUTIL_MapIter_st *res = imap->memst->Alloc(sizeof(CMUTIL_MapIter_st));
    memset(res, 0x0, sizeof(CMUTIL_MapIter_st));
    memcpy(res, &g_cmutil_map_iterator, sizeof(CMUTIL_Iterator));
    res->imap = imap;
    res->keys = CMCall(map, GetKeys);
    res->keyiter = CMCall(res->keys, Iterator);

    return (CMUTIL_Iterator*)res;
}

CMUTIL_STATIC void CMUTIL_MapClearBase(CMUTIL_Map *map, CMBool freedata)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    uint32_t i, j;

    for (i=0; i<imap->bucketsize; i++) {
        CMUTIL_Array *bucket = imap->buckets[i];
        if (bucket) {
            for (j=0; j<CMCall(bucket, GetSize); j++) {
                CMUTIL_MapItem *item =
                        (CMUTIL_MapItem*)CMCall(bucket, GetAt, j);
                imap->memst->Free(item->key);
                if (imap->freecb && freedata)
                    imap->freecb(item->value);
                imap->memst->Free(item);
            }
            bucket->Clear(bucket);
        }
    }

    CMCall(imap->keyset, Clear);
    imap->size = 0;
}

CMUTIL_STATIC void CMUTIL_MapClear(CMUTIL_Map *map)
{
    CMUTIL_MapClearBase(map, CMTrue);
}

CMUTIL_STATIC void CMUTIL_MapClearLink(CMUTIL_Map *map)
{
    CMUTIL_MapClearBase(map, CMFalse);
}

CMUTIL_STATIC void CMUTIL_MapDestroy(CMUTIL_Map *map)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    uint32_t i, j;

    for (i=0; i<imap->bucketsize; i++) {
        CMUTIL_Array *bucket = imap->buckets[i];
        if (bucket) {
            for (j=0; j<CMCall(bucket, GetSize); j++) {
                CMUTIL_MapItem *item =
                        (CMUTIL_MapItem*)CMCall(bucket, GetAt, j);
                imap->memst->Free(item->key);
                if (imap->freecb)
                    imap->freecb(item->value);
                imap->memst->Free(item);
            }
            CMCall(bucket, Destroy);
        }
    }
    CMCall(imap->keyset, Destroy);
    imap->memst->Free(imap->buckets);
    imap->memst->Free(imap);
}

CMUTIL_STATIC void CMUTIL_MapPrintTo(const CMUTIL_Map *map, CMUTIL_String *out)
{
    const CMUTIL_Map_Internal *imap = (const CMUTIL_Map_Internal*)map;
    uint32_t i, j;

    CMCall(out, AddChar, '{');
    for (i=0; i<imap->bucketsize; i++) {
        CMUTIL_Array *bucket = imap->buckets[i];
        if (bucket) {
            for (j=0; j<CMCall(bucket, GetSize); j++) {
                CMUTIL_MapItem *item =
                        (CMUTIL_MapItem*)CMCall(bucket, GetAt, j);
                if (CMCall(out, GetSize) > 1)
                    CMCall(out, AddChar, ',');
                CMCall(out, AddString, item->key);
            }
            CMCall(bucket, Destroy);
        }
    }
    CMCall(out, AddChar, '}');
    CMCall(imap->keyset, Destroy);
}

CMUTIL_STATIC void *CMUTIL_MapGetAt(
        const CMUTIL_Map *map, uint32_t index)
{
    const CMUTIL_Map_Internal *imap = (const CMUTIL_Map_Internal*)map;
    if (index < imap->size) {
        const char *key = (const char*)CMCall(imap->keyset, GetAt, index);
        return CMCall(map, Get, key);
    } else {
        CMLogErrorS("index out of bound map size: %u, index: %u",
                    (uint32_t)imap->size, index);
        return NULL;
    }
}

CMUTIL_STATIC void *CMUTIL_MapRemoveAt(
        CMUTIL_Map *map, uint32_t index)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    if (index < imap->size) {
        const char *key = (const char*)CMCall(imap->keyset, GetAt, index);
        return CMCall(map, Remove, key);
    } else {
        CMLogErrorS("index out of bound map size: %u, index: %u",
                    (uint32_t)imap->size, index);
        return NULL;
    }
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
    CMUTIL_MapPrintTo,
    CMUTIL_MapGetAt,
    CMUTIL_MapRemoveAt
};

CMUTIL_Map *CMUTIL_MapCreateInternal(
        CMUTIL_Mem *memst, uint32_t bucketsize,
        CMBool isucase, CMFreeCB freecb)
{
    CMUTIL_Map_Internal *imap = memst->Alloc(sizeof(CMUTIL_Map_Internal));
    memset(imap, 0x0, sizeof(CMUTIL_Map_Internal));

    memcpy(imap, &g_cmutil_map, sizeof(CMUTIL_Map));
    imap->buckets = memst->Alloc(sizeof(CMUTIL_Array)*(uint32_t)bucketsize);
    memset(imap->buckets, 0x0, sizeof(CMUTIL_Array)*(uint32_t)bucketsize);
    imap->bucketsize = bucketsize;
    imap->isucase = isucase;
    imap->keyset = CMUTIL_ArrayCreateInternal(
                memst, bucketsize, (int(*)(const void*,const void*))strcmp,
                NULL, CMTrue);
    imap->memst = memst;
    imap->freecb = freecb;

    return (CMUTIL_Map*)imap;
}

CMUTIL_Map *CMUTIL_MapCreateEx(
        uint32_t bucketsize, CMBool isucase, CMFreeCB freecb)
{
    return CMUTIL_MapCreateInternal(
                CMUTIL_GetMem(), bucketsize, isucase, freecb);
}

