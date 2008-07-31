#include "../config.h"
#include <assert.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#include <stdio.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <string.h>
#include "fm.h"

#include "test_funcs.h"

static int verbose = 0;

void
do_test(FMStructDescList list)
{
    int i = 0;
    
    while (list[i].format_name != NULL) i++;
    FMStructDescList tmp = malloc(sizeof(tmp[0]) * (i+1));

    i = 0;
    while (list[i].format_name != NULL) {
	tmp[i].format_name = list[i].format_name;
	tmp[i].field_list = copy_field_list(list[i].field_list);
	tmp[i].struct_size = -1;
	tmp[i].opt_info = NULL;
	i++;
    }
    tmp[i].format_name = NULL;
    tmp[i].field_list = NULL;
    tmp[i].struct_size = 0;
    tmp[i].opt_info = NULL;
    

    FMlocalize_structs(tmp);
    i = 0;
    while (tmp[i].format_name != NULL) {
	int j = 0;
	FMFieldList fields = tmp[i].field_list;
	while(fields[j].field_name != NULL) {
	    if (fields[j].field_offset != tmp[i].field_list[j].field_offset) {
		printf("Failed alignment of field %s in format %s, got %d, expected %d\n",
		       fields[j].field_name, tmp[i].format_name, tmp[i].field_list[j].field_offset,
		       fields[j].field_offset);
	    }
	    j++;
	}
	i++;
    }
    i = 0;
    while (tmp[i].format_name != NULL) {
	free_field_list(tmp[i].field_list);
	i++;
    }
    free(tmp);
}


int
main(argc, argv)
int argc;
char **argv;
{
    do_test(first_format_list);
    do_test(string_format_list);
    do_test(structured_format_list);
    do_test(two_string_format_list);
    do_test(fourth_format_list);
    do_test(later_format_list);
    do_test(nested_format_list);
    do_test(embedded_format_list);
    do_test(variant_format_list);
    do_test(string_array_format_list);
    do_test(derive_format_list);
    do_test(multi_array_format_list);
    do_test(triangle_format_list);
    do_test(add_action_format_list);
    do_test(node_format_list);
    return 0;
}
