/* sam.c - some stubs and interface code to tie sam's regexp.c
 * to the rest of wily.
 * S. Kilbane, 13/9/95.
 */

#include <setjmp.h>
#define SAM_CODE
#include "sam.h"

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
Finit(File *f, Text*t, Range *r)
{
	f->t = t;
	f->nrunes =  t->length;
	f->dot.r.p1 = r->p0;
	f->dot.r.w2 = r->p1;
	f->dot.f = f;
	return;
}

/*
 * stub routines
 */
static int
do_compile(String *s)
{
	compile(s);
	return (startinst == 0);
}

void
panic(char *str)
{
	(void)fprintf(stderr,"panic:%s\n",str);
	exit(1);
}

void
samerror(Err e)
{
	fprintf(stderr, "regexp error: %s\n",errs[e]);
	longjmp(regexp_state, 1);
}

void
Strduplstr(String *s0, String *s1)
{
	ulong n;

	n = s1->n;
	if (s0->n < s1->n)
		s0->s = srealloc(s0->s, n*RUNESIZE);
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
	fprintf(stderr, "regexp: %s, %c\n", errs[e], c);
	longjmp(regexp_state, 1);
}

void
Strzero(String *s)
{
	memset(s->s, 0, s->n*RUNESIZE);
}

/*
 * Seems to work, so now I need to sort out the handling of
 * ranges. I think it should search from the current p1,
 * wrap around, and continue to before p0. Should probably
 * play with ACME a bit, and see how that goes...
 */

/*
 * text_regexp(t,re,r,fwd) - the interface between wily and sam.
 * compile the regexp, and do a search from the current point.
 * If we find a match, set r, and return true. Otherwise,
 * return false.
 */
static Bool
text_strregexp(Text *t,  String str, Range *r, Bool fwd)
{
	File f;
	Bool found;
	ulong q0, q1;
	/* We run into all sorts of setjmp/longjmp madness
	 * if text_strregexp somehow calls itself.
	 * Let's make sure that doesn't happen.
	 * For this same reason, can't use diag anywhere
	 * in sam.c or regexp.c, fprintf(stderr,...) instead.
	 */
	static Bool active = false;

	assert(!active);
	active = true;
	if (setjmp(regexp_state)) {
		found = false;
		goto out;
	} 
	found =false;
	if (do_compile(&str)) {
		fprintf(stderr, "regexp: compilation failed\n");
		goto out;
	}
	q0 = r->p0;
	q1 = r->p1;
	Finit(&f, t, r);
	/* first, try after current posn to end of file. */
	found = fwd? execute(&f, q1, t->length) : bexecute(&f,q0);
	if (found) {
		r->p0 = sel.w[0].p1;
		r->p1 = sel.w[0].w2;
		goto out;
	}
	/* No good. wrap, and try from start of file. */
	found = fwd? execute(&f,0,t->length) : bexecute(&f,t->length);
	if (found) {
		r->p0 = sel.w[0].p1;
		r->p1 = sel.w[0].w2;
	}
out:
	active = false;
	return found;
}

Bool
text_utfregexp(Text *t, char *re, Range *r, Bool fwd)
{
	static String str;
	int l = (1+strlen(re))*sizeof(Rune);

	if (str.n <= l) {
		str.n = l;
		str.s = srealloc(str.s, str.n);
	}
	str.s[utftotext(str.s, re, re+strlen(re))] = 0;
	return text_strregexp(t, str, r, fwd);
}

Bool
text_regexp(Text *t, Rstring re, Range *r, Bool fwd)
{
	static String str;
	int l = (1+RSLEN(re))*sizeof(Rune);

	if (str.n <= l) {
		str.n = l;
		str.s = srealloc(str.s, str.n);
	}
	memcpy(str.s, re.r0, RSLEN(re)*sizeof(Rune));
	str.s[RSLEN(re)] = 0;

	return text_strregexp(t, str, r, fwd);
}

long
Tchars(Text *t, Rune *buf, ulong p0, ulong p1) {
	Range	r;
	
	assert(p0 <= p1);
	r = range( p0, MIN(p1, t->length));
	text_copy(t, r, buf);
	return RLEN(r);
}
