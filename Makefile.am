## Process this file with automake to generate a Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = po

bin_PROGRAMS = skamine
skamine_SOURCES = main.c
skamine_CPPFLAGS = @GTK_CFLAGS@ @RSVG_CFLAGS@
skamine_LDADD = @GTK_LIBS@ @RSVG_LIBS@

CLEANFILES = *~