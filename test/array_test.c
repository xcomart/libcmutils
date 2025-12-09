//
// Created by Dennis Park on 25. 9. 30..
//

#include <string.h>

#include "../src/libcmutils.h"

CMUTIL_LogDefine("test.array_test")

void destructor(void *data) {
    CMUTIL_GetMem()->Free(data);
}

int str_comparator(const void *a, const void *b) {
    return strcmp(a, b);
}

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


    arr = CMUTIL_ArrayCreateEx(5, str_comparator, destructor);

    CMCall(arr, Add, CMUTIL_GetMem()->Strdup("banana"));
    CMCall(arr, Add, CMUTIL_GetMem()->Strdup("cherry"));
    CMCall(arr, Add, CMUTIL_GetMem()->Strdup("apple"));
    CMLogInfo("after adding 3 string elements, size=%zu", CMCall(arr, GetSize));
    if (CMCall(arr, GetSize) != 3) {
        CMLogError("size mismatch after adding string elements");
        CMCall(arr, Destroy);
        CMUTIL_Clear();
        return -1;
    }

    CMUTIL_Iterator *iter = CMCall(arr, Iterator);
    CMLogInfo("array elements in order:");
    while (CMCall(iter, HasNext)) {
        char *s = (char*)CMCall(iter, Next);
        CMLogInfo(" - %s", s);
    }
    CMCall(iter, Destroy);


    CMUTIL_GetMem()->Free(CMCall(arr, RemoveAt, 0));
    if (CMCall(arr, GetSize) != 2) {
        CMLogError("size mismatch after adding string elements");
        CMCall(arr, Destroy);
        CMUTIL_Clear();
        return -1;
    }

    iter = CMCall(arr, Iterator);
    CMLogInfo("array elements in order:");
    while (CMCall(iter, HasNext)) {
        char *s = (char*)CMCall(iter, Next);
        CMLogInfo(" - %s", s);
    }
    CMCall(iter, Destroy);



    CMCall(arr, Destroy);

    CMUTIL_Clear();
    return 0;
}