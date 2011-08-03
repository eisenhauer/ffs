## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign

LDADD = ../libffs.la

INCLUDES = -I.. -I$(srcdir)/../../fm -I$(srcdir)/../tests

if !NO_SOCKETS
bin_PROGRAMS = FFSdump FFScp FFSsort
noinst_PROGRAMS = sort_test_generate
endif

all-local:
install-exec-local:

FFSdump_LDFLAGS = -static
