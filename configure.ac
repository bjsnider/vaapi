# Copyright (c) 2007 Intel Corporation. All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sub license, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice (including the
# next paragraph) shall be included in all copies or substantial portions
# of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
# IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
# ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# VA-API version
# - increment major for any ABI change (which shall not occur!)
# - increment minor for any interface change (e.g. new/modified function)
# - increment micro for any other change (new flag, new codec definition, etc.)
# - reset micro version to zero when minor version is incremented
# - reset minor version to zero when major version is incremented
m4_define([va_api_major_version], [0])
m4_define([va_api_minor_version], [32])
m4_define([va_api_micro_version], [0])

m4_define([va_api_version],
          [va_api_major_version.va_api_minor_version.va_api_micro_version])

# libva package version number, (as distinct from shared library version)
# XXX: we want the package version to remain at 1.0.x for VA-API 0.32.y
#
# - major version is automatically generated from VA-API major version
# - minor version is automatically generated from VA-API minor version
# - increment micro for any library release
# - reset micro version to zero when VA-API major or minor version is changed
m4_define([libva_major_version], [m4_eval(va_api_major_version + 1)])
m4_define([libva_minor_version], [m4_eval(va_api_minor_version - 32)])
m4_define([libva_micro_version], [15])
m4_define([libva_pre_version],   [0])

m4_define([libva_version],
          [libva_major_version.libva_minor_version.libva_micro_version])
m4_if(libva_pre_version, [0], [], [
m4_append([libva_version], libva_pre_version, [.pre])
])

# libva library version number (generated, do not change)
# XXX: we want the SONAME to remain at libva.so.1 for VA-API major == 0
#
# The library name is generated libva.<x>.<y>.0 where
# <x> = VA-API major version + 1
# <y> = 100 * VA-API minor version + VA-API micro version
#
# For example:
# VA-API 0.32.0 generates libva.so.1.3200.0
# VA-API 0.34.1 generates libva.so.1.3401.0
# VA-API 1.2.13 generates libva.so.2.213.0
m4_define([libva_interface_bias], [m4_eval(va_api_major_version + 1)])
m4_define([libva_interface_age],  [0])
m4_define([libva_binary_age],
          [m4_eval(100 * va_api_minor_version + va_api_micro_version - libva_interface_age)])

m4_define([libva_lt_current],
          [m4_eval(100 * va_api_minor_version + va_api_micro_version + libva_interface_bias)])
m4_define([libva_lt_revision],
          [m4_eval(libva_interface_age)])
m4_define([libva_lt_age],
          [m4_eval(libva_binary_age - libva_interface_age)])

# libdrm minimun version requirement
m4_define([libdrm_version], [2.4])

AC_PREREQ(2.57)
AC_INIT([libva], [libva_version], [waldo.bastian@intel.com], libva)
AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([dist-bzip2])

AM_CONFIG_HEADER([config.h])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

VA_API_MAJOR_VERSION=va_api_major_version
VA_API_MINOR_VERSION=va_api_minor_version
VA_API_MICRO_VERSION=va_api_micro_version
VA_API_VERSION=va_api_version
AC_SUBST(VA_API_MAJOR_VERSION)
AC_SUBST(VA_API_MINOR_VERSION)
AC_SUBST(VA_API_MICRO_VERSION)
AC_SUBST(VA_API_VERSION)

LIBVA_MAJOR_VERSION=libva_major_version
LIBVA_MINOR_VERSION=libva_minor_version
LIBVA_MICRO_VERSION=libva_micro_version
LIBVA_VERSION=libva_version
AC_SUBST(LIBVA_MAJOR_VERSION)
AC_SUBST(LIBVA_MINOR_VERSION)
AC_SUBST(LIBVA_MICRO_VERSION)
AC_SUBST(LIBVA_VERSION)

LIBVA_LT_CURRENT=libva_lt_current
LIBVA_LT_REV=libva_lt_revision
LIBVA_LT_AGE=libva_lt_age
LIBVA_LT_VERSION="$LIBVA_LT_CURRENT:$LIBVA_LT_REV:$LIBVA_LT_AGE"
LIBVA_LT_LDFLAGS="-version-info $LIBVA_LT_VERSION"
AC_SUBST(LIBVA_LT_VERSION)
AC_SUBST(LIBVA_LT_LDFLAGS)

AC_ARG_ENABLE(glx,
              [AC_HELP_STRING([--enable-glx],
                              [build with OpenGL for X11 support])],
              [], [enable_glx=yes])

AC_ARG_ENABLE(dummy-driver,
              [AC_HELP_STRING([--enable-dummy-driver],
                              [build dummy video driver])],
              [], [enable_dummy_driver=yes])
AM_CONDITIONAL(BUILD_DUMMY_DRIVER, test x$enable_dummy_driver = xyes)

AC_ARG_ENABLE(dummy-backend,
              [AC_HELP_STRING([--enable-dummy-backend],
                              [build dummy libva backend])],
              [], [enable_dummy_backend=no])

AC_ARG_WITH(drivers-path,
	    [AC_HELP_STRING([--with-drivers-path=[[path]]], [drivers path])],,
	    [with_drivers_path="$libdir/dri"])

LIBVA_DRIVERS_PATH="$with_drivers_path"
AC_SUBST(LIBVA_DRIVERS_PATH)

AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_CXX

AC_HEADER_STDC
AC_SYS_LARGEFILE

PKG_CHECK_MODULES([X11], [x11])
PKG_CHECK_MODULES([XEXT],[xext])
PKG_CHECK_MODULES([XFIXES], [xfixes])

# Check for recent enough DRM
LIBDRM_VERSION=libdrm_version
PKG_CHECK_MODULES([DRM], [libdrm >= $LIBDRM_VERSION])
AC_SUBST(LIBDRM_VERSION)

if test x$enable_dummy_backend = xyes; then
    PKG_CHECK_MODULES([UDEV], [libudev], [libudev=yes], [libudev=no])
fi
if test x$libudev = xno; then
   enable_dummy_backend=no
fi
AM_CONDITIONAL(BUILD_DUMMY_BACKEND, test x$enable_dummy_backend = xyes)

# Check for OpenGL (X11)
USE_GLX="no"
GL_DEPS_CFLAGS=""
GL_DEPS_LIBS=""
if test x$enable_glx = xyes; then
    AC_CHECK_HEADERS([GL/gl.h])
    AC_CHECK_HEADERS([GL/glx.h])
    AC_CHECK_LIB(GL, glXCreateContext, [
        USE_GLX="yes"
        GL_DEPS_LIBS="-lX11 -lGL"
    ])
fi
AC_SUBST(GL_DEPS_CFLAGS)
AC_SUBST(GL_DEPS_LIBS)
AM_CONDITIONAL(USE_GLX, test "$USE_GLX" = "yes")

# TBD: Check for EGL
USE_EGL="yes"
AM_CONDITIONAL(USE_EGL, test "$USE_EGL" = "yes")

# We only need the headers, we don't link against the DRM libraries
LIBVA_CFLAGS="$DRM_CFLAGS"
AC_SUBST(LIBVA_CFLAGS)
AC_SUBST(LIBVA_LIBS)

pkgconfigdir=${libdir}/pkgconfig
AC_SUBST(pkgconfigdir)

LIBVA_DISPLAY=x11
libvacorelib=libva.la
libvabackendlib=libva-$LIBVA_DISPLAY.la
AC_SUBST([libvacorelib])
AC_SUBST([libvabackendlib])

AC_OUTPUT([
	Makefile
	debian.upstream/Makefile
	va/Makefile
	va/va_version.h
	va/x11/Makefile
	va/glx/Makefile
	va/egl/Makefile
	va/dummy/Makefile
	dummy_drv_video/Makefile
	test/Makefile
	test/basic/Makefile
	test/decode/Makefile
	test/putsurface/Makefile
	test/encode/Makefile
	test/vainfo/Makefile
	pkgconfig/libva-egl.pc
	pkgconfig/libva-glx.pc
	pkgconfig/libva-tpi.pc
	pkgconfig/libva-x11.pc
	pkgconfig/libva.pc
        pkgconfig/Makefile
])

# Print a small summary

echo ""
echo "libva - ${LIBVA_VERSION} (VA-API ${VA_API_VERSION})"
echo ""

echo " � Global :"
echo "     Prefix: ${prefix}"
echo ""

AS_IF([test x$enable_dummy_driver = xyes], [DRIVERS="dummy $DRIVERS"])

echo " � Drivers: ${DRIVERS}"

AS_IF([test x$USE_GLX = xyes], [BACKENDS="glx $BACKENDS"])
AS_IF([test x$USE_EGL = xyes], [BACKENDS="egl $BACKENDS"])

BACKENDS="x11 $BACKENDS"
AS_IF([test x$enable_dummy_backend = xyes], [BACKENDS="dummy 
$BACKENDS"])

echo " � Winsys : ${BACKENDS}"

