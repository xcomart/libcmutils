/*
 * 15 - Dynamic libraries
 *
 * Shows: CMUTIL_LibraryCreate and GetProcedure - one API over dlopen and
 *        LoadLibrary.
 *
 * The library it loads is sample_plugin, built next to this executable. The
 * path comes from the build (SAMPLE_PLUGIN_PATH) and can be overridden with
 * the first command line argument. The file extension may be omitted: the
 * loader appends the platform one (.so, .dylib, .dll) when it is missing.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.library")

#ifndef SAMPLE_PLUGIN_PATH
# define SAMPLE_PLUGIN_PATH "sample_plugin"
#endif

typedef const char *(*name_fn)(void);
typedef int (*add_fn)(int, int);
typedef int (*greet_fn)(char*, int, const char*);

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : SAMPLE_PLUGIN_PATH;
    CMUTIL_Library *lib;
    name_fn get_name;
    add_fn add;
    greet_fn greet;
    char buffer[128];

    sample_init();

    SAMPLE_SECTION("loading a shared library");
    CMLogInfo("loading %s", path);

    lib = CMUTIL_LibraryCreate(path);
    if (lib == NULL) {
        CMLogError("load failed - pass the library path as the first argument");
        return sample_exit(1);
    }

    SAMPLE_SECTION("resolving symbols");

    /*
     * GetProcedure returns a void*, so the cast to a function pointer is on
     * you. Resolved symbols are cached inside the library object.
     */
    get_name = (name_fn)CMCall(lib, GetProcedure, "sample_plugin_name");
    add = (add_fn)CMCall(lib, GetProcedure, "sample_plugin_add");
    greet = (greet_fn)CMCall(lib, GetProcedure, "sample_plugin_greet");

    if (get_name == NULL || add == NULL || greet == NULL) {
        CMLogError("a symbol could not be resolved");
        CMCall(lib, Destroy);
        return sample_exit(1);
    }

    CMLogInfo("sample_plugin_name() -> %s", get_name());
    CMLogInfo("sample_plugin_add(20, 22) -> %d", add(20, 22));

    greet(buffer, (int)sizeof(buffer), "libcmutils");
    CMLogInfo("sample_plugin_greet() -> %s", buffer);

    /* A missing symbol is reported as NULL, not as a crash - the library
     * logs an error with a stack trace on the way out. */
    if (CMCall(lib, GetProcedure, "no_such_symbol") == NULL)
        CMLogInfo("an unknown symbol resolves to NULL");

    /* Destroy unloads the library; the function pointers die with it. */
    CMCall(lib, Destroy);

    return sample_exit(0);
}
