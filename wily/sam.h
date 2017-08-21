/*******************************************
 *	Utility declarations for regexp.c
 *******************************************/

/* This source file is derived from the sam.h file in the sam
   distribution, which is: */
/* Copyright (c) 1992 AT&T - All rights reserved. */

/*
 * sam and wily both define an error() function. To avoid changing
 * anything in regexp.c, we use a macro to rename it. This relies
 * on other wily functions including wily.h before sam.h.
 */
#ifndef WILY_H
#define error(X)	samerror(X)
#define SAM_CODE
#endif

#define emalloc salloc
#define erealloc srealloc

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "wily.h"
#include "text.h"

#define	NSUBEXP	10		/* number of () matches in a regexp */
typedef long		Posn;		/* file position or address */
typedef struct Address	Address;
typedef struct File	File;
typedef struct samRange	samRange;
typedef struct samRangeset	samRangeset;
typedef struct String String;
typedef struct Inst Inst;

typedef enum {
	Etoolong,
	Eleftpar,
	Erightpar,
	Emissop,
	Ebadregexp,
	Ebadclass,
	Eoverflow
} Err;

struct String {
	int n;
	Rune *s;
};


struct samRange
{
	Posn	p1, w2;
};

struct samRangeset
{
	samRange	w[NSUBEXP];
};

struct Address
{
	samRange	r;
	File	*f;
};

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif
enum {
	FALSE=0,
	TRUE = 1,
	RUNESIZE	= (sizeof(Rune)),
	INFINITY	 = 0x7FFFFFFFL
};

/*
 * In the original sam.h, the File structure is that which manages
 * all the caches and buffers of the file being edited. We're not
 * concerned with any of that - we just want to make the contents
 * of a View look like a File to sam.
 */

struct File
{
	Text		*t;		/* the Text that we'll search */
	Posn	nrunes;		/* total length of file */
	Address	dot;		/* current position */
};

#define	Fgetc(f)  (Tgetc(f->t))
#define	Fbgetc(f) (Tbgetc(f->t))

#define	Fgetcload(f,p)		Tgetcload((f)->t, (p))
#define	Fbgetcload(f,p)	Tbgetcload((f)->t, (p))
#define	Fgetcset(f,p)		Tgetcset((f)->t, (p))
#define	Fbgetcset(f,p)		Tbgetcset((f)->t, (p))
#define	Fchars(f,r,p0,p1)	Tchars((f)->t, (r),(p0),(p1))

int	bexecute(File*, Posn);
void	compile(String*);
int	execute(File*, Posn, Posn);
void	nextmatch(File*, String*, Posn, int);
void	error_c(Err, int);
void	panic(char*);
void Strduplstr(String *, String *);
int Strcmp(String *, String *);
void Strzero(String *);
void error(Err);		/* really samerror() */

extern samRangeset sel;
extern Inst *startinst;
