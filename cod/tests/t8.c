#include "config.h"
#include "cod.h"
#include "assert.h"
#include <stdlib.h>
#include <stdio.h>

int
main()
{
    {
	/* test pointer basics */
	char code_string[] = "\
{\n\
    int j = 2;\n\
    int * k;\n\
    k = &j;\n\
    *k = 4;\n\
    return (*k + 3);\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
	assert(result == 7);
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }
    {
	/* test cast of complex type (&j) to basic type (long) */
	char code_string[] = "\
{\n\
    int j = 2;\n\
    long addr;\n\
    int * k;\n\
    addr = (long)&j;\n\
    k = (unsigned long)addr;\n\
    return (*k + 3 + j);\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
	if (result != 7) {
	    printf("Expected 7, got %d in t8, subtest 2\n", result);
	}
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Testing of pointer assignments and conversions:
	 * DR_P -> DR_L/DR_UL
	 * DR_C/DR_UC/DR_S/DR_US/DR_I/DR_U/DR_L/DR_UL -> DR_P
	 */
	char code_string[] = "\
{\n\
    char c = 1;\n\
    unsigned char uc = 2;\n\
    short s = 3;\n\
    unsigned short us = 4;\n\
    int i = 5;\n\
    unsigned int ui = 6;\n\
    long l = 7;\n\
    unsigned long ul = 8;\n\
    \n\
    int * p;\n\
    p  = l;\n\
    p  = ul;\n\
    \n\
    c  = p;\n\
    uc = p;\n\
    s  = p;\n\
    us = p;\n\
    i  = p;\n\
    ui = p;\n\
    l  = p;\n\
    ul = p;\n\
    return ui;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
	assert(result==8);
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test op_inc pointer arithmetic on pointers
	 */
	char code_string[] = "\
{\n\
    int * p = (long)0;\n\
    p++;\n\
    return p;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
	assert(result == sizeof(int));
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test op_dec pointer arithmetic on pointers
	 */
	char code_string[] = "\
{\n\
    int * p = (long)0;\n\
    p--;\n\
    return p;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
	assert(result == -sizeof(int));
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test op_inc pointer arithmetic on pointers to pointers
	 */
	char code_string[] = "\
{\n\
    double ** p = (long)0;\n\
    p++;\n\
    return p;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
	assert(result == sizeof(double *));
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test op_plus pointer arithmetic on pointers
	 */
	char code_string[] = "\
{\n\
    int i;\n\
    double * p = (long)0;\n\
    double ** dp = &p;\n\
    p = (*dp) + 2;\n\
    return p;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
	assert(result == 2*sizeof(double));
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test op_plus pointer arithmetic on pointers to pointers
	 */
	char code_string[] = "\
{\n\
    int i;\n\
    double ** dp = (long)0;\n\
    dp = dp + 2;\n\
    return dp;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
	assert(result == 2*sizeof(double *));
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test op_minus pointer arithmetic between pointers
	 */
	char code_string[] = "\
{\n\
     long r;\n\
    double * dp1 = (long)24;\n\
    double * dp2 = (long)8;\n\
    r = dp1 - dp2;\n\
    return r;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
        assert(result == 2);
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test op_minus pointer arithmetic between pointers
	 */
	char code_string[] = "\
{\n\
     long r;\n\
    double * dp1 = (long)24;\n\
    double * dp2 = (long)8;\n\
    r = dp2 - dp1;\n\
    return r;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
        assert(result == -2);
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test op_minus pointer arithmetic between pointers to pointers
	 */
	char code_string[] = "\
{\n\
     long r;\n\
    double ** dp1 = (long)24;\n\
    double ** dp2 = (long)8;\n\
    r = dp1 - dp2;\n\
    return r;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
	if (result != 16/sizeof(void*)) { 
	    printf(" op minus result is %d\n", result);
	    exit(1);
	}
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test op_minus pointer arithmetic between pointers to pointers
	 */
	char code_string[] = "\
{\n\
     long r;\n\
    double ** dp1 = (long)24;\n\
    double ** dp2 = (long)8;\n\
    r = dp2 - dp1;\n\
    return r;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
        if (result != -(16/sizeof(void*))) {
	    printf(" 2nd op minus result is %d\n", result);
	    exit(1);
	}
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test op_minus pointer arithmetic on a pointer and an integral
	 */
	char code_string[] = "\
{\n\
    int i;\n\
    double * p = (long)0;\n\
    double ** dp = &p;\n\
    p = (*dp) - 2;\n\
    return p;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
	assert(result == -2*sizeof(double));
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test op_minus pointer arithmetic on a pointer to pointer and an integral
	 */
	char code_string[] = "\
{\n\
    int i;\n\
    double ** dp = (long)0;\n\
    dp = dp - 2;\n\
    return dp;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	long (*func)();
	long result;

	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)()) (long) gen_code->func;
	result = func();
	assert(result == -2*sizeof(double *));
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }
    {
	/*
	 * Test pointer casting
	 */
	char code_string[] = "\
{\n\
    int i;\n\
    unsigned char *p;\n\
    p = param;\n\
    return (long) p;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	unsigned char *p = (unsigned char *) "hiya";
	long (*func)(unsigned char *);
	long result;

	cod_subroutine_declaration("long test(unsigned char * param)", context);
	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (long(*)(unsigned char *)) (long) gen_code->func;
	result = func(p);
	assert(result == (long) p);
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }
    {
	/*
	 * Test postincrement to a pointer dereference
	 */
	char code_string[] = "\
{\n\
    return (*param)++;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	int i = 8;
	int (*func)(int *);
	int result;

	cod_subroutine_declaration("int test(int* param)", context);
	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (int(*)(int *)) (long) gen_code->func;
	result = func(&i);
	if (result != 8) printf("result was %d, not 8\n", result);
	if (i != 9) printf("i was %d, not 9\n", i);
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    {
	/*
	 * Test preincrement to a pointer dereference
	 */
	char code_string[] = "\
{\n\
    return ++(*param);\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	int i = 8;
	int (*func)(int *);
	int result;

	cod_subroutine_declaration("int test(int* param)", context);
	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (int(*)(int *)) (long) gen_code->func;
	result = func(&i);
	if (result != 9) printf("result was %d, not 8\n", result);
	if (i != 9) printf("i was %d, not 9\n", i);
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }
    {
	/*
	 * Test postincrement to a parameter pointer
	 */
	char code_string[] = "\
{\n\
    return ++param;\n\
}";

	cod_parse_context context = new_cod_parse_context();
	cod_code gen_code;
	int i = 8;
	int *(*func)(int *);
	int *result;

	cod_subroutine_declaration("int test(int* param)", context);
	gen_code = cod_code_gen(code_string, context);
	if(!gen_code) {
	  printf("Code generation failed!\n");
	  return -1;
	} 
	func = (int(*)(int *)) (long) gen_code->func;
	result = func(&i);
	if (result != (&i + 1)) printf("result was %lx, not %lx\n", result,
		&i + 1);
	cod_code_free(gen_code);
	cod_free_parse_context(context);
    }

    return 0;
}
