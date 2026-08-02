/*
 * 12 - Resource pooling
 *
 * Shows: CMUTIL_Pool with create / destroy / validity callbacks, CheckOut
 *        with a timeout, Release, and concurrent borrowers.
 *
 * Use it for objects that are expensive to build - database handles,
 * sockets, parsers. The pool keeps between initcnt and maxcnt of them alive
 * and pings the idle ones on an interval.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.pool")

/* Stands in for a real connection. */
typedef struct Connection {
    int id;
    int uses;
} Connection;

typedef struct Factory {
    int next_id;
    int live;
    CMUTIL_Mutex *mutex;
} Factory;

static void *connection_create(void *udata)
{
    Factory *factory = (Factory*)udata;
    Connection *conn = CMAlloc(sizeof(Connection));

    CMCall(factory->mutex, Lock);
    conn->id = factory->next_id++;
    conn->uses = 0;
    factory->live++;
    CMCall(factory->mutex, Unlock);

    CMLogDebug("created connection %d", conn->id);
    return conn;
}

static void connection_destroy(void *resource, void *udata)
{
    Factory *factory = (Factory*)udata;
    Connection *conn = (Connection*)resource;

    CMCall(factory->mutex, Lock);
    factory->live--;
    CMCall(factory->mutex, Unlock);

    CMLogDebug("destroyed connection %d after %d uses", conn->id, conn->uses);
    CMFree(conn);
}

/* Called on the ping interval, and on every checkout when testonborrow is
 * CMTrue. Returning CMFalse retires the resource. */
static CMBool connection_test(void *resource, void *udata)
{
    Connection *conn = (Connection*)resource;
    CMUTIL_UNUSED(udata);
    return conn->id >= 0 ? CMTrue : CMFalse;
}

static void borrower(void *udata)
{
    CMUTIL_Pool *pool = (CMUTIL_Pool*)udata;
    int i;

    for (i = 0; i < 4; i++) {
        /* Waits up to 1000 ms for a free resource; NULL means it gave up. */
        Connection *conn = (Connection*)CMCall(pool, CheckOut, 1000);
        if (conn == NULL) {
            CMLogWarn("checkout timed out");
            continue;
        }

        conn->uses++;
        CMLogInfo("using connection %d (use #%d)", conn->id, conn->uses);
        usleep(10000);

        /* Always give it back, on every path. */
        CMCall(pool, Release, conn);
    }
}

int main(void)
{
    CMUTIL_Pool *pool;
    CMUTIL_ThreadPool *threads;
    Factory factory;
    int i;

    sample_init();

    factory.next_id = 1;
    factory.live = 0;
    factory.mutex = CMUTIL_MutexCreate();

    SAMPLE_SECTION("creating the pool");

    pool = CMUTIL_PoolCreate(
            2,                  /* initial resources, built up front */
            4,                  /* maximum resources                 */
            connection_create,
            connection_destroy,
            connection_test,
            1000,               /* ping idle resources every second  */
            CMFalse,            /* do not test on every borrow       */
            &factory,           /* udata handed to the callbacks     */
            NULL);              /* NULL -> the pool creates a timer  */

    CMLogInfo("pool created, %d connections live", factory.live);

    SAMPLE_SECTION("borrowing from several threads");

    threads = CMUTIL_ThreadPoolCreate(-1, "borrowers");
    for (i = 0; i < 6; i++)
        CMCall(threads, Execute, borrower, pool);
    CMCall(threads, Wait);
    CMCall(threads, Destroy);

    CMLogInfo("%d connections live at the end (max was 4)", factory.live);

    /* Destroy releases every pooled resource through connection_destroy. */
    CMCall(pool, Destroy);
    CMLogInfo("%d connections live after Destroy", factory.live);

    CMCall(factory.mutex, Destroy);

    return sample_exit(0);
}
