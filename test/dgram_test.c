//
// Created by 박성진 on 25. 12. 11..
//

#include "libcmutils.h"

CMUTIL_LogDefine("test.dgram")

void *dgram_client(void *udata) {
    int ir = -1;
    CMUTIL_DGramSocket *dsock = (CMUTIL_DGramSocket *)udata;

    CMCall(dsock)

    return NULL;
}

int main() {
    int ir = -1;
    CMUTIL_Init(CMMemRecycle);

    CMUTIL_DGramSocket *dsock = CMUTIL_DGramSocketCreate();
    if (dsock == NULL) {
        CMLogError("datagram socket creation failed - failed");
        goto END_POINT;
    }

    ir = 0;
END_POINT:
    if (dsock) CMCall(dsock, Close);
    CMUTIL_Clear();
    return ir;
}