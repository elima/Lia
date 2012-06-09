SUBDIRS = \
	liblia \
	lia-core \
	lia-webview

DIST_SUBDIRS = \
	liblia \
	lia-core \
	lia-webview

EXTRA_DIST = \
	autogen.sh \
	m4/introspection.m4

CLEANFILES = *~

MAINTAINERCLEANFILES = \
	INSTALL Makefile.in aclocal.m4 configure config.guess config.sub \
	depcomp install-sh ltmain.sh missing mkinstalldirs config.h.in \
	stamp-h.in compile

DISTCLEANFILES = \
	cscope.files cscope.out

cscope.files:
	find src -name '*.[ch]' > $@

cscope.out: cscope.files
	cscope -b
