#ifndef RD_HEADERS_H
#define RD_HEADERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/param.h>
#ifdef __STDC__
char *strdup(char *);
#endif

#include <u.h>
#include <libc.h>
#include <util.h>
#include <libmsg.h>
#include "reader.h"
#include "proto.h"
#include "membuf.h"

#endif /* RD_HEADERS_H */
