#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([cmutils library],
		[1.0.0],
		[Dennis Soungjin Park <xcomart@gmail.com>],
		[cmutils])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE()

enable_dlopen=yes

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h locale.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gethostbyname gethostname gettimeofday localtime_r memmove memset realpath select socket strcasecmp strchr strerror strrchr strstr])

AC_CONFIG_FILES([Makefile src/Makefile demo/Makefile
src/Makefile])
AC_OUTPUT
