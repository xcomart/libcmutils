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
    char        *key;
    void        *value;
    uint32_t    hash;
    int         order;
} CMUTIL_MapItem;

typedef struct CMUTIL_Map_Internal {
    CMUTIL_Map      base;
    CMUTIL_Array    **buckets;
    size_t          size;
    CMUTIL_Array    *keyset;
    void            (*freecb)(void*);
    CMUTIL_Mem      *memst;
    CMBool          isucase;
    uint32_t        bucketsize;
    uint32_t        usecount;
    int             order_seq;
} CMUTIL_Map_Internal;

CMUTIL_STATIC uint32_t CMUTIL_MapHash(const char *in)
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
    return h;
}

CMUTIL_STATIC uint32_t CMUTIL_MapHashUpper(const char *in)
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
    return h;
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

CMUTIL_STATIC void CMUTIL_MapRebuild(CMUTIL_Map_Internal *imap)
{
    uint32_t i;
    uint32_t newbucketsize = imap->bucketsize * 2;
    CMUTIL_Array **newbuckets =
        imap->memst->Alloc(sizeof(CMUTIL_Array*) * newbucketsize);
    memset(newbuckets, 0x0, sizeof(CMUTIL_Array*) * newbucketsize);

    for (i=0; i<CMCall(imap->keyset, GetSize); i++) {
        CMUTIL_MapItem *item = (CMUTIL_MapItem*)CMCall(imap->keyset, GetAt, i);
        uint32_t index = item->hash % newbucketsize;
        CMUTIL_Array *bucket = newbuckets[index];
        if (!bucket) {
            bucket = CMUTIL_ArrayCreateInternal(
                        imap->memst, 5, CMUTIL_MapComparator,
                        NULL, CMTrue);
            newbuckets[index] = bucket;
        }
        // cannot be duplicated when map rebuild.
        CMCall(bucket, Add, item, NULL);
    }

    for (i=0; i<imap->bucketsize; i++) {
        CMUTIL_Array *bucket = imap->buckets[i];
        if (bucket) {
            CMCall(bucket, Destroy);
        }
    }
    imap->memst->Free(imap->buckets);
    imap->buckets = newbuckets;
    imap->bucketsize = newbucketsize;
}

CMUTIL_STATIC CMUTIL_MapItem *CMUTIL_MapPutBase(
        CMUTIL_Map_Internal *imap, const char* key, void* value, void **prev)
{
    uint32_t hash = imap->isucase?
                CMUTIL_MapHashUpper(key) :
                CMUTIL_MapHash(key);
    uint32_t index = hash % imap->bucketsize;
    CMUTIL_Array *bucket = imap->buckets[index];
    CMUTIL_MapItem *item = imap->memst->Alloc(sizeof(CMUTIL_MapItem));
    CMUTIL_MapItem *ires = NULL;
    if (prev)
        *prev = NULL;

    memset(item, 0x0, sizeof(CMUTIL_MapItem));

    if (!bucket) {
        imap->usecount++;
        if ((double)imap->usecount / (double)imap->bucketsize > 0.7) {
            // Rebuild the map to reduce the collision rate
            CMLogTrace("Map bucket size is %u, used count is %u, rebuild",
                imap->bucketsize, imap->usecount);
            CMUTIL_MapRebuild(imap);
            index = hash % imap->bucketsize;
        }
        bucket = CMUTIL_ArrayCreateInternal(
                    imap->memst, 5, CMUTIL_MapComparator,
                    NULL, CMTrue);
        imap->buckets[index] = bucket;
    }

    item->hash = hash;
    item->key = imap->memst->Strdup(key);
    if (imap->isucase)
        CMUTIL_MapToUpper(item->key);
    item->value = value;
    item->order = imap->order_seq++;
    if (!CMCall(bucket, Add, item, (void**)&ires)) {
        CMLogError("CMUTIL_Array Add failed");
        imap->memst->Free(item->key);
        imap->memst->Free(item);
        return NULL;
    }
    if (ires) {
        if (prev)
            *prev = ires->value;
        CMCall(imap->keyset, Remove, ires);
        imap->memst->Free(ires->key);
        imap->memst->Free(ires);
   } else {
        imap->size++;
    }
    return item;
}

CMUTIL_STATIC CMBool CMUTIL_MapPut(
        CMUTIL_Map *map, const char* key, void* value, void** prev)
{
    CMUTIL_Map_Internal *imap = (CMUTIL_Map_Internal*)map;
    CMUTIL_MapItem *item = CMUTIL_MapPutBase(imap, key, value, prev);
    if (!CMCall(imap->keyset, Add, item, NULL)) {
        CMLogError("CMUTIL_Array Add failed");
        CMCall(map, Remove, key);
        return CMFalse;
    }
    return CMTrue;
}

CMUTIL_STATIC void CMUTIL_MapPutAll(
        CMUTIL_Map *map, const CMUTIL_Map *src)
{
    const CMUTIL_Map_Internal *imap = (const CMUTIL_Map_Internal*)map;
    CMUTIL_StringArray *keys = CMCall(src, GetKeys);
    uint32_t i;
    for (i=0; i<CMCall(keys, GetSize); i++) {
        const CMUTIL_String *skey = CMCall(keys, GetAt, i);
        const char *key = CMCall(skey, GetCString);
        void *val = CMCall(src, Get, key);
        void *prev = NULL;
        CMCall(map, Put, key, val, &prev);
        if (prev) {
            if (imap->freecb)
                imap->freecb(prev);
            else {
                CMLogErrorS("No free callback provided for previous value");
            }
        }
    }
    CMCall(keys, Destroy);
}

CMUTIL_STATIC void *CMUTIL_MapGet(const CMUTIL_Map *map, const char* key)
{
    const CMUTIL_Map_Internal *imap = (const CMUTIL_Map_Internal*)map;
    uint32_t hash = imap->isucase?
                CMUTIL_MapHashUpper(key) :
                CMUTIL_MapHash(key);
    uint32_t index = hash % imap->bucketsize;
    CMUTIL_Array *bucket = imap->buckets[index];
    void *res = NULL;

    if (bucket) {
        CMUTIL_MapItem item = {(char*)key, NULL};
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
    uint32_t hash = imap->isucase?
                CMUTIL_MapHashUpper(key) :
                CMUTIL_MapHash(key);
    uint32_t index = hash % imap->bucketsize;
    CMUTIL_Array *bucket = imap->buckets[index];
    void *res = NULL;

    if (bucket) {
        CMUTIL_MapItem *ires = NULL;
        CMUTIL_MapItem item = {(char*)key, NULL, 0};
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
        CMUTIL_MapItem *item = (CMUTIL_MapItem*)CMCall(imap->keyset, GetAt, i);
        CMCall(res, AddCString, item->key);
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
    const CMUTIL_Map_Internal   *imap;
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
    uint32_t i;

    CMCall(out, AddChar, '{');
    for (i=0; i<CMCall(imap->keyset, GetSize); i++) {
        CMUTIL_MapItem *item =
                (CMUTIL_MapItem*)CMCall(imap->keyset, GetAt, i);
        if (CMCall(out, GetSize) > 1)
            CMCall(out, AddChar, ',');
        CMCall(out, AddString, item->key);
    }
    CMCall(out, AddChar, '}');
}

CMUTIL_STATIC void *CMUTIL_MapGetAt(
        const CMUTIL_Map *map, uint32_t index)
{
    const CMUTIL_Map_Internal *imap = (const CMUTIL_Map_Internal*)map;
    if (index < imap->size) {
        CMUTIL_MapItem *item = (CMUTIL_MapItem*)CMCall(imap->keyset, GetAt, index);
        return CMCall(map, Get, item->key);
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
        CMUTIL_MapItem *item = (CMUTIL_MapItem*)CMCall(imap->keyset, GetAt, index);
        return CMCall(map, Remove, item->key);
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

CMUTIL_STATIC int CMUTIL_MapCompareOrder(const void *a, const void *b)
{
    return ((CMUTIL_MapItem*)a)->order - ((CMUTIL_MapItem*)b)->order;
}

CMUTIL_Map *CMUTIL_MapCreateInternal(
        CMUTIL_Mem *memst, uint32_t bucketsize,
        CMBool isucase, CMFreeCB freecb)
{
    CMUTIL_Map_Internal *imap = memst->Alloc(sizeof(CMUTIL_Map_Internal));
    memset(imap, 0x0, sizeof(CMUTIL_Map_Internal));

    memcpy(imap, &g_cmutil_map, sizeof(CMUTIL_Map));
    imap->buckets = memst->Alloc(sizeof(CMUTIL_Array)*bucketsize);
    memset(imap->buckets, 0x0, sizeof(CMUTIL_Array)*bucketsize);
    imap->bucketsize = bucketsize;
    imap->isucase = isucase;
    imap->keyset = CMUTIL_ArrayCreateInternal(
                memst, bucketsize, CMUTIL_MapCompareOrder,
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

