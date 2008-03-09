## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign

SUBDIRS = . progs tests

noinst_HEADERS = assert.h config.h fm_internal.h

if NO_SOCKETS
libfm_la_SOURCES = fm_formats.c lookup3.c string_conversion.c fm_get.c xml.c null_io.c
else
libfm_la_SOURCES = fm_formats.c string_conversion.c lookup3.c server_acts.c server.c unix_io.c fm_get.c xml.c
endif


LDADD = libfm.la

lib_LTLIBRARIES = libfm.la
include_HEADERS = fm.h

