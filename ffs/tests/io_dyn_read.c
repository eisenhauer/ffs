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
#include "test_funcs.h"

extern char *data_type_to_str();

int
main(argc, argv)
int argc;
char **argv;
{
    FFSFile file = NULL;
    FFSTypeHandle first_ioformat = NULL, later_ioformat = NULL;
    FFSTypeHandle nested_ioformat = NULL, structured_ioformat = NULL;
    FFSTypeHandle sixth_ioformat = NULL;
    int check_only = 0;
    int finished = 0;

    switch (argc) {
    case 1:
	file = open_FFSfile("test_output", "r");
	break;
    case 2:
	if (strcmp(argv[1], "-check") == 0) {
	    file = open_FFSfile("test_output", "r");
	    check_only++;
	} else {
	    file = open_FFSfile(argv[1], "r");
	}
	break;
    case 3:
	if (strcmp(argv[1], "-check") == 0) {
	    check_only++;
	} else {
	    printf("Unknown arg \"%s\"\n", argv[1]);
	    exit(1);
	}
	file = open_FFSfile(argv[2], "r");
	break;
    }

    if (file == NULL) {
	printf("Open failed\n");
	exit(1);
    }
    if (!check_only)
	dump_File(file);

    first_ioformat = FFSset_fixed_target(file, first_format_list);
    nested_ioformat = FFSset_fixed_target(file, nested_format_list);
    sixth_ioformat = FFSset_fixed_target(file, variant_format_list);

    while (!finished) {
	char *comment;
	FFSTypeHandle new_format;
	char *format_name;

	if (FFSnext_data_handle(file) == first_ioformat) {
	    first_rec read_data;
	    if (!read_File(file, &read_data))
		IOperror(file, "read format1");
	    printf("first format rec had %d, %g, %c\n",
		   read_data.integer_field, read_data.double_field,
		   read_data.char_field);
	} else if (FFSnext_data_handle(file) == later_ioformat) {
	    later_rec read_data;
	    if (!read_File(file, &read_data))
		IOperror(file, "read later format");
	    printf("later format rec had %d, %s, %g\n",
		   read_data.integer_field, read_data.string,
		   read_data.double_field);
	} else if (FFSnext_data_handle(file) == nested_ioformat) {
	    nested_rec read_data;
	    if (!read_File(file, &read_data))
		IOperror(file, "read nested format");
	    printf("nested format had %d { %d, %d, %ld, %s, %g, %c } %s\n",
		   read_data.integer_field,
		   read_data.nested_rec.integer_field,
		   read_data.nested_rec.short_field,
		   read_data.nested_rec.long_field,
		   read_data.nested_rec.string,
		   read_data.nested_rec.double_field,
		   read_data.nested_rec.char_field,
		   read_data.string);
	} else if (FFSnext_data_handle(file) == sixth_ioformat) {
	    sixth_rec read_data;
	    int i;
	    if (!read_File(file, &read_data))
		IOperror(file, "read variant format");
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
	    read_raw_File(file, NULL, 0, NULL);
	}
    }
    return 0;
}
