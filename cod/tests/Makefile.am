
AUTOMAKE_OPTIONS = no-dependencies foreign

EXTRA_PROGRAMS = t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12 gray general mix atl_test strings

syntax-tests:  t4.stamp

run_time_tests: general_tests

general_tests: t1@EXEEXT@ t2.stamp t3@EXEEXT@ t5@EXEEXT@ t6@EXEEXT@ t8@EXEEXT@ t9@EXEEXT@ t10@EXEEXT@ t12@EXEEXT@ gray@EXEEXT@ mix@EXEEXT@ atl_test@EXEEXT@ @GENERAL_TEST@ strings@EXEEXT@
	./t1 -w test_output/t1/t1.@ARCH@
	./t3
	./t5 -w test_output/t5/t5.@ARCH@
	./t6
	./t8
	./t9
	./t10
	./t12
	./atl_test
	./gray
	./strings
	@GENERAL_TEST@

t1_SOURCES = t1.c data_funcs.c
t2_SOURCES = t2.c data_funcs.c
t5_SOURCES = t5.c data_funcs.c

check-local: syntax-tests @test_sub@ 

general_LDADD = ../libcod.la 

general.c:  general.ops
	perl general.ops

general.$(OBJEXT): general.c
	$(LIBTOOL) --mode=compile `echo $(COMPILE) | sed 's/-O[0-9]*//'`  -c $<

t4.stamp:  t4@EXEEXT@
	./t4 1>tmp.out  2>&1
	@grep -v "lib[vi]code.a" tmp.out > tmp1.out
	@grep -v "## Error.*error" tmp1.out > tmp.out
	@rm tmp1.out
	diff -w tmp.out t4.out && touch t4.stamp
	@rm tmp.out

t2.stamp:  t2@EXEEXT@
	./t2 1>tmp.out 2>&1
	@grep -v "lib[vi]code.a" tmp.out > tmp1.out
	@mv tmp1.out tmp.out
	diff -w tmp.out t2.out && touch t2.stamp
	@rm tmp.out

clean: clean-am
	rm -f t4.stamp t2.stamp ${EXTRA_PROGRAMS} general.c

LDADD = ../libcod.la
