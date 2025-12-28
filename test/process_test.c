//
// Created by 박성진 on 25. 12. 26..
//

#include <stdlib.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.process")

int main() {
    int ir = -1;
    CMUTIL_Init(CMUTIL_MEM_TYPE);

    CMUTIL_Process *proc = NULL;
    CMUTIL_Map *env = NULL;

    env = CMUTIL_MapCreateEx(CMUTIL_MAP_DEFAULT, CMFalse, CMFree);

    const char *path = getenv("PATH");
    CMUTIL_String *pathstr = CMUTIL_StringCreateEx(0, path);
    CMCall(pathstr, AddString, ":/opt/homebrew/bin");
    path = CMCall(pathstr, GetCString);
    CMCall(env, Put, "PATH", CMStrdup(path), NULL);

    const char *home = getenv("HOME");

    proc = CMUTIL_ProcessCreate(home, env, "openssl", "help", "sha256");
    ASSERT(proc != NULL, "Process Create");
    ASSERT(CMCall(proc, Start, CMProcStreamNone), "Process Start");

    ASSERT(CMCall(proc, Wait, 10*1000) == 0, "Process Wait");

    ir = 0;
END_POINT:
    if (pathstr) CMCall(pathstr, Destroy);
    if (env) CMCall(env, Destroy);
    if (proc) CMCall(proc, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}