#ifndef RD_HEADERS_H
#define RD_HEADERS_H

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/param.h>
#include <assert.h>
#ifdef __STDC__
char *strdup(char *);
#endif

#include <u.h>
#include <libc.h>
#include <util.h>
#include <libmsg.h>
#include "reader.h"
#include "proto.h"

#ifdef DEBUG
#define DPRINT(X)	(void)fprintf(stderr,"%s:%d:%s\n",__FILE__,__LINE__,X)
#else
#define DPRINT(X)
#endif

#define MAXFROMLINE	512
#define MAXDATE			24
#define MAXEMAIL		40
#define MAXFULLNAME	40
#define MAXSUBJECT		40

#define FROMEMAIL		0
#define FROMNAME		1
#define WHICHFROM		FROMNAME

#endif /* RD_HEADERS_H */
