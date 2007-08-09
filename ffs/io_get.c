
#include "config.h"

#ifndef MODULE
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
#include <ctype.h>
#include <sys/types.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#include "unix_defs.h"
#else
/* kernel build */
#include "kernel/pbio_kernel.h"
#include "kernel/kpbio.h"
#include "kernel/library.h"
#endif

#include "ffs.h"
#include "ffs_internal.h"
#include "assert.h"

#ifndef tolower
extern int tolower ARGS((int));
#endif

static long get_offset ARGS((void *, int, int));
extern void *io_malloc ARGS((int));


extern float
get_IOfloat(iofield, data)
IOFieldPtr iofield;
void *data;
{
    float tmp;
    ffs_internal_convert_field(NULL, iofield, data, float_type,
				sizeof(float), &tmp, 0, NULL, 0, 0);
    return tmp;
}

extern double
get_IOdouble(iofield, data)
IOFieldPtr iofield;
void *data;
{
    double tmp;
    ffs_internal_convert_field(NULL, iofield, data, float_type, sizeof(tmp),
				&tmp, 0, NULL, 0, 0);
    return tmp;
}

#if SIZEOF_LONG_DOUBLE != 0
extern long double
get_IOlong_double(iofield, data)
IOFieldPtr iofield;
void *data;
{
    long double tmp;
    ffs_internal_convert_field(NULL, iofield, data, float_type, sizeof(tmp),
				&tmp, 0, NULL, 0, 0);
    return tmp;
}
#endif

extern void
put_IOdouble(iofield, d, data)
IOFieldPtr iofield;
double d;
void *data;
{
    IOgetFieldStruct tmp_src_field;  /*OK */
    tmp_src_field.offset = 0;
    tmp_src_field.size = sizeof(double);
    tmp_src_field.data_type = float_type;
    tmp_src_field.byte_swap = 0;
    ffs_internal_convert_field(NULL, &tmp_src_field, &d, iofield->data_type,
	  iofield->size, (char *) data + iofield->offset, 0, NULL, 0, 0);
}

#if SIZEOF_LONG_DOUBLE != 0
extern void
put_IOlong_double(iofield, d, data)
IOFieldPtr iofield;
long double d;
void *data;
{
    IOgetFieldStruct tmp_src_field;  /*OK */
    tmp_src_field.offset = 0;
    tmp_src_field.size = sizeof(d);
    tmp_src_field.data_type = float_type;
    tmp_src_field.byte_swap = 0;
    tmp_src_field.src_float_format = ffs_my_float_format;
    tmp_src_field.target_float_format = ffs_my_float_format;
    ffs_internal_convert_field(NULL, &tmp_src_field, &d, iofield->data_type,
	  iofield->size, (char *) data + iofield->offset, 0, NULL, 0, 0);
}

#endif

extern short
get_IOshort(iofield, data)
IOFieldPtr iofield;
void *data;
{
    short tmp;
    ffs_internal_convert_field(NULL, iofield, data, integer_type, sizeof(tmp),
				&tmp, 0, NULL, 0, 0);
    return tmp;
}

extern unsigned short
get_IOushort(iofield, data)
IOFieldPtr iofield;
void *data;
{
    unsigned short tmp;
    ffs_internal_convert_field(NULL, iofield, data, unsigned_type,
				sizeof(tmp), &tmp, 0, NULL, 0, 0);
    return tmp;
}

extern int
get_IOint(iofield, data)
IOFieldPtr iofield;
void *data;
{
    int tmp;
    ffs_internal_convert_field(NULL, iofield, data, integer_type, sizeof(tmp),
				&tmp, 0, NULL, 0, 0);
    return tmp;
}

extern unsigned int
get_IOuint(iofield, data)
IOFieldPtr iofield;
void *data;
{
    unsigned int tmp;
    ffs_internal_convert_field(NULL, iofield, data, unsigned_type,
				sizeof(tmp), &tmp, 0, NULL, 0, 0);
    return tmp;
}


extern long
get_IOlong(iofield, data)
IOFieldPtr iofield;
void *data;
{
    long tmp;
    ffs_internal_convert_field(NULL, iofield, data, integer_type, sizeof(tmp),
				&tmp, 0, NULL, 0, 0);
    return tmp;
}

extern unsigned long
get_IOulong(iofield, data)
IOFieldPtr iofield;
void *data;
{
    unsigned long tmp;
    ffs_internal_convert_field(NULL, iofield, data, unsigned_type,
				sizeof(tmp), &tmp, 0, NULL, 0, 0);
    return tmp;
}

#if SIZEOF_LONG_LONG != 0
extern long long
get_IOlong_long(iofield, data)
IOFieldPtr iofield;
void *data;
{
    long long tmp;
    ffs_internal_convert_field(NULL, iofield, data, integer_type, sizeof(tmp),
				&tmp, 0, NULL, 0, 0);
    return tmp;
}

extern unsigned long long
get_IOulong_long(iofield, data)
IOFieldPtr iofield;
void *data;
{
    unsigned long long tmp;
    ffs_internal_convert_field(NULL, iofield, data, unsigned_type,
				sizeof(tmp), &tmp, 0, NULL, 0, 0);
    return tmp;
}

#endif

extern void
put_IOshort(iofield, d, data)
IOFieldPtr iofield;
int d;
void *data;
{
    IOgetFieldStruct tmp_src_field;  /*OK */
    tmp_src_field.offset = 0;
    tmp_src_field.size = sizeof(d);
    tmp_src_field.data_type = integer_type;
    tmp_src_field.byte_swap = 0;
    ffs_internal_convert_field(NULL, &tmp_src_field, &d, iofield->data_type,
	  iofield->size, (char *) data + iofield->offset, 0, NULL, 0, 0);
}

extern void
put_IOushort(iofield, d, data)
IOFieldPtr iofield;
unsigned int d;
void *data;
{
    IOgetFieldStruct tmp_src_field;  /*OK */
    tmp_src_field.offset = 0;
    tmp_src_field.size = sizeof(d);
    tmp_src_field.data_type = integer_type;
    tmp_src_field.byte_swap = 0;
    ffs_internal_convert_field(NULL, &tmp_src_field, &d, iofield->data_type,
	  iofield->size, (char *) data + iofield->offset, 0, NULL, 0, 0);
}

extern void
put_IOint(iofield, d, data)
IOFieldPtr iofield;
int d;
void *data;
{
    IOgetFieldStruct tmp_src_field;  /*OK */
    tmp_src_field.offset = 0;
    tmp_src_field.size = sizeof(d);
    tmp_src_field.data_type = integer_type;
    tmp_src_field.byte_swap = 0;
    ffs_internal_convert_field(NULL, &tmp_src_field, &d, iofield->data_type,
	  iofield->size, (char *) data + iofield->offset, 0, NULL, 0, 0);
}

extern void
put_IOuint(iofield, d, data)
IOFieldPtr iofield;
unsigned int d;
void *data;
{
    IOgetFieldStruct tmp_src_field;  /*OK */
    tmp_src_field.offset = 0;
    tmp_src_field.size = sizeof(d);
    tmp_src_field.data_type = integer_type;
    tmp_src_field.byte_swap = 0;
    ffs_internal_convert_field(NULL, &tmp_src_field, &d, iofield->data_type,
	  iofield->size, (char *) data + iofield->offset, 0, NULL, 0, 0);
}


extern void
put_IOlong(iofield, d, data)
IOFieldPtr iofield;
long d;
void *data;
{
    IOgetFieldStruct tmp_src_field;  /*OK */
    tmp_src_field.offset = 0;
    tmp_src_field.size = sizeof(d);
    tmp_src_field.data_type = integer_type;
    tmp_src_field.byte_swap = 0;
    ffs_internal_convert_field(NULL, &tmp_src_field, &d, iofield->data_type,
	  iofield->size, (char *) data + iofield->offset, 0, NULL, 0, 0);
}

extern void
put_IOulong(iofield, d, data)
IOFieldPtr iofield;
unsigned long d;
void *data;
{
    IOgetFieldStruct tmp_src_field;  /*OK */
    tmp_src_field.offset = 0;
    tmp_src_field.size = sizeof(d);
    tmp_src_field.data_type = integer_type;
    tmp_src_field.byte_swap = 0;
    ffs_internal_convert_field(NULL, &tmp_src_field, &d, iofield->data_type,
	  iofield->size, (char *) data + iofield->offset, 0, NULL, 0, 0);
}

extern void
get_IOlong8(iofield, data, low_long, high_long)
IOFieldPtr iofield;
void *data;
unsigned long *low_long;
long *high_long;
{
    *low_long = 0;
    if (high_long)
	*high_long = 0;
    if (iofield->data_type == integer_type) {
	if (iofield->size == 2 * sizeof(long)) {
	    int low_bytes_offset = iofield->offset;
	    int high_bytes_offset = iofield->offset;
	    IOgetFieldStruct tmp_iofield;  /*OK */
	    tmp_iofield = *iofield;
#ifdef WORDS_BIGENDIAN
	    if (iofield->byte_swap) {
		high_bytes_offset += sizeof(long);
	    } else {
		low_bytes_offset += sizeof(long);
	    }
#else
	    if (iofield->byte_swap) {
		high_bytes_offset += sizeof(long);
	    } else {
		low_bytes_offset += sizeof(long);
	    }
#endif
	    tmp_iofield.size = sizeof(long);
	    tmp_iofield.offset = low_bytes_offset;
	    *low_long = get_IOulong(&tmp_iofield, data);
	    if (high_long) {
		tmp_iofield = *iofield;
		tmp_iofield.size = sizeof(long);
		tmp_iofield.offset = high_bytes_offset;
		*high_long = get_IOlong(&tmp_iofield, data);
	    }
	} else {
	    *low_long = get_IOlong(iofield, data);
	}
    } else if (iofield->data_type == float_type) {
	MAX_FLOAT_TYPE tmp;
	ffs_internal_convert_field(NULL, iofield, data, float_type,
				    sizeof(tmp), &tmp, 0, NULL, 0, 0);
	*low_long = (long) tmp;
    } else {
	fprintf(stderr, "Get IOlong8 failed on invalid data type!\n");
	exit(1);
    }
}

extern void
put_IOlong8(iofield, data, low_long, high_long)
IOFieldPtr iofield;
void *data;
unsigned long low_long;
long high_long;
{
    if ((iofield->data_type == integer_type) || 
	(iofield->data_type == unsigned_type)) {
	if (iofield->size == 2 * sizeof(long)) {
	    int low_bytes_offset = iofield->offset;
	    int high_bytes_offset = iofield->offset;
#ifdef WORDS_BIGENDIAN
	    if (iofield->byte_swap) {
		high_bytes_offset += sizeof(long);
	    } else {
		low_bytes_offset += sizeof(long);
	    }
#else
	    if (iofield->byte_swap) {
		high_bytes_offset += sizeof(long);
	    } else {
		low_bytes_offset += sizeof(long);
	    }
#endif
	    memcpy((char *) data + low_bytes_offset, &low_long, sizeof(long));
	    memcpy((char *) data + high_bytes_offset, &high_long,
		   sizeof(long));
	} else {
	    put_IOlong(iofield, low_long, data);
	}
    } else {
	fprintf(stderr, "Put IOlong8 failed on invalid data type!\n");
	exit(1);
    }
}

extern int
get_IOulong8(iofield, data, low_long, high_long)
IOFieldPtr iofield;
void *data;
unsigned long *low_long;
unsigned long *high_long;
{
    *low_long = 0;
    if (high_long)
	*high_long = 0;
    if (iofield->data_type == unsigned_type) {
	if (iofield->size == 2 * sizeof(long)) {
	    int low_bytes_offset = iofield->offset;
	    int high_bytes_offset = iofield->offset;
	    IOgetFieldStruct tmp_iofield;  /*OK */
	    tmp_iofield = *iofield;
#ifdef WORDS_BIGENDIAN
	    if (iofield->byte_swap) {
		high_bytes_offset += sizeof(long);
	    } else {
		low_bytes_offset += sizeof(long);
	    }
#else
	    if (iofield->byte_swap) {
		high_bytes_offset += sizeof(long);
	    } else {
		low_bytes_offset += sizeof(long);
	    }
#endif
	    tmp_iofield.size = sizeof(unsigned long);
	    tmp_iofield.offset = low_bytes_offset;
	    *low_long = get_IOulong(&tmp_iofield, data);
	    if (high_long) {
		tmp_iofield = *iofield;
		tmp_iofield.size = sizeof(unsigned long);
		tmp_iofield.offset = high_bytes_offset;
		*high_long = get_IOulong(&tmp_iofield, data);
	    }
	    return 0;
	} else {
	    *low_long = get_IOulong(iofield, data);
	    return 0;
	}
    } else if (iofield->data_type == integer_type) {
	/* this is a bit to ugly to handle at the moment.. */
	assert(FALSE);
	return 0;
    } else if (iofield->data_type == float_type) {
	MAX_FLOAT_TYPE tmp;
	ffs_internal_convert_field(NULL, iofield, data, float_type,
				    sizeof(tmp), &tmp, 0, NULL, 0, 0);
	*low_long = (long) tmp;
	return 0;
    } else {
	fprintf(stderr, "Get IOlong8 failed on invalid data type!\n");
	exit(1);
    }
    /* NOTREACHED */
    return 0;
}

extern char
get_IOchar(iofield, data)
IOFieldPtr iofield;
void *data;
{
    if (iofield->data_type == char_type) {
	return (char) *((char *) data + iofield->offset);
    } else {
	fprintf(stderr, "Get Char failed on invalid data type!\n");
	exit(1);
    }
    /* NOTREACHED */
    return 0;
}

extern void
put_IOchar(iofield, c, data)
IOFieldPtr iofield;
int c;
void *data;
{
    if (iofield->data_type == char_type) {
	*((char *) data + iofield->offset) = c;
    } else {
	fprintf(stderr, "Put Char failed on invalid data type!\n");
	exit(1);
    }
}

extern int
get_IOenum(iofield, data)
IOFieldPtr iofield;
void *data;
{
    IOgetFieldStruct tmp_iofield;  /*OK */
    tmp_iofield = *iofield;
    tmp_iofield.data_type = integer_type;
    return get_IOint(&tmp_iofield, data);
}

extern void
put_IOenum(iofield, enm, data)
IOFieldPtr iofield;
int enm;
void *data;
{
    IOgetFieldStruct tmp_iofield;  /*OK */
    tmp_iofield = *iofield;
    tmp_iofield.data_type = integer_type;
    put_IOint(&tmp_iofield, enm, data);
}

extern char *
get_IOstring(iofield, data)
IOFieldPtr iofield;
void *data;
{
    return get_IOstring_base(iofield, data, data);
}

extern char *
get_IOstring_base(iofield, data, string_base)
IOFieldPtr iofield;
void *data;
void *string_base;
{
    unsigned long offset = get_offset((void *) ((char *) data + iofield->offset),
			     iofield->size, iofield->byte_swap);
    if (offset == 0) {
	return NULL;
    } else if (offset > (unsigned long) data) {		/* probably *
							 * converted *
							 * string */
	return (char *) offset;
    } else {
	return (char *) string_base + offset;
    }
}

extern void *
get_IOaddr(iofield, data, string_base, encode)
IOFieldPtr iofield;
void *data;
void *string_base;
int encode;
{
    unsigned long offset = get_offset((void *) ((char *) data + iofield->offset),
			     iofield->size, iofield->byte_swap);
    if (offset == 0) {
	return NULL;
    } else if (!encode) {
	return (char *) offset;
    } else {
	return (char *) string_base + offset;
    }
}

extern void
put_IOstring(iofield, str, data)
IOFieldPtr iofield;
const char *str;
void *data;
{
    assert(iofield->size >= sizeof(char *));
    /* we just can't handle this in a put field if the string size  in the 
     * 
     * * record isn't as big or bigger than the local one... */

    if (iofield->size == sizeof(char *)) {
	/* move in the address */
	memcpy((char *) data + iofield->offset, &str, sizeof(char *));
    } else {
	/* zero the field in the record */
	memset((char *) data + iofield->offset, 0, iofield->size);

	if (sizeof(char *) == sizeof(long)) {
	    put_IOlong(iofield, (long) str, data);
	} else if (sizeof(char *) == sizeof(int)) {
	    put_IOint(iofield, (long) str, data);
	} else {
	    assert(FALSE);
	}
    }
}

static long
get_offset(data, size, swap)
void *data;
int size;
int swap;
{
    IOgetFieldStruct field;  /*OK */
    field.offset = 0;
    field.size = size;
    field.data_type = integer_type;
    field.byte_swap = swap;
    if (size == sizeof(int)) {
	return get_IOlong(&field, data);
    } else {
	field.offset = size - sizeof(long);
	field.size = sizeof(long);
	return get_IOlong(&field, data);
    }
}

extern IOdata_type
str_to_data_type(str)
const char *str;
{
    const char *end;
    while (isspace((int)*str)) {	/* skip preceeding space */
	str++;
    }
    end = str + strlen(str) - 1;
    while (isspace((int)*end)) {	/* test trailing space */
	end--;
    }
    end++;
    switch(str[0]) {
    case 'i': case 'I': /* integer */
	if (((end - str) == 7) &&
	    ((str[1] == 'n') || (str[1] == 'N')) &&
	    ((str[2] == 't') || (str[2] == 'T')) &&
	    ((str[3] == 'e') || (str[3] == 'E')) &&
	    ((str[4] == 'g') || (str[4] == 'G')) &&
	    ((str[5] == 'e') || (str[5] == 'E')) &&
	    ((str[6] == 'r') || (str[6] == 'R'))) {
	    return integer_type;
	}
	break;
    case 'f': case 'F': /* float */
	if (((end - str) == 5) &&
	    ((str[1] == 'l') || (str[1] == 'L')) &&
	    ((str[2] == 'o') || (str[2] == 'O')) &&
	    ((str[3] == 'a') || (str[3] == 'A')) &&
	    ((str[4] == 't') || (str[4] == 'T'))) {
	    return float_type;
	}
	break;
    case 'd': case 'D': /* double */
	if (((end - str) == 6) &&
	    ((str[1] == 'o') || (str[1] == 'O')) &&
	    ((str[2] == 'u') || (str[2] == 'U')) &&
	    ((str[3] == 'b') || (str[3] == 'B')) &&
	    ((str[4] == 'l') || (str[4] == 'L')) &&
	    ((str[5] == 'e') || (str[5] == 'E'))) {
	    return float_type;
	}
	break;
    case 'c': case 'C': /* char */
	if (((end - str) == 4) &&
	    ((str[1] == 'h') || (str[1] == 'H')) &&
	    ((str[2] == 'a') || (str[2] == 'A')) &&
	    ((str[3] == 'r') || (str[3] == 'R'))) {
	    return char_type;
	}
	break;
    case 's': case 'S': /* string */
	if (((end - str) == 6) &&
	    ((str[1] == 't') || (str[1] == 'T')) &&
	    ((str[2] == 'r') || (str[2] == 'R')) &&
	    ((str[3] == 'i') || (str[3] == 'I')) &&
	    ((str[4] == 'n') || (str[4] == 'N')) &&
	    ((str[5] == 'g') || (str[5] == 'G'))) {
	    return string_type;
	}
	break;
    case 'e': case 'E': /* enumeration */
	if (((end - str) == 11) &&
	    ((str[1] == 'n') || (str[1] == 'N')) &&
	    ((str[2] == 'u') || (str[2] == 'U')) &&
	    ((str[3] == 'm') || (str[3] == 'M')) &&
	    ((str[4] == 'e') || (str[4] == 'E')) &&
	    ((str[5] == 'r') || (str[5] == 'R')) &&
	    ((str[6] == 'a') || (str[6] == 'A')) &&
	    ((str[7] == 't') || (str[7] == 'T')) &&
	    ((str[8] == 'i') || (str[8] == 'I')) &&
	    ((str[9] == 'o') || (str[9] == 'O')) &&
	    ((str[10] == 'n') || (str[10] == 'N'))) {
	    return enumeration_type;
	}
	break;
    case 'b': case 'B': /* boolean */
	if (((end - str) == 7) &&
	    ((str[1] == 'o') || (str[1] == 'O')) &&
	    ((str[2] == 'o') || (str[2] == 'O')) &&
	    ((str[3] == 'l') || (str[3] == 'L')) &&
	    ((str[4] == 'e') || (str[4] == 'E')) &&
	    ((str[5] == 'a') || (str[5] == 'A')) &&
	    ((str[6] == 'n') || (str[6] == 'N'))) {
	    return boolean_type;
	}
	break;
    case 'u': case 'U': /* unsigned integer */
	if (((end - str) == 16) &&
	    ((str[1] == 'n') || (str[1] == 'N')) &&
	    ((str[2] == 's') || (str[2] == 'U')) &&
	    ((str[3] == 'i') || (str[3] == 'M')) &&
	    ((str[4] == 'g') || (str[4] == 'E')) &&
	    ((str[5] == 'n') || (str[5] == 'R')) &&
	    ((str[6] == 'e') || (str[6] == 'A')) &&
	    ((str[7] == 'd') || (str[7] == 'T')) &&
	    ((str[8] == ' ') || (str[8] == '	')) &&
	    ((str[9] == 'i') || (str[9] == 'I')) &&
	    ((str[10] == 'n') || (str[10] == 'N')) &&
	    ((str[11] == 't') || (str[11] == 'T')) &&
	    ((str[12] == 'e') || (str[12] == 'E')) &&
	    ((str[13] == 'g') || (str[13] == 'G')) &&
	    ((str[14] == 'e') || (str[14] == 'E')) &&
	    ((str[15] == 'r') || (str[15] == 'R'))) {
	    return unsigned_type;
	}
	break;
    }
    return unknown_type;
}

extern int
IO_field_type_eq(str1, str2)
const char *str1;
const char *str2;
{
    IOdata_type t1, t2;
    long t1_count, t2_count;

    t1 = array_str_to_data_type(str1, &t1_count);
    t2 = array_str_to_data_type(str2, &t2_count);

    if ((t1_count == -1) && (t2_count == -1)) {
	/* variant array */
	char *tmp_str1 = base_data_type(str1);
	char *tmp_str2 = base_data_type(str2);
	
	char *colon1 = strchr(tmp_str1, ':');
	char *colon2 = strchr(tmp_str2, ':');
	char *lparen1 = strchr(str1, '[');
	char *lparen2 = strchr(str2, '[');
	int count1 = 0;
	int count2 = 0;

	if (colon1 != NULL) {
	    count1 = colon1 - tmp_str1;
	} else {
	    count1 = strlen(tmp_str1);
	}
	if (colon2 != NULL) {
	    count2 = colon2 - tmp_str2;
	} else {
	    count2 = strlen(tmp_str2);
	}
	/*compare base type */
	if (strncmp(tmp_str1, tmp_str2,(count1>count2)?count1:count2) != 0) {
	    /* base types differ */
	    return 0;
	}
	io_free(tmp_str1);
	io_free(tmp_str2);
	if ((lparen1 == NULL) || (lparen2 == NULL)) return -1;
	return (strcmp(lparen1, lparen2) == 0);
    }
    return ((t1 == t2) && (t1_count == t2_count));
}

extern char *
base_data_type(str)
const char *str;
{
    char *typ = io_strdup(str);
    if (strchr(typ, '[') != NULL) {	/* truncate at array stuff */
	*strchr(typ, '[') = 0;
    }
    return typ;
}

extern IOdata_type
array_str_to_data_type(str, element_count_ptr)
const char *str;
long *element_count_ptr;
{
    IOdata_type ret_type;
    char field_type[1024];
    char *left_paren;
    long element_count = 1;
    int field_type_len;
#ifdef MODULE
    char *temp_ptr = 0;
#endif
    if ((left_paren = strchr(str, '[')) == NULL) {
	*element_count_ptr = 1;
	ret_type = str_to_data_type(str);
	return ret_type;
    }
    field_type_len = left_paren - str;
    strncpy(field_type, str, field_type_len);
    field_type[field_type_len] = 0;
    ret_type = str_to_data_type(field_type);
    while (left_paren != NULL) {
	char *end;
	long tmp = strtol(left_paren + 1, &end, 10);
	if (end == (left_paren + 1)) {
	    /* non numeric, likely variable array */
	    *element_count_ptr = -1;
	    return ret_type;
	}
	if (tmp <= 0) {
	    printf("FFS - Illegal static array size, %ld in \"%s\"\n",
		   tmp, str);
	    break;
	}
	if (*end != ']') {
	    printf("FFS - unexpected character at: \"%s\" in \"%s\"\n",
		   end, str);
	    break;
	}
	element_count = element_count * tmp;
	left_paren = strchr(end, '[');
    }
    *element_count_ptr = element_count;
    return ret_type;
}

extern int
IOget_array_size_dimen(str, fields, dimen, control_field)
const char *str;
IOFieldList fields;
int dimen;
int *control_field;
{
    char *left_paren, *end;
    long static_size;

    *control_field = -1;
    if ((left_paren = strchr(str, '[')) == NULL) {
	return 0;
    }	
    while (dimen != 0) {
	left_paren = strchr(left_paren + 1, '[');
	if (left_paren == NULL) return 0;
	dimen--;
    }
    static_size = strtol(left_paren + 1, &end, 0);
    if (left_paren + 1 == end) {
	/* dynamic element */
	char field_name[1024];
	int count = 0;
	int i = 0;
	while (((left_paren+1)[count] != ']') &&
	       ((left_paren+1)[count] != 0)) {
	    field_name[count] = (left_paren+1)[count];
	    count++;
	}
	field_name[count] = 0;
	while (fields[i].field_name != NULL) {
	    if (strcmp(field_name, fields[i].field_name) == 0) {
		if (str_to_data_type(fields[i].field_type) ==
		    integer_type) {
		    *control_field = i;
		    return -1;
		} else {
		    fprintf(stderr, "Variable length control field \"%s\" not of integer type.\n", field_name);
		    return 0;
		}
	    }
	    i++;
	}
	fprintf(stderr, "Array dimension \"%s\" in type spec\"%s\" not recognized.\n",
		field_name, str);
	fprintf(stderr, "Dimension must be a field name (for dynamic arrays) or a positive integer.\n");
	fprintf(stderr, "To use a #define'd value for the dimension, use the IOArrayDecl() macro.\n");
	return -1;
    }
    if (*end != ']') {
	fprintf(stderr, "Malformed array dimension, unexpected character '%c' in type spec \"%s\"\n",
		*end, str);
	fprintf(stderr, "Dimension must be a field name (for dynamic arrays) or a positive integer.\n");
	fprintf(stderr, "To use a #define'd value for the dimension, use the IOArrayDecl() macro.\n");
	return -1;
    }
    if (static_size <= 0) {
	fprintf(stderr, "Non-positive array dimension %ld in type spec \"%s\"\n",
		static_size, str);
	fprintf(stderr, "Dimension must be a field name (for dynamic arrays) or a positive integer.\n");
	fprintf(stderr, "To use a #define'd value for the dimension, use the IOArrayDecl() macro.\n");
	return -1;
    }
    return static_size;
}

extern const char *
data_type_to_str(dat)
IOdata_type dat;
{
    switch (dat) {
    case integer_type:
	return "integer";
    case unsigned_type:
	return "unsigned integer";
    case float_type:
	return "float";
    case char_type:
	return "char";
    case string_type:
	return "string";
    case enumeration_type:
	return "enumeration";
    case boolean_type:
	return "boolean";
    default:
	return "unknown_type";
    }
}

extern IOFieldPtr
get_IOfieldPtrFromList(field_list, fieldname)
IOFieldList field_list;
const char *fieldname;
{
    int index;
    IOFieldPtr ret_val;
    IOdata_type data_type;

    for (index = 0; field_list[index].field_name != NULL; index++) {
	if (strcmp(field_list[index].field_name, fieldname) == 0) {
	    break;
	}
    }
    if (field_list[index].field_name == NULL)
	return NULL;

    data_type = str_to_data_type(field_list[index].field_type);
    if (data_type == unknown_type) {
	fprintf(stderr, "Unknown field type for field %s\n",
		field_list[index].field_name);
	return NULL;
    }
    ret_val = (IOFieldPtr) io_malloc(sizeof(*ret_val));
    ret_val->src_float_format = Format_Unknown;
    ret_val->target_float_format = ffs_my_float_format;
    ret_val->offset = field_list[index].field_offset;
    ret_val->size = field_list[index].field_size;
    ret_val->data_type = data_type;
    ret_val->byte_swap = FALSE;
    return ret_val;
}

extern IOFieldPtr
get_IOfieldPtr(iofile, formatname, fieldname)
IOFile iofile;
const char *formatname;
const char *fieldname;
{
    IOFormat format = get_IOformat_by_name(iofile, formatname);
    int index;
    IOFieldPtr ret_val;
    IOdata_type data_type;
    long junk;

    if (format == NULL)
	return NULL;

    for (index = 0; index < format->body->field_count; index++) {
	if (strcmp(format->body->field_list[index].field_name,
		   fieldname) == 0) {
	    break;
	}
    }
    if (index >= format->body->field_count)
	return NULL;

    data_type = array_str_to_data_type(format->body->field_list[index].field_type,
				       &junk);
    if (data_type == unknown_type) {
	fprintf(stderr, "Unknown field type for field %s\n",
		format->body->field_list[index].field_name);
	return NULL;
    }
    ret_val = (IOFieldPtr) io_malloc(sizeof(*ret_val));
    ret_val->offset = format->body->field_list[index].field_offset;
    ret_val->size = format->body->field_list[index].field_size;
    ret_val->data_type = data_type;
    ret_val->byte_swap = iofile->byte_reversal;
    ret_val->src_float_format = iofile->native_float_format;
    ret_val->target_float_format = ffs_my_float_format;
    return ret_val;
}

extern IOFieldPtr
get_local_IOfieldPtr(iofile, formatname, fieldname)
IOFile iofile;
const char *formatname;
const char *fieldname;
{
    IOFieldPtr ret_val = get_IOfieldPtr(iofile, formatname, fieldname);

    /* 
     * get_local_IOfieldPtr() differs from get_IOfieldPtr() only in
     * that the byte_swap value is always false.  This is because
     * local IOfield ptrs are to be used with the records returned
     * from local conversions.  A local conversion already has
     * performed the byte_swapping. 
     */
    if (ret_val)
	ret_val->byte_swap = FALSE;
    return ret_val;
}
