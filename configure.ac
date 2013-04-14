AC_PREREQ(2.60)
AC_INIT([skamine], [0.1], [simon@helgo.net])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.9.6 -Wall -Werror dist-bzip2])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
# Compiling sources with per-target flags requires AM_PROG_CC_C_O
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AM_PATH_GTK_3_0([3.0.0],,AC_MSG_ERROR([Gtk+ 3.0.0 or higher required.]))

PKG_CHECK_MODULES([RSVG], [librsvg-2.0],,AC_MSG_ERROR([librsvg 2.0 required.]))

PKG_CHECK_MODULES([GST], [gstreamer-0.10],,AC_MSG_ERROR([gstreamer 0.10 required.]))

dnl Define myprefix depending on wether our user gives us one or not
if test "x${prefix}" = "xNONE"; then
  myprefix=${ac_default_prefix}
else
  myprefix=${prefix}
fi

#DATADIRNAME="share"
#PACKAGE_DATA_DIR="data"
#AC_SUBST(PACKAGE_DATA_DIR)
AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${myprefix}/share/toddlerfun", [Toddler Fun data directory])

# AM_GNU_GETTEXT

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
        Makefile
	po/Makefile
])
AC_OUTPUT
