#include "config.h"
#include <assert.h>
#include <string.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#include "ffs.h"
#include "unix_defs.h"

extern char *data_type_to_str();

typedef struct _first_rec {
    int integer_field;
    double double_field;
    char char_field;
} first_rec, *first_rec_ptr;

static IOField field_list[] =
{
    {"integer field", "integer",
     sizeof(int), IOOffset(first_rec_ptr, integer_field)},
    {"double field", "float",
     sizeof(double), IOOffset(first_rec_ptr, double_field)},
    {"char field", "char",
     sizeof(char), IOOffset(first_rec_ptr, char_field)},
    {NULL, NULL, 0, 0}
};

#define ARRAY_SIZE 14

typedef struct _fourth_rec {
    long ifield;
    int int_array[ARRAY_SIZE];
#if SIZEOF_LONG_DOUBLE != 0 && SIZEOF_LONG_DOUBLE != SIZEOF_DOUBLE
    long double double_array[2][2];
#else
    double double_array[2][2];
#endif
} fourth_rec, *fourth_rec_ptr;

typedef struct _later_rec {
    int integer_field;
    char *string;
    double double_field;
} later_rec, *later_rec_ptr;

static IOField later_field_list[] =
{
    {"integer field", "integer",
     sizeof(int), IOOffset(later_rec_ptr, integer_field)},
    {"string field", "string",
     sizeof(char *), IOOffset(later_rec_ptr, string)},
    {"double field", "float",
     sizeof(double), IOOffset(later_rec_ptr, double_field)},
    {NULL, NULL, 0, 0}
};

typedef struct _second_rec {
    int integer_field;
    short short_field;
    long long_field;
    char *string;
    double double_field;
    char char_field;
} second_rec, *second_rec_ptr;

static IOField field_list2[] =
{
    {"integer field", "integer",
     sizeof(int), IOOffset(second_rec_ptr, integer_field)},
    {"short field", "integer",
     sizeof(short), IOOffset(second_rec_ptr, short_field)},
    {"long field", "integer",
     sizeof(long), IOOffset(second_rec_ptr, long_field)},
    {"string field", "string",
     sizeof(char *), IOOffset(second_rec_ptr, string)},
    {"double field", "float",
     sizeof(double), IOOffset(second_rec_ptr, double_field)},
    {"char field", "char",
     sizeof(char), IOOffset(second_rec_ptr, char_field)},
    {NULL, NULL, 0, 0}
};


typedef struct _nested_rec {
    int integer_field;
    second_rec nested_rec;
    char *string;
} nested_rec, *nested_rec_ptr;

static IOField nested_field_list[] =
{
    {"integer field", "integer",
     sizeof(((nested_rec_ptr) 0)->integer_field),
     IOOffset(nested_rec_ptr, integer_field)},
    {"nested record", "string format",
     sizeof(second_rec), IOOffset(nested_rec_ptr, nested_rec)},
    {"string field", "string",
     sizeof(char *), IOOffset(nested_rec_ptr, string)},
    {NULL, NULL, 0, 0}
};

typedef struct _embedded_rec {
    long ifield;
    char *string;
    float dfield;
} embedded_rec, *embedded_rec_ptr;

static IOField embedded_field_list[] =
{
    {"ifield", "integer",
     sizeof(long), IOOffset(embedded_rec_ptr, ifield)},
    {"string field", "string",
     sizeof(char *), IOOffset(embedded_rec_ptr, string)},
    {"dfield", "float",
     sizeof(float), IOOffset(embedded_rec_ptr, dfield)},
    {NULL, NULL, 0, 0}
};

typedef struct _fifth_rec {
    embedded_rec earray[4];
} fifth_rec, *fifth_rec_ptr;

static IOField field_list5[] =
{
    {"earray", "embedded[4]",
     sizeof(embedded_rec), IOOffset(fifth_rec_ptr, earray)},
    {NULL, NULL, 0, 0}
};

typedef struct _sixth_rec {
    char *string;
    long icount;
#if SIZEOF_LONG_LONG > SIZEOF_LONG
    long long *var_int_array;
#else
    long *var_int_array;
#endif
    double dfield;
    double *var_double_array;
} sixth_rec, *sixth_rec_ptr;

static IOField field_list6[] =
{
    {"string field", "string",
     sizeof(char *), IOOffset(sixth_rec_ptr, string)},
    {"icount", "integer",
     sizeof(long), IOOffset(sixth_rec_ptr, icount)},
    {"var_int_array", "integer[icount]",
     sizeof(((sixth_rec_ptr) 0)->var_int_array[0]), IOOffset(sixth_rec_ptr, var_int_array)},
    {"double field", "float",
     sizeof(double), IOOffset(sixth_rec_ptr, dfield)},
    {"var_double_array", "float[icount]",
     sizeof(double), IOOffset(sixth_rec_ptr, var_double_array)},
    {NULL, NULL, 0, 0}
};

int
main(argc, argv)
int argc;
char **argv;
{
    IOFile iofile = NULL;
    IOFormat first_ioformat = NULL, later_ioformat = NULL;
    IOFormat nested_ioformat = NULL, structured_ioformat = NULL;
    IOFormat sixth_ioformat = NULL;
    int check_only = 0;
    int finished = 0;

    switch (argc) {
    case 1:
	iofile = open_IOfile("test_output", "r");
	break;
    case 2:
	if (strcmp(argv[1], "-check") == 0) {
	    iofile = open_IOfile("test_output", "r");
	    check_only++;
	} else {
	    iofile = open_IOfile(argv[1], "r");
	}
	break;
    case 3:
	if (strcmp(argv[1], "-check") == 0) {
	    check_only++;
	} else {
	    printf("Unknown arg \"%s\"\n", argv[1]);
	    exit(1);
	}
	iofile = open_IOfile(argv[2], "r");
	break;
    }

    if (iofile == NULL) {
	printf("Open failed\n");
	exit(1);
    }
    if (!check_only)
	dump_IOFile(iofile);

    while (!finished) {
	char *comment;
	IOFormat new_format;
	char *format_name;

	switch (next_IOrecord_type(iofile)) {
	case IOcomment:
	    comment = read_comment_IOfile(iofile);
	    printf("From IOfile -> \"%s\"\n", comment);
	    break;
	case IOformat:
	    new_format = read_format_IOfile(iofile);
	    assert(new_format != NULL);
	    format_name = name_of_IOformat(new_format);
	    if (strcmp(format_name, "first format") == 0) {
		first_ioformat = new_format;
		set_IOconversion(iofile, "first format", field_list,
				 sizeof(first_rec));
	    } else if (strcmp(format_name, "later format") == 0) {
		printf("got later format\n");
		later_ioformat = new_format;
		set_IOconversion(iofile, "later format",
				 later_field_list,
				 sizeof(later_rec));
	    } else if (strcmp(format_name, "string format") == 0) {
		printf("got string format\n");
		set_IOconversion(iofile, "string format", field_list2,
				 sizeof(second_rec));
	    } else if (strcmp(format_name, "nested format") == 0) {
		printf("got nested format\n");
		nested_ioformat = new_format;
		set_IOconversion(iofile, "nested format", nested_field_list,
				 sizeof(nested_rec));
	    } else if (strcmp(format_name, "embedded") == 0) {
		printf("got embedded format\n");
		set_IOconversion(iofile, "embedded", embedded_field_list,
				 sizeof(embedded_rec));
	    } else if (strcmp(format_name, "structured array format") == 0) {
		printf("got structured array format\n");
		structured_ioformat = new_format;
		set_IOconversion(iofile, "structured array format", field_list5,
				 sizeof(fifth_rec));
	    } else if (strcmp(format_name, "variant array format") == 0) {
		printf("got variant array format\n");
		sixth_ioformat = new_format;
		set_IOconversion(iofile, "variant array format", field_list6,
				 sizeof(sixth_rec));
	    } else {
		printf("ignoring format %s\n", format_name);
		/* else ignore the thing */
	    }
	    break;
	case IOdata:
	    if (next_IOrecord_format(iofile) == first_ioformat) {
		first_rec read_data;
		if (!read_IOfile(iofile, &read_data))
		    IOperror(iofile, "read format1");
		printf("first format rec had %d, %g, %c\n",
		       read_data.integer_field, read_data.double_field,
		       read_data.char_field);
	    } else if (next_IOrecord_format(iofile) == later_ioformat) {
		later_rec read_data;
		if (!read_IOfile(iofile, &read_data))
		    IOperror(iofile, "read later format");
		printf("later format rec had %d, %s, %g\n",
		       read_data.integer_field, read_data.string,
		       read_data.double_field);
	    } else if (next_IOrecord_format(iofile) == structured_ioformat) {
		fifth_rec read_data;
		if (!read_IOfile(iofile, &read_data))
		    fprintf(stderr, "read failed for structured format\n");
		printf("structured format rec[%d] had %ld, %s, %g\n", 0,
		       read_data.earray[0].ifield, read_data.earray[0].string, read_data.earray[0].dfield);
		printf("structured format rec[%d] had %ld, %s, %g\n", 1,
		       read_data.earray[1].ifield, read_data.earray[1].string, read_data.earray[1].dfield);
		printf("structured format rec[%d] had %ld, %s, %g\n", 2,
		       read_data.earray[2].ifield, read_data.earray[2].string, read_data.earray[2].dfield);
		printf("structured format rec[%d] had %ld, %s, %g\n", 3,
		       read_data.earray[3].ifield, read_data.earray[3].string, read_data.earray[3].dfield);
	    } else if (next_IOrecord_format(iofile) == nested_ioformat) {
		nested_rec read_data;
		if (!read_IOfile(iofile, &read_data))
		    IOperror(iofile, "read nested format");
		printf("nested format had %d { %d, %d, %ld, %s, %g, %c } %s\n",
		       read_data.integer_field,
		       read_data.nested_rec.integer_field,
		       read_data.nested_rec.short_field,
		       read_data.nested_rec.long_field,
		       read_data.nested_rec.string,
		       read_data.nested_rec.double_field,
		       read_data.nested_rec.char_field,
		       read_data.string);
	    } else if (next_IOrecord_format(iofile) == sixth_ioformat) {
		sixth_rec read_data;
		int i;
		if (!read_IOfile(iofile, &read_data))
		    IOperror(iofile, "read variant format");
		printf("variant format had %s, %ld { ", read_data.string,
		       read_data.icount);
		for (i = 0; i < read_data.icount; i++) {
		    printf(" %ld,", (long) read_data.var_int_array[i]);
		}
		printf("},\n\t %g", read_data.dfield);
		for (i = 0; i < read_data.icount; i++) {
		    printf(" %g,", read_data.var_double_array[i]);
		}
		printf("}\n");
	    } else {
		printf("discarding a record\n");
		read_raw_IOfile(iofile, NULL, 0, NULL);
	    }
	    break;
	case IOerror:
	case IOend:
	    finished++;
	    break;
	}
    }
    return 0;
}
