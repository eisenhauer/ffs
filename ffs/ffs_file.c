
#include "config.h"

#include "assert.h"
#include "ffs.h"
#include "ffs_internal.h"
#include "cercs_env.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "errno.h"

#include "io_interface.h"

typedef enum {
    OpenForRead, Closed
} Status;

typedef struct format_info {
    int written_to_file;
} format_info;

struct _FFSFile {
    FFSContext c;
    FMContext fmc;

    FFSBuffer tmp_buffer;
    void *file_id;
    format_info *info;
    int info_size;
    int next_fid_len;
    int next_data_len;
    FFSBuffer buf;
    int read_ahead;
    int errno_val;
    FFSRecordType next_record_type;
    FFSTypeHandle next_data_handle;

    Status status;
    IOinterface_func write_func;
    IOinterface_func read_func;
    int max_iov;
    IOinterface_funcv writev_func;
    IOinterface_funcv readv_func;
    IOinterface_close close_func;

};

extern
void
free_FFSfile(FFSFile f)
{
    free(f->info);
    f->info = NULL;
    f->info_size = 0;
    free_FFSBuffer(f->buf);
    f->buf = NULL;
}




extern FFSFile
open_FFSfd(void *fd, const char *flags)
{
    void *file = fd;
    FFSFile f;
    int allow_input = 0, allow_output = 0;

    f = malloc(sizeof(struct _FFSFile));
    memset(f, 0, sizeof(*f));
    f->file_id = file;
    set_interface_FFSFile(f, ffs_file_write_func, ffs_file_read_func,
			 ffs_file_writev_func, ffs_file_readv_func, ffs_max_iov,
			 ffs_close_func);

    f->buf = create_FFSBuffer();
    f->status = OpenForRead;
    if (allow_input) {
	int magic_number;
	if ((f->read_func(file, &magic_number, 4, NULL, NULL) != 4) ||
	    (magic_number != htonl(MAGIC_NUMBER))) {
	    printf("read headers failed\n");
	    return NULL;
	}
	f->status = OpenForRead;
    }
    if (allow_output) {
	int magic_number = htonl(MAGIC_NUMBER);
	assert(sizeof(int) == 4);	/* otherwise must do other stuff */

	if (f->write_func(file, &magic_number, 4, NULL, NULL) != 4) {
	    printf("write headers failed\n");
	    return NULL;
	}
    }
    f->fmc = create_local_FMcontext();
    f->c = create_FFSContext_FM(f->fmc);
    return f;
}

extern FFSFile
open_FFSfile(const char *path, const char *flags)
{
    void *file;
    FFSFile f;
    int allow_input = 0, allow_output = 0;

    file = ffs_file_open_func(path, flags, &allow_input, &allow_output);

    if (file == NULL) {
	char msg[128];
	(void) sprintf(msg, "open_FFSfile failed for %s :", path);
	perror(msg);
	return NULL;
    }
    f = open_FFSfd(file, flags);
    return f;
}

FFSContext
FFSContext_of_file(FFSFile f)
{
    return f->c;
}

FMContext
FMContext_of_file(FFSFile f)
{
    return f->fmc;
}

extern void
set_interface_FFSFile(f, write_func, read_func, writev_func, readv_func,
		     max_iov, close_func)
FFSFile f;
IOinterface_func write_func;
IOinterface_func read_func;
IOinterface_funcv writev_func;
IOinterface_funcv readv_func;
int max_iov;
IOinterface_close close_func;
{
    f->write_func = write_func;
    f->read_func = read_func;
    f->max_iov = max_iov;
    f->writev_func = writev_func;
    f->readv_func = readv_func;
    f->close_func = close_func;
}

extern void
close_FFSfile(FFSFile file)
{
    file->close_func(file->file_id);
}

extern FFSFile
create_FFSfile();

void
init_format_info(FFSFile f, int index)
{
    if (f->info == NULL) {
	f->info = malloc(sizeof(f->info[0]) * (index+1));
	memset(f->info, 0, sizeof(f->info[0]) * (index+1));
	f->info_size = index + 1;
    } else if (f->info_size <= index) {
	f->info = realloc(f->info,
				 (index+1)*sizeof(f->info[0]));
	memset(&f->info[f->info_size], 0, 
	       sizeof(f->info[0]) * ((index+1) - f->info_size));
	f->info_size = index + 1;
    }
}

static
int
write_format_to_file(FFSFile f, FMFormat format)
{
    struct {
	int indicator;
	int format_len;
    } format_header;
    struct iovec vec[4];
    char *server_id;
    int id_len;
    char *server_rep;
    int rep_len;

    server_id = get_server_ID_FMformat(format, &id_len);
    server_rep = get_server_rep_FMformat(format, &rep_len);

    /*
     * next_data indicator is a 2 4-byte chunks in network byte order.
     * In the first chunk, 
     *    the top byte is 0x2, middle 2 are unused and
     *    the bottom byte are the size of the format id;
     * The second chunk holds the length of the format rep;
     */

    format_header.indicator = htonl((id_len & 0xff) | 0x2 << 24);
    format_header.format_len = htonl(rep_len);

    vec[0].iov_len = 8;
    vec[0].iov_base = &format_header;
    vec[1].iov_len = id_len;
    vec[1].iov_base = server_id;
    vec[2].iov_len = rep_len;
    vec[2].iov_base = server_rep;
    vec[3].iov_len = 0;
    vec[3].iov_base = NULL;
    if (f->writev_func(f->file_id, vec, 3, NULL, NULL) != 3) {
	printf("Write failed errno %d\n", errno);
	return 0;
    }
    return 1;
}

extern int
write_comment_FFSfile(FFSFile f, const char *comment)
{
    struct iovec vec[2];
    int byte_size = strlen(comment) + 1;
    int indicator;
    /*
     * next_comment indicator is a 4-byte chunk in network byte order.
     * The top byte is 0x1.  The bottom 3 bytes are the size of the data.
     */
    indicator = htonl((byte_size & 0xffffff) | 0x1 << 24);

    vec[0].iov_len = 4;
    vec[0].iov_base = &indicator;
    vec[1].iov_len = byte_size;
    vec[1].iov_base = comment;
    if (f->writev_func(f->file_id, vec, 2, NULL, NULL) != 2) {
	printf("Write failed errno %d\n", errno);
	return 0;
    }
    return 1;
}

extern int
write_encoded_FFSfile(FFSFile f, void *data, int byte_size, FFSContext c)
{
    FFSTypeHandle h = FFSTypeHandle_from_encode(c, (char*)data);
    FMFormat cf = FMFormat_of_original(h);
    int id_len = 0;
    char *id = get_server_ID_FMformat(cf, &id_len);
    int rep_len = 0;
    char *rep = get_server_rep_FMformat(cf, &rep_len);

    FMFormat f2 = load_external_format_FMcontext(f->fmc, id, id_len, rep);
    int index = f2->format_index;

    struct FFSEncodeVec vec[2];
    int indicator[2];

    init_format_info(f, index);
    if (!f->info[index].written_to_file) {
	if (write_format_to_file(f, f2) != 1) return 0;
    }

    /*
     * next_data indicator is two 4-byte chunks in network byte order.
     * The top byte is 0x3.  The next 3 bytes are reserved for future use.
     * The following 4-bytes are the size of the data -- assume size fits
     * in a signed int.
     */
    indicator[0] = htonl(0x3 << 24);
    indicator[1] = htonl(byte_size ); 


    vec[0].iov_len = 8;
    vec[0].iov_base = indicator;
    vec[1].iov_len = byte_size;
    vec[1].iov_base = data;
    if (f->writev_func(f->file_id, (struct iovec *)vec, 2, 
		       NULL, NULL) != 2) {
	printf("Write failed, errno %d\n", errno);
	return 0;
    }
    return 1;
}

				   
extern int
write_FFSfile(FFSFile f, FMFormat format, void *data)
{
    int byte_size;
    int index = format->format_index;
    int vec_count;
    FFSEncodeVector vec;
    int indicator[2];

    init_format_info(f, index);
    if (!f->info[index].written_to_file) {
	if (write_format_to_file(f, format) != 1) return 0;
    }

    vec = FFSencode_vector(f->buf, format, data);

    vec_count = 0;
    byte_size = 0;
    while (vec[vec_count].iov_base != NULL) {
	byte_size += vec[vec_count].iov_len;
	vec_count++;
    }

    /*
     * next_data indicator is two 4-byte chunks in network byte order.
     * The top byte is 0x3.  The next 3 bytes are reserved for future use.
     * The following 4-bytes are the size of the data -- assume size fits
     * in a signed int.
     */
    indicator[0] = htonl(0x3 << 24);
    indicator[1] = htonl(byte_size ); 

    /* 
     *  utilize the fact that we have some blank vec entries *before the 
     *  beginning* of the vec we are given.  see ffs.c 
     */
    vec--;
    vec_count++;
    vec[0].iov_len = 8;
    vec[0].iov_base = indicator;
    if (f->writev_func(f->file_id, (struct iovec *)vec, vec_count, 
		       NULL, NULL) != vec_count) {
	printf("Write failed, errno %d\n", errno);
	return 0;
    }
    return 1;
}

extern FFSTypeHandle
FFSread_format(FFSFile ffsfile)
{
    char *id;
    char *rep;
    FMFormat format;
    if (ffsfile->read_ahead == FALSE) {
	(void) FFSnext_record_type(ffsfile);
    }
    while (ffsfile->next_record_type != FFSformat) {
	switch (ffsfile->next_record_type) {
	case FFScomment:
	    if (ffsfile->tmp_buffer == NULL) {
		ffsfile->tmp_buffer = create_FFSBuffer();
	    }
	    (void) FFSread_comment(ffsfile);
	    (void) FFSnext_record_type(ffsfile);
	    break;
	case FFSdata:
	    if (ffsfile->tmp_buffer == NULL) {
		ffsfile->tmp_buffer = create_FFSBuffer();
	    }
	    (void) FFSread(ffsfile, NULL);
	    (void) FFSnext_record_type(ffsfile);
	    break;
	default:
	    return NULL;
	}
    }
    
    id = malloc(ffsfile->next_fid_len);
    rep = malloc(ffsfile->next_data_len);
    if (ffsfile->read_func(ffsfile->file_id, id, 
			   ffsfile->next_fid_len, NULL, NULL)
	!= ffsfile->next_fid_len) {
	printf("Read failed, errno %d\n", errno);
	return NULL;
    }
    if (ffsfile->read_func(ffsfile->file_id, rep, 
			   ffsfile->next_data_len, NULL, NULL)
	!= ffsfile->next_data_len) {
	printf("Read failed, errno %d\n", errno);
	return NULL;
    }
    ffsfile->read_ahead = FALSE;
    format = load_external_format_FMcontext(ffsfile->c->fmc, id, 
					    ffsfile->next_fid_len, rep);
    return FFSTypeHandle_by_index(ffsfile->c, format->format_index);
}

extern
FFSTypeHandle
FFSnext_type_handle(ffsfile)
FFSFile ffsfile;
{
    if (ffsfile->status != OpenForRead)
	return NULL;

    if (ffsfile->read_ahead == FALSE) {
	(void) FFSnext_record_type(ffsfile);
    }
    while (ffsfile->next_record_type != FFSdata) {
	switch (ffsfile->next_record_type) {
	case FFScomment:
	    (void) FFSread_comment(ffsfile);
	    (void) FFSnext_record_type(ffsfile);
	    break;
	case FFSformat:
	    (void) FFSread_format(ffsfile);
	    (void) FFSnext_record_type(ffsfile);
	    break;
	default:
	    return NULL;
	}
    }
    return ffsfile->next_data_handle;
}

extern int
FFSfile_next_decode_length(FFSFile iofile)
{
    FFSContext context = iofile->c;
    FFSTypeHandle th = FFSnext_type_handle(iofile);
    int len = iofile->next_data_len;
    FFS_decode_length_format(context, th, len);

}

extern
char *
FFSread_comment(ffsfile)
FFSFile ffsfile;
{
    if (ffsfile->status != OpenForRead)
	return NULL;

    if (ffsfile->read_ahead == FALSE) {
	(void) FFSnext_record_type(ffsfile);
    }
    while (ffsfile->next_record_type != FFScomment) {
	switch (ffsfile->next_record_type) {
	case FFSdata:
	    (void) FFSread(ffsfile, NULL);
	    (void) FFSnext_record_type(ffsfile);
	    break;
	case FFSformat:
	    (void) FFSread_format(ffsfile);
	    (void) FFSnext_record_type(ffsfile);
	    break;
	default:
	    return NULL;
	}
    }
    if (ffsfile->tmp_buffer == NULL) ffsfile->tmp_buffer = create_FFSBuffer();
    make_tmp_buffer(ffsfile->tmp_buffer, ffsfile->next_data_len);
    if (ffsfile->read_func(ffsfile->file_id, ffsfile->tmp_buffer->tmp_buffer, 
			   ffsfile->next_data_len, NULL, NULL)
	!= ffsfile->next_data_len) {
	printf("Read failed, errno %d\n", errno);
	return NULL;
    }
    ffsfile->read_ahead = FALSE;
    return ffsfile->tmp_buffer->tmp_buffer;
}

static
int
get_AtomicInt(file, file_int_ptr)
FFSFile file;
FILE_INT *file_int_ptr;
{
#if SIZEOF_INT == 4
    int tmp_value;
    if (file->read_func(file->file_id, &tmp_value, 4, NULL, NULL) != 4)
	return 0;

#else
    Baaad shit;
#endif
    ntohl(tmp_value);
    *file_int_ptr = tmp_value;
    return 1;
}


extern
FFSRecordType
FFSnext_record_type(ffsfile)
FFSFile ffsfile;
{
    FILE_INT indicator_chunk;
    FILE_INT count = 1;
    FILE_INT struct_size = 0;
 restart:
    if (ffsfile->status != OpenForRead) {
	return FFSerror;
    }
    if (ffsfile->tmp_buffer == NULL) {
	ffsfile->tmp_buffer = create_FFSBuffer();
    }
    if (ffsfile->read_ahead == FALSE) {
	if (!get_AtomicInt(ffsfile, &indicator_chunk)) {
	    ffsfile->next_record_type = 
		(ffsfile->errno_val) ? FFSerror : FFSend;
	    return ffsfile->next_record_type;
	}
	
	indicator_chunk = ntohl(indicator_chunk);
	switch (indicator_chunk >> 24) {
	case 0x1: /* comment */
		ffsfile->next_record_type = FFScomment;
		ffsfile->next_data_len = indicator_chunk & 0xffffff;
		break;
	case 0x2: /* format */
		ffsfile->next_record_type = FFSformat;
		ffsfile->next_fid_len = indicator_chunk & 0xffffff;
		if (!get_AtomicInt(ffsfile, &indicator_chunk)) {
		    ffsfile->next_record_type = (ffsfile->errno_val) ? FFSerror : FFSend;
		    return ffsfile->next_record_type;
		}
		ffsfile->next_data_len = ntohl(indicator_chunk);
		break;
	case 0x3: /* data */ {
		char *tmp_buf;
		int header_size;
		ffsfile->next_record_type = FFSdata;
		if (!get_AtomicInt(ffsfile, &indicator_chunk)) {
		    ffsfile->next_record_type = (ffsfile->errno_val) ? FFSerror : FFSend;
		    return ffsfile->next_record_type;
		}
		ffsfile->next_data_len = ntohl(indicator_chunk);
		make_tmp_buffer(ffsfile->tmp_buffer, ffsfile->next_data_len);
		tmp_buf = ffsfile->tmp_buffer->tmp_buffer;
		/* first get format ID, at least 8 bytes */
		if (ffsfile->read_func(ffsfile->file_id, tmp_buf, 8, NULL, NULL) != 8) {
		    ffsfile->next_record_type = (ffsfile->errno_val) ? FFSerror : FFSend;
		    return ffsfile->next_record_type;
		}
		ffsfile->next_fid_len = FMformatID_len(tmp_buf);
		if (ffsfile->next_fid_len > 8) {
		    int more = ffsfile->next_fid_len - 8;
		    if (ffsfile->read_func(ffsfile->file_id, tmp_buf + 8, more, NULL, NULL) != more) {
			ffsfile->next_record_type = (ffsfile->errno_val) ? FFSerror : FFSend;
			return ffsfile->next_record_type;
		    }
		}
		ffsfile->next_data_handle = 
		    FFS_target_from_encode(ffsfile->c, tmp_buf);
		if (ffsfile->next_data_handle == NULL) {
		    /* no target for this format, discard */
		    int more = ffsfile->next_data_len - ffsfile->next_fid_len;
		    if (ffsfile->read_func(ffsfile->file_id, tmp_buf + ffsfile->next_fid_len, more, NULL, NULL) != more) {
			ffsfile->next_record_type = (ffsfile->errno_val) ? FFSerror : FFSend;
			return ffsfile->next_record_type;
		    }
		    ffsfile->read_ahead = FALSE;
		    goto restart;
		    
		}
		header_size = FFSheader_size(ffsfile->next_data_handle);
		if (header_size > ffsfile->next_fid_len) {
		    int more = header_size - ffsfile->next_fid_len;
		    if (ffsfile->read_func(ffsfile->file_id, tmp_buf + ffsfile->next_fid_len, more, NULL, NULL) != more) {
			ffsfile->next_record_type = (ffsfile->errno_val) ? FFSerror : FFSend;
			return ffsfile->next_record_type;
		    }
		}
		break;
	default:
	    printf("CORRUPT FFSFILE\n");
	    exit(0);
	}
	}
	ffsfile->read_ahead = TRUE;
    }
    return ffsfile->next_record_type;
}

extern int
FFSnext_data_length(FFSFile file)
{
    FFSTypeHandle f;
    int header_size;
    int read_size;
    char *tmp_buf;

    if (file->status != OpenForRead)
	return 0;

    if (file->read_ahead == FALSE) {
	(void) FFSnext_record_type(file);
    }
    while (file->next_record_type != FFSdata) {
	switch (file->next_record_type) {
	case FFScomment:
	    (void) FFSread_comment(file);
	    (void) FFSnext_record_type(file);
	    break;
	case FFSformat:
	    (void) FFSread_format(file);
	    (void) FFSnext_record_type(file);
	    break;
	default:
	    return 0;
	}
    }
    return file->next_data_len;
}

extern int
FFSread(FFSFile file, void *dest)
{
    FFSTypeHandle f;
    int header_size;
    int read_size;
    char *tmp_buf;

    if (file->status != OpenForRead)
	return 0;

    if (file->read_ahead == FALSE) {
	(void) FFSnext_record_type(file);
    }
    while (file->next_record_type != FFSdata) {
	switch (file->next_record_type) {
	case FFScomment:
	    (void) FFSread_comment(file);
	    (void) FFSnext_record_type(file);
	    break;
	case FFSformat:
	    (void) FFSread_format(file);
	    (void) FFSnext_record_type(file);
	    break;
	default:
	    return 0;
	}
    }

    f = file->next_data_handle;
    header_size = FFSheader_size(f);
    read_size = file->next_data_len - header_size;
    tmp_buf = file->tmp_buffer->tmp_buffer;
    /* should have buffer optimization logic here.  
     * I.E. if decode_in_place_possible() handle differently.  later
     */
    /* read into temporary memory */
    if (file->read_func(file->file_id, tmp_buf + header_size, read_size, NULL, NULL) != read_size) {
	file->next_record_type = (file->errno_val) ? FFSerror : FFSend;
	return 0;
    }
    FFSdecode(file->c, file->tmp_buffer->tmp_buffer, dest);
    file->read_ahead = FALSE;
    return 1;
}

extern int
FFSread_to_buffer(FFSFile file, FFSBuffer b,  void **dest)
{
    FFSTypeHandle f;
    int header_size;
    int read_size;
    char *tmp_buf;

    if (file->status != OpenForRead)
	return 0;

    if (file->read_ahead == FALSE) {
	(void) FFSnext_record_type(file);
    }
    while (file->next_record_type != FFSdata) {
	switch (file->next_record_type) {
	case FFScomment:
	    (void) FFSread_comment(file);
	    (void) FFSnext_record_type(file);
	    break;
	case FFSformat:
	    (void) FFSread_format(file);
	    (void) FFSnext_record_type(file);
	    break;
	default:
	    return 0;
	}
    }

    f = file->next_data_handle;
    header_size = FFSheader_size(f);
    read_size = file->next_data_len - header_size;
    tmp_buf = file->tmp_buffer->tmp_buffer;
    /* should have buffer optimization logic here.  
     * I.E. if decode_in_place_possible() handle differently.  later
     */
    /* read into temporary memory */
    if (file->read_func(file->file_id, tmp_buf + header_size, read_size, NULL, NULL) != read_size) {
	file->next_record_type = (file->errno_val) ? FFSerror : FFSend;
	return 0;
    }
    FFSdecode_to_buffer(file->c, file->tmp_buffer->tmp_buffer, b->tmp_buffer);
    file->read_ahead = FALSE;
    return 1;
}

