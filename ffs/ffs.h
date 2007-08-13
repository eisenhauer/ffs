#ifndef __FFS_H__
#define __FFS_H__

#include "fm.h"
#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if defined(FUNCPROTO) || defined(__STDC__) || defined(__cplusplus) || defined(c_plusplus) || defined(_MSC_VER)
#ifndef	NULL
/* 
 * NULL --
 *      Null pointer.
 */
#define NULL	((void *) 0)
#endif
#else
#ifndef	NULL
/* 
 * NULL --
 *      Null pointer.
 */
#define NULL	0
#endif
#endif

#ifndef IOOffset
#define IOOffset(p_type,field) \
	((int) (((char *) (&(((p_type)NULL)->field))) - ((char *) NULL)))
#if defined(__STDC__) || defined(__ANSI_CPP__) || defined(_MSC_VER)
#define IOstr(s) #s
#else
#define IOstr(s) "s"
#endif
#define IOArrayDecl(type, size) IOstr(type[size])
#define IOArrayDecl2(type, size, size2) IOstr(type[size][size2])

#define IODefaultDecl(name, val) IOstr(name(val))

#endif

#define XML_OPT_INFO 0x584D4C20
#define COMPAT_OPT_INFO 0x45564F4C
#define COMPAT_OPT_INFO_IOFILE 0x45564F4D



typedef struct FFSBuffer *FFSBuffer;

typedef struct _IOContextStruct *FFSContext;

typedef struct _FFSTypeHandle *FFSTypeHandle;

extern FFSContext create_FFSContext();

extern FFSBuffer create_FFSBuffer();
extern void free_FFSBuffer(FFSBuffer buf);

extern char *
FFSencode(FFSBuffer b, FMFormat ioformat, void *data, int *buf_size);

extern int decode_in_place_possible(FFSTypeHandle);

extern FFSTypeHandle FFSTypeHandle_from_encode(FFSContext c, char *b);

extern FFSTypeHandle FFSTypeHandle_by_index(FFSContext c, int index);

extern char * FFSTypeHandle_name(FFSTypeHandle f);

extern void
establish_conversion(FFSContext c, FFSTypeHandle f,
			   FMStructDescList struct_list);

extern int
FFS_est_decode_length(FFSContext context, char *encoded, int record_length);

extern int
FFSdecode_in_place(FFSContext context, char *encode, void **dest_ptr);

extern int
FFSdecode_to_buffer(FFSContext context, char *encode, void *dest);

extern int
FFSdecode(FFSContext context, char *encode, char *dest);

extern void
free_FFSContext(FFSContext c);

#ifdef NOT_DEF
extern char *
encode_IOcontext_bufferB(IOContext iocontext, IOFormat ioformat,
			       IOBuffer iobuffer, void *data, 
			       int *buf_size);

typedef struct _IOConversionStruct *IOConversionPtr;

typedef struct _IOFormatStruct *IOFormat;

typedef struct _IOFormatStruct *IOFormatHandle;

typedef enum {
    OpenNoHeader, OpenHeader, OpenForRead, OpenBeginRead, Closed
} IOStatus;

typedef enum {
    IOerror, IOend, IOdata, IOformat, IOcomment
} IORecordType;

typedef struct _IOFileStruct *IOFile;

typedef struct _IOgetFieldStruct *IOFieldPtr;
typedef struct _ffsvec {
    void *data;
    IOFormat format;
} *ffsvec;

typedef struct _IOContext_stats *IOContext_stats;

/* File operations */
extern IOFile
open_IOfile(const char *path, const char *flag_str);

extern IOFile
open_IOfd(int fd, const char *flags);

extern IOFile
open_version_IOfd(int fd, int version);

extern int
version_of_IOfile(IOFile iofile);

extern void
set_array_order_IOfile(IOFile iofile, int column_major);

extern void
set_fd_IOfile(IOFile iofile, int fd);

extern void
close_IOfile(IOFile iofile);

extern int
poll_IOfile(IOFile iofile);

extern int
file_id_IOfile(IOFile iofile);

extern void
dump_IOFile(IOFile iofile);
extern void
dump_IOFile_as_XML(IOFile iofile);

extern void
free_IOfile(IOFile iofile);

extern IOContext
create_IOcontext();

typedef void *(*FFSGetFormatRepCallback)(void *format_ID, 
						int format_ID_length, 
						int host_IP,
						int host_port,
						void *app_context,
						void *client_data);


typedef int (*FFSGetPortCallback)(void *client_data);


extern IOContext
create_server_IOcontext(FFSGetFormatRepCallback format_rep_callback, 
			      FFSGetPortCallback get_port_callback, 
			      void *client_data);

extern IOContext
create_local_IOcontext();

extern void
free_IOcontext(IOContext context);

extern IOContext
create_IOsubcontext(IOContext master_context);

extern void
free_IOsubcontext(IOContext context);

typedef struct _io_encode_vec {
     void *iov_base;
     long  iov_len;
} *IOEncodeVector;

extern IOEncodeVector
encode_IOcontext_to_vector(IOContext iocontext, IOFormat ioformat,
				 void *data);

extern IOEncodeVector
encode_IOcontext_release_vector(IOContext iocontext, IOFormat ioformat,
				      void *data);

extern IOEncodeVector copy_all_to_FFSBuffer(FFSBuffer buf, IOEncodeVector vec);
extern IOEncodeVector copy_vector_to_IOBuffer(IOBuffer buf, IOEncodeVector vec);

extern IOEncodeVector
encode_IOcontext_vectorB(IOContext iocontext, IOBuffer tmp_buffer,
			       IOFormat ioformat, void *data);

extern int
decode_IOcontext(IOContext iocontext, char *src, void *data);

extern int
decode_to_buffer_IOcontext(IOContext iocontext, char *src, void *data);

extern int
decode_in_place_IOcontext(IOContext iocontext, char *src,
				void **dest_ptr);

/* format operations */
extern
IOFormatHandle
register_data_format(IOContext iofile, FMStructDescList struct_list);

extern
IOFormatHandle
register_simple_data_format(IOContext iofile, char *struct_name,
				  IOFieldList struct_field_list,
				  int struct_size,
				  IOOptInfo *optinfo);

extern void *
get_optinfo_IOFormat(IOFormat ioformat, int info_type, int *len_p);

extern void
dump_IOFormat(IOFormat ioformat);

extern void
dump_IOFormat_as_XML(IOFormat ioformat);

extern IOFormat
read_format_IOfile(IOFile iofile);

IOFormat *
get_subformats_IOformat(IOFormat ioformat);

IOFormat *
get_subformats_IOcontext(IOContext iocontext, void *buffer);

char **
get_subformat_names(IOFieldList field_list);

extern IOFormat
get_format_IOcontext(IOContext iocontext, void *buffer);

extern IOFormat
get_format_app_IOcontext(IOContext iocontext, void *buffer, 
			       void *app_context);

extern char *
get_server_rep_IOformat(IOFormat ioformat, int *rep_length);

extern char *
get_server_ID_IOformat(IOFormat ioformat, int *id_length);

extern IOFormat
load_external_format_IOcontext(IOContext iocontext, char *server_id,
				     int id_size, char *server_rep);

extern void
add_opt_info_IOformat(IOFormat format, int typ, int len, void *block);

extern void *
create_compat_info(IOFormat prior_format, char *xform_code, int *len_p);

typedef struct compat_formats {
    IOFormat prior_format;
    char *xform_code;
} *IOcompat_formats;

extern IOcompat_formats
get_compat_formats(IOFormat format);

extern void *
create_IOFile_compat_info(IOFormat prior_format, char *xform_code, int *len_p);

extern IOcompat_formats
get_IOFile_compat_formats(IOFormat format);

extern int
index_of_IOformat(IOFormat format);

extern IOFormat
next_IOrecord_format(IOFile iofile);

extern IOFormat
get_local_format_IOcontext(IOContext iocontext, void *buffer);

extern char *
name_of_IOformat(IOFormat format);

extern void
get_IOformat_characteristics(IOFormat format, IOfloat_format *ff, 
				   IOinteger_format *intf, int *column_major,
				   int *pointer_size);

extern char *
global_name_of_IOformat(IOFormat format);

extern int
global_name_eq(IOFormat format1, IOFormat format2);

extern int
pointer_size_of_IOformat(IOFormat format);

extern IOFile
iofile_of_IOformat(IOFormat format);

extern IOFormatList
IOlocalize_conv(IOContext context, IOFormat format);

extern IOFormatList
IOlocalize_register_conv(IOContext context, IOFormat format, 
	IOFieldList native_field_list, IOFormatList native_subformat_list,
	IOFormat *local_prior_format, int *local_struct_size_out);

extern
void
set_IOconversion(IOFile iofile, const char *formatname, IOFieldList field_list,
		       int struct_size);

extern
void
set_conversion_IOcontext(IOContext iocontext, IOFormat ioformat, 
			       IOFieldList field_list, int struct_size);

extern
int
has_conversion_IOformat(IOFormat ioformat);

extern
void
set_general_IOconversion(IOFile iofile, const char *formatname, 
			       IOFieldList field_list, int struct_size,
			       int pointer_size);

extern
void
set_local_IOconversion(IOFile iofile, const char *formatname);

extern
IOConversionPtr
IOcreate_mem_conv(IOFieldList old_field_list,
			int old_struct_size, int old_pointer_size,
			IOFieldList new_field_list,
			int new_struct_size, int new_pointer_size);
extern
void
set_notify_of_format_change(IOFile iofile, const char *formatname, int value);

/* Field List operations */
extern
IOFieldList
field_list_of_IOformat(IOFormat format);

extern
int
compare_field_lists(IOFieldList list1, IOFieldList list2);

typedef enum {Format_Less, Format_Greater, Format_Equal, 
	      Format_Incompatible} IOformat_order;

IOformat_order IOformat_cmp(IOFormat format1, IOFormat format2);

extern int
IOformat_compat_cmp(IOFormat format, IOFormat *formatList, 
		int listSize, IOcompat_formats *older_format);

extern int
IOformat_compat_cmp2(IOFormat format, IOFormat *formatList, 
			   int listSize, IOcompat_formats *older_format);

extern
IOFieldList
copy_field_list(IOFieldList list);

extern
IOFieldList
get_local_field_list(IOFormat ioformat);

extern
IOFieldList
localize_field_list(IOFieldList list, IOContext c);

extern
void
free_field_list(IOFieldList list);

extern void
free_IOFormatList(IOFormatList formatList);

extern
IOFieldList
max_field_lists(IOFieldList list1, IOFieldList list2);

extern
void
force_align_field_list(IOFieldList field_list, int pointer_size);

extern
int
struct_size_IOfield(IOFile iofile, IOFieldList list);

extern
int
struct_size_field_list(IOFieldList list, int pointer_size);


extern int
get_rep_len_format_ID(void *server_format_ID);

extern void
print_server_ID (unsigned char *ID);

/* Normal input and output operations */
extern
IORecordType
next_IOrecord_type(IOFile iofile);

extern int
write_IOfile(IOFile iofile, IOFormat ioformat, void *data);

extern int
writev_IOfile(IOFile iofile, ffsvec vec, int count);

extern int
read_IOfile(IOFile iofile, void *data);

extern int
read_to_buffer_IOfile(IOFile iofile, void *data, int buffer_len);

extern int
write_comment_IOfile(IOFile iofile, const char *comment);

extern
char *
read_comment_IOfile(IOFile iofile);

extern int
next_IOrecord_length(IOFile iofile);

extern int
this_IOrecord_length(IOContext context, char *src, int record_length);

/* array of record operations */
extern int
next_IOrecord_count(IOFile iofile);

extern int
read_array_IOfile(IOFile iofile, void *data, int count,
			int struct_size);

extern int
write_array_IOfile(IOFile iofile, IOFormat ioformat, void *data,
			 int count, int struct_size);

extern void
dump_unencoded_IOrecord(IOFile iofile, IOFormat ioformat, void *data);
extern int
dump_limited_unencoded_IOrecord(IOFile iofile, IOFormat ioformat,
				      void *data, int character_limit);
extern int
dump_limited_encoded_IOrecord(IOFile iofile, IOFormat ioformat,
				    void *data, int character_limit);
extern void
dump_unencoded_IOrecord_as_XML(IOFile iofile, IOFormat ioformat, void *data);

extern void
IOfree_var_rec_elements(IOFile iofile, IOFormat ioformat, void *data);

/* raw data handling operations */
extern void
dump_raw_IOrecord(IOFile iofile, IOFormat ioformat, void *data);
extern void
dump_raw_IOrecord_as_XML(IOFile iofile, IOFormat ioformat, void *data);

extern void
dump_encoded_as_XML(IOContext iocontext, void *data);

extern char*
IOencoded_to_XML_string(IOContext iocontext, void *data);

extern char*
IOunencoded_to_XML_string(IOContext iocontext, IOFormat ioformat, 
				void *data);

extern int
next_raw_IOrecord_length(IOFile iofile);

extern int
read_raw_IOfile(IOFile iofile, void *data, int buffer_max, IOFormat *ioformatptr);

extern int
read_raw_array_IOfile(IOFile iofile, IOFormat ioformat, void *data,
			    int count, int struct_size);

extern
IOFieldPtr
get_IOfieldPtrFromList(IOFieldList field_list, const char *field_name);

extern
IOFieldPtr
get_IOfieldPtr(IOFile iofile, const char *formatname, const char *fieldname);

extern
IOFieldPtr
get_local_IOfieldPtr(IOFile iofile, const char *formatname, const char *fieldname);

typedef struct _IOSaxParseInfo *IOSaxParseInfo;

typedef void (*IOSaxEndItemCallback(const char *element_name, void *data);

extern IOSaxParseInfo
IOcreate_SaxParseInfo(IOContext c, void *init_base);

extern void
IOset_parse_callback(IOSaxParseInfo info, IOSaxEndItemCallback callback);

extern void 
IOSaxStartElement(void *userData, const char *name, const char **atts);

extern void 
IOSaxEndElement(void *userData, const char *name);

extern void
IOSaxDataHandler(void *userData, const char *s, int len);

extern IOContext_stats 
get_IOstats(IOContext iocontext);

extern void
print_IOstats(IOContext_stats);

extern int
format_server_restarted(IOContext context);

extern void
general_format_server(int port, int do_restart, int verbose);

extern void
run_format_server(int port);

extern void
set_format_server(char *hostname, int port);

struct _IOContext_stats {
    int decode_bytes;
    int decode_msg_count;
    int encode_bytes;
    int encode_msg_count;
    int formats_present_count;
    int format_registration_count;
    int format_fetch_count;
    int to_format_server_bytes;
    int from_format_server_bytes;
    long last_server_time;
};

extern float get_IOfloat(IOFieldPtr iofield, void *data);
extern double get_IOdouble(IOFieldPtr iofield, void *data);
extern short get_IOshort(IOFieldPtr iofield, void *data);
extern int get_IOint(IOFieldPtr iofield, void *data);
extern long get_IOlong(IOFieldPtr iofield, void *data);
extern void get_IOlong8(IOFieldPtr iofield, void *data, unsigned long *low_long, long *high_long);
#if defined(SIZEOF_LONG_LONG)
#if SIZEOF_LONG_LONG != 0
extern long long get_IOlong_long(IOFieldPtr iofield, void *data);
extern unsigned long long get_IOulong_long(IOFieldPtr iofield, void *data);
#endif
#endif
#if defined(SIZEOF_LONG_DOUBLE)
#if SIZEOF_LONG_DOUBLE != 0
extern long double get_IOlong_double(IOFieldPtr iofield, void *data);
#endif
#endif
extern unsigned short get_IOushort(IOFieldPtr iofield, void *data);
extern unsigned int get_IOuint(IOFieldPtr iofield, void *data);
extern unsigned long get_IOulong(IOFieldPtr iofield, void *data);
extern int get_IOulong8(IOFieldPtr iofield, void *data, unsigned long *low_long, unsigned long *high_long);
extern char *get_IOstring(IOFieldPtr iofield, void *data);
extern char get_IOchar(IOFieldPtr iofield, void *data);
extern int get_IOenum(IOFieldPtr iofield, void *data);

extern void put_IOdouble(IOFieldPtr iofield, double, void *data);
extern void put_IOshort(IOFieldPtr iofield, int, void *data);
extern void put_IOint(IOFieldPtr iofield, int, void *data);
extern void put_IOlong(IOFieldPtr iofield, long, void *data);
extern void put_IOlong8(IOFieldPtr iofield, void *data, unsigned long low_long, long high_long);
extern void put_IOushort(IOFieldPtr iofield, unsigned int, void *data);
extern void put_IOuint(IOFieldPtr iofield, unsigned int, void *data);
extern void put_IOulong(IOFieldPtr iofield, unsigned long, void *data);
extern void put_IOstring(IOFieldPtr iofield, const char *, void *data);
extern void put_IOchar(IOFieldPtr iofield, int, void *data);
extern void put_IOenum(IOFieldPtr iofield, int, void *data);
extern int IO_field_type_eq(const char *str1, const char *str2);
extern int IOget_array_size_dimen(const char *str, IOFieldList fields, 
				  int dimen, int *control_field);
#endif
#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif
