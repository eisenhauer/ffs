// This is a copied program from ffs_write.c

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ffs.h"

typedef struct _second_rec {
    int len, len2;
    int *type_array;
    double *value_array;
    char char_field;
} second_rec, *second_rec_ptr;

FMField field_list2[] = {
    {"len", "integer", 
       sizeof(int), FMOffset(second_rec_ptr, len)},
    {"len2", "integer", 
       sizeof(int), FMOffset(second_rec_ptr, len2)},
    {"type_array", "int[len]",
       sizeof(double), FMOffset(second_rec_ptr, type_array)},
    {"value_array", "float[len]",
       sizeof(double), FMOffset(second_rec_ptr, value_array)},
    {"char field", "char",
       sizeof(char), FMOffset(second_rec_ptr, char_field)},
    { NULL, NULL, 0, 0}
};

extern void
install_drop_code(FMFormat f, char *field, char*code_str);

int
main(int argc, char **argv)
{
    FMContext src_context;
    FFSFile ffsfile;
    FMFormat second_rec_ioformat;

    second_rec rec2;

    FMStructDescRec str_list[5];
    int verbose = 0, indexed = 0;
    char *output_file = "marshal_output";
    int i;

    ffsfile = open_FFSfile(output_file, "w");

    src_context = create_local_FMcontext();

    str_list[0].format_name = "string format";
    str_list[0].field_list = field_list2;
    str_list[0].struct_size = sizeof(second_rec);
    str_list[0].opt_info = NULL;
    str_list[1].format_name = NULL;
    second_rec_ioformat = register_data_format(src_context, str_list);

    memset((char *) &rec2, 0, sizeof(rec2));
    rec2.len = 10;
    rec2.len2 = 10;
    rec2.type_array = malloc(sizeof(rec2.type_array[0]) * rec2.len);
    rec2.value_array = malloc(sizeof(rec2.value_array[0]) * rec2.len2);
    for (i=0; i++; i < rec2.len) {
	rec2.type_array[i] = 1;
	if ((i % 2) == 0) rec2.type_array[i] = 2;
	if ((i % 3) == 0) rec2.type_array[i] = 3;
    }
    rec2.char_field = 'A';
    install_subsample_code(second_rec_ioformat, "type_array", "{if (input.array[0] == 0.5) return 1; return 0;}");
    install_subsample_code(second_rec_ioformat, "value_array", "{if (input.array[0] == 0.5) return 1; return 0;}");
    if (!write_FFSfile(ffsfile, second_rec_ioformat, &rec2))
	printf("write failed\n");

    for (i=0; i++; i < rec2.len) {
	rec2.type_array[i] = 1;
	if ((i % 3) == 0) rec2.type_array[i] = 3;
	if ((i % 2) == 0) rec2.type_array[i] = 2;
    }

    if (!write_FFSfile(ffsfile, second_rec_ioformat, &rec2))
	printf("write failed\n");

    close_FFSfile(ffsfile);
    free_FFSfile(ffsfile);

    ffsfile = open_FFSfile(output_file, "r");
    FFSset_fixed_target(FFSContext_of_file(ffsfile), str_list);
    memset(&rec2, 0, sizeof(rec2));
    FFSread(ffsfile, &rec2);

/* change success conditions */
    if (rec2.type_array != NULL) {
        printf("Expected first rec array dropped\n");
	return 1;
    }
    FFSread(ffsfile, &rec2);
    if (rec2.type_array == NULL) {
        printf("First rec array dropped when it shouldn't have been\n");
	return 1;
    }
    if(rec2.value_array[0] != 2.5) {
        printf("First rec array element 0 wrong.\n");
	return 1;
    }
    unlink(output_file);
    return 0;
}
