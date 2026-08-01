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
#include <stdint.h>

CMUTIL_LogDefine("cmutils.process")

#if defined(MSWIN)
typedef HANDLE CMStream;
#define EMPTY_STREAM INVALID_HANDLE_VALUE
#else
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>

#if defined(APPLE)
// referencing 'environ' directly is not allowed in a shared library on
// darwin, the runtime provides an accessor instead.
# include <crt_externs.h>
# define CMUTIL_ENVIRON (*_NSGetEnviron())
#else
extern char **environ;
# define CMUTIL_ENVIRON environ
#endif

typedef int CMStream;
#define EMPTY_STREAM (-1)
#endif


typedef enum CMProcStatus_ {
    CM_IDLE_ = 0,
    CM_RUNNING_,
    CM_SUSPENDED_,
    CM_EXITED_
} CMProcStatus_;

typedef struct CMUTIL_Process_Internal {
    CMUTIL_Process      base;
    CMUTIL_Mem          *memst;
    CMUTIL_String       *command;
    CMUTIL_String       *cwd;
    CMProcStreamType    type;
    CMUTIL_Map          *env;
    CMUTIL_StringArray  *args;
#if defined(MSWIN)
    HANDLE              hproc;
#endif
    pid_t               pid;
    CMUTIL_Process      *pipe_from;
    CMUTIL_Process      *pipe_to;
    CMProcStatus_       status;
    CMStream            inpipe;
    CMStream            outpipe;
    CMStream            errpipe;
    CMUTIL_Thread       *reader;
} CMUTIL_Process_Internal;


CMUTIL_STATIC void CMUTIL_ClosePipe(CMStream strm)
{
    if (strm != EMPTY_STREAM) {
#if defined(MSWIN)
        CloseHandle(strm);
#else
        close(strm);
#endif
    }
}

#if defined(MSWIN)
CMUTIL_STATIC CMBool CMUTIL_CreatePipe(
    CMStream *pipe, CMBool inherit_read, CMBool inherit_write)
{
    SECURITY_ATTRIBUTES saAttr;
    // Set the bInheritHandle flag so pipe handles are inherited.
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = inherit_read || inherit_write;
    saAttr.lpSecurityDescriptor = NULL;

    pipe[0] = EMPTY_STREAM;
    pipe[1] = EMPTY_STREAM;

    // Create a pipe for the child process's STDIN / STDOUT / STDERR.
    if (!CreatePipe(pipe, pipe+1, &saAttr, 4096)) {
        CMLogErrorS("CreatePipe failed");
        pipe[0] = EMPTY_STREAM;
        pipe[1] = EMPTY_STREAM;
        return CMFalse;
    }
    // Ensure the read handle to the pipe is not inherited.
    if (!inherit_read && !SetHandleInformation(pipe[0], HANDLE_FLAG_INHERIT, 0)) {
        CMLogErrorS("SetHandleInformation failed");
        goto FAILED;
    }
    // Ensure the write handle to the pipe is not inherited.
    if (!inherit_write && !SetHandleInformation(pipe[1], HANDLE_FLAG_INHERIT, 0)) {
        CMLogErrorS("SetHandleInformation failed");
        goto FAILED;
    }
    return CMTrue;
FAILED:
    CMUTIL_ClosePipe(pipe[0]);
    CMUTIL_ClosePipe(pipe[1]);
    pipe[0] = EMPTY_STREAM;
    pipe[1] = EMPTY_STREAM;
    return CMFalse;
}

CMUTIL_STATIC CMBool CMUTIL_StartSubprocess(CMUTIL_Process *proc)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    CMStream p1[2] = { EMPTY_STREAM, EMPTY_STREAM };
    CMStream p2[2] = { EMPTY_STREAM, EMPTY_STREAM };
    CMStream p3[2] = { EMPTY_STREAM, EMPTY_STREAM };

    CMUTIL_ByteBuffer *envbuf = NULL;

    CMUTIL_String *cmd = NULL;
    int i;
    const char *cwd = NULL;
    char *env = NULL;
    BOOL bSuccess = FALSE;

    // Create a pipe for the child process's STDIN.
    if (!CMUTIL_CreatePipe(p1, CMTrue, CMFalse))
        goto ERROR_POINT;
    // Create a pipe for the child process's STDOUT.
    if (!CMUTIL_CreatePipe(p2, CMFalse, CMTrue))
        goto ERROR_POINT;
    // Create a pipe for the child process's STDERR.
    if (!CMUTIL_CreatePipe(p3, CMFalse, CMTrue))
        goto ERROR_POINT;

    ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
    ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdInput = p1[0];
    siStartInfo.hStdOutput = p2[1];
    siStartInfo.hStdError = p3[1];
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    siStartInfo.wShowWindow = SW_HIDE;


    cmd = CMCall(ip->command, Clone);
    for (i=0; i<CMCall(ip->args, GetSize); i++) {
        const CMUTIL_String *arg = CMCall(ip->args, GetAt, i);
        CMCall(cmd, AddChar, ' ');
        CMCall(cmd, AddChar, '"');
        CMCall(cmd, AddAnother, arg);
        CMCall(cmd, AddChar, '"');
    }
    if (ip->cwd) {
        cwd = CMCall(ip->cwd, GetCString);
        CMLogDebug("Working directory: %s", cwd);
    }
    if (ip->env) {
        const CMUTIL_Array *pairs = CMCall(ip->env, GetPairs);
        envbuf = CMUTIL_ByteBufferCreateInternal(ip->memst, 512);
        for (i=0; i<CMCall(pairs, GetSize); i++) {
            CMUTIL_MapPair *pair = CMCall(pairs, GetAt, i);
            const char *skey = CMCall(pair, GetKey);
            const char *value = CMCall(pair, GetValue);
            CMCall(envbuf, AddBytes, (uint8_t*)skey, (uint32_t)strlen(skey));
            CMCall(envbuf, AddByte, '=');
            CMCall(envbuf, AddBytes, (uint8_t*)value, (uint32_t)strlen(value));
            CMCall(envbuf, AddByte, '\0');
        }
        CMCall(envbuf, AddByte, '\0');
        env = (char*)CMCall(envbuf, GetBytes);
        if (CMLogIsEnabled(CMLogLevel_Debug)) {
            CMUTIL_String *buf = CMUTIL_StringCreate();
            CMCall(ip->env, PrintTo, buf, NULL);
            CMLogDebug("Environment variables: %s", CMCall(buf, GetCString));
            CMCall(buf, Destroy);
        }
    }

    bSuccess = CreateProcess(NULL,
                                  (LPTSTR)CMCall(cmd, GetCString), // command line
                                  NULL, // process security attributes
                                  NULL, // primary thread security attributes
                                  TRUE, // handles are inherited
                                  CREATE_NO_WINDOW, // creation flags
                                  env, // use parent's environment
                                  cwd, // use parent's current directory
                                  &siStartInfo, // STARTUPINFO pointer
                                  &piProcInfo);     // receives PROCESS_INFORMATION
    CMCall(cmd, Destroy);
    if (envbuf) CMCall(envbuf, Destroy);
    if (!bSuccess) {
        CMLogErrorS("CreateProcess failed");
        goto ERROR_POINT;
    }
    ip->hproc = piProcInfo.hProcess;
    CloseHandle(piProcInfo.hThread);
    ip->pid = piProcInfo.dwProcessId;
    // these pipes are used in child process no longer needed in parent

    ip->inpipe = p1[1];
    ip->outpipe = p2[0];
    ip->errpipe = p3[0];
    CMUTIL_ClosePipe(p1[0]);
    CMUTIL_ClosePipe(p2[1]);
    CMUTIL_ClosePipe(p3[1]);
    return CMTrue;
ERROR_POINT:
    CMUTIL_ClosePipe(p1[0]);
    CMUTIL_ClosePipe(p1[1]);
    CMUTIL_ClosePipe(p2[0]);
    CMUTIL_ClosePipe(p2[1]);
    CMUTIL_ClosePipe(p3[0]);
    CMUTIL_ClosePipe(p3[1]);

    ip->inpipe = EMPTY_STREAM;
    ip->outpipe = EMPTY_STREAM;
    ip->errpipe = EMPTY_STREAM;
    return CMFalse;
}

#else

CMUTIL_STATIC void CMUTIL_FreeEnviron(CMUTIL_Mem *memst, char **envp)
{
    if (envp) {
        int i;
        for (i = 0; envp[i]; i++)
            memst->Free(envp[i]);
        memst->Free(envp);
    }
}

/**
 * Builds a NULL terminated "KEY=VALUE" array out of the current environment
 * overlaid with ip->env. This must be done by the parent, the forked child
 * is not allowed to allocate memory.
 */
CMUTIL_STATIC char **CMUTIL_BuildEnviron(CMUTIL_Process_Internal *ip)
{
    char **cur = CMUTIL_ENVIRON;
    const CMUTIL_Array *pairs = ip->env? CMCall(ip->env, GetPairs):NULL;
    const int npairs = pairs? CMCall(pairs, GetSize):0;
    int ncur = 0, cnt = 0, i, j;
    char **res = NULL;
    size_t asize;

    while (cur && cur[ncur]) ncur++;

    asize = (size_t)(ncur + npairs + 1) * sizeof(char*);
    res = (char**)ip->memst->Alloc(asize);
    if (res == NULL) return NULL;
    memset(res, 0x0, asize);

    for (i = 0; i < ncur; i++) {
        const char *eq = strchr(cur[i], '=');
        const size_t klen = eq?
                    (size_t)(eq - cur[i]):strlen(cur[i]);
        const size_t ilen = strlen(cur[i]);
        CMBool overridden = CMFalse;
        for (j = 0; j < npairs; j++) {
            CMUTIL_MapPair *pair = (CMUTIL_MapPair*)CMCall(pairs, GetAt, j);
            const char *skey = CMCall(pair, GetKey);
            if (strlen(skey) == klen && strncmp(skey, cur[i], klen) == 0) {
                overridden = CMTrue;
                break;
            }
        }
        if (overridden) continue;
        res[cnt] = (char*)ip->memst->Alloc(ilen + 1);
        if (res[cnt] == NULL) goto FAILED;
        memcpy(res[cnt], cur[i], ilen + 1);
        cnt++;
    }

    for (j = 0; j < npairs; j++) {
        CMUTIL_MapPair *pair = (CMUTIL_MapPair*)CMCall(pairs, GetAt, j);
        const char *skey = CMCall(pair, GetKey);
        const char *value = CMCall(pair, GetValue);
        size_t klen, vlen;
        if (skey == NULL || value == NULL) continue;
        klen = strlen(skey);
        vlen = strlen(value);
        res[cnt] = (char*)ip->memst->Alloc(klen + vlen + 2);
        if (res[cnt] == NULL) goto FAILED;
        memcpy(res[cnt], skey, klen);
        res[cnt][klen] = '=';
        memcpy(res[cnt] + klen + 1, value, vlen + 1);
        cnt++;
    }
    res[cnt] = NULL;
    return res;
FAILED:
    CMUTIL_FreeEnviron(ip->memst, res);
    return NULL;
}

CMUTIL_STATIC CMBool CMUTIL_StartSubprocess(CMUTIL_Process *proc)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    // p1 is created only when the caller writes to the child's stdin,
    // so it must be initialized for the error cleanup path.
    CMStream p1[2] = { EMPTY_STREAM, EMPTY_STREAM };
    CMStream p2[2] = { EMPTY_STREAM, EMPTY_STREAM };
    CMStream p3[2] = { EMPTY_STREAM, EMPTY_STREAM };
    // p4 reports a failing execvp back to the parent, it is closed by the
    // kernel on a successful exec so the parent reads EOF then.
    CMStream p4[2] = { EMPTY_STREAM, EMPTY_STREAM };
    int i, nargs, exec_errno = 0;
    pid_t pid;
    ssize_t nread;
    char **args = NULL;
    char **envp = NULL;
    const char *command = NULL;
    const char *cwd = NULL;

    if (ip->type & CMProcStreamWrite && pipe(p1) == -1)
        goto err_pipe1;
    if (pipe(p2) == -1)
        goto err_pipe2;
    if (pipe(p3) == -1)
        goto err_pipe3;
    if (pipe(p4) == -1)
        goto err_pipe4;
    if (fcntl(p4[1], F_SETFD, FD_CLOEXEC) == -1) {
        CMLogErrorS("fcntl failed %d:%s", errno, strerror(errno));
        goto err_fork;
    }

    // everything the child needs is prepared here, see the comment in the
    // child branch below.
    command = CMCall(ip->command, GetCString);
    nargs = ip->args? CMCall(ip->args, GetSize):0;
    args = (char**)ip->memst->Alloc((size_t)(nargs + 2) * sizeof(char*));
    if (args == NULL) {
        CMLogErrorS("cannot allocate argument vector.");
        goto err_fork;
    }
    memset(args, 0x0, (size_t)(nargs + 2) * sizeof(char*));
    args[0] = (char*)command;
    for (i = 0; i < nargs; i++) {
        const CMUTIL_String *arg = CMCall(ip->args, GetAt, i);
        args[i+1] = (char*)CMCall(arg, GetCString);
    }

    envp = CMUTIL_BuildEnviron(ip);
    if (envp == NULL) {
        CMLogErrorS("cannot build child environment.");
        goto err_fork;
    }

    if (ip->cwd)
        cwd = CMCall(ip->cwd, GetCString);

    if (CMLogIsEnabled(CMLogLevel_Debug)) {
        if (cwd)
            CMLogDebug("Working directory: %s", cwd);
        if (ip->env) {
            CMUTIL_String *buf = CMUTIL_StringCreate();
            CMCall(ip->env, PrintTo, buf, NULL);
            CMLogDebug("Environment variables: %s", CMCall(buf, GetCString));
            CMCall(buf, Destroy);
        }
    }

    if ((pid = fork()) == -1) {
        CMLogErrorS("fork failed %d:%s", errno, strerror(errno));
        goto err_fork;
    }

    if (pid == 0) {
        /* Child process.
         * Only async-signal-safe calls are allowed from here on. The parent
         * may be multi threaded and fork clones the calling thread only, so
         * any lock a foreign thread held at fork time stays locked forever
         * in this image. Do not add logging, allocation or any other
         * library call before execvp. */
        if (ip->type & CMProcStreamWrite) {
            dup2(p1[0], STDIN_FILENO);
            close(p1[0]);
            close(p1[1]);
        } else if (ip->inpipe != EMPTY_STREAM) {
            dup2(ip->inpipe, STDIN_FILENO);
            close(ip->inpipe);
        }
        dup2(p2[1], STDOUT_FILENO);
        dup2(p3[1], STDERR_FILENO);
        close(p2[0]);
        close(p2[1]);
        close(p3[0]);
        close(p3[1]);
        close(p4[0]);

        if (cwd && chdir(cwd) != 0)
            goto err_child;

        CMUTIL_ENVIRON = envp;
        execvp(command, args);
err_child:
        {
            const int cerr = errno;
            const ssize_t wr = write(p4[1], &cerr, sizeof(cerr));
            (void)wr;
        }
        _exit(127);
    }

    /* Parent process. */
    ip->pid = pid;
    close(p4[1]);
    p4[1] = EMPTY_STREAM;
    do {
        nread = read(p4[0], &exec_errno, sizeof(exec_errno));
    } while (nread == -1 && errno == EINTR);
    close(p4[0]);
    p4[0] = EMPTY_STREAM;

    if (nread == (ssize_t)sizeof(exec_errno)) {
        int status = 0;
        CMLogErrorS("error running %s: %d:%s",
            command, exec_errno, strerror(exec_errno));
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR) ;
        goto err_fork;
    }

    if (ip->type & CMProcStreamWrite) {
        ip->inpipe = p1[1];  // write to process stdin
        CMUTIL_ClosePipe(p1[0]);
    }
    ip->outpipe = p2[0]; // read from process stdout
    ip->errpipe = p3[0]; // read from process stderr
    CMUTIL_ClosePipe(p2[1]);
    CMUTIL_ClosePipe(p3[1]);
    CMUTIL_FreeEnviron(ip->memst, envp);
    ip->memst->Free(args);
    return CMTrue;

err_fork:
    CMUTIL_FreeEnviron(ip->memst, envp);
    if (args) ip->memst->Free(args);
    CMUTIL_ClosePipe(p4[1]);
    CMUTIL_ClosePipe(p4[0]);
err_pipe4:
    CMUTIL_ClosePipe(p3[1]);
    CMUTIL_ClosePipe(p3[0]);
err_pipe3:
    CMUTIL_ClosePipe(p2[1]);
    CMUTIL_ClosePipe(p2[0]);
err_pipe2:
    if (ip->type & CMProcStreamWrite) {
        CMUTIL_ClosePipe(p1[1]);
        CMUTIL_ClosePipe(p1[0]);
    }
err_pipe1:
    return CMFalse;
}
#endif


CMUTIL_STATIC ssize_t CMUTIL_ProcessReadPipe(
    CMStream strm, uint8_t *data, size_t size)
{
    ssize_t read_size = 0;
#ifdef _WIN32
    while (read_size < (ssize_t)size) {
        DWORD rsz = 0;
        if (!ReadFile(strm, data+read_size,
            (DWORD)(size-read_size), &rsz, NULL)) {
            if (GetLastError() != ERROR_IO_PENDING) {
                return -1;
            }
            Sleep(10);
        }
        if (rsz > 0)
            read_size += rsz;
    }
#else
    read_size = read(strm, data, size);
#endif
    return read_size;
}

CMUTIL_STATIC ssize_t CMUTIL_ProcessWritePipe(
    CMStream strm, const uint8_t *data, size_t size)
{
    ssize_t write_size = 0;
#ifdef _WIN32
    while (write_size < (ssize_t)size) {
        DWORD wsz = 0;
        if (!WriteFile(strm, data+write_size,
            (DWORD)(size-write_size), &wsz, NULL)) {
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                return -1;
            }
            Sleep(10);
        }
        if (wsz > 0)
            write_size += wsz;
    }
#else
    write_size = write(strm, data, size);
#endif
    return write_size;
}

CMUTIL_STATIC void CMUTIL_ProcessFlushStdout(
    CMUTIL_Process_Internal *ip, CMUTIL_ByteBuffer *bbuf)
{
    const uint8_t *buf = CMCall(bbuf, GetBytes);
    const size_t size = CMCall(bbuf, GetSize);
    if (ip->pipe_to) {
        CMCall(ip->pipe_to, Write, buf, (uint32_t)size);
    } else {
        CMLogDebug("[%s] stdout - %.*s", CMCall(ip->command, GetCString), (int)size, buf);
    }
    CMCall(bbuf, Clear);
}

CMUTIL_STATIC void CMUTIL_ProcessFlushStderr(
    CMUTIL_Process_Internal *ip, CMUTIL_ByteBuffer *bbuf)
{
    const uint8_t *buf = CMCall(bbuf, GetBytes);
    const size_t size = CMCall(bbuf, GetSize);
    CMLogDebug("[%s] stderr - %.*s", CMCall(ip->command, GetCString), (int)size, buf);
    CMCall(bbuf, Clear);
}

CMUTIL_STATIC void *CMUTIL_ProcessReadProc(void *data)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)data;
    CMUTIL_ByteBuffer *stdbuf = CMUTIL_ByteBufferCreateInternal(
        ip->memst, 1025);
    CMUTIL_ByteBuffer *errbuf = CMUTIL_ByteBufferCreateInternal(
        ip->memst, 1025);
    int closecnt = 0;
#if defined(MSWIN)
    // anonymous pipe handles are not waitable synchronization objects,
    // so availability must be probed with PeekNamedPipe.
    if (ip->type & CMProcStreamRead && ip->pipe_to == NULL) {
        // stdout is consumed by the caller, not by this reader.
        closecnt |= 1;
    }
    if (ip->type & CMProcStreamReadErr) {
        // stderr is consumed by the caller, not by this reader.
        closecnt |= 2;
    }

    while (closecnt < 3) {
        uint8_t c;
        CMBool readany = CMFalse;
        DWORD avail = 0;
        if (!(closecnt & 1)) {
            if (!PeekNamedPipe(ip->outpipe, NULL, 0, NULL, &avail, NULL)) {
                closecnt |= 1;
            } else if (avail > 0) {
                ssize_t sz = CMUTIL_ProcessReadPipe(ip->outpipe, &c, 1);
                if (sz == 1) {
                    readany = CMTrue;
                    if (strchr("\r\n", c) == NULL) {
                        CMCall(stdbuf, AddByte, c);
                    }
                    if (CMCall(stdbuf, GetSize) == 1024 || c == '\n') {
                        CMUTIL_ProcessFlushStdout(ip, stdbuf);
                    }
                } else {
                    if (sz < 0)
                        CMLogError("read stdout error: %s", strerror(errno));
                    closecnt |= 1;
                }
            }
        }
        if (!(closecnt & 2)) {
            avail = 0;
            if (!PeekNamedPipe(ip->errpipe, NULL, 0, NULL, &avail, NULL)) {
                closecnt |= 2;
            } else if (avail > 0) {
                ssize_t sz = CMUTIL_ProcessReadPipe(ip->errpipe, &c, 1);
                if (sz == 1) {
                    readany = CMTrue;
                    if (strchr("\r\n", c) == NULL) {
                        CMCall(errbuf, AddByte, c);
                    }
                    if (CMCall(errbuf, GetSize) == 1024 || c == '\n') {
                        CMUTIL_ProcessFlushStderr(ip, errbuf);
                    }
                } else {
                    if (sz < 0)
                        CMLogError("read stderr error: %s", strerror(errno));
                    closecnt |= 2;
                }
            }
        }
        if (!readany) {
            if (CMCall(stdbuf, GetSize) > 0) {
                CMUTIL_ProcessFlushStdout(ip, stdbuf);
                CMCall(stdbuf, Clear);
            }
            if (CMCall(errbuf, GetSize) > 0) {
                CMUTIL_ProcessFlushStderr(ip, errbuf);
                CMCall(errbuf, Clear);
            }
            USLEEP(10000);
        }
    }
#else
    int fd_cnt = 2;
    struct pollfd pfd[2];
    struct pollfd *tpfd = pfd;
    memset(pfd, 0x0, sizeof(struct pollfd)*2);

    if (ip->type & CMProcStreamRead && ip->pipe_to == NULL) {
        // stdout is consumed by the caller, not by this reader.
        fd_cnt--;
        tpfd = pfd + 1;
        closecnt |= 1;
    }
    if (ip->type & CMProcStreamReadErr) {
        // stderr is consumed by the caller, not by this reader.
        fd_cnt--;
        closecnt |= 2;
    }
    pfd[0].fd = ip->outpipe;
    pfd[1].fd = ip->errpipe;
    pfd[0].events = POLLIN;
    pfd[1].events = POLLIN;

    while (closecnt < 3) {
        uint8_t c = 0;
        const int rc = poll(tpfd, fd_cnt, 0);
        if (rc > 0) {
            ssize_t n;
            if (pfd[0].revents) {
                n = CMUTIL_ProcessReadPipe(ip->outpipe, &c, 1);
                if (n == 1) {
                    if (strchr("\r\n", c) == NULL) {
                        CMCall(stdbuf, AddByte, c);
                    }
                    if (CMCall(stdbuf, GetSize) == 1024 || c == '\n') {
                        CMUTIL_ProcessFlushStdout(ip, stdbuf);
                    }
                } else {
                    if (n < 0)
                        CMLogError("read stdout error: %s", strerror(errno));
                    closecnt |= 1;
                }
                pfd[0].revents = 0;
                // switch to stderr only when this reader owns stderr,
                // otherwise the caller's stream would be stolen.
                if (closecnt & 1 && closecnt < 3 &&
                        !(ip->type & CMProcStreamReadErr)) {
                    fd_cnt = 1;
                    tpfd = pfd + 1;
                }
            }
            if (pfd[1].revents) {
                n = CMUTIL_ProcessReadPipe(ip->errpipe, &c, 1);
                if (n == 1) {
                    if (strchr("\r\n", c) == NULL) {
                        CMCall(errbuf, AddByte, c);
                    }
                    if (CMCall(errbuf, GetSize) == 1024 || c == '\n') {
                        CMUTIL_ProcessFlushStderr(ip, errbuf);
                    }
                } else {
                    if (n < 0)
                        CMLogError("read stderr error: %s", strerror(errno));
                    closecnt |= 2;
                }
                pfd[1].revents = 0;
                // a closed descriptor keeps reporting POLLHUP,
                // so drop it from the poll set.
                if (closecnt & 2 && closecnt < 3) {
                    fd_cnt = 1;
                    tpfd = pfd;
                }
            }
        } else if (rc == 0) {
            if (CMCall(stdbuf, GetSize) > 0) {
                CMUTIL_ProcessFlushStdout(ip, stdbuf);
                CMCall(stdbuf, Clear);
            }
            if (CMCall(errbuf, GetSize) > 0) {
                CMUTIL_ProcessFlushStderr(ip, errbuf);
                CMCall(errbuf, Clear);
            }
            USLEEP(10000);
        } else {
            CMLogError("rc(%d) poll error: %s", rc, strerror(errno));
            break;
        }
    }
#endif
    // ip->status = CM_EXITED_;
    if (CMCall(stdbuf, GetSize) > 0) {
        CMUTIL_ProcessFlushStdout(ip, stdbuf);
    }
    if (CMCall(errbuf, GetSize) > 0) {
        CMUTIL_ProcessFlushStderr(ip, errbuf);
    }

    CMCall(stdbuf, Destroy);
    CMCall(errbuf, Destroy);
    return NULL;
}


CMUTIL_STATIC CMBool CMUTIL_ProcessStart(
    CMUTIL_Process *proc, CMProcStreamType type)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    if (ip->status != CM_IDLE_ && ip->status != CM_EXITED_) {
        CMLogErrorS("Process is already running.");
        return CMFalse;
    }

    // clean up the remnants of a previous run, otherwise
    // the pipes and the process handle would be leaked.
    if (ip->reader) {
        CMCall(ip->reader, Join);
        ip->reader = NULL;
    }
    CMUTIL_ClosePipe(ip->inpipe);
    CMUTIL_ClosePipe(ip->outpipe);
    CMUTIL_ClosePipe(ip->errpipe);
    ip->inpipe = EMPTY_STREAM;
    ip->outpipe = EMPTY_STREAM;
    ip->errpipe = EMPTY_STREAM;
#if defined(MSWIN)
    if (ip->hproc) {
        CloseHandle(ip->hproc);
        ip->hproc = NULL;
    }
#endif

    ip->type = type;
    if (!CMUTIL_StartSubprocess(proc)) {
        CMLogError("Failed to start subprocess.");
        return CMFalse;
    }

    // start a reader thread
    ip->status = CM_RUNNING_;
    ip->reader = CMUTIL_ThreadCreate(CMUTIL_ProcessReadProc, ip, NULL);
    if (ip->reader == NULL) {
        CMLogError("Failed to create reader thread.");
        return CMFalse;
    }
    CMCall(ip->reader, Start);

    return CMTrue;
}

CMUTIL_STATIC pid_t CMUTIL_ProcessGetPid(CMUTIL_Process *proc)
{
    const CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    return ip->pid;
}

CMUTIL_STATIC const char *CMUTIL_ProcessGetCommand(CMUTIL_Process *proc)
{
    const CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    return CMCall(ip->command, GetCString);
}

CMUTIL_STATIC const char *CMUTIL_ProcessGetWorkDir(
    CMUTIL_Process *proc)
{
    const CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    return CMCall(ip->cwd, GetCString);
}

CMUTIL_STATIC const CMUTIL_StringArray *CMUTIL_ProcessGetArgs(
    CMUTIL_Process *proc)
{
    const CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    return ip->args;
}

CMUTIL_STATIC const CMUTIL_Map *CMUTIL_ProcessGetEnv(CMUTIL_Process *proc)
{
    const CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    return ip->env;
}

CMUTIL_STATIC void CMUTIL_ProcessSuspend(CMUTIL_Process *proc)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    if (ip->status != CM_RUNNING_) {
        CMLogErrorS("Process is not running.");
        return;
    }
#if defined(MSWIN)
    DebugActiveProcess(ip->pid);
#else
    kill(ip->pid, SIGSTOP);
#endif

    ip->status = CM_SUSPENDED_;
}

CMUTIL_STATIC void CMUTIL_ProcessResume(CMUTIL_Process *proc)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    if (ip->status != CM_SUSPENDED_) {
        CMLogErrorS("Process is not suspended.");
        return;
    }
#if defined(MSWIN)
    DebugActiveProcessStop(ip->pid);
#else
    kill(ip->pid, SIGCONT);
#endif

    ip->status = CM_RUNNING_;
}

CMUTIL_STATIC CMBool CMUTIL_ProcessPipeTo(
    CMUTIL_Process *proc, CMUTIL_Process *target)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    CMUTIL_Process_Internal *it = (CMUTIL_Process_Internal *)target;

    if (ip->status != CM_RUNNING_ ||
        (ip->type != CMProcStreamRead && ip->type != CMProcStreamReadWrite)) {
        CMLogErrorS("Process is not suitable for piping.");
        return CMFalse;
    }

    if (it->status != CM_RUNNING_ ||
        (it->type != CMProcStreamWrite && it->type != CMProcStreamReadWrite)) {
        CMLogErrorS("Target process is not suitable for piping.");
        return CMFalse;
    }

    it->pipe_from = proc;
    ip->pipe_to = target;
    return CMTrue;
}

CMUTIL_STATIC ssize_t CMUTIL_ProcessWrite(
    CMUTIL_Process *proc, const void *buf, size_t count)
{
    const CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    return CMUTIL_ProcessWritePipe(ip->inpipe, buf, count);
}

CMUTIL_STATIC ssize_t CMUTIL_ProcessRead(
    CMUTIL_Process *proc, CMUTIL_ByteBuffer *buf, size_t count)
{
    const CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    ssize_t ir;
    uint8_t bytes[1024];
    ssize_t nread = 0;
    while (nread < (ssize_t)count) {
        ssize_t remain = (ssize_t)count - nread;
        if (remain > sizeof(bytes)) remain = sizeof(bytes);
        ir = CMUTIL_ProcessReadPipe(ip->outpipe, bytes, remain);
        if (ir <= 0) break;
        CMCall(buf, AddBytes, bytes, (uint32_t)ir);
        nread += ir;
    }
    return nread;
}

CMUTIL_STATIC int CMUTIL_ProcessWait(CMUTIL_Process *proc, long millis)
{
    int status = -1;
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;

    if (ip->status == CM_RUNNING_) {
#if defined(MSWIN)
        if (ip->hproc) {
            DWORD exit_code = 0;
            DWORD wres = WaitForSingleObject(
                ip->hproc, millis < 0 ? INFINITE : (DWORD)millis);
            if (wres != WAIT_OBJECT_0) {
                // the child is still alive, joining the reader thread
                // would block until the child terminates.
                CMLogWarn("process(%d) wait timed out.", ip->pid);
                return -1;
            }
            if (!GetExitCodeProcess(ip->hproc, &exit_code))
                exit_code = (DWORD)-1;
            status = (int)exit_code;
        } else {
            return -1;
        }
#else
        long counts = millis / 100;
        long step = 100000;   // 100 milliseconds
        CMBool exited = CMFalse;
        if (millis < 0) {
            counts = INT32_MAX;
        } else {
            if (counts == 0) {
                counts = 1;
                step = millis * 1000;
            } else {
                counts++;
            }
        }
        while (counts--) {
            const pid_t pid = waitpid(ip->pid, &status, WNOHANG);
            if (pid > 0) {
                exited = CMTrue;
                break;
            }
            if (pid < 0) {
                // already reaped or not our child
                CMLogWarn("waitpid(%d) failed %d:%s",
                    ip->pid, errno, strerror(errno));
                exited = CMTrue;
                status = -1;
                break;
            }
            USLEEP((uint32_t)step);
        }
        if (!exited) {
            // the child is still alive, joining the reader thread
            // would block until the child terminates.
            CMLogWarn("process(%d) wait timed out.", ip->pid);
            return -1;
        }
#endif
        // the child has been reaped already, further signaling is invalid.
        ip->status = CM_EXITED_;
        if (ip->reader) {
            CMCall(ip->reader, Join);
            ip->reader = NULL;
        }
        if (CMLogIsEnabled(CMLogLevel_Debug)) {
            CMUTIL_String *buf = CMUTIL_StringCreate();
            const char *command = NULL;
            CMCall(buf, AddAnother, ip->command);
            CMCall(buf, AddChar, ' ');
            CMCall(ip->args, PrintTo, buf);
            command = CMCall(buf, GetCString);
            CMLogDebug("process(%s : %d) exit code: %d",
                command, ip->pid, status);
            CMCall(buf, Destroy);
        }
    } else {
        CMLogErrorS("Process is not running.");
        status = -1;
    }
    return status;
}

CMUTIL_STATIC void CMUTIL_ProcessKill(CMUTIL_Process *proc)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    if (ip->status == CM_RUNNING_ || ip->status == CM_SUSPENDED_) {
#if defined(MSWIN)
        // hproc is zero initialized, NULL is the only empty sentinel.
        if (ip->hproc) {
            TerminateProcess(ip->hproc, PROCESS_TERMINATE);
            WaitForSingleObject(ip->hproc, 1000);
            CloseHandle(ip->hproc);
            ip->hproc = NULL;
        }
#else
        {
            // the terminated child must be reaped, otherwise it stays zombie.
            int st = 0;
            int i;
            pid_t r = 0;
            kill(ip->pid, SIGTERM);
            for (i = 0; i < 20; i++) {
                r = waitpid(ip->pid, &st, WNOHANG);
                if (r != 0) break;
                USLEEP(50000);
            }
            if (r == 0) {
                kill(ip->pid, SIGKILL);
                waitpid(ip->pid, &st, 0);
            }
        }
#endif
    } else {
        CMLogErrorS("Process is not running.");
    }
    ip->status = CM_EXITED_;
}

CMUTIL_STATIC void CMUTIL_ProcessDestroy(CMUTIL_Process *proc)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    // disconnect from a process chain
    if (ip->pipe_from) {
        CMUTIL_Process_Internal *it = (CMUTIL_Process_Internal *)ip->pipe_from;
        it->pipe_to = NULL;
        ip->pipe_from = NULL;
    }
    if (ip->pipe_to) {
        CMUTIL_Process_Internal *it = (CMUTIL_Process_Internal *)ip->pipe_to;
        it->pipe_from = NULL;
        ip->pipe_to = NULL;
    }

    if (ip->status == CM_RUNNING_ || ip->status == CM_SUSPENDED_) {
        CMUTIL_ProcessKill(proc);
    }
    // the reader thread polls the pipes, so it must be joined
    // before the pipes are closed.
    if (ip->reader) {
        CMCall(ip->reader, Join);
        ip->reader = NULL;
    }
    CMUTIL_ClosePipe(ip->inpipe);
    CMUTIL_ClosePipe(ip->outpipe);
    CMUTIL_ClosePipe(ip->errpipe);
    ip->inpipe = EMPTY_STREAM;
    ip->outpipe = EMPTY_STREAM;
    ip->errpipe = EMPTY_STREAM;
    if (ip->command) CMCall(ip->command, Destroy);
    if (ip->cwd) CMCall(ip->cwd, Destroy);
    if (ip->args) CMCall(ip->args, Destroy);
#if defined(MSWIN)
    if (ip->hproc) {
        CloseHandle(ip->hproc);
        ip->hproc = NULL;
    }
#endif
    ip->memst->Free(ip);
}

CMUTIL_STATIC ssize_t CMUTIL_ProcessReadErr(
    CMUTIL_Process *proc, CMUTIL_ByteBuffer *buf, size_t count)
{
    const CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    ssize_t ir;
    uint8_t bytes[1024];
    ssize_t nread = 0;
    while (nread < (ssize_t)count) {
        ssize_t remain = (ssize_t)count - nread;
        if (remain > sizeof(bytes)) remain = sizeof(bytes);
        ir = CMUTIL_ProcessReadPipe(ip->errpipe, bytes, remain);
        if (ir <= 0) break;
        CMCall(buf, AddBytes, bytes, (uint32_t)ir);
        nread += ir;
    }
    return nread;
}

static CMUTIL_Process g_cmutil_process = {
    CMUTIL_ProcessStart,
    CMUTIL_ProcessGetPid,
    CMUTIL_ProcessGetCommand,
    CMUTIL_ProcessGetWorkDir,
    CMUTIL_ProcessGetArgs,
    CMUTIL_ProcessGetEnv,
    CMUTIL_ProcessSuspend,
    CMUTIL_ProcessResume,
    CMUTIL_ProcessPipeTo,
    CMUTIL_ProcessWrite,
    CMUTIL_ProcessRead,
    CMUTIL_ProcessWait,
    CMUTIL_ProcessKill,
    CMUTIL_ProcessDestroy,
    CMUTIL_ProcessReadErr
};

CMUTIL_Process *CMUTIL_ProcessCreateInternal(
        CMUTIL_Mem *memst,
        const char *cwd,
        CMUTIL_Map *env,
        const char *command,
        CMUTIL_StringArray *args)
{
    if (command == NULL || memst == NULL) {
        CMLogErrorS("Invalid arguments. command or memst is NULL");
        return NULL;
    } else {
        CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)memst->Alloc(
            sizeof(CMUTIL_Process_Internal));
        if (ip == NULL) {
            CMLogErrorS("Failed to allocate memory.");
            return NULL;
        }
        memset(ip, 0x0, sizeof(CMUTIL_Process_Internal));
        memcpy(ip, &g_cmutil_process, sizeof(CMUTIL_Process));
        ip->memst = memst;
        if (cwd)
            ip->cwd = CMUTIL_StringCreateInternal(memst, 0, cwd);
        ip->env = env;
        ip->command = CMUTIL_StringCreateInternal(memst, 0, command);
        ip->args = args;
        ip->status = CM_IDLE_;
        ip->inpipe = EMPTY_STREAM;
        ip->outpipe = EMPTY_STREAM;
        ip->errpipe = EMPTY_STREAM;
        ip->type = CMProcStreamNone;
        if (CMLogIsEnabled(CMLogLevel_Debug)) {
            int i;
            CMUTIL_String *buf = CMUTIL_StringCreateInternal(memst, 1024, NULL);
            CMCall(buf, AddAnother, ip->command);
            for (i = 0; i < CMCall(args, GetSize); i++) {
                const CMUTIL_String *arg = CMCall(args, GetAt, i);
                CMCall(buf, AddChar, ' ');
                CMCall(buf, AddAnother, arg);
            }
            CMLogDebug("Creating process: %s", CMCall(buf, GetCString));
            CMCall(buf, Destroy);
        }
        return (CMUTIL_Process *)ip;
    }
}

CMUTIL_Process *CMUTIL_ProcessCreateEx(
        const char *cwd,
        CMUTIL_Map *env,
        const char *command,
        ...)
{
    CMUTIL_Process *res = NULL;
    CMUTIL_StringArray *args = CMUTIL_StringArrayCreateInternal(
        CMUTIL_GetMem(), 10);
    char *sarg = NULL;
    va_list vargs;
    va_start(vargs, command);
    sarg = va_arg(vargs, char*);
    while (sarg != NULL) {
        CMCall(args, AddCString, sarg);
        sarg = va_arg(vargs, char*);
    }
    va_end(vargs);
    res = CMUTIL_ProcessCreateInternal(
        CMUTIL_GetMem(), cwd, env, command, args);
    if (res == NULL)
        CMCall(args, Destroy);
    return res;
}

/* end of file */
