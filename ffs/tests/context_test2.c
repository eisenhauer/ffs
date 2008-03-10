
#include "config.h"
#include <fcntl.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#include <stdio.h>
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

#include "test_funcs.h"

static void test_receive(char *buffer, int buf_size, int finished,
			       int test_level);
static void test_all_receive(char *buffer, int buf_size, int finished);
static void write_buffer(char *buf, int size);
static void read_test_only();

static int write_output = 0;
static char *output_file = NULL;
static char *read_file = NULL;
static int fail = 0;
static char *test_only = NULL;

static FFSContext rcv_context = NULL;

int
main(argc, argv)
int argc;
char **argv;
{
    FMContext src_context;
    FFSBuffer encode_buffer;
    FMFormat first_rec_ioformat, second_rec_ioformat, third_rec_ioformat;
    FMFormat fourth_rec_ioformat, later_ioformat, nested_ioformat;
    FMFormat embedded_rec_ioformat, fifth_rec_ioformat, triangle_ioformat;
    FMStructDescRec str_list[5];
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
    struct node nodes[10];
    struct visit_table v;
    FMFormat sixth_rec_ioformat, ninth_rec_ioformat, string_array_ioformat;
    FMFormat derive_ioformat, multi_array_ioformat, add_action_ioformat;
    FMFormat node_ioformat;

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
/*	    free_FFScontext(rcv_context);*/
	    rcv_context = NULL;
	}
	if (fail) exit(1);
	exit(0);
    }
    src_context = create_FMcontext();
    encode_buffer = create_FFSBuffer();

    str_list[0].format_name = "node";
    str_list[0].field_list = node_field_list;
    str_list[0].struct_size = sizeof(struct node);
    str_list[0].opt_info = NULL;
    str_list[1].format_name = NULL;
    node_ioformat = register_data_format(src_context, str_list);

    for (i = 0; i < sizeof(nodes)/sizeof(nodes[0]); i++) {
	nodes[i].node_num = i;
	nodes[i].link1 = nodes[i].link2 = NULL;
    }

    for (i=0; i <  sizeof(nodes)/sizeof(nodes[0]) - 1; i++) {
	nodes[i].link1 = &nodes[i+1];
    }
/*    nodes[0].link2 = &nodes[sizeof(nodes)/sizeof(nodes[0])-1];*/
    v.node_count = 0;
    nodes[0].node_num = calc_signature(&nodes[0], &v);
    xfer_buffer = FFSencode(encode_buffer, node_ioformat,
					  &nodes[0], &buf_size);
    test_all_receive(xfer_buffer, buf_size, 0);
    write_buffer(xfer_buffer, buf_size);

    nodes[0].link2 = NULL;
    nodes[sizeof(nodes)/sizeof(nodes[0]) - 1].link1 = &nodes[2];
    v.node_count = 0;
    nodes[0].node_num = 0;
    nodes[0].node_num = calc_signature(&nodes[0], &v);
    xfer_buffer = FFSencode(encode_buffer, node_ioformat,
					  &nodes[0], &buf_size);
    test_all_receive(xfer_buffer, buf_size, 0);
    write_buffer(xfer_buffer, buf_size);

    for (i=0; i <  sizeof(nodes)/sizeof(nodes[0]) - 1; i++) {
	nodes[i].link1 = nodes[i].link2 = NULL;
    }
    nodes[0].link1 = &nodes[1];
    nodes[0].link2 = &nodes[2];
    nodes[1].link1 = &nodes[3];
    nodes[1].link2 = &nodes[4];
    nodes[2].link1 = &nodes[5];
    nodes[2].link2 = &nodes[6];
    nodes[3].link1 = &nodes[7];
    nodes[3].link2 = &nodes[8];
    nodes[4].link1 = &nodes[9];

    v.node_count = 0;
    nodes[0].node_num = 0;
    nodes[0].node_num = calc_signature(&nodes[0], &v);
    xfer_buffer = FFSencode(encode_buffer, node_ioformat,
					  &nodes[0], &buf_size);
    test_all_receive(xfer_buffer, buf_size, 0);
    write_buffer(xfer_buffer, buf_size);
    
    free_FMcontext(src_context);
    free_FFSBuffer(encode_buffer);
    src_context = NULL;
    test_all_receive(NULL, 0, 1);
    write_buffer(NULL, 0);
    free_written_data();
    if (rcv_context != NULL) free_FFSContext(rcv_context);
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

static FFSTypeHandle first_rec_ioformat, second_rec_ioformat, third_rec_ioformat;
static FFSTypeHandle fourth_rec_ioformat, later_rec_ioformat, nested_rec_ioformat;
static FFSTypeHandle embedded_rec_ioformat, fifth_rec_ioformat, sixth_rec_ioformat;
static FFSTypeHandle ninth_rec_ioformat, string_array_ioformat, derive_ioformat;
static FFSTypeHandle multi_array_ioformat, triangle_ioformat, add_action_ioformat;
static FFSTypeHandle node_ioformat;

static void
set_targets(context)
FFSContext context;
{
    first_rec_ioformat = FFSset_fixed_target(context, first_format_list);
    second_rec_ioformat = FFSset_fixed_target(context, string_format_list);
    third_rec_ioformat = FFSset_fixed_target(context, two_string_format_list);
    fourth_rec_ioformat = FFSset_fixed_target(context, fourth_format_list);
    fifth_rec_ioformat = FFSset_fixed_target(context, structured_format_list);
    sixth_rec_ioformat = FFSset_fixed_target(context, variant_format_list);
    later_rec_ioformat = FFSset_fixed_target(context, later_format_list);
    nested_rec_ioformat = FFSset_fixed_target(context, nested_format_list);
    ninth_rec_ioformat = FFSset_fixed_target(context, ninth_format_list);
    string_array_ioformat = FFSset_fixed_target(context, string_array_format_list);
    derive_ioformat = FFSset_fixed_target(context, derive_format_list);
    multi_array_ioformat = FFSset_fixed_target(context, multi_array_format_list);
    triangle_ioformat = FFSset_fixed_target(context, triangle_format_list);
    add_action_ioformat = FFSset_fixed_target(context, add_action_format_list);
    node_ioformat = FFSset_fixed_target(context, node_format_list);
}

int base_size_func(FFSContext context, char *src, int rec_len,
		   int native_struct_size)
{
    return native_struct_size;
}

int total_size_func(FFSContext context, char *src, int rec_len, 
		    int native_struct_size)
{
    return FFS_est_decode_length(context, src, rec_len);
}

static int 
decode_in_place(FFSContext context, char *src, int src_len, void *dest)
{
    if (decode_in_place_possible(FFSTypeHandle_from_encode(context, src))) {
	int ret, header_len;
	char *real_dest;
	ret = FFSdecode_in_place(context, src, (void**)&real_dest);
	header_len = real_dest - src;
	memcpy(dest, real_dest, src_len - header_len);
	return ret;
    } else {
	return FFSdecode_to_buffer(context, src, dest);
    }
}

static int
decode_IOcontext_wrapper(FFSContext context, char *src, int src_len, void *dest)
{
    return FFSdecode(context, src, dest);
}

static int
decode_to_buffer_IOcontext_wrapper(FFSContext context, char *src, int src_len,
				   void *dest)
{
    return FFSdecode_to_buffer(context, src, dest);
}

typedef int (*size_func_t)(FFSContext context, char *src, int buf_size, 
				   int nativ_struct);

typedef int (*decode_func_t)(FFSContext context, char *src, int src_len, 
				   void *dest);

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
    static FFSContext c = NULL;
/*    static int comment_count[NUM_TESTS] = {0,0,0};*/
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
    static int node_rec_count[NUM_TESTS] = {0,0,0};

    static int unknown_rec_count[NUM_TESTS] = {0,0,0};
    size_func_t size_func = size_funcs[test_level];
    decode_func_t decode_func = decode_funcs[test_level];
    if (c == NULL) {
	c = create_FFSContext();
	rcv_context = c;
	set_targets(rcv_context);
    }
    if (!finished) {
/*	char *comment;*/
	FFSTypeHandle buffer_format = FFS_target_from_encode(rcv_context, buffer);

	if (buffer_format == NULL) {
	    printf("No format!\n");
	    exit(1);
	}
	if (((test_only == NULL) || (strcmp(test_only, "first_rec") == 0)) &&
	    (buffer_format == first_rec_ioformat)) {
	    first_rec read_data[10];
/*	    if (get_IOcontext_record_count(iocontext, buffer) == 1) {*/
		memset(&read_data[0], 0, sizeof(first_rec));
		if (!decode_func(rcv_context, buffer, buf_size, &read_data[0]))
		    printf("decode failed, first data\n");
		
		if (!first_rec_eq(&read_data[0], &rec1_array[first_rec_count[test_level]++])) {
		    printf("Rec1 failure\n");
		    fail++;
		}
/*	    } else {
		int count = next_IOrecord_count(iofile);
		if (count == 10) {
		    memset(&read_data[0], 0, sizeof(first_rec) * 10);
		    if (read_array_IOfile(iofile, &read_data[0], 10, sizeof(first_rec)) != 10)
			printf("decode failed, first array\n");
		    if (memcmp(&read_data[0], &rec1_array[first_rec_count[test_level]],
			       sizeof(first_rec) * 10) != 0) {
			printf("Rec1 failure\n");
			fail++;
		    }
		    first_rec_count[test_level] += 10;
		} else {
		    printf("Rec1 failure\n");
		    fail++;
		}
	    }		*/
	} else if (((test_only == NULL) || (strcmp(test_only, "second_rec") == 0)) &&
		   (buffer_format == second_rec_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, sizeof(second_rec));
	    second_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, second data failed\n");
	    if (!second_rec_eq(read_data, &rec2_array[second_rec_count[test_level]++])) {
		printf("Rec2 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "third_rec") == 0)) &&
		   (buffer_format == third_rec_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, sizeof(third_rec));
	    third_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, third data failed\n");
	    if (!third_rec_eq(read_data, &rec3_array[third_rec_count[test_level]++])) {
		printf("Rec3 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "fourth_rec") == 0)) &&
		   (buffer_format == fourth_rec_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, sizeof(fourth_rec));
	    fourth_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, fourth data failed\n");
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
	    int size = size_func(rcv_context, buffer, buf_size, sizeof(fifth_rec));
	    fifth_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, fifth data failed\n");
	    if (!fifth_rec_eq(read_data, &rec5)) {
		printf("Rec5 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	    fifth_rec_count[test_level]++;
	} else if (((test_only == NULL) || (strcmp(test_only, "sixth_rec") == 0)) &&
		   (buffer_format == sixth_rec_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, sizeof(sixth_rec));
	    sixth_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, variant format\n");
	    if (!sixth_rec_eq(read_data, &rec6_array[sixth_rec_count[test_level]++])) {
		printf("Rec6 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "nested_rec") == 0)) &&
		   (buffer_format == nested_rec_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, sizeof(nested_rec));
	    nested_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, variant format\n");
	    if (!nested_rec_eq(read_data, &rec7_array[nested_rec_count[test_level]++])) {
		printf("Rec7 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "later_rec") == 0)) &&
		   (buffer_format == later_rec_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, sizeof(later_rec));
	    later_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, variant format\n");
	    if (!later_rec_eq(read_data, &rec8_array[later_rec_count[test_level]++])) {
		printf("Rec8 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "ninth_rec") == 0)) &&
		   (buffer_format == ninth_rec_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, sizeof(ninth_rec));
	    ninth_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, variant format\n");
	    if (!ninth_rec_eq(read_data, &rec9_array[ninth_rec_count[test_level]++])) {
		printf("Rec9 failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "string_array") == 0)) &&
		   (buffer_format == string_array_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, 
				 sizeof(string_array_rec));
	    string_array_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, string array format\n");
	    if (!string_array_eq(read_data, 
				 &string_array_array[string_array_count[test_level]++])) {
		printf("string array failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "derive") == 0)) &&
		   (buffer_format == derive_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, 
				 sizeof(DeriveMsg));
	    DeriveMsgPtr read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, decode msg format\n");
	    if (!derive_eq(read_data, &derive)) {
		printf("derive msg failure, decode func %d\n", test_level);
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "multi_array") == 0)) &&
		   (buffer_format == multi_array_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, 
				 sizeof(multi_array));
	    multi_array_rec *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, decode msg format\n");
	    if (!multi_array_eq(read_data, &multi_array)) {
		printf("multi array failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "triangle_param") == 0)) &&
		   (buffer_format == triangle_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, 
				 sizeof(multi_array));
	    triangle_param *read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, decode msg format\n");
	    if (!triangle_param_eq(read_data, &triangle)) {
		printf("triangle param failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "add_action") == 0)) &&
		   (buffer_format == add_action_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, 
				 sizeof(add_rec));
	    add_rec_ptr read_data = get_mem(size);
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, decode msg format\n");
	    if (!add_rec_eq(read_data, &add_action_record)) {
		printf("add rec failure\n");
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	} else if (((test_only == NULL) || (strcmp(test_only, "node") == 0)) &&
		   (buffer_format == node_ioformat)) {
	    int size = size_func(rcv_context, buffer, buf_size, 
				 sizeof(add_rec));
	    node_ptr read_data = get_mem(size);
	    struct visit_table v;
	    int signature, calculated_sig;
	    memset(read_data, 0, size);
	    if (!decode_func(rcv_context, buffer, buf_size, read_data))
		printf("decode failed, decode msg format\n");
	    signature = read_data->node_num;
	    read_data->node_num = 0;
	    v.node_count = 0;
	    calculated_sig = calc_signature(read_data, &v);
	    if (signature != calculated_sig) {
		printf("Node sig not right, calculated %d, expected %d\n", 
		       calculated_sig, signature);
		fail++;
	    }
	    check_mem(size, (char*)read_data);
	    free(read_data);
	    node_rec_count[test_level]++;
	} else if (test_only == NULL) {
	    printf("discarding a record\n");
	    unknown_rec_count[test_level]++;
	}
    } else {
	/* finished */
	if (test_level == 0) {
	    free_FFSContext(rcv_context);
	    rcv_context = NULL;
	}
	if (test_only != NULL) return;
	if (node_rec_count[test_level] != 3) {
	    printf("Missed node, level %d\n", test_level);
	    fail++;
	}
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
    printf("Writing buffer of size %d\n", size);
    ssize = size;
    csize = ssize & 0xff;
    write(file_fd, &csize, 1);	/* low byte of 2-byte size */
    csize = ((ssize >> 8) & 0xff);
    write(file_fd, &csize, 1);	/* high byte of 2-byte size */
    write(file_fd, buf, size);
}
