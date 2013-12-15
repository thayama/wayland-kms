lib_LTLIBRARIES = libwayland-kms.la

AM_CFLAGS = -I$(top_srcdir)/include \
	    $(DEFINES) \
	    $(WAYLAND_CFLAGS) 

BUILT_SOURCES = wayland-kms-protocol.c \
		wayland-kms-server-protocol.h \
		wayland-kms-client-protocol.h
CLEANFILES = $(BUILT_SOURCES)

libwayland_kms_la_SOURCES = \
	wayland-kms.c \
	wayland-kms-auth.c \
	wayland-kms-protocol.c \
	$(BUILD_SOURCES)

libwayland_kms_la_CFLAGS = \
	@WAYLAND_SERVER_CFLAGS@ \
	@LIBDRM_CFLAGS@

libwayland_kms_la_LIBADD = \
	@WAYLAND_SERVER_LIBS@ \
	@LIBDRM_LIBS@

extdir = $(includedir)
ext_HEADERS = \
	wayland-kms.h \
	wayland-kms-server-protocol.h \
	wayland-kms-client-protocol.h

%-protocol.c : %.xml
	$(WAYLAND_SCANNER) code < $< > $@

%-server-protocol.h : %.xml
	$(WAYLAND_SCANNER) server-header < $< > $@

%-client-protocol.h : %.xml
	$(WAYLAND_SCANNER) client-header < $< > $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = wayland-kms.pc
