AUTOMAKE_OPTIONS = 1.4 foreign
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.h.in configure depcomp install-sh missing

AM_CPPFLAGS = -I$(top_srcdir)

bin_PROGRAMS = eplay

AM_CFLAGS = $(AM_CPPFLAGS) $(GCC_CFLAGS)

eplay_SOURCES = main.c gui.c output.c input.c kmsplayer.c mixer.c media.c eplay.h
eplay_LDADD = @EFL_LIBS@ @DRM_LIBS@ @DCE_LIBS@ @GST_LIBS@ @UDEV_LIBS@ @ALSA_LIBS@
eplay_CFLAGS = @EFL_CFLAGS@ @DRM_CFLAGS@ @DCE_CFLAGS@ @GST_CFLAGS@ @UDEV_CFLAGS@ @ALSA_CFLAGS@ $(AM_CFLAGS)
