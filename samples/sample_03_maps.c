/*
 * 03 - Hash maps
 *
 * Shows: CMUTIL_Map creation variants, key ownership, case-insensitive keys,
 *        GetKeys / GetPairs / GetAt / RemoveAt, iteration and PrintTo.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.maps")

/* Rendering callback for CMUTIL_Map::PrintTo. */
static const char *value_to_string(void *value)
{
    return value ? (const char*)value : "(null)";
}

static void sample_basic_map(void)
{
    CMUTIL_Map *map;
    void *previous = NULL;

    SAMPLE_SECTION("basic map - keys copied, values borrowed");

    /* CMUTIL_MapCreate() == 256 buckets, case sensitive, no value ownership */
    map = CMUTIL_MapCreate();

    CMCall(map, Put, "host", "127.0.0.1", NULL);
    CMCall(map, Put, "port", "9999", NULL);

    /* The fourth argument receives the previous value for that key. */
    CMCall(map, Put, "port", "8080", &previous);
    CMLogInfo("port replaced %s with %s",
              (const char*)previous, (const char*)CMCall(map, Get, "port"));

    CMLogInfo("size=%u host=%s",
              (unsigned)CMCall(map, GetSize),
              (const char*)CMCall(map, Get, "host"));

    /* Remove returns the value; with no free callback it is the caller's. */
    CMLogInfo("Remove(\"host\") -> %s", (const char*)CMCall(map, Remove, "host"));
    CMLogInfo("Get(\"host\") after removal -> %p", CMCall(map, Get, "host"));

    CMCall(map, Destroy);
}

static void sample_owning_map(void)
{
    CMUTIL_Map *env;
    CMUTIL_String *dump;
    const char *path;

    SAMPLE_SECTION("case-insensitive map that owns its values");

    /*
     * CMUTIL_MapCreateEx(bucketsize, case_insensitive, freecb, load_factor)
     * The free callback makes the map own the values, so they must come from
     * the library allocator.
     */
    env = CMUTIL_MapCreateEx(CMUTIL_MAP_DEFAULT, CMTrue, CMFree, 0.75f);

    path = getenv("PATH");
    CMCall(env, Put, "PATH", CMStrdup(path ? path : ""), NULL);
    CMCall(env, Put, "Lang", CMStrdup("C"), NULL);

    /* Case insensitive lookup: "lang", "LANG" and "Lang" are the same key. */
    CMLogInfo("Get(\"lang\") -> %s", (const char*)CMCall(env, Get, "lang"));
    CMLogInfo("Get(\"LANG\") -> %s", (const char*)CMCall(env, Get, "LANG"));

    dump = CMUTIL_StringCreate();
    CMCall(env, PrintTo, dump, value_to_string);
    CMLogInfo("keys: %s", CMCall(dump, GetCString));
    CMCall(dump, Destroy);

    /* Destroy frees every strdup'ed value. */
    CMCall(env, Destroy);
}

static void sample_ordered_access(void)
{
    CMUTIL_Map *map;
    CMUTIL_StringArray *keys;
    const CMUTIL_Array *pairs;
    CMUTIL_Iterator *iter;
    uint32_t i;

    SAMPLE_SECTION("insertion order is preserved");

    map = CMUTIL_MapCreate();
    CMCall(map, Put, "one", "1", NULL);
    CMCall(map, Put, "two", "2", NULL);
    CMCall(map, Put, "three", "3", NULL);

    /* GetKeys hands over a StringArray the caller destroys. */
    keys = CMCall(map, GetKeys);
    for (i = 0; i < CMCall(keys, GetSize); i++)
        CMLogInfo("key[%u] = %s", (unsigned)i, CMCall(keys, GetCString, i));
    CMCall(keys, Destroy);

    /* GetPairs returns the map's own array of CMUTIL_MapPair - do not free. */
    pairs = CMCall(map, GetPairs);
    for (i = 0; i < CMCall(pairs, GetSize); i++) {
        const CMUTIL_MapPair *pair = CMCall(pairs, GetAt, i);
        CMLogInfo("pair[%u] = %s -> %s", (unsigned)i,
                  CMCall(pair, GetKey), (const char*)CMCall(pair, GetValue));
    }

    /* The iterator walks values in insertion order. */
    iter = CMCall(map, Iterator);
    while (CMCall(iter, HasNext))
        CMLogInfo("value: %s", (const char*)CMCall(iter, Next));
    CMCall(iter, Destroy);

    /* Entries are also addressable by insertion index. */
    CMLogInfo("GetAt(1) -> %s", (const char*)CMCall(map, GetAt, 1));
    CMLogInfo("RemoveAt(0) -> %s", (const char*)CMCall(map, RemoveAt, 0));
    CMLogInfo("size after RemoveAt: %u", (unsigned)CMCall(map, GetSize));

    CMCall(map, Destroy);
}

int main(void)
{
    sample_init();

    sample_basic_map();
    sample_owning_map();
    sample_ordered_access();

    return sample_exit(0);
}
