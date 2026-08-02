/*
 * 02 - Arrays and lists
 *
 * Shows: CMUTIL_Array (plain, sorted, owning, stack operations, search),
 *        CMUTIL_List (deque operations, splicing) and CMUTIL_Iterator.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.arrays")

/* Comparator for an array of C strings. Sorting is what enables Find()
 * to use a binary search - and what makes Push/InsertAt/SetAt fail,
 * because position is decided by the comparator, not by the caller. */
static int compare_cstring(const void *a, const void *b)
{
    return strcmp((const char*)a, (const char*)b);
}

static void dump_array(const char *title, CMUTIL_Array *arr)
{
    CMUTIL_Iterator *iter = CMCall(arr, Iterator);
    CMUTIL_String *line = CMUTIL_StringCreate();
    /* CMCall must never be nested inside another CMCall's argument list:
     * the inner call is left unexpanded and will not compile. Use a
     * temporary instead. */
    unsigned count = (unsigned)CMCall(arr, GetSize);

    CMCall(line, AddPrint, "%s (%u):", title, count);
    while (CMCall(iter, HasNext)) {
        const char *item = (const char*)CMCall(iter, Next);
        CMCall(line, AddPrint, " %s", item);
    }
    CMLogInfo("%s", CMCall(line, GetCString));

    CMCall(line, Destroy);
    CMCall(iter, Destroy);
}

static void sample_plain_array(void)
{
    int values[] = { 10, 20, 30, 40 };
    CMUTIL_Array *arr;
    uint32_t idx = 0;
    void *found;

    SAMPLE_SECTION("plain array - insertion order, no ownership");

    /* CMUTIL_ArrayCreate() == CMUTIL_ArrayCreateEx(10, NULL, NULL) */
    arr = CMUTIL_ArrayCreateEx(4, NULL, NULL);

    /* The third argument receives the replaced element, NULL if not needed. */
    CMCall(arr, Add, &values[0], NULL);
    CMCall(arr, Add, &values[1], NULL);
    CMCall(arr, Add, &values[2], NULL);
    CMLogInfo("size after 3 adds: %u", (unsigned)CMCall(arr, GetSize));

    CMCall(arr, InsertAt, &values[3], 1);
    CMLogInfo("value at index 1 after InsertAt: %d",
              *(int*)CMCall(arr, GetAt, 1));

    /* SetAt returns the element it replaced. */
    CMLogInfo("SetAt replaced: %d",
              *(int*)CMCall(arr, SetAt, &values[0], 1));

    /* Without a comparator Find() is a linear scan by pointer identity. */
    found = CMCall(arr, Find, &values[2], &idx);
    CMLogInfo("Find(%d) -> %s at index %u", values[2],
              found ? "hit" : "miss", (unsigned)idx);

    /* RemoveAt returns the removed element; the array never frees it here. */
    CMLogInfo("RemoveAt(0) -> %d", *(int*)CMCall(arr, RemoveAt, 0));
    CMLogInfo("final size: %u", (unsigned)CMCall(arr, GetSize));

    CMCall(arr, Destroy);
}

static void sample_sorted_array(void)
{
    CMUTIL_Array *arr;
    uint32_t idx = 0;
    char *popped;

    SAMPLE_SECTION("sorted array that owns its elements");

    /*
     * A free callback transfers ownership of the elements to the array:
     * Destroy() (and Clear()) release them. Elements therefore have to be
     * allocated through the library.
     */
    arr = CMUTIL_ArrayCreateEx(5, compare_cstring, CMUTIL_GetMem()->Free);

    CMCall(arr, Add, CMStrdup("banana"), NULL);
    CMCall(arr, Add, CMStrdup("cherry"), NULL);
    CMCall(arr, Add, CMStrdup("apple"), NULL);
    dump_array("sorted", arr);

    /* Sorted arrays search with bisection. */
    if (CMCall(arr, Find, "cherry", &idx) != NULL)
        CMLogInfo("Find(\"cherry\") -> index %u", (unsigned)idx);

    /* Top/Bottom work on any array; on a sorted one they are max/min. */
    CMLogInfo("Bottom=%s Top=%s",
              (const char*)CMCall(arr, Bottom),
              (const char*)CMCall(arr, Top));

    /*
     * Push, InsertAt and SetAt are refused on a sorted array (they return
     * CMFalse / NULL and log an error): position is decided by the
     * comparator, not by the caller. Add is the only way in.
     */

    /* Pop hands the element back to the caller, ownership included. */
    popped = (char*)CMCall(arr, Pop);
    CMLogInfo("Pop -> %s (now owned by us, must be freed)", popped);
    CMFree(popped);

    /* Remove() looks the element up with the comparator. */
    popped = (char*)CMCall(arr, Remove, "apple");
    CMLogInfo("Remove(\"apple\") -> %s", popped);
    CMFree(popped);

    dump_array("remaining", arr);

    /* Destroy frees "banana" for us. */
    CMCall(arr, Destroy);
}

static void sample_stack(void)
{
    CMUTIL_Array *stack;

    SAMPLE_SECTION("array used as a stack");

    stack = CMUTIL_ArrayCreate();
    CMCall(stack, Push, "first");
    CMCall(stack, Push, "second");
    CMCall(stack, Push, "third");

    while (CMCall(stack, GetSize) > 0)
        CMLogInfo("pop -> %s", (const char*)CMCall(stack, Pop));

    CMCall(stack, Destroy);
}

static void sample_list(void)
{
    CMUTIL_List *list;
    CMUTIL_List *spare;
    CMUTIL_Iterator *iter;

    SAMPLE_SECTION("doubly linked list");

    /* CMUTIL_ListCreateEx(freecb) when the list should own its elements. */
    list = CMUTIL_ListCreate();

    CMCall(list, AddTail, "b");
    CMCall(list, AddTail, "c");
    CMCall(list, AddFront, "a");

    CMLogInfo("size=%u front=%s tail=%s",
              (unsigned)CMCall(list, GetSize),
              (const char*)CMCall(list, GetFront),
              (const char*)CMCall(list, GetTail));

    iter = CMCall(list, Iterator);
    while (CMCall(iter, HasNext))
        CMLogInfo("  item: %s", (const char*)CMCall(iter, Next));
    CMCall(iter, Destroy);

    CMLogInfo("RemoveFront -> %s", (const char*)CMCall(list, RemoveFront));
    CMLogInfo("RemoveTail  -> %s", (const char*)CMCall(list, RemoveTail));

    /* MoveAll splices one list into another, emptying the source. */
    spare = CMUTIL_ListCreate();
    CMCall(spare, AddTail, "x");
    CMCall(spare, AddTail, "y");
    CMCall(list, MoveAll, spare);
    CMLogInfo("after MoveAll: list=%u spare=%u",
              (unsigned)CMCall(list, GetSize),
              (unsigned)CMCall(spare, GetSize));

    CMCall(spare, Destroy);
    CMCall(list, Destroy);
}

int main(void)
{
    sample_init();

    sample_plain_array();
    sample_sorted_array();
    sample_stack();
    sample_list();

    return sample_exit(0);
}
