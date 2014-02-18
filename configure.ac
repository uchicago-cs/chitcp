#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([chitcp], [1.0], [do-not-email@example.org])
AC_CONFIG_SRCDIR([src/chitcpd/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([subdir-objects])
AM_SILENT_RULES([yes])


# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

LT_INIT


# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_OFF_T
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([bzero gethostbyname gettimeofday memset socket strdup])
PKG_CHECK_MODULES(CHECK, [check >= 0.9.8],,[AC_MSG_RESULT([no, testing is disabled])]) 

PKG_CHECK_MODULES([PROTOBUF_C], [libprotobuf-c >= 0.15],,[AC_MSG_ERROR([
  ERROR: libprotobuf-c >= 0.15 is required

  If libprotobuf-c is not available as an installable package
  on your system, you can download it at:
  
  http://code.google.com/p/protobuf-c/
  
  Note that you also need to install Google's protobuf library
  (http://code.google.com/p/protobuf/) before installing 
  libprotobuf-c.
  ])])
AC_CHECK_PROG([PROTOC_C], [protoc-c], [protoc-c])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
