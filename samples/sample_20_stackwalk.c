/*
 * 20 - Call stacks
 *
 * Shows: CMUTIL_StackWalker capturing the current stack as a string array or
 *        printing it into a buffer, and the logging macros that carry a
 *        stack trace.
 *
 * Symbolization depends on the platform and on the build: a stripped release
 * binary yields addresses rather than function names. Building with
 * -DCMAKE_BUILD_TYPE=RelWithDebInfo (or Debug) keeps the symbols.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.stackwalk")

static void print_stack(CMUTIL_StackWalker *walker)
{
    CMUTIL_StringArray *frames;
    CMUTIL_String *buffer;
    uint32_t i;

    /*
     * GetStack(skipdepth) drops that many extra frames from the top of the
     * capture; the walker's own frames are dropped for you. The caller owns
     * the returned array.
     *
     * How much is visible depends on the build: an optimizing compiler
     * inlines small static functions, so the helpers below this one may not
     * appear as separate frames at all.
     */
    frames = CMCall(walker, GetStack, 0);
    CMLogInfo("stack has %u frame(s)", (unsigned)CMCall(frames, GetSize));
    for (i = 0; i < CMCall(frames, GetSize); i++)
        CMLogInfo("  #%u %s", (unsigned)i, CMCall(frames, GetCString, i));
    CMCall(frames, Destroy);

    /* PrintStack writes the same thing into a string. */
    buffer = CMUTIL_StringCreate();
    CMCall(walker, PrintStack, buffer, 0);
    CMLogDebug("PrintStack produced %u bytes",
               (unsigned)CMCall(buffer, GetSize));
    CMCall(buffer, Destroy);
}

static void level_three(CMUTIL_StackWalker *walker)
{
    SAMPLE_SECTION("stack captured three calls deep");
    print_stack(walker);
}

static void level_two(CMUTIL_StackWalker *walker)
{
    level_three(walker);
}

static void level_one(CMUTIL_StackWalker *walker)
{
    level_two(walker);
}

static void failing_operation(void)
{
    SAMPLE_SECTION("logging with a stack trace");

    /*
     * Every level macro has a variant with a trailing S that appends the
     * call stack: CMLogTraceS, CMLogDebugS, CMLogInfoS, CMLogWarnS,
     * CMLogErrorS, CMLogFatalS. The %ex (or %s / %stack) pattern token is
     * where it lands in the layout.
     */
    CMLogErrorS("something went wrong here");
}

int main(void)
{
    CMUTIL_StackWalker *walker;

    sample_init();

    walker = CMUTIL_StackWalkerCreate();

    level_one(walker);
    failing_operation();

    CMCall(walker, Destroy);

    /* The same machinery backs the CMMemDebug allocator, which records a
     * stack for every allocation so a leak can be traced to its origin. */
    return sample_exit(0);
}
