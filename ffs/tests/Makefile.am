## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign

noinst_HEADERS = test_funcs.h

INCLUDES = -I../../fm -I../../cod -I..

LDADD = ../libffs.la

TESTS = context_test context_test2 fortran_test 

bin_PROGRAMS = 
#noinst_PROGRAMS = io_read io_read2 io_dyn_read io_test io_write context_test fortran_test xml_test compat_test io_default_test io_def_write 
noinst_PROGRAMS = context_test context_test2 io_write io_dyn_read fortran_test 

MOSTLYCLEANFILES = dcg/*
all-local:
install-exec-local:

context_test_SOURCES = context_test.c test_funcs.c
context_test2_SOURCES = context_test2.c test_funcs.c
fortran_test_SOURCES = fortran_test.c test_funcs.c
xml_test_SOURCES= xml_test.c test_funcs.c
compat_test_SOURCES= compat_test.c test_funcs.c
io_test_SOURCES= io_test.c test_funcs.c
io_default_test_SOURCES= io_default_test.c test_funcs.c
io_write_SOURCES= io_write.c test_funcs.c
io_dyn_read_SOURCES= io_dyn_read.c test_funcs.c


write_output: context_test
	rm -f ../output_dir/context_test_output.@ARCH@.bin
	./context_test -w ../output_dir/context_test_output.@ARCH@.bin

check-local:  context_test @OUTPUT_TARGET@
	@failed=0;\
	for i in ../output_dir/context_test_output*.bin; do \
	    if ./context_test -r $$i; then\
	    	echo "PASS: $$i";\
	    else\
	    	echo "FAILED: context_test -r $$i";\
	        failed=`expr $$failed + 1`; \
	    fi;\
	done;\
	if test "$$failed" -eq 0; then \
	    echo "All tests passed"; \
	else\
	    echo "$$failed tests failed";\
	fi;\
	test "$$failed" -eq 0


