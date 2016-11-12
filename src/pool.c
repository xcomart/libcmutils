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

CMUTIL_LogDefine("cmutils.pool")

typedef struct CMUTIL_Pool_Internal {
    CMUTIL_Pool			base;
    CMUTIL_Bool			intimer;
    CMUTIL_Timer		*timer;
    CMUTIL_Semaphore	*semp;
    int					totcnt;
    int					maxcnt;
    long				pingintv;
    CMUTIL_Bool			testonb;
    CMUTIL_List			*avail;
    CMUTIL_Mutex		*avlmtx;
    void				*udata;
    void				*(*createf)(void*);
    void				(*destroyf)(void*,void*);
    CMUTIL_Bool			(*testf)(void*,void*);
    CMUTIL_TimerTask	*pingtester;
    CMUTIL_Mem		*memst;
} CMUTIL_Pool_Internal;

CMUTIL_STATIC void *CMUTIL_PoolCheckOut(
        CMUTIL_Pool *pool, long millisec)
{
    CMUTIL_Pool_Internal *ipool = (CMUTIL_Pool_Internal*)pool;
    if (CMUTIL_CALL(ipool->semp, Acquire, millisec)) {
        void *res = NULL;
        CMUTIL_CALL(ipool->avlmtx, Lock);
        if (CMUTIL_CALL(ipool->avail, GetSize) > 0) {
            res = CMUTIL_CALL(ipool->avail, RemoveFront);
        } else {
            res = ipool->createf(ipool->udata);
            if (res == NULL)
                CMLogError("resource creation failed.");
            else
                ipool->totcnt++;
        }
        CMUTIL_CALL(ipool->avlmtx, Unlock);
        if (res && ipool->testonb) {
            if (!ipool->testf(res, ipool->udata)) {
                ipool->destroyf(res, ipool->udata);
                res = ipool->createf(ipool->udata);
                if (res == NULL) {
                    CMUTIL_CALL(ipool->semp, Release);
                    CMLogError("resource creation failed.");
                }
            }
        }
        return res;
    } else {
        CMLogWarn("resource get timed out.");
        return NULL;
    }
}

CMUTIL_STATIC void CMUTIL_PoolRelease(
        CMUTIL_Pool *pool, void *resource)
{
    CMUTIL_Pool_Internal *ipool = (CMUTIL_Pool_Internal*)pool;
    CMUTIL_CALL(ipool->avlmtx, Lock);
    CMUTIL_CALL(ipool->avail, AddTail, resource);
    CMUTIL_CALL(ipool->avlmtx, Unlock);
    CMUTIL_CALL(ipool->semp, Release);
}

CMUTIL_STATIC void CMUTIL_PoolAddResource(
        CMUTIL_Pool *pool, void *resource)
{
    CMUTIL_Bool incmax = CMUTIL_False;
    CMUTIL_Pool_Internal *ipool = (CMUTIL_Pool_Internal*)pool;
    CMUTIL_CALL(ipool->avlmtx, Lock);
    CMUTIL_CALL(ipool->avail, AddTail, resource);
    ipool->totcnt++;
    if (ipool->totcnt > ipool->maxcnt) {
        incmax = CMUTIL_True;
        ipool->maxcnt++;
    }
    CMUTIL_CALL(ipool->avlmtx, Unlock);
    if (incmax)
        CMUTIL_CALL(ipool->semp, Release);
}

CMUTIL_STATIC void CMUTIL_PoolDestroy(
        CMUTIL_Pool *pool)
{
    CMUTIL_Pool_Internal *ipool = (CMUTIL_Pool_Internal*)pool;
    if (ipool) {
        if (CMUTIL_CALL(ipool->avail, GetSize) < ipool->totcnt)
            CMLogWarn("pool resources are not returned.");
        if (ipool->intimer && ipool->timer)
            CMUTIL_CALL(ipool->timer, Destroy);
        if (!ipool->intimer && ipool->pingtester)
            CMUTIL_CALL(ipool->pingtester, Cancel);
        if (ipool->semp)
            CMUTIL_CALL(ipool->semp, Destroy);
        if (ipool->avlmtx)
            CMUTIL_CALL(ipool->avlmtx, Destroy);
        if (ipool->avail) {
            while (CMUTIL_CALL(ipool->avail, GetSize) > 0) {
                void *rsrc = CMUTIL_CALL(ipool->avail, RemoveFront);
                ipool->destroyf(rsrc, ipool->udata);
            }
            CMUTIL_CALL(ipool->avail, Destroy);
        }
        CMFree(ipool);
    }
}

CMUTIL_STATIC void CMUTIL_PoolPingTester(void *p)
{
    CMUTIL_Pool_Internal *ipool = (CMUTIL_Pool_Internal*)p;
    CMUTIL_Pool *pool = (CMUTIL_Pool*)ipool;
    int i, size = CMUTIL_CALL(ipool->avail, GetSize);
    for (i=0; i<size; i++) {
        /*
         * if "test on borrow" option is on,
         * test will automatically performed when checkout.
         */
        void *rsrc = CMUTIL_CALL(pool, CheckOut, 1);
        if (!ipool->testonb) {
            if (!ipool->testf(rsrc, ipool->udata)) {
                rsrc = ipool->createf(ipool->udata);
                if (rsrc == NULL)
                    CMLogError("cannot create resource.");
            }
        }
        if (rsrc)
            CMUTIL_CALL(pool, Release, rsrc);
    }
}

static CMUTIL_Pool g_cmutil_pool = {
    CMUTIL_PoolCheckOut,
    CMUTIL_PoolRelease,
    CMUTIL_PoolAddResource,
    CMUTIL_PoolDestroy
};

CMUTIL_Pool *CMUTIL_PoolCreateInternal(
        CMUTIL_Mem *memst,
        int initcnt, int maxcnt,
        void *(*createproc)(void *),
        void (*destroyproc)(void *, void *),
        CMUTIL_Bool (*testproc)(void *, void *),
        long pinginterval,
        CMUTIL_Bool testonborrow,
        void *udata, CMUTIL_Timer *timer)
{
    int i;
    CMUTIL_Pool_Internal *res = memst->Alloc(sizeof(CMUTIL_Pool_Internal));
    memset(res, 0x0, sizeof(CMUTIL_Pool_Internal));
    memcpy(res, &g_cmutil_pool, sizeof(CMUTIL_Pool));
    res->avail = CMUTIL_ListCreateInternal(memst, NULL);
    for (i=0; i<initcnt; i++)
        CMUTIL_CALL(res->avail, AddTail, createproc(udata));
    res->semp = CMUTIL_SemaphoreCreateInternal(memst, maxcnt);
    res->avlmtx = CMUTIL_MutexCreateInternal(memst);
    res->createf = createproc;
    res->destroyf = destroyproc;
    res->testf = testproc;
    res->pingintv = pinginterval;
    res->testonb = testonborrow;
    res->udata = udata;
    if (timer)
        res->timer = timer;
    else {
        res->timer = CMUTIL_TimerCreateInternal(memst, 1000, 1);
        res->intimer = CMUTIL_True;
    }
    res->pingtester = CMUTIL_CALL(
                res->timer, ScheduleDelayRepeat, pinginterval, pinginterval,
                CMUTIL_PoolPingTester, res);
    return (CMUTIL_Pool*)res;
}

CMUTIL_Pool *CMUTIL_PoolCreate(
        int initcnt, int maxcnt,
        void *(*createproc)(void *),
        void (*destroyproc)(void *, void *),
        CMUTIL_Bool (*testproc)(void *, void *),
        long pinginterval, CMUTIL_Bool testonborrow,
        void *udata, CMUTIL_Timer *timer)
{
    return CMUTIL_PoolCreateInternal(
                CMUTIL_GetMem(), initcnt, maxcnt,
                createproc, destroyproc, testproc,
                pinginterval, testonborrow, udata, timer);
}
