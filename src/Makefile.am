bin_PROGRAMS = skamine

skamine_SOURCES = \
	main.c	\
	theme.c	\
	theme.h

skamine_CPPFLAGS = \
	-I$(top_srcdir)					\
	-DG_LOG_DOMAIN=\"ToddlerFun\"			\
	-DDATADIR=\""$(pkgdatadir)"\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

skamine_CFLAGS = \
	   $(GTK_CFLAGS)	\
	   $(RSVG_CFLAGS)	\
	   $(GST_CFLAGS)	\
	   $(WARN_CFLAGS)		\
	   $(AM_CFLAGS)

skamine_LDFLAGS = \
	$(AM_LDFLAGS)

skamine_LDADD = \
	$(GTK_LIBS)	\
	$(RSVG_LIBS)	\
	$(GST_LIBS)	\
	$(INTLLIBS)

-include $(top_srcdir)/git.mk
