
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
    OpenNoHeader, OpenHeader, OpenForRead, OpenBeginRead, Closed
} Status;

typedef struct format_info {
    int written_to_file;
} format_info;

struct FFSFile {
    FFSContext c;

    void *file_id;
    format_info *info;
    int info_size;
    FFSBuffer buf;

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
open_FFSfile(const char *path, const char *flags)
{
    void *file;
    FFSFile f;
    int allow_input = 0, allow_output = 0;

    file = os_file_open_func(path, flags, &allow_input, &allow_output);

    if (file == NULL) {
	char msg[128];
	(void) sprintf(msg, "open_FFSfile failed for %s :", path);
	perror(msg);
	return NULL;
    }
    f = malloc(sizeof(struct FFSFile));
    memset(f, 0, sizeof(*f));
    f->file_id = file;
    f->buf = create_FFSBuffer();
    set_interface_FFSFile(f, os_file_write_func, os_file_read_func,
			 os_file_writev_func, os_file_readv_func, os_max_iov,
			 os_close_func);

    f->status = OpenNoHeader;
    if (allow_input) {
	int magic_number;
	if ((f->read_func(file, &magic_number, 4, NULL, NULL) != 4) ||
	    (magic_number != MAGIC_NUMBER)) {
	    printf("read headers failed\n");
	    return NULL;
	}
    }
    if (allow_output) {
	int magic_number = htonl(MAGIC_NUMBER);
	assert(sizeof(int) == 4);	/* otherwise must do other stuff */

	if (f->write_func(file, &magic_number, 4, NULL, NULL) != 4) {
	    printf("write headers failed\n");
	    return NULL;
	}
    }
    return f;
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
	printf("Write failed\n");
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
	printf("Write failed\n");
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
    struct iovec *vec;
    int indicator;

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
     * next_data indicator is a 4-byte chunk in network byte order.
     * The top byte is 0x3.  The bottom 3 bytes are the size of the data.
     */
    indicator = htonl((byte_size & 0xffffff) | 0x3 << 24);

    /* 
     *  utilize the fact that we have some blank vec entries *before the 
     *  beginning* of the vec we are given.  see ffs.c 
     */
    vec--;
    vec_count++;
    vec[0].iov_len = 4;
    vec[0].iov_base = &indicator;
    if (f->writev_func(f->file_id, vec, vec_count, NULL, NULL) != vec_count) {
	printf("Write failed, errno %d\n", errno);
	return 0;
    }
    return 1;
}
