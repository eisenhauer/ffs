
#include "config.h"
#include <fcntl.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <string.h>
#include <assert.h>
#include "ffs.h"
#include "unix_defs.h"

#include "test_funcs.h"

static void test_receive ARGS((char *buffer, int buf_size, int finished,
			       int test_level));
static void test_all_receive ARGS((char *buffer, int buf_size, int finished));
static void write_buffer ARGS((char *buf, int size));
static void read_test_only();

static int write_output = 0;
static char *output_file = NULL;
static char *read_file = NULL;
static int fail = 0;
static char *test_only = NULL;

static IOContext rcv_context = NULL;

int
main(argc, argv)
int argc;
char **argv;
{
    IOContext src_context;
    char *xfer_buffer;
    int buf_size;
    first_rec rec1;
    first_rec array1[10];
    second_rec rec2;
    third_rec rec3;
    fourth_rec rec4;
    later_rec rec5;
    later_rec2 rec6;
    nested_rec rec7;
    fifth_rec emb_array;
    sixth_rec var_array;
    ninth_rec var_var;
    string_array_rec str_array;
    int i, j;
    IOFormat fortran_array_ioformat;

    init_written_data();

    for (i=1; i<argc; i++) {
	if (strcmp(argv[i], "-w") == 0) {
	    output_file = argv[++i];
	    write_output++;
	} else if (strcmp(argv[i], "-r") == 0) {
	    read_file = argv[++i];
	} else if (strcmp(argv[i], "-t") == 0) {
	    test_only = argv[++i];
	} else {
	    printf("Unknown argument %s\n", argv[i]);
	    printf("Usage:\n\t-w\t  write test output\n");
	    printf("\t-r file\t  rest/process test data in <file>\n");
	    printf("\t-t test\t  test only format <test>\n");
	    exit(1);
	}
    }

    if (read_file) {
	read_test_only();
	free_written_data();
	if (rcv_context != NULL) {
	    free_IOcontext(rcv_context);
	    rcv_context = NULL;
	}
	if (fail) exit(1);
	exit(0);
    }
    src_context = create_IOcontext(NULL);
    
    set_array_order_IOfile((IOFile)src_context, 1);
    fortran_array_ioformat = register_IOcontext_format("multi_array",
						     multi_array_flds, 
						     src_context);
    xfer_buffer = encode_IOcontext_buffer(src_context, fortran_array_ioformat,
					  &fortran_array, &buf_size);
    test_all_receive(xfer_buffer, buf_size, 0);
    write_buffer(xfer_buffer, buf_size);

    
    free_IOcontext(src_context);
    src_context = NULL;
    test_all_receive(NULL, 0, 1);
    write_buffer(NULL, 0);
    free_written_data();
    if (rcv_context != NULL) free_IOcontext(rcv_context);
    if (fail) exit(1);
    return 0;
}

/* NT needs O_BINARY, but it doesn't exist elsewhere */
#ifndef O_BINARY
#define O_BINARY 0
#endif

static char *
get_buffer(size_p)
int *size_p;
{
    static int file_fd = 0;
    static char *buffer = NULL;
    char *tmp_buffer;
    static int last_size = -1;
    unsigned short ssize;
    int to_read;
    int tmp_size;
    unsigned char csize;
    unsigned int beef = 0xdeadbeef;

    if (read_file == NULL) exit(1);

    if (file_fd == 0) {
	file_fd = open(read_file, O_RDONLY|O_BINARY, 0777);
	buffer = malloc(1);
    }
    if (last_size != -1) {
	if (memcmp(buffer+last_size, &beef, 4) != 0) {
	    printf("memory overwrite error\n");
	}
    }
    read(file_fd, &csize, 1);	/* low byte of 2-byte size */
    ssize = csize;
    read(file_fd, &csize, 1);	/* high byte of 2-byte size */
    ssize += ((csize << 8) & 0xff00);
    to_read = ssize;
    buffer = realloc(buffer, to_read+4);
    tmp_buffer = buffer;
    while((tmp_size = read(file_fd, tmp_buffer, to_read)) != to_read) {
	if (tmp_size == 0) {
	    free(buffer);
	    return NULL;
	} else if (tmp_size == -1) {
	    perror("Read failure");
	    free(buffer);
	    return NULL;
	}
	to_read -= tmp_size;
	tmp_buffer += tmp_size;
    } 
    last_size = ssize;
    memcpy(buffer+last_size, &beef, 4);
    if (ssize == 0) {
	free(buffer);
	close(file_fd);
	file_fd = 0;
	return NULL;
    } else {
	*size_p = ssize;
	return buffer;
    }
}

static void
read_test_only()
{
    char *input;
    int size;
    while ((input = get_buffer(&size)) != NULL) {
	test_all_receive(input, size, 0);
    }
    test_all_receive(NULL, 0, 1);
}

static IOFormat first_rec_ioformat, second_rec_ioformat, third_rec_ioformat;
static IOFormat fourth_rec_ioformat, later_rec_ioformat, nested_rec_ioformat;
static IOFormat embedded_rec_ioformat, fifth_rec_ioformat, sixth_rec_ioformat;
static IOFormat ninth_rec_ioformat, string_array_ioformat, derive_ioformat;
static IOFormat multi_array_ioformat, triangle_ioformat, add_action_ioformat;

static void
set_conversion(context, formats)
IOContext context;
IOFormat *formats;
{
    int i;
    for (i = 0; formats[i] != NULL; i++) {
	IOFormat format = formats[i];
	char *format_name;
	format_name = name_of_IOformat(format);
	if (strcmp(format_name, "first format") == 0) {
	    first_rec_ioformat = format;
	    set_conversion_IOcontext(context, format, field_list,
				     sizeof(first_rec));
	} else if (strcmp(format_name, "string format") == 0) {
	    second_rec_ioformat = format;
	    set_conversion_IOcontext(context, format, field_list2,
				     sizeof(second_rec));
	} else if (strcmp(format_name, "two string format") == 0) {
	    third_rec_ioformat = format;
	    set_conversion_IOcontext(context, format, field_list3,
				     sizeof(third_rec));
	} else if (strcmp(format_name, "internal array format") == 0) {
	    fourth_rec_ioformat = format;
	    set_conversion_IOcontext(context, format, field_list4,
				     sizeof(fourth_rec));
	} else if (strcmp(format_name, "embedded") == 0) {
	    embedded_rec_ioformat = format;
	    set_conversion_IOcontext(context, format, embedded_field_list,
				     sizeof(embedded_rec));
	} else if (strcmp(format_name, "structured array format") == 0) {
	    fifth_rec_ioformat = format;
	    set_conversion_IOcontext(context, format, field_list5,
				     sizeof(fifth_rec));
	} else if (strcmp(format_name, "variant array format") == 0) {
	    sixth_rec_ioformat = format;
	    set_conversion_IOcontext(context, format, field_list6,
				     sizeof(sixth_rec));
	} else if (strcmp(format_name, "later format") == 0) {
	    later_rec_ioformat = format;
	    set_conversion_IOcontext(context, format, later_field_list, 
				     sizeof(later_rec));
	} else if (strcmp(format_name, "nested format") == 0) {
	    nested_rec_ioformat = format;
	    set_conversion_IOcontext(context, format, nested_field_list,
				     sizeof(nested_rec));
	} else if (strcmp(format_name, "EventVecElem") == 0) {
	    set_conversion_IOcontext(context, format, event_vec_elem_fields,
				     sizeof(struct _io_encode_vec));
	} else if (strcmp(format_name, "EventV") == 0) {
	    ninth_rec_ioformat = format;
	    set_conversion_IOcontext(context, format, field_list9,
				     sizeof(ninth_rec));
	} else if (strcmp(format_name, "StringArray") == 0) {
	    string_array_ioformat = format;
	    set_conversion_IOcontext(context, format, string_array_field_list,
				     sizeof(string_array_rec));
	} else if (strcmp(format_name, "IOfield_list") == 0) {
	    set_conversion_IOcontext(context, format, field_list_flds,
				     sizeof(IOFieldList*));
	} else if (strcmp(format_name, "DEFormatList") == 0) {
	    set_conversion_IOcontext(context, format, format_list_field_list,
				     sizeof(format_list_element));
	} else if (strcmp(format_name, "channel_ID") == 0) {
	    set_conversion_IOcontext(context, format, channel_id_flds,
				     sizeof(channel_ID_struct));
	} else if (strcmp(format_name, "Channel Derive") == 0) {
	    derive_ioformat = format;
	    set_conversion_IOcontext(context, format, derive_msg_field_list,
				     sizeof(DeriveMsg));
	} else if (strcmp(format_name, "multi_array") == 0) {
	    multi_array_ioformat = format;
	    set_conversion_IOcontext(context, format, multi_array_flds,
				     sizeof(multi_array));
	} else if (strcmp(format_name, "compressed_mesh_param") == 0) {
	    triangle_ioformat = format;
	    set_conversion_IOcontext(context, format, compressed_mesh,
				     sizeof(compressed_mesh_param));
	} else if (strcmp(format_name, "triangle_param") == 0) {
	    triangle_ioformat = format;
	    set_conversion_IOcontext(context, format, triangle_field,
				     sizeof(triangle_param));
	} else if (strcmp(format_name, "XMLFormatList") == 0) {
	    set_conversion_IOcontext(context, format, xml_format_list_flds,
				     sizeof(msg_format_list_element));
	} else if (strcmp(format_name, "add_action") == 0) {
	    add_action_ioformat = format;
	    set_conversion_IOcontext(context, format, add_field_list,
				     sizeof(add_rec));
	} else {
	    printf("Got unexpected format %s\n", format_name);
	}
    }
}


int base_size_func(IOContext context, char *src, int rec_len,
		   int native_struct_size)
{
    return native_struct_size;
}

int total_size_func(IOContext context, char *src, int rec_len, 
		    int native_struct_size)
{
    return this_IOrecord_length(context, src, rec_len);
}

static int 
decode_in_place(IOContext context, char *src, int src_len, void *dest)
{
    if (decode_in_place_possible(get_format_IOcontext(context, src))) {
	int ret, header_len;
	char *real_dest;
	ret = decode_in_place_IOcontext(context, src, (void**)&real_dest);
	header_len = real_dest - src;
	memcpy(dest, real_dest, src_len - header_len);
	return ret;
    } else {
	return decode_to_buffer_IOcontext(context, src, dest);
    }
}

static int
decode_IOcontext_wrapper(IOContext context, char *src, int src_len, void *dest)
{
    return decode_IOcontext(context, src, dest);
}

static int
decode_to_buffer_IOcontext_wrapper(IOContext context, char *src, int src_len,
				   void *dest)
{
    return decode_to_buffer_IOcontext(context, src, dest);
}

typedef int (*size_func_t) ARGS((IOContext context, char *src, int buf_size, 
				   int nativ_struct));

typedef int (*decode_func_t) ARGS((IOContext context, char *src, int src_len, 
				   void *dest));

size_func_t size_funcs[] = {base_size_func, total_size_func, total_size_func};
decode_func_t decode_funcs[] = {decode_IOcontext_wrapper, 
				decode_to_buffer_IOcontext_wrapper,
				decode_in_place};

#define NUM_TESTS 3

static void
test_all_receive(buffer, buf_size, finished)
char *buffer;
int buf_size;
int finished;
{
    int test_type = 0;
    char *tmp_buffer = malloc(buf_size);
    for ( test_type = 0; test_type < NUM_TESTS; test_type++) {
	memcpy(tmp_buffer, buffer, buf_size);
	test_receive(tmp_buffer, buf_size, finished, test_type);
    }
    free(tmp_buffer);
}
	
static void*
get_mem(size)
int size;
{
    char *buffer;
    unsigned int beef = 0xdeadbeef;

    buffer = malloc(size+4);
    memcpy(buffer+size, &beef, 4);
    return buffer;
}

static void
check_mem(size, buffer)
int size;
char *buffer;
{
    unsigned int beef = 0xdeadbeef;
    if (memcmp(buffer+size, &beef, 4) != 0) {
	printf("memory overwrite error\n");
    }
}

    
static void
test_receive(buffer, buf_size, finished, test_level)
char *buffer;
int buf_size;
int finished;
int test_level;
{
    static IOContext iocontext = NULL;
    static int first_rec_count[NUM_TESTS] = {0,0,0};
    static int second_rec_count[NUM_TESTS] = {0,0,0};
    static int third_rec_count[NUM_TESTS] = {0,0,0};
    static int fourth_rec_count[NUM_TESTS] = {0,0,0};
    static int fifth_rec_count[NUM_TESTS] = {0,0,0};
    static int sixth_rec_count[NUM_TESTS] = {0,0,0};
    static int nested_rec_count[NUM_TESTS] = {0,0,0};
    static int later_rec_count[NUM_TESTS] = {0,0,0};
    static int ninth_rec_count[NUM_TESTS] = {0,0,0};
    static int string_array_count[NUM_TESTS] = {0,0,0};

    static int unknown_rec_count[NUM_TESTS] = {0,0,0};
    size_func_t size_func = size_funcs[test_level];
    decode_func_t decode_func = decode_funcs[test_level];
    if (iocontext == NULL) {
	iocontext = create_IOcontext();
	rcv_context = iocontext;
    }
    if (!finished) {
	IOFormat buffer_format = get_format_IOcontext(iocontext, buffer);

	if (buffer_format == NULL) {
	    printf("No format!\n");
	    exit(1);
	}
	if (!has_conversion_IOformat(buffer_format)) {
	    IOFormat *buffer_formats;
	    buffer_formats = get_subformats_IOcontext(iocontext, buffer);
	    set_conversion(iocontext, buffer_formats);
	    free(buffer_formats);
	}
	if (((test_only == NULL) || (strcmp(test_only, "first_rec") == 0)) &&
	    (buffer_format == first_rec_ioformat)) {
	    first_rec read_data[10];
	    memset(&read_data[0], 0, sizeof(first_rec));
	    if (!decode_func(iocontext, buffer, buf_size, &read_data[0]))
		IOperror((IOFile)iocontext, "read first data");
	    if (!first_rec_eq(&read_data[0], &rec1_array[first_rec_count[test_level]++])) {
		printf("Rec1 failure\n");
		fail++;
	    }
	} else if (((test_only == NULL) || (strcmp(test_only, "second_rec") == 0)) &&
		   (buffer_format == second_rec_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, sizeof(second_rec));
	    second_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read second data failed");
	    if (!second_rec_eq(read_data, &rec2_array[second_rec_count[test_level]++])) {
		printf("Rec2 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "third_rec") == 0)) &&
		   (buffer_format == third_rec_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, sizeof(third_rec));
	    third_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read third data failed");
	    if (!third_rec_eq(read_data, &rec3_array[third_rec_count[test_level]++])) {
		printf("Rec3 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "fourth_rec") == 0)) &&
		   (buffer_format == fourth_rec_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, sizeof(fourth_rec));
	    fourth_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read fourth data failed");
	    if (!fourth_rec_eq(read_data, &rec4)) {
		printf("Rec4 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	    fourth_rec_count[test_level]++;
	} else if (buffer_format == embedded_rec_ioformat) {
	    
	    printf("Emb Rec failure\n");
	    fail++;
	} else if (((test_only == NULL) || (strcmp(test_only, "fifth_rec") == 0)) &&
		   (buffer_format == fifth_rec_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, sizeof(fifth_rec));
	    fifth_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read fifth data failed");
	    if (!fifth_rec_eq(read_data, &rec5)) {
		printf("Rec5 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	    fifth_rec_count[test_level]++;
	} else if (((test_only == NULL) || (strcmp(test_only, "sixth_rec") == 0)) &&
		   (buffer_format == sixth_rec_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, sizeof(sixth_rec));
	    sixth_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read variant format");
	    if (!sixth_rec_eq(read_data, &rec6_array[sixth_rec_count[test_level]++])) {
		printf("Rec6 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "nested_rec") == 0)) &&
		   (buffer_format == nested_rec_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, sizeof(nested_rec));
	    nested_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read variant format");
	    if (!nested_rec_eq(read_data, &rec7_array[nested_rec_count[test_level]++])) {
		printf("Rec7 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "later_rec") == 0)) &&
		   (buffer_format == later_rec_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, sizeof(later_rec));
	    later_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read variant format");
	    if (!later_rec_eq(read_data, &rec8_array[later_rec_count[test_level]++])) {
		printf("Rec8 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "ninth_rec") == 0)) &&
		   (buffer_format == ninth_rec_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, sizeof(ninth_rec));
	    ninth_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read variant format");
	    if (!ninth_rec_eq(read_data, &rec9_array[ninth_rec_count[test_level]++])) {
		printf("Rec9 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "string_array") == 0)) &&
		   (buffer_format == string_array_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, 
				 sizeof(string_array_rec));
	    string_array_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read string array format");
	    if (!string_array_eq(read_data, 
				 &string_array_array[string_array_count[test_level]++])) {
		printf("string array failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "derive") == 0)) &&
		   (buffer_format == derive_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, 
				 sizeof(DeriveMsg));
	    DeriveMsgPtr read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read decode msg format");
	    if (!derive_eq(read_data, &derive)) {
		printf("derive msg failure, decode func %d\n", test_level);
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "multi_array") == 0)) &&
		   (buffer_format == multi_array_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, 
				 sizeof(multi_array));
	    multi_array_rec2 *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read decode msg format");
	    if (!multi_array_eq((multi_array_rec*)read_data, (multi_array_rec*)&multi_array2)) {
		printf("multi array failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "triangle_param") == 0)) &&
		   (buffer_format == triangle_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, 
				 sizeof(triangle_param));
	    triangle_param *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read decode msg format");
	    if (!triangle_param_eq(read_data, &triangle)) {
		printf("triangle param failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "add_action") == 0)) &&
		   (buffer_format == add_action_ioformat)) {
	    int size = size_func(iocontext, buffer, buf_size, 
				 sizeof(add_rec));
	    add_rec_ptr read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(iocontext, buffer, buf_size, read_data))
		IOperror((IOFile)iocontext, "read decode msg format");
	    if (!add_rec_eq(read_data, &add_action_record)) {
		printf("add rec failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (test_only == NULL) {
	    printf("discarding a record\n");
	    unknown_rec_count[test_level]++;
	}
    } else {
	/* finished */
	if (test_level == 0) {
	    free_IOcontext(iocontext);
	    iocontext = NULL;
	}
	if (test_only != NULL) return;
	if (unknown_rec_count[test_level] != 0) {
	    printf("Got unknown\n");
	    fail++;
	}
    }
}

static void
write_buffer(buf, size)
char *buf;
int size;
{
    static int file_fd = 0;
    unsigned short ssize;
    unsigned char csize;
    if (output_file == NULL) return;

    if (file_fd == 0) {
	file_fd = open(output_file, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0777);
    }
    ssize = size;
    csize = ssize & 0xff;
    write(file_fd, &csize, 1);	/* low byte of 2-byte size */
    csize = ((ssize >> 8) & 0xff);
    write(file_fd, &csize, 1);	/* high byte of 2-byte size */
    write(file_fd, buf, size);
}
