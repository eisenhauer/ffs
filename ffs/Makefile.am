## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign
ACLOCAL_AMFLAGS = -I ../config

SUBDIRS = . progs tests

noinst_HEADERS = assert.h config.h ffs_gen.h ffs_internal.h 

INCLUDES = -I../fm

libffs_la_SOURCES = ffs.c ffs_formats.c ffs_conv.c ffs_gen.c ffs_file.c unix_io.c evol.c version.c

lib_LTLIBRARIES = libffs.la
EXTRA_libffs_la_SOURCES = ffs_malloc.c unix_io.c nt_io.c ffs_gen.c
libffs_la_DEPENDENCIES =  ../fm/libfm.la
libffs_la_LIBADD =  ../fm/libfm.la -latl
libffs_la_LDFLAGS = -no-undefined @DCG_LALIB@

LDADD = libffs.la
ffs.h: ffs.h.in
	$(SED) 's/UIO_SIZE_T_TYPE/@UIO_SIZE_T_TYPE@/g' < ffs.h.in > ffs.h

include_HEADERS = ffs.h io_interface.h
MOSTLYCLEANFILES = dcg/*

$(libffs_la_SOURCES) : ffs.h

all-local:
install-exec-local:

