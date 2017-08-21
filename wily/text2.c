/*******************************************
 *	Convenience wrappers around Text methods
 *******************************************/

#include "wily.h"
#include "text.h"
#include "view.h"
#include <ctype.h>

/****************************************************
	Random garbage
****************************************************/
void
text_allread (Text*t) {
	undo_reset(t);
	undo_start(t);
	viewlist_refresh(t->v);
}

/****************************************************
	Simple data access
****************************************************/
Data*
text_data(Text*t) {
	return t? t->data:0;
}

View*
text_view(Text*t) {
	return t->v;
}

ulong
text_length(Text*t) {
	return t->length;
}

Bool
text_needsbackup(Text*t) {
	return t->needsbackup;
}

/****************************************************
	Cooked data access
****************************************************/
/* Return body view associated with 't'. */
View*
text_body(Text*t) {
	if(t->data) {
		if (TEXT_ISTAG(t)) {
			t = data_body(t->data);
			return t->v;
		} else {
			return t->v;
		}
	} else {
		return 0;
	}
}

/****************************************************
	Simple data manipulations
****************************************************/

void
text_setneedsbackup(Text*t, Bool b) {
	t->needsbackup = b;
}

Bool
text_badrange(Text *t, Range r) {
	return r.p1 > t->length || r.p0 > r.p1;
}

void
text_addview(Text*t, View*v) {
	v->next = t->v;
	t->v = v;
}

/*
 * Remove 'v' from list of views displaying 't'.  If 'v' was the last
 * one, remove 't'.  If 't' represents some Data, make sure we've backed
 * it up if necessary.  Return 0 for success.
 */
int
text_rmview(Text*t, View *v) {
	View**ptr;

	if (t->v ==v && v->next == 0) {
		/* this is the last view */
		if (t->isbody && data_del(t->data))
			return -1;
	}
	/* update the list of views of the same body */
	for (ptr = &(t->v); *ptr != v; ptr = &( (*ptr)->next))
		;
	*ptr = v->next;

	if(!t->v) { /* we've deleted the last view */
		text_free(t);
		free(t);     
	}
	return 0;
}

/****************************************************
	Write to file
****************************************************/

/* Write the contents of 't' to 'fname'.  Return 0 for success. */
int
text_write(Text *t, char *fname) {
	int		fd;
	int		retval;

	/* open 0666 and rely on the umask */
	if((fd = open(fname,O_RDWR|O_CREAT|O_TRUNC, 0666))<0){
		diag(fname, "couldn't open %s for write",fname);
		return 1;
	}
	retval = text_write_range(t, range(0, t->length), fd);
	if(retval)
		diag(fname, "couldn't write %s",fname);
	close(fd);
	return retval;
}

/*Return a file descriptor open for reading from a temporary
 * file which has a copy of the current selection.
 * 
 * The input for the child comes from the current selection.
 * Put the selection into a text file and attach that text
 * file to the command's fdin.
 */
int
text_fd(Text *t, Range sel)
{
	char	*file = tmpnam(0);
	int	fd = -1;
	int	input = -1;

	if ((fd = open(file, O_WRONLY|O_CREAT, 0600)) < 0) {
		perror("open temp file");
		goto fail;
	}

	/* Now for the child's end.  Do it quick so we can unlink. */
	if ((input = open(file, O_RDONLY)) < 0) {
		perror("open temp file");
		goto fail;
	}

	if (unlink(file) < 0)
		perror("unlink temp file");
		/* no need to *do* anything about it */

	/* Our buffer is the most recent selection, or if the
	 * most recent selection is in a tag, the selection in the body
	 * of that win.
	 */
	if (text_write_range(t, sel, fd)) {
		perror("write temp file");
		goto fail;
	}
	if (close(fd) <  0)
		perror("close temp file");

	return input;

 fail:
	if (input >= 0)
		(void) close(input);
	if (fd >= 0) {
		(void) close(fd);
		(void) unlink(file);
	}
	return(-1);
}

/****************************************************
	Auto indent
****************************************************/
enum {MAXAI=128};

/*
 * Return an Rstring to be inserted after position 'p',
 * given that autoindent is on.
 * The Rstring is to a *static* Rune buffer.
 */
/** NB *not* reentrant */
Rstring
text_autoindent(Text *t, ulong p)
{
	static Rune buf[MAXAI];
	Rstring s;
	Range	r;
	int	i;

	buf[0] = '\n';
	s.r0 = buf;

	r = range(text_startOfLine(t, p), p);
	if (RLEN(r) > MAXAI)
		r.p1 = r.p0 + MAXAI;
	text_copy(t, r, buf +1);

	i = 1;
	while (i <= RLEN(r) && (buf[i] < 128) && isspace(buf[i])) /* FIXME unicode isspace() */
		i++;
	s.r1 = buf + i;

	return s;
}

/****************************************************
	Convenience functions
****************************************************/

/*
 * Return the character to start displaying from,
 * such that the character at 'p' will be offset by
 * 'height', when displayed
 * in a window of width 'w', with Font 'f'
 */ 
int
back_height(Text *t, ulong p, Font *f, int width, int height) {
	int	c, hpos;
	
	if (p > 0) --p;
	for( hpos = 0; p>0 && height>0; p--) {
		Tgetcset(t,p);
		c = Tgetc(t);
		switch(c) {
		case '\t': hpos += tabsize; break;
		case '\n': hpos = 0; height -= f->height; break;
		default: hpos += charwidth(f,c); break;
		}
		if (hpos >= width) {
			hpos =0;
			height -= f->height;
		}
	}
	return p? p+2 : p;
}

Range
text_all(Text*t){
	Range r;
	
	r.p0 = 0;
	r.p1 = t->length;
	return r;
}

void
text_fillbutton(Text*t, Fcode f) {
	View*v;
	for(v = t->v; v; v= v->next)
		view_fillbutton(v,f);
}

/* Try to copy 'n' Runes at 'p' from 't' into 'buf'.
 * Return the number of runes copied.
 */
ulong
text_ncopy(Text *t, Rune*buf, ulong p, ulong n)
{
	Range	r;
	if (p >= t->length)
		return 0;
	r = range(p, MIN(p+n, t->length));
	text_copy(t, r, buf);
	return RLEN(r);
}

/* Replace 'r' in 't' with 'utf', return the range of newly placed runes */
Range
text_replaceutf(Text*t, Range r, char*utf) {
	Rstring	s;

	s = utf2rstring(utf);
	r =  text_replace(t, r, s);
	if(s.r0)
		free(s.r0);
	return r;
}

/* Return a newly allocated string containing the utf
 * representation of the given range inside 't'
 */
char *
text_duputf(Text *t, Range r) {
	ulong	len = RLEN(r);
	ulong	n;
	char	*buf;

	if(!len)
		return strdup("");

	assert(len > 0);
	buf = salloc(len * UTFmax);
	n = text_copyutf(t, r, buf);
	buf[n] = '\0';
	return buf;
}

/* Copy Range 'r' from 't', convert it to utf, store it
 * in 'buf', (which must have enough space allocated).
 * Returns the number of utf characters copied.
 */
int
text_copyutf(Text*t, Range r, char *buf)
{
	Rune	*rbuf;
	int		rlen;
	int		n;

	rlen = RLEN(r);
	assert(rlen >= 0);

	if (rlen == 0)
		return 0;

	rbuf = salloc( rlen * sizeof(Rune));
	text_copy(t, r, rbuf);
	n =  texttoutf(buf, rbuf, rbuf + rlen);
	free(rbuf);
	return n;
}

/****************************************************
	Formatting text representing a directory
****************************************************/
static void
text_getdir(Text *t, char**names) {
	Frame	*f;
	char	*s;
	
	f = &t->v->f;
	s = columnate(Dx(f->r), f->maxtab, f->font, names);
	
	undo_reset(t);
	text_replaceutf(t, range(0, t->length), s);
	undo_start(t);
	free(s);
}

/*
* If 't' represents a directory, and doesn't have any modified text,
* reformat the directory and return true.  Otherwise return false.
*/
Bool
text_refreshdir(Text*t)
{
	char**names;

	if ( t->isbody && (names = data_names(t->data)) && undo_atmark(t) && t->v) {
		text_getdir(t, names);
		return true;
	} else {
		return false;
	}
}

/* Reformat 'd' (which must hold a directory) to fit nicely
 * in the smallest view displaying it.
 */
void
text_formatdir(Text *t, char**names) {
	if(!(t->v && names))
		return;

	text_getdir(t,names);
	viewlist_refresh(t->v);
}
