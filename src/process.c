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

CMUTIL_LogDefine("cmutils.process")

#if defined(MSWIN)
typedef HANDLE CMStream;
#define EMPTY_STREAM INVALID_HANDLE_VALUE
#else
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

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
    pid_t               pid;
    CMUTIL_Process      *pipe_from;
    CMUTIL_Process      *pipe_to;
    CMProcStatus_       status;
    CMStream            inpipe;
    CMStream            outpipe;
    CMStream            errpipe;
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
    Stream p1[2], p2[2], p3[2];

    // Create a pipe for the child process's STDIN.
    CMUTIL_CreatePipe(p1, true, false);
    // Create a pipe for the child process's STDOUT.
    CMUTIL_CreatePipe(p2, false, true);
    // Create a pipe for the child process's STDERR.
    CMUTIL_CreatePipe(p3, false, true);

    ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
    ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdInput = p1[0];
    siStartInfo.hStdOutput = p2[1];
    siStartInfo.hStdError = p3[1];
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    siStartInfo.wShowWindow = SW_HIDE;


    BOOL bSuccess = CreateProcess(nullptr,
                                  const_cast<TOLPX>(cmd.c_str()), // command line
                                  nullptr, // process security attributes
                                  nullptr, // primary thread security attributes
                                  TRUE, // handles are inherited
                                  CREATE_NO_WINDOW, // creation flags
                                  nullptr, // use parent's environment
                                  nullptr, // use parent's current directory
                                  &siStartInfo, // STARTUPINFO pointer
                                  &piProcInfo);     // receives PROCESS_INFORMATION
    if (!bSuccess) {
        SPDLOG_ERROR("CreateProcess failed");
        throw std::runtime_error("CreateProcess failed");
    }
    hproc = piProcInfo.hProcess;
    CloseHandle(piProcInfo.hThread);
    pid = piProcInfo.dwProcessId;
    // these pipes are used in child process no longer needed in parent

    infd = p1[1];
    outfd = p2[0];
    errfd = p3[0];
    closePipe(p1[0]);
    closePipe(p2[1]);
    closePipe(p3[1]);
}

#else

CMUTIL_STATIC CMBool CMUTIL_StartSubprocess(CMUTIL_Process *proc)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    int p1[2], p2[2], p3[2];

    if (ip->inpipe == EMPTY_STREAM && pipe(p1) == -1)
        goto err_pipe1;
    if (pipe(p2) == -1)
        goto err_pipe2;
    if (pipe(p3) == -1)
        goto err_pipe3;

    pid_t pid;
    if ((pid = fork()) == -1)
        goto err_fork;

    if (pid) {
        /* Parent process. */
        if (ip->inpipe == EMPTY_STREAM) {
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
    if (ip->inpipe == EMPTY_STREAM) {
        dup2(p1[0], STDIN_FILENO);
        close(p1[0]);
        close(p1[1]);
    } else
    {
        dup2(ip->inpipe, STDIN_FILENO);
        close(ip->inpipe);
    }
    dup2(p2[1], STDOUT_FILENO);
    dup2(p3[1], STDERR_FILENO);
    close(p2[0]);
    close(p2[1]);
    close(p3[0]);
    close(p3[1]);
    {
        // this is child process
        char **args = NULL;
        if (CMCall(ip->args, GetSize) > 0) {
            args = CMAlloc((CMCall(ip->args, GetSize) + 1) * sizeof(char *));
            for (size_t i = 0; i < CMCall(ip->args, GetSize); i++) {
                const CMUTIL_String *arg = CMCall(ip->args, GetAt, i);
                args[i] = (char*)CMCall(arg, GetCString);
            }
            args[CMCall(ip->args, GetSize)] = NULL;
        }
        execvp(CMCall(ip->command, GetCString), args);
   }
    // cannot be here in general case, below codes are error handler
    /* Error occurred. */
    fprintf(stderr, "error running %s: %s\n",
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



CMUTIL_STATIC CMBool CMUTIL_ProcessStart(
    CMUTIL_Process *proc, CMProcStreamType type)
{
    CMUTIL_Process_Internal *ip = (CMUTIL_Process_Internal *)proc;
    if (ip->status != CM_IDLE_ && ip->status != CM_EXITED_) {
        CMLogErrorS("Process is already running.");
        return CMFalse;
    }

#if defined(MSWIN)
#else
    const pid_t pid = fork();
    if (pid == -1) return CMFalse;
    if (pid == 0) {
        // this is child process
        char **args = NULL;
        if (CMCall(ip->args, GetSize) > 0) {
            args = CMAlloc((CMCall(ip->args, GetSize) + 1) * sizeof(char *));
            for (size_t i = 0; i < CMCall(ip->args, GetSize); i++) {
                const CMUTIL_String *arg = CMCall(ip->args, GetAt, i);
                args[i] = (char*)CMCall(arg, GetCString);
            }
            args[CMCall(ip->args, GetSize)] = NULL;
        }
        execvp(CMCall(ip->command, GetCString), args);
        exit(1);
    } else {
        // this is parent process
        ip->status = CM_RUNNING_;
        ip->pid = pid;
    }
#endif

    return CMTrue;
}