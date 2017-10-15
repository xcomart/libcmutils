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

//*****************************************************************************
// CMUTIL_List implementation
//*****************************************************************************

typedef struct CMUTIL_ListItem {
    struct CMUTIL_ListItem	*prev;
    struct CMUTIL_ListItem	*next;
    void					*data;
} CMUTIL_ListItem;

typedef struct CMUTIL_List_Internal {
    CMUTIL_List			base;
    CMUTIL_ListItem		*head;
    CMUTIL_ListItem		*tail;
    size_t				size;
    void				(*freecb)(void*);
    CMUTIL_Mem          *memst;
} CMUTIL_List_Internal;

CMUTIL_STATIC void CMUTIL_ListAddFront(CMUTIL_List *list, void *data)
{
    CMUTIL_List_Internal *ilist = (CMUTIL_List_Internal*)list;
    CMUTIL_ListItem *item = ilist->memst->Alloc(sizeof(CMUTIL_ListItem));
    memset(item, 0x0, sizeof(CMUTIL_ListItem));
    item->data = data;
    item->prev = NULL;
    item->next = ilist->head;
    if (ilist->head)
        ilist->head->prev = item;
    ilist->head = item;
    if (!ilist->tail)
        ilist->tail = item;
    ilist->size++;
}

CMUTIL_STATIC void CMUTIL_ListAddTail(CMUTIL_List *list, void *data)
{
    CMUTIL_List_Internal *ilist = (CMUTIL_List_Internal*)list;
    CMUTIL_ListItem *item = ilist->memst->Alloc(sizeof(CMUTIL_ListItem));
    memset(item, 0x0, sizeof(CMUTIL_ListItem));
    item->data = data;
    item->prev = ilist->tail;
    item->next = NULL;
    if (ilist->tail)
        ilist->tail->next = item;
    ilist->tail = item;
    if (!ilist->head)
        ilist->head = item;
    ilist->size++;
}

CMUTIL_STATIC void *CMUTIL_ListGetFront(const CMUTIL_List *list)
{
    const CMUTIL_List_Internal *ilist =
            (const CMUTIL_List_Internal*)list;
    if (list && ilist->head)
        return ilist->head->data;
    return NULL;
}

CMUTIL_STATIC void *CMUTIL_ListGetTail(const CMUTIL_List *list)
{
    const CMUTIL_List_Internal *ilist =
            (const CMUTIL_List_Internal*)list;
    if (list && ilist->tail)
        return ilist->tail->data;
    return NULL;
}

CMUTIL_STATIC void *CMUTIL_ListRemoveFront(CMUTIL_List *list)
{
    CMUTIL_List_Internal *ilist = (CMUTIL_List_Internal*)list;
    CMUTIL_ListItem *item = ilist->head;
    void *res = NULL;
    if (item) {
        res = item->data;
        ilist->head = item->next;
        if (ilist->head)
            ilist->head->prev = NULL;
        ilist->memst->Free(item);
        ilist->size--;
        if (ilist->size == 0)
            ilist->tail = NULL;
    }
    return res;
}

CMUTIL_STATIC void *CMUTIL_ListRemoveTail(CMUTIL_List *list)
{
    CMUTIL_List_Internal *ilist = (CMUTIL_List_Internal*)list;
    CMUTIL_ListItem *item = ilist->tail;
    void *res = NULL;
    if (item) {
        res = item->data;
        ilist->tail = item->prev;
        if (ilist->tail)
            ilist->tail->next = NULL;
        ilist->memst->Free(item);
        ilist->size--;
        if (ilist->size == 0)
            ilist->head = NULL;
    }
    return res;
}

CMUTIL_STATIC void *CMUTIL_ListRemove(CMUTIL_List *list, void *data)
{
    CMUTIL_List_Internal *ilist = (CMUTIL_List_Internal*)list;
    CMUTIL_ListItem *item = ilist->head;
    void *res = NULL;
    while (item) {
        if (item->data == data) {
            if (item == ilist->head)
                return CMCall(list, RemoveFront);
            if (item == ilist->tail)
                return CMCall(list, RemoveTail);
            item->prev->next = item->next;
            item->next->prev = item->prev;
            ilist->memst->Free(item);
            return data;
        }
        item = item->next;
    }
    return res;
}

CMUTIL_STATIC size_t CMUTIL_ListGetSize(const CMUTIL_List *list)
{
    const CMUTIL_List_Internal *ilist =
            (const CMUTIL_List_Internal*)list;
    return ilist->size;
}

typedef struct CMUTIL_ListIter_st {
    CMUTIL_Iterator             base;
    CMUTIL_ListItem             *curr;
    const CMUTIL_List_Internal	*list;
} CMUTIL_ListIter_st;

CMUTIL_STATIC CMUTIL_Bool CMUTIL_ListIterHasNext(const CMUTIL_Iterator *iter)
{
    const CMUTIL_ListIter_st *iiter = (const CMUTIL_ListIter_st*)iter;
    return iiter->curr? CMTrue:CMFalse;
}

CMUTIL_STATIC void *CMUTIL_ListIterNext(CMUTIL_Iterator *iter)
{
    CMUTIL_ListIter_st *iiter = (CMUTIL_ListIter_st*)iter;
    void *res = iiter->curr? iiter->curr->data:NULL;
    if (res)
        iiter->curr = iiter->curr->next;
    return res;
}

CMUTIL_STATIC void CMUTIL_ListIterDestroy(CMUTIL_Iterator *iter)
{
    CMUTIL_ListIter_st *iiter = (CMUTIL_ListIter_st*)iter;
    iiter->list->memst->Free(iiter);
}

static CMUTIL_Iterator g_cmutil_list_iterator = {
    CMUTIL_ListIterHasNext,
    CMUTIL_ListIterNext,
    CMUTIL_ListIterDestroy
};

CMUTIL_STATIC CMUTIL_Iterator *CMUTIL_ListIterator(const CMUTIL_List *list)
{
    const CMUTIL_List_Internal *ilist = (const CMUTIL_List_Internal*)list;
    CMUTIL_ListIter_st *res = ilist->memst->Alloc(sizeof(CMUTIL_ListIter_st));
    memset(res, 0x0, sizeof(CMUTIL_ListIter_st));
    memcpy(res, &g_cmutil_list_iterator, sizeof(CMUTIL_Iterator));
    res->curr = ilist->head;
    res->list = ilist;

    return (CMUTIL_Iterator*)res;
}

CMUTIL_STATIC void CMUTIL_ListDestroy(CMUTIL_List *list)
{
    CMUTIL_List_Internal *ilist = (CMUTIL_List_Internal*)list;
    CMUTIL_ListItem *curr, *next;
    curr = ilist->head;
    while (curr) {
        next = curr->next;
        if (ilist->freecb)
            ilist->freecb(curr->data);
        ilist->memst->Free(curr);
        curr = next;
    }
    ilist->memst->Free(ilist);
}

static CMUTIL_List g_cmutil_list = {
    CMUTIL_ListAddFront,
    CMUTIL_ListAddTail,
    CMUTIL_ListGetFront,
    CMUTIL_ListGetTail,
    CMUTIL_ListRemoveFront,
    CMUTIL_ListRemoveTail,
    CMUTIL_ListRemove,
    CMUTIL_ListGetSize,
    CMUTIL_ListIterator,
    CMUTIL_ListDestroy
};

CMUTIL_List *CMUTIL_ListCreateInternal(
        CMUTIL_Mem *memst, void(*freecb)(void*))
{
    CMUTIL_List_Internal *ilist = memst->Alloc(sizeof(CMUTIL_List_Internal));
    memset(ilist, 0x0, sizeof(CMUTIL_List_Internal));
    memcpy(ilist, &g_cmutil_list, sizeof(CMUTIL_List));
    ilist->memst = memst;
    ilist->freecb = freecb;
    return (CMUTIL_List*)ilist;
}

CMUTIL_List *CMUTIL_ListCreateEx(void(*freecb)(void*))
{
    return CMUTIL_ListCreateInternal(CMUTIL_GetMem(), freecb);
}

