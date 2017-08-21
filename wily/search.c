/*******************************************
 *	Search in text
 *******************************************/

#include "wily.h"
#include "text.h"

static char	*endword = "[^a-zA-Z0-9][^a-zA-Z0-9:.$|]*";
static char	*startword = "[^a-zA-Z0-9]";
static char 	*special = ".*+?(|)\\[]^$";

static Bool	findend(Text *t, Range *r, char *addr, Range dot);
static Rstring	word(Rstring s);
static Rstring	literal(Rstring s);
static void	strip_re_slash(char *re);

/* Look for the text in 'dot'.
 * If found, set '*r' and return true,
 * otherwise return false.
 */
Bool
text_look(Text*t, Range *r, Range dot) {
	RPath	buf;
	ulong	len;

	assert(!text_badrange(t,dot));

	len = RLEN(dot);
	/* don't try to search for HUGE dot */	
	if( len > MAXPATH)
		return false;
	
	text_copy(t, dot, buf);
	return text_findliteral(t, r, rstring(buf, buf + len));
}

Bool
text_findliteralutf(Text*t, Range *r, char*lit)
{
	Rstring	s;
	Bool	found;

	s = utf2rstring(lit);
	/* r->p0 = r->p1; */
	found = text_findliteral(t, r, s);
	free(s.r0);

	return found;
}

Bool
text_findwordutf(Text*t, Range *r, char*lit)
{
	Rstring	s;
	Bool	found;

	s = utf2rstring(lit);
	found = text_findword(t, r, s);
	free(s.r0);

	return found;
}

/*
 * If we can find 's' in 't' (start looking at 'r'), return true and
 * set 'r' to the location of the start of the string.  Otherwise,
 * return false.
 */
Bool
text_findliteral(Text *t, Range *r, Rstring s) {
	Rstring	s2;
	Bool		found;

	s2 = literal(s);
	found =  text_regexp(t, 	s2, r, 1);
	RSFREE(s2);
	return found;
}

Bool
text_findword(Text *t, Range *r, Rstring s) {
	Rstring	s2;
	Bool		found;

	s2 = word(s);
	found =  text_regexp(t, 	s2, r, 1);
	RSFREE(s2);
	r->p0++;
	return found;
}

/* If we can find 'addr', preferably someplace just after 'r',
 * set 'r' to the range we found, and return true, otherwise return false.
 * 'addr' may be any Sam-style address.
 */
Bool
text_search(Text *t, Range *r, char *addr, Range dot)
{
	char	*addr2;

	assert(addr);

	/*
	 * Find the second addr in the pair.  Some complexity here:
	 *	(1) If the first addr is a regexp, must first find its end -- but watch
	 *		out for escaped "/"'s.
	 *	(2) Otherwise, just find the first comma.
	 */
	 if (*(addr2=addr) == '-')
	 	addr2++;
	if (*addr2++ == '/') {
		/* find an unescaped "/" */
		while ((addr2 = strchr(addr2, '/')) && addr2[-1] == '\\')
			addr2++;
		/* check to see that it's followed by a comma */
		if (addr2 && *++addr2 != ',')
			addr2 = 0;
	}
	else
		addr2 = strchr(addr, ',');

	if (addr2)
		*addr2++ = '\0';

	if (*addr == '\0') {
		r->p0 = 0;
	} else if (!findend(t, r, addr, dot)) {
		return false;
	}
	if (addr2) {
		Range	range2 = *r;

		if (*addr2 == '\0')
			r->p1 = t->length;
		else if (!findend(t, &range2, addr2, dot))
			return false;
		else if ( range2.p1 < r->p0)
			return false;
		else
			r->p1 = range2.p1;
	}
	return true;
}

/*
 * Used to find one "end" of an addr (which might be the whole addr, but
 * never mind).  The address passed in here is not expected to contain
 * commas.
 */
static Bool
findend(Text *t, Range *r, char *addr, Range dot) {
	if(!strchr("/-#$.0123456789", *addr))
		return false;

	switch(*addr){
	case '/':
		strip_re_slash(addr);
		return text_utfregexp(t, addr+1, r, true);
	case '-':
		if(addr[1] != '/')
			return false;
		strip_re_slash(addr);
		return text_utfregexp(t, addr+2, r, 0);
	case '#':
		r->p0 = r->p1 = atol(addr + 1);
		return r->p0 <= t->length;
	case '$':
		if(addr[1] != '\0')
			return false;
		*r = text_lastline(t);
		return true;
	case '.':
		if(addr[1] != '\0')
			return false;
		*r = dot;
		return true;
	default:
		return text_findline(t, r, atol(addr));
	}
}

/*
 * Strip trailing slash from a regexp, if present and not escaped.
 * WARNING: This code can reference the character
 * immediately preceding its argument, so use with care.
 */
static void
strip_re_slash(char *re) {
	if ((re = strrchr(re, '/')) && re[1] == '\0' && re[-1] != '\\')
		re[0] = '\0';
}

static Rstring
literal(Rstring s) {
	Rstring	s2;
	Rune	*r;
	
	/* quote any special characters in 's' */
	s2.r0 = s2.r1 = (Rune*)salloc(RSLEN(s)*2*sizeof(Rune));
	for (r = s.r0; r < s.r1; r++){
		if(utfrune(special, *r))
			*s2.r1++ = '\\';
		*s2.r1++ = *r;
	}
	return s2;
}

static Rstring
word(Rstring s) {
	Rstring	s2;
	Rune	*r;
	int	nrunes;
	
	nrunes = RSLEN(s)*2 +strlen(startword) + strlen(endword);
	s2.r0 = s2.r1 = (Rune*)salloc(nrunes*sizeof(Rune));

	s2.r1 += utftotext(s2.r1, startword, startword + strlen(startword));

	for (r = s.r0; r < s.r1; r++){
		if(utfrune(special, *r))
			*s2.r1++ = '\\';
		*s2.r1++ = *r;
	}
	s2.r1 += utftotext(s2.r1, endword, endword + strlen(endword));
	return s2;
}

