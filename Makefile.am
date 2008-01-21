## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign

SUBDIRS= fm cod ffs .

all:
	rm -f fm.h cod.h ffs.h libfm.la libcod.la libffs.la
	$(LN_S) fm/fm.h
	$(LN_S) fm/libfm.la
	$(LN_S) cod/cod.h
	$(LN_S) cod/libcod.la
	$(LN_S) ffs/ffs.h
	$(LN_S) ffs/libffs.la
