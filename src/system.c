/*
MIT License

Copyright (c) 2020 Dennis Soungjin Park<xcomart@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#include "functions.h"

#include <sys/stat.h>
#include <errno.h>
#if defined(APPLE) || defined(BSD)
#include <sys/syslimits.h>
#endif

#if !defined(MSWIN)
# include <dirent.h>
#else
# include <direct.h>
#endif

#if defined(LINUX)
# define CMUTIL_MODTIME(a)  a.st_mtim.tv_sec
#else
# define CMUTIL_MODTIME(a)  a.st_mtime
#endif


CMUTIL_LogDefine("cmutil.system")


#if defined(_MSC_VER)
const int64_t DELTA_EPOCH_IN_MICROSECS = 11644473600000000;

/* IN UNIX the use of the timezone struct is obsolete;
I don't know why you use it.
See http://linux.about.com/od/commands/l/blcmdl2_gettime.htm
But if you want to use this structure to know about GMT(UTC) difference from
your local time it will be next: tz_minuteswest is the real difference in minutes from
GMT(UTC) and a tz_dsttime is a flag indicates whether daylight is now in use
*/

int __gettimeofday(struct timeval *tv/*in*/, void *tz/*in*/)
{
    FILETIME ft;
    int64_t tmpres = 0;
    TIME_ZONE_INFORMATION tz_winapi;
    int rez = 0;

    ZeroMemory(&ft, sizeof(ft));
    ZeroMemory(&tz_winapi, sizeof(tz_winapi));

    GetSystemTimeAsFileTime(&ft);

    tmpres = ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    /*converting file time to unix epoch*/
    tmpres /= 10;  /*convert into microseconds*/
    tmpres -= DELTA_EPOCH_IN_MICROSECS;
    tv->tv_sec = (int32_t)(tmpres*0.000001);
    tv->tv_usec = (tmpres % 1000000);

    CMUTIL_UNUSED(tz);
    //_tzset(),don't work properly, so we use GetTimeZoneInformation
    //rez = GetTimeZoneInformation(&tz_winapi);
    //tz->tz_dsttime = (rez == 2) ? true : false;
    //tz->tz_minuteswest = tz_winapi.Bias +
    //        ((rez == 2) ? tz_winapi.DaylightBias : 0);

    return 0;
}
#endif

typedef struct CMUTIL_Library_Internal {
    CMUTIL_Library      base;
    void                *library;
    CMUTIL_Mutex        *mutex;
    CMUTIL_Map          *procs;
    CMUTIL_Mem          *memst;
} CMUTIL_Library_Internal;

CMUTIL_STATIC void *CMUTIL_LibraryGetProcedure(
        const CMUTIL_Library *lib, const char *proc_name)
{
    const CMUTIL_Library_Internal *ilib = (const CMUTIL_Library_Internal*)lib;
    void *proc = NULL;
    CMCall(ilib->mutex, Lock);
    proc = CMCall(ilib->procs, Get, proc_name);
    if (!proc) {
#if defined(MSWIN)
        proc = GetProcAddress((HMODULE)ilib->library, proc_name);
#else
        proc = dlsym(ilib->library, proc_name);
#endif
        if (proc)
            CMCall(ilib->procs, Put, proc_name, proc, NULL);
    }
    CMCall(ilib->mutex, Unlock);
    if (proc == NULL) {
        CMLogErrorS("Failed to get procedure %s, %d:%s",
            proc_name, errno, strerror(errno));
        return NULL;
    }
    return proc;
}

CMUTIL_STATIC void CMUTIL_LibraryDestroy(
        CMUTIL_Library *lib)
{
    CMUTIL_Library_Internal *ilib = (CMUTIL_Library_Internal*)lib;
    if (ilib) {
        if (ilib->library) {
#if defined(MSWIN)
            FreeLibrary((HMODULE)ilib->library);
#else
            dlclose(ilib->library);
#endif
        }
        if (ilib->procs)
            CMCall(ilib->procs, Destroy);
        if (ilib->mutex)
            CMCall(ilib->mutex, Destroy);
        ilib->memst->Free(ilib);
    }
}

static CMUTIL_Library g_cmutil_library = {
        CMUTIL_LibraryGetProcedure,
        CMUTIL_LibraryDestroy
};

CMUTIL_Library *CMUTIL_LibraryCreateInternal(
        CMUTIL_Mem *memst, const char *path)
{
    char slib[1024];
    CMUTIL_Library_Internal *res =
            memst->Alloc(sizeof(CMUTIL_Library_Internal));
    const char *ext;

    memset(res, 0x0, sizeof(CMUTIL_Library_Internal));
    memcpy(res, &g_cmutil_library, sizeof(CMUTIL_Library));

    ext = strrchr(path, '.');
    if (ext && strcasecmp(ext + 1, CMUTIL_SO_EXT) == 0)
        sprintf(slib, "%s", path);
    else
        sprintf(slib, "%s.%s", path, CMUTIL_SO_EXT);

    res->memst = memst;

#if defined(MSWIN)
    res->library = LoadLibrary(slib);
#else
    res->library = dlopen(slib, RTLD_NOW);
#endif
    res->mutex = CMUTIL_MutexCreateInternal(memst);
    res->procs = CMUTIL_MapCreateInternal(
        memst, 10, CMFalse, NULL, 0.75f);

    return (CMUTIL_Library*)res;
}

CMUTIL_Library *CMUTIL_LibraryCreate(const char *path)
{
    return CMUTIL_LibraryCreateInternal(CMUTIL_GetMem(), path);
}


typedef struct CMUTIL_FileList_Internal {
    CMUTIL_FileList     base;
    CMUTIL_Array        *files;
    CMUTIL_Mem          *memst;
} CMUTIL_FileList_Internal;

typedef struct CMUTIL_File_Internal {
    CMUTIL_File         base;
    char                *path;
    char                *name;
    CMUTIL_Mem          *memst;
    CMBool              isref;
    int                 dummy_padder;
} CMUTIL_File_Internal;

typedef struct CMUTIL_FileStream_Internal {
    CMUTIL_FileStream   base;
    FILE                *fp;
    CMFileOpenMode      mode;
    CMUTIL_Mem          *memst;
} CMUTIL_FileStream_Internal;

CMUTIL_STATIC size_t CMUTIL_FileListCount(const CMUTIL_FileList *flist)
{
    const CMUTIL_FileList_Internal *iflist =
            (const CMUTIL_FileList_Internal*)flist;
    return CMCall(iflist->files, GetSize);
}

CMUTIL_STATIC CMUTIL_File *CMUTIL_FileListGetAt(
        const CMUTIL_FileList *flist, uint32_t index)
{
    const CMUTIL_FileList_Internal *iflist =
            (const CMUTIL_FileList_Internal*)flist;
    return (CMUTIL_File*)CMCall(iflist->files, GetAt, index);
}

CMUTIL_STATIC void CMUTIL_FileListDestroy(CMUTIL_FileList *flist)
{
    CMUTIL_FileList_Internal *iflist = (CMUTIL_FileList_Internal*)flist;
    if (iflist) {
        if (iflist->files) {
            uint32_t i;
            size_t count = CMCall(iflist->files, GetSize);
            for (i=0; i<count; i++) {
                CMUTIL_File_Internal *file =
                        (CMUTIL_File_Internal*)CMCall(flist, GetAt, i);
                file->isref = CMFalse;
                CMCall((CMUTIL_File*)file, Destroy);
            }
            CMCall(iflist->files, Destroy);
        }
        iflist->memst->Free(iflist);
    }
}

CMUTIL_STATIC ssize_t CMUTIL_FileStreamRead(
    const CMUTIL_FileStream *stream, CMUTIL_String *buffer, size_t size)
{
    const CMUTIL_FileStream_Internal *is =
            (CMUTIL_FileStream_Internal*)stream;
    if (is->mode == CMFileOpenRead) {
        char readbuf[1024];
        size_t totalread = 0;
        while (totalread < size) {
            size_t toread = size - totalread;
            size_t rdsz;
            if (toread > 1024)
                toread = 1024;
            rdsz = fread(readbuf, 1, toread, is->fp);
            if (rdsz > 0) {
                CMCall(buffer, AddNString, readbuf, rdsz);
                totalread += rdsz;
            } else {
                if (feof(is->fp))
                    break;
                return -1;
            }
        }
        return (ssize_t)totalread;
    }
    CMLogErrorS("File not opened in read mode");
    return -1;
}

CMUTIL_STATIC ssize_t CMUTIL_FileStreamWrite(
    const CMUTIL_FileStream *stream,
    const CMUTIL_String *buffer,
    size_t offset,
    size_t size)
{
    CMUTIL_FileStream_Internal *is =
            (CMUTIL_FileStream_Internal*)stream;
    if (is->mode == CMFileOpenRead)
        CMLogErrorS("File not opened in write mode");
    else if (CMCall(buffer, GetSize) >= offset + size) {
        size_t totalwrite = 0;
        const char *bufptr = CMCall(buffer, GetCString);
        while (totalwrite < size) {
            size_t towrite = size - totalwrite;
            size_t wrsz;
            if (towrite > 1024)
                towrite = 1024;
            wrsz = fwrite(bufptr + totalwrite, 1, towrite, is->fp);
            if (wrsz > 0) {
                totalwrite += wrsz;
            } else {
                return -1;
            }
        }
        return (ssize_t)totalwrite;
    } else {
        CMLogErrorS("Buffer size is smaller than write size");
    }
    return -1;
}

CMUTIL_STATIC void CMUTIL_FileStreamClose(
    CMUTIL_FileStream *stream)
{
    CMUTIL_FileStream_Internal *istream =
            (CMUTIL_FileStream_Internal*)stream;
    if (istream) {
        if (istream->fp)
            fclose(istream->fp);
        istream->memst->Free(istream);
    }
}

static CMUTIL_FileStream g_cmutil_filestream = {
    CMUTIL_FileStreamRead,      // Read
    CMUTIL_FileStreamWrite,     // Write
    CMUTIL_FileStreamClose      // Close
};

static CMUTIL_FileList g_cmutil_filelist = {
        CMUTIL_FileListCount,
        CMUTIL_FileListGetAt,
        CMUTIL_FileListDestroy
};

CMUTIL_STATIC CMUTIL_FileList_Internal *CMUTIL_FileListCreate(
        CMUTIL_Mem *memst)
{
    CMUTIL_FileList_Internal *res =
            memst->Alloc(sizeof(CMUTIL_FileList_Internal));
    memset(res, 0x0, sizeof(CMUTIL_FileList_Internal));
    memcpy(res, &g_cmutil_filelist, sizeof(CMUTIL_FileList));
    res->memst = memst;
    res->files = CMUTIL_ArrayCreateInternal(memst, 10, NULL, NULL, CMFalse);
    return res;
}


CMUTIL_STATIC CMUTIL_String *CMUTIL_FileGetContents(const CMUTIL_File *file)
{
    const CMUTIL_File_Internal *ifile = (const CMUTIL_File_Internal*)file;
    FILE *f = fopen(CMCall(file, GetFullPath), "rb");
    if (f) {
        char buffer[1024];
        size_t rdsz = 0;
        CMUTIL_String *cont =
                CMUTIL_StringCreateInternal(ifile->memst, 1024, NULL);
        while ((rdsz = fread(buffer, 1, 1024, f)) > 0)
            CMCall(cont, AddNString, buffer, rdsz);
        fclose(f);
        return cont;
    }
    return NULL;
}

CMUTIL_STATIC CMBool CMUTIL_FileDelete(const CMUTIL_File *file)
{
    return DeleteFile(CMCall(file, GetFullPath)) == 0 ?
            CMTrue:CMFalse;
}

CMUTIL_STATIC CMBool CMUTIL_FileIsFile(const CMUTIL_File *file)
{
    struct stat s;
    if ( stat(CMCall(file, GetFullPath), &s) == 0 ) {
        return s.st_mode & S_IFDIR ?
                CMFalse:CMTrue;
    }
    return CMFalse;
}

CMUTIL_STATIC CMBool CMUTIL_FileIsDirectory(const CMUTIL_File *file)
{
    struct stat s;
    if ( stat(CMCall(file, GetFullPath), &s) == 0 ) {
        return s.st_mode & S_IFDIR ?
                CMTrue:CMFalse;
    }
    return CMFalse;
}

CMUTIL_STATIC CMBool CMUTIL_FileIsExists(const CMUTIL_File *file)
{
    struct stat s;
    return stat(CMCall(file, GetFullPath), &s) == 0?
            CMTrue:CMFalse;
}

CMUTIL_STATIC long CMUTIL_FileLength(const CMUTIL_File *file)
{
    struct stat s;
    return (long)(stat(CMCall(file, GetFullPath), &s) == 0?
            s.st_size:-1);
}

CMUTIL_STATIC const char *CMUTIL_FileGetName(const CMUTIL_File *file)
{
    return ((const CMUTIL_File_Internal*)file)->name;
}

CMUTIL_STATIC const char *CMUTIL_FileGetFullPath(const CMUTIL_File *file)
{
    return ((const CMUTIL_File_Internal*)file)->path;
}

CMUTIL_STATIC time_t CMUTIL_FileModifiedTime(const CMUTIL_File *file)
{
    struct stat s;
    return stat(CMCall(file, GetFullPath), &s) == 0?
            CMUTIL_MODTIME(s):0;
}

CMUTIL_STATIC CMUTIL_FileList *CMUTIL_FileChildren(const CMUTIL_File *file)
{
    const CMUTIL_File_Internal *ifile = (const CMUTIL_File_Internal*)file;
    CMUTIL_FileList_Internal *flist = CMUTIL_FileListCreate(ifile->memst);
    const char *parent = CMCall(file, GetFullPath);
    char pathbuf[2048];

#if defined(MSWIN)
    WIN32_FIND_DATA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    sprintf(pathbuf, "%s\\*", parent);
    hFind = FindFirstFile(pathbuf, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            char *fname = ffd.cFileName;
            if (strcmp(fname, ".") && strcmp(fname, "..")) {
                sprintf(pathbuf, "%s\\%s", parent, fname);
                CMUTIL_File *f = CMUTIL_FileCreateInternal(
                            ifile->memst, pathbuf);
                ((CMUTIL_File_Internal*)f)->isref = CMTrue;
                CMCall(flist->files, Add, f, NULL);
            }
        } while (FindNextFile(hFind, &ffd) != 0);
    }
#else
    DIR *dirp;
    struct dirent *entry;
    dirp = opendir(parent);
    if (dirp) {
        while ((entry=readdir(dirp)) != NULL) {
            char *fname = entry->d_name;
            if (strcmp(fname, ".") && strcmp(fname, "..")) {
                sprintf(pathbuf, "%s/%s", parent, fname);
                CMUTIL_File *f = CMUTIL_FileCreateInternal(
                            ifile->memst, pathbuf);
                ((CMUTIL_File_Internal*)f)->isref = CMTrue;
                CMCall(flist->files, Add, f, NULL);
            }
        }
        closedir(dirp);
    }
#endif

    return (CMUTIL_FileList*)flist;
}

CMUTIL_STATIC void CMUTIL_FileFindInternal(
        CMUTIL_FileList_Internal *flist,
        const char *dpath,
        const char *pattern,
        CMBool recursive);

CMUTIL_STATIC void CMUTIL_FileFindFileOper(
        CMUTIL_FileList_Internal *flist,
        const char *pattern,
        CMBool recursive,
        const char *filepath,
        const char *fname)
{
    struct stat s;
    if ( stat(filepath, &s) == 0 && (s.st_mode & S_IFDIR)) {
        if (recursive)
            CMUTIL_FileFindInternal(
                        flist, filepath, pattern, recursive);
    } else {
        if (CMUTIL_PatternMatch(pattern, fname)) {
            CMUTIL_File *f = CMUTIL_FileCreateInternal(
                        flist->memst, filepath);
            if (f) {
                ((CMUTIL_File_Internal*)f)->isref = CMTrue;
                CMCall(flist->files, Add, f, NULL);
            } else {
                CMLogError("file creation failed for %s", filepath);
            }
        }
    }
}

CMUTIL_STATIC void CMUTIL_FileFindInternal(
        CMUTIL_FileList_Internal *flist,
        const char *dpath,
        const char *pattern,
        CMBool recursive)
{
    char pathbuf[2048];

#if defined(MSWIN)
    WIN32_FIND_DATA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    sprintf(pathbuf, "%s\\*", dpath);
    hFind = FindFirstFile(pathbuf, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            char *fname = ffd.cFileName;
            if (strcmp(fname, ".") && strcmp(fname, "..")) {
                sprintf(pathbuf, "%s\\%s", dpath, fname);
                CMUTIL_FileFindFileOper(
                            flist, pattern, recursive, pathbuf, fname);
            }
        } while (FindNextFile(hFind, &ffd) != 0);
    }
#else
    DIR *dirp;
    struct dirent *entry;
    dirp = opendir(dpath);
    if (dirp) {
        while ((entry=readdir(dirp)) != NULL) {
            char *fname = entry->d_name;
            if (strcmp(fname, ".") != 0 && strcmp(fname, "..") != 0) {
                sprintf(pathbuf, "%s/%s", dpath, fname);
                CMUTIL_FileFindFileOper(
                            flist, pattern, recursive, pathbuf, fname);
            }
        }
        closedir(dirp);
    }
#endif
}

CMUTIL_STATIC CMUTIL_FileList *CMUTIL_FileFind(
        const CMUTIL_File *file, const char *pattern, CMBool recursive)
{
    const CMUTIL_File_Internal *ifile = (const CMUTIL_File_Internal*)file;
    CMUTIL_FileList_Internal *flist = CMUTIL_FileListCreate(ifile->memst);
    CMUTIL_FileFindInternal(
                flist, CMCall(file, GetFullPath), pattern, recursive);
    return (CMUTIL_FileList*)flist;
}

CMUTIL_STATIC CMUTIL_FileStream *CMUTIL_FileCreateStream(
        const CMUTIL_File *file, CMFileOpenMode mode)
{
    const CMUTIL_File_Internal *ifile = (const CMUTIL_File_Internal*)file;
    const char *mode_str = NULL;
    FILE *f = NULL;
    switch (mode) {
        case CMFileOpenRead:
            mode_str = "rb";
            break;
        case CMFileOpenWrite:
            mode_str = "wb";
            break;
        case CMFileOpenAppend:
            mode_str = "ab";
            break;
        default:
            mode_str = "rb";
            break;
    }
    f = fopen(CMCall(file, GetFullPath), mode_str);
    if (f) {
        CMUTIL_FileStream_Internal *res =
                ifile->memst->Alloc(sizeof(CMUTIL_FileStream_Internal));
        memset(res, 0x0, sizeof(CMUTIL_FileStream_Internal));
        memcpy(res, &g_cmutil_filestream, sizeof(CMUTIL_FileStream));
        res->fp = f;
        res->mode = mode;
        res->memst = ifile->memst;
        return (CMUTIL_FileStream*)res;
    }
    return NULL;
}

CMUTIL_STATIC void CMUTIL_FileDestroy(CMUTIL_File *file)
{
    CMUTIL_File_Internal *ifile = (CMUTIL_File_Internal*)file;
    if (ifile && !ifile->isref) {
        if (ifile->name)
            ifile->memst->Free(ifile->name);
        if (ifile->path)
            ifile->memst->Free(ifile->path);
        ifile->memst->Free(ifile);
    }
}

static CMUTIL_File g_cmutil_file = {
    CMUTIL_FileGetContents,
    CMUTIL_FileDelete,
    CMUTIL_FileIsFile,
    CMUTIL_FileIsDirectory,
    CMUTIL_FileIsExists,
    CMUTIL_FileLength,
    CMUTIL_FileGetName,
    CMUTIL_FileGetFullPath,
    CMUTIL_FileModifiedTime,
    CMUTIL_FileChildren,
    CMUTIL_FileFind,
    CMUTIL_FileCreateStream,
    CMUTIL_FileDestroy
};

CMUTIL_File *CMUTIL_FileCreateInternal(CMUTIL_Mem *memst, const char *path)
{
    if (path) {
        const char *p;
        CMUTIL_File_Internal *res = memst->Alloc(sizeof(CMUTIL_File_Internal));

#if defined(MSWIN)
        char rpath[2048] = {0,};
        GetFullPathName(path, sizeof(rpath), rpath, NULL);
#else
        char rpath[PATH_MAX] = {0,};
        // in some system needs the result buffer size is PATH_MAX,
        // otherwise buffer overflow raised.
        if (realpath(path, rpath) == NULL)
            strcpy(rpath, path);
#endif

        memset(res, 0x0, sizeof(CMUTIL_File_Internal));
        memcpy(res, &g_cmutil_file, sizeof(CMUTIL_File));

        res->memst = memst;

        if ((p = strrchr(rpath, '/')) == NULL)
            p = strrchr(rpath, '\\');

        res->path = memst->Strdup(rpath);
        if (!p) {
            res->name = memst->Strdup(rpath);
        } else {
            res->name = memst->Strdup(p+1);
        }

        return (CMUTIL_File*)res;
    } else {
        return NULL;
    }
}

CMUTIL_File *CMUTIL_FileCreate(const char *path)
{
    return CMUTIL_FileCreateInternal(CMUTIL_GetMem(), path);
}

#if defined(MSWIN)
# define MKDIR(a, b)    _mkdir(a)
#else
# define MKDIR          mkdir
#endif

CMBool CMUTIL_PathCreate(const char *path, uint32_t mode)
{
    int retval, i;

    i = 0;
    while (0 != (retval = MKDIR(path, mode))) {
        char subpath[FILENAME_MAX] = "", *p, *q;

        // existing directory.
        if (errno == EEXIST)
            return CMTrue;

        // no parent directory.
        p = strrchr(path, '/'); q = strrchr(path, '\\');
        if (p && q)
            p = p > q? p:q;
        else if (q)
            p = q;
        if (NULL == p)
            return CMFalse;

        // no permission or no more parent directory
        if (++i > 2 || p == path)
            return CMFalse;
        strncat(subpath, path, (uint64_t)(p - path));     /* Appends NULL */
        // call recursively with the parent directory
        if (!CMUTIL_PathCreate(subpath, mode))
            return CMFalse;
    }
#if defined(MSWIN)
    CMUTIL_UNUSED(mode);
#endif
    return retval == 0? CMTrue:CMFalse;
}
