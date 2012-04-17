#include "config.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char **search_list = NULL;

void
CMdladdsearchdir(char *string)
{
    int count = 0;
    if (search_list == NULL) {
	search_list = malloc(2*sizeof(char*));
    } else {
	while(search_list[count] != NULL) count++;
    }
    search_list[count] = strdup(string);
    search_list[count+1] = NULL;
}

typedef struct {
    void *dlopen_handle;
    char *lib_prefix;
} *dlhandle;

void *
CMdlopen(char *lib)
{
    int i;
    dlhandle dlh;
    void *handle;
    char *tmp;

    char **list = search_list;
    if ((handle = dlopen(lib, 0)) == NULL) {
	while(list && (list[0] != NULL)) {
	    char *tmp = malloc(strlen(list[0]) + strlen(lib) + 2);
	    sprintf(tmp, "%s/%s", list[0], lib);
	    handle = dlopen(tmp, 0);
	    list++;
	    if (handle) list = NULL; // fall out
	}
    }
    if (!handle) return NULL;
    dlh = malloc(sizeof(*dlh));
    tmp = rindex(lib, '/'); /* find name start */
    if (!tmp) tmp = lib;
    dlh->lib_prefix = malloc(strlen(tmp) + 4);
    strcpy(dlh->lib_prefix, tmp);
    tmp = rindex(dlh->lib_prefix, '.');
    strcpy(tmp, "_LTX_");  /* kill postfix, add _LTX_ */
    dlh->dlopen_handle = handle;
    return (void*)dlh;
}

void*
CMdlsym(void *vdlh, char *sym)
{
    dlhandle dlh = (dlhandle)vdlh;
    char *tmp = malloc(strlen(sym) + strlen(dlh->lib_prefix) + 1);
    strcpy(tmp, dlh->lib_prefix);
    strcat(tmp, sym);
    return dlsym(dlh->dlopen_handle, tmp);
}