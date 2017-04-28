
#include <stdio.h>
#include "config.h"

static char *EVPath_version = "EVPath Version 4.0.154 rev. 26746  -- 2017-04-27 16:21:31 -0400 (Thu, 27 Apr 2017)\n";

#if defined (__INTEL_COMPILER)
//  Allow extern declarations with no prior decl
#  pragma warning (disable: 1418)
#endif
void EVprint_version()
{
    printf("%s",EVPath_version);
}
void EVfprint_version(FILE*out)
{
    fprintf(out, "%s",EVPath_version);
}

