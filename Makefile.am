## Process this file with automake to generate a Makefile.in

SUBDIRS = src po defaulttheme

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

desktopdir = $(datadir)/applications
desktop_in_files = toddlerfun.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

# distuninstallcheck_listfiles = find . -type f -print | grep -v scrollkeeper

EXTRA_DIST = \
	autogen.sh		\
	$(desktop_in_files)	

CLEANFILES = *~ \
	$(desktop_DATA)

DISTCLEANFILES =

MAINTAINERCLEANFILES = \
	ChangeLog \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/autoscan.log \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/configure \
	$(srcdir)/configure.scan \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	$(srcdir)/omf.make \
	$(srcdir)/xmldocs.make \
	$(srcdir)/gtk-doc.make \
	`find "$(srcdir)" -type f -name Makefile.in -print` \
	$(NULL)

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

