MAINTAINERCLEANFILES = \
	Makefile.in

sbin_PROGRAMS = lia-core

lia_core_CFLAGS = \
	-Wall \
	$(EVD_CFLAGS) \
	$(JSON_CFLAGS) \
	-I../liblia/ \
	-L../liblia/ \
	-DENABLE_TESTS="\"$(enable_tests)\""

if ENABLE_DEBUG
lia_core_CFLAGS += -Werror -g3 -O0 -ggdb
else
lia_core_CFLAGS += -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS
endif

lia_core_LDADD = \
	$(EVD_LIBS) \
	$(JSON_LIBS) \
	-l@PRJ_API_NAME@

lia_core_SOURCES = \
	lia-core-main.c
