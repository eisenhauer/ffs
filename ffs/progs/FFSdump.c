#include "config.h"
#include <fcntl.h>
#include <string.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include "ffs.h"
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

extern int FMdumpVerbose;

char *usage = "\
Usage: FFSdump [<option>] <filename>\n\
       FFSdump dumps records from FFS files.  Records may be\n\
       file headers, comments, record formats and data.\n\
       By default, record formats are not printed.  Printing for\n\
       any record type can be turned on or off with an option.\n\
          <option> is of the form {+,-}{<record type>}\n\
       where <record type> is one of : header, comments, formats, data\n";

int
main(argc, argv)
int argc;
char **argv;
{
    FFSFile iofile = NULL;
    int buffer_size = 1024;
    char *buffer = NULL;
    int i;
    char *filename = NULL;
    int dump_formats = 0, dump_header = 1, dump_comments = 1, dump_data = 1;
#ifdef HAVE_WINDOWS_H
    /* set up a scrolling console screen */
    HANDLE hConsole=GetStdHandle(STD_OUTPUT_HANDLE);
    COORD xy;
    SMALL_RECT rect;

    xy.X=100;
    xy.Y=300;
    rect.Left=0;
    rect.Top=0;
    rect.Right=90;
    rect.Bottom=50;

    SetConsoleScreenBufferSize(hConsole,xy);
    SetConsoleWindowInfo(hConsole,TRUE,&rect);
#endif

    printf("Dumping program is ");
    FFSprint_version();
    for (i = 1; i < argc; i++) {
	if ((argv[i][0] == '-') || (argv[i][0] == '+')) {
	    int value = (argv[i][0] == '-') ? 0 : 1;
	    if (strcmp(&argv[i][1], "formats") == 0) {
		dump_formats = value;
	    } else if (strcmp(&argv[i][1], "formats") == 0) {
		dump_formats = value;
	    } else if (strcmp(&argv[i][1], "header") == 0) {
		dump_header = value;
	    } else if (strcmp(&argv[i][1], "comments") == 0) {
		dump_comments = value;
	    } else if (strcmp(&argv[i][1], "data") == 0) {
		dump_data = value;
	    } else {
		fprintf(stderr, "Unknown option \"%s\"\n%s\n", argv[i], usage);
		exit(1);
	    }
	} else {
	    if (filename == NULL) {
		filename = argv[i];
	    } else {
		fprintf(stderr, "Extra argument specified \"%s\"\n%s\n",
			argv[i], usage);
		exit(1);
	    }
	}
    }

    if (filename == NULL) {
	fprintf(stderr, "%s", usage);
	exit(1);
    }
    iofile = open_FFSfile(filename, "R");

    if (iofile == NULL) {
	printf("File Open Failure \"%s\"", filename);
	perror("Opening input file");
	exit(1);
    }
    FMdumpVerbose = 1;
    if (dump_header)
/*	dump_FFSFile(iofile);*/

    buffer = malloc(1024);
    buffer_size = 1024;

    while (1) {
	FFSTypeHandle format;

	switch (FFSnext_record_type(iofile)) {
	case FFScomment: {
	    char *comment = FFSread_comment(iofile);
	    printf("Got a comment\n");
	    if (dump_comments) {
		if (comment)
		    printf("Comment -> \"%s\"\n", comment);
	    }
	    break;
	}
	case FFSformat:
	    format = FFSread_format(iofile);
	    dump_formats = 1;
	    if (dump_formats) {
		dump_FMFormat(FMFormat_of_original(format));
	    }
	    break;
	case FFSdata:
	    if (buffer_size < FFSnext_data_length(iofile)) {
		buffer_size = FFSnext_data_length(iofile);
		buffer = realloc(buffer, buffer_size);
	    }
	    FFSread_raw(iofile, buffer, buffer_size, &format);
	    if (dump_data) {
		dump_raw_FMrecord(iofile, format, buffer);
	    }
	    break;
	case FFSerror:
	case FFSend:
	    close_FFSfile(iofile);
	    exit(0);
	}
    }
}
