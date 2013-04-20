bin_PROGRAMS = toddlerfun

toddlerfun_SOURCES = \
	main.c	\
	theme.c	\
	theme.h

toddlerfun_CPPFLAGS = \
	-I$(top_srcdir)					\
	-DG_LOG_DOMAIN=\"ToddlerFun\"			\
	-DDATADIR=\""$(pkgdatadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\"	\
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

toddlerfun_CFLAGS = \
	   $(GTK_CFLAGS)	\
	   $(RSVG_CFLAGS)	\
	   $(GST_CFLAGS)	\
	   $(WARN_CFLAGS)		\
	   $(AM_CFLAGS)

toddlerfun_LDFLAGS = \
	$(AM_LDFLAGS)

toddlerfun_LDADD = \
	$(GTK_LIBS)	\
	$(RSVG_LIBS)	\
	$(GST_LIBS)	\
	$(INTLLIBS)

