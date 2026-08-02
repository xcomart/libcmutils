/*
 * 08 - Logging
 *
 * Shows: the level macros, guarding expensive messages, configuring a log
 *        system in code (console / file / rolling file appenders), named
 *        loggers with additivity, and loading the same structure from a
 *        .jsonc file.
 *
 * A logger name is declared once per source file with CMUTIL_LogDefine.
 * Names are hierarchical: "sample.logging.detail" is matched by a logger
 * configured as "sample.logging", which is matched by "sample", which falls
 * back to the root logger.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.logging")

#define PATTERN "%d{%F %T.%Q} %P-[%-10t] (%-15F:%04L) [%-5p] %c : %m%n%ex"

static void log_every_level(void)
{
    CMLogTrace("trace message");
    CMLogDebug("debug message, value=%d", 42);
    CMLogInfo("info message");
    CMLogWarn("warn message");
    CMLogError("error message");
    /* Fatal is not logged here so the sample output stays readable. */

    /* CMLog takes the level as a value instead of baking it into the name. */
    CMLog(CMLogLevel_Info, "level passed as a value");

    /* The trailing S appends the call stack to the message. */
    CMLogInfoS("this one carries a stack trace");
}

static void sample_code_configuration(void)
{
    CMUTIL_LogSystem *lsys;
    CMUTIL_ConfLogger *root;
    CMUTIL_ConfLogger *quiet;
    CMUTIL_LogAppender *console;
    CMUTIL_LogAppender *file;
    CMUTIL_LogAppender *rolling;

    SAMPLE_SECTION("configuring the log system in code");

    lsys = CMUTIL_LogSystemCreate();

    /* NULL name -> the root logger. The last argument is additivity. */
    root = CMCall(lsys, CreateLogger, NULL, CMLogLevel_Trace, CMTrue);

    /* Console appender; the last argument selects stderr over stdout. */
    console = CMUTIL_LogConsoleAppenderCreate("Console", PATTERN, CMFalse);
    /* Buffered and non-blocking: the writing thread never waits on I/O. */
    CMCall(console, SetAsync, 256);
    CMCall(root, AddAppender, console, CMLogLevel_Trace);

    file = CMUTIL_LogFileAppenderCreate("File", "sample_app.log", PATTERN);
    CMCall(root, AddAppender, file, CMLogLevel_Info);

    /* Rolls every minute here so the effect is visible in a short run;
     * the roll path is a strftime pattern. */
    rolling = CMUTIL_LogRollingFileAppenderCreate(
            "Rolling", "sample_rolling.log", CMLogTerm_Minute,
            "log/sample_rolling.log.%Y%m%d_%H%M", PATTERN);
    CMCall(root, AddAppender, rolling, CMLogLevel_Info);

    /*
     * A named logger matched by dotted prefix: "sample.logging.quiet" and
     * anything below it lands here. The closest matching logger decides the
     * level, so these names emit from Error up even though the root logger
     * accepts Trace. additivity=CMFalse additionally stops the messages
     * from reaching the root logger's appenders.
     */
    quiet = CMCall(lsys, CreateLogger,
                   "sample.logging.quiet", CMLogLevel_Error, CMFalse);
    CMCall(quiet, AddAppender, console, CMLogLevel_Error);

    /*
     * Installs the log system globally and destroys the previous one, so
     * the console logger set up by sample_init() goes away here.
     */
    CMUTIL_LogSystemSet(lsys);

    log_every_level();

    /* Guard message construction that is expensive to build. */
    if (CMLogIsEnabled(CMLogLevel_Debug)) {
        CMUTIL_String *report = CMUTIL_StringCreate();
        int i;
        for (i = 0; i < 5; i++)
            CMCall(report, AddPrint, "[item %d]", i);
        CMLogDebug("expensive report: %s", CMCall(report, GetCString));
        CMCall(report, Destroy);
    }

    CMLogInfo("wrote sample_app.log and sample_rolling.log");
}

static void sample_file_configuration(void)
{
    const char *path = SAMPLE_DATA("sample_log.jsonc");

    SAMPLE_SECTION("configuring the log system from a .jsonc file");

    /*
     * The same structure as above in declarative form. Reading it explicitly
     * is one option; the other is to do nothing at all - the first log call
     * configures the system from the file named by the CMUTIL_LOG_CONFIG
     * environment variable, or from cmutil_log.jsonc in the working
     * directory, which is what sample_init() relies on.
     *
     * Two things to know about CMUTIL_LogSystemConfigureFomJson:
     *
     *   - it installs the system it builds as the global one itself, so the
     *     result must NOT be handed to CMUTIL_LogSystemSet afterwards - that
     *     would destroy the system that is already installed and leave a
     *     dangling pointer behind;
     *   - it does not release a system that is already installed, so drop
     *     the current one with CMUTIL_LogSystemSet(NULL) first.
     *
     * On failure it falls back to a built-in console configuration rather
     * than returning NULL, so logging never breaks an unconfigured program.
     * An appender without a "type" is such a failure - for a logger the key
     * is optional and only marks the "root" one.
     */
    CMUTIL_LogSystemSet(NULL);
    (void)CMUTIL_LogSystemConfigureFomJson(path);

    CMLogInfo("now logging with the configuration from %s", path);
    log_every_level();
}

int main(void)
{
    sample_init();

    sample_code_configuration();
    sample_file_configuration();

    /* CMUTIL_Clear() destroys the installed log system for us. */
    return sample_exit(0);
}
