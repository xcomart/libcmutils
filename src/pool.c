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
    CMUTIL_Timer		*timer;
    CMUTIL_Semaphore	*semp;
    long				pingintv;
    CMUTIL_List			*avail;
    CMUTIL_Mutex		*avlmtx;
    void				*udata;
    void				*(*createf)(void*);
    void				(*destroyf)(void*,void*);
    CMBool			(*testf)(void*,void*);
    CMUTIL_TimerTask	*pingtester;
    CMUTIL_Mem          *memst;
    CMBool			intimer;
    uint32_t				totcnt;
    uint32_t				maxcnt;
    CMBool			testonb;
} CMUTIL_Pool_Internal;

CMUTIL_STATIC void *CMUTIL_PoolCheckOut(
        CMUTIL_Pool *pool, long millisec)
{
    CMUTIL_Pool_Internal *ipool = (CMUTIL_Pool_Internal*)pool;
    if (CMCall(ipool->semp, Acquire, millisec)) {
        void *res = NULL;
        CMCall(ipool->avlmtx, Lock);
        if (CMCall(ipool->avail, GetSize) > 0) {
            res = CMCall(ipool->avail, RemoveFront);
        } else {
            res = ipool->createf(ipool->udata);
            if (res == NULL)
                CMLogError("resource creation failed.");
            else
                ipool->totcnt++;
        }
        CMCall(ipool->avlmtx, Unlock);
        if (res && ipool->testonb) {
            if (!ipool->testf(res, ipool->udata)) {
                ipool->destroyf(res, ipool->udata);
                res = ipool->createf(ipool->udata);
                if (res == NULL) {
                    CMCall(ipool->semp, Release);
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
    CMCall(ipool->avlmtx, Lock);
    CMCall(ipool->avail, AddTail, resource);
    CMCall(ipool->avlmtx, Unlock);
    CMCall(ipool->semp, Release);
}

CMUTIL_STATIC void CMUTIL_PoolAddResource(
        CMUTIL_Pool *pool, void *resource)
{
    CMBool incmax = CMFalse;
    CMUTIL_Pool_Internal *ipool = (CMUTIL_Pool_Internal*)pool;
    CMCall(ipool->avlmtx, Lock);
    CMCall(ipool->avail, AddTail, resource);
    ipool->totcnt++;
    if (ipool->totcnt > ipool->maxcnt) {
        incmax = CMTrue;
        ipool->maxcnt++;
    }
    CMCall(ipool->avlmtx, Unlock);
    if (incmax)
        CMCall(ipool->semp, Release);
}

CMUTIL_STATIC void CMUTIL_PoolDestroy(
        CMUTIL_Pool *pool)
{
    CMUTIL_Pool_Internal *ipool = (CMUTIL_Pool_Internal*)pool;
    if (ipool) {
        if (CMCall(ipool->avail, GetSize) < ipool->totcnt)
            CMLogWarn("pool resources are not returned.");
        if (ipool->intimer && ipool->timer)
            CMCall(ipool->timer, Destroy);
        if (!ipool->intimer && ipool->pingtester)
            CMCall(ipool->pingtester, Cancel);
        if (ipool->semp)
            CMCall(ipool->semp, Destroy);
        if (ipool->avlmtx)
            CMCall(ipool->avlmtx, Destroy);
        if (ipool->avail) {
            while (CMCall(ipool->avail, GetSize) > 0) {
                void *rsrc = CMCall(ipool->avail, RemoveFront);
                ipool->destroyf(rsrc, ipool->udata);
            }
            CMCall(ipool->avail, Destroy);
        }
        CMFree(ipool);
    }
}

CMUTIL_STATIC void CMUTIL_PoolPingTester(void *p)
{
    CMUTIL_Pool_Internal *ipool = (CMUTIL_Pool_Internal*)p;
    CMUTIL_Pool *pool = (CMUTIL_Pool*)ipool;
    uint32_t i;
    size_t size = CMCall(ipool->avail, GetSize);
    for (i=0; i<size; i++) {
        /*
         * if "test on borrow" option is on,
         * test will automatically performed when checkout.
         */
        void *rsrc = CMCall(pool, CheckOut, 1);
        if (!ipool->testonb) {
            if (!ipool->testf(rsrc, ipool->udata)) {
                rsrc = ipool->createf(ipool->udata);
                if (rsrc == NULL)
                    CMLogError("cannot create resource.");
            }
        }
        if (rsrc)
            CMCall(pool, Release, rsrc);
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
        CMBool (*testproc)(void *, void *),
        long pinginterval,
        CMBool testonborrow,
        void *udata, CMUTIL_Timer *timer)
{
    int i;
    CMUTIL_Pool_Internal *res = memst->Alloc(sizeof(CMUTIL_Pool_Internal));
    memset(res, 0x0, sizeof(CMUTIL_Pool_Internal));
    memcpy(res, &g_cmutil_pool, sizeof(CMUTIL_Pool));
    res->avail = CMUTIL_ListCreateInternal(memst, NULL);
    for (i=0; i<initcnt; i++)
        CMCall(res->avail, AddTail, createproc(udata));
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
        res->intimer = CMTrue;
    }
    res->pingtester = CMCall(
                res->timer, ScheduleDelayRepeat, pinginterval, pinginterval,
                CMUTIL_PoolPingTester, res);
    return (CMUTIL_Pool*)res;
}

CMUTIL_Pool *CMUTIL_PoolCreate(
        int initcnt, int maxcnt,
        void *(*createproc)(void *),
        void (*destroyproc)(void *, void *),
        CMBool (*testproc)(void *, void *),
        long pinginterval, CMBool testonborrow,
        void *udata, CMUTIL_Timer *timer)
{
    return CMUTIL_PoolCreateInternal(
                CMUTIL_GetMem(), initcnt, maxcnt,
                createproc, destroyproc, testproc,
                pinginterval, testonborrow, udata, timer);
}
