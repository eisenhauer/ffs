#include "config.h"
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include "ffs.h"
#include "cod.h"

char *usage = "\
Usage: FFSsort [<option>] <in filename> <out filename>\n\
       FFSsort sortsFFS files. \n\
       Options are:\n\
		-e <expression>\n\
\n";

extern char *
dump_raw_FMrecord_to_string(FMContext fmc, FMFormat format, void *data);

typedef struct {
    void *data;
    long size;
    double sort_entry;
} data_entry;

void
write_data(data_entry *data_values, FFSContext c, FFSFile out_file)
{
    int i = 0;
    while (data_values[i].data != NULL) {
	write_encoded_FFSfile(out_file, data_values[i].data, 
			      data_values[i].size, c, NULL);
	i++;
    }
}

typedef double (*extraction_func)(void*data);
typedef struct {
    FFSTypeHandle format;
    extraction_func func;
} func_table_entry;

static
void parse_expression(char *expr, char **format_name, char **simple_expression, char **prog)
{
    char *colon = strchr(expr, ':');
    if (colon) {
	*colon = 0;
	*format_name = strdup(expr);
	*colon = ':';
	expr = colon+1;
    }
    *simple_expression = expr;
}

extraction_func
generate_single_handler(FFSTypeHandle format, char *expression)
{
    char *format_name = NULL;
    char *simple_expression = NULL;
    char *prog = NULL;
    FMFormat fmf = FMFormat_of_original(format);
    parse_expression(expression, &format_name, &simple_expression, &prog);
    if (format_name != NULL) {
	if (strcmp(name_of_FMformat(fmf), format_name) != 0)
	    /* expression was not for this format type */
	    return NULL;
    }
    if (simple_expression && !prog) {
	char *template = "{\nreturn input.%s;\n}";
	prog = malloc(strlen(template) + strlen(simple_expression));
	sprintf(prog, template, simple_expression);
    }
    if (!prog) return NULL;
    cod_parse_context context = new_cod_parse_context();
    FMContext c = fmf->context;
    cod_code gen_code;
    int id_len;
    cod_subroutine_declaration("double proc()", context);
    cod_add_encoded_param("input", get_server_ID_FMformat(fmf, &id_len), 0, c, context);
    gen_code = cod_code_gen(prog, context);
    if (!gen_code) {
	/* GSE fix leaks stuff! */
	return NULL;
    } else {
	return (extraction_func)gen_code->func;
    }
}

extraction_func
generate_sort_handler(FFSTypeHandle format, char **expressions)
{
    int i = 0;
    extraction_func f;
    while(expressions[i] != NULL) {
	f = generate_single_handler(format, expressions[i]);
	if (f) return f;
	i++;
    }
    printf("No valid expression for format %p\n", format);
    return NULL;
}

extraction_func
get_sort_handler(FFSTypeHandle format, char **expressions)
{
    static func_table_entry *func_table = NULL;
    static func_table_count = 0;
    int i;
    if (func_table == NULL) {
	func_table = malloc(sizeof(func_table[0]));
    }
    for (i=0; i < func_table_count; i++) {
	if (format == func_table[i].format) return func_table[i].func;
    }
    func_table = realloc(func_table, sizeof(func_table[0])*(func_table_count+2));
    func_table[func_table_count].format = format;
    func_table[func_table_count].func = generate_sort_handler(format, 
							      expressions);
    return func_table[func_table_count++].func;
}

/* qsort struct comparision function (double field) */ 
int compar(const void *a, const void *b) 
{ 
    data_entry *ia = (data_entry *)a;
    data_entry *ib = (data_entry *)b;
    if (ia->sort_entry < ib->sort_entry) return -1;
    if (ia->sort_entry > ib->sort_entry) return 1;
    return 0;
} 
 
int
main(argc, argv)
int argc;
char **argv;
{
    FFSFile in_file = NULL, out_file = NULL;
    int buffer_size = 1024;
    char *buffer = NULL;
    int i;
    int bitmap, format_count;
    FFSTypeHandle *in_formats;
    FMFormat *out_formats;
    char *in_filename = NULL, *out_filename = NULL;
    char **expressions = malloc(sizeof(expressions[0]));
    int expr_count = 0;
    int count = 0;
    data_entry *data_values = malloc(sizeof(data_entry));

    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    if (strcmp(argv[i], "-e") == 0) {
		if (argc <= i+1) {
		    fprintf(stderr, "Argument must follow -e\n");
		    fprintf(stderr, "%s", usage);
		    exit(0);
		}
		expressions = realloc(expressions, sizeof(expressions[0])*(expr_count+2));
		expressions[expr_count++] = argv[i+1];
		i++;
	    } else {
		fprintf(stderr, "Unknown argument \"%s\"\n", argv[i]);
	    }
	} else {
	    if (in_filename == NULL) {
		in_filename = argv[i];
	    } else if (out_filename == NULL) {
		out_filename = argv[i];
	    } else {
		fprintf(stderr, "Extra argument specified \"%s\"\n%s\n",
			argv[i], usage);
		exit(1);
	    }
	}
    }
    if (expr_count == 0) {
	fprintf(stderr, "At least one sort expression must be specified\n");
	fprintf(stderr, "%s", usage);
	exit(1);
    }
    if (!in_filename || !out_filename) {
	fprintf(stderr, "%s", usage);
	exit(1);
    }
    in_file = open_FFSfile(in_filename, "R");
    out_file = open_FFSfile(out_filename, "w");

    if (in_file == NULL) {
	printf("File Open Failure \"%s\"", in_filename);
	perror("Opening input file");
	exit(1);
    }

    if (out_file == NULL) {
	printf("File Open Failure \"%s\"", in_filename);
	perror("Opening output file");
	exit(1);
    }

    bitmap = FFSdata;
    FFSset_visible(in_file, bitmap);

    format_count = 0;
    in_formats = malloc(sizeof(FMFormat));
    out_formats = malloc(sizeof(FMFormat));
    while (1) {
        switch (FFSnext_record_type(in_file)) {
        case FFSdata:{
	    FFSTypeHandle format;
	    double (*handler)(void*data);
	    double sort_value;
	    int size = FFSnext_data_length(in_file);
	    buffer = malloc(size);
	    FFSread_raw_header(in_file, buffer, buffer_size, &format);
	    handler = get_sort_handler(format, expressions);
	    if (handler) {
		sort_value = handler(buffer);
		printf("Sort value is %g\n", sort_value);
	    } else {
		sort_value = 0.0;
		printf("No Sort value\n");
	    }
	    data_values[count].data = buffer;
	    data_values[count].size = size;
	    data_values[count].sort_entry = sort_value;
	    data_values = realloc(data_values, sizeof(data_values[0])*(count+2));
	    count++;
	    data_values[count].data = NULL;
	    break;
        }
        case FFSerror:
        case FFSend:
	    qsort(data_values, count, sizeof(data_values[0]), compar);
	    write_data(data_values, FFSContext_of_file(in_file), out_file);
            close_FFSfile(in_file);
            close_FFSfile(out_file);
            exit(0);
	default:
	    break;
        }
    }
}
