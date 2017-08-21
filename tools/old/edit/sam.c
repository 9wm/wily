/*
 * sam.c - some stubs and interface code to tie sam's regexp.c
 * to some standalone programs that hack a Rune-based file
 * extracted from a wily window.
 * S. Kilbane, 21/9/95.
 */

#include <setjmp.h>
#define SAM_CODE
#include "range.h"

/* the File that we're going to operate on.. */

File runefile;

/*
 * some global stuff that should be tidied up later...
 */

char origfile[FILENAME_MAX+1],  origaddr[FILENAME_MAX+1];
char tmpfilename[FILENAME_MAX+1];
Subrange *minmax;
ulong base;
char *utffile;
int modified;

static jmp_buf regexp_state;

static char *errs[] = {
	"Etoolong",
	"Eleftpar",
	"Erightpar",
	"Emissop",
	"Ebadregexp",
	"Ebadclass",
	"Eoverflow"
};

/*
 * Initialise a File to the Text given.
 */

void
Finit(File *f, char *filename)
{
	/* load the file in. */
	struct stat st;
	FILE *fp;
	ulong p0 = 0, p1;

	if (stat(filename,&st)) {
		perror(filename);
		exit(1);
	}
	p1 = st.st_size / RUNESIZE;
	if ((fp = fopen(filename,"r")) == 0) {
		perror(filename);
		exit(1);
	}
	f->nrunes = f->ngetc = p1;
	f->dot.r.p1 = 0;
	f->dot.r.p2 = p1;
	f->dot.f = f;
	f->getcbuf = salloc(p1*RUNESIZE);
	f->getci = f->getcp = 0;	/* start at beginning of range */
	fread(f->getcbuf, p1, RUNESIZE, fp);
	fclose(fp);
	return;
}

/*
 * stub routines
 */

int
Fgetcload(File *f, Posn p)
{
	return 0;
}

int Fbgetcload(File *f, Posn p)
{
	return -1;
}

int
Fbgetcset(File *f, Posn p)
{
	f->getcp = f->getci = p;
	return f->ngetc = f->nrunes;
}

int
Fgetcset(File *f, Posn p)
{
	f->getcp = f->getci = p;
	return f->ngetc = f->nrunes;
}

long
Fchars(File *f, Rune *r, Posn p0, Posn p1)
{
	long n;

	if (p0 > f->nrunes)
		p0 = f->nrunes;
	if (p1 > f->nrunes)
		p1 = f->nrunes;
	if ((n = p1-p0) < 0) {
		fprintf(stderr, "regexp: Negative amount asked for by Fchars()");
		return 0;
	}
	memcpy(r,f->getcbuf+p0, n*sizeof(Rune));
	return n;
}


/*
 * compile the regexp
 * returns 0 for success.
 */

int
init_regexp(char *re)
{
	static String str;
	int l = strlen(re);

	if (str.n <= l) {
		str.n = ++l;
		l *= sizeof(Rune);
		str.s = srealloc(str.s,l);
	}
	if (setjmp(regexp_state))
		return 0;		/* error occurred */
	str.s[utftotext(str.s, re, re+strlen(re))] = 0;
	compile(&str);
	return (startinst == 0);
}

/*
 * run_regexp(p0,p1) - the interface between wily and sam.
 * compile the regexp, and do a search from the current point.
 * If we find a match, set p0 and p1, and return true. Otherwise,
 * return false.
 */

int
run_regexp(ulong q0, ulong q1, ulong *p0, ulong *p1)
{
	if (setjmp(regexp_state))
		return 0;		/* error occurred */
	if (execute(&runefile, q0, q1)) {
		*p0 = sel.p[0].p1;
		*p1 = sel.p[0].p2;
		return 1;
	}
	/* no match */
	return 0;
}

void
panic(char *str)
{
	(void)fprintf(stderr,"panic:%s\n",str);
	exit(1);
}

void *
emalloc(ulong len)
{
	return salloc(len);
}

void *
erealloc(void *ptr, ulong len)
{
	return srealloc(ptr,len);
}

void
samerror(Err e)
{
	fprintf(stderr,"regexp error: %s\n",errs[e]);
	longjmp(regexp_state, 1);
}

void
Strduplstr(String *s0, String *s1)
{
	ulong n;

	n = s1->n * RUNESIZE;
	if (s0->n < s1->n)
		s0->s = erealloc(s0->s, n);
	s0->n = s1->n;
	memcpy(s0->s, s1->s, n);
}

int
Strcmp(String *s0, String *s1)
{
	Rune *i, *j;

	if (!s0->s || !s1->s)
		return 1;
	for (i = s0->s, j = s1->s; *i && (*i == *j); i++, j++)
		;
	return *i != *j;
}

void
error_c(Err e, int c)
{
	fprintf(stderr,"regexp: %s, %c\n", errs[e], c);
}

void
Strzero(String *s)
{
	memset(s->s, 0, s->n*RUNESIZE);
}

/*
 * Read the information about the file from stdin.
 * returns 0 for success.
 */

void
read_info(int rev)
{
	static char ranges[FILENAME_MAX];

	scanf("%s%s%s%d%lu\n",origfile,origaddr,tmpfilename, &modified,&base);
	DPRINT("of=%s,.=%s\n", origfile, origaddr);
	DPRINT("tf=%s,%s\n",tmpfilename, modified? "modified" : "unchanged");

	/* this is a simple implementation - we just look at the
	ranges received, and take the widest area covered. */

	if ((minmax = read_ranges(rev)) == 0) {
		fprintf(stderr,"failed to read ranges\n");
		exit(1);
	}
		
	DPRINT("Chosen range: %lu-%lu\n",minmax->p0, minmax->p1);
	return;
}

/*
 * The following is copied from libtext/text.c, to prevent having
 * to link with the whole libtext library, and hence the libframe
 * library.c
 */

ulong
utftotext(Rune *r, char *s1, char *s2)
{
	Rune	*q;
	char	*t;
	
	if (s2 <= s1)
		return 0;
	for (t = s1, q = r; t < s2; q++)
		if (*(uchar *)t < Runeself)
			*q = *t++;
		else
			t += chartorune(q, t);
	return t-s1;
}
