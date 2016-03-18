
#include "functions.h"

#include <sys/stat.h>
#include <errno.h>

#if !defined(MSWIN)
# include <dirent.h>
#else
# include <direct.h>
#endif

#if defined(LINUX)
# define CMUTIL_MODTIME(a)	a.st_mtim.tv_sec
#else
# define CMUTIL_MODTIME(a)	a.st_mtime
#endif


typedef struct CMUTIL_Library_Internal {
    CMUTIL_Library		base;
    void				*library;
    CMUTIL_Mutex		*mutex;
    CMUTIL_Map			*procs;
    CMUTIL_Mem_st		*memst;
} CMUTIL_Library_Internal;

CMUTIL_STATIC void *CMUTIL_LibraryGetProcedure(
        CMUTIL_Library *lib, const char *proc_name)
{
    CMUTIL_Library_Internal *ilib = (CMUTIL_Library_Internal*)lib;
    void *proc = NULL;
    CMUTIL_CALL(ilib->mutex, Lock);
    proc = CMUTIL_CALL(ilib->procs, Get, proc_name);
    if (!proc) {
#if defined(MSWIN)
        proc = GetProcAddress((HMODULE)ilib->library, proc_name);
#else
        proc = dlsym(ilib->library, proc_name);
#endif
        if (proc)
            CMUTIL_CALL(ilib->procs, Put, proc_name, proc);
    }
    CMUTIL_CALL(ilib->mutex, Unlock);
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
            CMUTIL_CALL(ilib->procs, Destroy);
        if (ilib->mutex)
            CMUTIL_CALL(ilib->mutex, Destroy);
        ilib->memst->Free(ilib);
    }
}

static CMUTIL_Library g_cmutil_library = {
        CMUTIL_LibraryGetProcedure,
        CMUTIL_LibraryDestroy
};

CMUTIL_Library *CMUTIL_LibraryCreateInternal(
        CMUTIL_Mem_st *memst, const char *path)
{
    char slib[1024];
    CMUTIL_Library_Internal *res =
            memst->Alloc(sizeof(CMUTIL_Library_Internal));

    memset(res, 0x0, sizeof(CMUTIL_Library_Internal));
    memcpy(res, &g_cmutil_library, sizeof(CMUTIL_Library));

    sprintf(slib, "%s.%s", path, CMUTIL_SO_EXT);

    res->memst = memst;

#if defined(MSWIN)
    res->library = LoadLibrary(slib);
#else
    res->library = dlopen(slib, RTLD_NOW);
#endif
    res->mutex = CMUTIL_MutexCreateInternal(memst);
    res->procs = CMUTIL_MapCreateInternal(memst, 10, NULL);

    return (CMUTIL_Library*)res;
}

CMUTIL_Library *CMUTIL_LibraryCreate(const char *path)
{
    return CMUTIL_LibraryCreateInternal(__CMUTIL_Mem, path);
}


typedef struct CMUTIL_FileList_Internal {
    CMUTIL_FileList		base;
    CMUTIL_Array		*files;
    CMUTIL_Mem_st		*memst;
} CMUTIL_FileList_Internal;

typedef struct CMUTIL_File_Internal {
    CMUTIL_File			base;
    char				*path;
    char				*name;
    CMUTIL_Bool			isref;
    CMUTIL_Mem_st		*memst;
} CMUTIL_File_Internal;

CMUTIL_STATIC int CMUTIL_FileListCount(CMUTIL_FileList *flist)
{
    CMUTIL_FileList_Internal *iflist = (CMUTIL_FileList_Internal*)flist;
    return CMUTIL_CALL(iflist->files, GetSize);
}

CMUTIL_STATIC CMUTIL_File *CMUTIL_FileListGetAt(
        CMUTIL_FileList *flist, int index)
{
    CMUTIL_FileList_Internal *iflist = (CMUTIL_FileList_Internal*)flist;
    return (CMUTIL_File*)CMUTIL_CALL(iflist->files, GetAt, index);
}

CMUTIL_STATIC void CMUTIL_FileListDestroy(CMUTIL_FileList *flist)
{
    CMUTIL_FileList_Internal *iflist = (CMUTIL_FileList_Internal*)flist;
    if (iflist) {
        if (iflist->files) {
            int i, count = CMUTIL_CALL(iflist->files, GetSize);
            for (i=0; i<count; i++) {
                CMUTIL_File_Internal *file =
                        (CMUTIL_File_Internal*)CMUTIL_CALL(flist, GetAt, i);
                file->isref = CMUTIL_False;
                CMUTIL_CALL((CMUTIL_File*)file, Destroy);
            }
            CMUTIL_CALL(iflist->files, Destroy);
        }
        iflist->memst->Free(iflist);
    }
}

static CMUTIL_FileList g_cmutil_filelist = {
        CMUTIL_FileListCount,
        CMUTIL_FileListGetAt,
        CMUTIL_FileListDestroy
};

CMUTIL_STATIC CMUTIL_FileList_Internal *CMUTIL_FileListCreate(
        CMUTIL_Mem_st *memst)
{
    CMUTIL_FileList_Internal *res =
            memst->Alloc(sizeof(CMUTIL_FileList_Internal));
    memset(res, 0x0, sizeof(CMUTIL_FileList_Internal));
    memcpy(res, &g_cmutil_filelist, sizeof(CMUTIL_FileList));
    res->memst = memst;
    res->files = CMUTIL_ArrayCreateInternal(memst, 10, NULL, NULL);
    return res;
}


CMUTIL_STATIC CMUTIL_String *CMUTIL_FileGetContents(CMUTIL_File *file)
{
    CMUTIL_File_Internal *ifile = (CMUTIL_File_Internal*)file;
    FILE *f = fopen(CMUTIL_CALL(file, GetFullPath), "rb");
    if (f) {
        char buffer[1024];
        size_t rdsz = 0;
        CMUTIL_String *cont =
                CMUTIL_StringCreateInternal(ifile->memst, 1024, NULL);
        while ((rdsz = fread(buffer, 1, 1024, f)) > 0)
            CMUTIL_CALL(cont, AddNString, buffer, (int)rdsz);
        fclose(f);
        return cont;
    }
    return NULL;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_FileDelete(CMUTIL_File *file)
{
    return DeleteFile(CMUTIL_CALL(file, GetFullPath)) == 0 ?
            CMUTIL_True:CMUTIL_False;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_FileIsFile(CMUTIL_File *file)
{
    struct stat s;
    if ( stat(CMUTIL_CALL(file, GetFullPath), &s) == 0 ) {
        return s.st_mode & S_IFDIR ?
                CMUTIL_False:CMUTIL_True;
    }
    return CMUTIL_False;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_FileIsDirectory(CMUTIL_File *file)
{
    struct stat s;
    if ( stat(CMUTIL_CALL(file, GetFullPath), &s) == 0 ) {
        return s.st_mode & S_IFDIR ?
                CMUTIL_True:CMUTIL_False;
    }
    return CMUTIL_False;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_FileIsExists(CMUTIL_File *file)
{
    struct stat s;
    return stat(CMUTIL_CALL(file, GetFullPath), &s) == 0?
            CMUTIL_True:CMUTIL_False;
}

CMUTIL_STATIC long CMUTIL_FileLength(CMUTIL_File *file)
{
    struct stat s;
    return (long)(stat(CMUTIL_CALL(file, GetFullPath), &s) == 0?
            s.st_size:0);
}

CMUTIL_STATIC const char *CMUTIL_FileGetName(CMUTIL_File *file)
{
    return ((CMUTIL_File_Internal*)file)->name;
}

CMUTIL_STATIC const char *CMUTIL_FileGetFullPath(CMUTIL_File *file)
{
    return ((CMUTIL_File_Internal*)file)->path;
}

CMUTIL_STATIC time_t CMUTIL_FileModifiedTime(CMUTIL_File *file)
{
    struct stat s;
    return stat(CMUTIL_CALL(file, GetFullPath), &s) == 0?
            CMUTIL_MODTIME(s):0;
}

CMUTIL_STATIC CMUTIL_FileList *CMUTIL_FileChildren(CMUTIL_File *file)
{
    CMUTIL_File_Internal *ifile = (CMUTIL_File_Internal*)file;
    CMUTIL_FileList_Internal *flist = CMUTIL_FileListCreate(ifile->memst);
    const char *parent = CMUTIL_CALL(file, GetFullPath);
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
                ((CMUTIL_File_Internal*)f)->isref = CMUTIL_True;
                CMUTIL_CALL(flist->files, Add, f);
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
                ((CMUTIL_File_Internal*)f)->isref = CMUTIL_True;
                CMUTIL_CALL(flist->files, Add, f);
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
        CMUTIL_Bool recursive);

CMUTIL_STATIC void CMUTIL_FileFindFileOper(
        CMUTIL_FileList_Internal *flist,
        const char *pattern,
        CMUTIL_Bool recursive,
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
            ((CMUTIL_File_Internal*)f)->isref = CMUTIL_True;
            CMUTIL_CALL(flist->files, Add, f);
        }
    }
}

CMUTIL_STATIC void CMUTIL_FileFindInternal(
        CMUTIL_FileList_Internal *flist,
        const char *dpath,
        const char *pattern,
        CMUTIL_Bool recursive)
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
                sprintf(pathbuf, "%s\\%s", parent, fname);
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
            if (strcmp(fname, ".") && strcmp(fname, "..")) {
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
        CMUTIL_File *file, const char *pattern, CMUTIL_Bool recursive)
{
    CMUTIL_File_Internal *ifile = (CMUTIL_File_Internal*)file;
    CMUTIL_FileList_Internal *flist = CMUTIL_FileListCreate(ifile->memst);
    CMUTIL_FileFindInternal(
                flist, CMUTIL_CALL(file, GetFullPath), pattern, recursive);
    return (CMUTIL_FileList*)flist;
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
    CMUTIL_FileDestroy
};

CMUTIL_File *CMUTIL_FileCreateInternal(CMUTIL_Mem_st *memst, const char *path)
{
    if (path) {
        char rpath[2048];
        const char *p;
        CMUTIL_File_Internal *res = memst->Alloc(sizeof(CMUTIL_File_Internal));

#if defined(MSWIN)
        GetFullPathName(path, 2048, rpath, NULL);
#else
        realpath(path, rpath);
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
    return CMUTIL_FileCreateInternal(__CMUTIL_Mem, path);
}

#if defined(MSWIN)
# define MKDIR(a, b)	_mkdir(a)
#else
# define MKDIR			mkdir
#endif

CMUTIL_Bool CMUTIL_PathCreate(const char *path, int mode)
{
    int retval, i;

    i = 0;
    while (0 != (retval = MKDIR(path, mode))) {
        char subpath[FILENAME_MAX] = "", *p, *q;

        // existing directory.
        if (errno == EEXIST)
            return CMUTIL_True;

        // no parent directory.
        p = strrchr(path, '/'); q = strrchr(path, '\\');
        if (p && q)
            p = p > q? p:q;
        else if (q)
            p = q;
        if (NULL == p)
            return CMUTIL_False;

        // no permission or path cannot be created.
        if (++i > 2)
            return CMUTIL_False;
        strncat(subpath, path, p - path);     /* Appends NULL */
        CMUTIL_FileCreate(subpath);
    }
#if defined(MSWIN)
    CMUTIL_UNUSED(mode);
#endif
    return retval;
}
