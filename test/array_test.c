//
// Created by Dennis Park on 25. 9. 30..
//

#include "../src/libcmutils.h"

CMUTIL_LogDefine("test.array_test")

int main()
{
    CMUTIL_Init(CMMemRecycle);

    int a[] = {1, 2, 3};

    CMUTIL_Array *arr = CMUTIL_ArrayCreateEx(
        4, NULL, NULL);

    CMLogDebug("array created, size=%zu", CMCall(arr, GetSize));

    CMCall(arr, Add, &a[0]);
    CMCall(arr, Add, &a[1]);
    CMCall(arr, Add, &a[2]);

    CMLogInfo("after adding 3 elements, size=%zu", CMCall(arr, GetSize));
    if (CMCall(arr, GetSize) != 3) {
        CMLogError("size mismatch after adding elements");
        CMCall(arr, Destroy);
        CMUTIL_Clear();
        return -1;
    }
    CMCall(arr, Destroy);

    CMUTIL_Clear();
    return 0;
}