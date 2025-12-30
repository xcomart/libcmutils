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

CMUTIL_LogDefine("cmutils.pool")

typedef struct CMUTIL_Pool_Internal {
    CMUTIL_Pool         base;
    CMUTIL_Timer        *timer;
    CMUTIL_Semaphore    *semp;
    long                pingintv;
    CMUTIL_List         *avail;
    CMUTIL_Mutex        *avlmtx;
    void                *udata;
    void                *(*createf)(void*);
    void                (*destroyf)(void*,void*);
    CMBool              (*testf)(void*,void*);
    CMUTIL_TimerTask    *pingtester;
    CMUTIL_Mem          *memst;
    CMBool              intimer;
    uint32_t            totcnt;
    uint32_t            maxcnt;
    uint32_t            mincnt;
    CMBool              testonb;
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
            if (res != NULL)
                ipool->totcnt++;
        }
        CMCall(ipool->avlmtx, Unlock);
        if (res) {
            if (ipool->testonb) {
                if (!ipool->testf(res, ipool->udata)) {
                    ipool->destroyf(res, ipool->udata);
                    res = ipool->createf(ipool->udata);
                    if (res == NULL) {
                        CMCall(ipool->semp, Release);
                        CMLogError("resource creation failed.");
                    }
                }
            }
        } else {
            CMCall(ipool->semp, Release);
            CMLogError("resource creation failed.");
        }
        return res;
    }
    CMLogWarn("resource get timed out.");
    return NULL;
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
         * test will automatically be performed when checkout.
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
        CMPoolItemCreateCB createproc,
        CMPoolItemFreeCB destroyproc,
        CMPoolItemTestCB testproc,
        long pinginterval,
        CMBool testonborrow,
        void *udata,
        CMUTIL_Timer *timer)
{
    int i;
    CMUTIL_Pool_Internal *res = memst->Alloc(sizeof(CMUTIL_Pool_Internal));
    memset(res, 0x0, sizeof(CMUTIL_Pool_Internal));
    memcpy(res, &g_cmutil_pool, sizeof(CMUTIL_Pool));
    res->memst = memst;
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
    res->mincnt = initcnt;
    if (initcnt > maxcnt)
        res->maxcnt = initcnt;
    else
        res->maxcnt = maxcnt;
    if (timer)
        res->timer = timer;
    else {
        res->timer = CMUTIL_TimerCreateInternal(memst, 1000, 1);
        res->intimer = CMTrue;
    }
    res->pingtester = CMCall(
                res->timer, ScheduleDelayRepeat, pinginterval, pinginterval,
                CMTrue, CMUTIL_PoolPingTester, res);
    return (CMUTIL_Pool*)res;
}

CMUTIL_Pool *CMUTIL_PoolCreate(
        int initcnt, int maxcnt,
        CMPoolItemCreateCB createproc,
        CMPoolItemFreeCB destroyproc,
        CMPoolItemTestCB testproc,
        long pinginterval, CMBool testonborrow,
        void *udata, CMUTIL_Timer *timer)
{
    return CMUTIL_PoolCreateInternal(
                CMUTIL_GetMem(), initcnt, maxcnt,
                createproc, destroyproc, testproc,
                pinginterval, testonborrow, udata, timer);
}
