/*
 * A minimal shared library, loaded at run time by sample_15_library.c.
 *
 * It deliberately does not link against libcmutils: a plugin only has to
 * export C symbols that the host can resolve by name.
 */

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
# define PLUGIN_API __declspec(dllexport)
#else
# define PLUGIN_API
#endif

PLUGIN_API const char *sample_plugin_name(void)
{
    return "sample_plugin";
}

PLUGIN_API int sample_plugin_add(int a, int b)
{
    return a + b;
}

PLUGIN_API int sample_plugin_greet(char *buffer, int size, const char *who)
{
    return snprintf(buffer, (size_t)size, "hello, %s, from the plugin", who);
}
