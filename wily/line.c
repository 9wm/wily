/***********************************************************
 *	Functions to do with finding/counting lines.
 *	
 *	Currently Wily doesn't keep track of line boundaries,
 *	and searches for them when it needs to.  These functions
 *	are all in the one file in case we ever decide to cache any
 *	of this information.
 ***********************************************************/

#include "wily.h"
#include "text.h"

/* What line number is at Rune position p? */
int
text_linenumber(Text *t, ulong p) {
	int	c;
	int	newlines=1;

	Tbgetcset(t, p);
	while ( (c=Tbgetc(t)) != -1)
		if(  c == NEWLINE)
			newlines++;
	return newlines;
}

/*
 * If 't' has at least 'n' lines, fills 'r' with the range for line 'n',
 * and returns true.  Otherwise, fills 'r' with gibberish and returns
 * false.
 */
Bool
text_findline(Text *t, Range *r, ulong n) {
	Bool	foundstart = false;
	int	c;

	if (n==0)
		return false;
	else if (n==1) {
		foundstart = true;
		r->p0 = 0;
	}
	n--;
	Tgetcset(t,0);
	for( ; (c=Tgetc(t)) != -1; ) {
		if( c == NEWLINE ) {
			if (foundstart)
				break;
			else if (!--n) {
				r->p0 = t->pos;
				foundstart = true;
			}
		}
	}
	r->p1 = t->pos;
	return foundstart;
}

/* Return the range for the last line of 't' */
Range
text_lastline(Text *t) {
	int	c;

	Tbgetcset(t, t->length);
	while ( (c=Tbgetc(t)) != -1  && c != NEWLINE)
		;
	return range(t->pos+1, t->length);
}

/* 
 * Find the offset of the first character of the line 'delta' away from 'pos'.
 *'delta' may be positive or negative.
 * 
 * text_nl(t, pos, 0):	start of current line
 * text_nl(t, pos, -1):	start of line above
 * text_nl(t, pos, 1):	start of line below
 */
ulong
text_nl(Text *t, ulong pos, int delta) {
	int	c;
	ulong	retval;

	assert(pos <= t->length);
	
	if(delta > 0) {
		Tgetcset(t, pos);
		while ( (c=Tgetc(t)) != -1 )
			if(c==NEWLINE)
				if(!--delta)
					break;
		retval = (c != -1) ? t->pos : t->length;
	} else {
		Tbgetcset(t,pos);
		while ( (c=Tbgetc(t)) != -1 )
			if(c==NEWLINE)
				if(!delta++)
					break;
		retval = (c != -1) ? t->pos+1 : 0;
	}
	return retval;
}

/* Return the position of the first character of the line containing 'p' */
ulong
text_startOfLine(Text *t, ulong p) {
	int c;
	
	Tbgetcset(t,p);
	do {
		c=Tbgetc(t);
	} while ( c != -1 && c != NEWLINE);
	return (c != -1) ? t->pos+1 : t->pos;
}
