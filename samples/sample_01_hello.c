/*
 * 01 - Hello, libcmutils
 *
 * The lifecycle every program shares, the CMCall convention and the
 * allocator macros.
 *
 * Shows: CMUTIL_Init, CMUTIL_Clear, CMUTIL_GetLibVersion, CMCall,
 *        CMUTIL_String basics, CMAlloc/CMStrdup/CMFree, CMUTIL_GetMem.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.hello")

int main(void)
{
    CMUTIL_String *str = NULL;
    char *copy = NULL;
    char *buf = NULL;

    /* CMUTIL_Init(CMMemRecycle) happens here. The three strategies are:
     *
     *   CMMemSystem  - plain malloc/free, no bookkeeping (use under valgrind)
     *   CMMemRecycle - pooled blocks, corruption checks, leak report (default)
     *   CMMemDebug   - recycle + a captured call stack per allocation (slow)
     */
    sample_init();

    SAMPLE_SECTION("library version");
    CMLogInfo("libcmutils %s", CMUTIL_GetLibVersion());

    SAMPLE_SECTION("objects are structs of function pointers");
    /*
     * CMCall(obj, Method, ...) expands to (obj)->Method((obj), ...), so the
     * receiver never has to be written twice. Two rules follow from that:
     *
     *   1. Never nest CMCall inside another CMCall's trailing arguments -
     *      the inner call is not macro-expanded and will not compile.
     *   2. Never pass an expression with side effects as the receiver, it
     *      may be evaluated twice.
     *
     * Both relax on a modern enough compiler - see CMUTIL_CALL_NESTED and
     * CMUTIL_CALL_SINGLE_EVAL in libcmutils.h - but portable code observes
     * them anyway.
     */
    str = CMUTIL_StringCreate();
    CMCall(str, AddString, "hello");
    CMCall(str, AddPrint, ", %s! (v%s)", "world", CMUTIL_GetLibVersion());

    CMLogInfo("%s", CMCall(str, GetCString));
    CMLogInfo("length: %u bytes", (unsigned)CMCall(str, GetSize));

    /* Every ownable object is released the same way. */
    CMCall(str, Destroy); str = NULL;

    SAMPLE_SECTION("allocating through the library");
    /*
     * Memory that library objects will own must come from the library's
     * allocators, otherwise CMMemRecycle/CMMemDebug cannot account for it.
     */
    copy = CMStrdup("owned by the caller");
    buf = CMAlloc(64);
    snprintf(buf, 64, "%s (%u bytes)", copy, 64u);
    CMLogInfo("%s", buf);

    CMFree(buf); buf = NULL;
    CMFree(copy); copy = NULL;

    /*
     * CMUTIL_GetMem() returns the active operator table. Its Free member is
     * the callback collections want when they should own their elements:
     *   CMUTIL_ArrayCreateEx(10, NULL, CMUTIL_GetMem()->Free)
     * CMFree is a macro over exactly that member.
     */
    CMLogInfo("allocator table at %p", (void*)CMUTIL_GetMem());

    SAMPLE_SECTION("shutdown");
    CMLogInfo("CMUTIL_Clear() reports whether anything leaked");

    /* Returns 1 if any allocation is still outstanding. */
    return sample_exit(0);
}
