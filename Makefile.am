## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign

SUBDIRS= fm cod ffs .

all:
	rm -f fm.h cod.h ffs.h
	$(LN_S) fm/fm.h
	$(LN_S) cod/cod.h
	$(LN_S) ffs/ffs.h

noinst_LTLIBRARIES = libfm.la libcod.la libffs.la

libfm_la_SOURCES = fm/libfm.la
libcod_la_SOURCES = cod/libcod.la
libffs_la_SOURCES = ffs/libffs.la
