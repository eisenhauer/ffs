
#include "config.h"

#include "assert.h"
#include "ffs.h"
#include "ffs_internal.h"
#include "cercs_env.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

static char *
make_tmp_buffer(FFSBuffer buf, int size);
static void *
quick_get_pointer(IOFieldPtr iofield, void *data);
static unsigned long
quick_get_ulong(IOFieldPtr iofield, void *data);
void
quick_put_ulong(IOFieldPtr iofield, unsigned long value, void *data);

static int add_to_tmp_buffer(FFSBuffer buf, int size);
static int
final_variant_size_for_record(int input_record_len, IOConversionPtr conv);

static void byte_swap(char *data, int size);

typedef struct addr_list {
    void *addr;
    int offset;
} addr_list_entry;

typedef struct encode_state {
    int copy_all;
    int output_len;
    int iovec_is_stack;
    int iovcnt;
    internal_iovec *iovec;
    int malloc_vec_size;
    int addr_list_is_stack;
    int addr_list_cnt;
    addr_list_entry *addr_list;
    int malloc_addr_size;
    int saved_offset_difference;
}*estate;

void
setup_header(FFSBuffer buf, FMFormat f, estate s) 
{
    int header_size = f->server_ID.length;
    int tmp_data;	/* offset for header */
    int align_pad;

    if (f->variant) {
	header_size += sizeof(FILE_INT);	/* length info */
    }
    align_pad = (8 - header_size) & 0x7;	/* align to 8 */
    header_size += align_pad;

    tmp_data = add_to_tmp_buffer(buf, header_size);

    memcpy((char *) buf->tmp_buffer + tmp_data, f->server_ID.value,
	   f->server_ID.length);

    memset((char*)buf->tmp_buffer + tmp_data + f->server_ID.length,
	   0, header_size - f->server_ID.length);

    /* fill in an IOV field for the header */
    s->iovec[0].iov_len = header_size;
    s->iovec[0].iov_offset = tmp_data;
    s->iovec[0].iov_base = NULL;	/* offset is in tmp_data */
    s->iovcnt++;
    s->output_len = header_size;
}


static char zeroes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
#define STACK_ARRAY_SIZE 100

static void
ensure_writev_room(estate s, int add_count)
{
    /* 
     * we need to track the length of the strings we're writing
     * assume a small number (STACK_ARRAY_SIZE) but expand
     * dynamically if we have more...  
     */
    if (!(s->malloc_vec_size) &&
	(s->iovcnt >= STACK_ARRAY_SIZE - add_count)) {
	int j;
	internal_iovec *tmpl = (internal_iovec *)
	    malloc(sizeof(internal_iovec) * (2 * STACK_ARRAY_SIZE + add_count));
	s->malloc_vec_size = (2 * STACK_ARRAY_SIZE + add_count);
	for (j = 0; j < s->iovcnt; j++) {
	    tmpl[j].iov_len = (s->iovec)[j].iov_len;
	    tmpl[j].iov_base = (s->iovec)[j].iov_base;
	    tmpl[j].iov_offset = (s->iovec)[j].iov_offset;
	}
	s->iovec = tmpl;
    } else if ((s->malloc_vec_size != 0) &&
	       (s->iovcnt >= s->malloc_vec_size - add_count)) {
	(s->malloc_vec_size) *= 2;
	s->iovec = realloc(s->iovec, sizeof(internal_iovec) *
				(s->malloc_vec_size));
    }
}

int
copy_data_to_tmp(estate s, FFSBuffer buf, void *data, int length, int req_alignment, int *tmp_data_loc)
{
    int pad = (req_alignment - s->output_len) & (req_alignment -1);  /*  only works if req_align is power of two */
    int tmp_data, msg_offset;
    switch (req_alignment) {
    case 1: case 2: case 4: case 8: case 16: break;
    default:
	assert(0);
    }
    ensure_writev_room(s, 2);
    tmp_data = add_to_tmp_buffer(buf, length + pad);
    if (pad != 0) {
	if (s->iovec[s->iovcnt-1].iov_base == NULL) {
	    /* last was tmp too */
	    memset((char*)buf->tmp_buffer + tmp_data, 0, pad);	/* zero pad */
	    tmp_data += pad;
	    s->iovec[s->iovcnt-1].iov_len += pad;
	} else {
	    s->iovec[s->iovcnt].iov_len = pad;
	    s->iovec[s->iovcnt].iov_offset = 0;
	    s->iovec[s->iovcnt].iov_base = zeroes;
	    s->iovcnt++;
	}
    }
    if (length != 0) {
	memcpy((char*)buf->tmp_buffer + tmp_data, data, length);
	s->iovec[s->iovcnt].iov_len = length + pad;
	s->iovec[s->iovcnt].iov_offset = tmp_data;
	s->iovec[s->iovcnt].iov_base = NULL;
	s->iovcnt++;
    }
    msg_offset = s->output_len + pad;
    if (tmp_data_loc) *tmp_data_loc = tmp_data;
    s->output_len += length + pad;
    return msg_offset;
}

int
add_data_iovec(estate s, FFSBuffer buf, void *data, int length, int req_alignment)
{
    int msg_offset;
    int pad = (req_alignment - s->output_len) & (req_alignment -1);  /*  only works if req_align is power of two */
    switch (req_alignment) {
    case 1: case 2: case 4: case 8: case 16: break;
    default:
	assert(0);
    }
    ensure_writev_room(s, 2);
    if (pad != 0) {
	s->iovec[s->iovcnt].iov_len = pad;
	s->iovec[s->iovcnt].iov_offset = 0;
	s->iovec[s->iovcnt].iov_base = zeroes;
	s->iovcnt++;
	s->output_len += pad;
    }
    if (length != 0) {
	s->iovec[s->iovcnt].iov_len = length;
	s->iovec[s->iovcnt].iov_offset = 0;
	s->iovec[s->iovcnt].iov_base = data;
	s->iovcnt++;
    }
    msg_offset = s->output_len;
    s->output_len += length;
    return msg_offset;
}

void
init_encode_state(estate state)
{
    state->output_len = 0;
    state->iovcnt = 0;
    state->malloc_vec_size = 0;
}

static void
handle_subfields(FFSBuffer buf, FMFormat f, estate s, int data_offset);

char *
FFSencode(FFSBuffer b, FMFormat fmformat, void *data, int *buf_size)
{
    internal_iovec stack_iov_array[STACK_ARRAY_SIZE];
    addr_list_entry stack_addr_list[STACK_ARRAY_SIZE];
    struct encode_state state;
    init_encode_state(&state);
    int base_offset = 0;
    int header_size;

    state.iovec_is_stack = 1;
    state.iovec = stack_iov_array;
    state.addr_list_is_stack = 1;
    state.addr_list = stack_addr_list;
    state.copy_all = 1;
    state.saved_offset_difference = 0;

    make_tmp_buffer(b, 0);

    /* setup header information */
    setup_header(b, fmformat, &state);

    header_size = state.output_len;
    state.saved_offset_difference = header_size;

    if (fmformat->variant || state.copy_all) {
	base_offset = copy_data_to_tmp(&state, b, data, 
				       fmformat->record_length, 1, NULL);
    }

    if (!fmformat->variant) {
	*buf_size = state.output_len;
	return b->tmp_buffer;
    }

    if (fmformat->recursive) {
	state.addr_list[state.addr_list_cnt].addr = data;
	state.addr_list[state.addr_list_cnt].offset = base_offset;
	state.addr_list_cnt++;
    }

    copy_data_to_tmp(&state, b, NULL, 0, 8, NULL);   /* force 64-bit align for var */
    handle_subfields(b, fmformat, &state, base_offset);
    {
	/* fill in actual length of data marshalled after header */
	char *tmp_data = b->tmp_buffer;
	int record_len = state.output_len - header_size;
	int len_align_pad = (4 - fmformat->server_ID.length) & 3;
	tmp_data += fmformat->server_ID.length + len_align_pad;
	memcpy(tmp_data, &record_len, 4);
    }
    *buf_size = state.output_len;
    return b->tmp_buffer;
}

static FFSEncodeVector
fixup_output_vector(FFSBuffer b, estate s)
{
    int size = (s->iovcnt + 3) * sizeof(struct FFSEncodeVec);
    int tmp_vec_offset = add_to_tmp_buffer(b, size);
    FFSEncodeVector ret;
    /* buffer will not be realloc'd after this */
    int i;

    ret = b->tmp_buffer + tmp_vec_offset;
    /* leave two blanks, see notes in ffs_file.c */
    /* the upshot is that we use these if we need to add headers */
    ret += 3;
    for (i=0; i < s->iovcnt; i++) {
	ret[i].iov_len = s->iovec[i].iov_len;
	if (s->iovec[i].iov_base != NULL) {
	    ret[i].iov_base = s->iovec[i].iov_base;
	} else {
	    ret[i].iov_base = b->tmp_buffer + s->iovec[i].iov_offset;
	}
    }
    ret[s->iovcnt].iov_base = NULL;
    if (!s->iovec_is_stack) {
	free(s->iovec);
	s->iovec = NULL;
    }
    if (!s->addr_list_is_stack && (s->addr_list != NULL)) {
	free(s->addr_list);
	s->addr_list = NULL;
    }
    return ret;
}

FFSEncodeVector
FFSencode_vector(FFSBuffer b, FMFormat fmformat, void *data)
{
    internal_iovec stack_iov_array[STACK_ARRAY_SIZE];
    addr_list_entry stack_addr_list[STACK_ARRAY_SIZE];
    struct encode_state state;
    init_encode_state(&state);
    int base_offset = 0;
    int header_size;

    state.iovec_is_stack = 1;
    state.iovec = stack_iov_array;
    state.addr_list_is_stack = 1;
    state.addr_list = stack_addr_list;
    state.copy_all = 0;
    state.saved_offset_difference = 0;

    make_tmp_buffer(b, 0);

    /* setup header information */
    setup_header(b, fmformat, &state);

    header_size = state.output_len;
    state.saved_offset_difference = header_size;

    if (fmformat->variant || state.copy_all) {
	base_offset = copy_data_to_tmp(&state, b, data, 
				       fmformat->record_length, 1, NULL);
    } else if (!state.copy_all) {
	base_offset = add_data_iovec(&state, b, data, 
				     fmformat->record_length, 1);
    }
    if (!fmformat->variant) {
	return fixup_output_vector(b, &state);
    }

    if (fmformat->recursive) {
	state.addr_list[state.addr_list_cnt].addr = data;
	state.addr_list[state.addr_list_cnt].offset = base_offset;
	state.addr_list_cnt++;
    }

    copy_data_to_tmp(&state, b, NULL, 0, 8, NULL);   /* force 64-bit align for var */
    handle_subfields(b, fmformat, &state, base_offset);
    {
	/* fill in actual length of data marshalled after header */
	char *tmp_data = b->tmp_buffer;
	int record_len = state.output_len - header_size;
	int len_align_pad = (4 - fmformat->server_ID.length) & 3;
	tmp_data += fmformat->server_ID.length + len_align_pad;
	memcpy(tmp_data, &record_len, 4);
    }
    return fixup_output_vector(b, &state);
}

static void
handle_subfield(FFSBuffer buf, FMFormat f, estate s, int data_offset, int parent_offset, FMTypeDesc *t);

static int
field_is_flat(FMFormat f, FMTypeDesc *t)
{
    switch (t->type) {
    case FMType_pointer:
	return FALSE;
    case FMType_array:
	if (t->static_size == 0) {
	    return FALSE;
	}
	return field_is_flat(f, t->next);
    case FMType_string:
	return FALSE;
    case FMType_subformat:
	return !f->field_subformats[t->field_index]->variant;
    case FMType_simple:
	return TRUE;
    }
}

static void
handle_subfields(FFSBuffer buf, FMFormat f, estate s, int data_offset)
{
    int i;
    int elements = 1;
    int j = 0;
    /* if base is not variant (I.E. doesn't contain addresses), return;*/
    if (!f->variant) return;

    for (i = 0; i < f->field_count; i++) {
	int subfield_offset = data_offset + f->field_list[i].field_offset;
	if (field_is_flat(f, &f->var_list[i].type_desc)) continue;
	handle_subfield(buf, f, s, subfield_offset, data_offset, 
			&f->var_list[i].type_desc); 
	
    }
}

static int
determine_size(FMFormat f, FFSBuffer buf, int parent_offset, FMTypeDesc *t)
{
    switch (t->type) {
    case FMType_pointer:
    case FMType_string:
	return f->pointer_size;
    case FMType_array: {
	int size = 1;
	while (t->type == FMType_array) {
	    if (t->static_size == 0) return f->pointer_size;
	    size *= t->static_size;
	    t = t->next;
	}
	size *= determine_size(f, buf, parent_offset, t);
	return size;
    }
    case FMType_subformat:
	return f->field_subformats[t->field_index]->record_length;
    case FMType_simple:
	return f->field_list[t->field_index].field_size;
    }
}

static void
handle_subfield(FFSBuffer buf, FMFormat f, estate s, int data_offset, int parent_offset, FMTypeDesc *t)
{

    switch (t->type) {
    case FMType_pointer:
    {
	struct _IOgetFieldStruct src_spec;
	int size, new_offset, tmp_data_loc;
	char *ptr_value;
	memset(&src_spec, 0, sizeof(src_spec));
	src_spec.size = f->pointer_size;
	ptr_value = quick_get_pointer(&src_spec, (char*)buf->tmp_buffer + data_offset);
	if (ptr_value == NULL) return;
	size = determine_size(f, buf, parent_offset, t->next);
	if (!s->copy_all && field_is_flat(f, t->next)) {
	    /* leave data where it sits */
	    new_offset = add_data_iovec(s, buf, ptr_value, size, 8);
	} else {
	    new_offset = copy_data_to_tmp(s, buf, ptr_value, size, 8, &tmp_data_loc);
	}
	quick_put_ulong(&src_spec, new_offset - s->saved_offset_difference, 
			(char*)buf->tmp_buffer + data_offset);
	if (field_is_flat(f, t->next)) return;
	handle_subfield(buf, f, s, tmp_data_loc, parent_offset, t->next);
	break;
    }
    case FMType_string:
    {
	struct _IOgetFieldStruct src_spec;
	char *ptr_value;
	int size, str_offset;
	memset(&src_spec, 0, sizeof(src_spec));
	src_spec.size = f->pointer_size;
	ptr_value = quick_get_pointer(&src_spec, (char*)buf->tmp_buffer + data_offset);
	if (ptr_value == NULL) return;
	size = strlen(ptr_value) + 1;
	if (!s->copy_all) {
	    /* leave data where it sits */
	    str_offset = add_data_iovec(s, buf, ptr_value, size, 1);
	} else {
	    str_offset = copy_data_to_tmp(s, buf, ptr_value, size, 1, NULL);
	}
	quick_put_ulong(&src_spec, str_offset - s->saved_offset_difference,
			(char*)buf->tmp_buffer + data_offset);
	break;
    }
    case FMType_array:
    {
	int elements = 1, i;
	int element_size;
	int var_array = 0;
	FMTypeDesc *next = t;
	int array_data_offset = data_offset;
	while (next->type == FMType_array) {
	    if (next->static_size == 0) {
		struct _IOgetFieldStruct src_spec;
		int field = next->control_field_index;
		memset(&src_spec, 0, sizeof(src_spec));
		src_spec.size = f->field_list[field].field_size;
		src_spec.offset = f->field_list[field].field_offset;
		int tmp = quick_get_ulong(&src_spec, (char*)buf->tmp_buffer + parent_offset);
		elements = elements * tmp;
		var_array = 1;
	    } else {
		elements = elements * next->static_size;
	    }
	    next = next->next;
	}
	element_size = determine_size(f, buf, parent_offset, next);
	if (var_array) {
	    struct _IOgetFieldStruct src_spec;
	    char *ptr_value;
	    int new_offset, size = element_size * elements;
	    memset(&src_spec, 0, sizeof(src_spec));
	    src_spec.size = f->pointer_size;
	    ptr_value = quick_get_pointer(&src_spec, (char*)buf->tmp_buffer + data_offset);
	    if (ptr_value == NULL) return;
	    if (!s->copy_all && field_is_flat(f, t->next)) {
		/* leave data where it sits */
		new_offset = add_data_iovec(s, buf, ptr_value, size, 8);
	    } else {
		new_offset = copy_data_to_tmp(s, buf, ptr_value, size, 8, &array_data_offset);
	    }
	    quick_put_ulong(&src_spec, new_offset - s->saved_offset_difference, 
			    (char*)buf->tmp_buffer + data_offset);
	}
	if (field_is_flat(f, next)) return;
	for (i = 0; i < elements ; i++) {
	    int element_offset = array_data_offset + i * element_size;
	    handle_subfield(buf, f, s, element_offset, parent_offset, next);
	}
	break;
    }
    case FMType_subformat:
    {
	int field_index = t->field_index;
	FMFormat subformat = f->field_subformats[field_index];
	handle_subfields(buf, subformat, s, data_offset);
	break;
    }
    default:
	assert(0);
    }
}

void
FFSfree_conversion(IOConversionPtr conv);

extern
void
free_FFSTypeHandle(FFSTypeHandle f)
{
    int i = 0;
    FFSfree_conversion(f->conversion);
    while(f->subformats && f->subformats[i]) {
	free_FFSTypeHandle(f->subformats[i]);
	f->subformats[i] = NULL;
	i++;
    }
    free(f->subformats);
    free(f->field_subformats);
    free(f);
}

extern
FFSTypeHandle
FFSTypeHandle_by_index(FFSContext c, int index)
{
    FFSTypeHandle handle = NULL;
    if (c->handle_list == NULL) {
	c->handle_list = malloc(sizeof(c->handle_list[0]) * (index+1));
	memset(c->handle_list, 0, sizeof(c->handle_list[0]) * (index+1));
	c->handle_list_size = index + 1;
    } else if (c->handle_list_size <= index) {
	c->handle_list = realloc(c->handle_list,
				 (index+1)*sizeof(c->handle_list[0]));
	memset(&c->handle_list[c->handle_list_size], 0, 
	       sizeof(c->handle_list[0]) * ((index+1) - c->handle_list_size));
	c->handle_list_size = index + 1;
    }
    if (c->handle_list[index] == NULL) {
	FMFormat fmf = FMformat_by_index(c->fmc, index);
	if (fmf == NULL) return NULL;
	c->handle_list[index] = malloc(sizeof(struct _FFSTypeHandle));
	handle = c->handle_list[index];
	handle->context = c;
	handle->format_id = index;
	handle->conversion = NULL;
	handle->warned_about_null_conversion = 0;
	handle->body = FMformat_by_index(c->fmc, index);
	if ((fmf->subformats && (fmf->subformats[0] != NULL)) || fmf->recursive) {
	    int i, k, subformat_count = 0;
	    while (fmf->subformats[subformat_count] != NULL) subformat_count++;
	    handle->subformats = 
		malloc(sizeof(FFSTypeHandle)*(subformat_count+1));
	    for (i = 0 ; i < subformat_count ; i++) {
		handle->subformats[i] = malloc(sizeof(*handle->subformats[i]));
		handle->subformats[i]->context = c;
		handle->subformats[i]->format_id = -1;
		handle->subformats[i]->conversion = NULL;
		handle->subformats[i]->warned_about_null_conversion = 0;
		handle->subformats[i]->body = fmf->subformats[i];
		handle->subformats[i]->subformats = NULL;
	    }
	    handle->subformats[subformat_count] = NULL;
	    handle->field_subformats = 
		malloc(fmf->field_count * sizeof(FFSTypeHandle));
	    memset(handle->field_subformats, 0, 
		   fmf->field_count * sizeof(FFSTypeHandle));
	    for (i = 0; i < fmf->field_count; i++) {
		if (fmf->field_subformats[i] != NULL) {
		    int j;
		    for (j = 0; j < subformat_count; j++) {
			if (fmf->field_subformats[i] == handle->subformats[j]->body) {
			    handle->field_subformats[i] = handle->subformats[j];
			}
		    }
		    if (fmf->field_subformats[i] == fmf) {
			/* recursive */
			handle->field_subformats[i] = handle;
		    }
		} else {
		    handle->field_subformats[i] = NULL;
		}
	    }
	    for (k = 0; k < subformat_count ; k++) {
		FFSTypeHandle sf = handle->subformats[k];
		FMFormat sfmf = fmf->subformats[k];
		sf->field_subformats = 
		    malloc(sfmf->field_count * sizeof(FFSTypeHandle));
		memset(sf->field_subformats, 0, 
		       sfmf->field_count * sizeof(FFSTypeHandle));
		for (i = 0; i < sfmf->field_count; i++) {
		    if (sfmf->field_subformats[i] != NULL) {
			int j;
			for (j = 0; j < subformat_count; j++) {
			    if (sfmf->field_subformats[i] == handle->subformats[j]->body) {
				sf->field_subformats[i] = handle->subformats[j];
			    }
			}
		    }
		}
	    }
	} else {
	    handle->subformats = NULL;
	    handle->field_subformats = NULL;
	}
    }
    return c->handle_list[index];
}

extern
FFSTypeHandle
FFSTypeHandle_from_encode(FFSContext c, char *buffer) 
{
    int index;
    FFSTypeHandle handle;
    /* first element in encoded buffer is format ID */
    FMFormat fmf = FMformat_from_ID(c->fmc, buffer);
    if (fmf == NULL) return NULL;
    index = fmf->format_index;
    handle = FFSTypeHandle_by_index(c, index);
    return handle;
}

extern
FFSContext
create_FFSContext()
{
    FFSContext c;
    c = (FFSContext) malloc((size_t) sizeof(*c));
    init_float_formats();
    c->fmc = create_FMcontext();
    c->handle_list_size = 0;
    c->handle_list = NULL;
    c->tmp.tmp_buffer = NULL;
    c->tmp.tmp_buffer_size = 0;
    c->tmp.tmp_buffer_in_use_size = 0;

    return c;
}

extern void
free_FFSContext(FFSContext c)
{
    int i;
    free(c->tmp.tmp_buffer);
    for (i = 0; i < c->handle_list_size; i++) {
	if (c->handle_list[i]) free_FFSTypeHandle(c->handle_list[i]);
    }
    free(c->handle_list);
    free_FMcontext(c->fmc);
    free(c);
}

#ifdef NOT_DEF
/* 
 * this field is for informational purposes only.  No formatting decisions
 * should be made based on its value.. 
 */
char *architecture = ARCH;
int in_use_ffs_version = -1;
static int ffs_marshaling_verbose = 0;

int write_IOheaders(IOFile iofile);
static int read_IOheaders(IOFile *iofileptr);

extern int set_ffs_version()
{
    if (in_use_ffs_version != -1) return in_use_ffs_version;

    if (cercs_getenv("FFS_FIXED_FORMAT_IDS") != NULL) {
	in_use_ffs_version = 5;
    } else {
	/* the current max version */
	in_use_ffs_version = 6;
    }
    if (getenv("FFS_MARSHALL_VERBOSE") != NULL) {
	ffs_marshaling_verbose = 1;
    } else {
	ffs_marshaling_verbose = 0;
    }
    return in_use_ffs_version;
}

/* Return 1 if ffs_conversion_generation = 1 else 0
 * This function won't be needed once if we have support
 * of default values in conversion_generation*/

static int
is_conversion_generation_set()
{
    char *gen_string = cercs_getenv("FFS_CONVERSION_GENERATION");
    int ffs_conversion_generation = FFS_CONVERSION_GENERATION_DEFAULT;
    return ffs_conversion_generation;
}


    
extern int
field_offset_compar(const void *a, const void *b)
{
    IOFieldList ia = (IOFieldList) a;
    IOFieldList ib = (IOFieldList) b;
    return (ia->field_offset - ib->field_offset);
}


extern
int
get_AtomicInt(iofile, file_int_ptr)
IOFile iofile;
FILE_INT *file_int_ptr;
{
#if SIZEOF_INT == 4
    int tmp_value;
    if (AtomicRead(iofile->file_id, &tmp_value, 4, iofile) != 4)
	return 0;

#else
    Baaad shit;
#endif
    if (iofile->byte_reversal)
	byte_swap((char *) &tmp_value, 4);
    *file_int_ptr = tmp_value;
    return 1;
}

extern
int
put_AtomicInt(iofile, file_int_ptr)
IOFile iofile;
FILE_INT *file_int_ptr;
{
#if SIZEOF_INT == 4
    int tmp_value = *file_int_ptr;
    if (AtomicWrite(iofile->file_id, &tmp_value, 4, iofile) != 4)
	return 0;
#else
    Baaad shit;
#endif
    return 1;
}

extern
int
AtomicRead(fd, buffer, length, iofile)
void *fd;
void *buffer;
int length;
IOFile iofile;
{
    return iofile->read_func(fd, buffer, length,
			     &iofile->errno_val, &iofile->result);
}

extern
int
AtomicReadV(iofile, iov, icount)
IOFile iofile;
struct iovec *iov;
int icount;
{
    if (iofile == NULL) {
	char *junk_result_str;
	int junk_errno;
	return os_readv_func(iofile->file_id, iov, icount, &junk_errno,
			     &junk_result_str);
    } else {
	return iofile->readv_func(iofile->file_id, iov, icount,
				  &iofile->errno_val, &iofile->result);
    }
}

extern
int
poll_IOfile(iofile)
IOFile iofile;
{
    if (iofile->poll_func) {
	return iofile->poll_func(iofile->file_id);
    } else {
	return TRUE;		/* always more to read if not eof */
    }
}


extern
int
AtomicWrite(fd, buffer, length, iofile)
void *fd;
void *buffer;
int length;
IOFile iofile;
{
    return iofile->write_func(fd, buffer, length,
			      &iofile->errno_val, &iofile->result);
}


/* 
 * Compress the internal iov list to pack adjacent memory blocks into 
 * a single entry.
 */
static void
compress_int_iov(int_iov, iovcnt)
internal_iovec *int_iov;
int *iovcnt;
{
    int last_index = 0;
    int i;
    for (i = 1; i < *iovcnt; i++) {
	int adjacent;
	if (int_iov[last_index].iov_base != NULL) {
	    void *last_end = (char *) int_iov[last_index].iov_base +
		int_iov[last_index].iov_len;
	    adjacent = (last_end == int_iov[i].iov_base);
	} else {
	    int last_end = int_iov[last_index].iov_offset +
		int_iov[last_index].iov_len;
	    adjacent = (last_end == int_iov[i].iov_offset);
	}
	if (adjacent) {
	    int_iov[last_index].iov_len += int_iov[i].iov_len;
	    int_iov[i].iov_len = 0;
	} else {
	    if (last_index + 1 != i) {
		int_iov[last_index + 1] = int_iov[i];
	    }
	    last_index = last_index + 1;
	}
    }
    *iovcnt = last_index + 1;
}

extern
int
AtomicWriteV(iofile, int_iov, iovcnt)
IOFile iofile;
internal_iovec *int_iov;
int iovcnt;
{
    int orig_iovcnt = iovcnt;
    int written_count = 0;
    int iovleft, i;
    struct iovec *iov = (struct iovec *) iofile->iov;

    /* try compressing the iovector to combine adjacent chunks. */
    /* This may modify the iovcnt value */
    compress_int_iov(int_iov, &iovcnt);

    iovleft = iovcnt;
    while (iovcnt > iofile->max_iov) {
	/* 
	 * if iovcnt is more than the number of chunks we can write in a 
	 * single AtomicWriteV, recurse to write out max allowed.
	 */
	int ret = AtomicWriteV(iofile, int_iov, iofile->max_iov);
	if (ret != iofile->max_iov)
	    return ret;
	int_iov += iofile->max_iov;
	iovcnt -= iofile->max_iov;
	iovleft -= iofile->max_iov;
    }

    /* 
     * Convert the internal IOV data structure to the simpler, (base, length)
     * structure used by the IO routines.
     */
    for (i = 0; i < iovcnt; i++) {
	iov[i].iov_len = int_iov[i].iov_len;
	if (int_iov[i].iov_base == NULL) {
	    iov[i].iov_base = (char *) iofile->tmp.tmp_buffer + int_iov[i].iov_offset;
	} else {
	    iov[i].iov_base = int_iov[i].iov_base;
	}
#ifdef PRINT_DEBUGGING
	{
	    int j;
	    printf("	[%d], %lx for %d ->", i, (long) iov[i].iov_base,
		   iov[i].iov_len);
	    for (j = 0; j < iov[i].iov_len - 4; j += 4) {
		int *tmp = (int *) (((char *) iov[i].iov_base) + j);
		printf("%08x ", *tmp);
	    }
	    for (; j < iov[i].iov_len; j++)
		printf("%02x", *(((char *) iov[i].iov_base) + j));
	    printf("\n");
	}
#endif
    }

    if (iovcnt == 1) {
	int iget = AtomicWrite(iofile->file_id, iov[0].iov_base,
			       iov[0].iov_len, iofile);
	if (iget == iov[0].iov_len)
	    return orig_iovcnt;
    }
    if (iofile->writev_func == NULL) {
	for (i = 0; i < iovcnt; i++) {
	    int iget = AtomicWrite(iofile->file_id, iov[i].iov_base,
				   iov[i].iov_len, iofile);
	    if (iget != iov[0].iov_len)
		return i;
	    written_count++;
	}
    } else {
	written_count = iofile->writev_func(iofile->file_id, iov, iovcnt,
					    &iofile->errno_val,
					    &iofile->result);
    }
    /* in case we compressed, adjust for it */
    return orig_iovcnt - (iovcnt - written_count);
}


void
reset_read_ahead(iofile)
IOFile iofile;
{
    iofile->read_ahead = FALSE;
    iofile->next_record_format = NULL;
    iofile->next_record_len = 0;
    iofile->remaining_count = 0;
}

extern int
version_of_IOfile(iofile)
IOFile iofile;
{
    return iofile->IOversion;
}

extern void
set_array_order_IOfile(IOFile iofile, int column_major)
{
    iofile->native_column_major_arrays = column_major;
}

extern
IOFile
open_IOfile(path, flags)
const char *path;
const char *flags;
{
    void *file;
    IOFile iofile;
    int allow_input = 0, allow_output = 0;

    file = os_file_open_func(path, flags, &allow_input, &allow_output);

    if (file == NULL) {
	char msg[128];
	(void) sprintf(msg, "open_IOfile failed for %s :", path);
	perror(msg);
	return NULL;
    }
    iofile = new_IOFile();
    iofile->file_id = file;

    set_interface_IOfile(iofile, os_file_write_func, os_file_read_func,
			 os_file_writev_func, os_file_readv_func, os_max_iov,
			 os_close_func, NULL);

    iofile->status = OpenNoHeader;
    reset_read_ahead(iofile);
    if (allow_input) {
	if (!read_IOheaders(&iofile)) {
	    printf("read headers failed\n");
	    return NULL;
	}
    }
    if (allow_output) {
	iofile->native_pointer_size = sizeof(char *);
	iofile->IOversion = CURRENT_IO_VERSION;
	if (!write_IOheaders(iofile)) {
	    printf("write headers failed\n");
	    return NULL;
	}
    }
    return iofile;
}

extern IOFile
open_version_IOfd(fd, version)
int fd, version;
{
    IOFile iofile;

    iofile = new_IOFile();
    iofile->file_id = (void *) (long)fd;
    iofile->status = OpenNoHeader;

    set_interface_IOfile(iofile, os_write_func, os_read_func,
			 os_writev_func, os_readv_func, os_max_iov,
			 os_close_func, os_poll_func);

    iofile->IOversion = version;
    reset_read_ahead(iofile);
    if (!write_IOheaders(iofile))
	return NULL;

    return iofile;
}

extern
IOFile
open_IOfd(fd, flag_str)
int fd;
const char *flag_str;
{
    IOFile iofile;
    int input = 0, output = 0;
    long tmp_flags = (long) flag_str;

    iofile = new_IOFile();
    iofile->file_id = (void *) (long)fd;
    iofile->status = OpenNoHeader;
    set_interface_IOfile(iofile, os_write_func, os_read_func,
			 os_writev_func, os_readv_func, os_max_iov,
			 os_close_func, os_poll_func);

    reset_read_ahead(iofile);

    tmp_flags &= ~(O_TRUNC);
    tmp_flags &= ~(O_CREAT);
    if ((O_RDONLY == tmp_flags) ||
	(O_WRONLY == tmp_flags)) {
	/* must be old style call */
	input = (O_RDONLY == (long) flag_str);
	output = (O_WRONLY & (long) flag_str);
    } else {
	if (strcmp(flag_str, "r") == 0) {
	    input = TRUE;
	} else if (strcmp(flag_str, "w") == 0) {
	    output = TRUE;
	} else {
	    fprintf(stderr, "Open flags value not understood in open_IOfd\n");
	    return NULL;
	}
    }
    if (input) {
	iofile->IOversion = CURRENT_IO_VERSION;
	if (!read_IOheaders(&iofile))
	    return NULL;
    }
    if (output) {
	iofile->IOversion = CURRENT_IO_VERSION;
	if (!write_IOheaders(iofile))
	    return NULL;
    }
    return iofile;
}

extern IOFile
create_IOfile()
{
    IOFile iofile = new_IOFile();
    return iofile;
}

static void *local_format_rep_callback(void* fid, int fidl, int hip, int hp, void*a, void*c) {return NULL;}
static int local_port_callback(void *cd) {return 0;}

extern
 IOContext
create_local_IOcontext()
{
    IOFile iofile;

    iofile = new_IOFile();
    iofile->status = OpenNoHeader;
    iofile->IOversion = CURRENT_IO_VERSION;
    iofile->is_context = TRUE;
    reset_read_ahead(iofile);
    iofile->server_callback = local_format_rep_callback;
    iofile->server_get_port_callback = local_port_callback;
    iofile->server_client_data = NULL;
    return (IOContext) iofile;
}

extern IOFile
open_created_IOfile(iofile, flag_str)
IOFile iofile;
char *flag_str;
{
    int input = 0, output = 0;
    long tmp_flags = (long) flag_str;

    tmp_flags &= ~(O_TRUNC);
    tmp_flags &= ~(O_CREAT);
    if ((O_RDONLY == tmp_flags) ||
	(O_WRONLY == tmp_flags)) {
	/* must be old style call */
	input = (O_RDONLY == (long) flag_str);
	output = (O_WRONLY & (long) flag_str);
    } else {
	if (strcmp(flag_str, "r") == 0) {
	    input = TRUE;
	} else if (strcmp(flag_str, "w") == 0) {
	    output = TRUE;
	} else {
	    fprintf(stderr, "Open flags value not understood in open_created_IOfile\n");
	    return NULL;
	}
    }
    reset_read_ahead(iofile);
    if (input) {
	iofile->IOversion = CURRENT_IO_VERSION;
	if (!read_IOheaders(&iofile))
	    return NULL;
    }
    if (output) {
	iofile->IOversion = CURRENT_IO_VERSION;
	if (!write_IOheaders(iofile))
	    return NULL;
    }
    return iofile;
}

extern void
set_interface_IOfile(iofile, write_func, read_func, writev_func, readv_func,
		     max_iov, close_func, poll_func)
IOFile iofile;
IOinterface_func write_func;
IOinterface_func read_func;
IOinterface_funcv writev_func;
IOinterface_funcv readv_func;
int max_iov;
IOinterface_close close_func;
IOinterface_poll poll_func;
{
    io_free(iofile->iov);
    iofile->write_func = write_func;
    iofile->read_func = read_func;
    iofile->max_iov = max_iov;
    iofile->iov = malloc(iofile->max_iov * sizeof(struct iovec));
    iofile->writev_func = writev_func;
    iofile->readv_func = readv_func;
    iofile->close_func = close_func;
    iofile->poll_func = poll_func;
}

extern void
set_socket_interface_IOfile(iofile)
IOFile iofile;
{
    set_interface_IOfile(iofile, os_write_func, os_read_func,
			 os_writev_func, os_readv_func, os_max_iov,
			 os_close_func, os_poll_func);
}

extern void
set_file_interface_IOfile(iofile)
IOFile iofile;
{
    set_interface_IOfile(iofile, os_file_write_func, os_file_read_func,
			 os_file_writev_func, os_file_readv_func, os_max_iov,
			 os_close_func, NULL);
}

extern void
set_fd_IOfile(iofile, fd)
IOFile iofile;
int fd;
{
    iofile->file_id = (void *) (long) fd;
}

extern void
close_IOfile(iofile)
IOFile iofile;
{
    if (iofile->is_context == 0) {
	iofile->close_func(iofile->file_id);
    }
    iofile->status = Closed;
}

extern void
free_IOcontext(iocontext)
IOContext iocontext;
{
    free_IOfile((IOFile) iocontext);
}

extern void
free_IOsubcontext(iocontext)
IOContext iocontext;
{
    free_IOfile((IOFile) iocontext);
}

extern int
file_id_IOfile(iofile)
IOFile iofile;
{
    return (int) (long) iofile->file_id;
}

extern void *
conn_IOfile(iofile)
IOFile iofile;
{
    return iofile->file_id;
}

extern void
set_conn_IOfile(iofile, conn)
IOFile iofile;
void *conn;
{
    iofile->file_id = conn;
}

extern
int
write_IOfile(iofile, ioformat, data)
IOFile iofile;
IOFormat ioformat;
void *data;
{
    struct _ffsvec elem;
    elem.format = ioformat;
    elem.data = data;
    return writev_IOfile(iofile, &elem, 1);
}

#define STACK_ARRAY_SIZE 10

static int
write_strings(iofile, write_iov, iovcnt)
IOFile iofile;
internal_iovec *write_iov;
int iovcnt;
{
    return (AtomicWriteV(iofile, write_iov, iovcnt) == iovcnt);
}

static void encode_buffer_to_vector(IOFile iofile, IOBuffer buf,
					  IOFormat ioformat, void *data,
					  int *malloced_array_size,
					  int *iovcnt,
					  internal_iovec ** write_iov,
				      int copy_to_buffer, int head_align,
					  int data_align);

extern
int
writev_IOfile(iofile, vec, count)
IOFile iofile;
ffsvec vec;
int count;
{
    int i;
    int iovcnt = 0;
    internal_iovec stack_iov_array[STACK_ARRAY_SIZE];
    internal_iovec *write_iov = stack_iov_array;
    int malloced_array_size = 0;
    int ret_val;

    if (iofile->status == Closed)
	return -1;

    make_tmp_buffer(&iofile->tmp, 0);
    for (i = 0; i < count; i++) {
	IOFormat ioformat = vec[i].format;
	void *data = vec[i].data;
	FILE_INT format_id;
	int tmp_id_offset, output_len, j;
	char *tmp_id;

	format_id = ioformat->format_id;

	if (iofile != ioformat->context) {
	    iofile->result = "format not associated with file";
	    return 0;
	}
	ensure_writev_room(&malloced_array_size, &iovcnt, &write_iov, 1);

	if ((iofile->tmp.tmp_buffer_in_use_size & 0x7) != 0) {
	    add_to_tmp_buffer(&iofile->tmp, 8 - (iofile->tmp.tmp_buffer_in_use_size & 0x7));
	}
	    
	tmp_id_offset = add_to_tmp_buffer(&iofile->tmp, sizeof(format_id));

	tmp_id = (char *) iofile->tmp.tmp_buffer + tmp_id_offset;
	memcpy(tmp_id, &format_id, sizeof(format_id));
	write_iov[iovcnt].iov_len = sizeof(format_id);
	write_iov[iovcnt].iov_offset = tmp_id_offset;
	write_iov[iovcnt].iov_base = NULL;	/* offset in tmp */
	iovcnt++;

	encode_buffer_to_vector(iofile, &iofile->tmp,
				ioformat, data, &malloced_array_size,
				&iovcnt, &write_iov,
				0 /* don't copy to buffer */ ,
				0 /* no extra header alignment */ ,
				0 /* no extra data alignment */ );
	/* update stats */
	output_len = 0;
	for (j=0; j< iovcnt; j++) output_len += write_iov[j].iov_len;
	iofile->stats.encode_bytes += output_len;
	iofile->stats.encode_msg_count++;

    }
    ret_val = write_strings(iofile, write_iov, iovcnt);
    if (malloced_array_size != 0) {
	io_free(write_iov);
    }
    return ret_val;
}

extern
int
write_array_IOfile(iofile, ioformat, data, count, struct_size)
IOFile iofile;
IOFormat ioformat;
void *data;
int count;
int struct_size;
{
    FILE_INT code = ARRAY_FOLLOWS;
    FILE_INT file_count = count;
    FILE_INT file_struct_size = struct_size;
    FILE_INT format_id = ioformat->format_id;
    int length;
    int result = 1;

    if (iofile->status == Closed)
	return -1;

    if (iofile != ioformat->context) {
	iofile->result = "format not associated with file";
	return 0;
    }
    if (ioformat->body->variant) {
	iofile->result = "Array operations not available on variant records";
	return 0;
    }
    if (struct_size < ioformat->body->record_length) {
	fprintf(stderr, "Array operation specified struct_size less than record length.  Format name \"%s\".\n", ioformat->body->format_name);
	return 0;
    }
    result &= put_AtomicInt(iofile, &code);
    result &= put_AtomicInt(iofile, &file_count);
    result &= put_AtomicInt(iofile, &file_struct_size);
    result &= put_AtomicInt(iofile, &format_id);
    if (!result)
	return 0;
    length = struct_size * count;

    /* update stats */
    iofile->stats.encode_bytes += length + /* header */ 4*4;
    iofile->stats.encode_msg_count += count;

    return (AtomicWrite(iofile->file_id, data, length, iofile) == length);
}

extern
int
next_IOrecord_length(iofile)
IOFile iofile;
{
    IOFormat format = next_IOrecord_format(iofile);
    IOConversionPtr conv;
    int next_record_len = next_raw_IOrecord_length(iofile);

    if (!format)
	return 0;

    conv = format->conversion;

    if (conv == NULL) {
	return next_record_len;
    } else {
	int align_delta_padding = (8 - conv->base_size_delta) & 0x7;
	IOFormat ioformat = next_IOrecord_format(iofile);
	int possible_var_size = (int) ((next_record_len - ioformat->body->record_length) *
				       (conv->max_var_expansion - 1.0));
	return next_record_len + conv->base_size_delta + align_delta_padding +
	    possible_var_size + 8;
    }
}

extern
int
next_raw_IOrecord_length(iofile)
IOFile iofile;
{
    if (iofile->next_record_format == NULL) {
	(void) next_IOrecord_format(iofile);
    }
    return iofile->next_record_len;
}

extern
IORecordType
next_IOrecord_type(iofile)
IOFile iofile;
{
    FILE_INT format_id;
    FILE_INT count = 1;
    FILE_INT struct_size = 0;
    if ((iofile->status != OpenForRead) && (iofile->status != OpenBeginRead)) {
	iofile->result = "File not ready to read";
	return IOerror;
    }
    if (iofile->read_ahead == FALSE) {
	if (!get_AtomicInt(iofile, &format_id)) {
	    iofile->next_record_type = (iofile->errno_val) ? IOerror : IOend;
	    return iofile->next_record_type;
	}
	if (format_id < 0) {
	    switch (format_id) {
	    case COMMENT_FOLLOWS:
		iofile->next_record_type = IOcomment;
		break;
	    case FORMAT_FOLLOWS:
		iofile->next_record_type = IOformat;
		break;
	    case REREGISTERED_FORMAT_FOLLOWS:
		if (!get_AtomicInt(iofile, &format_id))
		    return (iofile->errno_val) ? IOerror : IOend;
		if (format_id > iofile->reg_format_count) {
		    fprintf(stderr,
		      "Internal error, reregistered format too large\n");
		    exit(1);
		}
		if (iofile->format_list[format_id]->conversion != NULL) {
		    IOConversionPtr conv =
		    iofile->format_list[format_id]->conversion;
		    if (!conv->notify_of_format_change) {
			/* 
			 * hide new format by changing registered 
			 * conversion 
			 */
			IOFormat old_format = iofile->format_list[format_id];
			IOFormat new_format;
			IOFormatStruct tmp_format;
			IOConversionPtr conv;
			int struct_size;
			/* 
			 * copy info from new to old format, so we can 
			 * still return the old one as "next" 
			 */
			assert(next_IOrecord_type(iofile) == IOformat);
			iofile->format_list[format_id] = NULL;
			new_format = read_format_IOfile(iofile);
			conv = old_format->conversion;
			struct_size = old_format->body->record_length +
			    conv->base_size_delta;
			memcpy(&tmp_format, old_format, sizeof(*new_format));
			memcpy(old_format, new_format, sizeof(*new_format));
			memcpy(new_format, &tmp_format, sizeof(*new_format));
			iofile->format_list[format_id] = old_format;
			set_general_IOconversion(iofile,
					   old_format->body->format_name,
						 conv->native_field_list,
						 struct_size,
					 old_format->body->pointer_size);
			free_IOformat(new_format);
			return next_IOrecord_type(iofile);
		    }
		}
		/* the man's got to know things changed, ignore the cue */
		return next_IOrecord_type(iofile);
		/* NOTREACHED */
		break;
	    case ARRAY_FOLLOWS:
		if (!get_AtomicInt(iofile, &count))
		    return (iofile->errno_val) ? IOerror : IOend;
		if (!get_AtomicInt(iofile, &struct_size))
		    return (iofile->errno_val) ? IOerror : IOend;

		/* read the *REAL* format id */
		if (!get_AtomicInt(iofile, &format_id))
		    return (iofile->errno_val) ? IOerror : IOend;
		iofile->next_record_type = IOdata;
		break;
	    default:
		return (iofile->errno_val) ? IOerror : IOend;
	    }
	} else {
	    iofile->next_record_type = IOdata;
	}
	if (iofile->next_record_type == IOdata) {
	    if (format_id >= iofile->reg_format_count || format_id < 0) {
		fprintf(stderr, "Format id %d is out of range\n", format_id);
		fprintf(stderr, "Possible corrupt IO file....  exiting \n");
		return IOerror;
	    }
	    assert(format_id < iofile->reg_format_count);
	    iofile->next_record_format = iofile->format_list[format_id];
	    if (iofile->format_list[format_id]->body->variant) {
		FILE_INT record_len;
		if (!get_AtomicInt(iofile, &record_len))
		    return (iofile->errno_val) ? IOerror : IOend;
		iofile->next_record_len = record_len;
	    } else {
		if (struct_size != 0) {
		    iofile->next_record_len = struct_size;
		} else {
		    iofile->next_record_len =
			iofile->format_list[format_id]->body->record_length;
		}
	    }
	    iofile->remaining_count = count;
	}
	iofile->read_ahead = TRUE;
    }
    return iofile->next_record_type;
}

extern
int
next_IOrecord_count(iofile)
IOFile iofile;
{
    if (iofile->next_record_format == NULL) {
	(void) next_IOrecord_format(iofile);
    }
    return iofile->remaining_count;
}

extern
IOConversionPtr
IOcreate_mem_conv(old_field_list, old_struct_size, old_pointer_size,
		  new_field_list, new_struct_size, new_pointer_size)
IOFieldList old_field_list;
int old_struct_size;
int old_pointer_size;
IOFieldList new_field_list;
int new_struct_size;
int new_pointer_size;
{
    IOConversionPtr conv_ptr;
    IOFormatStruct tmp_format;
    IOFormatBody tmp_body;

    tmp_format.context = NULL;
    tmp_format.body = &tmp_body;
    tmp_body.record_length = old_struct_size;
    tmp_body.field_list = old_field_list;
    tmp_body.field_count = count_IOfield(old_field_list);
    tmp_body.pointer_size = old_pointer_size;
    tmp_body.float_format = ffs_my_float_format;
    tmp_body.column_major_arrays = 0;
    tmp_body.server_ID.length = 0;
    tmp_body.server_ID.value = NULL;
    generate_var_list(&tmp_format, NULL);
    conv_ptr = create_conversion(&tmp_format, new_field_list,
				 new_struct_size, new_pointer_size,
				 0, ffs_my_float_format, buffer_and_convert,
				 0, old_struct_size,
				 FALSE);
    /* more than buffer_and_convert, copy strings too... */
    conv_ptr->conversion_type = copy_dynamic_portion;
    io_free(tmp_body.var_list);
    return conv_ptr;
}

extern
void
set_conversion_IOcontext(iocontext, ioformat, field_list, struct_size)
IOContext iocontext;
IOFormat ioformat;
IOFieldList field_list;
int struct_size;
{
    set_IOconversion_for_format((IOFile) iocontext, ioformat, field_list,
				struct_size);
}

extern
void
set_IOconversion_for_format(iofile, file_ioformat, native_field_list,
			    native_struct_size)
IOFile iofile;
IOFormat file_ioformat;
IOFieldList native_field_list;
int native_struct_size;
{
    set_general_IOconversion_for_format(iofile, file_ioformat,
				   native_field_list, native_struct_size,
					(int) sizeof(char *));
}

extern
void
set_IOconversion(iofile, formatname, native_field_list, native_struct_size)
IOFile iofile;
const char *formatname;
IOFieldList native_field_list;
int native_struct_size;
{
    IOFormat file_ioformat = get_IOformat_by_name(iofile, formatname);
    if (file_ioformat == NULL) {
	fprintf(stderr, "Format %s not found in iofile.\n", formatname);
	return;
    }
    set_IOconversion_for_format(iofile, file_ioformat, native_field_list,
				native_struct_size);
}

extern
void
set_general_IOconversion(iofile, formatname, native_field_list,
			 native_struct_size, pointer_size)
IOFile iofile;
const char *formatname;
IOFieldList native_field_list;
int native_struct_size;
int pointer_size;
{
    IOFormat file_ioformat = get_IOformat_by_name(iofile, formatname);
    if (file_ioformat == NULL) {
	fprintf(stderr, "Format %s not found in iofile.\n", formatname);
	return;
    }
    set_general_IOconversion_for_format(iofile, file_ioformat,
				   native_field_list, native_struct_size,
					pointer_size);
}

extern
void
set_notify_of_format_change(iofile, formatname, value)
IOFile iofile;
const char *formatname;
int value;
{
    IOFormat file_ioformat = get_IOformat_by_name(iofile, formatname);
    IOConversionPtr conv_ptr = file_ioformat->conversion;
    if (conv_ptr != NULL) {
	conv_ptr->notify_of_format_change = value;
    } else {
	iofile->result = "Cannot set_notify_of_format_change because no conversion is registered";
    }
}

extern
void
set_local_IOconversion(iofile, formatname)
IOFile iofile;
const char *formatname;
{
    IOFormat file_ioformat = get_IOformat_by_name(iofile, formatname);
    IOconversion_type conv = none_required;
    IOFieldList input_field_list;
    int conv_index, i;
    IOConversionPtr conv_ptr =
    (IOConversionPtr) malloc(sizeof(IOConversionStruct));

    conv_ptr->notify_of_format_change = 1;
    conv_ptr->max_var_expansion = 1.0;
    conv_ptr->free_data = NULL;
    conv_ptr->free_func = NULL;
    if (file_ioformat == NULL) {
	fprintf(stderr, "Format %s not found in iofile.\n", formatname);
	return;
    }
    if (file_ioformat->body->byte_reversal)
	conv = direct_to_mem;

    input_field_list = file_ioformat->body->field_list;

    conv_index = 0;
    for (i = 0; i < file_ioformat->body->field_count; i++) {
	IOField input_field;
	IOdata_type data_type;
	long element_count;

	if (!file_ioformat->body->byte_reversal)
	    continue;		/* nothing to do for local */

	input_field = input_field_list[i];

	data_type = array_str_to_data_type(input_field.field_type,
					   &element_count);

	conv_ptr =
	    (IOConversionPtr) realloc(conv_ptr, sizeof(IOConversionStruct) +
				 conv_index * sizeof(IOconvFieldStruct));
	conv_ptr->conversions[conv_index].src_field.byte_swap =
	    file_ioformat->body->byte_reversal;
	data_type = array_str_to_data_type(input_field.field_type, 
					   &element_count);
	if (data_type == string_type) {
	    /* for local conversion, treat strings like ints */
	    data_type = integer_type;
	}
	conv_ptr->conversions[conv_index].src_field.data_type = data_type;
	conv_ptr->conversions[conv_index].src_field.offset =
	    input_field.field_offset;
	conv_ptr->conversions[conv_index].rc_swap = no_row_column_swap;
	conv_ptr->conversions[conv_index].src_field.size =
	    input_field.field_size;
	conv_ptr->conversions[conv_index].dest_size =
	    input_field.field_size;
	conv_ptr->conversions[conv_index].iovar =
	    &file_ioformat->body->var_list[i];
	conv_ptr->conversions[conv_index].dest_offset =
	    input_field.field_offset;
	conv_index++;
    }
    conv_ptr->conv_count = conv_index;
    conv_ptr->conversion_type = conv;
    conv_ptr->context = iofile;
    conv_ptr->ioformat = file_ioformat;

    file_ioformat->conversion = conv_ptr;
}


extern
int
read_raw_IOfile(iofile, data, buffer_max, ioformatptr)
IOFile iofile;
void *data;
int buffer_max;
IOFormat *ioformatptr;
{
    IOFormat ioformat;
    int next_record_len;

    if ((iofile->status != OpenForRead) && (iofile->status != OpenBeginRead))
	return -1;

    ioformat = next_IOrecord_format(iofile);
    if (ioformat == NULL)
	return 0;
    iofile->remaining_count--;
    next_record_len = iofile->next_record_len;

    if (iofile->remaining_count <= 0) {
	reset_read_ahead(iofile);
    }
    if (buffer_max < next_record_len) {
	char *buffer = (char *) malloc(next_record_len);
	if (data != NULL) {
	    fprintf(stderr, "Buffer length %d too small for format %s.  Record lost\n",
		    buffer_max, ioformat->body->format_name);
	}
	AtomicRead(iofile->file_id, buffer, next_record_len, iofile);
	free(buffer);
	return 0;
    } else {
	if (ioformatptr != NULL)
	    *ioformatptr = ioformat;
	if (AtomicRead(iofile->file_id, data, next_record_len, iofile)
	    < next_record_len)
	    return 0;
	return 1;
    }
    /* NOTREACHED */
}

extern
int
read_raw_array_IOfile(iofile, ioformat, data, count, struct_size)
IOFile iofile;
IOFormat ioformat;
void *data;
int count;
int struct_size;
{
    if ((iofile->status != OpenForRead) && (iofile->status != OpenBeginRead))
	return -1;

    if (ioformat != next_IOrecord_format(iofile)) {
	fprintf(stderr, "read_array_IOfile called with invalid ioformat\n");
	return -1;
    }
    if (ioformat->body->variant) {
	fprintf(stderr, "Cannot read arrays of variant records\n");
	return -1;
    }
    if (struct_size < ioformat->body->record_length) {
	fprintf(stderr, "Array operation specified struct_size less than record length.  Format name \"%s\".\n", ioformat->body->format_name);
	return 0;
    }
    if (ioformat == NULL)
	return 0;
    if (count > iofile->remaining_count) {
	count = iofile->remaining_count;
    }
    if (data == NULL) {
	int i;
	for (i = 0; i < count; i++) {
	    read_raw_IOfile(iofile, NULL, 0, NULL);
	}
	return count;
    }
    iofile->remaining_count -= count;
    if (iofile->remaining_count <= 0) {
	reset_read_ahead(iofile);
    }
    if (struct_size == iofile->next_record_len) {
	int read_size = struct_size * count;
	if (AtomicRead(iofile->file_id, data, read_size, iofile) < read_size)
	    return 0;
	return count;
    } else {
	int i;
	for (i = 0; i < count; i++) {
	    if (AtomicRead(iofile->file_id, data, iofile->next_record_len,
			   iofile) < iofile->next_record_len)
		return i;
	    data = (void *) ((char *) data + struct_size);
	}
	return count;
    }
    /* NOTREACHED */
}

static
int
read_record_from_file(iofile, conv, dest, dest_len, buffer_strings)
IOFile iofile;
IOConversionPtr conv;
void *dest;
int dest_len;
int buffer_strings;
{
    IOFormat ioformat;
    int input_record_len;
    IOConversionStruct null_conv;
    struct iovec read_iov[2];
    int read_icount = 1;
    int ret = 1;
    conversion_action params;

    if ((iofile->status != OpenForRead) && (iofile->status != OpenBeginRead))
	return -1;

    if (dest == NULL) {
	/* discard record */
	return read_raw_IOfile(iofile, NULL, 0, NULL);
    }
    if (conv == NULL) {
	null_conv.conversion_type = none_required;
	null_conv.conv_count = 0;
	null_conv.base_size_delta = 0;
	null_conv.max_var_expansion = 1.0;
	null_conv.context = iofile;
	null_conv.ioformat = next_IOrecord_format(iofile);
	null_conv.native_field_list = NULL;
	conv = &null_conv;
    }
    if (iofile != conv->context) {
	fprintf(stderr, "IOFile and conversion mismatch\n");
	return -1;
    }
    ioformat = next_IOrecord_format(iofile);
    if (ioformat == NULL)
	return 0;
    if (ioformat != conv->ioformat) {
	fprintf(stderr, "IOFormat and conversion mismatch\n");
	return -1;
    }
    iofile->remaining_count--;
    input_record_len = iofile->next_record_len;
    if (iofile->remaining_count <= 0) {
	reset_read_ahead(iofile);
    }
    params.cur_base = NULL;
    params.cur_variant = NULL;
    params.final_base = dest;
    params.variant_with_base = !buffer_strings;
    set_conversion_params(ioformat, input_record_len, conv, &params);

    /* update stats */
    iofile->stats.decode_bytes += input_record_len;
    iofile->stats.decode_msg_count++;

    if (ret == 0) {
	read_raw_IOfile(iofile, NULL, 0, NULL);
	return 0;
    }
    read_iov[0].iov_base = params.src_address;
    read_iov[0].iov_len = ioformat->body->record_length;
    read_icount = 1;
    if ((input_record_len - ioformat->body->record_length) != 0) {
	int src_align_pad = (8 - ioformat->body->record_length) & 0x7;
	/* 
	 * the record length may have provided padding essential to 
	 * the alignment of variant arrays in the string area.
	 * This realigns the buffer to a similar alignment.
	 */
	params.src_string_address =
	    (char *) params.src_string_address + src_align_pad;
	params.final_string_address =
	    (char *) params.final_string_address + src_align_pad;

	read_iov[1].iov_base = params.src_string_address;
	read_iov[1].iov_len = input_record_len - ioformat->body->record_length;
	read_icount += 1;
    }
    if (AtomicReadV(iofile, read_iov, read_icount) < read_icount)
	return 0;
    if (conv->conversion_type != none_required) {
	FFSconvert_record(conv, params.src_address, params.dest_address,
			 params.final_string_address,
			 params.src_string_address);
    }
    return !IOhas_error(iofile);
}

extern
int
read_IOfile(iofile, data)
IOFile iofile;
void *data;
{
    IOFormat format = next_IOrecord_format(iofile);
    IOConversionPtr conv;
    int struct_size;

    if (!format)
	return 0;

    conv = format->conversion;
    if (conv == NULL) {
	/* no conversion specified */
	if (data != NULL) {
	    iofile->result = "Must specify conversion to use read_IOfile";
	    /* discard record */
	    read_raw_IOfile(iofile, NULL, 0, NULL);
	    return 0;
	} else {
	    /* discard record */
	    read_raw_IOfile(iofile, NULL, 0, NULL);
	    return 1;
	}
    } else {
	struct_size = conv->base_size_delta + conv->ioformat->body->record_length;
    }
    /* read the record, leaving strings in tmp buffer */
    return read_record_from_file(iofile, conv, data, struct_size, TRUE);
}

extern
int
read_to_buffer_IOfile(iofile, data, buffer_len)
IOFile iofile;
void *data;
int buffer_len;
{
    IOFormat format = next_IOrecord_format(iofile);
    IOConversionPtr conv;

    if (!format)
	return 0;

    conv = format->conversion;

    /* read the entire record and put it in data */
    return read_record_from_file(iofile, conv, data, buffer_len, FALSE);
}

static int
read_and_convert_N_records(iofile, struct_size, rec_len, count, read_dest, conv_dest, conv)
IOFile iofile;
int struct_size;
int rec_len;
int count;
char *read_dest;
char *conv_dest;
IOConversionPtr conv;
{
    int i;
    if (struct_size == rec_len) {
	int read_size = struct_size * count;
	if (AtomicRead(iofile->file_id, read_dest, read_size, iofile) < read_size)
	    return 0;
    } else {
	int i;
	void *dest = read_dest;
	for (i = 0; i < count; i++) {
	    if (AtomicRead(iofile->file_id, dest, rec_len, iofile)
		< iofile->next_record_len)
		return i;
	    dest = (void *) ((char *) dest + rec_len);
	}
    }
    if (conv->conversion_type == none_required)
	return count;

    for (i = 0; i < count; i++) {
	FFSconvert_record(conv, (void *) ((char *) read_dest + (rec_len * i)),
		       (void *) ((char *) conv_dest + (struct_size * i)),
			 NULL, NULL);
	if (IOhas_error(iofile))
	    return i;
    }
    return count;
}

extern
int
read_array_IOfile(iofile, data, count, struct_size)
IOFile iofile;
void *data;
int count;
int struct_size;
{
    IOConversionPtr conv;
    int rec_len;
    IOFormat format = next_IOrecord_format(iofile);

    if (!format)
	return 0;

    conv = format->conversion;

    if ((iofile->status != OpenForRead) && (iofile->status != OpenBeginRead))
	return -1;

    if (conv == NULL) {
	/* no conversion specified */
	if (data != NULL) {
	    iofile->result = "Must specify conversion to use read_array_IOfile";
	    /* discard record */
	    read_raw_IOfile(iofile, NULL, 0, NULL);
	    return -1;
	}
    }
    if (conv->ioformat->body->variant) {
	iofile->result = "Cannot read arrays of variant records";
	return -1;
    }
    if (next_IOrecord_format(iofile) == NULL)
	return 0;
    if (count > iofile->remaining_count) {
	count = iofile->remaining_count;
    }
    if (data == NULL) {
	return read_raw_array_IOfile(iofile, format, data, count, struct_size);
    }
    iofile->remaining_count -= count;
    rec_len = iofile->next_record_len;
    if (iofile->remaining_count <= 0) {
	reset_read_ahead(iofile);
    }
    if (conv->conversion_type != buffer_and_convert) {
	int ret;
	ret = read_and_convert_N_records(iofile, struct_size, rec_len, count,
					 data, data, conv);
	return ret;
    } else {
	/* need buffer space.   */
	int tmp_size = TMP_BUFFER_INIT_SIZE;
	char *tmp = make_tmp_buffer(&iofile->tmp, tmp_size);
	int tmp_count = tmp_size / rec_len;
	int i;

	while (tmp_count == 0) {
	    tmp_size *= 2;
	    tmp_count = tmp_size / rec_len;
	}
	tmp = (char *) make_tmp_buffer(&iofile->tmp, tmp_size);
	if (tmp == NULL) {
	    iofile->result = "Record too large for buffered conversion";
	    return 0;
	}
	for (i = 0; i < count; i += tmp_count) {
	    if ((i + 1) * tmp_count > count) {
		tmp_count = count - i * tmp_count;
	    }
	    read_and_convert_N_records(iofile, struct_size, rec_len, tmp_count,
				    tmp, (char *) data + i * struct_size,
				       conv);
	}
    }

    return count;
    /* NOTREACHED */
}

static void
make_tmp_offsets(IOFile iofile, IOBuffer buf, IOFormat ioformat,
		       int *malloced_array_size, int *iovcnt,
		       internal_iovec ** write_iov, int *record_len,
		       int tmp_offset, int data_offset,
		       int copy_all);
static int
count_tmp_offsets(IOFormat ioformat, void *data, int data_offset);

static char zeroes[8] = {0, 0, 0, 0, 0, 0, 0, 0};

static int
fill_variant_element(ioformat, buf, i, strptr, record_len, write_iov, iovcnt,
		  tmp_offset, data_offset, malloced_array_size, copy_all)
IOFormat ioformat;
IOBuffer buf;
int i;
char *strptr;
int *iovcnt;
internal_iovec **write_iov;
int *record_len;
int tmp_offset;
int data_offset;
int *malloced_array_size;
int copy_all;
{
    int ret_val = 0;

    if (ffs_marshaling_verbose) {
	printf("Marshaling field \"%s\" in format \"%s\"\n\tbase address is 0x%lx\n", 
	       ioformat->body->field_list[i].field_name, 
	       ioformat->body->format_name, (long)strptr);
    }
    if (ioformat->body->var_list[i].var_array == FALSE) {
	/* simple string case */
	int item_len = strlen(strptr) + 1;
	if (copy_all) {
	    int tmp_offset = add_to_tmp_buffer(buf, item_len);
	    memcpy((char *) buf->tmp_buffer + tmp_offset, strptr, item_len);
	    (*write_iov)[*iovcnt].iov_offset = tmp_offset;
	    (*write_iov)[*iovcnt].iov_base = NULL;
	} else {
	    (*write_iov)[*iovcnt].iov_base = strptr;
	    (*write_iov)[*iovcnt].iov_offset = -1;
	}
	(*write_iov)[*iovcnt].iov_len = item_len;
	*record_len += (*write_iov)[*iovcnt].iov_len;
	(*iovcnt)++;
    } else {
	/* var array case */
	long var_element_count;
	IOFormat subformat = ioformat->field_subformats[i];
	int item_size = ioformat->body->field_list[i].field_size;
	int align_size = min_align_size(item_size);
	int current_align, total_variant_bytes;

	switch (align_size) {
	case 1:
	    current_align = 0;
	    break;
	case 2:
	    current_align = (*record_len) & 0x1;
	    break;
	case 4:
	    current_align = (*record_len) & 0x3;
	    break;
	case 8:
	    current_align = (*record_len) & 0x7;
	    break;
	case 16:
	    current_align = (*record_len) & 0xf;
	    break;
	default:
	    current_align = (*record_len) % align_size;
	    break;
	}
	if (current_align != 0) {
	    int item_len = align_size - current_align;
	    /* 
	     * argh, must keep variant arrays appropriately aligned, even
	     * in the file.  Otherwise it's trouble when we read them.
	     */
	    (*write_iov)[*iovcnt].iov_len = item_len;
	    if (copy_all) {
		int tmp_offset = add_to_tmp_buffer(buf, item_len);
		memcpy((char *) buf->tmp_buffer + tmp_offset, &zeroes[0], item_len);
		(*write_iov)[*iovcnt].iov_offset = tmp_offset;
		(*write_iov)[*iovcnt].iov_base = NULL;
	    } else {
		(*write_iov)[*iovcnt].iov_base = &zeroes[0];
		(*write_iov)[*iovcnt].iov_offset = -1;
	    }
	    *record_len += (*write_iov)[*iovcnt].iov_len;
	    ret_val = (*write_iov)[*iovcnt].iov_len;
	    (*iovcnt)++;
	}
	var_element_count = 
	    get_array_element_count(&ioformat->body->var_list[i],
					(char*)buf->tmp_buffer + data_offset,0);
	total_variant_bytes = item_size * var_element_count;
	if (ioformat->body->var_list[i].byte_vector) {
	    int array_offset;
	    long bytes_in_vector = 0;
	    struct _IOgetFieldStruct tmp_field;  /*OK*/
	    IOFieldPtr ctl_field = ioformat->body->var_list[i].dimens[0].control_field;

	    ensure_writev_room(malloced_array_size, iovcnt, write_iov,
			       (int) var_element_count + 2);
	    /* byte vector special case */
	    total_variant_bytes = item_size;
	    (*write_iov)[*iovcnt].iov_len = total_variant_bytes;
	    *record_len += (*write_iov)[*iovcnt].iov_len;

	    array_offset = add_to_tmp_buffer(buf, total_variant_bytes);
	    (*write_iov)[*iovcnt].iov_offset = array_offset;
	    (*write_iov)[*iovcnt].iov_base = NULL;	/* tmp offset */
	    (*iovcnt)++;

	    tmp_field.size = sizeof(long);
	    tmp_field.offset = 0;
	    /* replace control count with a 1 */
	    quick_put_ulong(ctl_field, (long) 1,
			    (char *) buf->tmp_buffer + data_offset);
	    for (i = 0; i < var_element_count; i++) {
		bytes_in_vector += ((IOEncodeVector) strptr)[i].iov_len;
	    }
	    /* fill in offset for combined bytes */

	    quick_put_ulong(&tmp_field, (long) *(record_len),
			    (char *) buf->tmp_buffer + array_offset);
	    tmp_field.offset = IOOffset(IOEncodeVector, iov_len);
	    /* fill in size for combined bytes */
	    quick_put_ulong(&tmp_field, bytes_in_vector,
			    (char *) buf->tmp_buffer + array_offset);
	    for (i = 0; i < var_element_count; i++) {
		(*write_iov)[*iovcnt].iov_len =
		    ((IOEncodeVector) strptr)[i].iov_len;
		(*write_iov)[*iovcnt].iov_base =
		    ((IOEncodeVector) strptr)[i].iov_base;
		(*write_iov)[*iovcnt].iov_offset = 0;
		*record_len += (*write_iov)[*iovcnt].iov_len;
		(*iovcnt)++;
	    }
	    return ret_val;
	}
	/* not the byte vector special case */

	(*write_iov)[*iovcnt].iov_len = total_variant_bytes;
	*record_len += (*write_iov)[*iovcnt].iov_len;
	if (((subformat != NULL) && (subformat->body->variant)) ||
	    (ioformat->body->var_list[i].string == TRUE)) {
	    int array_offset = add_to_tmp_buffer(buf, total_variant_bytes);
	    int j;
	    int element_offset = 0;
	    memcpy((char *) buf->tmp_buffer + array_offset, strptr,
		   total_variant_bytes);
	    (*write_iov)[*iovcnt].iov_offset = array_offset;
	    (*write_iov)[*iovcnt].iov_base = NULL;	/* tmp offset */
	    (*iovcnt)++;
	    for (j = 0; j < var_element_count; j++) {
		if (ioformat->body->var_list[i].string == TRUE) {
		    /* string array case */
		    char *substring = *(((char **) strptr) + j);
		    int item_len;
		    int in_rec_offset = 0;
		    if (substring != NULL) {
			ensure_writev_room(malloced_array_size, iovcnt, write_iov, 1);

			item_len = strlen(substring) + 1;
			(*write_iov)[*iovcnt].iov_len = item_len;
			if (copy_all) {
			    int substr_offset = add_to_tmp_buffer(buf, item_len);
			    memcpy((char *) buf->tmp_buffer + substr_offset,
				   substring, item_len);
			    (*write_iov)[*iovcnt].iov_offset = substr_offset;
			    (*write_iov)[*iovcnt].iov_base = NULL;
			} else {
			    (*write_iov)[*iovcnt].iov_base = substring;
			    (*write_iov)[*iovcnt].iov_offset = -1;
			}
			in_rec_offset = *record_len;
			*record_len += (*write_iov)[*iovcnt].iov_len;
			(*iovcnt)++;
		    }
		    *((long *) ((char *) buf->tmp_buffer + array_offset + element_offset)) = in_rec_offset;
		} else {
		    make_tmp_offsets(ioformat->context, buf,
				     subformat,
				     malloced_array_size,
				     iovcnt, write_iov,
				     record_len,
				     array_offset,
				array_offset + element_offset, copy_all);
		}
		element_offset += item_size;
	    }
	} else {
	    int item_len = (*write_iov)[*iovcnt].iov_len;
	    if (copy_all) {
		int tmp_offset = add_to_tmp_buffer(buf, item_len);
		memcpy((char *) buf->tmp_buffer + tmp_offset, strptr, item_len);
		(*write_iov)[*iovcnt].iov_offset = tmp_offset;
		(*write_iov)[*iovcnt].iov_base = NULL;
	    } else {
		(*write_iov)[*iovcnt].iov_base = strptr;
		(*write_iov)[*iovcnt].iov_offset = -1;
	    }
	    (*iovcnt)++;
	}
    }
    return ret_val;		/* amount we adjusted rec_len before *
				 * variant */
}

static int
count_variant_element(ioformat, i, data, subfield_data, data_offset)
IOFormat ioformat;
int i;
void *data;
void *subfield_data;
int data_offset;
{
    int count = 0;

    if (ioformat->body->var_list[i].dimen_count == 0) {
	assert(ioformat->body->var_list[i].string == TRUE);
	/* string case */
	return count;
    } else {
	/* var array case */
	long ctl_value;
	IOFormat subformat = ioformat->field_subformats[i];
	int item_size = ioformat->body->field_list[i].field_size;

	ctl_value = 
	    get_array_element_count(&ioformat->body->var_list[i],
				    (char*)data + data_offset, 0);
	if ((subformat != NULL) && (subformat->body->variant)) {
	    int j;
	    int element_offset = 0;
	    for (j = 0; j < ctl_value; j++) {
		count += count_tmp_offsets(subformat,
					   subfield_data,
					   data_offset + element_offset);
		element_offset += item_size;
	    }
	}
	if ((subformat == NULL) && ioformat->body->var_list[i].string) {
	    /* variant array of strings */
	    count += ctl_value;
	}
    }
    return count;
}

static int
count_tmp_offset(ioformat, i, data, data_offset)
IOFormat ioformat;
int i;
void *data;
int data_offset;
{
    int count = 0;
    if ((ioformat->body->var_list[i].string == TRUE) ||
	(ioformat->body->var_list[i].var_array == TRUE)) {
	struct _IOgetFieldStruct var_field;  /* OK */
	unsigned long ptr_value;

	count += 2;		/* one for data, one for padding */

	var_field.offset = ioformat->body->field_list[i].field_offset;
	if (ioformat->body->pointer_size == 0) {
	    /* 
	     * old style file, must be a string here, use it to
	     * fill in pointer size.
	     */
	    ioformat->body->pointer_size = ioformat->body->field_list[i].field_size;
	}
	var_field.size = ioformat->body->pointer_size;
	var_field.data_type = integer_type;
	var_field.byte_swap = 0;
	ptr_value = quick_get_pointer(&var_field, (char *) data + data_offset);

	if (ptr_value != 0) {
	    count +=
		count_variant_element(ioformat, i, data, (char *) ptr_value,
				      data_offset);
	}
    } else if (ioformat->field_subformats[i] != NULL) {
	int offset = ioformat->body->field_list[i].field_offset;
	count += count_tmp_offsets(ioformat->field_subformats[i],
			   (char *) data + offset, data_offset + offset);
    }
    return count;
}

static void
make_tmp_offset(iofile, buf, ioformat, i, malloced_array_size, iovcnt,
		write_iov, record_len, tmp_offset, data_offset, copy_all)
IOFile iofile;
IOBuffer buf;
IOFormat ioformat;
int i;
int *malloced_array_size;
int *iovcnt;
internal_iovec **write_iov;
int *record_len;
int data_offset;
int tmp_offset;
int copy_all;
{
    if ((ioformat->body->var_list[i].string == TRUE) ||
	(ioformat->body->var_list[i].var_array == TRUE)) {
	struct _IOgetFieldStruct var_field;  /* OK */
	unsigned long ptr_value;

	ensure_writev_room(malloced_array_size, iovcnt, write_iov, 2);

	var_field.offset = ioformat->body->field_list[i].field_offset;
	if (ioformat->body->pointer_size == 0) {
	    /* 
	     * old style file, must be a string here, use it to
	     * fill in pointer size.
	     */
	    ioformat->body->pointer_size = ioformat->body->field_list[i].field_size;
	}
	var_field.size = ioformat->body->pointer_size;
	/* we're writing this record, so we must know all  these things
	 * statically */
	ptr_value = quick_get_pointer(&var_field, (char *) buf->tmp_buffer + data_offset);

	if (ptr_value == 0) {
	    quick_put_ulong(&var_field, (unsigned long) 0,
			    (char *) buf->tmp_buffer + data_offset);
	} else if (ptr_value == (unsigned long) *record_len) {
	    /* untranslated string pointer still in memory from IOread */
	    int orig_data = 0;
	    char *strtmp = *record_len + (char *) (long)orig_data;
	    /* 
	     * this piece of code is so old and recently untested that
	     * I expect it has no possibility of working.  Just assert
	     * FALSE 
	     */
	    assert(FALSE);
	    fill_variant_element(ioformat, buf, i, strtmp, record_len,
			      write_iov, iovcnt, tmp_offset, data_offset,
				 malloced_array_size, copy_all);
	} else {
	    int string_offset = *record_len;
	    string_offset +=
		fill_variant_element(ioformat, buf, i, (char *) ptr_value,
			       record_len, write_iov, iovcnt, tmp_offset,
				     data_offset,
				     malloced_array_size, copy_all);
	    quick_put_ulong(&var_field, (unsigned long) string_offset,
			    (char *) buf->tmp_buffer + data_offset);
	}
    } else if (ioformat->field_subformats[i] != NULL) {
	int offset = ioformat->body->field_list[i].field_offset;
	make_tmp_offsets(iofile, buf, ioformat->field_subformats[i],
		      malloced_array_size, iovcnt, write_iov, record_len,
			 tmp_offset + offset, data_offset + offset,
			 copy_all);
    }
}

static int
count_tmp_offsets(ioformat, data, data_offset)
IOFormat ioformat;
void *data;
int data_offset;
{
    int i, count = 0;
    for (i = 0; i < ioformat->body->field_count; i++) {
	long elements = 1;
	int subfield_data_offset = 0;
	if ((ioformat->field_subformats[i] != NULL) ||
	    ioformat->body->var_list[i].string ||
	    ioformat->body->var_list[i].var_array) {
	    IOVarInfoList var = &ioformat->body->var_list[i];
	    int j = 0;
	    for (j = 0; j < var->dimen_count; j++) {
		if (var->dimens[j].control_field != NULL) {
		    elements = 1;
		    break;
		} else {
		    elements *= var->dimens[j].static_size;
		}
	    }

	    for (j = 0; j < elements; j++) {
		count += count_tmp_offset(ioformat, i, data,
					  subfield_data_offset);
		subfield_data_offset += ioformat->body->field_list[i].field_size;
	    }
	}
    }
    return count;
}

static void
make_tmp_offsets(iofile, buf, ioformat, malloced_array_size, iovcnt,
		 write_iov, record_len, tmp_offset, data_offset, copy_all)
IOFile iofile;
IOBuffer buf;
IOFormat ioformat;
int *malloced_array_size;
int *iovcnt;
internal_iovec **write_iov;
int *record_len;
int tmp_offset;
int data_offset;
int copy_all;
{
    int i;
    for (i = 0; i < ioformat->body->field_count; i++) {
	long elements = 1;
	int subfield_data_offset = data_offset;
	int subfield_tmp_offset = tmp_offset;
	if ((ioformat->field_subformats[i] != NULL) ||
	    ioformat->body->var_list[i].string ||
	    ioformat->body->var_list[i].var_array) {
	    IOVarInfoList var = &ioformat->body->var_list[i];

	    int j = 0;
	    for (j = 0; j < var->dimen_count; j++) {
		if (var->dimens[j].control_field != NULL) {
		    elements = 1;
		    break;
		} else {
		    elements *= var->dimens[j].static_size;
		}
	    }

	    for (j = 0; j < elements; j++) {
		make_tmp_offset(iofile, buf, ioformat, i, malloced_array_size,
				iovcnt, write_iov, record_len,
				subfield_tmp_offset, subfield_data_offset,
				copy_all);
		subfield_data_offset += ioformat->body->field_list[i].field_size;
		subfield_tmp_offset += ioformat->body->field_list[i].field_size;
	    }
	}
    }
}

/* 
 * Broad note on alignment.
 *
 * Data alignment is a difficult topic in FFS.  On many machines, individual
 * data elements must reside at addresses that are evenly divisible by some
 * value.  Typically this data alignment value is the size of the individual
 * data item, so a 4-byte integer can reside only at addresses that are evenly
 * divisible by 4 and an 8-byte float only at addresses that are evenly
 * divisible by 8.  Structures are typically aligned according to the largest
 * alignment of any supported data type on the machine (or at least the
 * largest alignment of any data type in the structure).  In normal operation,
 * proper alignment is maintained through the operation of the compiler (in
 * laying out structures and local data offsets), malloc (in returning only
 * blocks that are properly aligned) and stack operations (in ensuring that
 * the stack doesn't become misaligned, rendering local data misaligned).
 *
 * FFS must contend with alignment issues in the placement of data on the
 * receiving machine.  However, the desire to avoid excessive data copying
 * moves much of the alignment handling to the sender.  In FFS in a file or
 * over a socket, some issues vanish because individual header bytes can be
 * read and discarded, while taking care to read base and dynamic portions of
 * messages into aligned memory.  However, in non-connected FFS, where a
 * header, length info, base message, and dynamic portion must be combined
 * into a single memory block for transmission, data alignment impacts almost
 * everything.  Connected FFS was developed first and had looser
 * requirements, so was less careful about aligment and has been left so
 * in order to maintain backward compatibility.  Non-connected FFS is
 * stricter and has many places where padding is added to maintain alignment.
 * In particular, the header is manipulated so that the base record is
 * aligned, the base record is padded so that the dynamic block is aligned,
 * and every subelement in the dynamic block is padded if necessary so that
 * the next subelement is aligned.
 * 
 * One lurking monster in the FFS implementation is that in order to avoid
 * excess copying, the sender must make some assumptions about what alignment
 * will be acceptable to the receiver.  Currently we're assuming that aligning
 * at 8 byte boundaries will be acceptable to everyone.  If data sizes get
 * bigger, we may have a bit of a mess on our hands.  Of course, if this code
 * is still in use then I'll be surprised.  (Can you say year-2000-syndrome?)
 */
extern
char *
encode_IOcontext_buffer(context, ioformat, data, buf_size)
IOContext context;
IOFormat ioformat;
void *data;
int *buf_size;
{
    IOFile iofile = (IOFile) context;
    return encode_IOcontext_bufferB(context, ioformat, &iofile->tmp, data,
				    buf_size);
}


extern
char *
encode_IOcontext_bufferB(context, ioformat, iobuffer, data, buf_size)
IOContext context;
IOFormat ioformat;
IOBuffer iobuffer;
void *data;
int *buf_size;
{
    FILE_INT record_len = ioformat->body->record_length;
    IOFile iofile = (IOFile) context;
    void *tmp_data;
    int iovcnt = 0, i;
    internal_iovec stack_iov_array[STACK_ARRAY_SIZE];
    internal_iovec *write_iov = stack_iov_array;
    int malloced_array_size = 0;
    int header_size = ioformat->body->server_ID.length;
    int align_pad;
    int data_align_pad = (8 - ioformat->body->record_length) & 0x7;

    if (ioformat->body->variant) {
	header_size += sizeof(FILE_INT);
    }
    align_pad = (8 - header_size) & 0x7;
    header_size += align_pad;
    tmp_data = make_tmp_buffer(iobuffer, record_len + header_size +
			       data_align_pad);
    memcpy(tmp_data, ioformat->body->server_ID.value, ioformat->body->server_ID.length);

    /* fill in an IOV field for the header */
    write_iov[0].iov_len = header_size;
    write_iov[0].iov_offset = 0;
    write_iov[0].iov_base = NULL;	/* offset in tmp */
    iovcnt++;

    /* copy the actual record into the output buffer */
    memcpy((char *) tmp_data + write_iov[0].iov_len, data, record_len);

    /* fill in an IOV field for the record */
    write_iov[1].iov_len = ioformat->body->record_length;
    write_iov[1].iov_offset = write_iov[0].iov_len;
    write_iov[1].iov_base = NULL;	/* offset in tmp */
    iovcnt++;

    if (ioformat->body->variant) {
	int len_align_pad = (4 - ioformat->body->server_ID.length) & 3;
	write_iov[1].iov_len += data_align_pad;
	record_len += data_align_pad;
	make_tmp_offsets(iofile, iobuffer,
			 ioformat, &malloced_array_size, &iovcnt,
			 &write_iov, &record_len,
			 write_iov[iovcnt - 1].iov_offset	/* tmp_offset 
								 */ ,
			 write_iov[iovcnt - 1].iov_offset	/* data_offset 
								 */ ,
			 1 /* copy all to buffer */ );

	/* fill in actual variant record length */
	tmp_data = iobuffer->tmp_buffer;
	tmp_data = ((char *) tmp_data) + ioformat->body->server_ID.length + len_align_pad;
	memcpy(tmp_data, &record_len, sizeof(record_len));
    }
    /* check buffer */
    for (i = 0; i < iovcnt - 1; i++) {
	if ((write_iov[i].iov_base != NULL) ||
	    (write_iov[i].iov_len + write_iov[i].iov_offset !=
	     write_iov[i + 1].iov_offset)) {
	    assert(FALSE);
	}
    }
    *buf_size = (write_iov[iovcnt - 1].iov_len + write_iov[iovcnt - 1].iov_offset);
    if (malloced_array_size != 0) {
	io_free(write_iov);
    }
    return iobuffer->tmp_buffer;
}

static
void
encode_buffer_to_vector(iofile, buf, ioformat, data, malloced_array_size,
			iovcnt, write_iov, copy_to_buffer, head_align,
			data_align)
IOFile iofile;
IOBuffer buf;
IOFormat ioformat;
void *data;
int *malloced_array_size;
int *iovcnt;
internal_iovec **write_iov;
int copy_to_buffer;
int head_align;
int data_align;
{
    if (ioformat->body->variant) {
	FILE_INT record_len = ioformat->body->record_length;
	int tmp_data_offset;
	char *tmp_data;
	int len_align_pad = (4 - ioformat->body->server_ID.length) & 3;

	ensure_writev_room(malloced_array_size, iovcnt, write_iov, 2);

	tmp_data_offset =
	    add_to_tmp_buffer(buf, record_len + head_align +
			      sizeof(record_len) + data_align);

	tmp_data = (char *) buf->tmp_buffer + tmp_data_offset;
	/* copy record into buffer */
	memcpy(tmp_data + sizeof(record_len) + head_align, data, record_len);

	if (head_align != 0) {
	    /* zero head alignment area */
	    memset(tmp_data + sizeof(record_len), 0, head_align);
	}
	if (data_align != 0) {
	    /* zero data alignment area */
	    memset(tmp_data + sizeof(record_len) + head_align + record_len,
		   0, data_align);
	}
	(*write_iov)[*iovcnt].iov_len = sizeof(record_len) + head_align;
	(*write_iov)[*iovcnt].iov_offset = tmp_data_offset;
	(*write_iov)[*iovcnt].iov_base = NULL;	/* offset in tmp */
	(*iovcnt)++;

	(*write_iov)[*iovcnt].iov_len = ioformat->body->record_length + data_align;
	(*write_iov)[*iovcnt].iov_offset = tmp_data_offset +
	    sizeof(record_len) + head_align;
	(*write_iov)[*iovcnt].iov_base = NULL;	/* offset in tmp */
	(*iovcnt)++;

	record_len += data_align;
	make_tmp_offsets(iofile, buf, ioformat, malloced_array_size, iovcnt,
			 write_iov, &record_len,
			 tmp_data_offset + sizeof(record_len) + head_align	/* tmp_offset 
										 */ ,
	/* offset of the data in tmp buffer */
			 (*write_iov)[*iovcnt - 1].iov_offset,
			 copy_to_buffer);
	/* fill in actual variant record length */
	tmp_data = (char *) buf->tmp_buffer + tmp_data_offset + len_align_pad;
	*((FILE_INT *) tmp_data) = record_len;
    } else {
	char *tmp_data;
	int tmp_data_offset;

	if (copy_to_buffer) {
	    tmp_data_offset = add_to_tmp_buffer(buf, ioformat->body->record_length);

	    tmp_data = (char *) buf->tmp_buffer + tmp_data_offset;
	    memcpy(tmp_data, data, ioformat->body->record_length);
	    (*write_iov)[*iovcnt].iov_len = ioformat->body->record_length;
	    (*write_iov)[*iovcnt].iov_offset = tmp_data_offset;
	    (*write_iov)[*iovcnt].iov_base = NULL;	/* offset in tmp */
	    (*iovcnt)++;
	} else {
	    (*write_iov)[*iovcnt].iov_len = ioformat->body->record_length;
	    (*write_iov)[*iovcnt].iov_offset = 0;	/* value in base */
	    (*write_iov)[*iovcnt].iov_base = data;
	    (*iovcnt)++;
	}
    }

}

extern IOEncodeVector
copy_vector_to_IOBuffer(IOBuffer buf, IOEncodeVector vec) {
    int vec_offset = (char *) vec - (char *)buf->tmp_buffer;

    if (((char*)vec < (char*)buf->tmp_buffer)
        || ((char*)vec >= (char*)buf->tmp_buffer + buf->tmp_buffer_size)) {
        int i;
        for (i = 0; vec[i].iov_base; ++i);
        vec_offset = add_to_tmp_buffer(buf, (i + 1) * sizeof(*vec));
        memcpy((char *) buf->tmp_buffer + vec_offset, vec, (i + 1) * sizeof(*vec));
    }

    return (IOEncodeVector) ((char *) buf->tmp_buffer + vec_offset);
}

extern IOEncodeVector
copy_all_to_IOBuffer(IOBuffer buf, IOEncodeVector vec)
{
    int i = 0;
    int vec_offset = (long) vec - (long)buf->tmp_buffer;
    /* 
     * vec and some of the buffers may be in the memory managed by the
     * IOBuffer.  The goal here to is put *everything* into the IOBuffer.
     */
    assert(((unsigned long)vec >= (unsigned long)buf->tmp_buffer) && 
	   ((unsigned long)vec < (unsigned long)buf->tmp_buffer + buf->tmp_buffer_size));
    while (vec[i].iov_base != NULL) {
	if (((char*)vec[i].iov_base >= (char*)buf->tmp_buffer) &&
	    ((char*)vec[i].iov_base < (char*)buf->tmp_buffer + buf->tmp_buffer_size)) {
	    /* 
	     * remap pointers into temp so that they're offsets (must do
	     * this before we realloc the temp
	     */ 
	    vec[i].iov_base = (void*)((char *)vec[i].iov_base - (char *)buf->tmp_buffer + 1);
	}
	i++;
    }

    i = 0;
    while (((IOEncodeVector)((long)buf->tmp_buffer + vec_offset))[i].iov_base !=
	   NULL) {
	IOEncodeVector v = (void*)((long) buf->tmp_buffer + vec_offset);
	if ((unsigned long)v[i].iov_base > buf->tmp_buffer_size) {
	    /* if this is an external buffer, copy it */
	    int offset = add_to_tmp_buffer(buf, v[i].iov_len);
	    char * data = (char *) buf->tmp_buffer + offset;
	    /* add_to_tmp_buffer() might have remapped vector */
	    v = (void*)((char *) buf->tmp_buffer + vec_offset);
	    memcpy(data, v[i].iov_base, v[i].iov_len);
	    v[i].iov_base = (void*)(long)(offset + 1);
	}
	i++;
    }
    /* reallocation done now */
    vec = (void*)((long)buf->tmp_buffer + vec_offset);
    i = 0;
    while (vec[i].iov_base != NULL) {
	if (((long)vec[i].iov_base > 0) &&
	    ((long)vec[i].iov_base <= buf->tmp_buffer_size)) {
	    /* 
	     * remap pointers into temp so that they're addresses
	     */ 
	    vec[i].iov_base = (void*)((long)vec[i].iov_base + (char *)buf->tmp_buffer - 1);
	}
	i++;
    }
    return vec;
}

extern void
dump_encodevector(IOEncodeVector vec, IOFile ioc)
{
    int i = 0;
    while (vec[i].iov_base != NULL) i++;
    printf("Vector has %d elements:\n", i);
    i = 0;
    while (vec[i].iov_base != NULL) {
	printf("base %lx, size %lx  ", (long)vec[i].iov_base, vec[i].iov_len);
	if (vec[i].iov_base == &zeroes[0]) {
	    printf("zero value\n");
	} else if (((char*)vec[i].iov_base > (char*)ioc->tmp.tmp_buffer) &&
		   ((char*)vec[i].iov_base < (char*)ioc->tmp.tmp_buffer + ioc->tmp.tmp_buffer_size)) {
	    printf("Tmp value\n");
	} else {
	    printf("Extern value\n");
	}
	i++;
    }
}

static
int
count_buffer_to_vector(ioformat, data)
IOFormat ioformat;
void *data;
{
    int count = 0;
    if (ioformat->body->variant) {
	count += 2;

	count += count_tmp_offsets(ioformat, data, 0);
    } else {
	count++;
    }
    return count;
}

static IOEncodeVector
internal_encode_to_vector(iocontext, buf, ioformat, data)
IOContext iocontext;
IOBuffer buf;
IOFormat ioformat;
void *data;
{
    int i;
    int iovcnt = 0;
    int output_len;
    IOFile iofile = (IOFile) iocontext;
    internal_iovec stack_iov_array[STACK_ARRAY_SIZE];
    internal_iovec *write_iov = stack_iov_array;
    int malloced_array_size = 0;
    IOEncodeVector ret_val;
    int tmp_data;
    int header_size = ioformat->body->server_ID.length;
    int data_align_pad = /*(8 - ioformat->body->record_length) & 0x7*/ 0;
    int align_pad;
    int vec_count;

    if (iofile->status == Closed)
	return NULL;

    if (iofile != ioformat->context) {
	iofile->result = "format not associated with file";
	return 0;
    }
    vec_count = count_buffer_to_vector(ioformat, data);
    vec_count++;		/* header */
    vec_count++;		/* null at the end */

    /* reserve area at the beginning of the tmp_buffer for vector list */
    (void) make_tmp_buffer(buf, vec_count * sizeof(write_iov[0]));

    if (ioformat->body->variant) {
	header_size += sizeof(FILE_INT);
    }
    align_pad = (8 - header_size) & 0x7;
    header_size += align_pad;
    if (ioformat->body->variant) {
	/* 
	 *  remove header size and alignment again because 
	 *  encode_buffer_to_vector will add them back.
	 */
	header_size -= (sizeof(FILE_INT) + align_pad);
    }
    tmp_data = add_to_tmp_buffer(buf, header_size);
    memcpy((char *) buf->tmp_buffer + tmp_data, ioformat->body->server_ID.value,
	   ioformat->body->server_ID.length);

    /* fill in an IOV field for the header */
    write_iov[0].iov_len = header_size;
    write_iov[0].iov_offset = tmp_data;
    write_iov[0].iov_base = NULL;	/* offset in tmp */
    iovcnt++;

    ensure_writev_room(&malloced_array_size, &iovcnt, &write_iov, 1);

    encode_buffer_to_vector(iofile, buf, ioformat, data,
			    &malloced_array_size, &iovcnt, &write_iov,
			    0 /* don't copy to buffer */ ,
			    align_pad, data_align_pad);

    ret_val = buf->tmp_buffer;

    /* fill in vector */
    output_len = 0;
    for (i = 0; i < iovcnt; i++) {
	ret_val[i].iov_len = write_iov[i].iov_len;
	output_len += write_iov[i].iov_len;
	if (write_iov[i].iov_base == NULL) {
	    ret_val[i].iov_base = (char *) buf->tmp_buffer + write_iov[i].iov_offset;
	} else {
	    ret_val[i].iov_base = write_iov[i].iov_base;
	}
    }
    ret_val[iovcnt].iov_len = 0;
    ret_val[iovcnt].iov_base = NULL;

    /* update stats */
    iofile->stats.encode_bytes += output_len;
    iofile->stats.encode_msg_count++;

    if (malloced_array_size != 0) {
	io_free(write_iov);
    }
    return ret_val;
}

/* 
 * the difference between encode_IOcontext_to_vector() and 
 * encode_IOcontext_to_vector_tmp() is that encode_IOcontext_to_vector() 
 * leaves data in temporary storage associated with the IOContext value.
 * This data is valid only until the next operation on the IOcontext.
 * the encode_IOcontext_to_vector_tmp() version does not leave data with the
 * IOcontext.  While the data is built in a temporary area, a pointer to 
 * that memory is returned with the tmp_block_p out parameter and the 
 * IOContext does not retain a pointer.  The caller is responsible for 
 * eventually freeing() the tmp_block value.
 */
/* 
 *  This version is deprecated.  Use encode_IOcontext_release_vector instead.
 */
extern IOEncodeVector
encode_IOcontext_to_vector_tmp(iocontext, ioformat, data, tmp_block_p)
IOContext iocontext;
IOFormat ioformat;
void *data;
void **tmp_block_p;
{
    IOEncodeVector ret_val;
    ret_val = internal_encode_to_vector(iocontext, &((IOFile) iocontext)->tmp,
					ioformat, data);

    /* make the iocontext release the temporary buffer */
    ((IOFile) iocontext)->tmp.tmp_buffer = NULL;
    ((IOFile) iocontext)->tmp.tmp_buffer_size = 0;
    *tmp_block_p = ret_val;
    return ret_val;
}

extern IOEncodeVector
encode_IOcontext_to_vector(iocontext, ioformat, data)
IOContext iocontext;
IOFormat ioformat;
void *data;
{
    return internal_encode_to_vector(iocontext, &((IOFile) iocontext)->tmp,
				     ioformat, data);
}

extern IOEncodeVector
encode_IOcontext_release_vector(iocontext, ioformat, data)
IOContext iocontext;
IOFormat ioformat;
void *data;
{
    IOEncodeVector ret_val;
    ret_val = internal_encode_to_vector(iocontext, &((IOFile) iocontext)->tmp,
					ioformat, data);

    /* make the iocontext release the temporary buffer */
    ((IOFile) iocontext)->tmp.tmp_buffer = NULL;
    ((IOFile) iocontext)->tmp.tmp_buffer_size = 0;
    return ret_val;
}

extern IOEncodeVector
encode_IOcontext_vectorB(iocontext, buffer, ioformat, data)
IOContext iocontext;
IOFormat ioformat;
IOBuffer buffer;
void *data;
{
    IOEncodeVector ret_val;
    ret_val = internal_encode_to_vector(iocontext, buffer, ioformat, data);

    return ret_val;
}

extern
int
write_IOheaders(iofile)
IOFile iofile;
{
    int index;
    char write_block[24];
    internal_iovec write_vec[3];
    FILE_INT magic_number;
    FILE_INT IOformat;
    FILE_INT double_len;
    double magic_float;
    FILE_INT pointer_size;

    magic_float = MAGIC_FLOAT;
    magic_number = MAGIC_NUMBER;
    IOformat = iofile->IOversion;
    double_len = sizeof(double);
    pointer_size = sizeof(char *);
    if (!iofile->architecture)
	iofile->architecture = io_strdup(architecture);

    assert(sizeof(int) == 4);	/* otherwise must do other stuff */

    *((int *) (&write_block)) = magic_number;
    *((int *) (&write_block[0] + 4)) = IOformat;
    memcpy(&write_block[0] + 8, &double_len, sizeof(double_len));
    memcpy(&write_block[0] + 12, &magic_float, sizeof(magic_float));
    *((int *) (&write_block[0] + 12 + sizeof(double))) = pointer_size;

    write_vec[0].iov_base = iofile->architecture;
    write_vec[0].iov_len = strlen(iofile->architecture) + 1;
    write_vec[0].iov_offset = 0;
    write_vec[1].iov_base = &write_block[0];
    write_vec[1].iov_len = 16 + sizeof(double);
    write_vec[1].iov_offset = 0;

    if (AtomicWriteV(iofile, &write_vec[0], 2) != 2)
	return 0;

    for (index = 0; index < iofile->reg_format_count; index++) {
	if (!write_IOformat(iofile, iofile->format_list[index]))
	    return 0;
    }

    iofile->status = OpenHeader;
    return 1;
}

static
int
read_IOheaders(iofileptr)
IOFile *iofileptr;
{
    IOFile iofile = *iofileptr;
    char arch[128];
    FILE_INT arch_len = 0;
    FILE_INT object_len;
    FILE_INT magic_number = 0;
    char float_magic[32];

    arch[0] = 0;

    if (AtomicRead(iofile->file_id, &arch[arch_len], 1, iofile) != 1) {
	perror("Read failed while reading FFS headers");
	return 0;
    }
    while ((arch[arch_len] != 0) && (arch_len < 100)) {
	AtomicRead(iofile->file_id, &arch[++arch_len], 1, iofile);
    }
    arch[arch_len] = 0;

    iofile->architecture = (char *) malloc(arch_len + 1);
    strcpy(iofile->architecture, arch);

    iofile->IOversion = -1;
    /* read the length of the magic number */
    if (get_AtomicInt(iofile, &magic_number) == 0) {
	perror("Read failed while reading FFS headers");
	return 0;
    }
    if ((magic_number != MAGIC_NUMBER) &&
	(magic_number != REVERSE_MAGIC_NUMBER)) {
	if (((magic_number - 1) == MAGIC_NUMBER) ||
	    ((magic_number - 0x1000000) == REVERSE_MAGIC_NUMBER)) {
	    iofile->IOversion = 0;
	} else {
	    fprintf(stderr, "This is not an IO file.  Magic number mismatch\n");
	    return 0;
	}
    }
    iofile->byte_reversal = ((magic_number == REVERSE_MAGIC_NUMBER) ||
		   ((magic_number - 0x1000000) == REVERSE_MAGIC_NUMBER));
    if (iofile->IOversion != 0) {
	FILE_INT version;
	get_AtomicInt(iofile, &version);
	iofile->IOversion = version;
    }
    if (iofile->IOversion > CURRENT_IO_VERSION) {
	fprintf(stderr, "IO file is newer than library!  Version mismatch!\n");
	return 0;
    }
    get_AtomicInt(iofile, &object_len);
    if (object_len > sizeof(float_magic)) {
	fprintf(stderr, "Sizeof Float magic in file is out of bounds! value =%d\n", 
		object_len);
	return 0;
    }
    AtomicRead(iofile->file_id, &float_magic[0], object_len, iofile);
    iofile->native_float_format = FFSinfer_float_format((char*)&float_magic, object_len);
    if (iofile->native_float_format == Format_Unknown) {
	fprintf(stderr, "Unsupported floating point format in input file\n");
	return 0;
    }

    if (iofile->IOversion == 1) {
	FILE_INT format_count;
	FILE_INT max_record_len;
	int index;

	/* read old stuff */
	get_AtomicInt(iofile, &format_count);
	get_AtomicInt(iofile, &max_record_len);
	while (iofile->format_list_size < format_count) {
	    expand_IOFile(iofile);
	}

	for (index = 0; index < format_count; index++) {
	    iofile->format_list[index] = read_format_IOfile(iofile);
	}
	iofile->status = OpenForRead;

    } else {
	iofile->reg_format_count = 0;

	iofile->status = OpenBeginRead;
	if (iofile->IOversion >= 4) {
	    /* the size of pointers in this file */
	    get_AtomicInt(iofile, &iofile->native_pointer_size);
	}
    }
    if (iofile->IOversion <= 1) {
	int i;
	IOFormat vec_ioformat = NULL;
	for (i = 0; i < iofile->reg_format_count; i++) {
	    IOFormat ioformat = iofile->format_list[i];
	    int j;
	    if (strcmp(ioformat->body->format_name, "R3vector") == 0) {
		vec_ioformat = ioformat;
	    }
	    for (j = 0; j < ioformat->body->field_count; j++) {
		if (strcmp(ioformat->body->field_list[j].field_type,
			   "R3vector") == 0) {
		    if (vec_ioformat == NULL) {
			/* 
			 * Truely old IOfiles had R3vector as an atomic
			 * data type.  This supported some apps before
			 * we had nested structures.  Add R3vector to
			 * the list of formats.  
			 */
			IOField R3vector_field_list[4];
			vec_ioformat = new_IOFormat();
			R3vector_field_list[0].field_name = "x";
			R3vector_field_list[0].field_type = "double";
			R3vector_field_list[0].field_size = object_len;
			R3vector_field_list[0].field_offset = 0;
			R3vector_field_list[1].field_name = "y";
			R3vector_field_list[1].field_type = "double";
			R3vector_field_list[1].field_size = object_len;
			R3vector_field_list[1].field_offset = object_len;
			R3vector_field_list[2].field_name = "z";
			R3vector_field_list[2].field_type = "double";
			R3vector_field_list[2].field_size = object_len;
			R3vector_field_list[2].field_offset = object_len * 2;
			R3vector_field_list[3].field_name = NULL;
			R3vector_field_list[3].field_type = NULL;
			R3vector_field_list[3].field_size = 0;
			R3vector_field_list[3].field_offset = 0;

			vec_ioformat->body->format_name = io_strdup("R3vector");
			vec_ioformat->body->field_count = 3;
			vec_ioformat->body->variant = FALSE;
			vec_ioformat->body->record_length = object_len * 3;
			vec_ioformat->conversion = NULL;
			vec_ioformat->body->field_list =
			    copy_field_list(R3vector_field_list);
			vec_ioformat->body->var_list = NULL;
			vec_ioformat->body->byte_reversal = iofile->byte_reversal;
			vec_ioformat->body->float_format = iofile->native_float_format;
			vec_ioformat->body->column_major_arrays = iofile->native_column_major_arrays;
			vec_ioformat->body->server_format_rep = NULL;
			generate_var_list(vec_ioformat, NULL);
			expand_IOFile(iofile);
			iofile->format_list[iofile->reg_format_count++] =
			    vec_ioformat;
			vec_ioformat->format_id = iofile->reg_format_count - 1;
			vec_ioformat->context = iofile;
		    }
		    ioformat->field_subformats[j] = vec_ioformat;
		}
	    }
	}
    }
    return 1;
}

int
write_comment_IOfile(iofile, comment)
IOFile iofile;
const char *comment;
{
    FILE_INT code = COMMENT_FOLLOWS;
    FILE_INT length;

    if (iofile->status == Closed)
	return -1;

    if (!comment)
	return 0;

    length = strlen(comment);
    if (!put_AtomicInt(iofile, &code))
	return 0;
    if (!put_AtomicInt(iofile, &length))
	return 0;
    if (AtomicWrite(iofile->file_id, (void*)comment, length, iofile) 
	== length)
	return 1;
    return 0;
}

extern
char *
read_comment_IOfile(iofile)
IOFile iofile;
{
    FILE_INT string_size;
    char *tmp;
    if ((iofile->status != OpenForRead) && (iofile->status != OpenBeginRead))
	return NULL;

    if (next_IOrecord_type(iofile) != IOcomment)
	return NULL;

    if (!get_AtomicInt(iofile, &string_size))
	return 0;

    tmp = make_tmp_buffer(&iofile->tmp, string_size + 1);

    if (tmp == NULL) {
	iofile->result = "Comment too large for remaining memory";
	return NULL;
    }
    if (AtomicRead(iofile->file_id, tmp, string_size, iofile) < string_size)
	return NULL;

    tmp[string_size] = 0;

    reset_read_ahead(iofile);
    return tmp;
}

#ifndef MAX_IOV
#define MAX_IOV 16
#endif

extern
IOFile
new_IOFile()
{
    IOFile iofile;
    init_float_formats();
    iofile = (IOFile) malloc((size_t) sizeof(IOContextStruct));
    iofile->format_list_size = 0;
    iofile->format_list = NULL;
    iofile->reg_format_count = 0;
    iofile->file_id = NULL;
    iofile->tmp.tmp_buffer = NULL;
    iofile->tmp.tmp_buffer_size = 0;
    iofile->tmp.tmp_buffer_in_use_size = 0;
    iofile->byte_reversal = 0;
    iofile->architecture = (char *) 0;
    iofile->native_float_format = ffs_my_float_format;
    iofile->native_column_major_arrays = 0;
    iofile->native_pointer_size = sizeof(char*);
    iofile->errno_val = 0;
    iofile->result = NULL;
    memset(&iofile->stats, 0, sizeof(iofile->stats));
    
    iofile->is_context = 0;
    iofile->master_context = NULL;
    iofile->server_callback = NULL;
    iofile->server_get_port_callback = NULL;
    iofile->server_client_data = NULL;
    iofile->server_fd = (char *) -1;
    iofile->server_pid = 0;
    iofile->server_byte_reversal = 0;
    iofile->write_func = os_write_func;
    iofile->read_func = os_read_func;
    iofile->max_iov = MAX_IOV;
    iofile->iov = malloc(iofile->max_iov * sizeof(struct iovec));
    iofile->writev_func = os_writev_func;
    iofile->readv_func = os_readv_func;
    iofile->close_func = os_close_func;
    iofile->poll_func = os_poll_func;

    return (iofile);
}

extern void
dump_IOFile(iofile)
IOFile iofile;
{
    int index;
    printf("IOFile version %d: Status = ", iofile->IOversion);
    switch (iofile->status) {
    case OpenNoHeader:
	printf("OpenNoHeader");
	break;
    case OpenHeader:
	printf("OpenHeader");
	break;
    case OpenForRead:
	printf("OpenForRead");
	break;
    case OpenBeginRead:
	printf("OpenBeginRead");
	break;
    case Closed:
	printf("Closed");
	break;
    }
    if (iofile->architecture != NULL) {
	printf("; Architecture = %s", iofile->architecture);
    }
    printf("; Format List Size = %d;\n", iofile->reg_format_count);
    for (index = 0; index < iofile->reg_format_count; index++) {
	dump_IOFormat(iofile->format_list[index]);
    }
}


extern void
dump_IOFile_as_XML(iofile)
IOFile iofile;
{
    int index;
    printf("<IOFile version=\"%d\" status=\"", iofile->IOversion);
    switch (iofile->status) {
    case OpenNoHeader:
	printf("OpenNoHeader");
	break;
    case OpenHeader:
	printf("OpenHeader");
	break;
    case OpenForRead:
	printf("OpenForRead");
	break;
    case OpenBeginRead:
	printf("OpenBeginRead");
	break;
    case Closed:
	printf("Closed");
	break;
    }
    printf("\"");

    if (iofile->architecture != NULL) {
	printf("architecture=\"%s\"", iofile->architecture);
    }
    printf(">\n");
    printf("; Format List Size = %d;\n", iofile->reg_format_count);
    for (index = 0; index < iofile->reg_format_count; index++) {
	dump_IOFormat_as_XML(iofile->format_list[index]);
    }

    printf("</IOFile>\n");
}


extern int
IOhas_error(iofile)
IOFile iofile;
{
    return ((iofile->result != NULL) || (iofile->errno_val != 0));
}

extern void
IOperror(iofile, str)
IOFile iofile;
char *str;
{
    if (str == NULL) {
	str = "";
    }
    if (iofile == NULL) {
	fprintf(stderr, "%s: NULL IOfile\n", str);
    }
    if (iofile->errno_val == 0) {
	if (iofile->result != NULL) {
	    fprintf(stderr, "%s: %s\n", str, iofile->result);
	} else {
	    fprintf(stderr, "%s: No Error\n", str);
	}
    } else {
	/* not multi thread safe! */
	int tmp = errno;
	errno = iofile->errno_val;
	perror(str);
	errno = tmp;
    }
}

extern IOContext_stats
get_IOstats(IOContext iocontext)
{
    IOContext_stats ret = malloc(sizeof(struct _IOContext_stats));
    memcpy(ret, &((IOFile)iocontext)->stats, sizeof(struct _IOContext_stats));
    ret->formats_present_count = ((IOFile)iocontext)->reg_format_count;
    return ret;
}

extern void
print_IOstats(IOContext_stats stats)
{
    printf("FFS/JIX statistics:\n");
    printf(" bytes decoded/read : 	%d\n", stats->decode_bytes);
    printf(" messages decoded/read : 	%d\n", stats->decode_msg_count);
    printf(" bytes encoded/written : 	%d\n", stats->encode_bytes);
    printf(" messages encoded/written :	%d\n", stats->encode_msg_count);
    printf(" currently available format count :		%d\n",
	   stats->formats_present_count);
    printf(" number of formats registered :	%d\n",
	   stats->format_registration_count);
    printf(" number of external formats fetched :	%d\n",
	   stats->format_fetch_count);
    printf(" format bytes written to server (or file):	%d\n",
	   stats->to_format_server_bytes);
    printf(" format bytes read from server (or file):	%d\n",
	   stats->from_format_server_bytes);
    if (stats->last_server_time == 0) {
	printf(" no format server activity\n");
    } else {
	printf(" last format server action at %s\n", 
	       ctime((time_t *)&stats->last_server_time));
    }
}

#ifdef __LIBCATAMOUNT__

extern 
short bswap_16(short s)
{
	short m = s;
	byte_swap(&m, sizeof(s));
	return m;
}

extern 
int bswap_32(int s)
{
	int m = s;
	byte_swap(&m, sizeof(s));
	return m;
}
#endif

/* 
 * See the comments at the head of io_malloc.c for how this stuff lets
 * the user choose between system malloc() and Cthreads memory_alloc(). 
 */
void *(*io_malloc_routine) () = NULL;
void *(*io_realloc_routine) () = NULL;
void (*io_free_routine) () = NULL;

void *
io_malloc(size)
int size;
{
    if (io_malloc_routine != NULL) {
	return (*io_malloc_routine) (size);
    } else {
	return malloc(size);
    }
}

extern void *
io_realloc(ptr, size)
void *ptr;
int size;
{
    if (io_realloc_routine != NULL) {
	return (*io_realloc_routine) (ptr, size);
    } else {
	return realloc(ptr, size);
    }
}

extern void
io_free(ptr)
void *ptr;
{
    if (io_free_routine != NULL) {
	(*io_free_routine) (ptr);
    } else {
	free(ptr);
    }
}

char *
io_strdup(str)
const char *str;
{
    int len = strlen(str);
    char *tmp = malloc(len + 1);
    strcpy(tmp, str);
    return tmp;
}
#endif

/* 
 * inputs:
 *  1.  maybe --> where data is now, base & variant
 *  2.  where we want data : base
 *  3.  should variant be buffered or go with base?
 *  5.  input_record_len
 *  
 * outputs:
 *  1.  where data should be read into: base & variant
 *              - or -
 *      if and to where data should be moved: base & variant
 *  2.  source address for conversion
 *  3.  dest address for conversion
 *  4.  src string address
 *  5.  final string address
 *  6.  base_string_offset
 */
typedef struct _conversion_action {
    char *cur_base;		/* where record is now (NULL if not in
				 * mem) */
    char *cur_variant;		/* where variant part is now (NULL if not
				 * in mem) */
    char *final_base;		/* where record should go (NULL if tmp
				 * buffer) */
    int variant_with_base;	/* true if variant should end up contig
				 * w/base */

/* outputs */
/* iovector read_iov[2]; int    read_icount; */
    void *src_address;
    void *dest_address;
    void *src_string_address;
    void *final_string_address;
    int base_string_offset;
} conversion_action, *conversion_action_ptr;

static int
in_place_base_conversion_possible(conv)
IOConversionPtr conv;
{
    switch (conv->conversion_type) {
    case buffer_and_convert:
	/* 
	 * Buffer_and_convert differs from Copy_Strings in that it 
	 * means no size-changing conversions need be done on the
	 * variant part of the record.
	 */
	return FALSE;		/* no in-place conversion */
    case copy_dynamic_portion:
	/* 
	 * Copy_Strings differs from Buffer_and_convert in that it 
	 * means size-changing conversions are required for the
	 * variant part of the record.  
	 */
	return FALSE;		/* no in-place conversion */
    case none_required:
    case direct_to_mem:
	return TRUE;
    default:
	assert(FALSE);
    }
    /* NOTREACHED */
    return FALSE;
}

static int
in_place_variant_conversion_possible(conv)
IOConversionPtr conv;
{
    switch (conv->conversion_type) {
    case copy_dynamic_portion:
	/* 
	 * Copy_Strings differs from Buffer_and_convert in that it 
	 * means size-changing conversions are required for the
	 * variant part of the record.  
	 */
	return FALSE;		/* no in-place conversion */
    case buffer_and_convert:
	/* 
	 * Buffer_and_convert differs from Copy_Strings in that it 
	 * means no size-changing conversions need be done on the
	 * variant part of the record.
	 */
	return TRUE;		/* yes, variant in-place conversion is
				 * possible */
    case none_required:
    case direct_to_mem:
	return TRUE;
    default:
	assert(FALSE);
    }
    /* NOTREACHED */
    return FALSE;
}

extern int
decode_in_place_possible(format)
FFSTypeHandle format;
{
    if (format->conversion != NULL) {
	if (format->body->variant) {
	    return in_place_base_conversion_possible(format->conversion) &&
		in_place_variant_conversion_possible(format->conversion);
	} else {
	    return in_place_base_conversion_possible(format->conversion);
	}
    } else {
	return 0;
    }
}

#define expand_size_to_align(size) ((((size) & 0x7) == 0) ? (size) : (((size) + 8) & (int) -8))

static void
set_conversion_params(ioformat, input_record_len, conv, params)
FFSTypeHandle ioformat;
int input_record_len;
IOConversionPtr conv;
conversion_action_ptr params;
{
    FFSContext c = ioformat->context;
    int final_base_size;
    int src_base_size;
    int possible_converted_variant_size;
    int orig_variant_size;

    int dest_offset;
    void *dest_address;
    int src_offset;
    void *src_address;
    int src_string_offset;
    void *src_string_address;
    int final_string_offset;
    void *final_string_address;

    final_base_size = expand_size_to_align(ioformat->body->record_length + conv->base_size_delta);
    src_base_size = expand_size_to_align(ioformat->body->record_length);
    possible_converted_variant_size =
	final_variant_size_for_record(input_record_len, conv);
    orig_variant_size = input_record_len - expand_size_to_align(ioformat->body->record_length);

    make_tmp_buffer(&c->tmp, 0);
    /* set base dest values */
    if (params->final_base == NULL) {
	/* need memory for at least the base record in temp area */
	int buffer_required = Max(final_base_size, src_base_size);
	dest_offset = add_to_tmp_buffer(&c->tmp, buffer_required);
	dest_address = NULL;

    } else {
	dest_address = params->final_base;
	dest_offset = 0;
    }

    /* set base src values */
    if (in_place_base_conversion_possible(conv)) {
	/* 
	 * we can convert in place, so just use the already determined 
	 * destination for the src values
	 */
	src_offset = dest_offset;
	src_address = dest_address;
    } else {
	/* no converting in place, so source must be different than base */
	if ((params->cur_base == NULL) ||
	    (params->cur_base == (char *) dest_address)) {
	    /* 
	     * either the source is not in memory or it is the same as 
	     * where we want the record to end up.  Need temporary space.
	     */
	    int source_base_size = expand_size_to_align(ioformat->body->record_length);
	    src_offset = add_to_tmp_buffer(&c->tmp, source_base_size);
	    src_address = NULL;
	} else {
	    src_offset = 0;
	    src_address = params->cur_base;
	}
    }

    /* set final string address */
    if ((params->final_base == NULL) || (!params->variant_with_base)) {
	/* 
	 * either they didn't specified a specific base address, or they said
	 * to put the variant in temporary memory.  Either way, we need
	 * temporary memory for the final variant part.
	 *
	 * NOTE: the record length may have provided padding essential to the
	 * alignment of variant arrays in the string area.  Get a little extra
	 * memory so we can modify the base if necessary.
	 */
	int align_pad = (8 - ioformat->body->record_length) & 0x7;
	int buffer_required = Max(possible_converted_variant_size + align_pad,
				  orig_variant_size + align_pad);
	buffer_required = expand_size_to_align(buffer_required);
	final_string_offset = add_to_tmp_buffer(&c->tmp, buffer_required);
	final_string_address = NULL;
    } else {
	final_string_offset = 0;
	final_string_address =
	    params->final_base + expand_size_to_align(ioformat->body->record_length +
							  conv->base_size_delta);
    }

    /* set variant src values */
    if (in_place_variant_conversion_possible(conv)) {
	/* 
	 * we can convert in place, so just use the already determined 
	 * destination for the src values
	 */
	src_string_offset = final_string_offset;
	src_string_address = final_string_address;
    } else {
	/* no converting in place, so source must be different than base */
	if ((params->cur_variant == NULL) ||
	    (params->cur_base == (char *) dest_address)) {
	    /* 
	     * either the source is not in memory or it is the same as 
	     * where we want the record to end up.  Need temporary space.
	     */
	    int source_variant_size =	/* plus possible alignment of 8 */
		input_record_len - ioformat->body->record_length + 8;
	    src_string_offset = add_to_tmp_buffer(&c->tmp, source_variant_size);
	    src_string_address = NULL;
	} else {
	    src_string_offset = 0;
	    src_string_address = params->cur_variant;
	}
    }
    if (dest_address == NULL) {
	params->dest_address = (char *) c->tmp.tmp_buffer + dest_offset;
    } else {
	params->dest_address = dest_address;
    }
    if (src_address == NULL) {
	params->src_address = (char *) c->tmp.tmp_buffer + src_offset;
    } else {
	params->src_address = src_address;
    }
    if (final_string_address == NULL) {
	params->final_string_address = (char *) c->tmp.tmp_buffer + final_string_offset;
    } else {
	params->final_string_address = final_string_address;
    }
    if (src_string_address == NULL) {
	params->src_string_address = (char *) c->tmp.tmp_buffer + src_string_offset;
    } else {
	params->src_string_address = src_string_address;
    }
}

static int
final_variant_size_for_record(input_record_len, conv)
int input_record_len;
IOConversionPtr conv;
{
    return (int) ((input_record_len - conv->ioformat->body->record_length)
		  * conv->max_var_expansion);
}

extern int
FFS_est_decode_length(context, src, record_length)
FFSContext context;
char *src;
int record_length;
{
    FFSTypeHandle ioformat = FFSTypeHandle_from_encode(context, src);
    IOConversionPtr conv;
    int variant_part, final_base_size, src_base_size;

    if (ioformat == NULL)
	return -1;
    conv = ioformat->conversion;
    if (!ioformat->conversion)
	return record_length;
    variant_part = final_variant_size_for_record(record_length,
						 ioformat->conversion);
    final_base_size = expand_size_to_align(ioformat->body->record_length +
					   conv->base_size_delta);
    src_base_size = expand_size_to_align(ioformat->body->record_length);
    return variant_part + Max(final_base_size, src_base_size);
}

extern int
FFSheader_size(ioformat)
FFSTypeHandle ioformat;
{
    int header_size = ioformat->body->server_ID.length;
    int align_pad;
    if (ioformat->body->variant) {
	header_size += sizeof(FILE_INT);
    }
    align_pad = (8 - header_size) & 0x7;
    return header_size + align_pad;
}

extern
int
FFShas_conversion(ioformat)
FFSTypeHandle ioformat;
{
    return (ioformat->conversion != NULL);
}

static int
FFSinternal_decode(ioformat, src, dest, to_buffer)
FFSTypeHandle ioformat;
char *src;			/* incoming data to be decoded */
void *dest;			/* area to hold decoded data */
int to_buffer;
{
    FFSContext iofile = ioformat->context;
    IOConversionPtr conv;
    int input_record_len;
    IOConversionStruct null_conv;
    int align_pad;
    int header_size = FFSheader_size(ioformat);
    int data_align_pad;
    conversion_action params;

    conv = ioformat->conversion;

    if (conv == NULL) {
	null_conv.conversion_type = none_required;
	null_conv.max_var_expansion = 1.0;
	null_conv.conv_count = 0;
	null_conv.base_size_delta = 0;
	null_conv.context = iofile;
	null_conv.ioformat = ioformat;
	null_conv.native_field_list = NULL;
	conv = &null_conv;
    }
    if (iofile != conv->context) {
	fprintf(stderr, "IOFile and conversion mismatch\n");
	return -1;
    }
    if (ioformat == NULL)
	return 0;

    if (ioformat->body->variant) {
	FILE_INT record_len;
	int len_align_pad = (4 - ioformat->body->server_ID.length) & 3;
	FILE_INT *len_ptr = (FILE_INT *) (src + ioformat->body->server_ID.length +
					  len_align_pad);
	memcpy(&record_len, len_ptr, sizeof(FILE_INT));
	if (ioformat->body->byte_reversal)
	    byte_swap((char *) &record_len, 4);
	input_record_len = record_len;
    } else {
	input_record_len = ioformat->body->record_length;
    }
    align_pad = (8 - header_size) & 0x7;
    header_size += align_pad;
    data_align_pad = (8 - ioformat->body->record_length) & 0x7;

    params.cur_base = src + header_size;
    params.cur_variant = params.cur_base + ioformat->body->record_length +
	data_align_pad;
    params.final_base = dest;
    params.variant_with_base = to_buffer;
    set_conversion_params(ioformat, input_record_len, conv, &params);

    /* update stats 
    iofile->stats.decode_bytes += input_record_len;
    iofile->stats.decode_msg_count++;*/

    if (params.src_address != params.cur_base) {
	memcpy(params.src_address, params.cur_base, ioformat->body->record_length);
    }
    if (params.src_string_address != params.cur_variant) {
	if (input_record_len - ioformat->body->record_length - data_align_pad > 0) {
	    memcpy(params.src_string_address, params.cur_variant,
		   input_record_len - ioformat->body->record_length - data_align_pad);
	}
    }
    if (conv->conversion_type != none_required) {
	FFSconvert_record(conv, params.src_address, params.dest_address,
			 params.final_string_address,
			 params.src_string_address);
    }
    return 1;
}

static int
check_conversion(ioformat)
FFSTypeHandle ioformat;
{
    if (ioformat->conversion == NULL) {
	if (ioformat->warned_about_null_conversion == 0) {
	    fprintf(stderr, "FFS Warning:  Attempting to decode when no conversion has been set.  \n  Record is of type \"%s\", ioformat 0x%lx.\n  No data returned.\n",
		    ioformat->body->format_name, (long) ioformat);
	    ioformat->warned_about_null_conversion = 1;
	}
	return 0;
    }
    return 1;
}

extern int
FFSdecode(iocontext, src, dest)
FFSContext iocontext;
char *src;			/* incoming data to be decoded */
char *dest;			/* area to hold decoded data */
{
    FFSTypeHandle ioformat;
    ioformat = FFSTypeHandle_from_encode(iocontext, src);
    if (!check_conversion(ioformat)) {
	return 0;
    }
    return FFSinternal_decode(ioformat, src, dest, 0);
}

extern int
FFSdecode_in_place(iocontext, src, dest_ptr)
FFSContext iocontext;
char *src;			/* incoming data to be decoded */
void **dest_ptr;		/* area to hold pointer to decoded data */
{
    FFSTypeHandle ioformat = FFSTypeHandle_from_encode(iocontext, src);
    int header_size;
    int ret;

    if (ioformat == NULL) {
	return 0;
    }
    if (!check_conversion(ioformat)) {
	*dest_ptr = NULL;
	return 0;
    }
    header_size = FFSheader_size(ioformat);
    ret = FFSinternal_decode(ioformat, src, src + header_size, 1);
    *dest_ptr = src + header_size;
    return ret;
}

extern int
FFSdecode_to_buffer(iocontext, src, dest)
FFSContext iocontext;
char *src;			/* incoming data to be decoded */
void *dest;			/* area to hold decoded data */
{
    FFSTypeHandle ioformat;
    ioformat = FFSTypeHandle_from_encode(iocontext, src);
    if (!check_conversion(ioformat)) {
	return 0;
    }
    return FFSinternal_decode(ioformat, src, dest, 1);
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

char *
FFSTypeHandle_name(FFSTypeHandle f)
{
    return f->body->format_name;
}

#define TMP_BUFFER_INIT_SIZE 1024

FFSBuffer
create_FFSBuffer()
{
    FFSBuffer buf = malloc(sizeof(struct FFSBuffer));
    buf->tmp_buffer = NULL;
    buf->tmp_buffer_size = 0;
    buf->tmp_buffer_in_use_size = 0;
    return buf;
}

void
free_FFSBuffer(buf)
FFSBuffer buf;
{
    if (buf->tmp_buffer)
	free(buf->tmp_buffer);
    free(buf);
}

static
char *
make_tmp_buffer(buf, size)
FFSBuffer buf;
int size;
{
    if (buf->tmp_buffer_size == 0) {
	int tmp_size = Max(size, TMP_BUFFER_INIT_SIZE);
	buf->tmp_buffer = malloc(tmp_size);
	buf->tmp_buffer_size = tmp_size;
    }
    if (size > buf->tmp_buffer_size) {
	buf->tmp_buffer = realloc(buf->tmp_buffer, size);
	if (buf->tmp_buffer) {
	    buf->tmp_buffer_size = size;
	} else {
	    buf->tmp_buffer_size = 0;
	}
    }
    buf->tmp_buffer_in_use_size = size;
    return buf->tmp_buffer;
}

static
int
add_to_tmp_buffer(buf, size)
FFSBuffer buf;
int size;
{
    int old_size = buf->tmp_buffer_in_use_size;
    size += old_size;

    if (buf->tmp_buffer_size == 0) {
	int tmp_size = Max(size, TMP_BUFFER_INIT_SIZE);
	buf->tmp_buffer = malloc(tmp_size);
    }
    if (size > buf->tmp_buffer_size) {
	buf->tmp_buffer = realloc(buf->tmp_buffer, size);
	buf->tmp_buffer_size = size;
    }
    if (!buf->tmp_buffer) {
	buf->tmp_buffer_size = 0;
    } else {
	buf->tmp_buffer_in_use_size = size;
    }

    return old_size;
}

static unsigned long
quick_get_ulong(iofield, data)
IOFieldPtr iofield;
void *data;
{
    data = (void *) ((char *) data + iofield->offset);
    /* only used when field type is an integer and aligned by its size */
    switch (iofield->size) {
    case 1:
	return (unsigned long) (*((unsigned char *) data));
    case 2:
	return (unsigned long) (*((unsigned short *) data));
    case 4:
	return (unsigned long) (*((unsigned int *) data));
    case 8:
#if SIZEOF_LONG == 8
	if ((((long) data) & 0x0f) == 0) {
	    /* properly aligned */
	    return (unsigned long) (*((unsigned long *) data));
	} else {
	    union {
		unsigned long tmp;
		int tmpi[2];
	    } u;
	    u.tmpi[0] = ((int *) data)[0];
	    u.tmpi[1] = ((int *) data)[1];
	    return u.tmp;
	}
#else
	/* must be fetching 4 bytes of the 8 available */
#ifdef WORDS_BIGENDIAN
	return (*(((unsigned long *) data) +1));
#else
	return (*((unsigned long *) data));
#endif
#endif
    }
    return 0;
}

static void *
quick_get_pointer(iofield, data)
IOFieldPtr iofield;
void *data;
{
    union {
	void *p;
	unsigned long tmp;
	int tmpi[2];
    } u;
    data = (void *) ((char *) data + iofield->offset);
    /* only used when field type is an integer and aligned by its size */
    switch (iofield->size) {
    case 1:
	u.tmp = (unsigned long) (*((unsigned char *) data));
	break;
    case 2:
	u.tmp = (unsigned long) (*((unsigned short *) data));
	break;
    case 4:
	u.tmp = (unsigned long) (*((unsigned int *) data));
	break;
    case 8:
#if SIZEOF_LONG == 8
	if ((((long) data) & 0x0f) == 0) {
	    /* properly aligned */
	    u.tmp = (unsigned long) (*((unsigned long *) data));
	} else {
	    u.tmpi[0] = ((int *) data)[0];
	    u.tmpi[1] = ((int *) data)[1];
	    return u.p;
	}
#else
	/* must be fetching 4 bytes of the 8 available */
#ifdef WORDS_BIGENDIAN
	u.tmp = (*(((unsigned long *) data) +1));
#else
	u.tmp = (*((unsigned long *) data));
#endif
#endif
	break;
    }
    return u.p;
}

void
quick_put_ulong(iofield, value, data)
IOFieldPtr iofield;
unsigned long value;
void *data;
{
    data = (void *) ((char *) data + iofield->offset);
    /* only used when field type is an integer and aligned by its size */
    switch (iofield->size) {
    case 1:
	*((unsigned char *) data) = (unsigned char) value;
	break;
    case 2:
	*((unsigned short *) data) = (unsigned short) value;
	break;
    case 4:
	*((unsigned int *) data) = (unsigned int) value;
	break;
    case 8:
	if ((((long) data) & 0x0f) == 0) {
	    /* properly aligned */
	    *((unsigned long *) data) = (unsigned long) value;
	} else {
	    union {
		unsigned long v;
		unsigned int i[2];
	    }u;
	    u.v = value;
	    ((int *) data)[0] = u.i[0];
	    ((int *) data)[1] = u.i[1];
	}
	break;
    }
}

