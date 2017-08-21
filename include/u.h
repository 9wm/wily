/* Copyright (c) 1992 AT&T - All rights reserved. */

/* #ifdef */
#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <setjmp.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif /* HAVE_MALLOC_H */

#ifndef HAVE_ULONG
typedef unsigned long	ulong;
#endif
#ifndef HAVE_USHORT
typedef unsigned short	ushort;
#endif
#ifndef HAVE_UINT
typedef unsigned int	uint;
#endif
#ifndef HAVE_UCHAR
typedef unsigned char	uchar;
#endif
#ifndef HAVE_CADDR_T
typedef char *caddr_t;
#endif

#ifndef HAVE_REMOVE
#define remove(x) unlink(x)
#endif

#ifdef BROKEN_SPRINTF
int	Asprintf(char*,char*,...);
#else
#define Asprintf sprintf
#endif
