/* 
 * ------------------------------------------------------------------------
 * wrapper for operations defined in string.h like strdup, malloc ...
 * ------------------------------------------------------------------------
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sgstring.h"

void 
sg_oom(const char *file,int line) 
{
	fprintf(stderr,"Out of memory in %s line %d\n",file,line);
	exit(1);
}
