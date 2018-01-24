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

#if !defined(MSWIN)
# include <poll.h>
#endif
CMUTIL_LogDefine("cmutil.nio")

//*****************************************************************************
// CMUTIL NIO implementation
//*****************************************************************************

typedef struct CMUTIL_NIOBuffer_Internal {
    CMUTIL_NIOBuffer    base;
    uint8_t             *buffer;
    CMUTIL_Mem          *memst;
    CMBool              isreading;
    int                 position;
    int                 limit;
    int                 capacity;
    int                 mark;
    char                dummy_padder[4];
} CMUTIL_NIOBuffer_Internal;

CMUTIL_STATIC CMUTIL_NIOBuffer *CMUTIL_NIOBufferFlip(CMUTIL_NIOBuffer *buffer)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    if (ib->isreading) {
        ib->position = 0;
        ib->limit = ib->capacity;
        ib->isreading = CMFalse;
    } else {
        ib->limit = ib->position;
        ib->position = 0;
        ib->isreading = CMTrue;
    }
    ib->mark = 0;
    return buffer;
}

CMUTIL_STATIC CMBool CMUTIL_NIOBufferHasRemaining(
        const CMUTIL_NIOBuffer *buffer)
{
    const CMUTIL_NIOBuffer_Internal *ib =
            (const CMUTIL_NIOBuffer_Internal*)buffer;
    return ib->position < ib->limit? CMTrue:CMFalse;
}

CMUTIL_STATIC CMUTIL_NIOBuffer *CMUTIL_NIOBufferClear(
        CMUTIL_NIOBuffer *buffer)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    ib->limit = ib->capacity;
    ib->position = 0;
    ib->isreading = CMFalse;
    ib->mark = 0;
    return buffer;
}

CMUTIL_STATIC int CMUTIL_NIOBufferCapacity(
        const CMUTIL_NIOBuffer *buffer)
{
    const CMUTIL_NIOBuffer_Internal *ib =
            (const CMUTIL_NIOBuffer_Internal*)buffer;
    return ib->capacity;
}

CMUTIL_STATIC CMUTIL_NIOBuffer *CMUTIL_NIOBufferPut(
        CMUTIL_NIOBuffer *buffer, int data)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    if (ib->isreading) {
        CMLogErrorS("buffer is reading mode.");
        return NULL;
    }
    if (ib->position < ib->limit) {
        ib->buffer[ib->position++] = (uint8_t)data;
    } else {
        CMLogErrorS("buffer overflow.");
        return NULL;
    }
    return buffer;
}

CMUTIL_STATIC CMUTIL_NIOBuffer *CMUTIL_NIOBufferPutBytes(
        CMUTIL_NIOBuffer *buffer, const uint8_t *data, int length)
{
    return CMCall(buffer, PutBytesPart, data, 0, length);
}

CMUTIL_STATIC CMUTIL_NIOBuffer *CMUTIL_NIOBufferPutBytesPart(
        CMUTIL_NIOBuffer *buffer, const uint8_t *data, int offset, int length)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    if (ib->isreading) {
        CMLogErrorS("buffer is reading mode.");
        return NULL;
    }
    if (ib->limit - ib->position - length < 0) {
        CMLogErrorS("buffer overflow.");
        return NULL;
    }
    memcpy(ib->buffer, data+offset, (size_t)length);
    ib->position += length;
    return buffer;
}

CMUTIL_STATIC int CMUTIL_NIOBufferGet(
        CMUTIL_NIOBuffer *buffer)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    if (!ib->isreading) {
        CMLogErrorS("buffer is writing mode.");
        return -1;
    }
    if (ib->position < ib->limit) {
        return (uint8_t)ib->buffer[ib->position++];
    } else {
        CMLogErrorS("buffer underflow.");
        return -1;
    }
}

CMUTIL_STATIC int CMUTIL_NIOBufferGetAt(
        CMUTIL_NIOBuffer *buffer, int index)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    if (ib->isreading) {
        if (index < ib->limit) {
            return (uint8_t)ib->buffer[index];
        } else {
            CMLogErrorS("buffer underflow.");
            return -1;
        }
    } else {
        if (index < ib->position) {
            return (uint8_t)ib->buffer[index];
        } else {
            CMLogErrorS("buffer underflow.");
            return -1;
        }
    }
}

CMUTIL_STATIC CMUTIL_NIOBuffer *CMUTIL_NIOBufferGetBytes(
        CMUTIL_NIOBuffer *buffer, uint8_t *dest, int length)
{
    return CMCall(buffer, GetBytesPart, dest, 0, length);
}

CMUTIL_STATIC CMUTIL_NIOBuffer *CMUTIL_NIOBufferGetBytesPart(
        CMUTIL_NIOBuffer *buffer, uint8_t *dest, int offset, int length)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    if (!ib->isreading) {
        CMLogErrorS("buffer is writing mode.");
        return NULL;
    }
    if (ib->limit - ib->position - length < 0) {
        CMLogErrorS("buffer underflow.");
        return NULL;
    }
    memcpy(dest + offset, ib->buffer + ib->position, (size_t)length);
    ib->position += length;
    return buffer;
}

CMUTIL_STATIC void CMUTIL_NIOBufferCompact(
        CMUTIL_NIOBuffer *buffer)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    ib->mark = 0;
    if (ib->isreading) {
        int remain = ib->limit - ib->position;
        memmove(ib->buffer, ib->buffer+ib->position, (size_t)remain);
        ib->position = remain;
        ib->limit = ib->capacity;
        ib->isreading = CMFalse;
    } else {
        // same as Flip in write mode
        CMCall(buffer, Flip);
    }
}

CMUTIL_STATIC CMUTIL_NIOBuffer *CMUTIL_NIOBufferMark(
        CMUTIL_NIOBuffer *buffer)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    ib->mark = ib->position;
    return buffer;
}

CMUTIL_STATIC CMUTIL_NIOBuffer *CMUTIL_NIOBufferReset(
        CMUTIL_NIOBuffer *buffer)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    ib->position = ib->mark;
    return buffer;
}

CMUTIL_STATIC CMUTIL_NIOBuffer *CMUTIL_NIOBufferRewind(
        CMUTIL_NIOBuffer *buffer)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    ib->position = 0;
    return buffer;
}

CMUTIL_STATIC int CMUTIL_NIOBufferLimit(
        const CMUTIL_NIOBuffer *buffer)
{
    const CMUTIL_NIOBuffer_Internal *ib =
            (const CMUTIL_NIOBuffer_Internal*)buffer;
    return ib->limit;
}

CMUTIL_STATIC int CMUTIL_NIOBufferPosition(
        const CMUTIL_NIOBuffer *buffer)
{
    const CMUTIL_NIOBuffer_Internal *ib =
            (const CMUTIL_NIOBuffer_Internal*)buffer;
    return ib->position;
}

CMUTIL_STATIC CMBool CMUTIL_NIOBufferEquals(
        const CMUTIL_NIOBuffer *buffer, const CMUTIL_NIOBuffer *buffer2)
{
    return CMCall(buffer, CompareTo, buffer2) == 0? CMTrue:CMFalse;
}

CMUTIL_STATIC int CMUTIL_NIOBufferCompareTo(
        const CMUTIL_NIOBuffer *buffer, const CMUTIL_NIOBuffer *buffer2)
{
    const CMUTIL_NIOBuffer_Internal *iba =
            (const CMUTIL_NIOBuffer_Internal*)buffer;
    const CMUTIL_NIOBuffer_Internal *ibb =
            (const CMUTIL_NIOBuffer_Internal*)buffer2;
    int res = iba->limit - ibb->limit;
    if (res == 0)
        res = memcmp(iba->buffer, ibb->buffer, (size_t)iba->limit);
    return res;
}

CMUTIL_STATIC void CMUTIL_NIOBufferDestroy(CMUTIL_NIOBuffer *buffer)
{
    CMUTIL_NIOBuffer_Internal *ib = (CMUTIL_NIOBuffer_Internal*)buffer;
    if (ib) {
        if (ib->buffer)
            ib->memst->Free(ib->buffer);
        ib->memst->Free(ib);
    }
}

static CMUTIL_NIOBuffer g_cmutil_niobuffer = {
    CMUTIL_NIOBufferFlip,
    CMUTIL_NIOBufferHasRemaining,
    CMUTIL_NIOBufferClear,
    CMUTIL_NIOBufferCapacity,
    CMUTIL_NIOBufferPut,
    CMUTIL_NIOBufferPutBytes,
    CMUTIL_NIOBufferPutBytesPart,
    CMUTIL_NIOBufferGet,
    CMUTIL_NIOBufferGetAt,
    CMUTIL_NIOBufferGetBytes,
    CMUTIL_NIOBufferGetBytesPart,
    CMUTIL_NIOBufferCompact,
    CMUTIL_NIOBufferMark,
    CMUTIL_NIOBufferReset,
    CMUTIL_NIOBufferRewind,
    CMUTIL_NIOBufferLimit,
    CMUTIL_NIOBufferPosition,
    CMUTIL_NIOBufferEquals,
    CMUTIL_NIOBufferCompareTo,
    CMUTIL_NIOBufferDestroy
};

CMUTIL_NIOBuffer *CMUTIL_NIOBufferCreateInternal(
        CMUTIL_Mem *mem, int capacity)
{
    CMUTIL_NIOBuffer_Internal *ib =
            mem->Alloc(sizeof(CMUTIL_NIOBuffer_Internal));
    memset(ib, 0x0, sizeof(CMUTIL_NIOBuffer_Internal));

    memcpy(ib, &g_cmutil_niobuffer, sizeof(CMUTIL_NIOBuffer));
    ib->buffer = mem->Alloc(sizeof(uint8_t) * (size_t)capacity);
    ib->capacity = ib->limit = capacity;
    ib->memst = mem;
    return (CMUTIL_NIOBuffer*)ib;
}

CMUTIL_NIOBuffer *CMUTIL_NIOBufferCreate(int capacity)
{
    return CMUTIL_NIOBufferCreateInternal(CMUTIL_GetMem(), capacity);
}

typedef struct CMUTIL_NIOChannel_Internal {
    CMUTIL_NIOChannel   base;
    CMUTIL_Mem          *memst;
    SOCKET              fd;
} CMUTIL_NIOChannel_Internal;

typedef struct CMUTIL_NIOSelector_Internal {
    CMUTIL_NIOSelector  base;
    CMUTIL_Mem          *memst;
    CMUTIL_Map          *keys;
    CMUTIL_Socket       *sender;
    CMUTIL_Socket       *receiver;
    CMUTIL_RWLock       *rwlock;
    CMUTIL_Array        *selected;
} CMUTIL_NIOSelector_Internal;

typedef struct CMUTIL_NIOSelectionKey_Internal {
    CMUTIL_NIOSelectionKey      base;
    CMUTIL_NIOSelector_Internal *selector;
    CMUTIL_NIOChannel_Internal  *channel;
    int                         interops;
    int                         selectedops;
    CMBool                      canceled;
    char                        dummy_padder[4];
} CMUTIL_NIOSelectionKey_Internal;

CMUTIL_STATIC int CMUTIL_NIOSelectorSelectTimeout(
        CMUTIL_NIOSelector *selector, long timeout)
{
    CMUTIL_NIOSelector_Internal *is = (CMUTIL_NIOSelector_Internal*)selector;
    struct pollfd *pfds = NULL;
    size_t cnt = CMCall(is->keys, GetSize);
    uint32_t i;
    int res = 0;
    int ntout;

    CMCall(is->selected, Clear);
    cnt++;
    pfds = is->memst->Alloc(sizeof(struct pollfd) * cnt);
    memset(pfds, 0x0, sizeof(struct pollfd) * cnt);
    for (i=0; i<(cnt-1); i++) {
        CMUTIL_NIOSelectionKey_Internal *ik =
                (CMUTIL_NIOSelectionKey_Internal*)CMCall(is->keys, GetAt, i);
        if (ik->interops & CMUTIL_NIO_OP_ACCEPT ||
                ik->interops & CMUTIL_NIO_OP_READ)
            pfds[i].events = POLLIN;
        if (ik->interops & CMUTIL_NIO_OP_CONNECT ||
                ik->interops & CMUTIL_NIO_OP_WRITE)
            pfds[i].events |= POLLOUT;
        pfds[i].fd = ik->channel->fd;
        ik->selectedops = 0;
    }
    pfds[cnt-1].events = POLLIN;
    pfds[cnt-1].fd = CMCall(is->receiver, GetRawSocket);
    cnt++;

    // poll's timeout is int type, so implements long type timeout.
    while (timeout > 0 && res == 0) {
        if (timeout > INT32_MAX) {
            ntout = INT32_MAX;
        } else {
            ntout = (int)timeout;
        }
        timeout -= ntout;
        res = poll(pfds, cnt, ntout);
    }

    if (res > 0) {
        for (i=0; i<(cnt-1); i++) {
            if (pfds[i].revents) {
                CMUTIL_NIOSelectionKey_Internal *ik =
                        (CMUTIL_NIOSelectionKey_Internal*)CMCall(
                            is->keys, GetAt, i);
                if (ik->canceled)
                    continue;
                if (pfds[i].revents | POLLIN) {
                    if (ik->interops & CMUTIL_NIO_OP_ACCEPT)
                        ik->selectedops |= CMUTIL_NIO_OP_ACCEPT;
                    if (ik->interops & CMUTIL_NIO_OP_READ)
                        ik->selectedops |= CMUTIL_NIO_OP_READ;
                }
                if (pfds[i].revents | POLLOUT) {
                    if (ik->interops & CMUTIL_NIO_OP_CONNECT)
                        ik->selectedops |= CMUTIL_NIO_OP_CONNECT;
                    if (ik->interops & CMUTIL_NIO_OP_WRITE)
                        ik->selectedops |= CMUTIL_NIO_OP_WRITE;
                }
                CMCall(is->selected, Add, ik);
            }
        }
        if (pfds[cnt-1].revents) {
            CMCall(is->receiver, ReadByte);
            res = -1;
        }
    } else if (res < 0) {
        CMLogError("poll failed.");
    }

    return res;
}

CMUTIL_STATIC int CMUTIL_NIOSelectorSelect(
        CMUTIL_NIOSelector *selector)
{
    int res;
    // blocking until state being changed.
    while ((res = CMCall(selector, SelectTimeout, INT32_MAX)) == 0);
    return res;
}

CMUTIL_STATIC void CMUTIL_NIOSelectorClose(
        CMUTIL_NIOSelector *selector)
{

}

CMUTIL_STATIC CMBool CMUTIL_NIOSelectorIsOpen(
        const CMUTIL_NIOSelector *selector)
{

}