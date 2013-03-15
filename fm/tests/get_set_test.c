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

int
main(argc, argv)
int argc;
char **argv;
{
    third_rec test_rec;
    void *data_ptr, *tmp_ptr;
    char *tmp_string;
    FMContext context;
    FMFormat third_format;

    if (argc > 1) {
	if (strcmp(argv[1], "-v") == 0) {
	    verbose++;
	}
    }
    
    test_rec.integer_field = -15;
    test_rec.long_field = -2;
    test_rec.string = "penny arcade";
    test_rec.string2 = "xkcd";
    test_rec.double_field = 3.14159;
    
    data_ptr = &test_rec;

    /* stop using test_rec and see if we can access fields using the data_ptr and the field list */
    tmp_ptr = get_FMfieldAddr_by_name(field_list3, "integer field", data_ptr);
    assert(*((int*)tmp_ptr) == -15);

    tmp_ptr = get_FMfieldAddr_by_name(field_list3, "long field", data_ptr);
    assert(*((long*)tmp_ptr) == -2);

    /* OK, now get pointers (here, strings) and test */
    tmp_string = get_FMPtrField_by_name(field_list3, "string field", data_ptr, 0);
    assert(strcmp(tmp_string, "penny arcade") == 0);

    tmp_string = get_FMPtrField_by_name(field_list3, "string field2", data_ptr, 0);
    assert(strcmp(tmp_string, "xkcd") == 0);
    
    /* OK, now SET pointers (here, strings) and test */
    assert(set_FMPtrField_by_name(field_list3, "string field", data_ptr, strdup("Ack! Thbbft!")) == 1);

    assert(set_FMPtrField_by_name(field_list3, "string field2", data_ptr, strdup("svelte buoyant waterfowl")) == 1);
	   
    assert(strcmp(test_rec.string, "Ack! Thbbft!") == 0);
    assert(strcmp(test_rec.string2, "svelte buoyant waterfowl") == 0);

    /* Finally, using a format, free the strings that are in this structure */
    context = create_local_FMcontext();
    third_format = FMregister_simple_format(context, "third", field_list3, sizeof(test_rec));

    FMfree_var_rec_elements(third_format, data_ptr);

    return 0;
}


