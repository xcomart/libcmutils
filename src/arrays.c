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

CMUTIL_LogDefine("cmutil.array")

//*****************************************************************************
// CMUTIL_Array implementation
//*****************************************************************************

typedef struct CMUTIL_Array_Internal {
    CMUTIL_Array	base;
    CMBool		issorted;
    int             dummy_padder;
    void			**data;
    size_t			capacity;
    size_t			size;
    int				(*comparator)(const void*,const void*);
    void			(*freecb)(void*);
    CMUTIL_Mem	    *memst;
} CMUTIL_Array_Internal;

CMUTIL_STATIC void CMUTIL_ArrayCheckSize(
        CMUTIL_Array_Internal *array, size_t insize)
{
    size_t reqsz = (array->size + insize);
    if (array->capacity < reqsz) {
        size_t newcap = array->size * 2;
        while (newcap < reqsz) newcap *= 2;
        array->data = array->memst->Realloc(
                    array->data, (uint32_t)newcap * sizeof(void*));
        array->capacity = newcap;
    }
}

CMUTIL_STATIC int CMUTIL_ArraySearchPrivate(
        const CMUTIL_Array_Internal *arr,
        const void *obj, int(*cb_cmp)(const void*,const void*), uint32_t* pidx)
{
    int found = 0;
    size_t stpos, edpos, idx = 0;

    /* find appropriate position using binary search */
    stpos = 0;
    edpos = arr->size;
    if (edpos > 0) {
        while (found == 0) {
            size_t mid = (edpos+stpos) / 2;
            int cmp = cb_cmp(obj, *(arr->data+mid));
            if (cmp == 0) {
                found = 1;
                idx = mid;
            } else if (cmp < 0) {
                if (mid == stpos) {
                    found = 2;
                    idx = mid;
                } else {
                    edpos = mid;
                }
            } else if (cmp > 0) {
                if (mid == (edpos-1)) {
                    found = 2;
                    idx = edpos;
                } else {
                    stpos = mid;
                }
            }
        }
    } else {
        found = 2;
    }

    *pidx = (uint32_t)idx;
    return found;
}

CMUTIL_STATIC void* CMUTIL_ArrayInsertAtPrivate(
        CMUTIL_Array *array, void *item, uint32_t index)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;

    CMUTIL_ArrayCheckSize(iarray, 1);
    if (index < iarray->size)
        memmove(iarray->data+index+1, iarray->data+index,
                (uint32_t)(iarray->size - index) * sizeof(void*));
    iarray->data[index] = item;
    iarray->size++;

    return item;
}

CMUTIL_STATIC void* CMUTIL_ArrayInsertAt(
        CMUTIL_Array *array, void *item, uint32_t index)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;

    if (!iarray->issorted)
        return CMUTIL_ArrayInsertAtPrivate(array, item, index);
    CMLogError("InsertAt is not applicable to sorted array.");
    return NULL;
}

CMUTIL_STATIC void* CMUTIL_ArraySetAtPrivate(
        CMUTIL_Array *array, void *item, uint32_t index)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    void *res = NULL;
    if (index < iarray->size) {
        res = iarray->data[index];
        iarray->data[index] = item;
    } else {
        CMLogError("Index out of range: %d (array size is %d).",
                   index, iarray->size);
    }

    return res;
}

CMUTIL_STATIC void* CMUTIL_ArraySetAt(
        CMUTIL_Array *array, void *item, uint32_t index)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    if (!iarray->issorted)
        return CMUTIL_ArraySetAtPrivate(array, item, index);
    CMLogError("SetAt is not applicable to sorted array.");
    return NULL;
}

CMUTIL_STATIC void *CMUTIL_ArrayAdd(CMUTIL_Array *array, void *item)
{
    void *res = NULL;
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    if (iarray->issorted) {
        uint32_t index;
        int found = CMUTIL_ArraySearchPrivate(iarray, item,
                iarray->comparator, &index);
        if (found == 1) {
            // found same data and duplication
            res = CMCall(array, GetAt, index);
            iarray->data[index] = item;
        } else if (found != 0) {
            CMUTIL_ArrayInsertAtPrivate(array, item, index);
        }
    } else {
        CMUTIL_ArrayCheckSize(iarray, 1);
        iarray->data[iarray->size++] = item;
    }
    return res;
}

CMUTIL_STATIC void *CMUTIL_ArrayRemoveAt(CMUTIL_Array *array, uint32_t index)
{
    void *res = NULL;
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    if (index < iarray->size) {
        res = iarray->data[index];
        memmove(iarray->data+index, iarray->data+index+1,
                (uint32_t)(iarray->size - index - 1) * sizeof(void*));
        iarray->size--;
    } else {
        CMLogError("Index out of range: %d (array size is %d).",
                   index, iarray->size);
    }
    return res;
}

CMUTIL_STATIC void *CMUTIL_ArrayGetAt(const CMUTIL_Array *array, uint32_t index)
{
    void *res = NULL;
    const CMUTIL_Array_Internal *iarray = (const CMUTIL_Array_Internal*)array;
    if (index < iarray->size)
        res = iarray->data[index];
    CMLogError("Index out of range: %d (array size is %d).",
               index, iarray->size);
    return res;
}

CMUTIL_STATIC void *CMUTIL_ArrayRemove(
        CMUTIL_Array *array, const void *compval)
{
    void *res = NULL;
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    if (iarray->issorted) {
        uint32_t index;
        int found = CMUTIL_ArraySearchPrivate(iarray, compval,
                iarray->comparator, &index);
        if (found == 1)
            // found data and remove it
            res = CMCall(array, RemoveAt, index);
    } else {
        CMLogError("Remove method only applicable to sorted array.");
    }
    return res;
}

CMUTIL_STATIC void *CMUTIL_ArrayFind(
        const CMUTIL_Array *array, const void *compval, uint32_t *index)
{
    void *res = NULL;
    uint32_t idx = 0;
    const CMUTIL_Array_Internal *iarray = (const CMUTIL_Array_Internal*)array;
    if (iarray->issorted) {
        int found = CMUTIL_ArraySearchPrivate(iarray, compval,
                iarray->comparator, &idx);
        if (found == 1)
            // found data
            res = iarray->data[idx];
        if (index)
            *index = idx;
    } else {
        uint32_t i;
        for (i=0; i<iarray->size; i++) {
            CMBool found = CMFalse;
            if (iarray->comparator) {
                if (iarray->comparator(compval, iarray->data[i]) == 0)
                    found = CMTrue;
            } else {
                if (iarray->data[i] == compval)
                    found = CMTrue;
            }
            if (found) {
                res = iarray->data[i];
                if (index)
                    *index = i;
                break;
            }
        }
    }
    return res;
}

CMUTIL_STATIC size_t CMUTIL_ArrayGetSize(const CMUTIL_Array *array)
{
    const CMUTIL_Array_Internal *iarray = (const CMUTIL_Array_Internal*)array;
    return iarray->size;
}

CMUTIL_STATIC CMBool CMUTIL_ArrayPush(CMUTIL_Array *array, void *item)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    if (iarray->issorted) {
        CMLogError("Push method cannot applicable to sorted array.");
        return CMFalse;
    }
    CMCall(array, Add, item);
    return CMTrue;
}

CMUTIL_STATIC void *CMUTIL_ArrayPop(CMUTIL_Array *array)
{
    size_t size = CMCall(array, GetSize);
    if (size > 0)
        return CMCall(array, RemoveAt, (uint32_t)(size-1));
    return NULL;
}

CMUTIL_STATIC void *CMUTIL_ArrayTop(const CMUTIL_Array *array)
{
    size_t size = CMCall(array, GetSize);
    if (size > 0)
        return CMCall(array, GetAt, (uint32_t)(size-1));
    return NULL;
}

CMUTIL_STATIC void *CMUTIL_ArrayBottom(const CMUTIL_Array *array)
{
    size_t size = CMCall(array, GetSize);
    if (size > 0)
        return CMCall(array, GetAt, 0);
    return NULL;
}

typedef struct CMUTIL_ArrayIterator_st {
    CMUTIL_Iterator             base;
    const CMUTIL_Array_Internal	*iarray;
    uint32_t                        index;
    int                         dummy_padder;
} CMUTIL_ArrayIterator_st;

CMUTIL_STATIC CMBool CMUTIL_ArrayIterHasNext(
        const CMUTIL_Iterator *iter)
{
    const CMUTIL_ArrayIterator_st *iiter =
            (const CMUTIL_ArrayIterator_st*)iter;
    return CMCall((const CMUTIL_Array*)iiter->iarray, GetSize) > iiter->index?
                CMTrue:CMFalse;
}

CMUTIL_STATIC void *CMUTIL_ArrayIterNext(CMUTIL_Iterator *iter)
{
    CMUTIL_ArrayIterator_st *iiter = (CMUTIL_ArrayIterator_st*)iter;
    if (CMCall((const CMUTIL_Array*)iiter->iarray, GetSize) > iiter->index)
        return CMCall((const CMUTIL_Array*)iiter->iarray,
                      GetAt, iiter->index++);
    return NULL;
}

CMUTIL_STATIC void CMUTIL_ArrayIterDestroy(CMUTIL_Iterator *iter)
{
    CMUTIL_ArrayIterator_st *iiter = (CMUTIL_ArrayIterator_st*)iter;
    if (iiter)
        iiter->iarray->memst->Free(iiter);
}

static CMUTIL_Iterator g_cmutil_array_iterator = {
    CMUTIL_ArrayIterHasNext,
    CMUTIL_ArrayIterNext,
    CMUTIL_ArrayIterDestroy
};

CMUTIL_STATIC CMUTIL_Iterator *CMUTIL_ArrayIterator(const CMUTIL_Array *array)
{
    const CMUTIL_Array_Internal *iarray = (const CMUTIL_Array_Internal*)array;
    CMUTIL_ArrayIterator_st *res =
            iarray->memst->Alloc(sizeof(CMUTIL_ArrayIterator_st));
    memset(res, 0x0, sizeof(CMUTIL_ArrayIterator_st));
    memcpy(res, &g_cmutil_array_iterator, sizeof(CMUTIL_Iterator));
    res->iarray = iarray;
    return (CMUTIL_Iterator*)res;
}

CMUTIL_STATIC void CMUTIL_ArrayClear(CMUTIL_Array *array)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    iarray->size = 0;
}

CMUTIL_STATIC void CMUTIL_ArrayDestroy(CMUTIL_Array *array)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    if (iarray) {
        if (iarray->data) {
            if (iarray->freecb) {
                uint32_t i;
                for (i=0; i<iarray->size; i++)
                    iarray->freecb(iarray->data[i]);
            }
            iarray->memst->Free(iarray->data);
        }
        iarray->memst->Free(iarray);
    }
}

static CMUTIL_Array g_cmutil_array = {
        CMUTIL_ArrayAdd,
        CMUTIL_ArrayRemove,
        CMUTIL_ArrayInsertAt,
        CMUTIL_ArrayRemoveAt,
        CMUTIL_ArraySetAt,
        CMUTIL_ArrayGetAt,
        CMUTIL_ArrayFind,
        CMUTIL_ArrayGetSize,
        CMUTIL_ArrayPush,
        CMUTIL_ArrayPop,
        CMUTIL_ArrayTop,
        CMUTIL_ArrayBottom,
        CMUTIL_ArrayIterator,
        CMUTIL_ArrayClear,
        CMUTIL_ArrayDestroy
};

CMUTIL_Array *CMUTIL_ArrayCreateInternal(
        CMUTIL_Mem *mem,
        size_t initcapacity,
        int(*comparator)(const void*,const void*),
        void(*freecb)(void*),
        CMBool sorted)
{
    CMUTIL_Array_Internal *iarray = mem->Alloc(sizeof(CMUTIL_Array_Internal));
    memset(iarray, 0x0, sizeof(CMUTIL_Array_Internal));

    memcpy(iarray, &g_cmutil_array, sizeof(CMUTIL_Array));
    iarray->data = mem->Alloc(sizeof(void*) * (uint32_t)initcapacity);
    iarray->capacity = initcapacity;
    iarray->comparator = comparator;
    iarray->issorted = sorted;
    iarray->freecb = freecb;
    iarray->memst = mem;

    return (CMUTIL_Array*)iarray;
}

CMUTIL_Array *CMUTIL_ArrayCreateEx(
        size_t initcapacity,
        int(*comparator)(const void*,const void*),
        void(*freecb)(void*))
{
    return CMUTIL_ArrayCreateInternal(
                CMUTIL_GetMem(), initcapacity,
                comparator, freecb, comparator? CMTrue:CMFalse);
}


