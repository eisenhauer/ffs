#include "config.h"

#ifndef MODULE
#include "assert.h"
#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <sys/types.h>
#include <ctype.h>
extern char *getenv(const char *name);

#ifdef HAVE_DRISC_H
#include "drisc.h"
#define static_ctx c 
#define VCALL7V(subr, argstr, arg1, arg2, arg3, arg4, arg5, arg6, arg7) dr_scallv(c, (void*)subr, argstr, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define VCALL6V(subr, argstr, arg1, arg2, arg3, arg4, arg5, arg6) dr_scallv(c, (void*)subr, argstr, arg1, arg2, arg3, arg4, arg5, arg6)
#define VCALL5V(subr, argstr, arg1, arg2, arg3, arg4, arg5) dr_scallv(c, (void*)subr, argstr, arg1, arg2, arg3, arg4, arg5)
#define VCALL4V(subr, argstr, arg1, arg2, arg3, arg4) dr_scallv(c, (void*)subr, argstr, arg1, arg2, arg3, arg4)
#define VCALL4P(subr, argstr, arg1, arg2, arg3, arg4) dr_scallp(c, (void*)subr, argstr, arg1, arg2, arg3, arg4)
#define VCALL3V(subr, argstr, arg1, arg2, arg3) dr_scallv(c, (void*)subr, argstr, arg1, arg2, arg3)
#define VCALL2V(subr, argstr, arg1, arg2) dr_scallv(c, (void*)subr, argstr, arg1, arg2)
#define TYPE_ALIGN(c, t) dr_type_align(c, t)
#define _vrr(x) x
#endif
#else
/* kernel build */
#include "kernel/pbio_kernel.h"
#include "kernel/kpbio.h"
#include "kernel/library.h"
#endif
#include "ffs.h"
#include "ffs_internal.h"
#if defined(HAVE_DRISC_H)
#include "ffs_gen.h"
#endif
#include "assert.h"
#include "cercs_env.h"

static MAX_INTEGER_TYPE get_big_int(IOFieldPtr iofield, void *data);
static MAX_FLOAT_TYPE get_big_float(IOFieldPtr iofield, void *data);
static MAX_UNSIGNED_TYPE get_big_unsigned(IOFieldPtr iofield, void *data);
static void byte_swap(char *data, int size);

static int get_double_warn = 0;
static int get_long_warn = 0;

FMfloat_format ffs_my_float_format = Format_Unknown;
/* 
 * ffs_reverse_float_formats identifies for each format what, 
 * if any, format is its byte-swapped reverse.
*/
FMfloat_format ffs_reverse_float_formats[] = {
    Format_Unknown, /* no format complements unknown */
    Format_IEEE_754_littleendian, /* littleendian complements bigendian */
    Format_IEEE_754_bigendian, /* bigendian complements littleendian */
    Format_Unknown /* no exact opposite for mixed-endian (ARM) */
};

static unsigned char IEEE_754_4_bigendian[] = 
  {0x3c, 0x00, 0x00, 0x00};
static unsigned char IEEE_754_4_littleendian[] = 
  {0x00, 0x00, 0x00, 0x3c};
static unsigned char IEEE_754_8_bigendian[] = 
  {0x3f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char IEEE_754_8_littleendian[] = 
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f};
static unsigned char IEEE_754_8_mixedendian[] = 
  {0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00};
static unsigned char IEEE_754_16_bigendian[] = 
  {0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char IEEE_754_16_littleendian[] = 
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x3f};
static unsigned char IEEE_754_16_mixedendian[] = 
  {0x00, 0x00, 0xf8, 0x3f, 0x00, 0x00, 0x00, 0x00, 
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static char *float_format_str[] = {
    "Unknown float format",
    "IEEE 754 float (bigendian)", 
    "IEEE 754 float (littleendian)",
    "IEEE 754 float (mixedendian)"};

static int IO_shut_up = 0;

static int
min_align_size(size)
int size;
{
    int align_size = 8;		/* conservative on current machines */
    switch (size) {
    case 7:
    case 6:
    case 5:
    case 4:
	align_size = 4;
	break;
    case 3:
	align_size = 2;
	break;
    case 2:
    case 1:
	align_size = size;
	break;
    }
    return align_size;
}

static FMfloat_format
infer_float_format(char *float_magic, int object_len)
{
    switch (object_len) {
    case 4:
	if (memcmp(float_magic, &IEEE_754_4_bigendian[0], 4) == 0) {
	    return Format_IEEE_754_bigendian;
	} else if (memcmp(float_magic, &IEEE_754_4_littleendian[0], 4) == 0) {
	    return Format_IEEE_754_littleendian;
	}
	break;
    case 8:
	if (memcmp(float_magic, &IEEE_754_8_bigendian[0], 8) == 0) {
	    return Format_IEEE_754_bigendian;
	} else if (memcmp(float_magic, &IEEE_754_8_littleendian[0], 8) == 0) {
	    return Format_IEEE_754_littleendian;
	} else if (memcmp(float_magic, &IEEE_754_8_mixedendian[0], 8) == 0) {
	    return Format_IEEE_754_mixedendian;
	}
	break;
    case 16:
	if (memcmp(float_magic, &IEEE_754_16_bigendian[0], 16) == 0) {
	    return Format_IEEE_754_bigendian;
	} else if (memcmp(float_magic, &IEEE_754_16_littleendian[0], 16) ==0){
	    return Format_IEEE_754_littleendian;
	} else if (memcmp(float_magic, &IEEE_754_16_mixedendian[0], 16) == 0){
	    return Format_IEEE_754_mixedendian;
	}
	break;
    }
    return Format_Unknown;
}

extern void
init_float_formats()
{
    static int done = 0;
    if (!done) {
	double d = MAGIC_FLOAT;
	ffs_my_float_format = infer_float_format((char*)&d, sizeof(d));
	switch (ffs_my_float_format) {
	case Format_IEEE_754_bigendian:
	case Format_IEEE_754_littleendian:
	case Format_IEEE_754_mixedendian:
	    break;
	case Format_Unknown:
	    fprintf(stderr, "Warning, unknown local floating point format\n");
	    break;
	}
	done++;
    }
}
	

void
FFSfree_conversion(conv)
IOConversionPtr conv;
{
    int i;
    for (i = 0; i < conv->conv_count; i++) {
	if (conv->conversions[i].subconversion) {
	    FFSfree_conversion(conv->conversions[i].subconversion);
	}
	if (conv->conversions[i].default_value) {
	    free(conv->conversions[i].default_value);
	}
    }
    if (conv->native_field_list) {
	for (i = 0; conv->native_field_list[i].field_name != NULL; i++) {
	    free((char*)conv->native_field_list[i].field_name);
	    free((char*)conv->native_field_list[i].field_type);
	}
	free(conv->native_field_list);
    }
    if (conv->free_func != NULL) {
	conv->free_func(conv->free_data);
    }
    free(conv);
}

/*
 * Copy fieldname from io_field to out_field_name and 
 * removes the default value from it.
 * default value is then stored in default_val after converting it to 
 * appropriate format.
 * */

static void
field_name_strip_get_default(io_field, out_field_name, default_val)
const FMField *io_field;
char *out_field_name;
void **default_val;
{
    char *s, *s1;
    char *base_type = base_data_type(io_field->field_type);
    FMdata_type data_type = str_to_data_type(base_type);
    strncpy(out_field_name, io_field->field_name, 64);
    s = strchr(out_field_name, '(');
    *default_val = NULL;
    free(base_type);
    if(s) {
	*s = '\0';
	s++;
	s1 = strchr(s, ')');
	if(s1) {
	    *s1 = '\0';
	}
	if((int)(s1-s)> 0){
	    str_to_val(s, data_type, io_field->field_size, default_val);
	}
    }
}
 
static void
create_default_conversion(FMField iofield, void *default_value, 
			  IOConversionPtr *conv_ptr_ptr, int conv_index)
{
    IOConversionPtr	conv_ptr =
	(IOConversionPtr) realloc(*conv_ptr_ptr, sizeof(IOConversionStruct) +
				  conv_index * sizeof(IOconvFieldStruct));
    *conv_ptr_ptr = conv_ptr;
    memset(&conv_ptr->conversions[conv_index].src_field, 0, 
	   sizeof(conv_ptr->conversions[conv_index].src_field));
    conv_ptr->conversions[conv_index].subconversion = NULL;
    conv_ptr->conversions[conv_index].iovar = NULL;
    conv_ptr->conversions[conv_index].dest_size = iofield.field_size;
    conv_ptr->conversions[conv_index].dest_offset = iofield.field_offset;
    conv_ptr->conversions[conv_index].default_value = default_value;
    conv_ptr->conversions[conv_index].rc_swap = no_row_column_swap;
}

static
IOConversionPtr
create_conversion(src_ioformat, target_field_list, target_struct_size,
		  pointer_size, byte_reversal, target_fp_format,
		  initial_conversion, target_column_major,
		  string_offset_size, converted_strings)
FFSTypeHandle src_ioformat;
FMFieldList target_field_list;
int target_struct_size;
int pointer_size;
int byte_reversal;
FMfloat_format target_fp_format;
IOconversion_type initial_conversion;
int target_column_major;
int string_offset_size;
int converted_strings;
{
    int target_field_count = count_FMfield(target_field_list);
    FMFieldList nfl_sort = copy_field_list(target_field_list);
    FMFieldList input_field_list = src_ioformat->body->field_list;
    FMVarInfoList input_var_list = src_ioformat->body->var_list;
    IOconversion_type conv = initial_conversion;
    int input_index, conv_index = 0, i = 0;
    FMfloat_format src_float_format = src_ioformat->body->float_format;
    IOConversionPtr conv_ptr =
	(IOConversionPtr) malloc(sizeof(IOConversionStruct));
    int column_row_swap_necessary = (target_column_major != src_ioformat->body->column_major_arrays);
    
    if (target_fp_format == -1) target_fp_format = ffs_my_float_format;

    conv_ptr->notify_of_format_change = 0;
    conv_ptr->context = src_ioformat->context;
    conv_ptr->ioformat = src_ioformat;
    conv_ptr->base_size_delta = target_struct_size -
	src_ioformat->body->record_length;
    conv_ptr->max_var_expansion = 1.0;
    conv_ptr->conv_count = 0;
    qsort(nfl_sort, target_field_count, sizeof(nfl_sort[0]),
	  field_offset_compar);
    conv_ptr->native_field_list = nfl_sort;
    conv_ptr->target_pointer_size = pointer_size;
    conv_ptr->required_alignment = 8; /* placeholder */
    conv_ptr->free_data = NULL;
    conv_ptr->free_func = NULL;
    conv_ptr->conv_func = NULL;
    conv_ptr->conv_func4 = NULL;
    conv_ptr->conv_func2 = NULL;
    conv_ptr->conv_func1 = NULL;
    conv_ptr->string_offset_size = string_offset_size;
    conv_ptr->converted_strings = converted_strings;

    /* 
     * We assume that the fields listed in the target_field_list are
     * those that the user is interested in.  Skipping input fields
     * is OK... 
     */

    if (src_ioformat->body->record_length > target_struct_size) {
	/* if input record is larger than target record, must do buffered */
	switch (conv) {
	case none_required:
	case direct_to_mem:
	    conv = buffer_and_convert;
	    break;
	case buffer_and_convert:
	case copy_dynamic_portion:
	    break;
	default:
	    assert(FALSE);
	}
    }
    /* try for no conversion/direct_to_memory first.. */
  restart:
    if (conv_index > 0) {
	/* really a restart, NULL a couple of things */
	int i = 0;
	for (i=0; i < conv_index; i++) {
	    conv_ptr->conversions[i].iovar = NULL;
	    if (conv_ptr->conversions[i].default_value) {
		free(conv_ptr->conversions[i].default_value);
	    }
	}
    }
    conv_ptr->conversion_type = conv;
    input_index = conv_index = 0;
    for (i = 0; i < target_field_count; i++) {
	FMField input_field;
	FMdata_type in_data_type, target_data_type;
	long in_elements, target_elements;
	void *default_val = NULL;
	char tmp_field_name[64];
	int multi_dimen_array = 0;

	/* 
	 * all fields in the target list must appear at the same offset
	 * as in the input list.
	 */
	input_index = 0;
	
	field_name_strip_get_default(&nfl_sort[i], tmp_field_name, &default_val);
	while (strcmp(tmp_field_name,
		      input_field_list[input_index].field_name) != 0) {
	    input_index++;
	    if (input_index >= src_ioformat->body->field_count) {
		if(default_val){
		    if ((conv == buffer_and_convert) || 
			(conv == copy_dynamic_portion)) {
			input_index = -1; /* Basically invalidating input_index
					   Indication for using default_val */
			break;
		    } else {
			if (default_val) {
			    free(default_val);
			    default_val = NULL;
			}
			conv = buffer_and_convert;
			goto restart;
		    }
		}
		fprintf(stderr,
			"Requested field %s missing from input format\n",
			nfl_sort[i].field_name);
		FFSfree_conversion(conv_ptr);
		return NULL;
	    }
	}
	if(input_index == -1){
	    create_default_conversion(nfl_sort[i], default_val, &conv_ptr, 
				      conv_index);
	    conv_ptr->conversion_type = conv;
	    conv_ptr->conv_count = ++conv_index;
	    continue;
	} else {
	    if (default_val) {
		free(default_val);
		default_val = NULL;
	    }
	}
	input_field = input_field_list[input_index];
	in_data_type = array_str_to_data_type(input_field.field_type,
					      &in_elements);
	if (in_elements != 1) {
	    char *first_bracket = memchr(input_field.field_type, '[', strlen(input_field.field_type));
	    if (first_bracket != NULL) {
		char *sec_bracket = memchr(first_bracket+1, '[', strlen(first_bracket));
		if (sec_bracket) multi_dimen_array++;
	    }
	}
	    
	if (in_elements == -1) {
	    in_elements = 1;	/* var array */
	}
	target_data_type = array_str_to_data_type(nfl_sort[i].field_type,
						  &target_elements);
	if (target_elements == -1) {
	    target_elements = 1;	/* var array */
	}
	switch (conv) {
	case none_required:
	case direct_to_mem:
	    if (nfl_sort[i].field_offset !=
		input_field_list[input_index].field_offset) {

		/* 
		 * we were planning direct to memory transfer, but found a 
		 * field that we couldn't do that way.   start over
		 * planning buffer and convert. 
		 */
		conv = buffer_and_convert;
		goto restart;
	    }
	    if ((in_elements == target_elements) &&
		(in_data_type == target_data_type) &&
		(in_data_type != unknown_type) &&
		(!byte_reversal) &&
		((in_data_type != float_type) ||
		 (target_fp_format == src_float_format)) &&
		(!input_var_list[input_index].string &&
		 !input_var_list[input_index].var_array) &&
		(!column_row_swap_necessary || !multi_dimen_array) &&
		(nfl_sort[i].field_size == input_field.field_size)) {
		/* nothing to do for this field */
		continue;
	    }
	    if ((in_elements != 1) &&
		(nfl_sort[i].field_size != input_field.field_size)) {
		/* Can't do direct to memory with array field interleaving 
		 * 
		 */
		conv = buffer_and_convert;
		goto restart;
	    }
	    if (column_row_swap_necessary && multi_dimen_array) {
		/* Can't transpose an array in place
		 * 
		 */
		conv = copy_dynamic_portion;
		goto restart;
	    }

	    if ((in_data_type == unknown_type) &&
		!input_var_list[input_index].var_array) {
		FFSTypeHandle format = src_ioformat->field_subformats[input_index];
		if ((format != NULL) && (format->conversion != NULL)) {
		    switch (format->conversion->conversion_type) {
		    case copy_dynamic_portion:
			assert(conv == copy_dynamic_portion);
			break;
		    case buffer_and_convert:
			conv = buffer_and_convert;
			goto restart;
		    case direct_to_mem:
			conv = direct_to_mem;
			break;
		    case none_required:
			continue;
		    }
		}
	    }
	    /* falling through */
	case buffer_and_convert:
	    if (input_var_list[input_index].var_array) {
		if (nfl_sort[i].field_size != input_field.field_size) {
		    /* argh.  Must buffer variant part too */
		    conv = copy_dynamic_portion;
		    goto restart;
		}
	    }
	    break;
	case copy_dynamic_portion:
	    if (input_var_list[input_index].var_array) {
		/* 
		 * expansion value includes padding for possibly having
		 * to re-align the value to the proper boundary.
		 * (This is generally a gross overestimate, but it's safe.)
		 */
		int local_size = nfl_sort[i].field_size +
		min_align_size(nfl_sort[i].field_size);
		double expansion = ((double) local_size) / input_field.field_size;
		if (expansion > conv_ptr->max_var_expansion) {
		    conv_ptr->max_var_expansion = expansion;
		}
	    }
	    break;
	default:
	    assert(FALSE);
	}
	if ((in_elements != target_elements) || 
	    (in_data_type != target_data_type)) {

	    fprintf(stderr,
		    "Requested field %s base type %s \n   differs from source type %s\n\n",
		    nfl_sort[i].field_name, nfl_sort[i].field_type,
		    input_field.field_type);
	    FFSfree_conversion(conv_ptr);
	    return NULL;
	}
	if (conv == none_required) {
	    conv = direct_to_mem;
	}
	conv_ptr =
	    (IOConversionPtr) realloc(conv_ptr, sizeof(IOConversionStruct) +
				 conv_index * sizeof(IOconvFieldStruct));
	conv_ptr->conversion_type = conv;
	memset(&conv_ptr->conversions[conv_index].src_field, 0, 
	       sizeof(conv_ptr->conversions[conv_index].src_field));
	conv_ptr->conversions[conv_index].src_field.byte_swap = byte_reversal;
	conv_ptr->conversions[conv_index].src_field.src_float_format =
	    src_float_format;
	conv_ptr->conversions[conv_index].src_field.target_float_format =
	    target_fp_format;
	conv_ptr->conversions[conv_index].subconversion = NULL;
	conv_ptr->conversions[conv_index].iovar = NULL;
	conv_ptr->conversions[conv_index].rc_swap = no_row_column_swap;
	if (column_row_swap_necessary & multi_dimen_array) {
	    if (src_ioformat->body->column_major_arrays) {
		conv_ptr->conversions[conv_index].rc_swap = swap_source_column_major;
	    } else {
		conv_ptr->conversions[conv_index].rc_swap = swap_source_row_major;
	    }
	}
	in_data_type = array_str_to_data_type(input_field.field_type, 
					      &in_elements);
	if (in_elements != 1) {
	    conv_ptr->conversions[conv_index].iovar =
		&input_var_list[input_index];
	}
	if (in_data_type == unknown_type) {
	    FFSTypeHandle format = src_ioformat->field_subformats[input_index];
	    if ((format != NULL) && (format->conversion != NULL)) {
		IOConversionPtr subconv;
		int struct_size = format->conversion->base_size_delta +
		format->body->record_length;
		subconv =
		    create_conversion(format,
				      format->conversion->native_field_list,
				      struct_size, pointer_size,
				      byte_reversal, target_fp_format,
				      conv, target_column_major,
				      string_offset_size,
				      converted_strings);
		conv_ptr->conversions[conv_index].subconversion = subconv;
	    } else {
		fprintf(stderr, "Unknown field type for field %s ->\"%s\", format %lx\n",
			input_field.field_name,
			src_ioformat->body->field_list[input_index].field_type,
			(long)src_ioformat);
		FFSfree_conversion(conv_ptr);
		return NULL;
	    }
	}
	if (input_var_list[input_index].var_array) {
	    in_elements = 1;
	}
	conv_ptr->conversions[conv_index].src_field.data_type = in_data_type;
	conv_ptr->conversions[conv_index].src_field.offset =
	    input_field.field_offset;
	conv_ptr->conversions[conv_index].src_field.size =
	    input_field.field_size;
	conv_ptr->conversions[conv_index].dest_size =
	    nfl_sort[i].field_size;
	conv_ptr->conversions[conv_index].dest_offset =
	    nfl_sort[i].field_offset;
	conv_ptr->conversions[conv_index].default_value = NULL;
	conv_ptr->conv_count = ++conv_index;
	if (default_val) {
	    free(default_val);
	    default_val = NULL;
	}
    }
    conv_ptr->conv_func = generate_conversion(conv_ptr, 8);
    switch(conv_ptr->required_alignment) {
    case 1:
	conv_ptr->conv_func1 = conv_ptr->conv_func;
	/* falling */
    case 2:
	conv_ptr->conv_func2 = conv_ptr->conv_func;
	/* falling */
    case 4:
	conv_ptr->conv_func4 = conv_ptr->conv_func;
	/* falling */
    case 8:
    case 16:
    case 32:
    case 64:
	/* really can't imagine these currently, but... */
    case 0:
	/* zero is no conversion, so no requirement */
	break;
    default:
	fprintf(stderr, "Funky alignment, %d, for conversion %s\n",
		conv_ptr->required_alignment, 
		conv_ptr->ioformat->body->format_name);
    }
    return conv_ptr;
}

extern
void
set_general_IOconversion_for_format(iofile, file_ioformat, native_field_list,
				    native_struct_size, pointer_size)
FFSContext iofile;
FFSTypeHandle file_ioformat;
FMFieldList native_field_list;
int native_struct_size;
int pointer_size;
{
    IOConversionPtr conv_ptr;
    IOconversion_type conv_type = none_required;
    int string_offset_size;
    int data_align_pad = (8 - file_ioformat->body->record_length) & 0x7;

    if (file_ioformat->body->byte_reversal)
	conv_type = direct_to_mem;

    string_offset_size = file_ioformat->body->record_length + data_align_pad;

    conv_ptr = create_conversion(file_ioformat, native_field_list,
				 native_struct_size, pointer_size,
				 file_ioformat->body->byte_reversal, 
				 ffs_my_float_format, conv_type,
/*				 iofile->native_column_major_arrays*/ 0,
				 string_offset_size, FALSE);

    if (conv_ptr == NULL) {
	fprintf(stderr, "Set_IOconversion failed for format name %s\n",
		file_ioformat->body->format_name);
	return;
    }
    conv_ptr->context = iofile;
    if (file_ioformat->conversion != NULL) {
	FFSfree_conversion(file_ioformat->conversion);
    }
    file_ioformat->conversion = conv_ptr;
}

static int
set_conversion_from_list(iocontext, ioformat, struct_list)
FFSContext iocontext;
FFSTypeHandle ioformat;
FMStructDescList struct_list;
{
    int i = 0;
    while(struct_list[i].format_name != NULL) {
	if (strcmp(struct_list[i].format_name, ioformat->body->format_name) == 0) {
	    break;
	}
	i++;
    }
    if (struct_list[i].format_name == NULL) {
	printf("Local structure description for type \"%s\" not found in IOStructDescList\n",
	       ioformat->body->format_name);
	return 0;
    }

    set_general_IOconversion_for_format(iocontext, ioformat,
					struct_list[i].field_list, 
					struct_list[i].struct_size,
					(int) sizeof(char *));
    return 1;
}

extern
void
establish_conversion(iocontext, ioformat, struct_list)
FFSContext iocontext;
FFSTypeHandle ioformat;
FMStructDescList struct_list;
{
    int i;
    int subformat_count = 0;
    while (ioformat->body->subformats && ioformat->body->subformats[subformat_count])
	subformat_count++;
    for (i=subformat_count-1; i>= 0; i--) {
	FFSTypeHandle f = ioformat->subformats[i];
	if (!(set_conversion_from_list(iocontext, f, struct_list))) {
	    return;
	}
    }
    if (!(set_conversion_from_list(iocontext, ioformat, struct_list))) {
	return;
    }
}


static void
convert_addr_field(iofile, src_spec, src, dest_size, dest, string_offset_size,
		   string_base_p, size_delta, converted_strings, src_offset_p,
		   dest_p, required_alignment)
FFSContext iofile;
IOFieldPtr src_spec;
void *src;
int dest_size;
void *dest;
int string_offset_size;
char **string_base_p;
int size_delta;
int converted_strings;
int *src_offset_p;
void **dest_p;
int required_alignment;
{
    int align_tmp;
    if ((dest_size == sizeof(char *)) && (*string_base_p != NULL)) {
	struct _IOgetFieldStruct tmp_src_field;  /* OK */
	MAX_INTEGER_TYPE tmp_int;
	char **dest_field = (char **) dest;
	tmp_src_field = *src_spec;

	tmp_src_field.data_type = integer_type;

	tmp_int = get_big_int(&tmp_src_field, src);

	*src_offset_p = tmp_int;
	if (tmp_int != 0) {
	    /* handle possibly different string base */
	    tmp_int -= (long) string_offset_size;
	    *dest_field = *string_base_p + tmp_int;
	    if ((align_tmp = (((unsigned long)*dest_field) % required_alignment)) != 0) {
		*dest_field += (required_alignment - align_tmp);
		*string_base_p  += (required_alignment - align_tmp);
	    }
	} else {
	    *dest_field = NULL;
	}
	*dest_p = *dest_field;
    } else if ((dest_size > sizeof(char *)) && (*string_base_p != NULL)) {
	/* native field is bigger than char*, store it */
	struct _IOgetFieldStruct tmp_src_field;  /* OK */
	MAX_UNSIGNED_TYPE tmp_int;
	tmp_src_field = *src_spec;

	tmp_src_field.data_type = integer_type;

	tmp_int = get_big_unsigned(&tmp_src_field, src);

	*src_offset_p = tmp_int;

	if (tmp_int != 0) {
	    /* handle possibly different string base */
	    tmp_int -= (long) string_offset_size;
	    *dest_p = tmp_int + *string_base_p;
	    tmp_int = tmp_int + (MAX_UNSIGNED_TYPE) (unsigned long) *string_base_p;
	    if ((align_tmp = (((unsigned long)*dest_p) % required_alignment)) != 0) {
		*dest_p = (char*)*dest_p + (required_alignment - align_tmp);
		tmp_int += (required_alignment - align_tmp);
		*string_base_p  += (required_alignment - align_tmp);
	    }
	} else {
	    *dest_p = NULL;
	}
	tmp_src_field.offset = 0;
	tmp_src_field.size = sizeof(MAX_UNSIGNED_TYPE);
	tmp_src_field.byte_swap = FALSE;
	ffs_internal_convert_field(iofile, &tmp_src_field, &tmp_int,
				    unsigned_type, dest_size,
				    dest, 0, *string_base_p, 0, FALSE);
    } else {
	/* not a native field struct.  Keep the pointer as an offset. */
	struct _IOgetFieldStruct tmp_src_field;  /* OK */
	MAX_INTEGER_TYPE tmp_int;
	tmp_src_field = *src_spec;

	tmp_src_field.data_type = integer_type;
	tmp_int = get_big_int(&tmp_src_field, src);

	*src_offset_p = tmp_int;
	if (tmp_int != 0) {
	    if (!converted_strings) {
		/* record size might change, adjust offset */
		tmp_int += size_delta;
	    }
	    *dest_p = tmp_int + *string_base_p;
	    if ((align_tmp = (((unsigned long)*dest_p) % required_alignment)) != 0) {
		*dest_p =  (char*)*dest_p + (required_alignment - align_tmp);
		tmp_int += (required_alignment - align_tmp);
		*string_base_p  += (required_alignment - align_tmp);
	    }
	} else {
	    *dest_p = NULL;
	}
	tmp_src_field.offset = 0;
	tmp_src_field.size = sizeof(MAX_INTEGER_TYPE);
	tmp_src_field.byte_swap = FALSE;
	ffs_internal_convert_field(iofile, &tmp_src_field, &tmp_int,
				    integer_type, dest_size,
				    dest, 0, *string_base_p, 0, FALSE);
    }
}

extern void
ffs_internal_convert_field(iofile, src_spec, src, dest_type, dest_size, dest,
	  string_offset_size, string_base, size_delta, converted_strings)
FFSContext iofile;
IOFieldPtr src_spec;
void *src;
FMdata_type dest_type;
int dest_size;
void *dest;
int string_offset_size;
char *string_base;
int size_delta;
int converted_strings;
{
    int float_OK = 1;
    if (dest_type == float_type) {
	FMfloat_format reverse_src = 
	    ffs_reverse_float_formats[src_spec->src_float_format];

	if (!src_spec->byte_swap && 
	    src_spec->src_float_format != src_spec->target_float_format) {
	    float_OK = 0;
	} else if (src_spec->byte_swap &&
		   reverse_src != src_spec->target_float_format) {
	    float_OK = 0;
	}
    }		   
    /* quick check to see if it's just a copy... */
    if ((dest_type != string_type) && (dest_type == src_spec->data_type) &&
	(dest_size == src_spec->size) && (float_OK == 1)) {
	if (src_spec->byte_swap) {
	    int i;
	    char *destc = (char *) dest;
	    char *srcc = (char *) src + src_spec->offset;
	    for (i = 0; i < (dest_size >> 1); i++) {
		char tmp = srcc[dest_size - i - 1];
		destc[dest_size - i - 1] = srcc[i];
		destc[i] = tmp;
	    }
	    if ((dest_size & 0x1) != 0) {
		destc[dest_size >> 1] = srcc[dest_size >> 1];
	    }
	} else {
	    char *srcc = (char *) src + src_spec->offset;
	    memcpy(dest, srcc, dest_size);
	}
	return;
    }
    switch (dest_type) {
    case integer_type:
	{
	    MAX_INTEGER_TYPE tmp = get_big_int(src_spec, src);
	    if (dest_size == sizeof(char)) {
		char *dest_field = (char *) dest;
		*dest_field = (char) tmp;
	    } else if (dest_size == sizeof(short)) {
		short *dest_field = (short *) dest;
		*dest_field = (short) tmp;
	    } else if (dest_size == sizeof(int)) {
		int *dest_field = (int *) dest;
		*dest_field = tmp;
	    } else if (dest_size == sizeof(long)) {
		long *dest_field = (long *) dest;
		*dest_field = tmp;
#if SIZEOF_LONG_LONG != 0
	    } else if (dest_size == sizeof(long long)) {
		long long lltmp = tmp;
		memcpy(dest, &lltmp, sizeof(long long));
#endif
	    } else {
/*		iofile->result = "size problems in conversion";*/
	    }
	    break;
	}
    case char_type:
	{
	    char tmp = get_big_int(src_spec, src);
	    char *dest_field = (char *) dest;
	    *dest_field = tmp;
	    break;
	}
    case boolean_type:
    case enumeration_type:
	{
	    int tmp = get_big_int(src_spec, src);
	    int *dest_field = (int *) dest;
	    *dest_field = tmp;
	    break;
	}
    case unsigned_type:
	{
	    MAX_UNSIGNED_TYPE tmp = get_big_unsigned(src_spec, src);
	    if (dest_size == sizeof(unsigned char)) {
		unsigned char *dest_field = (unsigned char *) dest;
		*dest_field = (unsigned char) tmp;
	    } else if (dest_size == sizeof(unsigned short)) {
		unsigned short *dest_field = (unsigned short *) dest;
		*dest_field = (unsigned short) tmp;
	    } else if (dest_size == sizeof(int)) {
		unsigned int *dest_field = (unsigned int *) dest;
		*dest_field = tmp;
	    } else if (dest_size == sizeof(long)) {
		unsigned long *dest_field = (unsigned long *) dest;
		*dest_field = tmp;
#if SIZEOF_LONG_LONG != 0
	    } else if (dest_size == sizeof(long long)) {
		unsigned long long *dest_field = (unsigned long long *) dest;
		*dest_field = tmp;
#endif
	    } else {
/*		iofile->result = "size problems in conversion";*/
	    }
	    break;
	}
    case float_type:
	{
	    MAX_FLOAT_TYPE tmp = get_big_float(src_spec, src);
	    if (dest_size == sizeof(float)) {
		float *dest_field = (float *) dest;
		*dest_field = (float) tmp;
	    } else if (dest_size == sizeof(double)) {
		double *dest_field = (double *) dest;
		*dest_field = tmp;
#if SIZEOF_LONG_DOUBLE != 0
	    } else if (dest_size == sizeof(long double)) {
		long double *dest_field = (long double *) dest;
		*dest_field = tmp;
#endif
	    } else {
/*		iofile->result = "size problems in conversion";*/
	    }
	    break;
	}
    case string_type:
	{
	    void *junk;
	    int junk_int;
	    /* use Junk values because we don't care about address output */
	    convert_addr_field(iofile, src_spec, src, dest_size, dest,
			       string_offset_size, &string_base, size_delta,
			       converted_strings, &junk_int, &junk, 1);
	    break;
	}
    default:
	assert(FALSE);
    }
}

extern long
get_array_element_count(FMFormat f, FMVarInfoList var, char *data, int encode)
{
    int i;
    long count = 1;
    long tmp;
    for (i = 0; i < var->dimen_count; i++) {
	if (var->dimens[i].static_size != 0) {
	    count = count * var->dimens[i].static_size;
	} else {
	    int field = var->dimens[i].control_field_index;
	    struct _IOgetFieldStruct tmp_src_spec;
	    memset(&tmp_src_spec, 0, sizeof(tmp_src_spec));
	    tmp_src_spec.size = f->field_list[field].field_size;
	    tmp_src_spec.offset = f->field_list[field].field_offset;
	    tmp_src_spec.data_type = integer_type;
	    if (!encode) {
		tmp_src_spec.byte_swap = 0;
		tmp_src_spec.src_float_format = tmp_src_spec.target_float_format;
	    }
	    tmp = get_big_int(&tmp_src_spec, data);
	    count = count * tmp;
	}
    }
    return count;
}

extern long
get_static_array_element_count(FMVarInfoList var)
{
    int i;
    long count = 1;
    if (var == NULL) return 1;
    for (i = 0; i < var->dimen_count; i++) {
	if (var->dimens[i].static_size != 0) {
	    count = count * var->dimens[i].static_size;
	} else {
	    return -1;
	}
    }
    return count;
}

static void
do_var_part_conv(IOConversionPtr conv, IOconvFieldStruct * conv_field,
		     void *src_area, void *final_area, int control_value,
		       void *src_string_base, void **final_string_base, void *orig_src);

static int debug_code_generation = -1;
static void
internal_convert_record(IOConversionPtr conv, void *src, void *dest, 
			      void **final_string_base_p, void *src_string_base);

static void
print_IOConversion(conv_ptr, indent)
IOConversionPtr conv_ptr;
int indent;
{
    int i;
    int ind;
    if (indent == 0) {
	for (ind = 0; ind < indent; ind++)
	    printf("    ");
	printf("IOConversion base type is ");
	if (conv_ptr == NULL) {
	    printf("NULL\n");
	    return;
	}
	switch (conv_ptr->conversion_type) {
	case none_required:
	    printf("None_Required\n");
	    break;
	case direct_to_mem:
	    printf("Direct_to_Memory\n");
	    break;
	case buffer_and_convert:
	    printf("Buffer_and_Convert\n");
	    break;
        case copy_dynamic_portion:
	    printf("Copy_Dynamic_Portion\n");
	    break;
	default:
	    assert(FALSE);
	    break;
	}
    }
    for (ind = 0; ind < indent; ind++)
	printf("    ");
    printf(" base_size_delta=%d, max_var_exp=%g, target_pointer_size=%d, string_offset=%d, converted_strings=%d\n",
	   conv_ptr->base_size_delta, conv_ptr->max_var_expansion,
	   conv_ptr->target_pointer_size, conv_ptr->string_offset_size,
	   conv_ptr->converted_strings);
    printf(" conversion_function= %lx, required_align=%d\n",
	   (long) conv_ptr->conv_func, conv_ptr->required_alignment);
    for (ind = 0; ind < indent; ind++)
	printf("    ");
    printf("  There are %d conversions registered:\n", conv_ptr->conv_count);
    for (i = 0; i < conv_ptr->conv_count; i++) {
	FMVarInfoStruct *iovar = conv_ptr->conversions[i].iovar;
	IOFieldPtr src_field = &conv_ptr->conversions[i].src_field;
	for (ind = 0; ind < indent; ind++)
	    printf("    ");
	printf("  Conversion %d:\n", i);
	for (ind = 0; ind < indent; ind++)
	    printf("    ");
	printf("    Base type : %s", data_type_to_str(src_field->data_type));
	if (conv_ptr->conversions[i].iovar != NULL) {
	    int j;
	    for (j = 0; j < iovar->dimen_count; j++) {
		if (iovar->dimens[j].static_size != 0) {
		    printf("[%d]", iovar->dimens[j].static_size);
		} else {
		    /* variant array */
		    int field = iovar->dimens[j].control_field_index;
		    FMFormat f = conv_ptr->ioformat->body;
		    int offset = f->field_list[field].field_offset;
		    int size = f->field_list[field].field_size;
		    printf("[ size at offset %d, %dbytes ]", offset, size);
		}
	    }
	}
	
	if (conv_ptr->conversions[i].rc_swap == swap_source_column_major) {
	    printf(" row/column swap required (SRC column-major) - ");
	}
	if (conv_ptr->conversions[i].rc_swap == swap_source_row_major) {
	    printf(" row/column swap required (SRC row-major) - ");
	}
	if (src_field->byte_swap) {
	    printf(" byte order reversal required\n");
	} else {
	    printf("\n");
	}
	if ((src_field->data_type == float_type) &&
	    (src_field->src_float_format != src_field->target_float_format)) {
	    printf("conversion from %s to %s required\n", 
		   float_format_str[(int)src_field->src_float_format],
		   float_format_str[(int)src_field->target_float_format]);
	}
	for (ind = 0; ind < indent; ind++)
	    printf("    ");
	if (conv_ptr->conversions[i].default_value == NULL) {
	    printf("    Src offset : %d    size %d\n", src_field->offset,
		   src_field->size);
	} else {
	    int j;
	    printf("    Default value : 0x");
	    for (j=0; j< conv_ptr->conversions[i].dest_size; j++) {
		printf("%02x", ((unsigned char*)conv_ptr->conversions[i].default_value)[j]);
	    }
	    printf("\n");
	}
	for (ind = 0; ind < indent; ind++)
	    printf("    ");
	printf("    Dst offset : %d    size %d\n",
	       conv_ptr->conversions[i].dest_offset,
	       conv_ptr->conversions[i].dest_size);
	if (conv_ptr->conversions[i].subconversion) {
	    for (ind = 0; ind < indent; ind++)
		printf("    ");
	    printf("    Subconversion as follows:\n");
	    print_IOConversion(conv_ptr->conversions[i].subconversion,
			       indent + 1);
	}
    }
}

static void
print_IOConversion_as_XML(conv_ptr, indent)
IOConversionPtr conv_ptr;
int indent;
{
    int i;
    int ind;
    if (indent == 0) {
	for (ind = 0; ind < indent; ind++)
	    printf("    ");
	printf("<IOConversion baseType=\"");
	if (conv_ptr == NULL) {
	    printf("NULL\" />");
	    return;
	}
	switch (conv_ptr->conversion_type) {
	case none_required:
	    printf("None_Required");
	    break;
	case direct_to_mem:
	    printf("Direct_to_Memory");
	    break;
	case buffer_and_convert:
	    printf("Buffer_and_Convert");
	    break;
	case copy_dynamic_portion:
	    printf("Copy_Strings");
	    break;
	default:
	    assert(FALSE);
	    break;
	}
	printf("\">\n");
    }
    for (ind = 0; ind < indent; ind++)
	printf("    ");
    printf("<baseSizeDelta>%d</baseSizeDelta>\n", conv_ptr->base_size_delta);
    printf("<maxVarExpansion>%g</maxVarExpansion>\n", conv_ptr->max_var_expansion);
    printf("<targetPointerSize>%d</targetPointerSize>\n", conv_ptr->target_pointer_size);
    printf("<stringOffsetSize>%d</stringOffsetSize>\n", conv_ptr->string_offset_size);
    printf("<convertedStrings>%d</convertedStrings>\n", conv_ptr->converted_strings);
    for (ind = 0; ind < indent; ind++)
	printf("    ");
    for (i = 0; i < conv_ptr->conv_count; i++) {
	IOFieldPtr src_field = &conv_ptr->conversions[i].src_field;
	FMVarInfoStruct *iovar = conv_ptr->conversions[i].iovar;

	for (ind = 0; ind < indent; ind++)
	    printf("    ");
	printf("<registeredConversion>\n");
	for (ind = 0; ind < indent; ind++)
	    printf("    ");
	printf("<baseType>%s</baseType>\n",
	 data_type_to_str(src_field->data_type));
	printf("<controlField>\n");
	if (conv_ptr->conversions[i].iovar != NULL) {
	    int j;
	    for (j = 0; j < iovar->dimen_count; j++) {
		if (iovar->dimens[j].static_size != 0) {
		    printf("<arrayDimension>%d</arrayDimension>", 
			   iovar->dimens[j].static_size);
		} else {
		    int field = iovar->dimens[j].control_field_index;
		    FMFormat f = conv_ptr->ioformat->body;
		    int offset = f->field_list[field].field_offset;
		    int size = f->field_list[field].field_size;
		    /* variant array */
		    printf("<offset>%d</offset><size units=\"bytes\">%d</size>\n",
			   offset, size);
		}
	    }
	}
	printf("</controlField>\n");
	if (src_field->byte_swap) {
	    printf("<byteReversal />\n");
	} else {
	    printf("\n");
	}
	for (ind = 0; ind < indent; ind++)
	    printf("    ");
	printf("<sourceOffset>%d</sourceOffset><sourceSize>%d</sourceSize>\n",
	       src_field->offset,
	       src_field->size);
	for (ind = 0; ind < indent; ind++)
	    printf("    ");
	printf("<destOffset>%d</destOffset><destSize>%d</destSize>\n",
	       conv_ptr->conversions[i].dest_offset,
	       conv_ptr->conversions[i].dest_size);
	if (conv_ptr->conversions[i].subconversion) {
	    for (ind = 0; ind < indent; ind++)
		printf("    ");
	    print_IOConversion_as_XML(conv_ptr->conversions[i].subconversion,
				      indent + 1);
	}
    }
    printf("</IOConversion>\n");
}


extern void
dump_IOConversion(conv_ptr)
IOConversionPtr conv_ptr;
{
    print_IOConversion(conv_ptr, 0);
}

extern void
dump_IOConversion_as_XML(conv_ptr)
IOConversionPtr conv_ptr;
{
    print_IOConversion_as_XML(conv_ptr, 0);
}

void
FFSconvert_record(conv, src, dest, final_string_base, src_string_base)
IOConversionPtr conv;
void *src;
void *dest;
void *final_string_base;
void *src_string_base;
{
    if (src_string_base == NULL) {
	src_string_base = final_string_base;
    }
    if (conv->conv_func) {
/*	printf("%s","");*/
	if (debug_code_generation) {
	    int i;
	    int limit = 30;
	    int *tmp = (int *) (((char *) src_string_base) -
				(((long) src_string_base) % 4));
	    printf("record of type \"%s\", contents :\n", 
		   conv->ioformat->body->format_name);
	    if (limit * sizeof(int) > conv->ioformat->body->record_length)
		limit = conv->ioformat->body->record_length / sizeof(int);
	    for (i = 0; i < limit; i += 4) {
		printf("%lx: %8x %8x %8x %8x\n", (long) ((char *) src) + (i * 4),
		       ((int *) src)[i], ((int *) src)[i + 1],
		       ((int *) src)[i + 2], ((int *) src)[i + 3]);
	    }
	    if (src_string_base != NULL) {
		printf("string contents :\n");
		limit = 10;
/*		if (conv->ioformat->body->variant) {
		    FILE_INT record_len;
		    int len_align_pad = (4 - conv->ioformat->body->server_ID.length) & 3;
		    FILE_INT *len_ptr = (FILE_INT *) (src + conv->ioformat->body->server_ID.length +
						      len_align_pad);
		    memcpy(&record_len, len_ptr, sizeof(FILE_INT));
		    if (conv->ioformat->body->byte_reversal)
			byte_swap((char *) &record_len, 4);
		    record_len -= conv->ioformat->body->record_length;
		    if (limit * sizeof(int) > record_len) {
			limit = record_len / sizeof(int);
		    }
		}
*/		for (i = 0; i < limit; i += 4) {
		    printf("%lx: %8x %8x %8x %8x\n", (long) ((char *) tmp) + (i * 4),
			   ((int *) tmp)[i],
			   ((int *) tmp)[i + 1],
			   ((int *) tmp)[i + 2],
			   ((int *) tmp)[i + 3]);
		}
	    }
	}
	conv->conv_func(src, dest, final_string_base, src_string_base);
	return;
    } else {
	internal_convert_record(conv, src, dest, &final_string_base, src_string_base);
    }
}

void
transpose_array(int *dimens, char *src, char *dest, int source_column_major,
		FMdata_type dest_type, int dest_size, IOConversionPtr conv,
		void *dest_base, IOFieldPtr src_spec, 
		void **final_string_base_p)
{
    int dimen_count = 0;
    int *index;
    int i, cur_index;
    int jump = 1;

    while (dimens[dimen_count] != 0) dimen_count++;
    struct _IOgetFieldStruct tmp_spec = *src_spec;

    if (dimen_count <= 1) return;
    index = malloc(sizeof(int) * dimen_count);
    for(i = 0; i< dimen_count; i++) index[i] = 0;
    cur_index = 0;
    jump = 1;
    for (i = 0; i < dimen_count-1; i++) {
	jump = (jump * dimens[i]);
    }
    while(index[0] < dimens[0]) {
	if (cur_index == (dimen_count-1)) {
	    int col_index_base = 0;
	    int row_index_base = 0;
	    int i;
	    void * dest_field;
	    if (dimen_count >= 2) {
		col_index_base = index[dimen_count-1];
		for (i = dimen_count-1; i >= 0; i--) {
		    col_index_base = (col_index_base * dimens[i] + index[i]);
		}
		row_index_base = index[0];
		for (i = 1; i < dimen_count; i++) {
		    row_index_base = (row_index_base * dimens[i] + index[i]);
		}
	    }
	    if (source_column_major) {
		dest_field = ((char*)dest_base) + dest_size * row_index_base;
		tmp_spec.offset = tmp_spec.size * col_index_base;
	    } else {
		dest_field = ((char*)dest_base) + dest_size * col_index_base;
		tmp_spec.offset = tmp_spec.size * row_index_base;
	    }
	    for(i=0; i < dimens[cur_index]; i++) {
		if (dest_type != unknown_type) {
		    /* simple (native) field or variant array */
		    if (dest_type != string_type) {
/*GSE var char blocks here*/
			ffs_internal_convert_field(conv->context, &tmp_spec, src,
						    dest_type, dest_size,
				    dest_field, conv->string_offset_size,
						    *final_string_base_p,
						    conv->base_size_delta,
						conv->converted_strings);
		    } else {
			printf("Bad type in transpose\n");
			free(index);
			return;
		    }
		} else {
		    printf("Bad type in transpose\n");
		    free(index);
		    return;
		}
		if (source_column_major) {
		    dest_field = (char*)dest_field + dest_size;
		    tmp_spec.offset += tmp_spec.size * jump;
		} else {
		    dest_field = (char*)dest_field + (dest_size * jump);
		    tmp_spec.offset += tmp_spec.size;
		}
		col_index_base += jump;
		row_index_base++;
	    }
	    cur_index--;
	    index[cur_index]++;
	} else {
	    if (index[cur_index] == dimens[cur_index]) {
		index[cur_index] = 0;
		cur_index--;
		if (cur_index == -1) {
		    free(index);
		    return;
		}
		index[cur_index]++;
	    } else {
		cur_index++;
	    }
	}
    }
}

static void
internal_convert_record(conv, src, dest, final_string_base_p, src_string_base)
IOConversionPtr conv;
void *src;
void *dest;
void **final_string_base_p;
void *src_string_base;
{
    int i;
    long *control_value = NULL;

    for (i = 0; i < conv->conv_count; i++) {
	if (conv->conversions[i].iovar != NULL) {
	    if (get_static_array_element_count(conv->conversions[i].iovar) == -1) {
		if (control_value == NULL) {
		    int j;
		    control_value = (long *) malloc(sizeof(long) * conv->conv_count);
		    for (j = 0; j < conv->conv_count; j++)
			control_value[j] = 0;
		}
		control_value[i] = 
		    get_array_element_count(conv->ioformat->body, conv->conversions[i].iovar, src, 1);
	    }
	}
    }
    for (i = 0; i < conv->conv_count; i++) {
	IOFieldPtr src_spec = &conv->conversions[i].src_field;
	int elements = get_static_array_element_count(conv->conversions[i].iovar);
	int byte_swap = conv->conversions[i].src_field.byte_swap;

	if (conv->conversions[i].src_field.size == 1) byte_swap = 0;
	if (conv->conversions[i].default_value){
	    void *dest_field = (void *) (conv->conversions[i].dest_offset + 
					 (char *) dest);
	    memcpy(dest_field, conv->conversions[i].default_value, 
		   conv->conversions[i].dest_size);
	} else if (!byte_swap &&
		   (src_spec->size == conv->conversions[i].dest_size) &&
		   (conv->conversions[i].subconversion == NULL) &&
		   (elements != -1) &&
		   (conv->conversions[i].rc_swap == no_row_column_swap) &&
		   ((src_spec->data_type != float_type) || 
		    (src_spec->src_float_format == src_spec->target_float_format)) &&
		   ((src_spec->data_type != string_type) || 
		    (final_string_base_p == NULL) || ((*final_string_base_p) == NULL))) {
	    /* data movement is all that is required */
	    void *dest_field = (void *) (conv->conversions[i].dest_offset +
					 (char *) dest);
	    if (elements == 1) {
		memcpy(dest_field, (char *) src + src_spec->offset,
		       src_spec->size);
	    } else {
		memcpy(dest_field, (char *) src + src_spec->offset,
		       src_spec->size * elements);
	    }
	} else if ((conv->conversions[i].rc_swap == no_row_column_swap) ||
		   (elements == -1) /* var array */ ){
	    int j;
	    void *dest_field = (void *) (conv->conversions[i].dest_offset +
					 (char *) dest);
	    struct _IOgetFieldStruct tmp_spec;  /* OK */
	    FMdata_type dest_type = src_spec->data_type;
	    int dest_size = conv->conversions[i].dest_size;
	    int iter_count;
	    tmp_spec = *src_spec;

	    if (elements == -1) {
		/* really a variant array, adjust address like a string */
		tmp_spec.data_type = string_type;
		tmp_spec.size = conv->ioformat->body->pointer_size;
		dest_type = string_type;
		dest_size = conv->target_pointer_size;
		iter_count = 1;
	    } else {
		iter_count = elements;
	    }
	    tmp_spec.offset = src_spec->offset - tmp_spec.size;
	    for (j = 0; j < iter_count; j++) {
		tmp_spec.offset += tmp_spec.size;
		if (dest_type != unknown_type) {
		    /* simple (native) field or variant array */
		    if (dest_type != string_type) {
/*GSE var char blocks here*/
			ffs_internal_convert_field(conv->context, &tmp_spec, src,
						    dest_type, dest_size,
				    dest_field, conv->string_offset_size,
						    *final_string_base_p,
						    conv->base_size_delta,
						conv->converted_strings);
		    } else {
			int src_offset;
			void *src_base;
			void *dst_base;
			/* string or variant array */
			int req_align;
			if (elements == -1) {
			    req_align = min_align_size(conv->conversions[i].dest_size);
			} else {
			    req_align = 1;
			}
			convert_addr_field(conv->context, &tmp_spec, src,
					   dest_size, dest_field,
					   conv->string_offset_size,
					   final_string_base_p,
					   conv->base_size_delta,
					   conv->converted_strings,
					   &src_offset, &dst_base,
					   req_align);
			src_base = (char *) src_string_base + src_offset
			    - conv->string_offset_size;
			if ((conv->conversion_type == copy_dynamic_portion) ||
			    (elements == -1)) {
			    int control = 0;
			    if (control_value != NULL)
				control = control_value[i];
			    do_var_part_conv(conv, &conv->conversions[i],
					     src_base, dst_base,
					     control, src_string_base,
					     final_string_base_p, src);
			}
		    }
		} else {
		    IOConversionPtr subconv =
		    conv->conversions[i].subconversion;
		    void *subsrc = (void *) ((char *) src + tmp_spec.offset);
		    internal_convert_record(subconv, subsrc, dest_field,
				     final_string_base_p, src_string_base);
		}
		dest_field = (void *) ((char *) dest_field +
				       conv->conversions[i].dest_size);
	    }
	} else {   
	    /* 
	     * the only remaining possibility is that this is a
	     * multi-dimensional array that requires a transpose as well as
	     * some possible conversion
	     */
	    int source_column_major = 
		(conv->conversions[i].rc_swap == swap_source_column_major);
	    int dimen_count = conv->conversions[i].iovar->dimen_count;
	    int *dimens = malloc(sizeof(int) * (dimen_count + 1));
	    int j;
	    FMdata_type dest_type = src_spec->data_type;
	    int dest_size = conv->conversions[i].dest_size;
	    void *dest_base = (void *) (conv->conversions[i].dest_offset +
					 (char *) dest);

	    for (j=0; j < dimen_count; j++) {
		int stat = conv->conversions[i].iovar->dimens[j].static_size;
		if (stat != -1) {
		    dimens[j] = stat;
		} else {
		    FMFormat f = conv->ioformat->body;
		    int field = conv->conversions[i].iovar->dimens[j].control_field_index;
		    struct _IOgetFieldStruct tmp_src_spec;
		    memset(&tmp_src_spec, 0, sizeof(tmp_src_spec));
		    tmp_src_spec.size = f->field_list[field].field_size;
		    tmp_src_spec.offset = f->field_list[field].field_offset;
		    tmp_src_spec.data_type = integer_type;
		    dimens[j] = get_big_int(&tmp_src_spec, src);
		}
	    }
	    dimens[dimen_count] = 0;
	    transpose_array(dimens, (char *) src + src_spec->offset, dest_base,
			    source_column_major, dest_type, dest_size, conv, 
			    dest_base, src_spec, final_string_base_p);
	    free(dimens);
	}

    }
    if (control_value != NULL) {
	free(control_value);
    }
}

static void
do_var_part_conv(conv, conv_field, src_area, final_area,
		 control_value, src_string_base, final_string_base_p, orig_src)
IOConversionPtr conv;
IOconvFieldStruct *conv_field;
void *src_area;
void *final_area;
int control_value;
void *src_string_base;
void **final_string_base_p;
void *orig_src;
{
    int copy_length = 0;
    IOFieldPtr src_spec = &(conv_field->src_field);
    int elements = get_static_array_element_count(conv_field->iovar);

    /* handle copying or conversion */
    if (elements != -1) {
	/* simple string */
	if ((src_area != NULL) && (final_area != NULL)) {
	    int copy_length = strlen(src_area) + 1;
	    memcpy(final_area, src_area, copy_length);
	}
    } else {
	/* variant array */
	int byte_swap = conv_field->src_field.byte_swap;
	if (src_spec->size == 1) byte_swap = 0;
	if (!byte_swap &&
	    (src_spec->src_float_format == src_spec->target_float_format) &&
	    (src_spec->size == conv_field->dest_size) &&
	    (conv_field->subconversion == NULL) &&
	    (conv_field->rc_swap == no_row_column_swap) &&
	    (src_spec->data_type != string_type)) {

	    /* data movement is all that is required */
	    if (conv->conversion_type == copy_dynamic_portion) {
		copy_length = conv_field->dest_size *
		    control_value;
		memcpy(final_area, src_area, copy_length);
	    } else {
		/* not even that is necessary */
	    }
	} else if (conv_field->rc_swap == no_row_column_swap) {
	    /* must do conversions */
	    int length = control_value;
	    int j;
	    void *dest_field = final_area;
	    struct _IOgetFieldStruct tmp_spec;  /* OK */
	    FMdata_type dest_type = src_spec->data_type;
	    int dest_size = conv_field->dest_size;

	    tmp_spec = *src_spec;
	    *final_string_base_p = (char *) *final_string_base_p +
		(length * (dest_size - src_spec->size));
	    for (j = 0; j < length; j++) {
		tmp_spec.offset = j * src_spec->size;
		if (dest_type != unknown_type) {
		    /* simple (native) field in variant array */
		    if (src_spec->data_type != string_type) {
			ffs_internal_convert_field(conv->context, &tmp_spec,
						    src_area, dest_type, 
						    dest_size, dest_field,
						    conv->string_offset_size,
						    *final_string_base_p,
						    conv->base_size_delta,
						    conv->converted_strings);
		    } else {
			/* variant string array */
			void *dest_base;
			void *src_base;
			int src_offset;
			convert_addr_field(conv->context, &tmp_spec, src_area,
					   dest_size, dest_field,
					   conv->string_offset_size, 
					   final_string_base_p, 
					   conv->base_size_delta,
					   conv->converted_strings,
					   &src_offset, &dest_base, 1);
			src_base = (char *) src_string_base + src_offset
			    - conv->string_offset_size;
			if (conv->conversion_type == copy_dynamic_portion) {
			    int copy_length = strlen(src_base) + 1;
			    memcpy(dest_base, src_base, copy_length);
			}
		    }
		        
		} else {
		    IOConversionPtr subconv =
		    conv_field->subconversion;
		    void *subsrc = (char *) src_area + j * src_spec->size;
		    internal_convert_record(subconv, subsrc, dest_field,
					    final_string_base_p,
					    src_string_base);
		}
		dest_field = (void *) ((char *) dest_field +
				       conv_field->dest_size);
	    }
	} else {
	    /* 
	     * the only remaining possibility is that this is a
	     * multi-dimensional array that requires a transpose as well as
	     * some possible conversion
	     */
	    int source_column_major = 
		(conv_field->rc_swap == swap_source_column_major);
	    int dimen_count = conv_field->iovar->dimen_count;
	    int *dimens = malloc(sizeof(int) * (dimen_count + 1));
	    int j;
	    FMdata_type dest_type = src_spec->data_type;
	    int dest_size = conv_field->dest_size;
	    for (j=0; j < dimen_count; j++) {
		int stat = conv_field->iovar->dimens[j].static_size;
		if (stat != -1) {
		    dimens[j] = stat;
		} else {
		    FMFormat f = conv->ioformat->body;
		    int field =conv_field->iovar->dimens[j].control_field_index;
		    struct _IOgetFieldStruct tmp_src_spec;
		    memset(&tmp_src_spec, 0, sizeof(tmp_src_spec));
		    tmp_src_spec.size = f->field_list[field].field_size;
		    tmp_src_spec.offset = f->field_list[field].field_offset;
		    tmp_src_spec.data_type = integer_type;
		    dimens[j] = get_big_int(&tmp_src_spec, orig_src);
		}
	    }
	    dimens[dimen_count] = 0;
/*	    printf("Would do transpose 2, src_area = %lx, dest_base = %lx\n",
	    src_area, final_area);*/
	    transpose_array(dimens, (char *) src_area, final_area,
			    source_column_major, dest_type, dest_size, conv, 
			    final_area, src_spec, final_string_base_p);
	    free(dimens);
	}
    }
}

static MAX_INTEGER_TYPE
get_big_int(iofield, data)
IOFieldPtr iofield;
void *data;
{
    if (iofield->data_type == integer_type) {
	if (iofield->size == sizeof(char)) {
	    char tmp;
	    memcpy(&tmp, (char *) data + iofield->offset, sizeof(char));
	    return (long) tmp;
	} else if (iofield->size == sizeof(short)) {
	    short tmp;
	    memcpy(&tmp, (char *) data + iofield->offset, sizeof(short));
	    if (iofield->byte_swap)
		byte_swap((char *) &tmp, sizeof(short));
	    return (long) tmp;
	} else if (iofield->size == sizeof(int)) {
	    int tmp;
	    memcpy(&tmp, (char *) data + iofield->offset, sizeof(int));
	    if (iofield->byte_swap)
		byte_swap((char *) &tmp, sizeof(int));
	    return (long) tmp;
	} else if (iofield->size == sizeof(long)) {
	    long tmp;
	    memcpy(&tmp, (char *) data + iofield->offset, sizeof(long));
	    if (iofield->byte_swap)
		byte_swap((char *) &tmp, sizeof(long));
	    return tmp;
	} else if (iofield->size == 2 * sizeof(long)) {
	    long tmp;
	    int low_bytes_offset = iofield->offset;
#ifdef WORDS_BIGENDIAN
	    if (!iofield->byte_swap) {
		low_bytes_offset += sizeof(long);
	    }
#else
	    if (iofield->byte_swap) {
		low_bytes_offset += sizeof(long);
	    }
#endif
	    memcpy(&tmp, (char *) data + low_bytes_offset, sizeof(long));
	    if (iofield->byte_swap)
		byte_swap((char *) &tmp, sizeof(long));
	    return tmp;
	} else {
	    if (!IO_shut_up && !get_long_warn) {
		fprintf(stderr, "Get Long failed!  Size problems.  File int size is %d.\n", iofield->size);
		get_long_warn++;
	    }
	    return -1;
	}
    } else if (iofield->data_type == unsigned_type) {
	MAX_UNSIGNED_TYPE tmp = get_big_unsigned(iofield, data);
	return (MAX_UNSIGNED_TYPE) tmp;
    } else if (iofield->data_type == float_type) {
	MAX_FLOAT_TYPE tmp = get_big_float(iofield, data);
#ifndef METICULOUS_FLOATS_AND_LONGS
	return (MAX_INTEGER_TYPE) (long) (double) tmp;
#else
	return (MAX_INTEGER_TYPE) tmp;
#endif
    } else {
	fprintf(stderr, "Get IOlong failed on invalid data type!\n");
	exit(1);
    }
    /* NOTREACHED */
    return 0;
}

static MAX_UNSIGNED_TYPE
get_big_unsigned(iofield, data)
IOFieldPtr iofield;
void *data;
{
    if ((iofield->data_type == unsigned_type) || 
	(iofield->data_type == enumeration_type) || 
	(iofield->data_type == boolean_type)) {
	if (iofield->size == sizeof(char)) {
	    unsigned char tmp;
	    memcpy(&tmp, (char *) data + iofield->offset, sizeof(char));
	    return (MAX_UNSIGNED_TYPE) tmp;
	} else if (iofield->size == sizeof(short)) {
	    unsigned short tmp;
	    memcpy(&tmp, (char *) data + iofield->offset, sizeof(short));
	    if (iofield->byte_swap)
		byte_swap((char *) &tmp, sizeof(short));
	    return (MAX_UNSIGNED_TYPE) tmp;
	} else if (iofield->size == sizeof(int)) {
	    unsigned int tmp;
	    memcpy(&tmp, (char *) data + iofield->offset, sizeof(int));
	    if (iofield->byte_swap)
		byte_swap((char *) &tmp, sizeof(int));
	    return (MAX_UNSIGNED_TYPE) tmp;
	} else if (iofield->size == sizeof(long)) {
	    unsigned long tmp;
	    memcpy(&tmp, (char *) data + iofield->offset, sizeof(long));
	    if (iofield->byte_swap)
		byte_swap((char *) &tmp, sizeof(long));
	    return tmp;
	} else if (iofield->size == 2 * sizeof(long)) {
	    unsigned long tmp;
	    int low_bytes_offset = iofield->offset;
#ifdef WORDS_BIGENDIAN
	    if (!iofield->byte_swap) {
		low_bytes_offset += sizeof(long);
	    }
#else
	    if (iofield->byte_swap) {
		low_bytes_offset += sizeof(long);
	    }
#endif
	    memcpy(&tmp, (char *) data + low_bytes_offset, sizeof(long));
	    if (iofield->byte_swap)
		byte_swap((char *) &tmp, sizeof(long));
	    return tmp;
	} else {
	    if (!IO_shut_up && !get_long_warn) {
		fprintf(stderr, "Get Long failed!  Size problems.  File int size is %d.\n", iofield->size);
		get_long_warn++;
	    }
	    return 0;
	}
    } else if (iofield->data_type == integer_type) {
	MAX_INTEGER_TYPE tmp = get_big_int(iofield, data);
	return (MAX_UNSIGNED_TYPE) tmp;
    } else if (iofield->data_type == float_type) {
	MAX_FLOAT_TYPE tmp = get_big_float(iofield, data);
#ifndef METICULOUS_FLOATS_AND_LONGS
	return (MAX_UNSIGNED_TYPE) (long) (double) tmp;
#else
	return (MAX_UNSIGNED_TYPE) tmp;
#endif
    } else {
	fprintf(stderr, "Get IOulong failed on invalid data type!\n");
	exit(1);
    }
    /* NOTREACHED */
    return 0;
}

#define CONV(a,b) ((a<<16) + b)
static void
float_conversion(unsigned char*value, int size, FMfloat_format src_format,
		 FMfloat_format dest_format)
{
    int tmp;
    if (src_format == dest_format) return;
    if (src_format == ffs_reverse_float_formats[dest_format]) {
	byte_swap((char *) value, size);
	return;
    }
    switch (CONV(src_format, dest_format)) {
    case CONV(Format_IEEE_754_bigendian, Format_IEEE_754_mixedendian):
    case CONV(Format_IEEE_754_mixedendian, Format_IEEE_754_bigendian):
	byte_swap((char*)&value[0], 4);
	byte_swap((char*)&value[4], 4);
	break;
    case CONV(Format_IEEE_754_littleendian, Format_IEEE_754_mixedendian):
    case CONV(Format_IEEE_754_mixedendian, Format_IEEE_754_littleendian):
	tmp = *(int*)&value[0];
	*(int*)&value[0] = *(int*)&value[4];
	*(int*)&value[4] = tmp;
	break;
    default:
	printf("unanticipated float conversion \n");
    }
}

static MAX_FLOAT_TYPE
get_big_float(iofield, data)
IOFieldPtr iofield;
void *data;
{
    if (iofield->data_type == float_type) {
	if (iofield->size == sizeof(float)) {
	    float tmp;
	    MAX_FLOAT_TYPE tmp2;
	    memcpy(&tmp, ((char *) data + iofield->offset), sizeof(float));
	    if (iofield->byte_swap)
		byte_swap((char *) &tmp, sizeof(float));
	    tmp2 = tmp;
	    return tmp2;
	} else if (iofield->size == sizeof(double)) {
	    double tmp;
	    MAX_FLOAT_TYPE tmp2;
	    memcpy(&tmp, ((char *) data + iofield->offset), sizeof(double));
	    float_conversion((unsigned char *)&tmp, sizeof(double), 
			     (FMfloat_format)iofield->src_float_format,
			     (FMfloat_format)iofield->target_float_format);
	    tmp2 = tmp;
	    return tmp2;
#if SIZEOF_LONG_DOUBLE != 0
	} else if (iofield->size == sizeof(long double)) {
	    long double tmp;
	    MAX_FLOAT_TYPE tmp2;
	    memcpy(&tmp, ((char *) data + iofield->offset),
		   sizeof(long double));
	    if (iofield->byte_swap)
		byte_swap((char *) &tmp,
			  sizeof(long double));
	    tmp2 = tmp;
	    return tmp2;
#endif
	} else {
	    if (!IO_shut_up && !get_double_warn) {
		fprintf(stderr, "Get Double failed!  Size problems.  File double size is %d.\n", iofield->size);
		get_double_warn++;
	    }
	    return 0.0;
	}
    } else if (iofield->data_type == integer_type) {
	MAX_INTEGER_TYPE tmp = get_big_int(iofield, data);
#ifndef METICULOUS_FLOATS_AND_LONGS
	/* 
	 * A concession to inter-compiler interoperability...  We
	 * shouldn't need the (double)(long) casts here.  If we don't
	 * use them, AND we're on a machine which doesn't support "long 
	 * long" and "long double" in native code, gcc generates calls 
	 * to libgcc and the resulting library can't be linked with
	 * anything but GCC.  This can be a problem for installed
	 * libraries.  Using the casts avoids that particular problem,
	 * at a cost of data loss in the case of someone converting a
	 * "long long" that doesn't fit in a long to a floating point
	 * value. 
	 */
	return (MAX_FLOAT_TYPE) (double) (long) tmp;
#else
	return (MAX_FLOAT_TYPE) tmp;
#endif
    } else if (iofield->data_type == unsigned_type) {
	MAX_UNSIGNED_TYPE tmp = get_big_unsigned(iofield, data);
#ifndef METICULOUS_FLOATS_AND_LONGS
	return (MAX_FLOAT_TYPE) (double) (long) tmp;
#else
	return (MAX_FLOAT_TYPE) tmp;
#endif
    } else {
	fprintf(stderr, "Get Double failed on invalid data type!\n");
	exit(1);
    }
    /* NOTREACHED */
    return 0;
}

static void
byte_swap(data, size)
char *data;
int size;
{
    int i;
    assert((size % 2) == 0);
    for (i = 0; i < size / 2; i++) {
	char tmp = data[i];
	data[i] = data[size - i - 1];
	data[size - i - 1] = tmp;
    }
}

extern int
min_align_type(typ, size)
FMdata_type typ;
int size;
{
#ifndef HAVE_DRISC_H
    return min_align_size(size);
#else
    static drisc_ctx c = NULL;
    if (c == NULL) c = dr_init();
    switch (typ) {
    case float_type:
	if (size == dr_type_size(c, DR_D)) return dr_type_align(c, DR_D);
	if (size == dr_type_size(c, DR_F)) return dr_type_align(c, DR_F);
	/* punt */
	return min_align_size(size);
    case integer_type: case char_type: case string_type:
	if (size == dr_type_size(c, DR_C)) return dr_type_align(c, DR_C);
	if (size == dr_type_size(c, DR_S)) return dr_type_align(c, DR_S);
	if (size == dr_type_size(c, DR_I)) return dr_type_align(c, DR_I);
	if (size == dr_type_size(c, DR_L)) return dr_type_align(c, DR_L);
	/* punt */
	return min_align_size(size);
    case unsigned_type: case enumeration_type: case boolean_type:
	if (size == dr_type_size(c, DR_UC)) return dr_type_align(c, DR_UC);
	if (size == dr_type_size(c, DR_US)) return dr_type_align(c, DR_US);
	if (size == dr_type_size(c, DR_U)) return dr_type_align(c, DR_U);
	if (size == dr_type_size(c, DR_UL)) return dr_type_align(c, DR_UL);
	/* punt */
	return dr_type_align(c, DR_B);
    default:
	return dr_type_align(c, DR_B);
    }
#endif
}

#if !defined(HAVE_DRISC_H)
extern
 conv_routine
generate_conversion(conv, alignment)
IOConversionPtr conv;
int alignment;
{
    return NULL;
}
#else

#undef max
#define max(x,y) (x<y?y:x)

static int
drisc_type(field)
struct _IOgetFieldStruct *field;
{
    switch(field->data_type) {
    case integer_type:
    case unsigned_type:
	switch(field->size) {
	case 1:
	    return DR_C;
	case 2:
	    return DR_S;
	case 4:
	    return DR_I;
	case 8:
	    return DR_L;
	}
	return DR_I;
    case float_type:
	if (field->size == SIZEOF_DOUBLE) {
	    return DR_D;
	} else if (field->size == SIZEOF_FLOAT) {
	    return DR_F;
	} else {
	    return DR_I;
	}
    case char_type:
	return DR_C;
    case string_type:
	return DR_P;
    case boolean_type:
    case enumeration_type:
	return DR_I;
    default:
	return DR_I;
    }
}

static int
subfield_required_align(c, conv, i, offset)
drisc_ctx c;
IOConversionPtr conv;
int i;
int offset;
{
    int field_required_align;
    int field_align, access_align;
    static int structure_align = -1;
    int elements = get_static_array_element_count(conv->conversions[i].iovar);
    if (structure_align == -1) {
	int j;
	for (j=DR_C; j<= DR_D; j++) {
	    structure_align = max(structure_align, TYPE_ALIGN(c, j));
	}
    }
    if (elements == -1) {
	field_required_align = TYPE_ALIGN(c, DR_P);
    } else if (conv->conversions[i].subconversion == NULL) {
	int drisc_data_type = drisc_type(&conv->conversions[i].src_field);
	field_required_align = TYPE_ALIGN(c, drisc_data_type);
    } else {
	field_required_align = structure_align;
    }
    field_align = offset % field_required_align;
    if (field_align == 0) {
	access_align = field_required_align;
    } else {
	int access_align1 = min_align_size(field_align);
	int access_align2 =
	    min_align_size(field_required_align - field_align);
	access_align = max(access_align1, access_align2);
    }
    return access_align;
}

static int
conv_required_alignment(c, conv)
drisc_ctx c;
IOConversionPtr conv;
{
    int i;
    int required_align = 0;
    if (conv->conv_count == 0) return 0;
    for (i = 0; i < conv->conv_count; i++) {
	int subfield_requires = 
	    subfield_required_align(c, conv, i, 
				    conv->conversions[i].src_field.offset);
	required_align = max(subfield_requires, required_align);
    }
    assert(required_align != 0);
    return required_align;
}

static
 conv_routine
 generate_conversion_code(drisc_ctx c,
				IOConversionPtr conv, dr_reg_t * args,
				int assume_align, int register_args,
				int extra_src_offset, 
				int extra_dest_offset);

static int ffs_conversion_generation = -1;
static int generation_verbose = -1;

#define gen_fatal(str) do {fprintf(stderr, "%s\n", str); exit(1);} while (0)

extern
 conv_routine
generate_conversion(conv, base_alignment)
IOConversionPtr conv;
int base_alignment;
{
    drisc_ctx c = NULL;
    void (*conversion_routine)();
    dr_reg_t args[6];
    dr_reg_t tmp_regs[10];
    char *format_name = conv->ioformat->body->format_name;
    int count = 0, register_args = 1;

    if (ffs_conversion_generation == -1) {
	char *gen_string = cercs_getenv("FFS_CONVERSION_GENERATION");
	ffs_conversion_generation = FFS_CONVERSION_GENERATION_DEFAULT;
	if (gen_string != NULL) {
#ifdef MODULE
	    ffs_conversion_generation = strtol(gen_string, NULL, 10);
            if ((ffs_conversion_generation == LONG_MIN) ||
                (ffs_conversion_generation == LONG_MAX) || 
                (ffs_conversion_generation == 0)) {
#else
	    if (sscanf(gen_string, "%d", &ffs_conversion_generation) != 1) {
#endif
		if (*gen_string == 0) {
		    /* empty string, just turn on generation */
		    ffs_conversion_generation = 1;
		} else {
		    printf("Unable to parse FFS_CONVERSION_GENERATION environment variable \"%s\".\n", gen_string);
		}
	    }
	}
	debug_code_generation =
	    (getenv("FFS_CONVERSION_DEBUG") != NULL);
	generation_verbose =
	    (getenv("FFS_CONVERSION_VERBOSE") != NULL);
    }
    if (!ffs_conversion_generation)
	return NULL;
    if (generation_verbose) {
	printf("For format %s ===================\n", format_name);
	dump_IOConversion(conv);
    }

    {
	/* 
	 *  Determine whether or not some arguments should be left on the
	 *  stack.  The issue is that some architectures (x86) don't have
	 *  enough registers to follow the usual vcode convention of
	 *  immediately moving all arguments into registers for the
	 *  convenience of the generated subroutine.  If we let it do that,
	 *  we don't have enough temporary registers for use here.  So, we
	 *  first find out how many we have by doing getreg() until it fails
	 *  (or a max of 10 times).  If we don't have at least 8 registers
	 *  (four for args and four for temporary use), then we only want 
	 *  the first two arguments of the conversion code (the source and
	 *  destination address) to be kept in registers.  Trick vcode into
	 *  doing this by making all but two of the registers unavailable.
	 *  
	 *  IF WE DETERMINE WE HAVE ENOUGH REGISTERS, 'register_args' is set
	 *  to TRUE and the 'args' array contains the usual register
	 *  numbers.  If we don't have enough registers, 'register_args' is
	 *  false and args[2] and args[3] contain the stack offsets of the
	 *  third and fourth arguments.
	 */
	c = dr_init();
	count = 0;
	for (; count < sizeof(tmp_regs)/sizeof(tmp_regs[0]); count++) 
	    tmp_regs[count] = -1;
	count = 0;
	for (; count < sizeof(tmp_regs)/sizeof(tmp_regs[0]); count++) {
	    if (dr_getreg(c, &tmp_regs[count], DR_I, DR_VAR) == 0) {
		break;
	    }
	}
	if (count <= 8) {
	    int i;
	    register_args = 0;
	    for (i= 2; i < count; i++) {  /* Make all but 2 unavail*/
		dr_mk_unavail(c, DR_I, tmp_regs[i]);
	    }
	}
    }
    if (register_args) {
	/* Normal, lots of registers, case */

	dr_proc_params(c, "convert", DR_I, "%p%p%p%p");
	args[0] = dr_param(c, 0);
	args[1] = dr_param(c, 1);
	args[2] = dr_param(c, 2);
	args[3] = dr_param(c, 3);

    } else {
	/* very few registers case */

	drisc_parameter_type dr_params[4];	/* drisc param info */
	int i;
	for (i=0; i < 4; i++) {
	    dr_param_alloc(c, i, DR_P, (dr_reg_t*)&args[i]);
	    dr_param_struct_alloc(c, i, DR_P, &(dr_params[i]));
	}
	dr_proc(c, "convert", DR_I);

	/* store argument stack offsets in args[2] and args[3] */
	args[2] = dr_params[2].offset;
	args[3] = dr_params[3].offset;
	for (count=2; count<sizeof(tmp_regs)/sizeof(tmp_regs[0]); count++) {
	    if (tmp_regs[count] != -1) {
		/* make the other registers available again */
		dr_mk_avail(c, DR_I, tmp_regs[count]);
	    }
	}
    }
    if (debug_code_generation) {
	if (register_args) {
	    VCALL6V( printf, "%P%P%p%p%p%p",
		     "convert for %s called with src= %lx, dest %lx, final_string =%lx, src_string =%lx\n",
		     format_name, args[0], args[1], args[2], args[3]);
	} else {
#ifdef HAVE_DRISC_H	    
	    dr_reg_t v_at;
	    if (dr_getreg(c, &v_at, DR_I, DR_TEMP) == 0) {
		gen_fatal("Out of regs 1\n");
	    }
#endif
	    VCALL4V(printf, "%P%P%p%p",
		     "convert for %s called with src= %lx, dest %lx\n",
		     format_name, args[0], args[1]);
	    dr_ldpi(c, v_at, dr_lp(c), args[2]);
	    VCALL2V(printf, "%P%p",
		     "               src_string_base %lx\n",
		     v_at);
	    dr_ldpi(c, v_at, dr_lp(c), args[3]);
	    VCALL2V(printf, "%P%p",
		     "               final_string_base %lx\n",
		     v_at);
#ifdef HAVE_DRISC_H	    
	    dr_putreg(c, v_at, DR_I);
#endif
	}
    }
    conv->required_alignment = conv_required_alignment(c, conv);
    if (register_args) {
	dr_reg_t tmp;
	int mask;
	int zero_target = dr_genlabel(c);
	if (dr_getreg(c, &tmp, DR_I, DR_VAR) == 0) {
	    printf("out of regs for mod\n");
	}
	switch(conv->required_alignment) {
	case 2:
	    mask = 0x1;
	    break;
	case 4:
	    mask = 0x3;
	    break;
	case 8:
	    mask = 0x7;
	    break;
	default:
	    mask = 0;
	}
	if (mask != 0) {
	    dr_anduli(c, tmp, args[0], mask);
	    dr_beqli(c, tmp, 0, zero_target);
	    VCALL4V(printf, "%P%P%p%I",
		    "convert for %s called with bad align src= %lx, align is %d\n",
		    format_name, args[0], conv->required_alignment);
	    dr_label(c, zero_target);
	}
    }
    generate_conversion_code(c, conv, args, base_alignment, register_args, 0, 0);
    dr_retp(c, args[2]);
    conversion_routine = (void(*)())dr_end(c);
    if (generation_verbose) {
	dr_dump(c);
    }
    conv->free_data = c;
    conv->free_func = (void(*)(void*))&dr_free_context;
    return (conv_routine) conversion_routine;
}
/* #define REG_DEBUG(x) printf x ; */
#define REG_DEBUG(x)

static void
gen_var_part_conv(drisc_ctx c, IOConversionPtr conv, int i, int control_base,
			int assume_align, dr_reg_t src_addr, dr_reg_t dest_addr,
			dr_reg_t src_string_base, dr_reg_t final_string_base,
			int register_args);


static void
gen_mem_float_conv(drisc_ctx c, struct _IOgetFieldStruct src, int src_addr, 
		   int src_offset, int assume_align,
		   int dest_reg, int dest_offset,
		   int dest_size, int dst_aligned)
{
    FMfloat_format src_format = (FMfloat_format) src.src_float_format;
    FMfloat_format dst_format = (FMfloat_format) src.target_float_format;
    int src_drisc_type = drisc_type(&src);

    if (src_format == dst_format) {
	dr_reg_t tmp;
	switch (src_drisc_type) {
	case DR_D:
	    if (assume_align >= TYPE_ALIGN(c, DR_D)) {
		dr_getreg(c, &tmp, DR_D, DR_TEMP);
		dr_lddi(c, tmp, src_addr, src_offset);
		dr_stdi(c, tmp, dest_reg, dest_offset);
		dr_putreg(c, tmp, DR_D);
		return;
	    }
	    break;
	case DR_F:
	    if (assume_align >= TYPE_ALIGN(c, DR_D)) {
		dr_getreg(c, &tmp, DR_F, DR_TEMP);
		dr_ldfi(c, tmp, src_addr, src_offset);
		dr_stfi(c, tmp, dest_reg, dest_offset);
		dr_putreg(c, tmp, DR_F);
		return;
	    }
	    break;
	}
	gen_memcpy(c, src_addr, src_offset, dest_reg, dest_offset, 0,
		   dest_size);
	return;
    }
    if (src_format == ffs_reverse_float_formats[dst_format]) {
	switch(dest_size) {
	case sizeof(short): {
	    dr_reg_t tmp;
	    dr_getreg(c, &tmp, DR_S, DR_TEMP);
	    dr_ldbssi(c, tmp, src_addr, src_offset);
	    dr_stsi(c, tmp, dest_reg, dest_offset);
	    dr_putreg(c, tmp, DR_S);
	    break;
	}
	case sizeof(int): {
	    dr_reg_t tmp;
	    dr_getreg(c, &tmp, DR_I, DR_TEMP);
	    dr_ldbsii(c, tmp, src_addr, src_offset);
	    dr_stii(c, tmp, dest_reg, dest_offset);
	    dr_putreg(c, tmp, DR_I);
	    break;
	}
#if SIZEOF_LONG == 8
	case sizeof(long): 
	    if (((src_offset & 0x7) == 0) && (assume_align >= sizeof(long))) {
		dr_reg_t tmp;
		dr_getreg(c, &tmp, DR_L, DR_TEMP);
		dr_ldbsli(c, tmp, src_addr, src_offset);
		dr_stli(c, tmp, dest_reg, dest_offset);
		dr_putreg(c, tmp, DR_L);
		break;
	    } else {
		dr_reg_t tmp, tmp2;
		int i;
		dr_getreg(c, &tmp, DR_I, DR_TEMP);
		dr_getreg(c, &tmp2, DR_I, DR_TEMP);
		for (i = 0; i < (dest_size >> 1); i += sizeof(int)) {
		    int near_offset = i*sizeof(int);
		    int far_offset = dest_size - (i+1)*sizeof(int);
		    dr_ldbsii(c, tmp, src_addr, src_offset + near_offset);
		    dr_ldbsii(c, tmp2, src_addr, src_offset + far_offset);
		    dr_stii(c, tmp, dest_reg, dest_offset + far_offset);
		    dr_stii(c, tmp2, dest_reg, dest_offset + near_offset);
		}
		dr_putreg(c, tmp, DR_I);
		dr_putreg(c, tmp2, DR_I);
		break;
	    }
#endif
	default: 
	{
	    dr_reg_t tmp, tmp2;
	    int i;
	    dr_getreg(c, &tmp, DR_L, DR_TEMP);
	    dr_getreg(c, &tmp2, DR_L, DR_TEMP);
	    for (i = 0; i < (dest_size >> 1); i += sizeof(long)) {
		int near_offset = i*sizeof(int);
		int far_offset = dest_size - (i+1)*sizeof(int);
		dr_ldbsli(c, tmp, src_addr, src_offset + near_offset);
		dr_ldbsli(c, tmp2, src_addr, src_offset + far_offset);
		dr_stli(c, tmp, dest_reg, dest_offset + far_offset);
		dr_stli(c, tmp2, dest_reg, dest_offset + near_offset);
	    }
	    dr_putreg(c, tmp, DR_L);
	    dr_putreg(c, tmp2, DR_L);
	}
	}
	return;
    }
    switch (CONV(src_format, dst_format)) {
    case CONV(Format_IEEE_754_bigendian, Format_IEEE_754_mixedendian):
    case CONV(Format_IEEE_754_mixedendian, Format_IEEE_754_bigendian):
	{
	    /* byte swap in place */
	    dr_reg_t tmp;
	    int i;
	    dr_getreg(c, &tmp, DR_I, DR_TEMP);
	    for (i = 0; i < dest_size; i += sizeof(int)) {
		dr_ldbsii(c, tmp, src_addr, src_offset + i);
		dr_stii(c, tmp, dest_reg, dest_offset + i);
	    }
	    dr_putreg(c, tmp, DR_I);
	}
	break;
    case CONV(Format_IEEE_754_littleendian, Format_IEEE_754_mixedendian):
    case CONV(Format_IEEE_754_mixedendian, Format_IEEE_754_littleendian):
	{
	    /* swap words, no byteswapping */
	    dr_reg_t tmp, tmp2;
	    int i;
	    dr_getreg(c, &tmp, DR_I, DR_TEMP);
	    dr_getreg(c, &tmp2, DR_I, DR_TEMP);
	    for (i = 0; i < (dest_size >> 1); i += sizeof(int)) {
		int near_offset = i;
		int far_offset = dest_size - (i+sizeof(int));
		dr_ldii(c, tmp, src_addr, src_offset + near_offset);
		dr_ldii(c, tmp2, src_addr, src_offset + far_offset);
		dr_stii(c, tmp, dest_reg, dest_offset + far_offset);
		dr_stii(c, tmp2, dest_reg, dest_offset + near_offset);
	    }
	    dr_putreg(c, tmp, DR_I);
	    dr_putreg(c, tmp2, DR_I);
	}
	break;
    default:
	printf("unanticipated float conversion \n");
    }
}
	    
static void
gen_simple_field_conv(c, tmp_spec, assume_align, src_addr, src_offset, 
		      dest_size, dest_type, dest_addr, dest_offset,
		      string_offset_size, final_string_base, base_size_delta,
		      converted_strings)
drisc_ctx c;
struct _IOgetFieldStruct tmp_spec;
int assume_align;
dr_reg_t src_addr;
int src_offset;
int dest_size;
FMdata_type dest_type;
dr_reg_t dest_addr;
int dest_offset;
int string_offset_size;
dr_reg_t final_string_base;
int base_size_delta;
int converted_strings;
{
    /* simple conversion */
    iogen_oprnd src_oprnd;
    struct _IOgetFieldStruct dest_spec;
    int src_drisc_type;
    int dst_drisc_type;
    int src_required_align;
    int dst_required_align;
    int src_is_aligned = 1;
    int dst_is_aligned = 1;

    dest_spec.data_type = dest_type;
    dest_spec.size = dest_size;
    src_drisc_type = drisc_type(&tmp_spec);
    dst_drisc_type = drisc_type(&dest_spec);
    src_required_align = TYPE_ALIGN(c, src_drisc_type);
    dst_required_align = TYPE_ALIGN(c, dst_drisc_type);
    if ((assume_align < src_required_align) || 
	((src_offset % src_required_align) != 0)) {
	src_is_aligned = 0;
    }
    if ((assume_align < dst_required_align) || 
	((dest_offset % dst_required_align) != 0)) {
	dst_is_aligned = 0;
    }

    if (tmp_spec.data_type != float_type) {
	assert(dest_type != string_type);

	src_oprnd = gen_operand(src_addr, src_offset, tmp_spec.size,
				tmp_spec.data_type,
				src_is_aligned,
				tmp_spec.byte_swap);

	if (src_oprnd.address) {
	    gen_load(c, &src_oprnd);
	}
	if (src_oprnd.data_type != dest_type) {
	    iogen_oprnd tmp_oprnd;
	    tmp_oprnd = gen_type_conversion(c, src_oprnd, dest_type);
	    free_oprnd(c, src_oprnd);
	    src_oprnd = tmp_oprnd;
	}
	if (src_oprnd.size != dest_size) {
	    iogen_oprnd tmp_oprnd;
	    tmp_oprnd = gen_size_conversion(c, src_oprnd, dest_size);
	    free_oprnd(c, src_oprnd);
	    src_oprnd = tmp_oprnd;
	}
	gen_store(c, src_oprnd, dest_addr, dest_offset,
		  dest_size, dest_type, dst_is_aligned);
	free_oprnd(c, src_oprnd);
    } else {
	if (tmp_spec.size == dest_size) {
	    gen_mem_float_conv(c, tmp_spec, src_addr, src_offset, assume_align,
			       dest_addr, dest_offset, dest_size, 
			       dst_is_aligned);
	    return;
	}
	if ((dst_drisc_type != DR_I) && (src_drisc_type != DR_I)) {
	    /* both float sizes supported */
	    int float_conv_offset = dr_local(c, dst_drisc_type);
	    dr_reg_t float_reg;
	    gen_mem_float_conv(c, tmp_spec, src_addr, src_offset, assume_align,
			       dr_lp(c), float_conv_offset, dest_size, 1);
	    dr_getreg(c, &float_reg, DR_D, DR_TEMP);
	    switch(dst_drisc_type) {
	    case DR_D:
		dr_lddi(c, float_reg, dr_lp(c), float_conv_offset);
		dr_cvd2f(c, float_reg, float_reg);
		dr_stfi(c, float_reg, dest_addr, dest_offset);
		break;
	    case DR_F:
		dr_ldfi(c, float_reg, dr_lp(c), float_conv_offset);
		dr_cvf2d(c, float_reg, float_reg);
		dr_stdi(c, float_reg, dest_addr, dest_offset);
		break;	    
	    }
	    dr_putreg(c, float_reg, DR_D);
	    return;
	}
	printf("must do call to conversion subroutine\n");
    }
}

static void
gen_addr_field_conv(c, tmp_spec, assume_align, src_addr, src_offset,
		    dest_size, dest_addr, dest_offset, string_offset_size,
  final_string_base, src_string_base, base_size_delta, converted_strings,
		    string_src_reg, string_dest_reg, register_args)
drisc_ctx c;
struct _IOgetFieldStruct tmp_spec;
int assume_align;
dr_reg_t src_addr;
int src_offset;
int dest_size;
dr_reg_t dest_addr;
int dest_offset;
int string_offset_size;
dr_reg_t final_string_base;
dr_reg_t src_string_base;
int base_size_delta;
int converted_strings;
dr_reg_t *string_src_reg;
dr_reg_t *string_dest_reg;
int register_args;
{
    iogen_oprnd src_oprnd;
    int src_drisc_type;
    int src_required_align;
    int src_is_aligned = 1;
    int null_target = dr_genlabel(c);


    src_drisc_type = drisc_type(&tmp_spec);
    src_required_align = TYPE_ALIGN(c, src_drisc_type);
    if ((assume_align < src_required_align) || 
	((src_offset % src_required_align) != 0)) {
	src_is_aligned = 0;
    }


    /* handle addresses */
    src_oprnd = gen_fetch(c, src_addr, src_offset, tmp_spec.size,
			  integer_type, src_is_aligned,
			  tmp_spec.byte_swap);

    /* src_oprnd now holds the offset value */
    if (dest_size >= sizeof(char *)) {

	if (src_oprnd.size != dest_size) {
	    /* make it the right size to operate on */
	    iogen_oprnd tmp_oprnd;
	    tmp_oprnd = gen_size_conversion(c, src_oprnd, sizeof(long));
	    free_oprnd(c, src_oprnd);
	    src_oprnd = tmp_oprnd;
	}
	/* generate : if it's zero, leave it zero  branch away */
	dr_beqli(c, src_oprnd.vc_reg, 0, null_target);

	/* else, sub the string_offset_size */
	dr_subli(c, src_oprnd.vc_reg, src_oprnd.vc_reg,
		string_offset_size);
 
	/* Moving to here to more effiecntly use registers */   
	if (!dr_getreg(c, string_src_reg, DR_P, DR_VAR))
	  gen_fatal("gen field convert out of registers C\n");
	if (!dr_getreg(c, string_dest_reg, DR_P, DR_VAR))
	  gen_fatal("gen field convert out of registers D\n");
	REG_DEBUG(("Getting reg %d for string src reg\n", *string_src_reg));
	REG_DEBUG(("Getting reg %d for string dest reg\n", *string_dest_reg));

	/* calculate the address of this in the source */
	if (register_args) {
	    dr_addl(c, *string_src_reg, src_oprnd.vc_reg, src_string_base);
	} else {
	    dr_ldpi(c, *string_src_reg, dr_lp(c), src_string_base);
	    dr_addl(c, *string_src_reg, src_oprnd.vc_reg, *string_src_reg);
	}
	    
	/* and the address in the destination */
	if (register_args) {
	    dr_addl(c, src_oprnd.vc_reg, src_oprnd.vc_reg, final_string_base);
	} else {
	    dr_ldpi(c, *string_dest_reg, dr_lp(c), final_string_base);
	    dr_addl(c, src_oprnd.vc_reg, src_oprnd.vc_reg, *string_dest_reg);
	}
	dr_label(c, null_target);

	dr_movp(c, *string_dest_reg, src_oprnd.vc_reg);

	if (dest_size > sizeof(char *)) {
	    iogen_oprnd tmp_oprnd;
	    tmp_oprnd = gen_size_conversion(c, src_oprnd,
					    dest_size);
	    free_oprnd(c, src_oprnd);
	    src_oprnd = tmp_oprnd;
	}
	REG_DEBUG(("Regs %d and %d are src and dest \n",
		   _vrr(*string_src_reg), _vrr(*string_dest_reg)));
	free_oprnd(c, src_oprnd);
    }
}

static void
gen_subfield_conv(c, conv, i, tmp_spec, dest_type, assume_align, src_addr,
	     src_offset, dest_size, dest_addr, dest_offset, control_base,
		  final_string_base, src_string_base, register_args)
drisc_ctx c;
IOConversionPtr conv;
int i;
struct _IOgetFieldStruct tmp_spec;
FMdata_type dest_type;
int assume_align;
dr_reg_t src_addr;
int src_offset;
int dest_size;
dr_reg_t dest_addr;
int dest_offset;
int control_base;
dr_reg_t final_string_base;
dr_reg_t src_string_base;
int register_args;
{
    int elements = get_static_array_element_count(conv->conversions[i].iovar);
    if (dest_type != unknown_type) {
	/* simple (native) field or variant array */
	if (dest_type != string_type) {
	    gen_simple_field_conv(c, tmp_spec, assume_align,
				  src_addr, src_offset,
				  dest_size, dest_type,
				  dest_addr, dest_offset,
				  conv->string_offset_size,
				  final_string_base,
				  conv->base_size_delta,
				  conv->converted_strings);
	} else {
	    dr_reg_t string_src_reg;
	    dr_reg_t string_dest_reg;
	    gen_addr_field_conv(c, tmp_spec, assume_align,
				src_addr, src_offset,
				dest_size, dest_addr,
				dest_offset,
				conv->string_offset_size,
				final_string_base,
				src_string_base,
				conv->base_size_delta,
				conv->converted_strings,
				&string_src_reg,
				&string_dest_reg,
				register_args);
	    if ((conv->conversion_type == copy_dynamic_portion) ||
		(elements == -1)) {
		if (register_args) {
		    /* many register case */
		    gen_var_part_conv(c, conv, i, control_base,
				      assume_align,
				      string_dest_reg,
				      string_src_reg,
				      src_string_base,
				      final_string_base,
				      register_args);
		    /* store final string value */
		    dr_stpi(c, string_dest_reg, dest_addr, dest_offset);

		    REG_DEBUG(("Putting reg %d for string src reg\n", string_src_reg));
		    REG_DEBUG(("Putting reg %d for string dest reg\n", string_dest_reg));
		    dr_putreg(c, string_src_reg, DR_P);
		    dr_putreg(c, string_dest_reg, DR_P);
		} else {
		    /*         limited register case  :
		     * save old src and dest values so we can reuse those
		     * registers for the conversion of the variable part
		     */
		    int src_storage = dr_local(c, DR_P);
		    int dest_storage = dr_local(c, DR_P);
		    dr_stpi(c, src_addr, dr_lp(c), src_storage);
		    dr_stpi(c, dest_addr, dr_lp(c), dest_storage);
		    dr_movp(c, src_addr, string_src_reg);
		    dr_movp(c, dest_addr, string_dest_reg);
		    dr_putreg(c, string_src_reg, DR_P);
		    dr_putreg(c, string_dest_reg, DR_P);
		    REG_DEBUG(("Putting reg %d for string src reg\n", string_src_reg));
		    REG_DEBUG(("Putting reg %d for string dest reg\n", string_src_reg));
		    gen_var_part_conv(c, conv, i, control_base,
				      assume_align,
				      dest_addr,
				      src_addr,
				      src_string_base,
				      final_string_base,
				      register_args);

		    /* save the final string address in the src reg */
		    dr_movp(c, src_addr, dest_addr);

		    /* restore the original dest addr */
		    dr_ldpi(c, dest_addr, dr_lp(c), dest_storage);

		    /* store final string value */
		    dr_stpi(c, src_addr, dest_addr, dest_offset);

		    /* restore value of src_addr */
		    dr_ldpi(c, src_addr, dr_lp(c), src_storage);
		}
	    } else {
		REG_DEBUG(("Putting reg %d for string src reg\n", string_src_reg));
		REG_DEBUG(("Putting reg %d for string dest reg\n", string_dest_reg));
		/* store final string value */
		dr_stpi(c, string_dest_reg, dest_addr, dest_offset);
		dr_putreg(c, string_src_reg, DR_P);
		dr_putreg(c, string_dest_reg, DR_P);
	    }
	}
    } else {
	/* subconversion */
	IOConversionPtr subconv =
	    conv->conversions[i].subconversion;
	if ((subconv->required_alignment == 0) ||
	    ((assume_align >= 8) &&
	     ((src_offset % subconv->required_alignment) == 0))) {
	    if (register_args) {
		/* many register case */
		dr_reg_t new_src, new_dest, ret;
		if (!dr_getreg(c, &new_src, DR_P, DR_TEMP) ||
		    !dr_getreg(c, &new_dest, DR_P, DR_TEMP))
		    gen_fatal("temp vals in subcall\n");
		REG_DEBUG(("Getting %d and %d for new src & dest\n", 
			   new_src, new_dest));
		dr_addpi(c, new_src, src_addr, src_offset);
		dr_addpi(c, new_dest, dest_addr, dest_offset);
		ret = VCALL4P(subconv->conv_func, "%p%p%p%p", new_src,
			 new_dest, final_string_base, src_string_base);
		dr_movp(c, final_string_base, ret);
		REG_DEBUG(("Putting %d and %d for new src & dest\n", 
			   new_src, new_dest));
		if (debug_code_generation) {
		    VCALL2V(printf, "%P%p",
			    "After subroutine call, new src_string_base is %lx\n", src_string_base);
		}
		dr_putreg(c, new_src, DR_P);
		dr_putreg(c, new_dest, DR_P);
	    } else {
		int src_storage = dr_local(c, DR_P);
		int dest_storage = dr_local(c, DR_P);
		dr_reg_t reg_src_string_base, reg_final_string_base;

		/* save values of src_addr and dest_addr */
		dr_stpi(c, src_addr, dr_lp(c), src_storage);
		dr_stpi(c, dest_addr, dr_lp(c), dest_storage);

		if (!dr_getreg(c, &reg_src_string_base, DR_P, DR_TEMP) ||
		    !dr_getreg(c, &reg_final_string_base, DR_P, DR_TEMP))
		    gen_fatal("temp string vals in subcall\n");
		REG_DEBUG(("Getting %d and %d for reg src base & reg dest base\n", reg_src_string_base, reg_final_string_base));
		dr_addpi(c, src_addr, src_addr, src_offset);
		dr_addpi(c, dest_addr, dest_addr, dest_offset);
		dr_ldpi(c, reg_final_string_base, dr_lp(c), final_string_base);
		dr_ldpi(c, reg_src_string_base, dr_lp(c), src_string_base);
		VCALL4V(subconv->conv_func, "%p%p%p%p", src_addr,
			 dest_addr, reg_final_string_base, reg_src_string_base);
		REG_DEBUG(("Putting %d and %d for reg src base & reg dest base\n", reg_src_string_base, reg_final_string_base));
		dr_putreg(c, reg_src_string_base, DR_P);
		dr_putreg(c, reg_final_string_base, DR_P);
		/* restore values of src_addr and dest_addr */
		dr_ldpi(c, src_addr, dr_lp(c), src_storage);
		dr_ldpi(c, dest_addr, dr_lp(c), dest_storage);
	    }
	} else {
	    /* misaligned substructure, can't call subconversion */
	    dr_reg_t args[4];
	    args[0] = src_addr;
	    args[1] = dest_addr;
	    args[2] = final_string_base;
	    args[3] = src_string_base;
	    generate_conversion_code(c, subconv, args, assume_align, 
				     register_args, src_offset, dest_offset);
	}
    }
}

extern
 conv_routine
generate_conversion_code(c, conv, args, assume_align, register_args, 
			 extra_src_offset, extra_dest_offset)
drisc_ctx c;
IOConversionPtr conv;
dr_reg_t *args;
int assume_align;
int register_args;
int extra_src_offset;
int extra_dest_offset;
{
    int i;
    dr_reg_t src_addr = args[0];
    dr_reg_t dest_addr = args[1];
    dr_reg_t final_string_base = args[2];
    dr_reg_t src_string_base = args[3];
    int control_base = -1;

    for (i = 0; i < conv->conv_count; i++) {
	int elements = 
	    get_static_array_element_count(conv->conversions[i].iovar);
	if (elements == -1) {
	    iogen_oprnd count_oprnd;
	    int count_storage;
	    int j;
	    int first_assign = 1;
	    int dimen_count = conv->conversions[i].iovar->dimen_count;
	    FMDimen *dimens = conv->conversions[i].iovar->dimens;
	    if (control_base == -1) {
		control_base = dr_localb(c, sizeof(int) * conv->conv_count);
	    }
	    for (j=0; j< dimen_count; j++) {
		struct _IOgetFieldStruct control_field;
		int field = dimens[j].control_field_index;
		iogen_oprnd src_oprnd;
		if (dimens[j].static_size == 0) {
		    int src_is_aligned = 1;
		    int src_drisc_type;
		    int src_required_align;
		    FMFormat f = conv->ioformat->body;
		    memset(&control_field, 0, sizeof(control_field));
		    control_field.size = f->field_list[field].field_size;
		    control_field.offset = f->field_list[field].field_offset;
		    control_field.data_type = integer_type;
		    src_drisc_type = drisc_type(&control_field);
		    src_required_align = TYPE_ALIGN(c, src_drisc_type);
		    if ((assume_align < src_required_align) || 
			(((control_field.offset +extra_src_offset) % src_required_align) == 0)) {
			src_is_aligned = 0;
		    }
		    if ((first_assign == 0) && (register_args== 0)) {
			/* 
			 *    we've got something in a register, 
			 *    *and* we're tight on registers 
			 */
			dr_stii(c, count_oprnd.vc_reg, dr_lp(c), count_storage);
			REG_DEBUG(("Putting %d as count\n", 
				   _vrr(count_oprnd.vc_reg)));
			dr_putreg(c, count_oprnd.vc_reg, DR_I);
		    }
			
		    src_oprnd = gen_fetch(c, src_addr, 
					  control_field.offset + extra_src_offset,
					  control_field.size,
					  control_field.data_type,
					  src_is_aligned, control_field.byte_swap);
		    if (src_oprnd.size != sizeof(int)) {
			iogen_oprnd tmp_oprnd;
			tmp_oprnd = gen_size_conversion(c, src_oprnd, sizeof(int));
			free_oprnd(c, src_oprnd);
			src_oprnd = tmp_oprnd;
		    }
		    if ((first_assign == 0) && (register_args == 0)) {
			/* 
			 *    we've got something in a register, 
			 *    *and* we're tight on registers 
			 */
			dr_getreg(c, &count_oprnd.vc_reg, DR_I, DR_TEMP);
			REG_DEBUG(("Getting %d as count\n", _vrr(count_oprnd.vc_reg)));
			dr_ldii(c, count_oprnd.vc_reg, dr_lp(c), count_storage);
		    }
		} else {
		    src_oprnd.address = 0;
		    src_oprnd.data_type = integer_type;
		    src_oprnd.size = sizeof(int);
		    src_oprnd.offset = 0;
		    src_oprnd.aligned = 0;
		    src_oprnd.byte_swap = 0;
		    if (!dr_getreg(c, &src_oprnd.vc_reg, DR_I, DR_TEMP))
			gen_fatal("gen const out of registers\n");
		    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(src_oprnd.vc_reg)));
		    dr_seti(c, src_oprnd.vc_reg, dimens[j].static_size);
		}
		if (first_assign) {
		    count_storage = dr_local(c, DR_I);
		    count_oprnd = src_oprnd;
		    first_assign = 0;
		} else {
		    dr_muli(c, count_oprnd.vc_reg, count_oprnd.vc_reg, 
			    src_oprnd.vc_reg);
		    free_oprnd(c, src_oprnd);
		}
	    }
	    gen_store(c, count_oprnd, dr_lp(c), control_base + i * sizeof(int),
		      sizeof(int), integer_type, TRUE /* aligned */ );
	    free_oprnd(c, count_oprnd);
	}
    }
    for (i = 0; i < conv->conv_count; i++) {
	IOFieldPtr src_spec = &conv->conversions[i].src_field;
	int byte_swap = conv->conversions[i].src_field.byte_swap;
	int elements = 
	    get_static_array_element_count(conv->conversions[i].iovar);
	if (conv->conversions[i].src_field.size == 1) byte_swap = 0;
	if (conv->conversions[i].default_value) {
	    iogen_oprnd src_oprnd;
	    int dst_is_aligned = (assume_align >= sizeof(long)) &
		(((conv->conversions[i].dest_offset + extra_dest_offset) % 8) == 0);
	    src_oprnd = gen_set(c, conv->conversions[i].dest_size, 
				conv->conversions[i].default_value);
	    gen_store(c, src_oprnd, dest_addr,
		      conv->conversions[i].dest_offset+extra_dest_offset,
		      conv->conversions[i].dest_size,
		      src_oprnd.data_type, dst_is_aligned);
	    free_oprnd(c, src_oprnd);
	} else if (!byte_swap &&
		   (src_spec->src_float_format == src_spec->target_float_format) &&
		   (src_spec->size == conv->conversions[i].dest_size) &&
		   (conv->conversions[i].subconversion == NULL) &&
		   (elements != -1) &&
		   (conv->conversions[i].rc_swap == no_row_column_swap) &&
		   ((src_spec->data_type != string_type))) {
	    /* data movement is all that is required */
	    int total_size = src_spec->size * elements;

	    if (total_size <= sizeof(long)) {
		iogen_oprnd src_oprnd;
		int src_is_aligned = (assume_align >= sizeof(long)) &
		    (((src_spec->offset +extra_src_offset) % 8) == 0);
		int dst_is_aligned = (assume_align >= sizeof(long)) &
		    (((conv->conversions[i].dest_offset + extra_dest_offset) % 8) == 0);
		src_oprnd = gen_fetch(c, src_addr, 
				      src_spec->offset + extra_src_offset,
				      total_size, src_spec->data_type,
				      src_is_aligned, 0);
		gen_store(c, src_oprnd, dest_addr,
			  conv->conversions[i].dest_offset+extra_dest_offset,
			  conv->conversions[i].dest_size,
			  src_spec->data_type, dst_is_aligned);
		free_oprnd(c, src_oprnd);
	    } else {
		gen_memcpy(c, src_addr, src_spec->offset + extra_src_offset, 
			   dest_addr,
			   conv->conversions[i].dest_offset + extra_dest_offset,
			   0, total_size);
	    }
	} else if ((conv->conversions[i].rc_swap == no_row_column_swap) ||
		   (elements == -1) /* var array */ ){
	    dr_reg_t loop_var;
	    int loop;

	    int dest_offset = conv->conversions[i].dest_offset + extra_dest_offset;
	    int src_offset = src_spec->offset + extra_src_offset;
	    FMdata_type dest_type = src_spec->data_type;
	    int dest_size = conv->conversions[i].dest_size;
	    struct _IOgetFieldStruct tmp_spec;  /* OK */
	    tmp_spec = *src_spec;
	    tmp_spec.offset = 0;

	    if (elements == -1) {
		/* really a variant array, adjust address like a string */
		tmp_spec.data_type = string_type;
		tmp_spec.size = conv->ioformat->body->pointer_size;
		dest_type = string_type;
		dest_size = conv->target_pointer_size;
		elements = 1;
	    }
	    if (elements == 1) {   /* GSE fix this */
		gen_subfield_conv(c, conv, i, tmp_spec, dest_type, 
				  assume_align, src_addr, src_offset,
				  dest_size, dest_addr, dest_offset,
				  control_base,
				  final_string_base, src_string_base,
				  register_args);
	    } else {
		int src_storage = dr_local(c, DR_P);
		int dest_storage = dr_local(c, DR_P);
		int loop_storage = 0;
		int src_align_offset = src_offset %
		    subfield_required_align(c, conv, i, 0);
		int loop_var_type;

		/* save values of src_addr and dest_addr */
		dr_stpi(c, src_addr, dr_lp(c), src_storage);
		dr_stpi(c, dest_addr, dr_lp(c), dest_storage);

		if (((elements == -1) ||
		    (conv->conversions[i].subconversion == NULL)) &&
		    !debug_code_generation) {
		    if (!dr_getreg(c, &loop_var, DR_I, DR_TEMP))
			gen_fatal("gen field convert out of registers BB \n");
		    loop_var_type = DR_TEMP;
		} else {
		    /* may call a subconversion in here, use VARs */
		    if (!dr_getreg(c, &loop_var, DR_I, DR_VAR))
			gen_fatal("gen field convert out of registers CC\n");
		    loop_var_type = DR_VAR;
		}
		REG_DEBUG(("Getting %d as loop_var\n", _vrr(loop_var)));
		dr_addpi(c, src_addr, src_addr, src_offset - src_align_offset);
		dr_addpi(c, dest_addr, dest_addr, dest_offset);

		if (elements == -1) {
		    /* 
		     * really a variant array, adjust address like a string 
		     */
		    tmp_spec.data_type = string_type;
		    tmp_spec.size = conv->ioformat->body->pointer_size;
		}
		/* gen conversion loop */
		loop = dr_genlabel(c);
		dr_seti(c, loop_var, elements);
		dr_label(c, loop);
		if (!register_args) {
		    /* store away loop var and free the reg */
		    loop_storage = dr_local(c, DR_I);
		    dr_stii(c, loop_var, dr_lp(c), loop_storage);
		    REG_DEBUG(("Putting %d as loop_var\n", _vrr(loop_var)));
		    dr_putreg(c, loop_var, DR_I);
		}
		gen_subfield_conv(c, conv, i, tmp_spec, dest_type, assume_align,
				  src_addr, src_align_offset,
				  dest_size, dest_addr, 0,
				  control_base,
				  final_string_base, src_string_base,
				  register_args);

		/* generate end of loop */
		if (!register_args) {
		    /* store away loop var and free the reg */
		    dr_getreg(c, &loop_var, DR_I, loop_var_type);
		    REG_DEBUG(("Getting %d as loop_var\n", _vrr(loop_var)));
		    dr_ldii(c, loop_var, dr_lp(c), loop_storage);
		}
		dr_subli(c, loop_var, loop_var, 1);
		dr_addpi(c, src_addr, src_addr, tmp_spec.size);
		dr_addpi(c, dest_addr, dest_addr,
			conv->conversions[i].dest_size);
		if (debug_code_generation) {
		    VCALL4V(printf, "%P%p%p%p",
			     "loopvar = %x, src %x, dest %x\n", loop_var,
			     src_addr, dest_addr);
		}
		dr_bgtli(c, loop_var, 0, loop);
		dr_putreg(c, loop_var, DR_I);
		REG_DEBUG(("Putting %d as loop_var\n", _vrr(loop_var)));

		/* restore values of src_addr and dest_addr */
		dr_ldpi(c, src_addr, dr_lp(c), src_storage);
		dr_ldpi(c, dest_addr, dr_lp(c), dest_storage);
	    }
	} else {   
	    /* 
	     * the only remaining possibility is that this is a
	     * multi-dimensional array that requires a transpose as well as
	     * some possible conversion
	     */
	    int dimen_count = conv->conversions[i].iovar->dimen_count;
	    int src_storage = dr_local(c, DR_P);
	    int dest_storage = dr_local(c, DR_P);
	    
	    /* save values of src_addr and dest_addr */
	    dr_stpi(c, src_addr, dr_lp(c), src_storage);
	    dr_stpi(c, dest_addr, dr_lp(c), dest_storage);
	    
	    
/*	    generate_transpose(
	    VCALL1V(transpose, );*/
	    /* restore values of src_addr and dest_addr */
	    dr_ldpi(c, src_addr, dr_lp(c), src_storage);
	    dr_ldpi(c, dest_addr, dr_lp(c), dest_storage);
	    
	}
    }
    return NULL;
}

static void
gen_var_part_conv(c, conv, i, control_base, assume_align, dest_addr,
		  src_addr, src_string_base, final_string_base, 
		  register_args)
drisc_ctx c;
IOConversionPtr conv;
int i;
int control_base;
int assume_align;
dr_reg_t src_addr, dest_addr;
dr_reg_t src_string_base, final_string_base;
int register_args;
{
    const char *field_name;
    int src_offset = conv->conversions[i].src_field.offset;
    int j = 0;
    /* 
     * This section handles converting or moving the body 
     * of a dynamic array or string (the actual pointer was handled 
     * above)
     */

    int elements = 
	get_static_array_element_count(conv->conversions[i].iovar);
    while (conv->ioformat->body->field_list[j].field_offset != src_offset)
	j++;
    field_name = conv->ioformat->body->field_list[j].field_name;
/** handle copying or conversion */
    if (debug_code_generation) {
	if (register_args) {
	    VCALL7V(printf, "%P%P%I%p%p%p%p",
		     "varconvpart conversion \"%s\" %d src %lx, dest %lx, src_string_base %lx, final_string_base %lx\n",
		     field_name,
		     i, src_addr, dest_addr, 
		     src_string_base, final_string_base);
	} else {
#ifdef HAVE_DRISC_H	    
	    dr_reg_t v_at;
	    if (dr_getreg(c, &v_at, DR_I, DR_TEMP) == 0) {
		gen_fatal("Out of regs 1\n");
	    }
#endif
	    VCALL5V(printf, "%P%P%I%p%p",
		     "varconvpart conversion \"%s\" %d src %lx, dest %lx\n",
		     field_name,
		     i, src_addr, dest_addr);
	    dr_ldpi(c, v_at, dr_lp(c), src_string_base);
	    VCALL2V(printf, "%P%p",
		     "                       src_string_base %lx\n",
		     v_at);
	    dr_ldpi(c, v_at, dr_lp(c), final_string_base);
	    VCALL2V(printf, "%P%p",
		     "                       final_string_base %lx\n",
		     v_at);
#ifdef HAVE_DRISC_H	    
	    dr_putreg(c, v_at, DR_I);
#endif
	}
    }
    if (elements != -1) {
	/* generate a call to strcpy to do the move */
	int end = dr_genlabel(c);
	dr_beqpi(c, dest_addr, 0, end);
	VCALL2V(strcpy, "%p%p", dest_addr, src_addr);
	dr_label(c, end);
	return;
    }
    {
	IOFieldPtr src_spec = &conv->conversions[i].src_field;
	/* variant array */
	int byte_swap = conv->conversions[i].src_field.byte_swap;
	int required_alignment = 1;
	if (conv->conversions[i].src_field.size == 1) byte_swap = 0;
	if (conv->conversions[i].subconversion != NULL) {
	    required_alignment = 
		conv->conversions[i].subconversion->required_alignment;
	} else {
	    /* overassumption */
	    required_alignment = conv->conversions[i].dest_size;
	}
	if (required_alignment > 1) {
#ifdef HAVE_DRISC_H	    
	    dr_reg_t tmp;
	    if (dr_getreg(c, &tmp, DR_I, DR_TEMP) == 0) {
		gen_fatal("Out of regs 1\n");
	    }
#endif
	    dr_negl(c, tmp, dest_addr);
	    dr_andli(c, tmp, tmp, required_alignment -1);
	    if (register_args) {
		dr_addl(c, final_string_base, final_string_base, tmp);
	    } else {
#ifdef HAVE_DRISC_H	    
		dr_reg_t v_at;
		if (dr_getreg(c, &v_at, DR_I, DR_TEMP) == 0) {
		    gen_fatal("Out of regs 1\n");
		}
#endif
		dr_ldpi(c, v_at, dr_lp(c), final_string_base);
		if (debug_code_generation) {
		    dr_savel(c, tmp);
		    dr_savel(c, v_at);
		    VCALL2V(printf, "%P%p",
			    "before adjustment    final_string_base %lx\n",
			    v_at);
		    dr_restorel(c, v_at);
		    dr_restorel(c, tmp);
		}
		dr_ldpi(c, v_at, dr_lp(c), final_string_base);
		dr_addl(c, v_at, v_at, tmp);
		dr_stpi(c, v_at, dr_lp(c), final_string_base);
		dr_ldpi(c, v_at, dr_lp(c), final_string_base);
		dr_savel(c, tmp);
		if (debug_code_generation) {
		    VCALL2V(printf, "%P%p",
			    "after adjustment    final_string_base %lx\n",
			    v_at);
		}
		dr_restorel(c, tmp);
#ifdef HAVE_DRISC_H	    
		dr_putreg(c, v_at, DR_I);
#endif
	    }
	    dr_addp(c, dest_addr, dest_addr, tmp);
	    if (debug_code_generation) {
		VCALL3V(printf, "%P%p%p", "	after dynarray alignment adjustment, dest_addr = %lx, final_string base = %lx\n", dest_addr, final_string_base);
	    }
#ifdef HAVE_DRISC_H	    
	    dr_putreg(c, tmp, DR_I);
#endif
	}

	if (!byte_swap &&
	    (src_spec->size == conv->conversions[i].dest_size) &&
	    (conv->conversions[i].subconversion == NULL) &&
	    (src_spec->data_type != string_type) &&
	    ((src_spec->data_type != float_type) || 
		(src_spec->src_float_format == src_spec->target_float_format))) {

	    /* data movement is all that is required */
	    if (conv->conversion_type == copy_dynamic_portion) {
		dr_reg_t size_reg;
		if (!dr_getreg(c, &size_reg, DR_I, DR_TEMP))
		    gen_fatal("gen var convert size out of registers BB \n");
		REG_DEBUG(("Getting %d as size_reg\n", _vrr(size_reg)));
		/* load control (array size) value */
		dr_ldii(c, size_reg, dr_lp(c), control_base + i * sizeof(int));
		/* scale it by destination size */
		dr_mulii(c, size_reg, (dr_reg_t)size_reg,
			conv->conversions[i].dest_size);
		/* generate a memcpy */
		gen_memcpy(c, src_addr, 0, dest_addr, 0, size_reg, 0);
		dr_putreg(c, size_reg, DR_I);
		REG_DEBUG(("Putting %d as size_reg\n", _vrr(size_reg)));
	    } else {
		/* 
		 * not even that is necessary, variant part is
		 * already where it needs to go 
		 */
	    }
	} else {
	    /* must do conversions one by one */
	    int src_storage = dr_local(c, DR_P);
	    int dest_storage = dr_local(c, DR_P);
	    int local_loop_storage = dr_local(c, DR_I);
	    int local_src_storage = dr_local(c, DR_P);
	    int local_dest_storage = dr_local(c, DR_P);
	    dr_reg_t loop_var;
	    int dest_size = conv->conversions[i].dest_size;
	    int loop = dr_genlabel(c);
	    int end = dr_genlabel(c);
	    FMdata_type dest_type = src_spec->data_type;

	    /* save values of src_addr and dest_addr */
	    dr_stpi(c, src_addr, dr_lp(c), src_storage);
	    dr_stpi(c, dest_addr, dr_lp(c), dest_storage);

	    if (((elements == -1) ||
		 (conv->conversions[i].subconversion == NULL)) &&
		!debug_code_generation) {
		if (!dr_getreg(c, &loop_var, DR_I, DR_VAR))
		    gen_fatal("gen var convert loop out of registers CC\n");
	    } else {
		/* may call a subconversion in here, use VARs */
		if (!dr_getreg(c, &loop_var, DR_I, DR_VAR))
		    gen_fatal("gen var convert loop out of registers DD\n");
	    }
	    REG_DEBUG(("Getting %d as var_loop_var\n", _vrr(loop_var)));
	    dr_ldii(c, loop_var, dr_lp(c), control_base + i * sizeof(int));

	    if ((dest_size - src_spec->size) != 0) {
		dr_reg_t tmp;
		if (!dr_getreg(c, &tmp, DR_I, DR_TEMP)) {
		    gen_fatal("in var loop EE");
		}
		REG_DEBUG(("Getting %d as tmp str adjust\n", tmp));
		dr_mulii(c, tmp, loop_var, (dest_size - src_spec->size));

		/* adjust string base !!!!  non-local variation */
		/* this is because new variant part may change size */
		if (register_args) {
		    dr_addl(c, final_string_base, final_string_base, tmp);
		} else {
#ifdef HAVE_DRISC_H	    
		    dr_reg_t v_at;
		    if (dr_getreg(c, &v_at, DR_I, DR_TEMP) == 0) {
			gen_fatal("Out of regs 1\n");
		    }
#endif
		    dr_ldpi(c, v_at, dr_lp(c), final_string_base);
		    if (debug_code_generation) {
			dr_savel(c, tmp);
			dr_savel(c, v_at);
			VCALL2V(printf, "%P%p",
				 "before adjustment    final_string_base %lx\n",
				 v_at);
			dr_restorel(c, v_at);
			dr_restorel(c, tmp);
		    }
		    dr_ldpi(c, v_at, dr_lp(c), final_string_base);
		    dr_addl(c, v_at, v_at, tmp);
		    dr_stpi(c, v_at, dr_lp(c), final_string_base);
		    dr_ldpi(c, v_at, dr_lp(c), final_string_base);
		    if (debug_code_generation) {
			VCALL2V(printf, "%P%p",
				 "after adjustment    final_string_base %lx\n",
				 v_at);
		    }
#ifdef HAVE_DRISC_H	    
		    dr_putreg(c, v_at, DR_I);
#endif
		}
		dr_putreg(c, tmp, DR_I);
		REG_DEBUG(("Putting %d as tmp str adjust\n", tmp));
	    }
	    dr_label(c, loop);
	    dr_beqli(c, loop_var, 0, end);
	    if (debug_code_generation) {
		VCALL5V(printf, "%P%P%i%p%p",
			"top varloopvar = %s[%x], src %lx, dest %lx\n",
			field_name, loop_var, src_addr, dest_addr);
	    }
	    if (!register_args) {
		/* store away loop var and free the reg */
		dr_stii(c, loop_var, dr_lp(c), local_loop_storage);
		dr_putreg(c, loop_var, DR_I);
		REG_DEBUG(("Putting %d as loop_var\n", loop_var));
	    } else {
		dr_savep(c, loop_var);
	    }
	    dr_stpi(c, src_addr, dr_lp(c), local_src_storage);
	    dr_stpi(c, dest_addr, dr_lp(c), local_dest_storage);

	    if (dest_type != string_type) {
		int subelement_align = 1;
		int src_structure_size = conv->conversions[i].src_field.size;
		if ((src_structure_size & 0x1) == 0) subelement_align = 2;
		if ((src_structure_size & 0x3) == 0) subelement_align = 4;
		if ((src_structure_size & 0x7) == 0) subelement_align = 8;
		if ((src_structure_size & 0xf) == 0) subelement_align = 16;
		gen_subfield_conv(c, conv, i, *src_spec, dest_type,
				  subelement_align, src_addr, 0, dest_size,
				  dest_addr, 0,
				  control_base, final_string_base,
				  src_string_base, register_args);
	    } else {
		IOConversionStruct tmp_conv = *conv;
		tmp_conv.conv_count = 0;
		tmp_conv.conversions[0] = conv->conversions[i];
		tmp_conv.conversions[0].iovar = NULL;
		gen_subfield_conv(c, &tmp_conv, 0, *src_spec, dest_type,
				  assume_align, src_addr, 0, dest_size,
				  dest_addr, 0,
				  control_base, final_string_base,
				  src_string_base, register_args);
	    }
	    dr_ldpi(c, dest_addr, dr_lp(c), local_dest_storage);
	    dr_ldpi(c, src_addr, dr_lp(c), local_src_storage);
	    if (!register_args) {
		/* store away loop var and free the reg */
		dr_getreg(c, &loop_var, DR_I, DR_VAR);
		REG_DEBUG(("Getting %d as lop_var FF\n", loop_var));
		dr_ldii(c, loop_var, dr_lp(c), local_loop_storage);
	    } else {
		dr_restorep(c, loop_var);
	    }
	    if (debug_code_generation) {
		VCALL5V(printf, "%P%P%i%p%p",
			"bottom varloopvar = %s[%x], src %lx, dest %lx\n",
			field_name, loop_var, src_addr, dest_addr);
	    }
	    dr_subli(c, loop_var, loop_var, 1);
	    dr_addpi(c, src_addr, src_addr, src_spec->size);
	    dr_addpi(c, dest_addr, dest_addr,
		    conv->conversions[i].dest_size);
	    dr_jv(c, loop);
	    dr_putreg(c, loop_var, DR_I);
	    REG_DEBUG(("Putting %d as lop_var\n", loop_var));
	    dr_label(c, end);
	    /* restore values of src_addr and dest_addr */
	    dr_ldpi(c, src_addr, dr_lp(c), src_storage);
	    dr_ldpi(c, dest_addr, dr_lp(c), dest_storage);
	}
    }
}

#endif
