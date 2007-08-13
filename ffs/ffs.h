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
FFShas_conversion(FFSTypeHandle format);

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

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif
