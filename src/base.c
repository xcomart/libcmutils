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

//static CMUTIL_Mutex *g_cmutil_init_mutex = NULL;
static int g_cmutil_init_cnt = 0;

void CMUTIL_Init(CMMemOper memoper)
{
    if (g_cmutil_init_cnt == 0) {
        CMUTIL_CallStackInit();
        CMUTIL_MemDebugInit(memoper);
        CMUTIL_ThreadInit();
        CMUTIL_StringBaseInit();
        CMUTIL_XmlInit();
        CMUTIL_NetworkInit();
        CMUTIL_LogInit();
    }
    g_cmutil_init_cnt++;
}

void CMUTIL_Clear()
{
    if (g_cmutil_init_cnt > 0) {
        g_cmutil_init_cnt--;
        if (g_cmutil_init_cnt == 0) {
            CMUTIL_LogClear();
            CMUTIL_NetworkClear();
            CMUTIL_XmlClear();
            CMUTIL_StringBaseClear();
            CMUTIL_ThreadClear();
            CMUTIL_MemDebugClear();
            CMUTIL_CallStackClear();
        }
    }
}

void CMUTIL_UnusedP(void *p,...)
{
    (void)(p);
    // does nothing.
}

const char *CMUTIL_GetLibVersion()
{
    static char *verstr = LIBCMUTIL_VER;
    return verstr;
}
