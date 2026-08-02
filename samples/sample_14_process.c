/*
 * 14 - Subprocesses
 *
 * Shows: CMUTIL_ProcessCreate with an explicit working directory, environment
 *        and argument list, attaching to the child's standard streams, Wait,
 *        Kill and PipeTo.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.process")

/* The environment handed to the child. Nothing is inherited implicitly, so
 * PATH has to be provided for the executable lookup to work. */
static CMUTIL_Map *build_env(void)
{
    CMUTIL_Map *env = CMUTIL_MapCreateEx(
            CMUTIL_MAP_DEFAULT, CMFalse, CMFree, 0.75f);
    const char *path = getenv("PATH");

    CMCall(env, Put, "PATH", CMStrdup(path ? path : "/usr/bin:/bin"), NULL);
    CMCall(env, Put, "SAMPLE_VAR", CMStrdup("hello from the parent"), NULL);
    return env;
}

static void sample_run_and_wait(CMUTIL_Map *env)
{
    CMUTIL_Process *proc;
    int exit_code;

    SAMPLE_SECTION("running a command and waiting for it");

    /*
     * CMUTIL_ProcessCreate(cwd, env, command, ...) NULL-terminates the
     * argument list for you; CMUTIL_ProcessCreateEx wants the NULL spelled
     * out. CMProcStreamNone leaves the child's stdout and stderr attached to
     * ours.
     */
#if defined(MSWIN)
    proc = CMUTIL_ProcessCreate(".", env, "cmd", "/c", "echo child speaking");
#else
    proc = CMUTIL_ProcessCreate(".", env, "/bin/echo", "child speaking");
#endif
    if (proc == NULL) {
        CMLogError("could not create the process");
        return;
    }

    if (CMCall(proc, Start, CMProcStreamNone) == CMFalse) {
        CMLogError("could not start the process");
        CMCall(proc, Destroy);
        return;
    }

    CMLogInfo("started pid %ld running '%s' in '%s'",
              (long)CMCall(proc, GetPid),
              CMCall(proc, GetCommand),
              CMCall(proc, GetWorkDir));

    /* Wait blocks up to the timeout in milliseconds; -1 waits forever. */
    exit_code = CMCall(proc, Wait, 10000);
    CMLogInfo("exited with %d", exit_code);

    CMCall(proc, Destroy);
}

static void sample_capture_output(CMUTIL_Map *env)
{
    CMUTIL_Process *proc;
    CMUTIL_ByteBuffer *out;
    ssize_t nread;

    SAMPLE_SECTION("capturing the child's stdout");

    /*
     * CMProcStreamRead enables Read(); the other flags are
     * CMProcStreamWrite (Write), CMProcStreamReadWrite and
     * CMProcStreamReadErr (ReadErr), and they can be combined.
     */
#if defined(MSWIN)
    proc = CMUTIL_ProcessCreate(".", env, "cmd", "/c", "echo %SAMPLE_VAR%");
#else
    proc = CMUTIL_ProcessCreate(
            ".", env, "/bin/sh", "-c", "echo \"$SAMPLE_VAR\"");
#endif
    if (proc == NULL || CMCall(proc, Start, CMProcStreamRead) == CMFalse) {
        CMLogError("could not start the process");
        if (proc) CMCall(proc, Destroy);
        return;
    }

    out = CMUTIL_ByteBufferCreateEx(1024);
    while ((nread = CMCall(proc, Read, out, 256)) > 0)
        CMLogDebug("read %d bytes", (int)nread);

    {
        uint8_t *bytes = CMCall(out, GetBytes);
        uint32_t len = (uint32_t)CMCall(out, GetSize);
        CMLogInfo("child printed: %.*s", (int)len, (const char*)bytes);
    }

    CMLogInfo("exit code %d", CMCall(proc, Wait, 10000));

    CMCall(out, Destroy);
    CMCall(proc, Destroy);
}

static void sample_kill(CMUTIL_Map *env)
{
    CMUTIL_Process *proc;

    SAMPLE_SECTION("killing a process that outlives its welcome");

#if defined(MSWIN)
    proc = CMUTIL_ProcessCreate(
            ".", env, "cmd", "/c", "ping", "-n", "30", "127.0.0.1");
#else
    proc = CMUTIL_ProcessCreate(".", env, "/bin/sh", "-c", "sleep 30");
#endif
    if (proc == NULL || CMCall(proc, Start, CMProcStreamNone) == CMFalse) {
        CMLogError("could not start the process");
        if (proc) CMCall(proc, Destroy);
        return;
    }

    CMLogInfo("started pid %ld, waiting 200 ms before killing it",
              (long)CMCall(proc, GetPid));
    usleep(200 * 1000);

    /*
     * Suspend/Resume are also available for stop-and-go control.
     *
     * Kill terminates the child and reaps it, so it is a complete
     * replacement for Wait - calling Wait afterwards finds nothing left to
     * wait for and reports an error.
     */
    CMCall(proc, Kill);
    CMLogInfo("killed");

    CMCall(proc, Destroy);
}

int main(void)
{
    CMUTIL_Map *env;

    sample_init();

    env = build_env();

    sample_run_and_wait(env);
    sample_capture_output(env);
    sample_kill(env);

    CMCall(env, Destroy);

    return sample_exit(0);
}
