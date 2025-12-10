//
// Created by Dennis Park on 25. 9. 30..
//

#include <string.h>

#include "../src/libcmutils.h"

CMUTIL_LogDefine("test.array")

void destructor(void *data) {
    CMFree(data);
}

int str_comparator(const void *a, const void *b) {
    return strcmp(a, b);
}

int main()
{
    int ir = -1;
    CMUTIL_Init(CMMemRecycle);

    int a[] = {1, 2, 3};

    CMUTIL_Iterator *iter = NULL;
    CMUTIL_Array *arr = CMUTIL_ArrayCreateEx(
        4, NULL, NULL);
    CMLogInfo("array creation with initial capacity - passed");

    CMCall(arr, Add, &a[2]);
    CMLogInfo("array add element - passed");
    CMCall(arr, Add, &a[1]);
    CMCall(arr, Add, &a[0]);

    if (CMCall(arr, GetSize) != 3) {
        CMLogError("size mismatch after adding elements");
        goto END_POINT;
    }
    CMLogInfo("array size validation - passed");

    CMCall(arr, Destroy); arr = NULL;


    arr = CMUTIL_ArrayCreateEx(5, str_comparator, destructor);
    CMLogInfo("sorted array creation with initial capacity - passed");

    CMCall(arr, Add, CMStrdup("banana"));
    CMCall(arr, Add, CMStrdup("cherry"));
    CMCall(arr, Add, CMStrdup("apple"));

    if (strcmp(CMCall(arr, GetAt, 0), "apple") != 0) {
        CMLogError("first element of sorted array must be 'apple' but '%s' - failed",
            CMCall(arr, GetAt, 0));
        CMCall(arr, Destroy);
        goto END_POINT;
    }
    CMLogInfo("sorting test - passed");

    if (CMCall(arr, GetSize) != 3) {
        CMLogError("size mismatch after adding string elements - failed");
        goto END_POINT;
    }

    iter = CMCall(arr, Iterator);
    while (CMCall(iter, HasNext)) {
        char *s = (char*)CMCall(iter, Next);
        CMLogInfo(" - %s", s);
    }
    CMCall(iter, Destroy); iter = NULL;
    CMLogInfo("array iterator test - passed");


    CMFree(CMCall(arr, RemoveAt, 0));
    if (CMCall(arr, GetSize) != 2) {
        CMLogError("size mismatch after adding string elements");
        goto END_POINT;
    }

    iter = CMCall(arr, Iterator);
    CMLogInfo("array elements in order:");
    while (CMCall(iter, HasNext)) {
        char *s = (char*)CMCall(iter, Next);
        CMLogInfo(" - %s", s);
    }

    ir = 0;
END_POINT:
    if (iter) CMCall(iter, Destroy);
    if (arr) CMCall(arr, Destroy);
    CMUTIL_Clear();
    return ir;
}