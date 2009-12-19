#include "config.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <ffs.h>
#include "cercs_env.h"
#include "ffs_internal.h"
#include "ffs_marshal.h"

extern field_marshal_info
get_marshal_info(FMFormat f, FMTypeDesc *t)
{
    format_marshal_info info = (format_marshal_info) f->ffs_info;
    int i;
    if (info == NULL) return NULL;
    for (i=0; i < info->count; i++) {
	if (info->field_info[i].t == t) {
	    return &info->field_info[i];
	}
    }
    return NULL;
}
