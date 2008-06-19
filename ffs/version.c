#if defined(FUNCPROTO) || defined(__STDC__) || defined(__cplusplus) || defined(c_plusplus)
#ifndef ARGS
#define ARGS(args) args
#endif
#else
#ifndef ARGS
#define ARGS(args) (/*args*/)
#endif
#endif

#ifdef MODULE
#include <kernel/pbio_kernel.h>
#else
#include <stdio.h>
#include "config.h"
#include "unix_defs.h"
#endif

static char *PBIO_version = "PBIO Version 4.0.93 -- Wed Jun 18 20:17:45 EDT 2008\n";

void
IOprint_version()
{
    printf("%s",PBIO_version);
}

