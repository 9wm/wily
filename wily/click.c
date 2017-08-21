/*******************************************
 *	Handle double-click, etc.
 *******************************************/

#include "wily.h"
#include "text.h"
#include <ctype.h>

/* Matching brackets for double-click
 */
static Rune	l1[] =		{ '{', '[', '(', '<', 0253, 0};
static Rune	r1[] =		{'}', ']', ')', '>', 0273, 0};
static Rune	l2[] =		{ '\n', 0};
static Rune	r2[] =		{'\n', 0};
static Rune	l3[] =		{ '\'', '"', '`', '>', 0};
static Rune	r3[] =		{'\'', '"', '`', '<', 0};

Rune		*left[]=	{ l1, l2, l3, 0};
Rune		*right[]=	{ r1, r2, r3, 0};

/* Return false if 'c' is obviously not an alphanumeric character,
 * or if it is in the stoplist.
 */
static Bool
okchar(int c, char *stoplist) {
	/*
	 * Hard to get absolutely right.  Use what we know about ASCII
	 * and assume anything above the Latin control characters is
	 * potentially an alphanumeric.
	 */
	if(c <= ' ')
		return false;
	if(0x7F<=c && c<=0xA0)
		return false;
	if(utfrune(stoplist, c))
		return false;
	return 1;
}

static int
clickmatch(Text *t, int cl, int cr, int dir) {
	int c, nest;

	nest=1;
	while((c=(dir>0? Tgetc(t) : Tbgetc(t))) > 0) {
		if(cl=='\n' && c==0x04)	/* EOT is trouble */
			return 1;
		if(c == cr){
			if(--nest == 0)
				return 1;
		}else if(c == cl)
			nest++;
	}
	return cl=='\n' && nest==1;
}

static Rune *
strrune(Rune *s, Rune c) {
	Rune c1;

	if(c == 0) {
		while(*s++)
			;
		return s-1;
	}

	while( (c1 = *s++))
		if(c1 == c)
			return s-1;
	return 0;
}

/* Expand the selection (pp0-pp1) left and right, stopping
 * at characters that aren't alphanumeric or are in the stop list 's'
 */
Range
text_expand(Text *t, Range r, char *s) {
	int	c;

	Tgetcset(t, r.p1);
	while( (c = Tgetc(t)) !=-1 && okchar(c,s))
		r.p1++;
	Tbgetcset(t, r.p0);
	while((c = Tbgetc(t)) != -1 && okchar(c,s))
		r.p0--;
	return r;
}

/*
 * Expand a doubleclick at p0, return the resulting range.
 */
Range
text_doubleclick(Text *t, ulong p0) {
	int c, i;
	Rune *r, *l;
	Range	dot;

	if(p0 > t->length)
		return dot;
	dot.p0 = dot.p1 = p0;
	for(i=0; left[i]; i++){
		l = left[i];
		r = right[i];
		/* try left match */
		if(p0 == 0){
			Tgetcset(t, p0);
			c = '\n';
		}else{
			Tgetcset(t, p0-1);
			c = Tgetc(t);
		}
		if(c!=-1 && strrune(l, c)){
			if(clickmatch(t, c, r[strrune(l, c)-l], 1)){
				dot.p0 = p0;
				dot.p1 = t->pos-(c!='\n');
			}
			return dot;
		}
		/* try right match */
		if(p0 == t->length){
			Tbgetcset(t, p0);
			c = '\n';
		}else{
			Tbgetcset(t, p0+1);
			c = Tbgetc(t);
		}
		if(c !=-1 && strrune(r, c)){
			if(clickmatch(t, c, l[strrune(r, c)-r], -1)){
				dot.p0 = t->pos;
				if(c!='\n' || t->pos!=0 ||
				   (Tgetcset(t, 0),Tgetc(t))=='\n')
					dot.p0++;
				dot.p1 = p0+(p0<t->length && c=='\n');
			}
			return dot;
		}
	}
	/* try filling out word to right */
	Tgetcset(t, p0);
	while((c=Tgetc(t))!=-1 && okchar(c, notdoubleclick))
		dot.p1++;
	/* try filling out word to left */
	Tbgetcset(t, p0);
	while((c=Tbgetc(t))!=-1 && okchar(c, notdoubleclick))
		dot.p0--;
	return dot;
}

/*
 * Find start of word, going back from p0.
 */
ulong
text_startofword(Text *t, ulong p0) {
	int c;

	Tbgetcset(t, p0);

	/* skip over special chars then skip until special char */
	while ( (c=Tbgetc(t))!=-1 && c != '\n' && !okchar(c, notdoubleclick) )
		;
	if(c=='\n' && (t->pos != p0-1))
		return t->pos + 1;
	while ( (c=Tbgetc(t))!=-1 && okchar(c, notdoubleclick) )
		;
	return t->pos + (c!=-1);
}

