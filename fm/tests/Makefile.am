## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign

noinst_HEADERS = test_funcs.h

LDADD = ../libfm.la

TESTS = align_test

if NO_SOCKETS
bin_PROGRAMS = 
else
bin_PROGRAMS = format_test self_format_test align_test
endif

all-local:
install-exec-local:

format_test_SOURCES = format_test.c test_funcs.c
self_format_test_SOURCES = self_format_test.c test_funcs.c
align_test_SOURCES = align_test.c test_funcs.c

check_target:
