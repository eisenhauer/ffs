
#include "config.h"

#ifndef MODULE
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_WINDOWS_H
#include <winsock.h>
#endif
#include <ctype.h>
#include <time.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#include <stdlib.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <stdio.h>
#include <sys/types.h>
#include <limits.h>
#include <strings.h>
#else
#include "kernel/pbio_kernel.h"
#include "kernel/kpbio.h"
#include "kernel/library.h"
#endif

#include "assert.h"
#include "fm.h"
#include "fm_internal.h"

int IOdumpVerbose = 0;

static int server_register_format(FMContext fmc, FMFormat ioformat);
static int self_server_register_format(FMContext fmc,
					     FMFormat ioformat);

static void byte_swap(char *data, int size);
static FMFormat server_get_format(FMContext iocontext, void *buffer);
static char *stringify_field_type(const char *type, 
					FMFormat base_format,
					char *buffer, int size);
static int get_host_port_format_ID(void *format_ID);
static int get_host_IP_format_ID(void *format_ID);
static int is_self_server(FMContext fmc);
static void expand_FMContext(FMContext fmc);
static int IOget_array_size_dimen(const char *str, FMFieldList fields,
				  int dimen, int *control_field);
static int field_type_eq(const char *str1, const char *str2);

/* 
 * this reference to establish_server_connection is a function pointer 
 * rather than an extern so that the server_acts.o file is not loaded 
 * unless create_IOcontext() (which is also in that file) appears in 
 * the user program.  This avoids requiring the user to always link 
 * with -lnsl, -lsocket, etc when he might not otherwise need to.
 */
int (*establish_server_connection_ptr)(FMContext fmc, int do_fallback);

FMfloat_format fm_my_float_format = Format_Unknown;
/* 
 * fm_reverse_float_formats identifies for each format what, 
 * if any, format is its byte-swapped reverse.
*/
FMfloat_format fm_reverse_float_formats[] = {
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

static void
init_float_formats()
{
    static int done = 0;
    if (!done) {
	double d = MAGIC_FLOAT;
	fm_my_float_format = infer_float_format((char*)&d, sizeof(d));
	switch (fm_my_float_format) {
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
	

extern
FMContext
new_FMContext()
{
    FMContext c;
    init_float_formats();
    c = (FMContext) malloc((size_t) sizeof(FMContextStruct));
    memset(c, 0, sizeof(FMContextStruct));
    c->ref_count = 1;
    c->format_list_size = 0;
    c->format_list = NULL;
    c->reg_format_count = 0;
    c->byte_reversal = 0;
    c->native_float_format = fm_my_float_format;
    c->native_column_major_arrays = 0;
    c->native_pointer_size = sizeof(char*);
    c->errno_val = 0;
    c->result = NULL;
    
    c->self_server = 0;
    c->server_fd = (char *) -1;
    c->server_pid = 0;
    c->server_byte_reversal = 0;
    c->master_context = NULL;

    return (c);
}

extern
FMContext
create_local_FMcontext()
{
    FMContext fmc = new_FMContext();
    fmc->self_server = 1;
    return fmc;
}

static void
free_FMformat(body)
FMFormat body;
{
    int i;
    body->ref_count--;
    if (body->ref_count != 0)
	return;
    free(body->format_name);
    free(body->master_struct_list);  /* not subelements */
    for (i = 0; i < body->field_count; i++) {

	free((char*)body->field_list[i].field_name);
	free((char*)body->field_list[i].field_type);
	if (body->var_list != NULL) {
	    if (body->var_list[i].dimens != NULL) {
		int j;
/*		for (j = 0; j < body->var_list[i].dimen_count; j++) {
		    if (body->var_list[i].dimens[j].control_field_index != -1)
			free(body->var_list[i].dimens[j].control_field);
		}*/
		free(body->var_list[i].dimens);
	    }
	    if (body->var_list[i].type_desc.next != NULL) {
		FMTypeDesc *tmp = body->var_list[i].type_desc.next;
		while (tmp != NULL) {
		    FMTypeDesc *next = tmp->next;
		    free(tmp);
		    tmp = next;
		}
	    }
	}
    }
    free(body->field_list);
    free(body->var_list);
    i = 0;
    while (body->subformats && body->subformats[i]) {
	body->subformats[i]->subformats = NULL;
	free_FMformat(body->subformats[i++]);
    }
    free(body->subformats);
    free(body->field_subformats);
    if (body->server_format_rep != NULL)
	free(body->server_format_rep);
    if (body->server_ID.value != NULL)
	free(body->server_ID.value);
    if (body->opt_info != NULL) {
	FMOptInfo *o = body->opt_info;
/*	while (o->info_type != -1) {
	    free(o->info_block);
	    o++;
	    }*/
	free(o);
    }
    if ((body->xml_out != NULL) && (body->xml_out != (void*)-1)) {
/*	free_XML_output_info(body->xml_out);*/
    }
    free(body);
}

static int format_server_verbose = -1;
extern void dump_FMFormat(FMFormat ioformat);
unsigned char ID_length[] = {8, 10, 12};

extern int
FMformatID_len(char *buffer)
{
    return ID_length[version_of_format_ID(buffer)];
}

extern void
add_opt_info_FMformat(format, typ, len, block)
FMFormat format;
int typ;
int len;
void *block;
{
    int count = 0;
    if (format->opt_info == NULL) {
	format->opt_info = malloc(2*sizeof(FMOptInfo));
    } else {
	while(format->opt_info[count].info_type != -1) count++;
	format->opt_info = realloc(format->opt_info,
					 (count + 2) *sizeof(FMOptInfo));
    }
    format->opt_info[count].info_type = typ;
    format->opt_info[count].info_len = len;
    format->opt_info[count].info_block = (char*)block;
    format->opt_info[count + 1].info_type = -1;
}

static FMFormat
copy_fmformat_to_fmc(format, fmc)
FMFormat format;
FMContext fmc;
{
    FMFormat new_format;
    int i;

    for (i = 0; i < fmc->reg_format_count; i++) {
	if (fmc->format_list[i] == format) {
	    if (format_server_verbose == 1) {
		printf("Copy, format %lx exists in context %lx as format %lx\n",
		       (long)format, (long)fmc, (long)fmc->format_list[i]);
	    }
	    return fmc->format_list[i];
	}
    }
    new_format = malloc(sizeof(struct _FMFormatBody));
    if (format_server_verbose == 1) {
	printf("Copy, entering format %lx into context %lx as new format %lx\n",
	       (long) format, (long) fmc, (long) new_format);
    }
    new_format->context = fmc;
    new_format = format;
    format->ref_count++;

    if (fmc->reg_format_count == fmc->format_list_size) {
	expand_FMContext(fmc);
    }
    fmc->format_list[fmc->reg_format_count++] = new_format;

    return new_format;
}

extern FMcompat_formats
FMget_compat_formats(FMFormat ioformat)
{
    FMcompat_formats ret;
    int count = 0;
    int i = 0;
    /*printf("%s:%d In funtion %s\n", __FILE__, __LINE__, __FUNCTION__);*/
    if(ioformat->opt_info)
	ret = malloc(sizeof(struct compat_formats));
    else
	return NULL;
    while (ioformat->opt_info[i].info_type != 0) {
	if (ioformat->opt_info[i].info_type == COMPAT_OPT_INFO) {
	    char *buffer = ioformat->opt_info[i].info_block;
	    int fid_length = ID_length[version_of_format_ID(buffer)];
	    ret[count].prior_format = 
		FMformat_from_ID(ioformat->context, buffer);
	    ret[count].xform_code = buffer + fid_length;
	    count++;
	    ret = realloc(ret, sizeof(struct compat_formats) * (count + 1));
	}
	i++;
    }
    if (count == 0) {
	free(ret);
	ret = NULL;
    } else {
	ret[count].prior_format = NULL;
	ret[count].xform_code = NULL;
    }
    return ret;
}
    
FMFormat
get_local_format_IOcontext(iocontext, buffer)
FMContext iocontext;
void *buffer;
{
    FMContext fmc = (FMContext) iocontext;
    int i;
    if (format_server_verbose == -1) {
	if (getenv("FORMAT_SERVER_VERBOSE") == NULL) {
	    format_server_verbose = 0;
	} else {
	    format_server_verbose = 1;
	}
    }
    if (format_server_verbose == 1) {
	printf("Get Format searching in context %lx for format ", 
	       (long)iocontext);
	print_server_ID(buffer);
	printf("\n");
    }
    switch (version_of_format_ID(buffer)) {
    case 1:
    {
	INT2 format_id = ((version_1_format_ID*) buffer)->format_identifier;
	/* shortcut on comparisons.  check likely difference first */
	for (i = fmc->reg_format_count - 1; i >= 0; i--) {
	    version_1_format_ID* id = (version_1_format_ID*)
		fmc->format_list[i]->server_ID.value;
	    if (format_id != id->format_identifier) continue;
	    if (memcmp(buffer, fmc->format_list[i]->server_ID.value,
		       fmc->format_list[i]->server_ID.length) == 0) {
		return fmc->format_list[i];
	    }
	}
	break;
    }
    case 2:
    {
	INT2 rep_len = ((version_2_format_ID*) buffer)->rep_len;
	/* shortcut on comparisons.  check likely difference first */
	for (i = fmc->reg_format_count - 1; i >= 0; i--) {
	    version_2_format_ID* id = (version_2_format_ID*)
		fmc->format_list[i]->server_ID.value;
	    if (rep_len != id->rep_len) continue;
	    if (memcmp(buffer, fmc->format_list[i]->server_ID.value,
		       fmc->format_list[i]->server_ID.length) == 0) {
		return fmc->format_list[i];
	    }
	}
	break;
    }
    default:
	for (i = fmc->reg_format_count - 1; i >= 0; i--) {
	    if (memcmp(buffer, fmc->format_list[i]->server_ID.value,
		       fmc->format_list[i]->server_ID.length) == 0) {
		return fmc->format_list[i];
	    }
	}
    }
    if (fmc->master_context != NULL) {
	return get_local_format_IOcontext(fmc->master_context, buffer);
    }
    return NULL;
}

FMFormat
FMformat_from_ID(iocontext, buffer)
FMContext iocontext;
char *buffer;
{
    FMContext fmc = (FMContext) iocontext;
    FMFormat new_format;
    int i;

    
    if (format_server_verbose == -1) {
	if (getenv("FORMAT_SERVER_VERBOSE") == NULL) {
	    format_server_verbose = 0;
	} else {
	    format_server_verbose = 1;
	}
    }
    if (format_server_verbose && (memcmp(buffer, "\0\0\0\0\0\0\0\0", 6) == 0)) {
	printf("   ->>>>   Null id in FMformat_from_ID\n");
    }
    
    switch (version_of_format_ID(buffer)) {
    case 1:
    {
	unsigned INT2 format_id = ((version_1_format_ID*) buffer)->format_identifier;
	/* shortcut on comparisons.  check likely difference first */
	for (i = fmc->reg_format_count - 1; i >= 0; i--) {
	    version_1_format_ID* id = (version_1_format_ID*)
		fmc->format_list[i]->server_ID.value;
	    if (format_id != id->format_identifier) continue;
	    if (memcmp(buffer, fmc->format_list[i]->server_ID.value,
		       fmc->format_list[i]->server_ID.length) == 0) {
		return fmc->format_list[i];
	    }
	}
	break;
    }
    case 2:
    {
	INT2 rep_len = ((version_2_format_ID*) buffer)->rep_len;
	/* shortcut on comparisons.  check likely difference first */
	for (i = fmc->reg_format_count - 1; i >= 0; i--) {
	    version_2_format_ID* id = (version_2_format_ID*)
		fmc->format_list[i]->server_ID.value;
	    if (rep_len != (INT2) id->rep_len) continue;
	    if (memcmp(buffer, fmc->format_list[i]->server_ID.value,
		       fmc->format_list[i]->server_ID.length) == 0) {
		return fmc->format_list[i];
	    }
	}
	break;
    }
    default:
	for (i = fmc->reg_format_count - 1; i >= 0; i--) {
	    if (memcmp(buffer, fmc->format_list[i]->server_ID.value,
		       fmc->format_list[i]->server_ID.length) == 0) {
		return fmc->format_list[i];
	    }
	}
    }
    if (is_self_server(fmc)) {
	return NULL;
    } else {
	new_format = server_get_format(iocontext, buffer);
    }
    if (format_server_verbose == 1) {
	printf("Read format from format server  %lx\n",
	       (long)new_format);
	if (new_format != NULL) {
	    dump_FMFormat(new_format);
	} else {
	    printf("Format unknown.  Request was: ");
	    print_server_ID((void*)buffer);
	    printf("\n\tcontext was %lx\n", (long)iocontext);
	}
    }
    return new_format;
}

char **
get_subformat_names(field_list)
FMFieldList field_list;
{
    int name_count = 0;
    int field = 0;
    char **name_list = malloc(sizeof(char *));
    while (field_list[field].field_name != NULL) {
	char *base_type = base_data_type(field_list[field].field_type);
	if (str_to_data_type(base_type) == unknown_type) {
	    name_list = realloc(name_list, sizeof(char *) * (name_count + 1));
	    name_list[name_count++] = base_type;
	} else {
	    free(base_type);
	}
	field++;
    }
    name_list = realloc(name_list, sizeof(char *) * (name_count + 1));
    name_list[name_count] = NULL;
    if (name_count == 0) {
	free(name_list);
	name_list = NULL;
    }
    return name_list;
}

static
void
get_subformats_context(ioformat, format_list_p, format_count_p)
FMFormat ioformat;
FMFormat **format_list_p;
int *format_count_p;
{
    int field;
    for (field = 0; field < ioformat->field_count; field++) {
	FMFormat subformat = ioformat->field_subformats[field];
	if (subformat != NULL) {
	    int i;
	    get_subformats_context(subformat, format_list_p, format_count_p);
	    *format_list_p = realloc(*format_list_p,
			       sizeof(FMFormat) * (*format_count_p + 1));
	    for (i=0; i< *format_count_p; i++) {
		if ((*format_list_p)[i] == subformat) {
		    /* don't add it if it's already there */
		    subformat = NULL;
		}
	    }
	    if (subformat != NULL) {
		(*format_list_p)[(*format_count_p)++] = subformat;
	    }
	}
    }
}

FMFormat *
get_subformats_IOformat(ioformat)
FMFormat ioformat;
{
    int format_count = 0;
    FMFormat *format_list = malloc(sizeof(FMFormat));
    get_subformats_context(ioformat, &format_list, &format_count);
    format_list = realloc(format_list, sizeof(FMFormat) * (format_count + 2));
    format_list[format_count] = ioformat;
    format_list[format_count + 1] = NULL;
    return format_list;
}

int
FMformat_index(FMFormat f)
{
    return f->format_index;
}

FMFormat
FMformat_by_index(fmc, index)
FMContext fmc;
int index;
{
    if (index < fmc->reg_format_count) {
	return fmc->format_list[index];
    } else {
	return NULL;
    }
}


FMFormat
new_FMFormat()
{
    FMFormat ioformat;
    ioformat = (FMFormat) malloc(sizeof(FMFormatBody));
    ioformat->field_subformats = NULL;
    ioformat->superformat = NULL;
    ioformat->subformats = NULL;
    ioformat->ref_count = 1;
    ioformat->master_struct_list = NULL;
    ioformat->format_name = NULL;
    ioformat->byte_reversal = 0;
    ioformat->alignment = 0;
    ioformat->column_major_arrays = 0; /* by default, C mode */
    ioformat->recursive = 0;
    ioformat->float_format = fm_my_float_format;
    ioformat->pointer_size = 0;
    ioformat->IOversion = -1;
    ioformat->field_list = NULL;
    ioformat->var_list = NULL;
    ioformat->server_format_rep = NULL;
    ioformat->server_ID.length = 0;
    ioformat->server_ID.value = NULL;
    ioformat->opt_info = NULL;
    ioformat->xml_out = NULL;
    return (ioformat);
}

extern
void
expand_FMContext(fmc)
FMContext fmc;
{
    int new_count = fmc->format_list_size + 10;
    int new_size = sizeof(FMFormat) * (new_count);
    int i;

    if (fmc->format_list != NULL) {
	fmc->format_list = (FMFormat *)
	    realloc((void *) fmc->format_list, (unsigned) new_size);
    } else {
	fmc->format_list = (FMFormat *) malloc((unsigned) new_size);
    }
    fmc->format_list_size = new_count;
    for (i = fmc->reg_format_count; i < fmc->format_list_size; i++) {
	fmc->format_list[i] = NULL;
    }
}

static int
is_var_array_field(FMFieldList field_list, int field)
{
    int done = 0;
    int ret = 0;
    int dimen_count = 0;
    int control_val;
    while (!done) {
	int static_size = IOget_array_size_dimen(field_list[field].field_type,
						 field_list, dimen_count, 
						 &control_val);
	dimen_count++;
	if (static_size == 0) {
	    done++;
	    continue;
	}
	if ((static_size == -1) && (control_val == -1)) {
	    /* failed validation, errors already delivered */
	    return -1;
	}
	if (control_val != -1) {
	    /* dynamic array */
	    ret = 1;
	}
    }
    return ret;
}

static FMTypeDesc *
new_FMTypeDesc()
{
    FMTypeDesc *tmp = malloc(sizeof(*tmp));
    memset(tmp, 0, sizeof(*tmp));
    return tmp;
}
    
/*
	integer			an integer
	*integer		a pointer to an integer
	integer[35]		35 integers
	*integer[35]		35 pointers to integers
	*(integer[35])		a pointer to 35 integers
	*(integer[35])[10]	10 pointers to 35 integers
*/

static FMTypeDesc *
gen_type_desc(FMFormat f, int field, const char *typ)
{
    char *first_open = strchr(typ, '(');
    char *last_close = strrchr(typ, ')');
    if ((first_open && !last_close) || (!first_open && last_close)) {
	fprintf(stderr, "Mismatched parenthesis in type spec \"%s\" .\n",
		f->field_list[field].field_type);
	return NULL;
    }
    if (first_open) {
	FMTypeDesc *base, *root, *tmp;
	/* recurse */
	char *t = strdup(typ);
	char *first = strchr(t, '(');
	char *last = strrchr(t, ')');
	*last = 0;
	base = gen_type_desc(f, field, first +1);
	while (first <= last) *(first++) = 'a';   /* wash it out */
	tmp = root = gen_type_desc(f, field, t);
	while (tmp->next) tmp = tmp->next;
	*tmp = *base;   /* wipe out Simple */
	free(t);
	return root;
    } else {
	int dimen_count = 0;
	FMTypeDesc *root = new_FMTypeDesc();
	FMTypeDesc *simple = root;
	int done = 0;
	int var_array = 0;
	long junk;
	root->type = FMType_simple;
	root->next = NULL;
	root->field_index = field;
	while (isspace((int)*typ)) {	/* skip preceeding space */
	    typ++;
	}
	while (*typ == '*') {
	    FMTypeDesc *tmp = new_FMTypeDesc();
	    tmp->type = FMType_pointer;
	    tmp->field_index = field;
	    tmp->next = root;
	    root = tmp;
	    typ++;
	    while (isspace((int)*typ)) {	/* skip preceeding space */
		typ++;
	    }
	}
	/* now we've handled any pointer specs */
	root->data_type = array_str_to_data_type(typ, &junk);
	if (strncmp(typ, "string", 6) == 0) {
	    if ((*(typ + 6) == '[') || (*(typ + 6) == 0) ||
		isspace(*(typ+6))) {
		simple->type == FMType_string;
	    }
	}

	while (!done) {
	    FMFieldList field_list = f->field_list;
	    int control_val;
	    FMTypeDesc *tmp;
	    int static_size = IOget_array_size_dimen(typ,
						     field_list, dimen_count, 
						     &control_val);
	    if (static_size == 0) {
		done++;
		continue;
	    }
	    tmp = new_FMTypeDesc();
	    tmp->type = FMType_array;
	    tmp->field_index = field;
	    tmp->static_size = static_size;
	    if (static_size == -1) {
		tmp->static_size = 0;
		var_array = 1;
	    }
	    tmp->control_field_index = control_val;
	    tmp->next = root;
	    root = tmp;
	    dimen_count++;
	}
	if (var_array) {
	    FMTypeDesc *tmp = new_FMTypeDesc();
	    tmp->type = FMType_pointer;
	    tmp->next = root;
	    root = tmp;
	}
	return root;
    }
}

typedef struct {
    char tmp;
    void *p;
} *sp;

typedef struct {
    char tmp;
    short s;
} *ss;

typedef struct {
    char tmp;
    int i;
} *si;

typedef struct {
    char tmp;
    long l;
} *sl;

typedef struct {
    char tmp;
    float f;
} *sf;

typedef struct {
    char tmp;
    double d;
} *sd;

static int
type_alignment(ioformat, field)
FMFormat ioformat;
int field;
{
    FMVarInfoList var = &ioformat->var_list[field];
    FMTypeDesc *t = &var->type_desc;
    int size = ioformat->field_list[field].field_size;
    while (t != NULL) {
	switch (t->type) {
	case FMType_pointer:
	case FMType_string:
	    return FMOffset(sp, p);
	case FMType_array:
	    t = t->next;
	    break;
	case FMType_subformat:
	    return ioformat->field_subformats[field]->alignment;
	case FMType_simple:
	    switch(t->data_type) {
	    case integer_type: case unsigned_type:
	    case enumeration_type: case boolean_type: case char_type:
		if (size == 1) return 1;
		if ((size == sizeof(short)) || (size < sizeof(int)))
		    return FMOffset(ss, s);
		if ((size < sizeof(int))  || (size < sizeof(long)))
		    return FMOffset(si, i);
		return FMOffset(sl, l);
		break;
	    case unknown_type: case string_type:
		assert(0);
	    case float_type:
		if (size < sizeof(float)) return size;
		if (size < sizeof(double)) return FMOffset(sf, f);
		return FMOffset(sd, d);
	    }
	    break;
	}
    }
}

static void
gen_var_dimens(FMFormat ioformat, int field)
{
    FMFieldList field_list = ioformat->field_list;
    FMVarInfoList new_var_list = ioformat->var_list;
    const char *typ = field_list[field].field_type;
    int dimen_count = 0;
    int done = 0;
    FMDimen *dimens = NULL;
    FMTypeDesc *tmp, *last;
    if ((!strchr(typ, '*')) && (!strchr(typ, '['))) {
	new_var_list[field].type_desc.next = NULL;
	new_var_list[field].type_desc.type = FMType_simple;
	new_var_list[field].type_desc.data_type = str_to_data_type(typ);
    } else {
	FMTypeDesc *desc = gen_type_desc(ioformat, field, typ);
	new_var_list[field].type_desc = *desc;
	free(desc);
    }
    tmp = &new_var_list[field].type_desc;
    last = NULL;
    while(tmp->next != NULL) {
	last = tmp;
	tmp = tmp->next;
	if (tmp->type == FMType_pointer) {
	    ioformat->variant = 1;
	}
    }
    if (new_var_list[field].data_type == string_type) {
	tmp->type = FMType_string;
    } else if (ioformat->field_subformats[field] != NULL) {
	tmp->type = FMType_subformat;
	tmp->field_index = field;
	if (ioformat->field_subformats[field]->recursive) {
	    if (last) last->pointer_recursive++;
	}
    }
    while (!done) {
	int control_val;
	int static_size = IOget_array_size_dimen(field_list[field].field_type,
						 field_list, dimen_count, 
						 &control_val);
	if (static_size == 0) {
	    done++;
	    continue;
	}
	if (dimens == NULL) {
	    dimens = malloc(sizeof(dimens[0]));
	} else {
	    dimens = realloc(dimens, sizeof(dimens[0]) * (dimen_count + 1));
	}
	dimens[dimen_count].static_size = static_size;
	dimens[dimen_count].control_field_index = -1;
	if (control_val != -1) {
	    /* got a variant array */
	    ioformat->variant = 1;
	    new_var_list[field].var_array = 1;
	    dimens[dimen_count].control_field_index = control_val;
	    dimens[dimen_count].static_size = 0;
	} else {
	    dimens[dimen_count].static_size = static_size;
	}
	dimen_count++;
    }
    new_var_list[field].dimens = dimens;
    new_var_list[field].dimen_count = dimen_count;
}

extern int
generate_var_list(ioformat, formats)
FMFormat ioformat;
FMFormat *formats;
{
    FMVarInfoList new_var_list;
    FMFieldList field_list = ioformat->field_list;
    int field_count = ioformat->field_count;
    int field;
    static int first = 1;
    if (first) {
	first = 0;
    }
    if (ioformat->var_list)
	free(ioformat->var_list);
    if (ioformat->field_subformats)
	free(ioformat->field_subformats);
    new_var_list = (FMVarInfoList)
	malloc((size_t) sizeof(FMVarInfoStruct) * field_count);
    ioformat->field_subformats = malloc(sizeof(FMFormat) * field_count);
    ioformat->var_list = new_var_list;
    ioformat->alignment = 1;
    for (field = 0; field < field_count; field++) {
	long elements;
	int type_align;
	new_var_list[field].string = 0;
	new_var_list[field].var_array = 0;
	new_var_list[field].byte_vector = 0;
	new_var_list[field].dimen_count = 0;
	new_var_list[field].dimens = NULL;
	new_var_list[field].type_desc.next = NULL;
	ioformat->field_subformats[field] = NULL;
	new_var_list[field].data_type =
	    array_str_to_data_type(field_list[field].field_type,
				   &elements);
	if (new_var_list[field].data_type == string_type) {
	    ioformat->variant = 1;
	    new_var_list[field].string = 1;
	}
	if (new_var_list[field].data_type == unknown_type) {
	    char *base_type = base_data_type(field_list[field].field_type);
	    FMFormat subformat = NULL;
	    int j = 0;
	    while (formats && formats[j] != NULL) {
		if (strcmp(base_type, formats[j]->format_name) == 0) {
		    subformat = formats[j];
		}
		j++;
	    }
	    if (strcmp(base_type, ioformat->format_name) == 0) {
		subformat = ioformat;
		ioformat->recursive = 1;
		ioformat->variant = 1;
	    }
	    if (subformat == NULL) {
		(void) fprintf(stderr, "Field \"%s\" base type \"%s\" is not a simple type or registered format name.\n",
			       ioformat->field_list[field].field_name,
			       base_type);
		(void) fprintf(stderr, "Format rejected.\n ");
		return 0;
	    }
		
	    new_var_list[field].byte_vector =
		(strcmp(base_type, "IOEncodeElem") == 0);
	    free(base_type);
	    if (subformat != NULL) {
		ioformat->variant |= subformat->variant;
		ioformat->recursive |= subformat->recursive;
	    }
	    ioformat->field_subformats[field] = subformat;
	}
	gen_var_dimens(ioformat, field);
	type_align = type_alignment(ioformat, field);
	if (ioformat->alignment < type_align) {
	    ioformat->alignment = type_align;
	}
    }
    return 1;
}

#ifdef WORDS_BIGENDIAN
#define OUR_BYTE_ORDER 1
#define OTHER_BYTE_ORDER 0
#else
#define OUR_BYTE_ORDER 0
#define OTHER_BYTE_ORDER 1
#endif

    
static format_rep
add_server_subformat_rep(ioformat, super_rep, super_rep_size)
FMFormat ioformat;
char *super_rep;
int *super_rep_size;
{
    int byte_reversal = ioformat->byte_reversal;
    int rep_size = (sizeof(struct _field_wire_format) *
		     (ioformat->field_count));
    int i;
    int opt_info_count = 0;
    struct _subformat_wire_format *rep;
    char *string_base;
    int cur_offset;
    struct _field_wire_format *fields;

    rep_size += strlen(ioformat->format_name) + 1;
    for (i = 0; i < ioformat->field_count; i++) {
	rep_size += strlen(ioformat->field_list[i].field_name) + 1;
	rep_size += strlen(ioformat->field_list[i].field_type) + 1;
    }

    rep_size += sizeof(struct _subformat_wire_format_0);

    rep_size = (rep_size + 3) & -4;  /* round up by even 4 */
    while (ioformat->opt_info && 
	   (ioformat->opt_info[opt_info_count].info_type != 0)) {
	rep_size += ioformat->opt_info[opt_info_count].info_len;
	rep_size = (rep_size + 3) & -4;  /* round up by even 4 */
	opt_info_count++;
    }
    rep_size += (opt_info_count + 1) * sizeof(struct _opt_info_wire_format);

    super_rep = realloc(super_rep, *super_rep_size + rep_size+4); /* little extra */
    rep = (struct _subformat_wire_format *) (super_rep + *super_rep_size);
    cur_offset = (sizeof(struct _subformat_wire_format_0) + 
		  (sizeof(struct _field_wire_format) * 
		   (ioformat->field_count)));
    rep->f.f0.server_rep_version = 0;
    rep->f.f0.header_size = sizeof(struct _subformat_wire_format_0);
    
    rep->f.f0.column_major_arrays = ioformat->column_major_arrays;
    rep->f.f0.alignment = ioformat->alignment;
    rep->f.f0.opt_info_offset = 0; /* will be set later */

    string_base = (char *) rep;

    rep->f.f0.name_offset = cur_offset;
    if (byte_reversal) byte_swap((char*) &rep->f.f0.name_offset, 2);
    strcpy(string_base + cur_offset, ioformat->format_name);
    cur_offset += strlen(ioformat->format_name) + 1;

    rep->f.f0.field_count = ioformat->field_count;
    if (byte_reversal) byte_swap((char*) &rep->f.f0.field_count, 2);
    rep->f.f0.record_length = ioformat->record_length;
    if (byte_reversal) byte_swap((char*) &rep->f.f0.record_length, 4);
    rep->f.f0.record_byte_order = ioformat->byte_reversal ? 
	OTHER_BYTE_ORDER : OUR_BYTE_ORDER;
    rep->f.f0.pointer_size = ioformat->pointer_size;
    rep->f.f0.floating_point_rep = ioformat->float_format;

    fields = (struct _field_wire_format *) ((char*) rep + 
					    rep->f.f0.header_size);
    for (i = 0; i < ioformat->field_count; i++) {
	fields[i].field_size = ioformat->field_list[i].field_size;
	if (byte_reversal) byte_swap((char*) &fields[i].field_size, 4);
	fields[i].field_offset = ioformat->field_list[i].field_offset;
	if (byte_reversal) byte_swap((char*) &fields[i].field_offset, 4);
	fields[i].field_name_offset = cur_offset;
	if (byte_reversal) byte_swap((char*) &fields[i].field_name_offset, 2);
	strcpy(string_base + cur_offset, 
	       ioformat->field_list[i].field_name);
	cur_offset += strlen(ioformat->field_list[i].field_name) + 1;
	fields[i].field_type_offset = cur_offset;
	if (byte_reversal) byte_swap((char*) &fields[i].field_type_offset, 2);
	strcpy(string_base + cur_offset,
	       ioformat->field_list[i].field_type);
	cur_offset += strlen(ioformat->field_list[i].field_type) +1;
    }
    {
	char *info_base;
	struct _opt_info_wire_format tmp_base;
	int pad;

	/* fill in opt info fields */
	while ((cur_offset & 0x3) != 0) {
	    *(string_base + cur_offset++) = 0;
	}
	rep->f.f0.opt_info_offset = cur_offset;
	if (byte_reversal) byte_swap((char*) &rep->f.f0.opt_info_offset, 2);
	info_base = cur_offset +string_base;
	cur_offset += (opt_info_count + 1) *
	    sizeof(struct _opt_info_wire_format);

	tmp_base.info_type = 0;
	tmp_base.info_len = 0;
	tmp_base.info_offset = 0;

	memcpy(info_base + opt_info_count* sizeof(tmp_base), &tmp_base, 
	       sizeof(tmp_base));
	i = 0;
	for ( ; i < opt_info_count; i++) {
	    tmp_base.info_type = ioformat->opt_info[i].info_type;
	    if (byte_reversal) byte_swap((char*) &tmp_base.info_type, 4);
	    tmp_base.info_len = ioformat->opt_info[i].info_len;
	    if (byte_reversal) byte_swap((char*) &tmp_base.info_len, 4);
	    tmp_base.info_offset = cur_offset;
	    if (byte_reversal) byte_swap((char*) &tmp_base.info_offset, 4);
	    memcpy(info_base + i*sizeof(tmp_base), &tmp_base, sizeof(tmp_base));
	    memcpy(string_base + cur_offset, 
		   ioformat->opt_info[i].info_block,
		   ioformat->opt_info[i].info_len);
	    cur_offset += tmp_base.info_len;
	    while ((cur_offset & 0x3) != 0) {
		*(string_base + cur_offset++) = 0;
	    }
	}
    }

    while ((cur_offset & 0x3) != 0) {
	*(string_base + cur_offset++) = 0;
    }
    assert(cur_offset == rep_size);
    rep->f.f0.subformat_rep_length = htons(rep_size);
    *super_rep_size += rep_size;
    return (format_rep) super_rep;
}

static format_rep
build_server_format_rep(ioformat)
FMFormat ioformat;
{
    int subformat_count = 0;
    FMFormat *subformats = ioformat->subformats;
    int i;
    struct _format_wire_format *rep = malloc(sizeof(*rep));
    int rep_size = sizeof(*rep);

    while(subformats && subformats[subformat_count]) subformat_count++;
    if (subformat_count >= 100) return NULL;  /* no way */

    rep = (struct _format_wire_format *)
	add_server_subformat_rep(ioformat, rep, &rep_size);
    for (i=0; i < subformat_count; i++) {
        rep = (struct _format_wire_format *)
	    add_server_subformat_rep(subformats[i], rep, &rep_size);
    }
    
    rep->f.f0.format_rep_length = htons(rep_size);
    rep->f.f0.record_byte_order = ioformat->byte_reversal ? 
	OTHER_BYTE_ORDER : OUR_BYTE_ORDER;
    rep->f.f0.server_rep_version = 0;
    rep->f.f0.subformat_count = subformat_count;
    rep->f.f0.recursive_flag = 0;  /* GSE must set right */
    rep->f.f0.unused1_in_format_0 = 0;
    rep->f.f0.unused2_in_format_0 = 0;
    return (format_rep) rep;
}

/* 
 * search_compatible_formats looks in contexts to see if a format (for 
 * which we are seeking a format_ID) has already been registered or has
 * been retrieved from the format server.  This is done by comparing their 
 * server_format_rep fields.
 */
static FMFormat
search_compatible_formats(iocontext, ioformat)
FMContext iocontext;
FMFormat ioformat;
{
    int i, search_rep_length;
    FMContext fmc = (FMContext) iocontext;

    if (ioformat->server_format_rep == NULL) {
	return NULL;
    }
    search_rep_length = ntohs(ioformat->server_format_rep->format_rep_length);

    /* search locally first */
    for (i = 0; i < fmc->reg_format_count; i++) {
	FMFormat tmp = fmc->format_list[i];
	int rep_length;

	if (tmp == ioformat)
	    continue;
	if (tmp->server_format_rep == NULL) {
	    continue;
	}
	if (strcmp(ioformat->format_name,
		   fmc->format_list[i]->format_name) != 0)
	    continue;
	rep_length = ntohs(tmp->server_format_rep->format_rep_length);
	if (search_rep_length != rep_length) {
	    if (format_server_verbose == 1) {
		printf("Format %s found in context %lx, but server reps have different lengths, %d and %d\n",
		       ioformat->format_name, (long) iocontext,
		       search_rep_length, rep_length);
	    }
	    continue;
	}
	if (memcmp(ioformat->server_format_rep, tmp->server_format_rep,
		   search_rep_length) == 0) {
	    return tmp;
	} else {
	    if (format_server_verbose == 1) {
		printf("Format %s found in context %lx, but server reps are different\n",
		       ioformat->format_name, (long) iocontext);
	    }
	}
    }
    if (fmc->master_context != NULL) {
	return search_compatible_formats(fmc->master_context, ioformat);
    }
    return NULL;
}

static int
is_self_server(FMContext fmc)
{
    if (fmc->master_context != NULL) {
	return is_self_server((FMContext) fmc->master_context);
    }
    return fmc->self_server;
}

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

static void
field_name_strip_default(char *field_name)
{
	/* '(' will have to be changed if IODefaultDecl is changed */
	char *s = strchr(field_name, '(');
	if(s)
		*s = '\0';
}

#define Max(i,j) ((i<j) ? j : i)
extern int
field_offset_compar(const void *a, const void *b)
{
    FMFieldList ia = (FMFieldList) a;
    FMFieldList ib = (FMFieldList) b;
    return (ia->field_offset - ib->field_offset);
}

FMFieldList
validate_and_copy_field_list(FMFieldList field_list, FMFormat ioformat)
{
    int field;
    FMFieldList new_field_list;
    int field_count = count_FMfield(field_list);
    new_field_list = (FMFieldList) malloc((size_t) sizeof(FMField) *
					     (field_count + 1));
    for (field = 0; field < field_count; field++) {
	int field_size = 0;
	if (strchr(field_list[field].field_type, '[') == NULL) {
	    /* not an array */
	    if (index(field_list[field].field_type, '*') == NULL) {
		field_size = field_list[field].field_size;
	    } else {
		field_size = ioformat->pointer_size;
	    }
	} else {
	    int ret = is_var_array_field(field_list, field);
	    if (ret == -1) return NULL;   /* rejected */
	    if ((ret == 1) || (index(field_list[field].field_type, '*'))) {
		/* variant array, real_field_size is
		 * fmc->pointer_size */
		field_size = ioformat->pointer_size;
		if (field_size <= 0) {
		    fprintf(stderr, "Pointer Size is not positive! BAD! pointer size = %d\n",
			    ioformat->pointer_size);
		    return NULL;
		}
	    } else {
		long elements;
		FMdata_type base_type;
		
		base_type = array_str_to_data_type(field_list[field].field_type,
						   &elements);
		if ((base_type != unknown_type) &&
		    (field_list[field].field_size > 16)) {
		    fprintf(stderr, "Field size for field %s in format %s is large, check to see if it is valid.\n",
			    field_list[field].field_name, ioformat->format_name);
		}
		field_size = field_list[field].field_size * elements;
		if (field_size <= 0) {
		    fprintf(stderr, "Field Size is not positive!  field type \"%s\" base= %d, elements=%ld\n",
			    field_list[field].field_type,
			    field_list[field].field_size, elements);
		    return NULL;
		}
	    }
	}

	ioformat->record_length = Max(ioformat->record_length,
					 field_list[field].field_offset +
					    field_size);
	new_field_list[field].field_name = strdup(field_list[field].field_name);
	field_name_strip_default((char *)new_field_list[field].field_name);
	new_field_list[field].field_type = strdup(field_list[field].field_type);
	new_field_list[field].field_size = field_list[field].field_size;
	new_field_list[field].field_offset = field_list[field].field_offset;
    }
    new_field_list[field_count].field_name = NULL;
    new_field_list[field_count].field_type = NULL;
    new_field_list[field_count].field_size = 0;
    new_field_list[field_count].field_offset = 0;
    ioformat->field_count = field_count;
    ioformat->field_list = new_field_list;
    qsort(ioformat->field_list, field_count, sizeof(FMField),
	  field_offset_compar);
    return new_field_list;
}

static void 
free_format_list(FMFormat *formats)
{
    while (formats && formats[0]) {
	free_FMformat(formats[0]);
	formats++;
    }
}

FMFormat
register_simple_data_format(FMContext context, char *struct_name,
			    FMFieldList struct_field_list,
			    int struct_size, FMOptInfo *opt_info)
{
    FMStructDescRec str_rec[2];
    str_rec[0].format_name = struct_name;
    str_rec[0].field_list = struct_field_list;
    str_rec[0].struct_size = struct_size;
    str_rec[0].opt_info = opt_info;
    str_rec[1].format_name = NULL;
    return register_data_format(context, &str_rec[0]);
}

static int
on_list(FMFormat candidate, FMFormat *list, int list_count)
{
    int i;
    for (i=0; i < list_count; i++) {
	if (list[i] == candidate) return 1;
    }
    return 0;
}

static int
try_add(FMFormat candidate, FMFormat *list, int list_count)
{
    int i;
    /* we can add a format IFF
       it's not already on the list *and*
       if all of its field_subformats are either NULL or on the list
    */
    if (on_list(candidate, list, list_count)) return 0;

    for (i = 0; i < candidate->field_count; i++) {
	if (candidate->field_subformats[i] != NULL) {
	    if (!on_list(candidate->field_subformats[i], list, list_count)) {
		return 0;
	    }
	}
    }
    return 1;
}

static
void
topo_order_subformats(super_format, format_count)
FMFormat super_format;
int format_count;
{
    FMFormat tmp[100]; /* surely large enough */
    assert(format_count < 100);
    FMFormat *format_list = super_format->subformats;
    int formats = 0;
    for (formats = 0; formats < format_count ; formats++) {
	int i = 0;
	// find a subformat we can add
	tmp[formats] = NULL;
	for (i=0; i < format_count; i++) {
	    if (try_add(format_list[i], tmp, formats)) {
		tmp[formats] = format_list[i];
		break;
	    }
	}
	if (tmp[formats] == NULL) {
	    /* nothing to add?  must have circular dependency.  Break it. */
	    if (i == (format_count - 1)) {
		/* last thing, add top */
		tmp[formats] = super_format;
	    } else {
		int j;
		for (j = 0; j < format_count; j++) {
		    if ((format_list[j] != super_format) &&
			(!on_list(format_list[j], tmp, formats))) {
			tmp[formats] = format_list[j];
		    }
		}
	    }
	}
    }
    for (formats = 0; formats < format_count; formats++) {
	format_list[formats] = tmp[format_count - formats - 1];
	format_list[formats]->superformat = super_format;
    }
}

extern FMFormat
FMlookup_format(FMContext context, FMStructDescList struct_list)
{
    int i;
    for (i=0 ; i < context->reg_format_count; i++) {
	if (context->format_list[i]->orig_struct_list == struct_list) {
	    return context->format_list[i];
	}
    }
    return NULL;
}

FMFormat
register_data_format(FMContext context, FMStructDescList struct_list)
{
    int i, field;
    int struct_count = 0;
    FMFormat *formats, ret;
    FMStructDescList master_struct_list;

    for (struct_count = 0; struct_list[struct_count].format_name != NULL; 
	 struct_count++) /* set struct_count */;
    
    formats = malloc(sizeof(formats[0]) * (struct_count+1));
    master_struct_list = malloc(sizeof(struct_list[0]) * (struct_count+1));
    formats[struct_count] = NULL;
    for (i = 0 ; i < struct_count; i++ ) {
	FMFormat ioformat = new_FMFormat();
	FMFieldList new_field_list;
	ioformat->format_name = strdup(struct_list[i].format_name);
	master_struct_list[i].format_name = ioformat->format_name;
	ioformat->pointer_size = context->native_pointer_size;
	ioformat->column_major_arrays = context->native_column_major_arrays;
	ioformat->float_format = context->native_float_format;
	ioformat->context = context;
	ioformat->record_length = 0;
	ioformat->variant = 0;
	ioformat->record_length = struct_list[i].struct_size;

	new_field_list = 
	    validate_and_copy_field_list(struct_list[i].field_list, ioformat);
	formats[i] = ioformat;

	if (new_field_list == NULL) {
	    free_format_list(formats);
	    return NULL;
	}
	master_struct_list[i].field_list = ioformat->field_list;
	master_struct_list[i].struct_size = struct_list[i].struct_size;
	master_struct_list[i].opt_info = NULL;
	if (struct_list[i].opt_info != NULL) {
	    FMOptInfo *opt_info = struct_list[i].opt_info;
	    int opt_info_count = 0;
	    while (opt_info[opt_info_count].info_type != 0) opt_info_count++;
	    formats[0]->opt_info = malloc(sizeof(opt_info[0]) * 
						(opt_info_count + 1));
	    memcpy(formats[0]->opt_info, opt_info, sizeof(opt_info[0]) * 
		   (opt_info_count + 1));
	    /* 
	     * we should vet the opt_info that we understand here.  I.E. check 
	     * to see if the XML info is appropriate, that the compatability info
	     * will work, etc. 
	     */
	} else {
	    formats[i]->opt_info = NULL;
	}
    }

    for (i= 0; i < struct_count; i++) {
	if (!generate_var_list(formats[i], formats)) {
	    free_format_list(formats);
	    return NULL;
	}
    }

    formats[0]->subformats = malloc(sizeof(FMFormat) * struct_count);
    memcpy(formats[0]->subformats, &formats[1], sizeof(FMFormat) * struct_count);
    formats[0]->subformats[struct_count-1] = NULL;
    topo_order_subformats(formats[0], struct_count-1);
    /* bubble up the variant flags */
    for (i= 0; i < struct_count; i++) {
	int j;
	for (j= 0; j < struct_count; j++) {
	    FMFormat iof = formats[j];
	    for (field = 0; field < iof->field_count; field++) {
		FMFormat subformat = iof->field_subformats[field];
		if (subformat != NULL) {
		    iof->variant |= subformat->variant;
		}
	    }
	}
    }
    for (i= 0; i < struct_count; i++) {
	FMFormat ioformat = formats[i];
	int last_field_end = 0;
	FMFieldList field_list = ioformat->field_list;
	for (field = 0; field < ioformat->field_count; field++) {
	    FMFormat subformat = ioformat->field_subformats[field];
	    if (subformat != NULL) {
		
		if (field_list[field].field_size <
		    subformat->record_length) {
		    (void) fprintf(stderr, "Field \"%s\" base type \"%s\" has size %d\n",
				   field_list[field].field_name,
				   field_list[field].field_type, 
				   field_list[field].field_size);
		    (void) fprintf(stderr, "  this is smaller than record size for \"%s\" (%d bytes)\n",
				   subformat->format_name,
				   subformat->record_length);
		    free_format_list(formats);
		    return NULL;
		}
	    }
	    if (field_list[field].field_offset < last_field_end) {
		(void) fprintf(stderr, "Fields \"%s\" and \"%s\" in format \"%s\" overlap!  Format rejected.\n",
			       field_list[field - 1].field_name,
			       field_list[field].field_name,
			       ioformat->format_name);
		free_format_list(formats);
		return NULL;
	    }
	    last_field_end = field_list[field].field_offset +
		field_list[field].field_size;
	    if (ioformat->var_list[field].var_array) {
		last_field_end = field_list[field].field_offset +
		    ioformat->pointer_size;
	    } else if (ioformat->var_list[field].type_desc.type == FMType_pointer) {
		last_field_end = field_list[field].field_offset +
		    ioformat->pointer_size;
	    }
	}
	if (struct_list[i].struct_size < last_field_end) {
	    (void) fprintf(stderr, "Structure size for structure %s is smaller than last field size + offset.  Format rejected.\n",
			   ioformat->format_name);
	    free_format_list(formats);
	    return NULL;
	}
    }

    master_struct_list[struct_count].format_name = NULL;
    formats[0]->master_struct_list = master_struct_list;
    {
	FMFormat cache_format;
	if (format_server_verbose == -1) {
	    if (getenv("FORMAT_SERVER_VERBOSE") == NULL) {
		format_server_verbose = 0;
	    } else {
		format_server_verbose = 1;
	    }
	}
	formats[0]->server_format_rep = build_server_format_rep(formats[0]);
	cache_format = search_compatible_formats(context, formats[0]);
	if (cache_format == NULL) {
	    /* register format with server */
	    if (is_self_server(context)) {
		if (self_server_register_format(context, formats[0]) == 0) 
		    return NULL;
	    } else {
		if (server_register_format(context, formats[0]) == 0)
		    return NULL;
	    }
	    if (format_server_verbose == 1) {
		printf("Registered format with format server - %lx  in context %lx\n",
		    (long) formats[0], (long) context);
		dump_FMFormat(formats[0]);
	    }
	} else {
	    free_FMformat(formats[0]);
	    formats[0] = cache_format;
	    formats[0]->ref_count++;

	    if (format_server_verbose == 1) {
		printf("Cache hit on format registration %lx \"%s\" ", 
		       (long)formats[0], formats[0]->format_name);
		print_format_ID(formats[0]);
		printf("\n");
	    }
	    free(formats);
	    return cache_format;
	}
    }
    if (context->reg_format_count == context->format_list_size) {
	expand_FMContext(context);
    }
    formats[0]->format_index = context->reg_format_count++;
    context->format_list[formats[0]->format_index] = formats[0];
    formats[0]->orig_struct_list = struct_list;
    ret = formats[0];
    free(formats);
    return ret;
}

INT4 FFS_self_server_IP_addr = 0;

extern void hashlittle2( 
  const void *key,       /* the key to hash */
  size_t      length,    /* length of the key */
  INT4   *pc,        /* IN: primary initval, OUT: primary hash */
  INT4   *pb)        /* IN: secondary initval, OUT: secondary hash */;


extern void
generate_format2_server_ID(server_ID_type *server_ID,
			   struct _format_wire_format_0 *server_format_rep)
{
    INT4 hash1 = 0, hash2 = 0;
    server_ID->length = 12;
    server_ID->value = malloc(12);
    ((version_2_format_ID *) server_ID->value)->version = 2;
    ((version_2_format_ID *) server_ID->value)->unused = 0;
    hashlittle2((int*)server_format_rep, 
		ntohs(server_format_rep->format_rep_length),
		&hash1, &hash2);
    if (format_server_verbose) {
	unsigned char *tmp = (unsigned char*)server_format_rep;
	int size = ntohs(server_format_rep->format_rep_length);
	int i;
	printf("Server rep is : ");
	for (i=0; i< size; i++) {
	    printf("%02x ", tmp[i]);
	}
	printf("\n");
    }
    ((version_2_format_ID *) server_ID->value)->rep_len = 
	htons(ntohs(server_format_rep->format_rep_length) >> 2);
    ((version_2_format_ID *) server_ID->value)->hash1 = htonl(hash1);
    ((version_2_format_ID *) server_ID->value)->hash2 = htonl(hash2);
}

static int
self_server_register_format(fmc, ioformat)
FMContext fmc;
FMFormat ioformat;
{
    format_rep server_format_rep;
    if (format_server_verbose == -1) {
	if (getenv("FORMAT_SERVER_VERBOSE") == NULL) {
	    format_server_verbose = 0;
	} else {
	    format_server_verbose = 1;
	}
    }
    /* we're a format server ourselves, assign an ID */
    if (fmc->master_context != NULL) {
	return self_server_register_format((FMContext) fmc->master_context,
					   ioformat);
    }
    if (ioformat->context != fmc) {
	/* registering in a master, copy it down */
	assert(0);
    }
    server_format_rep = build_server_format_rep(ioformat);
    generate_format2_server_ID(&ioformat->server_ID, server_format_rep);
    if (format_server_verbose == 1) {
	printf("Registering %s to locally-issued format ID ",
	       ioformat->format_name);
	print_format_ID(ioformat);
	printf("\n");
    }
    return 1;
}

int
count_FMfield(list)
FMFieldList list;
{
    int i = 0;
    while (list[i].field_name != NULL) {
	i++;
    }
    return i;
}

extern
int
struct_size_IOfield(fmc, list)
FMContext fmc;
FMFieldList list;
{
    int i = 0;
    int struct_size = 0;
    while (list[i].field_name != NULL) {
	int field_size = 0;
	if (is_var_array_field(list, i) == 1) {
	    /* variant array, real_field_size is ioformat->pointer_size */
	    if ((fmc == NULL) || (fmc->native_pointer_size == 0)) {
		field_size = sizeof(char *);
	    } else {
		field_size = fmc->native_pointer_size;
	    }
	} else {
	    if (fmc) {
		long elements;
		array_str_to_data_type(list[i].field_type, &elements);
		field_size = list[i].field_size * elements;
	    } else {
		field_size = list[i].field_size;
	    }
	}
	assert(field_size > 0);
	struct_size = Max(struct_size,
			  (list[i].field_offset + field_size));
	i++;
    }
    return struct_size;
}

extern
int
struct_size_field_list(list, pointer_size)
FMFieldList list;
int pointer_size;
{
    int i = 0;
    int struct_size = 0;
    while (list[i].field_name != NULL) {
	int field_size = 0;
	if (is_var_array_field(list, i) == 1) {
	    /* variant array, real_field_size is ioformat->pointer_size */
	    field_size = pointer_size;
	} else {
	    long elements;
	    array_str_to_data_type(list[i].field_type, &elements);
	    field_size = list[i].field_size * elements;
	}
	assert(field_size > 0);
	struct_size = Max(struct_size,
			  (list[i].field_offset + field_size));
/*	printf("i=%d field_name=%s field_type=%s struct_size= %d, offset=%d size=%d\n", i, list[i].field_name, list[i].field_type, struct_size, list[i].field_offset, field_size);*/
	i++;
    }
    return struct_size;
}

extern
FMFieldList
field_list_of_IOformat(format)
FMFormat format;
{
    return format->field_list;
}

extern
int
compare_field_lists(list1, list2)
FMFieldList list1, list2;
{
    int i = 0;
    do {
	if ((strcmp(list1[i].field_name, list2[i].field_name) != 0) ||
	    (list1[i].field_size != list2[i].field_size) ||
	    (list1[i].field_offset != list2[i].field_offset) ||
	    !field_type_eq(list1[i].field_type, list2[i].field_type)) {
	    return 1;
	}
	i++;
    } while ((list1[i].field_name != NULL) || (list2[i].field_name != NULL));

    return 0;
}

/* 
 * Max field list returns the "maximum" of two field lists. 
 * Basically, it prefers list1.  If all fields in list2 are in list 1, and 
 * if no fields in list2 are bigger than the corresponding field in
 * list1, the maximum returned is list1.  If there is a field in list2 
 * that is bigger than the corresponding field in list1, we'll still
 * use the ordering and even try to use the offsets in list1.  If
 * there's actually a field in list2 that isn't in list1, then most
 * bets are off as to exactly what will happen.  Basically the
 * resulting list will contain all fields, but we'll use the offsets
 * from their original lists to establish an ordering and then bump
 * the offsets up as necessary to ensure no overlap.  The result is 
 * likely a jumble that isn't necessarily terribly close to what was in 
 * either list...  Bummer for performance perhaps, but being smarter is
 * hard. 
 */
extern
FMFieldList
max_field_lists(list1, list2)
FMFieldList list1, list2;
{
    FMFieldList max_field_list = NULL;
    FMFieldList tlist2;
    int field_count1 = count_FMfield(list1);
    int field_count2 = count_FMfield(list2);
    int max_field_count = 0;
    int i, j;

    /* make a copy of list2 that we can scratch on */
    tlist2 = (FMFieldList) malloc((size_t) sizeof(FMField) * field_count2);
    memcpy(tlist2, list2, sizeof(FMField) * field_count2);

    max_field_list = (FMFieldList) malloc((size_t) sizeof(FMField) *
				      (field_count1 + field_count2 + 1));

    for (i = 0; i < field_count1; i++) {
	/* for each field in list1 */
	for (j = 0; j < field_count2; j++) {
	    if (tlist2[j].field_name &&
	      (strcmp(list1[i].field_name, tlist2[j].field_name) == 0)) {
		break;
	    }
	}
	if (j == field_count2) {
	    /* field exists only in 1, use it as is */
	    max_field_list[max_field_count].field_name =
		strdup(list1[i].field_name);
	    max_field_list[max_field_count].field_type =
		strdup(list1[i].field_type);
	    max_field_list[max_field_count].field_size =
		list1[i].field_size;
	    max_field_list[max_field_count].field_offset =
		list1[i].field_offset;
	    max_field_count++;
	} else {
	    /* field list in both.  Check types and take max size */
	    if (!field_type_eq(list1[i].field_type, tlist2[j].field_type)) {
		/* Yikes!  Types are bad!  Serious problem */
		free(tlist2);
		free(max_field_list);
		return NULL;
	    }
	    max_field_list[max_field_count].field_name =
		strdup(list1[i].field_name);
	    max_field_list[max_field_count].field_type =
		strdup(list1[i].field_type);
	    max_field_list[max_field_count].field_size =
		Max(list1[i].field_size, tlist2[j].field_size);
	    /* tentative offset */
	    max_field_list[max_field_count].field_offset =
		list1[i].field_offset;
	    max_field_count++;
	    /* mark field in list 2 as used */
	    tlist2[j].field_name = NULL;
	}
    }
    for (j = 0; j < field_count2; j++) {
	if (tlist2[j].field_name != NULL) {
	    /* didn't mark this field as used.  Must only be in list2 */
	    max_field_list[max_field_count].field_name =
		strdup(tlist2[j].field_name);
	    max_field_list[max_field_count].field_type =
		strdup(tlist2[j].field_type);
	    max_field_list[max_field_count].field_size =
		tlist2[j].field_size;
	    max_field_list[max_field_count].field_offset =
		tlist2[j].field_offset;
	    max_field_count++;
	}
    }
    free(tlist2);

    max_field_list[max_field_count].field_name = NULL;
    max_field_list[max_field_count].field_type = NULL;
    max_field_list[max_field_count].field_size = 0;
    max_field_list[max_field_count].field_offset = 0;

    /* sort the max list by offset now */
    qsort(max_field_list, max_field_count, sizeof(FMField),
	  field_offset_compar);
    for (i = 1; i < max_field_count; i++) {
	int last_field_end = max_field_list[i - 1].field_offset +
	max_field_list[i - 1].field_size;
	if (max_field_list[i].field_offset < last_field_end) {
	    /* damn, got some overlap.  Push this field up to fix. */
	    max_field_list[i].field_offset = last_field_end;
	}
    }
    return max_field_list;
}

extern
FMFieldList
copy_field_list(list)
FMFieldList list;
{
    int field_count = count_FMfield(list);
    FMFieldList new_field_list;
    int field;

    new_field_list = (FMFieldList) malloc((size_t) sizeof(FMField) *
					     (field_count + 1));
    for (field = 0; field < field_count; field++) {
	new_field_list[field].field_name = strdup(list[field].field_name);
	new_field_list[field].field_type = strdup(list[field].field_type);
	new_field_list[field].field_size = list[field].field_size;
	new_field_list[field].field_offset = list[field].field_offset;
    }
    new_field_list[field_count].field_name = NULL;
    new_field_list[field_count].field_type = NULL;
    new_field_list[field_count].field_offset = 0;
    new_field_list[field_count].field_size = 0;
    return new_field_list;
}

extern
void
free_field_list(list)
FMFieldList list;
{
    int i = 0;
    while (list[i].field_name != NULL) {
	free((char*)list[i].field_name);
	free((char*)list[i].field_type);
	i++;
    }
    free(list);
}

extern void
free_FMFormatList(FMFormatList format_list)
{
    int i = 0;
    while(format_list[i].format_name){
	free(format_list[i].format_name);
	free_field_list(format_list[i].field_list);
	i++;
    }
}

extern int
field_name_compar(const void *a, const void *b)
{
    FMFieldList ia = (FMFieldList) a;
    FMFieldList ib = (FMFieldList) b;
    return (strcmp(ia->field_name, ib->field_name));
}

FMformat_order
FMformat_cmp(format1, format2)
FMFormat format1;
FMFormat format2;
{
    FMformat_order tmp_result = Format_Equal;
    FMFieldList field_list1 =
	copy_field_list(field_list_of_IOformat(format1));
    FMFieldList field_list2 =
	copy_field_list(field_list_of_IOformat(format2));
    FMFormat *subformats1 = NULL, *subformats2 = NULL, *tmp_subformats = NULL;
    int field_count1, field_count2;
    int i, j, limit;

    /* count fields */
    for (field_count1 = 0; field_list1[field_count1].field_name != NULL;
	 field_count1++);

    /* count fields */
    for (field_count2 = 0; field_list2[field_count2].field_name != NULL;
	 field_count2++);

    qsort(field_list1, field_count1, sizeof(FMField), field_name_compar);
    qsort(field_list2, field_count2, sizeof(FMField), field_name_compar);

    limit = field_count1;
    if (field_count2 < limit)
	limit = field_count2;
    i = j = 0;
    while ((i < field_count1) && (j < field_count2)) {
	int name_cmp;
	if ((name_cmp = strcmp(field_list1[i].field_name,
			       field_list2[j].field_name)) == 0) {
	    /* fields have same name */
	    if (!field_type_eq(field_list1[i].field_type,
				  field_list2[j].field_type)) {
		return Format_Incompatible;
	    }
	} else if (name_cmp < 0) {
	    /* name_cmp>0  a field in field_list1 that doesn't appear in 2 */
	    if (tmp_result == Format_Less) {
		/* we previously found the opposite */
		tmp_result = Format_Incompatible;
		goto free_lists;
	    } else {
		/* tentatively decide that list1 is a superset of 2 */
		tmp_result = Format_Greater;
	    }
	    /* skip fields in 1 until we find the next field in 2 */
	    while (strcmp(field_list1[i].field_name,
			  field_list2[j].field_name) < 0) {
		i++;
		if (i == field_count1) {
		    /* didn't find one */
		    tmp_result = Format_Incompatible;
		    goto free_lists;
		}
	    }
	    i--;
	    j--;
	} else {
	    /* name_cmp>0  a field in field_list2 that doesn't appear in 1 */
	    if (tmp_result == Format_Greater) {
		tmp_result = Format_Incompatible;
		goto free_lists;
	    } else {
		tmp_result = Format_Less;
	    }
	    /* skip fields in field_list2 until we find matching */
	    while (strcmp(field_list1[i].field_name,
			  field_list2[j].field_name) > 0) {
		j++;
		if (j == field_count2) {
		    /* didn't find one */
		    tmp_result = Format_Incompatible;
		    goto free_lists;
		}
	    }
	    i--;
	    j--;
	}
	i++;
	j++;
    }
    if (i < field_count1) {
	if (tmp_result == Format_Less) {
	    tmp_result = Format_Incompatible;
	    goto free_lists;
	} else {
	    tmp_result = Format_Greater;
	}
    } else if (j < field_count2) {
	if (tmp_result == Format_Greater) {
	    tmp_result = Format_Incompatible;
	    goto free_lists;
	} else {
	    tmp_result = Format_Less;
	}
    }
    /* go through subformats */
    tmp_subformats = subformats1 = get_subformats_IOformat(format1);
    subformats2 = get_subformats_IOformat(format2);

    while (*subformats1 != NULL) {
	char *sub1_name = name_of_FMformat(*subformats1);
	int i = 0;
	if (*subformats1 == format1) {
	    /* self appears in subformat list, skip it */
	    subformats1++;
	    continue;
	}
	while (subformats2[i] != NULL) {
	    if (strcmp(sub1_name, name_of_FMformat(subformats2[i])) == 0) {
		/* same name, compare */
		FMformat_order sub_result;
		sub_result = FMformat_cmp(*subformats1,
					  subformats2[i]);
		if (sub_result == Format_Incompatible) {
		    tmp_result = Format_Incompatible;
		    goto free_lists;
		}
		if (sub_result != Format_Equal) {
		    /* subformats are not equal! */
		    if (tmp_result == Format_Equal) {
			/* we thought they were equal before */
			tmp_result = sub_result;
		    } else if (tmp_result != sub_result) {
			tmp_result = Format_Incompatible;
			goto free_lists;
		    }
		}
		break;
	    }
	    i++;
	}
	subformats1++;
    }

 free_lists:
    free_field_list(field_list1);
    free_field_list(field_list2);
    if (tmp_subformats) free(tmp_subformats);
    if (subformats2) free(subformats2);
    return tmp_result;
}

extern
char *
name_of_FMformat(format)
FMFormat format;
{
    return format->format_name;
}

extern FMStructDescList
format_list_of_FMFormat(FMFormat format)
{
    return format->master_struct_list;
}

extern FMdata_type
array_str_to_data_type(str, element_count_ptr)
const char *str;
long *element_count_ptr;
{
    FMdata_type ret_type;
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
field_type_eq(str1, str2)
const char *str1;
const char *str2;
{
    FMdata_type t1, t2;
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
	free(tmp_str1);
	free(tmp_str2);
	if ((lparen1 == NULL) || (lparen2 == NULL)) return -1;
	return (strcmp(lparen1, lparen2) == 0);
    }
    return ((t1 == t2) && (t1_count == t2_count));
}

extern char *
base_data_type(str)
const char *str;
{
    char *typ;
    while (isspace((int)*str) || (*str == '*') || (*str == '(')) {	/* skip preceeding space */
	str++;
    }
    typ = strdup(str);
    if (strchr(typ, '[') != NULL) {	/* truncate at array stuff */
	*strchr(typ, '[') = 0;
    }
    return typ;
}

extern FMdata_type
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
IOget_array_size_dimen(str, fields, dimen, control_field)
const char *str;
FMFieldList fields;
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
FMdata_type dat;
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

extern
void
get_IOformat_characteristics(format, ff, intf, column_major, pointer_size)
FMFormat format;
FMfloat_format *ff;
FMinteger_format *intf;
int *column_major;
int *pointer_size;
{
#ifdef WORDS_BIGENDIAN
    if (format->byte_reversal) {
	*intf = Format_Integer_littleendian;
    } else {
	*intf = Format_Integer_bigendian;
    }
#else
    if (format->byte_reversal) {
	*intf = Format_Integer_bigendian;
    } else {
	*intf = Format_Integer_littleendian;
    }
#endif
    *ff = format->float_format;
    *column_major = format->column_major_arrays;
    *pointer_size = format->pointer_size;
}

extern
int
pointer_size_of_IOformat(format)
FMFormat format;
{
    return format->pointer_size;
}

extern
FMContext
fmc_of_IOformat(format)
FMFormat format;
{
    return format->context;
}


extern void
dump_FMFormat(ioformat)
FMFormat ioformat;
{
    int index, i;
    printf("\tFormat \"%s\"; size = %d; Field_Count = %d; Endian = %d;\n\t\t  Pointer size = %d; Column_major_arrays = %d; Alignment = %d; File Version = %d; ",
	   ioformat->format_name, ioformat->record_length,
	   ioformat->field_count, ioformat->byte_reversal,
	   ioformat->pointer_size, ioformat->column_major_arrays,
	   ioformat->alignment, ioformat->IOversion);
    printf("Server ID = ");
    for (i = 0; i < ioformat->server_ID.length; i++) {
	printf("%02x", ((unsigned char *) ioformat->server_ID.value)[i]);
    }
    printf("\n");
    for (index = 0; index < ioformat->field_count; index++) {
	printf("\t\t%s \"%s\"; size = %d; offset = %d\n",
	       ioformat->field_list[index].field_name,
	       ioformat->field_list[index].field_type,
	       ioformat->field_list[index].field_size,
	       ioformat->field_list[index].field_offset);
    }
    if (ioformat->opt_info == NULL) {
	printf("\tNo Optional Format Info\n");
    } else {
	int i = 0;
	while (ioformat->opt_info[i].info_type != 0) {
	    int typ = ioformat->opt_info[i].info_type;
	    int j, text = 1, len = ioformat->opt_info[i].info_len;
/*	    if (ioformat->opt_info[i].info_type == COMPAT_OPT_INFO) {
		char *buffer = ioformat->opt_info[i].info_block;
		int fid_length = ID_length[version_of_format_ID(buffer)];
		printf("\t Opt Info Backwards Compatability\n");
		printf("\t\t Compatible with format ");
		print_server_ID((unsigned char *)buffer);
		printf("\t\t through transform code:  \n");
		printf("%s", buffer + fid_length);
		i++;
		continue;
		}*/
	    printf("\t Opt Info \"%c%c%c%c\", size %d, block begins:\n\t\t",
		   (typ >> 24) & 0xff, (typ >> 16) & 0xff, (typ >> 8) & 0xff,
		   typ & 0xff, len);
	    for(j=0; ((j < 10) && (j < len)); j++) {
		if (!isprint((int)((char*)ioformat->opt_info[i].info_block)[j])) text = 0;
	    }
	    if (text == 1) {
		printf("\"");
		for(j=0;((j < 50) && (j < len)); j++) {
		    printf("%c", ((char*)ioformat->opt_info[i].info_block)[j]);
		}
		printf("\"\n");
	    } else {
		for(j=0;((j < 20) && (j < len)); j++) {
		    printf("%02x ", ((unsigned char*)ioformat->opt_info[i].info_block)[j]);
		}
		printf("\n");
	    }
	    i++;
	}
    }
}

extern void
dump_FMFormat_as_XML(ioformat)
FMFormat ioformat;
{
    int index, i;
    printf("<FMFormat>\n");
    printf("<formatID>%d</formatID>\n", ioformat->format_index);
    printf("<formatName>%s</formatName>\n", ioformat->format_name);
    printf("<recordLength>%d</recordLength>\n", ioformat->record_length);
    printf("<fieldCount>%d</fieldCount>\n", ioformat->field_count);
    printf("<byteReversal>%d</byteReversal>\n", ioformat->byte_reversal);
    printf("<alignment>%d</alignment>\n", ioformat->alignment);
    printf("<columnMajorArrays>%d</columnMajorArrays>\n", ioformat->column_major_arrays);
    printf("<pointerSize>%d</pointerSize>\n", ioformat->pointer_size);
    printf("<IOversion>%d</IOversion>\n", ioformat->IOversion);
    printf("<serverID>");
    for (i = 0; i < ioformat->server_ID.length; i++) {
	printf("%02x", ((unsigned char *) ioformat->server_ID.value)[i]);
    }
    printf("</serverID>\n");
    for (index = 0; index < ioformat->field_count; index++) {
	printf("<IOField>\n");
	printf("<fieldName>%s</fieldName>\n<fieldType>%s</fieldType>\n<fieldSize>%d</fieldSize>\n<fieldOffset>%d</fieldOffset>\n",
	       ioformat->field_list[index].field_name,
	       ioformat->field_list[index].field_type,
	       ioformat->field_list[index].field_size,
	       ioformat->field_list[index].field_offset);
    }

}

extern void
add_ref_FMcontext(c)
FMContext c;
{
    c->ref_count++;
}

extern void
free_FMcontext(c)
FMContext c;
{
    int i;
    c->ref_count--;
    if (c->ref_count != 0) return;
    for (i = 0; i < c->reg_format_count; i++) {
	free_FMformat(c->format_list[i]);
    }
    free(c->format_list);
    free(c);
}

#define DUMP
#ifdef DUMP
static void
free_FMfield(ioformat, field, data, string_base, encode, verbose)
FMFormat ioformat;
int field;
void *data;
void *string_base;
int encode;
int verbose;
{
    FMFieldList iofield = &ioformat->field_list[field];
    FMVarInfoList iovar = &ioformat->var_list[field];
    int field_offset = iofield->field_offset;
    int field_size = iofield->field_size;
    const char *field_type = iofield->field_type;
    int data_offset = field_offset;
    int byte_reversal = ioformat->byte_reversal;
    int dimension;
    FMFormat subformat = NULL;
    int sub_field_size;
 
    if ((iovar->string == 0) &&
	(iovar->var_array == 0) &&
	(iovar->data_type != unknown_type)) {
	/* must be simple data type or array of simple data types */
	return;
    }
    dimension = FMget_array_element_count(ioformat, iovar, data, encode);
    if (iovar->var_array) {
	FMgetFieldStruct descr;  /* OK */
	long tmp_offset;
	descr.offset = iofield->field_offset;
	descr.size = ioformat->pointer_size;
	descr.data_type = integer_type;
	descr.byte_swap = byte_reversal;
	
	tmp_offset = get_FMlong(&descr, data);
	data = (void *) tmp_offset;
	data_offset = 0;
	sub_field_size = iofield->field_size;
    } else {
	if (ioformat->IOversion <= 3) {
	    sub_field_size = iofield->field_size / dimension;
	} else {
	    sub_field_size = iofield->field_size;
	}
    }
    if (!iovar->string) { 
	/* must be substructured */
	char *typ = base_data_type(field_type);
	subformat = ioformat->field_subformats[field];
	free(typ);
    }
    if (iovar->string || (subformat && subformat->variant)){
	for (; dimension > 0; dimension--) {
	    if (iovar->string) {
		char *tmp_str;
		FMgetFieldStruct descr;  /* OK */
		descr.offset = data_offset;
		descr.size = field_size;
		descr.data_type = string_type;
		descr.byte_swap = 0;
		tmp_str = (char *) get_FMaddr(&descr, data, string_base, encode);
		free(tmp_str);
	    } else if (subformat != NULL) {
		FMfree_var_rec_elements(subformat, (char*)data + data_offset);
	    }
	    data_offset += sub_field_size;
	}
    }
    if (iovar->var_array) {
	free(data);
    }    
}

extern void
FMfree_var_rec_elements(ioformat, data)
FMFormat ioformat;
void *data;
{
    int index;
    if (ioformat->variant == 0) return;  /* nothing to do */
    for (index=0; index < ioformat->field_count; index++) {
	free_FMfield(ioformat, index, data, data, 0, 1);
    }
}

static int
dump_FMfield(FMFormat top_format, FMFormat ioformat, int index, void *data,
		   void *string_base, int encode, int verbose);
static int
dump_limited_FMfield(FMFormat top_format, FMFormat ioformat, int index, 
			   void *data, void *string_base, int encode, 
			   int verbose, int char_limit);

extern void
dump_FMfield_as_XML(FMFormat top_format, FMFormat ioformat, int index, void *data,
			  void *string_base, int encode, int verbose);

extern void
dump_raw_FMrecord(fmc, ioformat, data)
FMContext fmc;
FMFormat ioformat;
void *data;
{
    int index;
    if (IOdumpVerbose)
	printf("Record type %s :", ioformat->format_name);
    for (index = 0; index < ioformat->field_count; index++) {
	(void) dump_FMfield(ioformat, ioformat, index, data, data, 1,
			    IOdumpVerbose);
    }
    printf("\n");
}

extern int
FMdump_data(format, data, character_limit)
FMFormat format;
void *data;
int character_limit;
{
    int index;
    int char_count = 0;
    for (index = 0; index < format->field_count; index++) {
	if ((character_limit > 0) && (char_count > character_limit))
	    break;
	char_count += dump_limited_FMfield(format, format, index, data, 
					   data, 0, 1, 
					   character_limit - char_count);
    }
    printf("\n");
    return ((character_limit > 0) && (char_count > character_limit));
}

extern int
FMdump_encoded_data(format, data, character_limit)
FMFormat format;
void *data;
int character_limit;
{
    int index;
    int char_count = 0;
    int header_size = format->server_ID.length;
    if (format->variant) {
	header_size += sizeof(INT4);
    }
    header_size += (8 - header_size) & 0x7;
    data = (char*)data + header_size;

    for (index = 0; index < format->field_count; index++) {
	if ((character_limit > 0) && (char_count > character_limit))
	    break;
	char_count += dump_limited_FMfield(format, format, index, data, 
					   data, 1, 1, 
					   character_limit - char_count);
    }
    printf("\n");
    return ((character_limit > 0) && (char_count > character_limit));
}

extern void
dump_unencoded_FMrecord_as_XML(fmc, format, data)
FMContext fmc;
FMFormat format;
void *data;
{
    int index;
    if (FMhas_XML_info(format)) {
/*GSE	FMdump_XML(format, data, -1);*/
	return;
    }
    printf("<%s>\n", format->format_name);
    for (index = 0; index < format->field_count; index++) {
	dump_FMfield_as_XML(format, format, index, data, data, 0, 1);
    }
    printf("</%s>\n", format->format_name);
}


/* big enough for most single values */
#define MAX_VALUE_SIZE 512

extern int
sdump_value(str, field_type, field_size, field_offset, top_format, data,
	    string_base, byte_reversal, float_format, encode)
char *str;
const char *field_type;
int field_size;
int field_offset;
FMFormat top_format;
void *data;
void *string_base;
int byte_reversal;
int float_format;
int encode;
{
    FMgetFieldStruct descr;  /*OK */
    descr.offset = field_offset;
    descr.size = field_size;
    descr.data_type = str_to_data_type(field_type);
    descr.byte_swap = byte_reversal;
    descr.src_float_format = float_format ;
    descr.target_float_format = fm_my_float_format;

    if (descr.data_type == integer_type) {
	if (field_size <= sizeof(long)) {
	    long tmp = get_FMlong(&descr, data);
	    sprintf(str, "%ld", tmp);
	} else if (field_size == 2 * sizeof(long) && field_size == 8) {
	    unsigned long low_long;
	    long high_long;
	    get_FMlong8(&descr, data, &low_long, &high_long);
	    if (high_long == 0) {
		sprintf(str, "%ld", low_long);
	    } else {
		sprintf(str, "0x%lx%08lx", high_long, low_long);
	    }
	} else if (field_size > sizeof(long)) {
	    sprintf(str, "+long int+");
	} else {
	    sprintf(str, "+int size %d+", field_size);
	}
    } else if (descr.data_type == unsigned_type) {
	if (field_size <= sizeof(unsigned long)) {
	    unsigned long tmp = get_FMulong(&descr, data);
	    sprintf(str, "%lu", tmp);
	} else if (field_size == 2 * sizeof(long) && field_size == 8) {
	    unsigned long low_long, high_long;
	    get_FMulong8(&descr, data, &low_long, &high_long);
	    if (high_long == 0) {
		sprintf(str, "%lu", low_long);
	    } else {
		sprintf(str, "0x%lx%08lx", high_long, low_long);
	    }
	} else if (field_size > sizeof(long)) {
	    sprintf(str, "+ulong int+");
	} else {
	    sprintf(str, "+uint size %u+", field_size);
	}
    } else if (descr.data_type == enumeration_type) {
	unsigned long tmp = get_FMulong(&descr, data);
	sprintf(str, "%lu", tmp);
    } else if (descr.data_type == boolean_type) {
	unsigned long tmp = get_FMulong(&descr, data);
	sprintf(str, "%lu", tmp);
	if (tmp == 0) {
	    strcpy(str, "false");
	} else {
	    strcpy(str, "true");
	}
    } else if (descr.data_type == float_type) {
	if (field_size == sizeof(float)) {
	    float tmp = get_FMfloat(&descr, data);
	    sprintf(str, "%g", tmp);
	} else if (field_size == sizeof(double)) {
	    double tmp = get_FMdouble(&descr, data);
	    sprintf(str, "%g", tmp);
#if SIZEOF_LONG_DOUBLE != 0 && SIZEOF_LONG_DOUBLE != SIZEOF_DOUBLE
	} else if (field_size == sizeof(long double)) {
	    long double tmp;
	    memcpy(&tmp, (float *) ((char *) data + field_offset),
		   sizeof(double));
	    sprintf(str, "%Lg", tmp);
#endif
	} else {
	    if (field_size < sizeof(float)) {
		sprintf(str, "+tiny float+");
	    } else if (field_size > sizeof(double)) {
		sprintf(str, "+big float+");
	    } else {
		sprintf(str, "+float size %u+", field_size);
	    }
	}
    } else if (descr.data_type == char_type) {
	sprintf(str, "%c", *(char *) ((char *) data + field_offset));
    } else if (descr.data_type == string_type) {
	char *tmp_str = (char *) get_FMaddr(&descr, data, string_base, encode);
	if (tmp_str == 0) {
	    sprintf(str, "\"\"");
	} else {
	    if (strlen(tmp_str) + 3 < MAX_VALUE_SIZE) {
		sprintf(str, "\"%s\"", tmp_str);
	    } else {
		str[0] = '"';
		strncpy(&str[1], tmp_str, MAX_VALUE_SIZE - 10);
		str[MAX_VALUE_SIZE - 10] = 0;
		strcat(str, " ... \"");
	    }
	}
    } else if (strcmp(field_type, "R3vector") == 0) {
	sprintf(str, "(%g, %g, %g)", *(double *) ((char *) data + field_offset),
	     *(double *) ((char *) data + field_offset + sizeof(double)),
	*(double *) ((char *) data + field_offset + 2 * sizeof(double)));
    } else {
	return 0;
    }
    return 1;
}

static int
dump_value(field_type, field_size, field_offset, top_format, data, string_base,
	   byte_reversal, float_format, encode, verbose, char_limit)
char *field_type;
int field_size;
int field_offset;
FMFormat top_format;
void *data;
void *string_base;
int byte_reversal;
int float_format;
int encode;
int verbose;
int char_limit;
{
    char buf[MAX_VALUE_SIZE];	/* surely big enough for a single value? */
    FMFormat format;
    int char_count = 0;
    if (sdump_value(buf, field_type, field_size, field_offset, top_format, data,
		    string_base, byte_reversal, float_format, encode)) {
	printf("%s", buf);
	char_count = strlen(buf);
    } else {
	char *typ = base_data_type(field_type);
	int i = 0;
	format = NULL;
	while(top_format->subformats && top_format->subformats[i]) {
	    char *name = top_format->subformats[i]->format_name;
	    if (0 == strcmp(name, typ)) {
		format = top_format->subformats[i];
	    }
	    i++;
	}
	if (format != NULL) {
	    int index;
	    printf("{ ");
	    for (index = 0; index < format->field_count; index++) {
		char_count +=
		    dump_limited_FMfield(top_format, format, index,
				 (void *) ((char *) data + field_offset),
					 string_base, encode, verbose,
					 char_limit - char_count);
		if ((char_limit != -1) && (char_count > char_limit)) {
		    printf(" ... } ");
		    return char_count;
		}
	    }
	    printf("} ");
	    char_count += 4;
	} else {
	    printf("Unknown type");
	}
	free(typ);
    }
    return char_count;
}

static int
dump_FMfield(top_format, format, field, data, string_base, encode, verbose)
FMFormat top_format;
FMFormat format;
int field;
void *data;
void *string_base;
int encode;
int verbose;
{
    return dump_limited_FMfield(top_format, format, field, data, string_base, 
				encode, verbose, -1);
}

extern long
FMget_array_element_count(FMFormat f, FMVarInfoList var, char *data, int encode)
{
    int i;
    long count = 1;
    long tmp;
    for (i = 0; i < var->dimen_count; i++) {
	if (var->dimens[i].static_size != 0) {
	    count = count * var->dimens[i].static_size;
	} else {
	    int field = var->dimens[i].control_field_index;
	    struct _FMgetFieldStruct tmp_src_spec;
	    memset(&tmp_src_spec, 0, sizeof(tmp_src_spec));
	    tmp_src_spec.size = f->field_list[field].field_size;
	    tmp_src_spec.offset = f->field_list[field].field_offset;
	    tmp_src_spec.data_type = integer_type;
	    tmp_src_spec.byte_swap = f->byte_reversal;
	    if (!encode) {
		tmp_src_spec.byte_swap = 0;
		tmp_src_spec.src_float_format = tmp_src_spec.target_float_format;
	    }
	    tmp = get_FMlong(&tmp_src_spec, data);
	    count = count * tmp;
	}
    }
    return count;
}

static int
dump_limited_FMfield(top_format, format, field, data, string_base, encode, verbose, char_limit)
FMFormat top_format;
FMFormat format;
int field;
void *data;
void *string_base;
int encode;
int verbose;
int char_limit;
{
    FMFieldList fmfield = &format->field_list[field];
    FMVarInfoList iovar = &format->var_list[field];
    int field_offset = fmfield->field_offset;
    int field_size = fmfield->field_size;
    const char *field_type = fmfield->field_type;
    const char *field_name = fmfield->field_name;
    int char_count = 0;
    int byte_reversal = format->byte_reversal;
    int float_format = format->float_format;
    char *left_paren = NULL;
    char *temp_ptr = 0;

    if (encode == 0) byte_reversal = 0;

    if (verbose) {
	printf("%s = ", field_name);
	char_count += strlen(field_name) + 3;
    }
    if ((left_paren = strchr(field_type, '[')) == NULL) {
	char_count +=
	    dump_value(field_type, field_size, field_offset, top_format, data,
		       string_base, byte_reversal, float_format, encode, 
		       verbose, char_limit);
    } else if (strchr(left_paren + 1, '[') == NULL) {
	/* single dimen array */
	long dimension = 0;
	char sub_type[64];
	int sub_field_size;
	int offset = fmfield->field_offset;
	printf("{ ");
	*left_paren = 0;
	strcpy(sub_type, field_type);
	*left_paren = '[';
	dimension = strtol(left_paren + 1, NULL, 10);

	if ((dimension == LONG_MIN) || (dimension == LONG_MAX) ||
	    (dimension == 0)) {
	    if (iovar->var_array != 1) {
		fprintf(stderr, "Couldn't parse array size in \"%s\"\n",
			field_type);
		return char_count;
	    } else {
		FMgetFieldStruct descr;  /* OK */
		long tmp_offset;

		dimension = FMget_array_element_count(format, iovar, data, encode);

		descr.offset = fmfield->field_offset;
		descr.size = format->pointer_size;
		descr.data_type = integer_type;
		descr.byte_swap = byte_reversal;
		tmp_offset = get_FMlong(&descr, data);
		if (encode) {
		    data = (char *) string_base + tmp_offset;
		} else {
		    data = (void *) tmp_offset;
		}
		offset = 0;
	    }
	    sub_field_size = fmfield->field_size;
	} else {
	    /* normal internal array */
	    if (format->IOversion <= 3) {
		sub_field_size = fmfield->field_size / dimension;
	    } else {
		sub_field_size = fmfield->field_size;
	    }
	}
	for (; dimension > 0; dimension--) {
	    char_count +=
		dump_value(sub_type, sub_field_size, offset, top_format, data,
			   string_base, byte_reversal, float_format, encode, 
			   verbose, char_limit);
	    offset += sub_field_size;
	    if ((char_limit != -1) && (char_count > char_limit)) {
		printf(" ... ");
		return char_count;
	    }
	    if (dimension != 1)
		printf(", ");
	}
	printf("}");
    } else {
	/* double dimen array */
	long dimension1 = 0;
	long dimension2 = 0;
	char sub_type[64];
	int sub_field_size, offset = fmfield->field_offset;
	printf("{ ");
	*left_paren = 0;
	strcpy(sub_type, field_type);
	dimension1 = strtol(left_paren + 1, &temp_ptr, 10);
	dimension2 = strtol(temp_ptr + 2, &temp_ptr, 10);

	if ((dimension2 == LONG_MIN) || (dimension2 == LONG_MAX) ||
	    (dimension2 == 0) || (dimension1 == 0)) {
	    *left_paren = '[';
	    fprintf(stderr, "Couldn't parse array size in \"%s\"\n",
		    field_type);
	    return char_count;
	}
	*left_paren = '[';
	if (format->IOversion <= 3) {
	    sub_field_size = fmfield->field_size / (dimension1 * dimension2);
	} else {
	    sub_field_size = fmfield->field_size;
	}
	for (; dimension2 > 0; dimension2--) {
	    int i = 0;
	    printf("\n\t{ ");
	    for (; i < dimension1; i++) {
		char_count +=
		    dump_value(sub_type, sub_field_size, offset, top_format,
			       data, string_base, byte_reversal, float_format,
			       encode, verbose, char_limit);
		offset += sub_field_size;
		if ((char_limit != -1) && (char_count > char_limit)) {
		    printf(" ... ");
		    return char_count;
		}
		if (i != dimension1 - 1)
		    printf(", ");
	    }
	    printf("}");
	}
	printf("}");
    }
    if (verbose)
	printf("; ");
    else
	printf(" ");

    return char_count;
}


extern int
sdump_value_as_XML(str, field_type, field_size, field_offset, top_format, data,
		   string_base, byte_reversal, float_format, encode)
char *str;
char *field_type;
int field_size;
int field_offset;
FMFormat top_format;
void *data;
void *string_base;
int byte_reversal;
int float_format;
int encode;
{
    FMgetFieldStruct descr;  /*OK */
    descr.offset = field_offset;
    descr.size = field_size;
    descr.data_type = str_to_data_type(field_type);
    descr.byte_swap = byte_reversal;
    descr.src_float_format = float_format;
    descr.target_float_format = fm_my_float_format;

    if (descr.data_type == integer_type) {
	if (field_size <= sizeof(long)) {
	    long tmp = get_FMlong(&descr, data);
	    sprintf(str, "%ld", tmp);
	} else if (field_size == 2 * sizeof(long) && field_size == 8) {
	    unsigned long low_long;
	    long high_long;
	    get_FMlong8(&descr, data, &low_long, &high_long);
	    if (high_long == 0) {
		sprintf(str, "%ld", low_long);
	    } else {
		sprintf(str, "0x%lx%08lx", high_long, low_long);
	    }
	} else if (field_size > sizeof(long)) {
	    sprintf(str, "<scalar type=\"long\" />");
	} else {
	    sprintf(str, "<scalar type=\"int\" size=\"%d\" />", field_size);
	}
    } else if (descr.data_type == unsigned_type) {
	if (field_size <= sizeof(unsigned long)) {
	    unsigned long tmp = get_FMulong(&descr, data);
	    sprintf(str, "%lu", tmp);
	} else if (field_size == 2 * sizeof(long) && field_size == 8) {
	    unsigned long low_long, high_long;
	    get_FMulong8(&descr, data, &low_long, &high_long);
	    if (high_long == 0) {
		sprintf(str, "%lu", low_long);
	    } else {
		sprintf(str, "0x%lx%08lx", high_long, low_long);
	    }
	} else if (field_size > sizeof(long)) {
	    sprintf(str, "<scalar type=\"unsignedLong\" />");
	} else {
	    sprintf(str, "<scalar type=\"unsignedInt\" size=\"%d\" />", field_size);
	}
    } else if (descr.data_type == enumeration_type) {
	sprintf(str, "%u", *(int *) ((char *) data + field_offset));
    } else if (descr.data_type == boolean_type) {
	if (*(int *) ((char *) data + field_offset) == 0) {
	    strcpy(str, "false");
	} else {
	    strcpy(str, "true");
	}
    } else if (descr.data_type == float_type) {
	if (field_size == sizeof(float)) {
	    float tmp = get_FMfloat(&descr, data);
	    sprintf(str, "%g", tmp);
	} else if (field_size == sizeof(double)) {
	    double tmp = get_FMdouble(&descr, data);
	    sprintf(str, "%g", tmp);
#if SIZEOF_LONG_DOUBLE != 0 && SIZEOF_LONG_DOUBLE != SIZEOF_DOUBLE
	} else if (field_size == sizeof(long double)) {
	    long double tmp;
	    memcpy(&tmp, (float *) ((char *) data + field_offset),
		   sizeof(double));
	    sprintf(str, "%Lg", tmp);
#endif
	} else {
	    if (field_size < sizeof(float)) {
		sprintf(str, "<scalar type=\"small-float\" />");
	    } else if (field_size > sizeof(double)) {
		sprintf(str, "<scalar type=\"big-float\" />");
	    } else {
		sprintf(str, "<scalar type=\"float\" size=\"%u\" />", field_size);
	    }
	}
    } else if (descr.data_type == char_type) {
	sprintf(str, "%c", *(char *) ((char *) data + field_offset));
    } else if (descr.data_type == string_type) {
	char *tmp_str = (char *) get_FMaddr(&descr, data, string_base, encode);
	if (tmp_str != 0) {
	    if (strlen(tmp_str) + 3 < MAX_VALUE_SIZE) {
		sprintf(str, "%s", tmp_str);
	    } else {
		sprintf(str, "%s", tmp_str);
		/* 
		 * str[0] = '"'; strncpy(&str[1], tmp_str,
		 * MAX_VALUE_SIZE-10); str[MAX_VALUE_SIZE-10] = 0;
		 * strcat(str, " ... \""); */
	    }
	}
    } else {
	return 0;
    }
    return 1;
}
#endif

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

/* 
 * ** Code below this point relates to operations to and from the format
 * server  */

#define FILE_INT INT4

static int
put_serverAtomicInt(fd, file_int_ptr, fmc)
void *fd;
FILE_INT *file_int_ptr;
FMContext fmc;
{
#if SIZEOF_INT == 4
    int tmp_value = *file_int_ptr;
    int junk_errno;
    char *junk_result_str;
    if (os_server_write_func(fd, &tmp_value, 4, &junk_errno, &junk_result_str) != 4)
	return 0;
#else
    Baaad shit;
#endif
    return 1;
}

static int
get_serverAtomicInt(fd, file_int_ptr, byte_reversal)
void *fd;
FILE_INT *file_int_ptr;
int byte_reversal;
{
#if SIZEOF_INT == 4
    int tmp_value;
    int junk_errno;
    char *junk_result_str;
    if (os_server_read_func(fd, &tmp_value, 4, &junk_errno, &junk_result_str) != 4)
	return 0;
#else
    Baaad shit;
#endif
    if (byte_reversal)
	byte_swap((char *) &tmp_value, 4);
    *file_int_ptr = tmp_value;
    return 1;
}

extern int
unix_timeout_read_func(void *conn, void *buffer, int length, 
		       int *errno_p, char **result_p);

static int
get_serverTimeoutInt(fd, file_int_ptr, byte_reversal)
void *fd;
FILE_INT *file_int_ptr;
int byte_reversal;
{
#if SIZEOF_INT == 4
    int tmp_value;
    int junk_errno;
    char *junk_result_str;
    if (unix_timeout_read_func(fd, &tmp_value, 4, &junk_errno, &junk_result_str) != 4)
	return 0;
#else
    Baaad shit;
#endif
    if (byte_reversal)
	byte_swap((char *) &tmp_value, 4);
    *file_int_ptr = tmp_value;
    return 1;
}

static int server_read_failure = 0;

extern int
serverAtomicRead(fd, buffer, length)
void *fd;
void *buffer;
int length;
{
    char *junk_result_str = NULL;
    int junk_errno;
    int ret = os_server_read_func(fd, buffer, length, &junk_errno,
				  &junk_result_str);

    if (ret != length) {
	if (format_server_verbose == 1) {
	    printf("server read error, return is %d, length %d, errno %d\n",
		   ret, length, junk_errno);
	    if (junk_result_str != NULL) {
		printf("result_string is %s\n", junk_result_str);
	    }
	}
	server_read_failure++;
    }
    return ret;
}

extern int
serverAtomicWrite(fd, buffer, length)
void *fd;
void *buffer;
int length;
{
    char *junk_result_str;
    int junk_errno;
    return os_server_write_func(fd, buffer, length, &junk_errno,
				&junk_result_str);
}


static void
provisional_use_warning(fd)
int fd;
{
    static int warned = 0;

    if (warned)
	return;
    warned++;

    fprintf(stderr, "The contacted format_server daemon allows only temporary use.\n");
    fprintf(stderr, " See http://www.cc.gatech.edu/systems/projects/MOSS/servers.html for more info.\n");
}

static void
dump_server_error(char *string, FMContext context)
{
}

static int
server_register_format(fmc, format)
FMContext fmc;
FMFormat format;
{
    int tries = 0;

    if (fmc->master_context != NULL) {
	return server_register_format((FMContext) fmc->master_context,
				      format);
    }
  retry:
    if (tries++ > 2) {
	dump_server_error("Failed to contact format server\n", fmc);
    }
    if (establish_server_connection_ptr == NULL) {
	assert(0);
    }
    if (establish_server_connection_ptr(fmc, 1) == 0) {
	if (establish_server_connection_ptr(fmc, 1) == 0) {
	    dump_server_error("Failed to contact format server\n", fmc);
	    return 0;
	}
    } else {
	struct {
	    char reg[2];
	    unsigned short len;
	    } tmp = {{'f', 2}, 0 };	/* format reg, version 2 */
	int ret;
	int errno;
	char *errstr;
	char ret_info[2];

	format_rep rep = format->server_format_rep;

	tmp.len = rep->format_rep_length;
	ret = os_server_write_func(fmc->server_fd, &tmp, sizeof(tmp),
				   &errno, &errstr);
	if (ret != sizeof(tmp)) 
	    goto retry;

	ret = os_server_write_func(fmc->server_fd, 
				   (char*) rep + sizeof(tmp.len),
				   ntohs(rep->format_rep_length) - sizeof(tmp.len),
				   &errno, &errstr);
	if (ret != ntohs(rep->format_rep_length) - sizeof(tmp.len))
	    goto retry;

	if (serverAtomicRead(fmc->server_fd, &ret_info[0], 2) != 2) {
	    dump_server_error("Read failed for format server.  Out of domain?\n",
			      fmc);
	    return 0;
	}
	if (ret_info[0] == 'P') {
	    provisional_use_warning((int) (long) fmc->server_fd);
	} else {
	    if (ret_info[0] != 'I') {
		printf("Bad character from format server %c\n", ret_info[0]);
		return 0;
	    }
	}
	format->server_ID.length = ret_info[1];
	format->server_ID.value = malloc(format->server_ID.length);
	if (serverAtomicRead(fmc->server_fd, (void *) format->server_ID.value,
			     format->server_ID.length)
	    != format->server_ID.length) {
	    goto retry;
	}
    }

    if (format->context != fmc) {
	/* registering in a master, copy it down */
	assert(0);
    }
    return 1;
}

static const char xchars[] = "0123456789abcdef";
#define nibble2hex(val) (xchars[val & 0x0f])

static char *
stringify_field_type(type, base_format, buffer, size)
const char *type;
FMFormat base_format;
char *buffer;
int size;
{
    char *index_start;
    unsigned char *server_ID;

    if (base_format == NULL) {
	strcpy(buffer, type);
	return (char*)type;
    }
    if (strchr(type, ':') != NULL) {
	strcpy(buffer, type);
	return (char*)type;		/* already stringified */
    }
    server_ID = (unsigned char *) base_format->server_ID.value;

    index_start = strchr(type, '[');
    assert(strlen(type) + 2 * base_format->server_ID.length + 2 <= (unsigned int) size);
    if (base_format->server_ID.length == 8) {
	if (index_start == NULL) {
	    sprintf(buffer, "%s:%02x%02x%02x%02x:%02x%02x%02x%02x", type,
		  server_ID[0], server_ID[1], server_ID[2], server_ID[3],
		 server_ID[4], server_ID[5], server_ID[6], server_ID[7]);
	} else {
	    *index_start = 0;
	    sprintf(buffer, "%s:%02x%02x%02x%02x:%02x%02x%02x%02x[%s", type,
		  server_ID[0], server_ID[1], server_ID[2], server_ID[3],
		  server_ID[4], server_ID[5], server_ID[6], server_ID[7],
		    (index_start + 1));
	    *index_start = '[';
	}
    } else {
	int i;
	char *end;
	strcpy(buffer, type);
	index_start = strchr(buffer, '[');
	if (index_start != NULL)
	    *index_start = 0;
	strcat(buffer, ":");
	end = buffer + strlen(buffer);
	for (i = 0; i < base_format->server_ID.length; i++) {
	    end[2 * i] = nibble2hex(base_format->server_ID.value[i] >> 4);
	    end[2 * i + 1] = nibble2hex(base_format->server_ID.value[i]);
	}
	end[2 * i] = 0;
	if (index_start != NULL) {
	    index_start = strchr(type, '[');
	    strcat(buffer, index_start);
	}
    }
    return buffer;
}

extern int
global_name_eq(format1, format2)
FMFormat format1;
FMFormat format2;
{
    if (format1->server_ID.length != format2->server_ID.length)
	return 0;
    return !memcmp(format1->server_ID.value, format2->server_ID.value,
		   format1->server_ID.length);
}

extern
char *
global_name_of_FMFormat(format)
FMFormat format;
{
    int size = strlen(format->format_name) + 3 +
    2 * format->server_ID.length;
    char *buffer = malloc(size);
    return stringify_field_type(format->format_name, format, buffer, size);
}

extern int
write_format_to_fd(fd, format)
void *fd;
FMFormat format;
{
    FILE_INT name_length, field_count, record_length;
    int field;
    int result = 1;
    int record_byte_order;
    int pointer_size;
    int IOversion;

    if ((!format) || (!format->format_name)) {
	int zero = 0;
	result = put_serverAtomicInt(fd, &zero, NULL);
	return !result;
    }
    name_length = strlen(format->format_name);
    field_count = format->field_count;
    record_length = format->record_length;
    pointer_size = format->pointer_size;
    IOversion = format->IOversion;
    if (format->byte_reversal) {
	record_byte_order = OTHER_BYTE_ORDER;
    } else {
	record_byte_order = OUR_BYTE_ORDER;
    }
    result &= put_serverAtomicInt(fd, &name_length, NULL);
    result &= put_serverAtomicInt(fd, &field_count, NULL);
    result &= put_serverAtomicInt(fd, &record_length, NULL);
    result &= put_serverAtomicInt(fd, &record_byte_order, NULL);
    result &= put_serverAtomicInt(fd, &pointer_size, NULL);
    result &= put_serverAtomicInt(fd, &IOversion, NULL);
    if (!result)
	return 0;
    if (serverAtomicWrite(fd, format->format_name, name_length + 1) !=
	name_length + 1)
	return 0;

    for (field = 0; field < format->field_count; field++) {
	FILE_INT field_name_len, field_type_len, field_size, field_offset;
	FMField *fmfield = &(format->field_list[field]);
	char field_type_buffer[512];
	char *ftype;
	FMFormat subformat = NULL;
	if (format->var_list != NULL) {
	    subformat = format->field_subformats[field];
	}
	memset(field_type_buffer, 0, sizeof(field_type_buffer));
	ftype = stringify_field_type(fmfield->field_type, subformat,
				     field_type_buffer,
				     (int) sizeof(field_type_buffer));
	field_name_len = strlen(fmfield->field_name);
	field_type_len = strlen(ftype);
	field_size = fmfield->field_size;
	field_offset = fmfield->field_offset;
	result &= put_serverAtomicInt(fd, &field_name_len, NULL);
	result &= put_serverAtomicInt(fd, &field_type_len, NULL);
	result &= put_serverAtomicInt(fd, &field_size, NULL);
	result &= put_serverAtomicInt(fd, &field_offset, NULL);
	if (!result)
	    return 0;
	if (serverAtomicWrite(fd, fmfield->field_name, field_name_len + 1) !=
	    field_name_len + 1)
	    return 0;
	if (serverAtomicWrite(fd, ftype, field_type_len + 1) !=
	    field_type_len + 1)
	    return 0;
    }
    return 1;
}

extern FMFormat
expand_subformat_from_rep(rep)
struct _subformat_wire_format *rep;
{
    FMFormat format = new_FMFormat();
    struct _field_wire_format *fields;
    int field;
    UINT2 tmp;
    INT4 tmp2;
    int byte_reversal = ((rep->f.f0.record_byte_order & 0x1) != OUR_BYTE_ORDER);

    tmp = rep->f.f0.name_offset;
    if (byte_reversal) byte_swap((char*)&tmp, 2);
    format->format_name = malloc(strlen((char *) rep + tmp) + 1);
    strcpy(format->format_name, (char *) rep + tmp);
    tmp = rep->f.f0.field_count;
    if (byte_reversal) byte_swap((char*)&tmp, 2);
    format->field_count = tmp;
    format->variant = 0;
    tmp2 = rep->f.f0.record_length;
    if (byte_reversal) byte_swap((char*)&tmp2, 4);
    format->record_length = tmp2;
    format->byte_reversal = ((rep->f.f0.record_byte_order & 0x1) != OUR_BYTE_ORDER);
    format->pointer_size = rep->f.f0.pointer_size;
    tmp = rep->f.f0.floating_point_rep;
    if (byte_reversal) byte_swap((char*)&tmp, 2);
    format->float_format = (FMfloat_format) tmp;
    if (format->float_format == Format_Unknown) {
	/* old data must be pure-endian IEEE 754*/
	if (rep->f.f0.record_byte_order == 1) {
	    /* bigendian */
	    format->float_format = Format_IEEE_754_bigendian;
	} else {
	    format->float_format = Format_IEEE_754_littleendian;
	}
    }
    format->field_list = (FMFieldList) malloc(sizeof(FMField) *
				      (format->field_count + 1));
    format->var_list = NULL;

    if (rep->f.f0.server_rep_version == 0) {
	fields = (struct _field_wire_format *) 
	    ((char*)rep + sizeof(struct _subformat_wire_format_0));
    } else {
	fields = (struct _field_wire_format *) 
	    ((char*) rep + rep->f.f0.header_size);
    }
    for (field = 0; field < format->field_count; field++) {
	FMField *fmfield = &(format->field_list[field]);
	struct _field_wire_format *wire = &fields[field];
	tmp = wire->field_name_offset;
	if (byte_reversal) byte_swap((char*)&tmp, 2);
	fmfield->field_name = malloc(strlen((char *) rep + tmp) + 1);
	strcpy((char*)fmfield->field_name, (char *) rep + tmp);
	tmp = wire->field_type_offset;
	if (byte_reversal) byte_swap((char*)&tmp, 2);
	fmfield->field_type = malloc(strlen((char *) rep + tmp) + 1);
	strcpy((char*)fmfield->field_type, (char *) rep + tmp);
	fmfield->field_size = wire->field_size;
	if (byte_reversal) byte_swap((char*)&fmfield->field_size, 4);
	fmfield->field_offset = wire->field_offset;
	if (byte_reversal) byte_swap((char*)&fmfield->field_offset, 4);
    }
    format->field_list[format->field_count].field_size = 0;
    format->field_list[format->field_count].field_offset = 0;
    format->field_list[format->field_count].field_name = NULL;
    format->field_list[format->field_count].field_type = NULL;
    format->column_major_arrays = 0;
    {
	struct _opt_info_wire_format tmp_info;
	int offset, info_count = 0;

	format->alignment = rep->f.f0.alignment;
	format->column_major_arrays = rep->f.f0.column_major_arrays;
	tmp = rep->f.f0.opt_info_offset;
	if (byte_reversal) byte_swap((char*)&tmp, 2);

	if (tmp != 0) {
	    offset = tmp;
	    format->opt_info = malloc(sizeof(FMOptInfo));
	    do {
		memcpy(&tmp_info, offset + (char*) rep, sizeof(tmp_info));
		if (tmp_info.info_type != 0) {
		    format->opt_info = 
			realloc(format->opt_info,
				sizeof(FMOptInfo) * (info_count + 2));
		    tmp2 = tmp_info.info_type;
		    if (byte_reversal) byte_swap((char*)&tmp2, 4);
		    format->opt_info[info_count].info_type = tmp2;
			
		    tmp2 = tmp_info.info_len;
		    if (byte_reversal) byte_swap((char*)&tmp2, 4);
		    format->opt_info[info_count].info_len = tmp2;

		    tmp2 = tmp_info.info_offset;
		    if (byte_reversal) byte_swap((char*)&tmp2, 4);
		    format->opt_info[info_count].info_block = 
			(char*)rep + tmp2;
		    info_count++;
		    offset += sizeof(tmp_info);
		}
	    } while (tmp_info.info_type != 0);
	    format->opt_info[info_count].info_type = 0;
	    format->opt_info[info_count].info_len = 0;
	    format->opt_info[info_count].info_block = 0;
	}
    }
    return format;
}


extern FMFormat
expand_format_from_rep(rep)
format_rep rep;
{
    int format_count;
    FMFormat top_format;
    FMFormat *subformats;
    FMStructDescList master_struct_list;
    int i;

    struct _subformat_wire_format *subrep = (struct _subformat_wire_format*)
	(((char*)rep ) + sizeof(struct _format_wire_format_0));
    int byte_reversal = ((rep->record_byte_order & 0x1) != OUR_BYTE_ORDER);
    format_count = rep->subformat_count;
    top_format = expand_subformat_from_rep(subrep);
    subformats = malloc(sizeof(subformats[0]) * (format_count + 1));
    master_struct_list = malloc(sizeof(master_struct_list[0]) * (format_count+2));
    for (i = 0; i < format_count; i++) {
	UINT2 last_subrep_size = ntohs(subrep->f.f0.subformat_rep_length);
	subrep = (struct _subformat_wire_format*)(((char*)subrep) + last_subrep_size);
	subformats[i] = expand_subformat_from_rep(subrep);
	master_struct_list[i+1].format_name = subformats[i]->format_name;
	master_struct_list[i+1].field_list = subformats[i]->field_list;
	master_struct_list[i+1].struct_size = subformats[i]->record_length;
	master_struct_list[i+1].opt_info = NULL;
    }
    subformats[format_count] = NULL;
    master_struct_list[format_count+1].format_name = NULL;
    master_struct_list[format_count+1].field_list = NULL;
    master_struct_list[format_count+1].struct_size = 0;
    master_struct_list[format_count+1].opt_info = NULL;
    master_struct_list[0].format_name = top_format->format_name;
    master_struct_list[0].field_list = top_format->field_list;
    master_struct_list[0].struct_size = top_format->record_length;
    master_struct_list[0].opt_info = NULL;
    top_format->subformats = subformats;
    top_format->server_format_rep = rep;
    top_format->master_struct_list = master_struct_list;
    return top_format;
}
    
    
static void
fill_derived_format_values(FMContext fmc, FMFormat format)
{
    FMFieldList field_list;
    int field;
    format->context = fmc;
    format->variant = 0;
    format->recursive = 0;
    field_list = format->field_list;
    for (field = 0; field < format->field_count; field++) {
	int field_size = 0;
	if (is_var_array_field(field_list, field) == 1) {
	    /* variant array, real_field_size is format->pointer_size */
	    field_size = format->pointer_size;
	} else {
	    long elements = 1;
	    FMdata_type base_type;
	    base_type = array_str_to_data_type(field_list[field].field_type,
					       &elements);
	    if ((base_type != unknown_type) &&
		(field_list[field].field_size > 16)) {
		fprintf(stderr, "Field size for field %s in format %s is large, check to see if it is valid.\n",
			field_list[field].field_name, format->format_name);
	    }
	    field_size = field_list[field].field_size * elements;
	}
    }
    generate_var_list(format, format->subformats);
    for (field = 0; field < format->field_count; field++) {
	if (format->var_list[field].string == 1) {
	    format->variant = 1;
	} else {
	    char *base_type =
	    base_data_type(field_list[field].field_type);
	    FMFormat subformat = NULL;

	    /* if field is of another record format, fill that in */
	    if (str_to_data_type(base_type) == unknown_type) {
	        FMFormat *subformats = format->subformats;
		while (subformats && subformats[0]) {
		    if (strcmp(base_type, subformats[0]->format_name) == 0) {
		    
		        format->field_subformats[field] = subformats[0];
		    }
		    subformats++;
		}
		subformat = format->field_subformats[field];
	    }
	    if (format->var_list[field].var_array == 1) {
		/* got a variant array */
		format->variant = 1;
	    } else {
		/* if field is variant by its subformat being variant */
		if (subformat != NULL) {
		    format->variant |= subformat->variant;
		}
	    }
	    free(base_type);
	}
    }
}

extern void
add_format_to_iofile(fmc, format, id_size, id_buffer, index)
FMContext fmc;
FMFormat format;
int id_size;
void *id_buffer;
int index;
{
    int subformat_count = 0;
    int i, field;

    if (format_server_verbose == 1) {
	printf("Entering format %s (%lx) into context %lx ", 
	       format->format_name, (long)format,
	       (long)fmc);
	print_server_ID(id_buffer);
    }
    while (format->subformats && format->subformats[subformat_count]) {
	format->subformats[subformat_count]->subformats = format->subformats;
	subformat_count++;
    }
    if (id_size) {
	format->server_ID.length = id_size;
	format->server_ID.value = malloc(id_size);
	memcpy(format->server_ID.value, id_buffer, id_size);
    }

    fill_derived_format_values(fmc, format);
    for (i=0; i < subformat_count; i++) {
	fill_derived_format_values(fmc, format->subformats[i]);
    }	
    if (fmc->reg_format_count == fmc->format_list_size) {
	expand_FMContext(fmc);
    }
    if (index == -1) {
	index = fmc->reg_format_count++;
    } else {
 	if (fmc->format_list[index] != NULL) {
  	    free_FMformat(fmc->format_list[index]);
  	}
  	if (index > fmc->reg_format_count) {
  	    printf("Internal error. skipped format ids format %s.\n", 
		   format->format_name);
	    return;
  	}
  	if (index == fmc->reg_format_count) {
 	    /* new format came in */
  	    fmc->reg_format_count++;
  	}
    }
    fmc->format_list[index] = format;
    format->format_index = index;
    topo_order_subformats(format, subformat_count);
    /* bubble up the variant flags */
    for (i= 0; i < subformat_count; i++) {
	int j;
	for (j= 0; j < subformat_count; j++) {
	    FMFormat iof = format->subformats[j];
	    for (field = 0; field < iof->field_count; field++) {
		FMFormat subformat = iof->field_subformats[field];
		if (subformat != NULL) {
		    iof->variant |= subformat->variant;
		}
	    }
	}
    }
    for (field = 0; field < format->field_count; field++) {
	FMFormat subformat = format->field_subformats[field];
	if (subformat != NULL) {
	    format->variant |= subformat->variant;
	}
    }
}

static FMFormat
server_get_format(iocontext, buffer)
FMContext iocontext;
void *buffer;
{
    FMContext fmc = (FMContext) iocontext;
    FMFormat format = NULL;
    int id_size = 8;
    int retry_count = 0;

  retry:
    if (retry_count > 3)
	return NULL;
    if (establish_server_connection_ptr == NULL) {
	assert(0);
    }
    if (establish_server_connection_ptr(fmc, 1) == 0) {
	if (establish_server_connection_ptr(fmc, 1) == 0) {
	    printf("Failed to contact format server\n");
	    exit(1);
	}
    } else {
	char get[2] =
	{'g', 8};		/* format get, size */
	char block_version;
	UINT2 length;
	char return_char = 0;
	format_rep rep;

	if (version_of_format_ID(buffer) < sizeof(ID_length)) {
	    id_size = get[1] = ID_length[version_of_format_ID(buffer)];
	}
	if (serverAtomicWrite(fmc->server_fd, &get[0], 2) != 2) {
	    perror("write to Format server failed");
	    return NULL;
	}
	if (serverAtomicWrite(fmc->server_fd, buffer, id_size) != id_size) {
	    perror("write to Format server failed");
	    return NULL;
	}
	if (serverAtomicRead(fmc->server_fd, &return_char, 1) != 1) {
	    if (format_server_verbose == 1) {
		printf("Retrying because of failed read\n");
	    }
	    retry_count++;
	    goto retry;
	}
	if (return_char == 'P') {
	    provisional_use_warning((int) (long) fmc->server_fd);
	    if (serverAtomicRead(fmc->server_fd, &return_char, 1) != 1) {
		if (format_server_verbose == 1) {
		    printf("Retrying because of failed read\n");
		}
		retry_count++;
		goto retry;
	    }
	}
	if (return_char != 'f') {
	    if (format_server_verbose == 1) {
		printf("Retrying because of failed read\n");
	    }
	    retry_count++;
	    goto retry;
	}
	if (serverAtomicRead(fmc->server_fd, &block_version, 1) != 1) {
	    if (format_server_verbose == 1) {
		printf("Retrying because of failed read\n");
	    }
	    retry_count++;
	    goto retry;
	}
	if (block_version != 1) {
	    if (format_server_verbose == 1) {
		fprintf(stderr, "Unknown version \"%d\"in block registration\n", block_version);
	    }
	    return NULL;
	}
	if (serverAtomicRead(fmc->server_fd, &length, sizeof(length)) !=
	    sizeof(length)) {
	    if (format_server_verbose == 1) {
		printf("Retrying because of failed read\n");
	    }
	    retry_count++;
	    goto retry;
	}
	length = ntohs(length);
	if (length == 0) {
	    format = NULL;
	} else {
	    rep = malloc(length);
	    rep->format_rep_length = htons((short)length);
	    if (serverAtomicRead(fmc->server_fd, ((char *) rep) + sizeof(length),
		 length - sizeof(length)) != (length - sizeof(length))) {
		if (format_server_verbose == 1) {
		    printf("Retrying because of failed read\n");
		}
		retry_count++;
		goto retry;
	    }
	    format = expand_format_from_rep(rep);

	}
    }
    if (format == NULL)
	return NULL;
    add_format_to_iofile(fmc, format, id_size, buffer, -1);
    return format;
}

extern void
server_get_server_ID(fd, server_ID)
void *fd;
void *server_ID;
{
    int id_size = 8;

    serverAtomicRead(fd, server_ID, id_size);
}

extern void
stringify_server_ID(unsigned char *ID, char *buffer, int len)
{
    int id_size = 8;
    int point = 0;
    int i;
    switch (version_of_format_ID(ID)) {
    case 0:
	if (len < id_size * 2) break;
	for (i = 0; i < id_size; i++) {
	    point += sprintf(&buffer[point], "%2x", ID[i]);
	}
	break;
    case 1:{
	version_1_format_ID id1;
	memcpy(&id1, ID, 10);
	if (len < 3+3+6+10+6+50) /* approx size */ return;
	sprintf(buffer, "<ID ver=%d, salt %d, port %d, IP_addr %x, formatID %d>\n",
	       id1.version, id1.salt, ntohs(id1.port),
	       ntohl(id1.IP_addr), ntohs(id1.format_identifier));
	    break;
	}
    case 2:{
	version_2_format_ID *id2 = (version_2_format_ID*)ID;
	if (len < 3+3+6+10+6+50) /* approx size */ return;
	sprintf(buffer, "<ID ver=%d, unused %d, rep_len %d, hash1 %x, hash2 %x>\n",
	       id2->version, id2->unused, ntohs(id2->rep_len) << 2,
	       ntohl(id2->hash1), ntohl(id2->hash2));
	    break;
	}
    default:
	if (len < 30) return;
	sprintf(buffer, "<Unknown format version %d\n",
	       *((unsigned char *) ID));
	break;
    }
}

extern void
print_server_ID(ID)
unsigned char *ID;
{
    char buffer[256];
    stringify_server_ID(ID, buffer, sizeof(buffer));
    printf(buffer);
}

extern void
print_format_ID(format)
FMFormat format;
{
    print_server_ID( (unsigned char *) format->server_ID.value);
}

static int
get_host_IP_format_ID(format_ID)
void *format_ID;
{
    switch (version_of_format_ID(format_ID)) {
    case 0:
    case 2:
	return 0;
	break;
    case 1:{
	    version_1_format_ID *id1 = (version_1_format_ID *) format_ID;
	    int tmp;
	    memcpy(&tmp, &id1->IP_addr, 4);
	    return ntohl(tmp);
	    break;
	}
    default:
	printf("<Unknown format version %d\n",
	       *((unsigned char *) format_ID));
	break;
    }
    return 0;
}

extern int
get_rep_len_format_ID(format_ID)
void *format_ID;
{
    switch (version_of_format_ID(format_ID)) {
    case 2:{
	    version_2_format_ID *id2 = (version_2_format_ID *) format_ID;
	    short tmp;
	    memcpy(&tmp, &id2->rep_len, 2);
		tmp = ntohs(tmp);
	    return tmp << 2;
	    break;
	}
    case 0:
    case 1:
	printf("Format version %d has no size information \n",
	       *((unsigned char *) format_ID));
	break;
    default:
	printf("Unknown format version %d\n",
	       *((unsigned char *) format_ID));
	break;
    }
    return 0;
}

static int
get_host_port_format_ID(format_ID)
void *format_ID;
{
    switch (version_of_format_ID(format_ID)) {
    case 0:
    case 2:
	return 0;
	break;
    case 1:{
	    version_1_format_ID *id1 = (version_1_format_ID *) format_ID;
	    short port;
	    memcpy(&port, &id1->port, 2);
	    return ntohs(port);
	    break;
	}
    default:
	printf("<Unknown format version %d\n",
	       *((unsigned char *) format_ID));
	break;
    }
    /* not reached */
    return 0;
}

/* write header information to the format server */
extern int
server_write_header(fmc, enc_len, enc_buffer)
FMContext fmc;
int enc_len;
unsigned char *enc_buffer;
{
    FILE_INT magic = MAGIC_NUMBER + 2;
    FILE_INT float_format = fmc->native_float_format;
    FILE_INT server_pid;
    if (enc_len == 0) {
	put_serverAtomicInt(fmc->server_fd, &magic, fmc);
	put_serverAtomicInt(fmc->server_fd, &float_format, fmc);
    } else {
	FILE_INT key_len = enc_len;
	magic = MAGIC_NUMBER + 3;
	put_serverAtomicInt(fmc->server_fd, &magic, fmc);
	put_serverAtomicInt(fmc->server_fd, &key_len, fmc);
	serverAtomicWrite(fmc->server_fd, enc_buffer, key_len);
    }	
    get_serverAtomicInt(fmc->server_fd, &magic, 0);
    get_serverAtomicInt(fmc->server_fd, &server_pid, 0);
    if ((fmc->server_pid != 0) && (fmc->server_pid != server_pid)) {
	return 0;
    } else {
	fmc->server_pid = server_pid;
    }
    if (magic != MAGIC_NUMBER) {
	if (magic == REVERSE_MAGIC_NUMBER) {
	    fmc->server_byte_reversal = 1;
	} else {
	    /* close client */
	    return -1;
	}
    }
    return 1;
}

extern void
server_write_char(fsc, chr)
FSClient fsc;
char *chr;
{
    serverAtomicWrite(fsc->fd, chr, 1);
}

extern void
server_read_header(fsc)
FSClient fsc;
{
    FILE_INT magic;
    FILE_INT float_format;
    FILE_INT pid = getpid();

    fsc->byte_reversal = 0;
    get_serverTimeoutInt(fsc->fd, &magic, fsc->byte_reversal);

    switch (magic) {
    case MAGIC_NUMBER:
	break;
    case REVERSE_MAGIC_NUMBER:
	fsc->byte_reversal = 1;
	break;
    case MAGIC_NUMBER + 1:
	fsc->version = 1;
	break;
    case REVERSE_MAGIC_NUMBER + 0x1000000:
	fsc->version = 1;
	fsc->byte_reversal = 1;
	break;
    case MAGIC_NUMBER + 2:
	fsc->version = 2;
	break;
    case REVERSE_MAGIC_NUMBER + 0x2000000:
	fsc->version = 2;
	fsc->byte_reversal = 1;
	break;
    case MAGIC_NUMBER + 3:
	fsc->version = 3;
	break;
    case REVERSE_MAGIC_NUMBER + 0x3000000:
	fsc->version = 3;
	fsc->byte_reversal = 1;
	break;
    default:
	close((int)(long)fsc->fd);
	return;
    }
    if (fsc->version <= 2) {
	get_serverAtomicInt(fsc->fd, &float_format, fsc->byte_reversal);
    } else {
	FILE_INT key_len;
	unsigned char *key;
	get_serverAtomicInt(fsc->fd, &key_len, fsc->byte_reversal);
	if (key_len > 1024) {
	    close((int)(long)fsc->fd);
	    return;
	}
	key = malloc(key_len);
	serverAtomicRead(fsc->fd, key, key_len);
	fsc->key = key;
	fsc->key_len = key_len;
    }
    magic = MAGIC_NUMBER;
    put_serverAtomicInt(fsc->fd, &magic, NULL);
    if (fsc->version >= 2) {
	put_serverAtomicInt(fsc->fd, &pid, NULL);
    }
}

extern int
version_of_format_ID(void *server_ID)
{
    /* format ID is at least 64 bits */
    char *char_ID = (char *) server_ID;
    if (server_ID == NULL) return -1;
    if ((char_ID[4] == 0) && (char_ID[5] == 0) && 
	(char_ID[6] == 0) && (char_ID[7] == 0)) {
	/* version 0 format_IDs have no version info, but second int is
	 * zero */
	return 0;
    } else {
	/* first byte is version ID */
	return *((char *) server_ID);
    }
}

extern char *
get_server_rep_FMformat(format, rep_length)
FMFormat format;
int *rep_length;
{
    if (format->server_format_rep == NULL) {
	format->server_format_rep = 
	    build_server_format_rep(format);
    }
    *rep_length = ntohs(format->server_format_rep->format_rep_length);
    return (char*)format->server_format_rep;
}

extern char *
get_server_ID_FMformat(format, id_length)
FMFormat format;
int *id_length;
{
    *id_length = format->server_ID.length;
    return format->server_ID.value;
}


extern FMFormat
load_external_format_FMcontext(iocontext, server_id, id_size, server_rep)
FMContext iocontext;
char *server_id;
int id_size;
char *server_rep;
{
    FMFormat format = get_local_format_IOcontext(iocontext, server_id);

    if (format != NULL) {
	if (format_server_verbose == 1) {
	    printf("Load external format already exists  - ");
	    print_server_ID((void*)server_id);
	}
	/* format is already here */
	return format;
    }
    format = expand_format_from_rep((format_rep)server_rep);

    if (format == NULL) {
	if (format_server_verbose == 1) {
	    printf("Couldn't expand external format  - ");
	    print_server_ID((void*)server_id);
	}
	return NULL;
    }
    add_format_to_iofile((FMContext)iocontext, format, id_size, server_id, -1);
    return format;
}

extern int
format_server_restarted(FMContext context)
{
    int ret = establish_server_connection_ptr((FMContext)context, 1);
    return (ret == 0);
}

