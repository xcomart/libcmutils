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
#include <sys/poll.h>

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
#ifdef _WIN32
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

    // Create a pipe for the child process's STDIN / STDOUT / STDERR.
    if (!CreatePipe(pipe, pipe+1, &saAttr, 4096)) {
        CMLogErrorS("CreatePipe failed");
        return CMFalse;
    }
    // Ensure the read handle to the pipe is not inherited.
    if (!inherit_read && !SetHandleInformation(pipe[0], HANDLE_FLAG_INHERIT, 0)) {
        CMLogErrorS("SetHandleInformation failed");
        return CMFalse;
    }
    // Ensure the write handle to the pipe is not inherited.
    if (!inherit_write && !SetHandleInformation(pipe[1], HANDLE_FLAG_INHERIT, 0)) {
        CMLogErrorS("SetHandleInformation failed");
        return CMFalse;
    }
    return CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_StartSubprocess(CMUTIL_Process *proc)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;

    // SECURITY_ATTRIBUTES saAttr;
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    // HANDLE cin = nullptr, cout = nullptr, cerr = nullptr;
    CMStream p1[2], p2[2], p3[2];

    CMUTIL_String *cmd = NULL;
    int i;

    // Create a pipe for the child process's STDIN.
    CMUTIL_CreatePipe(p1, CMTrue, CMFalse);
    // Create a pipe for the child process's STDOUT.
    CMUTIL_CreatePipe(p2, CMFalse, CMTrue);
    // Create a pipe for the child process's STDERR.
    CMUTIL_CreatePipe(p3, CMFalse, CMTrue);

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
        CMCall(cmd, AddAnother, arg);
    }

    BOOL bSuccess = CreateProcess(NULL,
                                  (LPTSTR)CMCall(cmd, GetCString), // command line
                                  NULL, // process security attributes
                                  NULL, // primary thread security attributes
                                  TRUE, // handles are inherited
                                  CREATE_NO_WINDOW, // creation flags
                                  NULL, // use parent's environment
                                  NULL, // use parent's current directory
                                  &siStartInfo, // STARTUPINFO pointer
                                  &piProcInfo);     // receives PROCESS_INFORMATION
    CMCall(cmd, Destroy);
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

CMUTIL_STATIC CMBool CMUTIL_StartSubprocess(CMUTIL_Process *proc)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    CMStream p1[2], p2[2], p3[2];
    int i;
    pid_t pid;
    CMUTIL_StringArray *keys = NULL;

    if (ip->type & CMProcStreamWrite && pipe(p1) == -1)
        goto err_pipe1;
    if (pipe(p2) == -1)
        goto err_pipe2;
    if (pipe(p3) == -1)
        goto err_pipe3;

    if ((pid = fork()) == -1) {
        CMLogErrorS("fork failed %d:%s", errno, strerror(errno));
        goto err_fork;
    }

    if (pid) {
        /* Parent process. */
        ip->pid = pid;
        if (ip->type & CMProcStreamWrite) {
            ip->inpipe = p1[1];  // write to process stdin
            close(p1[0]);
        }
        ip->outpipe = p2[0]; // read from process stdout
        ip->errpipe = p3[0]; // read from process stderr
        close(p2[1]);
        close(p3[1]);
        return CMTrue;
    }
    /* Child process. */
    if (ip->type & CMProcStreamWrite) {
        dup2(p1[0], STDIN_FILENO);
        close(p1[0]);
        close(p1[1]);
    } else {
        dup2(ip->inpipe, STDIN_FILENO);
        close(ip->inpipe);
    }
    dup2(p2[1], STDOUT_FILENO);
    dup2(p3[1], STDERR_FILENO);
    close(p2[0]);
    close(p2[1]);
    close(p3[0]);
    close(p3[1]);

    // set environment variables
    if (ip->env) {
        keys = CMCall(ip->env, GetKeys);
        for (i = 0; i < CMCall(keys, GetSize); i++) {
            const CMUTIL_String *key = CMCall(keys, GetAt, i);
            const char *skey = CMCall(key, GetCString);
            const char *value = CMCall(ip->env, Get, skey);
            setenv(skey, value, 1);
        }
        CMCall(keys, Destroy);
        if (CMLogIsEnabled(CMLogLevel_Debug)) {
            CMUTIL_String *buf = CMUTIL_StringCreate();
            CMCall(ip->env, PrintTo, buf, NULL);
            CMLogDebug("Environment variables: %s", CMCall(buf, GetCString));
            CMCall(buf, Destroy);
        }
    }
    if (ip->cwd) {
        CMLogDebug("Working directory: %s", CMCall(ip->cwd, GetCString));
        if (chdir(CMCall(ip->cwd, GetCString)) != 0) {
            CMLogErrorS("chdir failed %d:%s", errno, strerror(errno));
        }
    }
    {
        // this is child process
        char **args = CMAlloc((CMCall(ip->args, GetSize) + 2) * sizeof(char *));
        memset(args, 0, (CMCall(ip->args, GetSize) + 2) * sizeof(char *));
        args[0] = (char*)CMCall(ip->command, GetCString);
        if (CMCall(ip->args, GetSize) > 0) {
            for (i = 0; i < CMCall(ip->args, GetSize); i++) {
                const CMUTIL_String *arg = CMCall(ip->args, GetAt, i);
                args[i+1] = (char*)CMCall(arg, GetCString);
            }
        }
        if (execvp(CMCall(ip->command, GetCString), args) != 0) {
            CMLogErrorS("execvp failed %d:%s", errno, strerror(errno));
        }
        CMFree(args);
    }
    // cannot be here in general cases, the below codes are error handler
    /* Error occurred. */
    CMLogErrorS("error running %s: %s",
        CMCall(ip->command, GetCString), strerror(errno));
    abort();

err_fork:
    close(p3[1]);
    close(p3[0]);
err_pipe3:
    close(p2[1]);
    close(p2[0]);
err_pipe2:
    if (ip->inpipe == EMPTY_STREAM) {
        close(p1[1]);
        close(p1[0]);
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
            (ssize_t)size-read_size, &rsz, NULL)) {
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
            (ssize_t)size-write_size, &wsz, NULL)) {
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
        CMCall(ip->pipe_to, Write, buf, size);
    } else {
        fwrite(buf, 1, size, stdout);
    }
    CMCall(bbuf, Clear);
}

CMUTIL_STATIC void CMUTIL_ProcessFlushStderr(
    CMUTIL_Process_Internal *ip, CMUTIL_ByteBuffer *bbuf)
{
    const uint8_t *buf = CMCall(bbuf, GetBytes);
    const size_t size = CMCall(bbuf, GetSize);
    fwrite(buf, 1, size, stderr);
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
    int fd_cnt = 2;
#if defined(MSWIN)
    HANDLE pfd[2];
    HANDLE *tpfd = pfd;
    if (ip->type & CMProcStreamRead) {
        fd_cnt--;
        tpfd = pfd + 1;
    }
    while (closecnt < 3) {
        uint8_t c;
        DWORD rc = WaitForMultipleObjects(fd_cnt, tpfd, FALSE, 0);
        if (rc >= WAIT_OBJECT_0 && rc <= WAIT_OBJECT_0 + fd_cnt - 1) {
            if (rc == WAIT_OBJECT_0 && fd_cnt == 2) {
                ssize_t sz = CMUTIL_ProcessReadPipe(ip->outpipe, &c, 1);
                if (sz == 1) {
                    CMCall(stdbuf, AddByte, c);
                    if (CMCall(stdbuf, GetSize) == 1024) {
                        CMUTIL_ProcessFlushStdout(ip, stdbuf);
                    }
                } else if (sz < 0) {
                    CMLogError("read stdout error: %s", strerror(errno));
                    closecnt |= 1;
                }
            } else if ((rc == WAIT_OBJECT_0 + 1 && fd_cnt == 2) ||
                (rc == WAIT_OBJECT_0 && fd_cnt == 1)) {
                ssize_t sz = CMUTIL_ProcessReadPipe(ip->errpipe, &c, 1);
                if (sz == 1) {
                    CMCall(errbuf, AddByte, c);
                    if (CMCall(errbuf, GetSize) == 1024) {
                        CMUTIL_ProcessFlushStderr(ip, errbuf);
                    }
                } else if (sz < 0) {
                    CMLogError("read stderr error: %s", strerror(errno));
                    closecnt |= 2;
                }
            }
        } else if (rc == WAIT_FAILED) {
            CMLogError("read stdout error: %s", strerror(errno));
            closecnt = 3;
        } else if (rc == WAIT_TIMEOUT) {
            USLEEP(10000);
        }
    }
#else
    struct pollfd pfd[2];
    struct pollfd *tpfd = pfd;
    memset(pfd, 0x0, sizeof(struct pollfd)*2);

    if (ip->type & CMProcStreamRead) {
        fd_cnt--;
        tpfd = pfd + 1;
    }
    pfd[0].fd = ip->outpipe;
    pfd[1].fd = ip->errpipe;
    pfd[0].events = POLLIN;
    pfd[1].events = POLLIN;

    while (closecnt < 3) {
        uint8_t c;
        const int rc = poll(tpfd, fd_cnt, 0);
        if (rc > 0) {
            ssize_t n;
            if (pfd[0].revents) {
                n = CMUTIL_ProcessReadPipe(ip->outpipe, &c, 1);
                if (n == 1) {
                    CMCall(stdbuf, AddByte, c);
                    if (CMCall(stdbuf, GetSize) == 1024) {
                        CMUTIL_ProcessFlushStdout(ip, stdbuf);
                    }
                } else if (n < 0) {
                    CMLogError("read stdout error: %s", strerror(errno));
                    closecnt |= 1;
                } else if (pfd[0].revents & POLLHUP) {
                    closecnt |= 1;
                }
                pfd[0].revents = 0;
                if (closecnt & 1 && closecnt < 3) {
                    fd_cnt = 1;
                    tpfd = pfd + 1;
                }
            }
            if (pfd[1].revents) {
                n = CMUTIL_ProcessReadPipe(ip->errpipe, &c, 1);
                if (n == 1) {
                    CMCall(errbuf, AddByte, c);
                    if (CMCall(errbuf, GetSize) == 1024) {
                        CMUTIL_ProcessFlushStderr(ip, errbuf);
                    }
                } else if (n < 0) {
                    CMLogError("read stderr error: %s", strerror(errno));
                    closecnt |= 2;
                } else if (pfd[1].revents & POLLHUP) {
                    closecnt |= 2;
                }
                pfd[1].revents = 0;
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
    while (nread < count) {
        ssize_t remain = (ssize_t)count - nread;
        if (remain > sizeof(bytes)) remain = sizeof(bytes);
        ir = CMUTIL_ProcessReadPipe(ip->outpipe, bytes, remain);
        if (ir <= 0) break;
        CMCall(buf, AddBytes, bytes, ir);
        nread += ir;
    }
    return nread;
}

CMUTIL_STATIC int CMUTIL_ProcessWait(CMUTIL_Process *proc, long millis)
{
    int status;
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;

    if (ip->status == CM_RUNNING_) {
#if defined(MSWIN)
        if (ip->hproc) {
            DWORD exit_code;
            WaitForSingleObject(ip->hproc, millis < 0 ? INFINITE : millis);
            GetExitCodeProcess(ip->hproc, &exit_code);
            status = (int)exit_code;
        } else {
            status = -1;
        }
#else
        long counts = millis / 100;
        long step = 10000;
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
            if (pid > 0) break;
            usleep(step);
        }
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
#endif
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
        if (ip->hproc != INVALID_HANDLE_VALUE) {
            TerminateProcess(ip->hproc, PROCESS_TERMINATE);
            ip->hproc = INVALID_HANDLE_VALUE;
        }
#else
        kill(ip->pid, SIGTERM);
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
    CMUTIL_ClosePipe(ip->inpipe);
    CMUTIL_ClosePipe(ip->outpipe);
    CMUTIL_ClosePipe(ip->errpipe);
    if (ip->reader) CMCall(ip->reader, Join);
    if (ip->command) CMCall(ip->command, Destroy);
    if (ip->cwd) CMCall(ip->cwd, Destroy);
    if (ip->args) CMCall(ip->args, Destroy);
#if defined(MSWIN)
    if (ip->hproc != INVALID_HANDLE_VALUE) CloseHandle(ip->hproc);
#endif
    ip->memst->Free(ip);
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
    CMUTIL_ProcessDestroy
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
