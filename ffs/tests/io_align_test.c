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


extern void 
local_align_field_list(IOFieldList field_list, int pointer_size);

void
do_test(IOFile iofile, char *name, IOFieldList fields)
{
    int i = 0;
    IOFieldList tmp = copy_field_list(fields);

    local_align_field_list(tmp, sizeof(char*));
    while(fields[i].field_name != NULL) {
	if (fields[i].field_offset != tmp[i].field_offset) {
	    printf("Failed alignment of field %s in format %s, got %d, expected %d\n",
		   fields[i].field_name, name, tmp[i].field_offset,
		   fields[i].field_offset);
	}
	i++;
    }
}


int
main(argc, argv)
int argc;
char **argv;
{
    IOFile iofile;
    do_test(iofile, "first format", field_list);
    do_test(iofile, "string format", field_list2);
    do_test(iofile, "two string format", field_list3);
    do_test(iofile, "internal array format", field_list4);
    do_test(iofile, "embedded", embedded_field_list);
    do_test(iofile, "structured array format", field_list5);
    do_test(iofile, "variant array format", field_list6);
    do_test(iofile, "later format", later_field_list);
    do_test(iofile, "nested format", nested_field_list);
    do_test(iofile, "EventVecElem", event_vec_elem_fields);
    do_test(iofile, "EventV", field_list9);
    do_test(iofile, "string_array", string_array_field_list);
    return 0;
}
