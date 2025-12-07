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

#ifndef PLATFORMS_H
#define PLATFORMS_H

#if defined(__CYGWIN__)     /* CYGWIN implements a UNIX API */
# undef WIN
# undef _WIN
# undef _WIN32
# undef _WIN64
# undef __WIN__
# if !defined(CYGWIN)
#  define CYGWIN
# endif
#endif

#if (defined(_WIN32) || defined(_WIN64)) && !defined(MSWIN)
# define MSWIN
# define CMUTIL_SO_EXT      "dll"
# define CMUTIL_LIB_ENV     "PATH"
# define CMUTIL_LIB_DFLT    ""
# define CMUTIL_PATH_SEPS   ";"
#endif

/* dynamic library of cygwin is DLL */
#if defined(__CYGWIN__)
# define CMUTIL_SO_EXT      "dll"
#endif

/* Defined if OS is Linux and compiler is gcc. */
#if defined(__linux__) && !defined(LINUX)
# define LINUX
#endif

/* Defined if OS is Solaris or SunOS. */
#if defined(sun) && !defined(SUNOS)
# define SUNOS
#endif

/* Defined if the operating system is AIX version 3.2 or higher. */
#if defined(_AIX32) && !defined(AIX)
# define AIX
#endif

/* Defined on all HP-UX systems. */
#if defined(__hpux) && !defined(HPUX)
# define HPUX
# define CMUTIL_SO_EXT      "sl"
# define CMUTIL_LIB_ENV     "SHLIB_PATH"
#endif

/* Defined on all Apple Systems(OS X, iOS, MacOS) */
#if defined(__APPLE__) && !defined(APPLE)
# define APPLE
# define CMUTIL_SO_EXT      "dylib"
# define CMUTIL_LIB_ENV     "DYLD_LIBRARY_PATH"
#endif

#if !defined(CMUTIL_PATH_SEPS)
# define CMUTIL_PATH_SEPS   ":"
#endif


#if !defined(CMUTIL_SO_EXT)
# define CMUTIL_SO_EXT      "so"
#endif

#if !defined(CMUTIL_LIB_ENV)
# define CMUTIL_LIB_ENV     "LD_LIBRARY_PATH"
#endif

#if !defined(CMUTIL_LIB_DFLT)
# define CMUTIL_LIB_DFLT    "/lib:/usr/lib:/usr/local/lib"
#endif


#if !defined(MSWIN)
# include <dlfcn.h>
# include <dirent.h>
# include <unistd.h>
#endif

#if !defined(_MSC_VER)
# define DeleteFile unlink
#endif


#if defined(MSWIN)
# ifdef UNICODE
#  undef UNICODE
# endif
# if !defined(WINVER)
#  define WINVER 0x0600
# endif
# if !defined(_WIN32_WINNT)
#  define _WIN32_WINNT WINVER
# endif
# include <winsock2.h>
# include <windows.h>
# include <time.h>
# if !defined(SHUT_RDWR)
#  define SHUT_RDWR SD_BOTH
# endif
typedef int socklen_t;
// gethostbyname is thread safe in windows
# define gethostbyname_r(n,p,b,s,l,e)   \
    memcpy(p,gethostbyname(n),sizeof(struct hostent))
# define poll               WSAPoll
# define localtime_r(a,b)   memcpy(b, localtime(a), sizeof(struct tm))
# define GETPID             (pid_t)GetCurrentProcessId
# define S_CRLF             "\r\n"
# define USLEEP(x)          Sleep((x) / 1000)
#else
# include <unistd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# if defined(SUNOS)
#  include <sys/filio.h>
# else
#  include <sys/ioctl.h>
# endif

# define GETPID             (pid_t)getpid
# define S_CRLF             "\n"
# define USLEEP             usleep
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#if defined(MSWIN)
# if _WIN64
#  define ARCH64
# else
#  define ARCH32
# endif
#else
# if __x86_64__ || __ppc64__ || __LP64__
#  define ARCH64
# else
#  define ARCH32
# endif
#endif

#endif // PLATFORMS_H

