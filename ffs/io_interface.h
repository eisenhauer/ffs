#if defined(HAVE_WINDOWS_H) && !defined(NEED_IOVEC_DEFINE)
#define NEED_IOVEC_DEFINE
#endif

#ifdef NEED_IOVEC_DEFINE
struct	iovec {
     void *iov_base;
     int   iov_len;
};
#endif

#if defined(FUNCPROTO) || defined(__STDC__) || defined(__cplusplus) || defined(c_plusplus)
#ifndef ARGS
#define ARGS(args) args
#endif
#else
#ifndef ARGS
#define ARGS(args) (/*args*/)
#endif
#endif

typedef int (*IOinterface_func) ARGS((void *conn, void *buffer, int length,
				      int *errno_p, char **result_p));

typedef int (*IOinterface_funcv) ARGS((void *conn, struct iovec *iov, 
				       int icount, int *errno_p, 
				       char **result_p));

typedef int (*IOinterface_close) ARGS((void *conn));

typedef int (*IOinterface_poll) ARGS((void *conn));

typedef void *(*IOinterface_open) ARGS((const char *path,
					const char *flag_str, 
					int *input, int *output));
typedef void (*IOinterface_init) ARGS((void ));

extern IOFile
create_IOfile();

extern void
set_interface_IOfile ARGS((IOFile iofile, IOinterface_func write_func, 
			   IOinterface_func read_func, 
			   IOinterface_funcv writev_func,
			   IOinterface_funcv readv_func, int max_iov,
			   IOinterface_close close_func,
			   IOinterface_poll poll_func));
extern void *
get_conn_IOfile ARGS((IOFile iofile));

extern void
set_conn_IOfile ARGS((IOFile iofile, void *conn));

extern IOFile
open_created_IOfile ARGS((IOFile iofile, char *flags));

extern void
set_socket_interface_IOfile ARGS((IOFile iofile));

extern void
set_file_interface_IOfile ARGS((IOFile iofile));

