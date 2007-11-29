## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign

noinst_HEADERS = test_funcs.h

INCLUDES = -I../../fm -I../../cod -I..

LDADD = ../libffs.la

TESTS = context_test

bin_PROGRAMS = 
#noinst_PROGRAMS = io_read io_read2 io_dyn_read io_test io_write context_test fortran_test xml_test compat_test io_default_test io_def_write 
noinst_PROGRAMS = context_test io_write io_dyn_read

MOSTLYCLEANFILES = dcg/*
all-local:
install-exec-local:

context_test_SOURCES = context_test.c test_funcs.c
fortran_test_SOURCES = fortran_test.c test_funcs.c
xml_test_SOURCES= xml_test.c test_funcs.c
compat_test_SOURCES= compat_test.c test_funcs.c
io_test_SOURCES= io_test.c test_funcs.c
io_default_test_SOURCES= io_default_test.c test_funcs.c
io_write_SOURCES= io_write.c test_funcs.c
io_dyn_read_SOURCES= io_dyn_read.c test_funcs.c


local_context_test: context_test@EXE_EXT@
	./context_test@EXE_EXT@ -w /tmp/context.@ARCH@
	cp /tmp/context.@ARCH@ @SPECIAL_TEST_DIR@/.@ARCH@
	rm /tmp/context.@ARCH@
	mv -f @SPECIAL_TEST_DIR@/.@ARCH@ @SPECIAL_TEST_DIR@/context.@ARCH@
	@chmod ugo+r @SPECIAL_TEST_DIR@/context.@ARCH@
	for targ in @SPECIAL_TEST_DIR@/context.*; \
	do ./context_test@EXE_EXT@ -r "$$targ"  || (echo " in $$targ" && exit 0);\
	done

../output_dir/test_output.@ARCH@.v6b.bin: io_write@EXE_EXT@ io_align_test@EXE_EXT@
	./io_align_test
	./io_def_write
	./io_default_test
	./io_write
	rm -f ../output_dir/test_output.@ARCH@.v6b.bin
	mv test_output ../output_dir/test_output.@ARCH@.v6b.bin

check_target:  io_test@EXE_EXT@ ../output_dir/test_output.@ARCH@.v6b.bin
	@failed=0;\
	for i in ../output_dir/test*.bin; do \
	    if ./io_test@EXE_EXT@ $$i; then\
	    	echo "PASS: $$i";\
	    else\
	    	echo "FAILED: io_test $$i";\
	        failed=`expr $$failed + 1`; \
	    fi;\
	done;\
	if test "$$failed" -eq 0; then \
	    echo "All tests passed"; \
	else\
	    echo "$$failed tests failed";\
	fi;\
	test "$$failed" -eq 0


