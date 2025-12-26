//
// Created by 박성진 on 25. 12. 26..
//

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.process")

int main() {
    int ir = -1;
    CMUTIL_Init(CMUTIL_MEM_TYPE);

    CMUTIL_Process *proc = NULL;

    proc = CMUTIL_ProcessCreate(
        "/Users/comart", NULL, "/opt/homebrew/bin/openssl");
    ASSERT(proc != NULL, "Process Create");
    ASSERT(CMCall(proc, Start, CMProcStreamNone), "Process Start");

    ASSERT(CMCall(proc, Wait, 10*1000), "Process Wait");

    ir = 0;
END_POINT:
    if (proc) CMCall(proc, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}