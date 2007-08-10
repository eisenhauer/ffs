#ifndef __FM__
#define __FM__

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif
typedef struct _FMContextStruct *FMContext;

extern
FMContext create_FMcontext();
FMContext create_local_FMcontext();
extern void free_FMcontext();

#define FMOffset(p_type,field) \
	((int) (((char *) (&(((p_type)NULL)->field))) - ((char *) NULL)))
#if defined(__STDC__) || defined(__ANSI_CPP__) || defined(_MSC_VER)
#define FMstr(s) #s
#else
#define FMstr(s) "s"
#endif
#define FMArrayDecl(type, size) FMstr(type[size])
#define FMArrayDecl2(type, size, size2) FMstr(type[size][size2])

#define FMDefaultDecl(name, val) FMstr(name(val))

typedef struct _FMField {
    const char *field_name;	/* Field name */
    const char *field_type;	/* Representation type desired */
    int field_size;		/* Size in bytes of representation */
    int field_offset;		/* Offset from base to put field value */
} FMField, *FMFieldList;

typedef struct _FMOptInfo {
    int info_type;
    int info_len;
    char *info_block;
} FMOptInfo;

/*!
 * A structure to hold Format Name / Field List associations.
 *
 *
 *  This is used to associate names with field lists.  Together these define 
 *  a structure that can be composed into larger structures.
 */
typedef struct _FMformat_list {
    /*! the name to be associated with this structure */
    char *format_name;
    /*! the FFS-style list of fields within this structure */
    FMFieldList field_list;
    int struct_size;
    FMOptInfo *opt_info;
}FMFormatRec, *FMFormatList, FMStructDescRec, *FMStructDescList;

typedef enum {
  Format_Unknown= 0, Format_IEEE_754_bigendian = 1, 
  Format_IEEE_754_littleendian = 2, Format_IEEE_754_mixedendian = 3
} FMfloat_format;

typedef enum {
    Format_Integer_Unknown = 0, Format_Integer_bigendian = 1,
    Format_Integer_littleendian = 2
} FMinteger_format;

typedef enum {
    unknown_type, integer_type, unsigned_type, float_type,
    char_type, string_type, enumeration_type, boolean_type
} FMdata_type;

typedef struct _FMFormatBody *FMFormatBodyPtr;
typedef FMFormatBodyPtr FMFormat;

typedef enum {
    FMType_pointer, FMType_array, FMType_string, FMType_subformat, FMType_simple
}FMTypeEnum;
	
typedef struct FMTypeDesc {
    struct FMTypeDesc *next;
    FMTypeEnum type;
    int field_index;
    int static_size;
    int control_field_index;
} FMTypeDesc;

typedef struct FMDimen {
    int static_size;
    int control_field_index;
} FMDimen;

typedef struct _FMVarInfoStruct {
    int string;
    int var_array;
    int byte_vector;
    FMdata_type data_type;
    int dimen_count;
    FMDimen *dimens;
    FMTypeDesc type_desc;
} FMVarInfoStruct, *FMVarInfoList;

typedef struct _xml_output_info *xml_output_info;

struct _format_wire_format_0;
typedef struct _format_wire_format_0 *format_rep;

typedef struct _server_ID_struct {
    int length;
    char *value;
} server_ID_type;

typedef struct _FMFormatBody {
    int ref_count;
    FMContext context;
    char *format_name;
    int format_index;
    server_ID_type server_ID;
    int record_length;
    int byte_reversal;
    FMfloat_format float_format;
    int pointer_size;
    int IOversion;
    int field_count;
    int variant;
    int recursive;
    int column_major_arrays;
    FMFormat *subformats;
    FMFieldList field_list;
    FMVarInfoList var_list;
    FMFormat *field_subformats;
    FMOptInfo *opt_info;
    xml_output_info xml_out;
    format_rep server_format_rep;    
} FMFormatBody;

extern char *
get_server_rep_FMformat(FMFormat ioformat, int *rep_length);

extern char *
get_server_ID_FMformat(FMFormat ioformat, int *id_length);

FMFormat
FMformat_from_ID(FMContext iocontext, char *buffer);

int
FMformat_index(FMFormat f);

FMFormat
FMformat_by_index(FMContext c, int index);

extern FMFormat
load_external_format_FMcontext(FMContext iocontext, char *server_id,
				     int id_size, char *server_rep);

extern void
add_opt_info_FMformat(FMFormat format, int typ, int len, void *block);

extern FMFormat
register_data_format(FMContext context, FMStructDescList struct_list);

typedef enum {Format_Less, Format_Greater, Format_Equal, 
	      Format_Incompatible} FMformat_order;

FMformat_order FMformat_cmp(FMFormat format1, FMFormat format2);

extern char *
name_of_FMformat(FMFormat format);

extern FMFieldList
copy_field_list(FMFieldList list);

extern int 
count_FMfield(FMFieldList list);

extern void print_server_ID(unsigned char *ID);
extern void print_format_ID(FMFormat ioformat);

#define XML_OPT_INFO 0x584D4C20
#define COMPAT_OPT_INFO 0x45564F4C
#define COMPAT_OPT_INFO_FMFILE 0x45564F4D

typedef struct _IOgetFieldStruct *IOFieldPtr;
#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif
