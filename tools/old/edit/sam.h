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

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "wily.h"

#define	NSUBEXP	10		/* number of () matches in a regexp */
typedef long		Posn;		/* file position or address */
typedef struct Address	Address;
typedef struct File	File;
typedef struct Range	Range;
typedef struct Rangeset	Rangeset;
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


struct Range
{
	Posn	p1, p2;
};

struct Rangeset
{
	Range	p[NSUBEXP];
};

struct Address
{
	Range	r;
	File	*f;
};


#define	FALSE	0
#define	TRUE	1
#define	RUNESIZE	(sizeof(Rune))
#define	NGETC	128		/* Shouldn't actually be used */
#define	INFINITY	0x7FFFFFFFL


/*
 * In the original sam.h, the File structure is that which manages
 * all the caches and buffers of the file being edited. We're not
 * concerned with any of that - we just want to make the contents
 * of a Text look like a File to sam.
 */

struct File
{
	Text *t;			/* the Text that we'll search */
	Posn	nrunes;	/* total length of file */
	Address	dot;		/* current position */
	Rune	*getcbuf;	/* pointer to t->text */
	int	ngetc;		/* must be ==nrunes */
	int	getci;		/* index into getcbuf */
	Posn	getcp;	/* must ==getci */
};


#define	Fgetc(f)  ((--(f)->ngetc<0)? Fgetcload(f, (f)->getcp) : (f)->getcbuf[(f)->getcp++, (f)->getci++])
#define	Fbgetc(f) (((f)->getci<=0)? Fbgetcload(f, (f)->getcp) : (f)->getcbuf[--(f)->getcp, --(f)->getci])

int	Fbgetcload(File*, Posn);
int	Fbgetcset(File*, Posn);
long	Fchars(File*, Rune*, Posn, Posn);
int	Fgetcload(File*, Posn);
int	Fgetcset(File*, Posn);
int	bexecute(File*, Posn);
void	compile(String*);
int	execute(File*, Posn, Posn);
void	nextmatch(File*, String*, Posn, int);
void	*emalloc(ulong);
void	*erealloc(void*, ulong);
void	error_c(Err, int);
void	panic(char*);
void Strduplstr(String *, String *);
int Strcmp(String *, String *);
void Strzero(String *);
void error(Err);		/* really samerror() */

extern Rangeset sel;
extern Inst *startinst;
