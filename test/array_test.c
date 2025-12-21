//
// Created by Dennis Park on 25. 9. 30..
//

#include <string.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.array")

void destructor(void *data) {
    CMFree(data);
}

int main()
{
    int ir = -1;
    CMUTIL_Init(CMUTIL_MEM_TYPE);

    int a[] = {1, 2, 3};

    char *str = NULL;
    CMUTIL_Iterator *iter = NULL;
    CMUTIL_Array *arr = CMUTIL_ArrayCreateEx(
        4, NULL, NULL);
    ASSERT(arr != NULL, "CMUTIL_ArrayCreateEx with capacity");
    ASSERT(CMCall(arr, GetSize) == 0, "GetSize");

    CMCall(arr, Add, &a[2]);
    ASSERT((CMCall(arr, GetSize) == 1 && *(int*)CMCall(arr, GetAt, 0) == 3), "Add");

    CMCall(arr, Add, &a[0]);
    CMCall(arr, Add, &a[0]);
    CMCall(arr, Add, &a[0]);
    CMCall(arr, Add, &a[0]);
    ASSERT(CMCall(arr, GetSize) == 5, "array extension");

    ASSERT(CMCall(arr, Remove, &a[2]) == &a[2] && CMCall(arr, GetSize) == 4, "Remove in unsorted array");

    ASSERT(CMCall(arr, RemoveAt, 1) == &a[0] && CMCall(arr, GetSize) == 3, "RemoveAt");

    CMCall(arr, InsertAt, &a[1], 1);
    ASSERT(*(int*)CMCall(arr, GetAt, 1) == 2 && CMCall(arr, GetSize) == 4, "InsertAt");

    ASSERT(CMCall(arr, SetAt, &a[2], 1) == &a[1] && *(int*)CMCall(arr, GetAt, 1) == 3 && CMCall(arr, GetSize) == 4, "SetAt & GetAt");

    uint32_t idx = 0;
    ASSERT(CMCall(arr, Find, &a[2], &idx) == &a[2] && idx == 1, "Find in unsorted array");

    ASSERT(CMCall(arr, Push, &a[1]) && CMCall(arr, GetSize) == 5, "Push");
    ASSERT(*(int*)CMCall(arr, Top) == 2, "Top");
    ASSERT(*(int*)CMCall(arr, Bottom) == 1, "Bottom");
    ASSERT(*(int*)CMCall(arr, Pop) == 2 && CMCall(arr, GetSize) == 4, "Pop");

    CMCall(arr, Clear);
    ASSERT(CMCall(arr, GetSize) == 0, "Clear");

    CMCall(arr, Destroy); arr = NULL;


    arr = CMUTIL_ArrayCreateEx(5, (int(*)(const void*,const void*))strcmp, destructor);
    ASSERT(arr != NULL, "CMUTIL_ArrayCreateEx with comparator and destructor");
    ASSERT(CMCall(arr, GetSize) == 0, "GetSize");

    CMCall(arr, Add, CMStrdup("banana"));
    CMCall(arr, Add, CMStrdup("cherry"));
    CMCall(arr, Add, CMStrdup("apple"));

    ASSERT(CMCall(arr, GetSize) == 3, "GetSize after adding 3 string elements");
    ASSERT(strcmp(CMCall(arr, GetAt, 0), "apple") == 0, "Check sorted order at index 0");

    str = (char*)CMCall(arr, Remove, "banana");
    ASSERT(strcmp(str, "banana") == 0 && CMCall(arr, GetSize) == 2, "Remove string element");
    CMFree(str); str = NULL;

    str = CMStrdup("banana");
    ASSERT(CMCall(arr, Push, str) == CMFalse && CMCall(arr, GetSize) == 2, "Push to sorted array should fail");
    CMCall(arr, Add, str); str = NULL;
    ASSERT(strcmp(CMCall(arr, Top), "cherry") == 0, "Top in sorted array");
    ASSERT(strcmp(CMCall(arr, Bottom), "apple") == 0, "Bottom in sorted array");
    str = CMCall(arr, Pop);
    ASSERT(strcmp(str, "cherry") == 0 && CMCall(arr, GetSize) == 2, "Pop in sorted array");
    CMCall(arr, Add, str); str = NULL;


    iter = CMCall(arr, Iterator);
    ASSERT(iter != NULL, "Iterator");
    int count = 0;
    while (CMCall(iter, HasNext)) {
        char *s = (char*)CMCall(iter, Next);
        CMLogInfo("%d - %s", count, s);
        count++;
    }
    CMCall(iter, Destroy); iter = NULL;
    ASSERT(count == CMCall(arr, GetSize), "Iterator count matches array size");

    str = (char*)CMCall(arr, Remove, "cherry");
    ASSERT(strcmp(str, "cherry") == 0 && CMCall(arr, GetSize) == 2, "Remove in sorted array");
    CMCall(arr, Add, str); str = NULL;

    str = CMCall(arr, RemoveAt, 1);
    ASSERT(strcmp(str, "banana") == 0 && CMCall(arr, GetSize) == 2, "RemoveAt in sorted array");
    CMCall(arr, Add, str); str = NULL;

    idx = 0;
    ASSERT(strcmp(CMCall(arr, Find, "banana", &idx), "banana") == 0 && idx == 1, "Find in sorted array");

    str = CMStrdup("damson");
    ASSERT(CMCall(arr, SetAt, str, 1) == NULL, "SetAt to sorted array should fail");
    ASSERT(strcmp(CMCall(arr, GetAt, 1), "banana") == 0, "GetAt in sorted array");
    ASSERT(CMCall(arr, GetSize) == 3, "GetSize after SetAt/GetAt in sorted array");

    ir = 0;
END_POINT:
    if (str) CMFree(str);
    if (iter) CMCall(iter, Destroy);
    if (arr) CMCall(arr, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}