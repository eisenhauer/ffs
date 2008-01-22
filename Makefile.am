## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign

SUBDIRS= fm cod ffs .

all:
	rm -f fm.h cod.h ffs.h
	$(LN_S) fm/fm.h
	$(LN_S) cod/cod.h
	$(LN_S) ffs/ffs.h

noinst_LTLIBRARIES = libfm.la libcod.la libffs.la

libfm_la_SOURCES =
libfm_la_LIBADD = fm/libfm.la
libcod_la_SOURCES =
libcod_la_LIBADD = cod/libcod.la
libffs_la_SOURCES =
libffs_la_LIBADD = ffs/libffs.la
