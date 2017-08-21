/*
 * general utilities...
 */

#include "headers.h"

char *
sstrdup(char *str)
{
	char *s = salloc(strlen(str)+1);
	strcpy(s,str);
	return s;
}

void
panic(const char *msg)
{
	fprintf(stderr,"%s\n", msg);
	exit(1);
}

ulong
atoul(char *str)
{
	return strtoul((const char *)str, (char **)0, 10);
}
