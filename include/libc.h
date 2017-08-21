/* Copyright (c) 1992 AT&T - All rights reserved. */

	/* Plan 9 C library interface */

typedef	unsigned short		Rune;

#define	sprint				sprintf
/* Not used and conflicts */
#if 0
#define	dup(a,b)			dup2(a,b)
#endif
#define	seek(a,b,c)			lseek(a,b,c)
#define	create(name, mode, perm)	creat(name, perm)
#define	exec(a,b)			execv(a,b)
#define	USED(a)
#define SET(a)

#define	_exits(v)			if (v!=0) _exit(1); else _exit(0)

enum
{
	OREAD	=	0,		/* open for read */
	OWRITE	=	1,		/* open for write */
	ORDWR	=	2,		/* open for read/write */
	ERRLEN	=	64		/* length of error message */
};

enum
{
	UTFmax		= 3,		/* maximum bytes per rune */
	Runesync	= 0x80,		/* cannot represent part of a utf sequence (<) */
	Runeself	= 0x80,		/* rune and utf sequences are the same (<) */
	Runeerror	= 0x80		/* decoding error in utf */
};

/*
 * new rune routines
 */
extern	int	runetochar(char*, Rune*);
extern	int	chartorune(Rune*, char*);
extern	int	runelen(long);
extern	int	fullrune(char*, int);

/*
 * rune routines from converted str routines
 */
extern	long	utflen(char*);		/* was countrune */
extern	char*	utfrune(char*, long);
extern	char*	utfrrune(char*, long);
extern	char*	utfutf(char*, char*);
/*
 *	Miscellaneous functions
 */
#ifdef	NEEDVARARG
extern	void	fprint();
#else
extern	void	fprint(int, char *, ...);
#endif
extern	int	notify (void(*)(void *, char *));
extern	int	errstr(char *);
extern	char*	getuser(void);
extern	void	exits(char*);
