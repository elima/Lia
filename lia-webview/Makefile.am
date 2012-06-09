MAINTAINERCLEANFILES = \
	Makefile.in

htmldatadir = $(datadir)/lia/webview

sbin_PROGRAMS = lia-webview

lia_webview_CFLAGS = \
	-Wall \
	$(EVD_CFLAGS) \
	$(JSON_CFLAGS) \
	-I../liblia/ \
	-L../liblia/ \
	-DENABLE_TESTS="\"$(enable_tests)\"" \
	-DHTML_DATA_DIR="\"$(htmldatadir)\""

if ENABLE_DEBUG
lia_webview_CFLAGS += -Werror -g3 -O0 -ggdb
else
lia_webview_CFLAGS += -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS
endif

lia_webview_LDADD = \
	$(EVD_LIBS) \
	$(JSON_LIBS) \
	-l@PRJ_API_NAME@

lia_webview_SOURCES = \
	lia-webview-main.c

install-data-local: html
	mkdir -p $(htmldatadir)
	cp -r ./html/* $(htmldatadir)/

EXTRA_DIST = \
	html/private/index.html \
	html/public/index.html
