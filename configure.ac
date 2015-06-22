#
#  Copyright 2013 Renesas Solutions Corp.
#

# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([wayland-kms], [1.5.0],
        [http://www.renesas.com/], [wayland-kms])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])
#AM_MAINTAINER_MODE

# Initialize libtool
AC_PROG_LIBTOOL

# Check for wayland-scanner
AC_CHECK_PROG([WAYLAND_SCANNER], [wayland-scanner], [wayland-scanner], [no])
if test x"${WAYLAND_SCANNER}" == x"no" ; then
	AC_MSG_ERROR([Please install wayland-scanner.])
fi

# Obtain compiler/linker options for dependencies
PKG_CHECK_MODULES([WAYLAND_SERVER], [wayland-server])
PKG_CHECK_MODULES([WAYLAND_CLIENT], [wayland-client])
PKG_CHECK_MODULES([LIBDRM], [libdrm])

AC_CONFIG_FILES([Makefile wayland-kms.pc])
AC_OUTPUT
