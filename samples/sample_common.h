/*
 * libcmutils samples - shared helpers.
 *
 * Every sample follows the same shape:
 *
 *     CMUTIL_LogDefine("sample.xxx")     // one logger name per source file
 *
 *     int main(void) {
 *         int rc = 0;
 *         sample_init(CMLogLevel_Debug); // CMUTIL_Init() + console logging
 *         ...
 *         return sample_exit(rc);        // CMUTIL_Clear() + leak report
 *     }
 *
 * sample_exit() returns non-zero when CMUTIL_Clear() reports that something
 * allocated through the library was never released, so running a sample is
 * also a leak check.
 */

#ifndef CMUTIL_SAMPLE_COMMON_H
#define CMUTIL_SAMPLE_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libcmutils.h"

/*
 * Directory holding the data files shipped with the samples
 * (samples/conf). Defined by the build; the fallback keeps the sources
 * usable when they are compiled by hand.
 */
#ifndef SAMPLE_DATA_DIR
# define SAMPLE_DATA_DIR "conf"
#endif

/** Build a path to a file shipped in samples/conf. */
#define SAMPLE_DATA(fname) SAMPLE_DATA_DIR "/" fname

/** Print a visible section header in the sample output. */
#define SAMPLE_SECTION(title) \
    CMLogInfo("======== " title " ========")

/**
 * Initialize the library with console logging.
 *
 * The log system configures itself the first time anything is logged, from
 * the file named by the CMUTIL_LOG_CONFIG environment variable or from
 * cmutil_log.jsonc in the working directory. Pointing the variable at the
 * configuration shipped with the samples makes every sample log the same
 * way no matter which directory it is started from.
 *
 * sample_08_logging.c shows the same thing done programmatically.
 */
static void sample_init(void)
{
    const char *config = SAMPLE_DATA("sample_console_log.jsonc");

#if defined(MSWIN)
    _putenv_s("CMUTIL_LOG_CONFIG", config);
#else
    setenv("CMUTIL_LOG_CONFIG", config, 1);
#endif

    /* Must be the first library call in the process. */
    CMUTIL_Init(CMMemRecycle);
}

/**
 * Release every library resource and report leaks.
 *
 * @param result the exit code the sample wants to return.
 * @return @a result, or 1 when an allocation was leaked.
 */
static int sample_exit(int result)
{
    if (CMUTIL_Clear() != CMTrue) {
        fprintf(stderr, "*** leak detected: some object was never destroyed\n");
        return 1;
    }
    return result;
}

#endif /* CMUTIL_SAMPLE_COMMON_H */
