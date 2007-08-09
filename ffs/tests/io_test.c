#include "config.h"
#include <assert.h>
#include <fcntl.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <string.h>
#include "ffs.h"
#include "unix_defs.h"

#include "test_funcs.h"

static int verbose = 0;
static char *test_only = NULL;

int
size_func_sizeof(iofile, size)
IOFile iofile;
int size;
{
    return size;
}

int
size_func_next_size(iofile, size)
IOFile iofile;
int size;
{
    return next_IOrecord_length(iofile);
}

int
read_func_no_buffer(iofile, data, size)
IOFile iofile;
void *data;
int size;
{
    return read_IOfile(iofile, data);
}

int
read_func_buffer(iofile, data, size)
IOFile iofile;
void *data;
int size;
{
    return read_to_buffer_IOfile(iofile, data, size);
}


void
do_test(input_file, size_func, read_func)
char *input_file;
int (*size_func) (IOFile, int);
int (*read_func) ();
{
    IOFile iofile;
    IOFormat first_rec_ioformat = NULL, second_rec_ioformat = NULL;
    IOFormat third_rec_ioformat = NULL, fourth_rec_ioformat = NULL;
    IOFormat later_rec_ioformat = NULL, nested_rec_ioformat = NULL;
    IOFormat embedded_rec_ioformat = NULL, fifth_rec_ioformat = NULL;
    IOFormat sixth_rec_ioformat = NULL, ninth_rec_ioformat = NULL;
    IOFormat string_array_ioformat = NULL;
    int finished = 0;
    int comment_count = 0;
    int first_rec_count = 0;
    int second_rec_count = 0;
    int third_rec_count = 0;
    int fourth_rec_count = 0;
    int fifth_rec_count = 0;
    int sixth_rec_count = 0;
    int nested_rec_count = 0;
    int later_rec_count = 0;
    int ninth_rec_count = 0;
    int string_array_rec_count = 0;

    int unknown_rec_count = 0;
    iofile = open_IOfile(input_file, "r");

    init_written_data();

    while (!finished) {
	char *comment;
	IOFormat new_format;
	char *format_name;

	switch (next_IOrecord_type(iofile)) {
	case IOcomment:
	    comment = read_comment_IOfile(iofile);
	    if (strcmp(comment, comment_array[comment_count++]) != 0) {
		printf("From IOfile -> \"%s\"\n", comment);
		exit(1);
	    }
	    break;
	case IOformat:
	    new_format = read_format_IOfile(iofile);
	    assert(new_format != NULL);
	    format_name = name_of_IOformat(new_format);
	    if (((test_only == NULL) || (strcmp(test_only, "first") == 0)) &&
		(strcmp(format_name, "first format") == 0)) {
		first_rec_ioformat = new_format;
		set_IOconversion(iofile, "first format", field_list,
				 sizeof(first_rec));
	    } else if (((test_only == NULL) || (strcmp(test_only, "string") == 0)) &&
		       (strcmp(format_name, "string format") == 0)) {
		second_rec_ioformat = new_format;
		set_IOconversion(iofile, "string format", field_list2,
				 sizeof(second_rec));
	    } else if (((test_only == NULL) || (strcmp(test_only, "two string") == 0)) &&
		       (strcmp(format_name, "two string format") == 0)) {
		third_rec_ioformat = new_format;
		set_IOconversion(iofile, "two string format", field_list3,
				 sizeof(third_rec));
	    } else if (((test_only == NULL) || (strcmp(test_only, "internal array") == 0)) &&
		       (strcmp(format_name, "internal array format") == 0)) {
		fourth_rec_ioformat = new_format;
		set_IOconversion(iofile, "internal array format", field_list4,
				 sizeof(fourth_rec));
	    } else if (((test_only == NULL) || (strcmp(test_only, "embedded") == 0)
			|| (strcmp(test_only, "structured array") == 0)) &&
		       (strcmp(format_name, "embedded") == 0)) {
		embedded_rec_ioformat = new_format;
		set_IOconversion(iofile, "embedded", embedded_field_list,
				 sizeof(embedded_rec));
	    } else if (((test_only == NULL) || (strcmp(test_only, "structured array") == 0)) &&
		       (strcmp(format_name, "structured array format") == 0)) {
		fifth_rec_ioformat = new_format;
		set_IOconversion(iofile, "structured array format", field_list5,
				 sizeof(fifth_rec));
	    } else if (((test_only == NULL) || (strcmp(test_only, "variant array") == 0)) &&
		       (strcmp(format_name, "variant array format") == 0)) {
		sixth_rec_ioformat = new_format;
		set_IOconversion(iofile, "variant array format", field_list6,
				 sizeof(sixth_rec));
	    } else if (((test_only == NULL) || (strcmp(test_only, "later") == 0)) &&
		       (strcmp(format_name, "later format") == 0)) {
		later_rec_ioformat = new_format;
		set_IOconversion(iofile, "later format",
				 later_field_list,
				 sizeof(later_rec));
	    } else if (((test_only == NULL) || (strcmp(test_only, "nested") == 0)) &&
		       (strcmp(format_name, "nested format") == 0)) {
		nested_rec_ioformat = new_format;
		set_IOconversion(iofile, "nested format", nested_field_list,
				 sizeof(nested_rec));
	    } else if (((test_only == NULL) || (strcmp(test_only, "EventVec") == 0)) &&
		       (strcmp(format_name, "EventVecElem") == 0)) {
		set_IOconversion(iofile, "EventVecElem", event_vec_elem_fields,
				 sizeof(struct _io_encode_vec));
	    } else if (((test_only == NULL) || (strcmp(test_only, "EventV") == 0)) &&
		       (strcmp(format_name, "EventV") == 0)) {
		ninth_rec_ioformat = new_format;
		set_IOconversion(iofile, "EventV", field_list9,
				 sizeof(ninth_rec));
	    } else if (((test_only == NULL) || (strcmp(test_only, "string_array") == 0)) &&
		       (strcmp(format_name, "string_array") == 0)) {
		string_array_ioformat = new_format;
		set_IOconversion(iofile, "string_array", 
				 string_array_field_list,
				 sizeof(string_array_rec));
	    } else {
		if (test_only == NULL) {
		    printf("Got unexpected format %s\n", format_name);
		    exit(1);
		}
	    }
	    break;
	case IOdata:
	    if (verbose) 
		printf("Record of format %s\n",
		       name_of_IOformat(next_IOrecord_format(iofile)));
	    if (next_IOrecord_format(iofile) == first_rec_ioformat) {
		first_rec read_data[10];
		if (next_IOrecord_count(iofile) == 1) {
		    memset(&read_data[0], 0, sizeof(first_rec));
		    if (!read_IOfile(iofile, &read_data[0]))
			IOperror(iofile, "read first data");
		    if (memcmp(&read_data[0], &rec1_array[first_rec_count++],
			       sizeof(first_rec)) != 0) {
			if (read_data[0].integer_field != 
			    rec1_array[first_rec_count-1].integer_field) {
			    printf("read integer = %d (0x%x), exp integer = %d (0x%x)\n",
				   read_data[0].integer_field, 
				   read_data[0].integer_field, 
				   rec1_array[first_rec_count-1].integer_field,
				   rec1_array[first_rec_count-1].integer_field);
			}
			if (read_data[0].double_field != 
			    rec1_array[first_rec_count-1].double_field) {
			    printf("read double = %g, exp double = %g\n",
				   read_data[0].double_field, 
				   rec1_array[first_rec_count-1].double_field);
			}
			if (read_data[0].char_field != 
			    rec1_array[first_rec_count-1].char_field) {
			    printf("read char = %c, exp char = %c\n",
				   read_data[0].char_field, 
				   rec1_array[first_rec_count-1].char_field);
			}
			printf("Rec1 failure\n");
			exit(1);
		    }
		} else {
		    int count = next_IOrecord_count(iofile);
		    if (count == 10) {
			memset(&read_data[0], 0, sizeof(first_rec) * 10);
			if (read_array_IOfile(iofile, &read_data[0], 10, sizeof(first_rec)) != 10)
			    IOperror(iofile, "read first array");
			if (memcmp(&read_data[0], &rec1_array[first_rec_count],
				   sizeof(first_rec) * 10) != 0) {
			    printf("Rec1 array failure\n");
			    exit(1);
			}
			first_rec_count += 10;
		    } else {
			printf("Rec1 failure\n");
			exit(1);
		    }
		}
	    } else if (next_IOrecord_format(iofile) == second_rec_ioformat) {
		int size = size_func(iofile, sizeof(second_rec));
		second_rec *read_data = malloc(size);
		memset(read_data, 0, size);
		if (!read_func(iofile, read_data, size))
		    IOperror(iofile, "read second data failed");
		if (!second_rec_eq(read_data, &rec2_array[second_rec_count++])) {
		    printf("Rec2 failure\n");
		    exit(1);
		}
		free(read_data);
	    } else if (next_IOrecord_format(iofile) == third_rec_ioformat) {
		int size = size_func(iofile, sizeof(third_rec));
		third_rec *read_data = malloc(size);
		memset(read_data, 0, size);
		if (!read_func(iofile, read_data, size))
		    IOperror(iofile, "read third data failed");
		if (!third_rec_eq(read_data, &rec3_array[third_rec_count++])) {
		    printf("Rec3 failure\n");
		    exit(1);
		}
		free(read_data);
	    } else if (next_IOrecord_format(iofile) == fourth_rec_ioformat) {
		int size = size_func(iofile, sizeof(fourth_rec));
		fourth_rec *read_data = malloc(size);
		memset(read_data, 0, size);
		if (!read_func(iofile, read_data, size))
		    IOperror(iofile, "read fourth data failed");
		if (!fourth_rec_eq(read_data, &rec4)) {
		    printf("Rec4 failure\n");
		    exit(1);
		}
		free(read_data);
		fourth_rec_count++;
	    } else if (next_IOrecord_format(iofile) == embedded_rec_ioformat) {

		printf("Emb Rec failure\n");
		exit(1);
	    } else if (next_IOrecord_format(iofile) == fifth_rec_ioformat) {
		int size = size_func(iofile, sizeof(fifth_rec));
		fifth_rec *read_data = malloc(size);
		memset(read_data, 0, size);
		if (!read_func(iofile, read_data, size))
		    IOperror(iofile, "read fifth data failed");
		if (!fifth_rec_eq(read_data, &rec5)) {
		    printf("Rec5 failure\n");
		    exit(1);
		}
		free(read_data);
		fifth_rec_count++;
	    } else if (next_IOrecord_format(iofile) == sixth_rec_ioformat) {
		int size = size_func(iofile, sizeof(sixth_rec));
		sixth_rec *read_data = malloc(size);
		memset(read_data, 0, size);
		if (!read_func(iofile, read_data, size))
		    IOperror(iofile, "read variant format");
		if (!sixth_rec_eq(read_data, &rec6_array[sixth_rec_count++])) {
		    printf("Rec6 failure\n");
		    exit(1);
		}
		free(read_data);
	    } else if (next_IOrecord_format(iofile) == nested_rec_ioformat) {
		int size = size_func(iofile, sizeof(nested_rec));
		nested_rec *read_data = malloc(size);
		memset(read_data, 0, size);
		if (!read_func(iofile, read_data, size))
		    IOperror(iofile, "read variant format");
		if (!nested_rec_eq(read_data, &rec7_array[nested_rec_count++])) {
		    printf("Rec7 failure\n");
		    exit(1);
		}
		free(read_data);
	    } else if (next_IOrecord_format(iofile) == later_rec_ioformat) {
		int size = size_func(iofile, sizeof(later_rec));
		later_rec *read_data = malloc(size);
		memset(read_data, 0, size);
		if (!read_func(iofile, read_data, size))
		    IOperror(iofile, "read variant format");
		if (!later_rec_eq(read_data, &rec8_array[later_rec_count++])) {
		    printf("Rec8 failure\n");
		    exit(1);
		}
		free(read_data);
	    } else if (next_IOrecord_format(iofile) == ninth_rec_ioformat) {
		int size = size_func(iofile, sizeof(ninth_rec));
		ninth_rec *read_data = malloc(size);
		memset(read_data, 0, size);
		if (!read_func(iofile, read_data, size))
		    IOperror(iofile, "read variant format");
		if (!ninth_rec_eq(read_data, &rec9_array[ninth_rec_count++])) {
		    printf("Rec9 failure\n");
		    exit(1);
		}
		free(read_data);
	    } else if (next_IOrecord_format(iofile) == string_array_ioformat) {
		int size = size_func(iofile, sizeof(string_array_rec));
		string_array_rec *read_data = malloc(size);
		memset(read_data, 0, size);
		if (!read_func(iofile, read_data, size))
		    IOperror(iofile, "read variant format");
		if (!string_array_eq(read_data, &string_array_array[string_array_rec_count++])) {
		    printf("string_array failure\n");
		    exit(1);
		}
		free(read_data);

	    } else {
		if (test_only == NULL) {
		    printf("discarding a record\n");
		}
		read_raw_IOfile(iofile, NULL, 0, NULL);
		unknown_rec_count++;
	    }
	    break;
	case IOerror:
	case IOend:
	    finished++;
	    break;
	}
    }

    close_IOfile(iofile);
    free_IOfile(iofile);
    free_written_data();
    if (first_rec_count != sizeof(rec1_array) / sizeof(rec1_array[0])) {
	printf("Missed first\n");
	exit(1);
    }
    if (second_rec_count != sizeof(rec2_array) / sizeof(rec2_array[0])) {
	printf("Missed second\n");
	exit(1);
    }
    if (third_rec_count != sizeof(rec3_array) / sizeof(rec3_array[0])) {
	printf("Missed third\n");
	exit(1);
    }
    if (fourth_rec_count != 1) {
	printf("Missed fourth\n");
	exit(1);
    }
    if (fifth_rec_count != 1) {
	printf("Missed fifth\n");
	exit(1);
    }
    if (sixth_rec_count != sizeof(rec6_array) / sizeof(rec6_array[0])) {
	printf("Missed sixth\n");
	exit(1);
    }
    if (nested_rec_count != sizeof(rec7_array) / sizeof(rec7_array[0])) {
	printf("Missed sixth\n");
	exit(1);
    }
    if (later_rec_count != 3) {
	printf("Missed later\n");
	exit(1);
    }
    if (unknown_rec_count != 0) {
	printf("Got unknown\n");
	exit(1);
    }
    if (ninth_rec_count != 5) {
	printf("missed rec9\n");
	exit(1);
    }
    if ((string_array_rec_count != 5) && (string_array_rec_count != 0)){
	printf("missed string_array\n");
	exit(1);
    }
}


int
main(argc, argv)
int argc;
char **argv;
{
    int i;
    char *input_file = "test_output";

    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    /* argument */
	    if (argv[i][1] == 'v') {
		verbose++;
	    } else if (argv[i][1] == 't') {
		test_only = argv[++i];
	    } else {
		printf("Unknown argument \"%s\"\n", argv[i]);
	    }
	} else {
	    input_file = argv[i];
	}
    }
    do_test(input_file, size_func_sizeof, read_func_no_buffer);
    do_test(input_file, size_func_next_size, read_func_buffer);
    return 0;
}
