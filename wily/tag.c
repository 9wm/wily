/*******************************************
 *	Maintain and monitor tags
 *******************************************/

#include "wily.h"
#include "view.h"
#include "text.h"
#include <string.h>
#include <ctype.h>

/* The tag for a window contains: a label then whitespace, then some 
 * system-maintained tools, then a pipe symbol, then user stuff.
 */

static char * whitespace_regexp = "[ \t\n]+";
static Range	tag_findtool(Text *, char*);

static Bool		wily_modifying_tag = false;

/************************************************
	Gettools and Settools only used by the messaging interface.
	It's still up in the air what exactly they should do.
	TODO - ensure space before/after "|" separator.
************************************************/
char*
tag_gettools(Text*t){
	Range r;
	
	r = nr;
	if(text_utfregexp(t, "\\|.*", &r, true)) {
		r.p0++;
		return text_duputf(t, r);
	} else {
		return "";
	}
}

void
tag_settools(Text*t, char*s) {
	Range r = nr;
	ulong	len;
	
	if(text_utfregexp(t, "\\|.*", &r, true)) {
		r.p0++;
	} else {
		len = text_length(t);
		r = range(len,len);
	}
	text_replaceutf(t,r,s);
}

void
tag_set(Text*t, char*s) {
	View	*v;

	assert(TEXT_ISTAG(t));

	wily_modifying_tag = true;
	text_replaceutf(t, text_all(t), s);
	if ((v = text_view(t)))
		v->anchor = text_length(t);
	wily_modifying_tag = false;
}

void
tag_setlabel(Text *t, char *s)
{
	Range	r;
	Path		buf;
	ulong	l;
	
	wily_modifying_tag = true;

	/* find first whitespace_regexp */
	r = range(0,0);
	if(! text_utfregexp(t, whitespace_regexp, &r, true)) {
		l = text_length(t);
		r = range(l,l);
	}

	r.p0 = 0;
	sprintf(buf, "%s ", s);
	text_replaceutf(t, r, buf);
	wily_modifying_tag = false;
}

/* Remove 's' from the tool section of tag 't', if it exists there.
 */
void
tag_rmtool(Text *t, char *s)
{
	Range	r;
	
	wily_modifying_tag = true;

	r = tag_findtool(t, s);
	if(RLEN(r))
		text_replace(t, r, rstring(0,0));
	if(STRSAME(s,"Put"))
		text_fillbutton(t, Zero);
	wily_modifying_tag = false;
}

/* Replace 'r' in 't' with 's' and a trailing space */
static void
place_tool(Text*t, Range r, char*s) {
	Path	tmp;
	
	sprintf(tmp, "%s ", s);
	text_replaceutf(t,r,tmp);
}

/* 
 * Add a string to represent a running command to 't'.
 *
 * Almost the same as tag_addtool, the difference being
 * that tag_addrunning will create duplicates, e.g. the
 * tag could read "xterm xterm "
 */
void
tag_addrunning(Text *t, char *cmd) {
	Range	r;
	
	r = tag_findtool(t,cmd);
	r.p0  = r.p1;	/* don't replace */
	place_tool(t,r,cmd);
}

/* Add 's' to the tool section of tag 't', unless it's there already.
*/
void
tag_addtool(Text *t, char *s)
{
	Range	r;

	wily_modifying_tag = true;
	
	r = tag_findtool(t,s);
	if(!RLEN(r))
		place_tool(t,r,s);
	if(STRSAME(s,"Put"))
		text_fillbutton(t, F);
	wily_modifying_tag = false;
}

/* If 's' is in the system tools section of t, return the range
 * containing 's' and all the immediately following whitespace.
 * Otherwise return a 0-length range where the next tool
 * should be inserted.
 */
static Range
tag_findtool(Text*t, char*s)
{
	Range	pos, endpos;
	ulong	l;

	endpos = pos = nr;
	if(text_findliteralutf(t, &endpos, "|")) {
		endpos.p1 = endpos.p0;
	} else {
		l = text_length(t);
		endpos = range(l,l);
	}

	if(text_findwordutf(t, &pos, s) && pos.p0 < endpos.p0)
		return pos;

	return endpos;
}

/*
 * 't' has been modified in some fashion. 'p' is the index of the
 * most recently modified character.
 */
void
tag_modified(Text*t, ulong p) {
	Range	r;
	Path		buf;
	int		n;
	
	assert(t->data && TEXT_ISTAG(t));
	if( wily_modifying_tag)
		return;
	
	/* find first whitespace_regexp */
	r = nr;
	if(text_utfregexp(t, whitespace_regexp, &r, true)) {
		if(p>r.p0)
			return;
		r.p1 = r.p0;
		r.p0 = 0;
	} else {
		r = text_all(t);
	}

	n = text_copyutf(t, r, buf);
	buf[n] = 0;
	data_setlabel(text_data(t), buf);
}
