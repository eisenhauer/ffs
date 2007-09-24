## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign

SUBDIRS = . tests

noinst_HEADERS = assert.h config.h ffs_gen.h ffs_internal.h 

libffs_la_SOURCES = ffs.c ffs_conv.c ffs_gen.c ffs_file.c unix_io.c

lib_LTLIBRARIES = libffs.la
EXTRA_libffs_la_SOURCES = ffs_malloc.c unix_io.c nt_io.c ffs_gen.c
libffs_la_DEPENDENCIES =  ../fm/libfm.la
libffs_la_LIBADD =  ../fm/libfm.la
libffs_la_LDFLAGS = -no-undefined @DCG_LALIB@

LDADD = libffs.la
ffs.h: ffs.h.in
	$(SED) 's/UIO_SIZE_T_TYPE/@UIO_SIZE_T_TYPE@/g' < ffs.h.in > ffs.h

include_HEADERS = ffs.h io_interface.h
MOSTLYCLEANFILES = dcg/*
all-local:
install-exec-local:

