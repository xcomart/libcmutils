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
    void			**data;
    int				capacity;
    int				size;
    CMUTIL_Bool		issorted;
    int				(*comparator)(const void*,const void*);
    void			(*freecb)(void*);
    CMUTIL_Mem	    *memst;
} CMUTIL_Array_Internal;

CMUTIL_STATIC void CMUTIL_ArrayCheckSize(
        CMUTIL_Array_Internal *array, int insize)
{
    int reqsz = (array->size + insize);
    if (array->capacity < reqsz) {
        int newcap = array->size * 2;
        while (newcap < reqsz) newcap *= 2;
        array->data = array->memst->Realloc(
                    array->data, newcap * sizeof(void*));
        array->capacity = newcap;
    }
}

CMUTIL_STATIC int CMUTIL_ArraySearchPrivate(CMUTIL_Array_Internal *arr,
        const void *obj, int(*cb_cmp)(const void*,const void*), int* pidx)
{
    int stpos, edpos, found = 0, idx = 0;

    /* find appropriate position using binary search */
    stpos = 0;
    edpos = arr->size;
    if (edpos > 0) {
        while (found == 0) {
            int mid = (edpos+stpos) / 2;
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

    *pidx = idx;
    return found;
}

CMUTIL_STATIC void* CMUTIL_ArrayInsertAtPrivate(
        CMUTIL_Array *array, void *item, int index)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;

    CMUTIL_ArrayCheckSize(iarray, 1);
    if (index < iarray->size)
        memmove(iarray->data+index+1, iarray->data+index,
                (iarray->size - index) * sizeof(void*));
    iarray->data[index] = item;
    iarray->size++;

    return item;
}

CMUTIL_STATIC void* CMUTIL_ArrayInsertAt(
        CMUTIL_Array *array, void *item, int index)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;

    if (!iarray->issorted)
        return CMUTIL_ArrayInsertAtPrivate(array, item, index);
    CMLogError("InsertAt is not applicable to sorted array.");
    return NULL;
}

CMUTIL_STATIC void* CMUTIL_ArraySetAtPrivate(
        CMUTIL_Array *array, void *item, int index)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    void *res = NULL;
    if (index < iarray->size && index > -1) {
        res = iarray->data[index];
        iarray->data[index] = item;
    } else {
        CMLogError("Index out of range: %d (array size is %d).",
                   index, iarray->size);
    }

    return res;
}

CMUTIL_STATIC void* CMUTIL_ArraySetAt(
        CMUTIL_Array *array, void *item, int index)
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
        int index;
        int found = CMUTIL_ArraySearchPrivate(iarray, item,
                iarray->comparator, &index);
        if (found == 1) {
            // found same data and duplication
            res = CMUTIL_CALL(array, GetAt, index);
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

CMUTIL_STATIC void *CMUTIL_ArrayRemoveAt(CMUTIL_Array *array, int index)
{
    void *res = NULL;
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    if (index > -1 && index < iarray->size) {
        res = iarray->data[index];
        memmove(iarray->data+index, iarray->data+index+1,
                (iarray->size - index - 1) * sizeof(void*));
        iarray->size--;
    } else {
        CMLogError("Index out of range: %d (array size is %d).",
                   index, iarray->size);
    }
    return res;
}

CMUTIL_STATIC void *CMUTIL_ArrayGetAt(CMUTIL_Array *array, int index)
{
    void *res = NULL;
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    if (index > -1 && index < iarray->size)
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
        int index;
        int found = CMUTIL_ArraySearchPrivate(iarray, compval,
                iarray->comparator, &index);
        if (found == 1)
            // found data and remove it
            res = CMUTIL_CALL(array, RemoveAt, index);
    } else {
        CMLogError("Remove method only applicable to sorted array.");
    }
    return res;
}

CMUTIL_STATIC void *CMUTIL_ArrayFind(
        CMUTIL_Array *array, const void *compval, int *index)
{
    void *res = NULL;
    int idx = 0;
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    if (iarray->issorted) {
        int found = CMUTIL_ArraySearchPrivate(iarray, compval,
                iarray->comparator, &idx);
        if (found == 1)
            // found data
            res = iarray->data[idx];
        if (index)
            *index = idx;
    } else {
        int i;
        for (i=0; i<iarray->size; i++) {
            CMUTIL_Bool found = CMUTIL_False;
            if (iarray->comparator) {
                if (iarray->comparator(compval, iarray->data[i]) == 0)
                    found = CMUTIL_True;
            } else {
                if (iarray->data[i] == compval)
                    found = CMUTIL_True;
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

CMUTIL_STATIC int CMUTIL_ArrayGetSize(CMUTIL_Array *array)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    return iarray->size;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_ArrayPush(CMUTIL_Array *array, void *item)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
    if (iarray->issorted) {
        CMLogError("Push method cannot applicable to sorted array.");
        return CMUTIL_False;
    }
    CMUTIL_CALL(array, Add, item);
    return CMUTIL_True;
}

CMUTIL_STATIC void *CMUTIL_ArrayPop(CMUTIL_Array *array)
{
    int size = CMUTIL_CALL(array, GetSize);
    if (size > 0)
        return CMUTIL_CALL(array, RemoveAt, size-1);
    return NULL;
}

CMUTIL_STATIC void *CMUTIL_ArrayTop(CMUTIL_Array *array)
{
    int size = CMUTIL_CALL(array, GetSize);
    if (size > 0)
        return CMUTIL_CALL(array, GetAt, size-1);
    return NULL;
}

CMUTIL_STATIC void *CMUTIL_ArrayBottom(CMUTIL_Array *array)
{
    int size = CMUTIL_CALL(array, GetSize);
    if (size > 0)
        return CMUTIL_CALL(array, GetAt, 0);
    return NULL;
}

typedef struct CMUTIL_ArrayIterator_st {
    CMUTIL_Iterator			base;
    CMUTIL_Array_Internal	*iarray;
    int						index;
} CMUTIL_ArrayIterator_st;

CMUTIL_STATIC CMUTIL_Bool CMUTIL_ArrayIterHasNext(CMUTIL_Iterator *iter)
{
    CMUTIL_ArrayIterator_st *iiter = (CMUTIL_ArrayIterator_st*)iter;
    return CMUTIL_CALL((CMUTIL_Array*)iiter->iarray, GetSize) > iiter->index?
                CMUTIL_True:CMUTIL_False;
}

CMUTIL_STATIC void *CMUTIL_ArrayIterNext(CMUTIL_Iterator *iter)
{
    CMUTIL_ArrayIterator_st *iiter = (CMUTIL_ArrayIterator_st*)iter;
    if (CMUTIL_CALL((CMUTIL_Array*)iiter->iarray, GetSize) > iiter->index)
        return CMUTIL_CALL((CMUTIL_Array*)iiter->iarray, GetAt, iiter->index++);
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

CMUTIL_STATIC CMUTIL_Iterator *CMUTIL_ArrayIterator(CMUTIL_Array *array)
{
    CMUTIL_Array_Internal *iarray = (CMUTIL_Array_Internal*)array;
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
                int i;
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
        int initcapacity,
        int(*comparator)(const void*,const void*),
        void(*freecb)(void*))
{
    CMUTIL_Array_Internal *iarray = mem->Alloc(sizeof(CMUTIL_Array_Internal));
    memset(iarray, 0x0, sizeof(CMUTIL_Array_Internal));

    memcpy(iarray, &g_cmutil_array, sizeof(CMUTIL_Array));
    iarray->data = mem->Alloc(sizeof(void*) * initcapacity);
    iarray->capacity = initcapacity;
    iarray->comparator = comparator;
    iarray->issorted = comparator? CMUTIL_True:CMUTIL_False;
    iarray->freecb = freecb;
    iarray->memst = mem;

    return (CMUTIL_Array*)iarray;
}

CMUTIL_Array *CMUTIL_ArrayCreateEx(
        int initcapacity,
        int(*comparator)(const void*,const void*),
        void(*freecb)(void*))
{
    return CMUTIL_ArrayCreateInternal(
                CMUTIL_GetMem(), initcapacity, comparator, freecb);
}


