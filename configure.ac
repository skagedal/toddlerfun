AC_PREREQ(2.61)
AC_INIT([toddlerfun], [0.1.0], [simon@helgo.net])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.10 foreign -Wall -Werror dist-bzip2  no-dist-gzip])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Check for programs
AC_PROG_CC

AM_PROG_CC_C_O

# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT

# *****
# GNOME
# *****

GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])

# ***************************
# Check for required packages
# ***************************

LIBGTK_REQUIRED=3.0.0

PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= $LIBGTK_REQUIRED])

PKG_CHECK_MODULES([RSVG], [librsvg-2.0],,AC_MSG_ERROR([librsvg 2.0 required.]))
PKG_CHECK_MODULES([GST], [gstreamer-0.10],,AC_MSG_ERROR([gstreamer 0.10 required.]))

# ********************
# Internationalisation
# ********************

IT_PROG_INTLTOOL([0.40.0])

AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

AC_SUBST([GETTEXT_PACKAGE], [toddlerfun])
AC_DEFINE([GETTEXT_PACKAGE], [PACKAGE_TARNAME], [Define to the gettext package name.])

AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

AC_CONFIG_FILES([
Makefile
src/Makefile
po/Makefile.in
defaulttheme/Makefile
])

AC_OUTPUT
