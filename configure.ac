AC_INIT(eplay, 0.0.0)
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(configure.ac)
AM_CONFIG_HEADER(config.h)
AC_PROG_CC
AM_INIT_AUTOMAKE(1.6 dist-bzip2)
PKG_CHECK_MODULES([EFL], [elementary eeze ecore-input-evas ecore-input])
PKG_CHECK_MODULES(GST, gstreamer-0.10 >= 0.10.0)
PKG_CHECK_MODULES(DRM, [libdrm])
PKG_CHECK_MODULES(DCE, [libdce])
PKG_CHECK_MODULES(UDEV, [libudev])
PKG_CHECK_MODULES(ALSA, [alsa])
PKG_CHECK_MODULES(XKB, [xkbcommon])

if test "x$GCC" = "xyes"; then
	GCC_CFLAGS="-Werror -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -g -fvisibility=hidden -Wstrict-prototypes -Wmissing-prototypes"
fi
AC_SUBST(GCC_CFLAGS)

AC_OUTPUT(Makefile)
