/*
 * 13 - Files and directories
 *
 * Shows: CMUTIL_File inspection, CMUTIL_PathCreate, CMUTIL_FileStream
 *        reading and writing, directory listing with Children and glob
 *        search with Find.
 *
 * Everything happens under a scratch directory in the working directory,
 * which the sample removes again at the end.
 */

#include "sample_common.h"

#if defined(_MSC_VER)
# include <direct.h>
# define rmdir _rmdir
#endif

CMUTIL_LogDefine("sample.files")

#define WORKDIR "sample_files"

static void write_file(const char *path, const char *text)
{
    CMUTIL_File *file = CMUTIL_FileCreate(path);
    CMUTIL_FileStream *stream = CMCall(file, CreateStream, CMFileOpenWrite);
    CMUTIL_String *buffer = CMUTIL_StringCreateEx(0, text);
    size_t size = CMCall(buffer, GetSize);

    /* Write(buffer, offset, size) - the whole buffer here. */
    CMCall(stream, Write, buffer, 0, size);
    CMCall(stream, Close);

    CMCall(buffer, Destroy);
    CMCall(file, Destroy);
}

static void sample_write_and_read(void)
{
    CMUTIL_File *file;
    CMUTIL_FileStream *stream;
    CMUTIL_String *buffer;
    ssize_t nread;

    SAMPLE_SECTION("writing and reading a file");

    /* Creates the whole directory tree; the mode is used on POSIX. */
    CMUTIL_PathCreate(WORKDIR "/nested", 0755);

    write_file(WORKDIR "/notes.txt", "first line\nsecond line\n");
    write_file(WORKDIR "/data.csv", "a,b,c\n1,2,3\n");
    write_file(WORKDIR "/nested/deep.txt", "buried\n");

    file = CMUTIL_FileCreate(WORKDIR "/notes.txt");
    CMLogInfo("name        : %s", CMCall(file, GetName));
    CMLogInfo("full path   : %s", CMCall(file, GetFullPath));
    CMLogInfo("exists      : %s", CMCall(file, IsExists) ? "yes" : "no");
    CMLogInfo("is file     : %s", CMCall(file, IsFile) ? "yes" : "no");
    CMLogInfo("is directory: %s", CMCall(file, IsDirectory) ? "yes" : "no");
    CMLogInfo("length      : %ld bytes", CMCall(file, Length));
    CMLogInfo("modified    : %ld (unix time)",
              (long)CMCall(file, ModifiedTime));

    /* Whole-file read. */
    buffer = CMCall(file, GetContents);
    CMLogInfo("GetContents :\n%s", CMCall(buffer, GetCString));
    CMCall(buffer, Destroy);

    /* Streamed read: Read appends to the string and returns the byte count,
     * 0 at end of file. */
    stream = CMCall(file, CreateStream, CMFileOpenRead);
    buffer = CMUTIL_StringCreate();
    while ((nread = CMCall(stream, Read, buffer, 8)) > 0)
        CMLogDebug("read %d bytes, %u buffered",
                   (int)nread, (unsigned)CMCall(buffer, GetSize));
    CMLogInfo("streamed    : %u bytes total",
              (unsigned)CMCall(buffer, GetSize));
    CMCall(buffer, Destroy);
    CMCall(stream, Close);

    /* Appending. */
    stream = CMCall(file, CreateStream, CMFileOpenAppend);
    buffer = CMUTIL_StringCreateEx(0, "appended line\n");
    {
        size_t size = CMCall(buffer, GetSize);
        CMCall(stream, Write, buffer, 0, size);
    }
    CMCall(buffer, Destroy);
    CMCall(stream, Close);
    CMLogInfo("after append: %ld bytes", CMCall(file, Length));

    CMCall(file, Destroy);
}

static void sample_listing(void)
{
    CMUTIL_File *dir;
    CMUTIL_FileList *children;
    CMUTIL_FileList *found;
    size_t i;

    SAMPLE_SECTION("listing and searching");

    dir = CMUTIL_FileCreate(WORKDIR);

    children = CMCall(dir, Children);
    CMLogInfo("%s has %u entries",
              WORKDIR, (unsigned)CMCall(children, Count));
    for (i = 0; i < CMCall(children, Count); i++) {
        CMUTIL_File *child = CMCall(children, GetAt, (uint32_t)i);
        CMLogInfo("  %-12s %s",
                  CMCall(child, GetName),
                  CMCall(child, IsDirectory) ? "(directory)" : "(file)");
    }
    CMCall(children, Destroy);

    /*
     * Find takes a glob pattern: ? any character except a separator,
     * * any run of them, [abc] and [a-z] sets and ranges, [!a-z] negated,
     * a leading ! to negate a whole segment. Matching is case insensitive.
     * The last argument turns on recursion.
     */
    found = CMCall(dir, Find, "*.txt", CMTrue);
    CMLogInfo("recursive search for *.txt found %u file(s)",
              (unsigned)CMCall(found, Count));
    for (i = 0; i < CMCall(found, Count); i++) {
        CMUTIL_File *hit = CMCall(found, GetAt, (uint32_t)i);
        CMLogInfo("  %s", CMCall(hit, GetFullPath));
    }
    CMCall(found, Destroy);

    CMCall(dir, Destroy);
}

static void cleanup(void)
{
    static const char *paths[] = {
        WORKDIR "/nested/deep.txt",
        WORKDIR "/notes.txt",
        WORKDIR "/data.csv"
    };
    size_t i;

    SAMPLE_SECTION("cleaning up");

    /*
     * Delete unlinks a file. It is not a recursive remove and it does not
     * drop directories - CMUTIL_PathCreate has no counterpart, so the
     * scratch directories are removed with the platform call.
     */
    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        CMUTIL_File *file = CMUTIL_FileCreate(paths[i]);
        CMBool ok = CMCall(file, Delete);
        CMLogDebug("delete %-28s %s", paths[i], ok ? "ok" : "failed");
        CMCall(file, Destroy);
    }

    rmdir(WORKDIR "/nested");
    rmdir(WORKDIR);
    CMLogInfo("scratch directory removed");
}

int main(void)
{
    sample_init();

    sample_write_and_read();
    sample_listing();
    cleanup();

    return sample_exit(0);
}
