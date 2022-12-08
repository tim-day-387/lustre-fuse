AC_PREREQ([2.59])

rm -f config.cache

AC_INIT(lustrefuse, 0.1)

AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE
AM_INIT_AUTOMAKE([ 1.9.2 ])

AC_PROG_CC

AC_CHECK_LIB(fuse, fuse_main , , [AC_MSG_ERROR(fuse not found)])
AC_CHECK_LIB(lustre, __liblustre_setup_ , , [AC_MSG_ERROR(liblustre not found)])
AC_CHECK_LIB(lustre, fuse_unlink , , [AC_MSG_ERROR(no.  Read the SYSIO_LABEL_NAMES part of the lustrefuse howto.)])
AC_CHECK_LIB(lustre, fuse_statvfs , , [AC_MSG_ERROR(no.  Read the STATVFS part of the lustrefuse howto.)])

# version 2.5 of fuse
CFLAGS="$CFLAGS -D_FILE_OFFSET_BITS=64 -DSYSIO_LABEL_NAMES=\"fuse_\""

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
