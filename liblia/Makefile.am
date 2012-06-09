MAINTAINERCLEANFILES = \
        Makefile.in \
	lia-marshal.c \
	lia-marshal.h


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = @PRJ_API_NAME@.pc

library_includedir=$(includedir)/@PRJ_API_NAME@
library_include_HEADERS = lia.h

htmldatadir = $(datadir)/lia/webview
sysconfigdir = $(sysconfdir)/lia

sysconfig_DATA = \
	dbus-daemon-priv.conf \
	dbus-daemon-prot.conf \
	dbus-daemon-pub.conf

CLEANFILES = \
	*~ @PRJ_API_NAME@.pc

AM_CFLAGS = $(GLIB_CFLAGS) -Wall \
	-DPKGDATADIR="\"$(pkgdatadir)\"" \
	-DENABLE_TESTS="\"$(enable_tests)\"" \
        -DSYS_PROG_DIR="\"$(sbindir)\"" \
        -DSYS_CONF_DIR="\"$(sysconfigdir)\"" \
	-DHTML_DATA_DIR="\"$(htmldatadir)\""

if ENABLE_DEBUG
AM_CFLAGS += -Werror -g3 -O0 -ggdb
else
AM_CFLAGS += -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS
endif

lia-marshal.h: lia-marshal.list
	glib-genmarshal --header \
		--prefix=lia_marshal lia-marshal.list > lia-marshal.h

lia-marshal.c: lia-marshal.list lia-marshal.h
	glib-genmarshal --body \
		--prefix=lia_marshal lia-marshal.list > lia-marshal.c

# libraries
lib_LTLIBRARIES = lib@PRJ_API_NAME@.la

# liblia
source_c = \
	lia-marshal.c \
	lia-auth-service.c \
	lia-rdf-store.c \
	lia-application.c \
	lia-core.c \
	lia-webview.c

source_h = \
	lia-marshal.h \
	lia-defines.h \
	lia-auth-service.h \
	lia-rdf-store.h \
	lia-application.h \
	lia-core.h \
	lia-webview.h

source_h_priv =

lib@PRJ_API_NAME@_la_LIBADD = \
	$(EVD_LIBS) \
	$(JSON_LIBS)

lib@PRJ_API_NAME@_la_CFLAGS  = \
	$(AM_CFLAGS) \
	$(EVD_CFLAGS) \
	$(JSON_CFLAGS)

lib@PRJ_API_NAME@_la_LDFLAGS = \
	-version-info 0:1:0 \
	-no-undefined

lib@PRJ_API_NAME@_la_SOURCES = \
	$(source_c) \
	$(source_h) \
	$(source_h_priv)

liadir = $(includedir)/@PRJ_API_NAME@
lia_HEADERS = \
	$(source_h) \
	$(source_h_priv)

# introspection support
if HAVE_INTROSPECTION

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=$(srcdir) \
	--identifier-prefix=Lia \
	--warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

introspection_sources = $(lia_HEADERS) $(source_c)

@PRJ_GIR_API_NAME@.gir: lib@PRJ_API_NAME@.la Makefile
@PRJ_GIR_TARGET_NAME@_INCLUDES = GLib-2.0 Gio-2.0 GObject-2.0
@PRJ_GIR_TARGET_NAME@_CFLAGS = $(INCLUDES) $(JSON_CFLAGS)
@PRJ_GIR_TARGET_NAME@_LIBS = lib@PRJ_API_NAME@.la uuid
@PRJ_GIR_TARGET_NAME@_FILES = $(addprefix $(srcdir)/,$(introspection_sources))

INTROSPECTION_GIRS += @PRJ_GIR_API_NAME@.gir

girdir = $(datadir)/gir-1.0
dist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(dist_gir_DATA) $(typelib_DATA)
endif

maintainer-clean-local:
	rm -rf tmp-introspect*

EXTRA_DIST = \
	lia-marshal.list
