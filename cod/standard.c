#include "config.h"
#ifndef LINUX_KERNEL_MODULE
#include "stdio.h"
#endif
#ifdef LINUX_KERNEL_MODULE
#ifndef MODULE
#define MODULE
#endif
#ifndef __KERNEL__
#define __KERNEL__
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#endif
#ifndef LINUX_KERNEL_MODULE
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#endif
#include "cod.h"
#include "cod_internal.h"
#include "structs.h"
#include "assert.h"
#ifndef LINUX_KERNEL_MODULE
#include <ctype.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <string.h>
#else
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/mm.h>
#endif
#ifndef LINUX_KERNEL_MODULE
#ifdef HAVE_ATL_H
#include "atl.h"
#endif
#ifdef HAVE_CERCS_ENV_H
#include "cercs_env.h"
#endif
#endif


#ifndef LINUX_KERNEL_MODULE
#ifdef HAVE_ATL_H
static int
attr_set(attr_list l, char *name)
{
    atom_t atom = attr_atom_from_string(name);
    attr_value_type junk;
    attr_value junk2;
    if (atom == 0 ) return 0;
    
    return query_attr(l, atom, &junk, &junk2);
}

static attr_list
attr_create_list()
{
    return create_attr_list();
}

static void
attr_free_list(attr_list l)
{
    free_attr_list(l);
}

static void
std_set_int_attr(attr_list l, char *name, int value)
{
    atom_t atom = attr_atom_from_string(name);
    if (atom == 0 ) return;

    set_int_attr(l, atom, value);
}

static void
std_set_long_attr(attr_list l, char *name, long value)
{
    atom_t atom = attr_atom_from_string(name);
    if (atom == 0 ) return;

    set_long_attr(l, atom, value);
}

static void
std_set_double_attr(attr_list l, char *name, double value)
{
    atom_t atom = attr_atom_from_string(name);
    if (atom == 0 ) return;

    set_double_attr(l, atom, value);
}

static void
std_set_float_attr(attr_list l, char *name, float value)
{
    atom_t atom = attr_atom_from_string(name);
    if (atom == 0 ) return;

    set_float_attr(l, atom, value);
}

static void
std_set_string_attr(attr_list l, char *name, char *value)
{
    atom_t atom = attr_atom_from_string(name);
    if (atom == 0 ) return;

    set_string_attr(l, atom, value);
}

static int
attr_ivalue(attr_list l, char *name)
{
    atom_t atom = attr_atom_from_string(name);
    int i = 0;
    if (atom == 0 ) return 0;
    
    get_int_attr(l, atom, &i);
    return i;
}

static long
attr_lvalue(attr_list l, char *name)
{
    atom_t atom = attr_atom_from_string(name);
    long lo = 0;
    if (atom == 0 ) return 0;
    
    get_long_attr(l, atom, &lo);
    return lo;
}

static double
attr_dvalue(attr_list l, char *name)
{
    atom_t atom = attr_atom_from_string(name);
    double d;
    if (atom == 0 ) return 0;
    
    get_double_attr(l, atom, &d);
    return d;
}

static float
attr_fvalue(attr_list l, char *name)
{
    atom_t atom = attr_atom_from_string(name);
    float f;
    if (atom == 0 ) return 0;
    
    get_float_attr(l, atom, &f);
    return f;
}

static char *
attr_svalue(attr_list l, char *name)
{
    atom_t atom = attr_atom_from_string(name);
    char *s;
    if (atom == 0 ) return 0;
    
    get_string_attr(l, atom, &s);
    return strdup(s);
}
#endif

static char extern_string[] = "\n\
	int attr_set(attr_list l, string name);\n\
	attr_list create_attr_list();\n\
	void free_attr_list(attr_list l);\n					\
	void set_long_attr(attr_list l, string name, long value);\n\
	void set_float_attr(attr_list l, string name, double value);\n\
	void set_double_attr(attr_list l, string name, double value);\n\
	void set_int_attr(attr_list l, string name, int value);\n\
	void set_string_attr(attr_list l, string name, string value);\n\
	int attr_ivalue(attr_list l, string name);\n\
	long attr_lvalue(attr_list l, string name);\n\
	double attr_dvalue(attr_list l, string name);\n\
	double attr_fvalue(attr_list l, string name);\n\
	char* attr_svalue(attr_list l, string name);\n\
        void chr_get_time( chr_time *time);\n\
        void chr_timer_diff( chr_time *diff_time, chr_time *src1, chr_time *src2);\n\
	int chr_timer_eq_zero( chr_time *time);\n\
	void chr_timer_sum( chr_time *sum_time, chr_time *src1, chr_time *src2);\n\
	void chr_timer_start (chr_time *timer);\n\
	void chr_timer_stop (chr_time *timer);\n\
	double chr_time_to_nanosecs (chr_time *time);\n\
	double chr_time_to_microsecs (chr_time *time);\n\
	double chr_time_to_millisecs (chr_time *time);\n\
	double chr_time_to_secs (chr_time *time);\n\
	double chr_approx_resolution();\n";
static char internals[] = "\n\
	void cod_NoOp(int duration);\n";

static cod_extern_entry internal_externs[] = 
{
    {"cod_NoOp", (void*)(long)0xdeadbeef},    /* value is unimportant, but can't be NULL */
    {NULL, NULL}
};

static cod_extern_entry externs[] = 
{
#ifdef HAVE_ATL_H
    {"attr_set", (void*)(long)attr_set},
    {"create_attr_list", (void*)(long)attr_create_list},
    {"free_attr_list", (void*)(long)attr_free_list},
    {"set_int_attr", (void*)(long)std_set_int_attr},
    {"set_long_attr", (void*)(long)std_set_long_attr},
    {"set_double_attr", (void*)(long)std_set_double_attr},
    {"set_float_attr", (void*)(long)std_set_float_attr},
    {"set_string_attr", (void*)(long)std_set_string_attr},
    {"attr_ivalue", (void*)(long)attr_ivalue},
    {"attr_lvalue", (void*)(long)attr_lvalue},
    {"attr_dvalue", (void*)(long)attr_dvalue},
    {"attr_fvalue", (void*)(long)attr_fvalue},
    {"attr_svalue", (void*)(long)attr_svalue},
#endif
#ifdef HAVE_CERCS_ENV_H
    {"chr_get_time", (void*)(long)chr_get_time},
    {"chr_timer_diff", (void*)(long)chr_timer_diff},
    {"chr_timer_eq_zero", (void*)(long)chr_timer_eq_zero},
    {"chr_timer_sum", (void*)(long)chr_timer_sum},
    {"chr_timer_start", (void*)(long)chr_timer_start},
    {"chr_timer_stop", (void*)(long)chr_timer_stop},
    {"chr_time_to_nanosecs", (void*)(long)chr_time_to_nanosecs},
    {"chr_time_to_microsecs", (void*)(long)chr_time_to_microsecs},
    {"chr_time_to_millisecs", (void*)(long)chr_time_to_millisecs},
    {"chr_time_to_secs", (void*)(long)chr_time_to_secs},
    {"chr_approx_resolution", (void*)(long)chr_approx_resolution},
#endif
    {(void*)0, (void*)0}
};

#ifdef HAVE_CERCS_ENV_H
FMField chr_time_list[] = {
    {"d1", "double", sizeof(double), FMOffset(chr_time*, d1)}, 
    {"d2", "double", sizeof(double), FMOffset(chr_time*, d2)}, 
    {"d3", "double", sizeof(double), FMOffset(chr_time*, d3)}, 
    {NULL, NULL, 0, 0}};
#endif

extern void
cod_add_standard_elements(cod_parse_context context)
{
#ifdef HAVE_ATL_H
    sm_ref attr_node = cod_new_reference_type_decl();
    attr_node->node.reference_type_decl.name = strdup("attr_list");
    cod_add_decl_to_parse_context("attr_list", attr_node, context);
    cod_add_decl_to_scope("attr_list", attr_node, context);
    cod_add_defined_type("attr_list", context);
#endif
#ifdef HAVE_CERCS_ENV_H
    cod_add_simple_struct_type("chr_time", chr_time_list, context);
#endif
    cod_add_defined_type("cod_type_spec", context);
    cod_add_defined_type("cod_exec_context", context);
    cod_add_defined_type("cod_closure_context", context);
    cod_semanticize_added_decls(context);
    
#if defined(HAVE_ATL_H) && defined(HAVE_CERCS_ENV_H)
    cod_assoc_externs(context, externs);
    cod_parse_for_context(extern_string, context);
#endif
    cod_assoc_externs(context, internal_externs);
    cod_parse_for_context(internals, context);
    cod_swap_decls_to_standard(context);
}

#else /* LINUX_KERNEL_MODULE */

extern void
cod_add_standard_elements(cod_parse_context context)
{
}
#endif /* LINUX_KERNEL_MODULE */

#if NO_DYNAMIC_LINKING
#define sym(x) (void*)(long)x
#else
#define sym(x) (void*)0
#endif

static cod_extern_entry string_externs[] = 
{
    {"strchr", (void*)(long)strchr},
    {NULL, NULL}
};

static char string_extern_string[] = "\n\
	char	*strchr(const char *str, int chr);\n\
";

#include <math.h>

static cod_extern_entry math_externs[] = 
{
    {"acos", sym(acos)},
    {"asin", sym(asin)},
    {"atan", sym(atan)},
    {"atan2", sym(atan2)},
    {"cos", sym(cos)},
    {"sin", sym(sin)},
    {"tan", sym(tan)},
    {"acosh", sym(acosh)},
    {"asinh", sym(asinh)},
    {"atanh", sym(atanh)},
    {"cosh", sym(cosh)},
    {"sinh", sym(sinh)},
    {"tanh", sym(tanh)},
    {"exp", sym(exp)},
    {"exp2", sym(exp2)},
    {"expm1", sym(expm1)},
    {"log", sym(log)},
    {"log10", sym(log10)},
    {"log2", sym(log2)},
    {"log1p", sym(log1p)},
    {"logb", sym(logb)},
    {"modf", sym(modf)},
    {"ldexp", sym(ldexp)},
    {"frexp", sym(frexp)},
    {"ilogb", sym(ilogb)},
    {"scalbn", sym(scalbn)},
    {"scalbln", sym(scalbln)},
    {"fabs", sym(fabs)},
    {"cbrt", sym(cbrt)},
    {"hypot", sym(hypot)},
    {"pow", sym(pow)},
    {"sqrt", sym(sqrt)},
    {"erf", sym(erf)},
    {"erfc", sym(erfc)},
    {"lgamma", sym(lgamma)},
    {"tgamma", sym(tgamma)},
    {"ceil", sym(ceil)},
    {"floor", sym(floor)},
    {"nearbyint", sym(nearbyint)},
    {"rint", sym(rint)},
    {"lrint", sym(lrint)},
    {"round", sym(round)},
    {"lround", sym(lround)},
    {"trunc", sym(trunc)},
    {"fmod", sym(fmod)},
    {"remainder", sym(remainder)},
    {"remquo", sym(remquo)},
    {"copysign", sym(copysign)},
    {"nan", sym(nan)},
    {NULL, NULL}
};

static char math_extern_string[] = "\n\
double acos(double a);\n\
double asin(double a);\n\
double atan(double a);\n\
double atan2(double b, double a);\n\
double cos(double a);\n\
double sin(double a);\n\
double tan(double a);\n\
double acosh(double a);\n\
double asinh(double a);\n\
double atanh(double a);\n\
double cosh(double a);\n\
double sinh(double a);\n\
double tanh(double a);\n\
double exp(double a);\n\
double exp2(double a); \n\
double expm1(double a); \n\
double log(double a);\n\
double log10(double a);\n\
double log2(double a);\n\
double log1p(double a);\n\
double logb(double a);\n\
double modf(double b, double * a);\n\
double ldexp(double b, int a);\n\
double frexp(double b, int * a);\n\
int ilogb(double a);\n\
double scalbn(double b, int a);\n\
double scalbln(double b, long int a);\n\
double fabs(double a);\n\
double cbrt(double a);\n\
double hypot(double b, double a);\n\
double pow(double b, double a);\n\
double sqrt(double a);\n\
double erf(double a);\n\
double erfc(double a);\n\
double lgamma(double a);\n\
double tgamma(double a);\n\
double ceil(double a);\n\
double floor(double a);\n\
double nearbyint(double a);\n\
double rint(double a);\n\
long   lrint(double a);\n\
double round(double a);\n\
long   lround(double a);\n\
double trunc(double a);\n\
double fmod(double a, double b);\n\
double remainder(double a, double b);\n\
double remquo(double a, double b, int *c);\n\
double copysign(double a, double b);\n\
double nan(const char * a);\n\
";

static void dlload_externs(char *libname, cod_extern_entry *externs);

extern void
cod_process_include(char *name, cod_parse_context context)
{
    int char_count = index(name, '.') - name;
    if (char_count < 0) char_count = strlen(name);
    if (strncmp(name, "string", char_count) == 0) {
	cod_assoc_externs(context, string_externs);
	cod_parse_for_context(string_extern_string, context);
    } else if (strncmp(name, "math", char_count) == 0) {
	dlload_externs("libm", math_externs);
	cod_assoc_externs(context, math_externs);
	cod_parse_for_context(math_extern_string, context);
    }

}
#include <dlfcn.h>
static void 
dlload_externs(char *libname, cod_extern_entry *externs)
{
#if NO_DYNAMIC_LINKING
    return;
#else
    int i = 0;
    char *name = malloc(strlen(libname) + strlen(LIBRARY_EXT) + 1);
    strcpy(name, libname);
    strcat(name, LIBRARY_EXT);
    void *handle = dlopen(name, RTLD_LAZY);
    free(name);
    while(externs[i].extern_name) {
	externs[i].extern_value = dlsym(handle, externs[i].extern_name);
	i++;
    }
#endif
}
