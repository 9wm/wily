/* We use a simple buffer-gap.  This is not too good for working
 * with large files if we're simultaneously working at both ends of
 * the file, because everytime we make a change, we have to move
 * the gap to the position of the change.
 *
 * If we wanted to handle large files efficiently we should
 * probably use multiple gaps, or multiple separate buffers.
 * What we have is probably sufficient for most cases, though.
 *
 * If we wanted to worry about our memory footprint, instead
 * of keeping the whole file in memory as Runes, we could copy
 * it to a (or many) auxiliary files and convert to Runes on demand.
 * This would also help with latency of opening big files.  It would
 * also be much more complex.  Virtual memory is our friend.
 */

#include "wily.h"
#include "text.h"
#include <ctype.h>

static void	setgap(Text *t, ulong p, int n);

#ifndef NDEBUG
static Rune *
findnull(Rune*start, Rune*end)
{
	Rune	*p;

	for(p=start; p<end; p++)
		if(!*p)
			return p;
	return 0;
}
#endif

Bool
text_invariants(Text *t)
{
#ifndef NDEBUG
	Rune	*p;

	assert(t->gap.p1 >= t->gap.p0);
	assert(t->gap.p1 <= t->alloced);
	assert(t->length <= t->alloced);
	assert(!(p=findnull(t->text, t->text + t->gap.p0)));
	assert(!(p=findnull(t->text + t->gap.p1, t->text + t->alloced)));
	assert( TEXT_ISTAG(t) || t->data);  /* if we're not a tag, we have data */
#endif
	return true;
}

/* Read the contents of 't' from 'fd', which should have 'len' bytes.
 * Return 0 for success, -1 for failure
 */
int
text_read(Text *t, int fd, int len)
{
	int	desired, nread;
	char	buf[BUFFERSIZE];
	extern int utftotext_unconverted;
	int	offset;
	
	/* Ensure we have enough rune space.  Do it one
	 * block to avoid fragmentation
	 */
	desired = len*sizeof(Rune) + GAPSIZE;
	if (t->alloced < desired) {
		t->alloced = desired;
		free(t->text);
		t->text = salloc(t->alloced * sizeof(Rune));
	}
	t->length = 0;
	t->pos = 0;
	offset = 0;
	while(len > 0) {
		desired = MIN(len, BUFFERSIZE - offset);
		nread = read(fd, buf + offset, desired);
		if(nread<=0)
			return -1;
		t->length += utftotext(t->text+t->length, buf, buf + nread +offset);
		len -= nread;
		
		/*
		 * If there were bytes at the end of the buffer that
		 * weren't a complete rune, copy them back to the
		 * start of the buffer for the next time.
		 */
		if ( (offset = utftotext_unconverted)) {
			memcpy(buf, buf + desired - offset, offset);
		}
	}
	t->gap = range(t->length, t->alloced);
	undo_reset(t);
	undo_start(t);
	close(fd);
	viewlist_refresh(t->v);
	return 0;
}

/**
 * Convert the runes in [r0, r1) to UTF and write to 'fd'.
 * Return 0 for success.
 **/
static int
utfwrite(Rune *r0, Rune *r1, int fd) {
	char	buf[BUFFERSIZE+UTFmax];
	Rune	*p;
	char		*t;
	int		nwrite, nwritten;
	
	assert(r1 >= r0);
	assert(fd >= 0);
	
	p = r0;
	while(p < r1) {
		t = buf;
		while(p <r1 && t < buf + BUFFERSIZE) {
			t += runetochar(t, p++);
		}
		nwrite = t-buf;
		for (t = buf; nwrite > 0; nwrite -= nwritten, t += nwritten) {
			nwritten = write(fd, t, nwrite);
			if (nwritten <= 0)
				return -1;
		}
	}
	return 0;
}

void
gaptranslate (Range r, Range gap, Range *before, Range *after) {
	Range NULLRANGE = {0,0};
	
	/* Before the gap */
	*before = (r.p0 < gap.p0) ? 
			range(r.p0, MIN(r.p1, gap.p0)) : 
			NULLRANGE;
	/* After the gap */
	*after = (r.p1 > gap.p0) ? 
			range (MAX(gap.p1, r.p0 +  RLEN(gap)), r.p1 + RLEN(gap)) :
			NULLRANGE;
}

/* Write section 'r' of 't' to 'fd'.  Return 0 for success */
int
text_write_range(Text*t, Range r, int fd) {
	Range	b, a;	/* before, after the gap */
	
	gaptranslate(r, t->gap, &b, &a);
	return utfwrite(t->text + b.p0, t->text + b.p1, fd) ||
		utfwrite(t->text + a.p0, t->text + a.p1, fd);
}

/* Fill 'buf' with (legal) range 'logical'. */
void
text_copy(Text *t, Range r, Rune *buf)
{
	Range	b,a;	/* before and after the gap */
	ulong	bsize;
	
	assert(text_invariants(t));
	assert((r.p1 >= r.p0) && (r.p1 <= t->length));
	
	gaptranslate(r, t->gap, &b, &a);
	bsize = RLEN(b);
	memcpy(buf, t->text +b.p0, bsize*sizeof(Rune));
	buf += bsize;
	memcpy(buf, t->text + a.p0, RLEN(a)*sizeof(Rune));
}

Text *
text_alloc(Data *d, Bool isbody)
{
	Text	*t;

	t = NEW(Text);
	t->alloced = 0;
	t->text = 0;
	t->length = 0;
	t->pos = 0;
	
	t->gap = nr;
	t->data = d;
	t->isbody = isbody;
	t->v = 0;		/* to be assigned later */
	t->did = t->undone = t->mark = 0;
	t->undoing = NoUndo;
	t->needsbackup = false;
	return t;
}

/* Replace data in range 'r' of 't' with 's'.  Update displays.
 * Return the range of the inserted text.
 */
Range
text_replace(Text *t, Range r, Rstring s)
{
	ulong	rlen = RLEN(r);
	ulong	rslen = RSLEN(s);
	int		delta = rslen - rlen;

	assert(ROK(r) && RSOK(s) && r.p1 <= t->length);
	assert(text_invariants(t));
	assert(!findnull(s.r0, s.r1));

	if(!(RLEN(r) || RSLEN(s)))
		return r;

	undo_record(t, r, s);
	if(t->gap.p0 != r.p0 || RLEN(t->gap) < delta)
		setgap(t, r.p0, delta);
	memcpy(t->text + t->gap.p0, s.r0, rslen*sizeof(Rune));
	t->gap.p1 += rlen;
	t->gap.p0 += rslen;
	t->length += delta;

	/* invalidate cache? */

	viewlist_replace(t->v, r, s);
	

	if(t->data){
		if(TEXT_ISTAG(t))
			tag_modified(t, r.p0);
		else
			data_sendreplace(t->data, r, s);
	}

	r.p1 = r.p0 + rslen;
	assert(text_invariants(t));
	return r;
}

/* Clean up any resources which t uses.
 */
void
text_free(Text *t)
{
	undo_free(t);
	if(t->text)
		free(t->text);
}

/************* static functions *********************/

static void
movegap(Text *t, ulong p)
{
	assert(text_invariants(t));
	/* move the gap to 'p' */
	if (p < t->gap.p0) {
		memmove(t->text + p + RLEN(t->gap), 
				t->text + p,
				(t->gap.p0 - p)*sizeof(Rune));
		t->gap.p1 -= t->gap.p0 - p;
		t->gap.p0 = p;
	} else if (p > t->gap.p0) {
		memmove (t->text + t->gap.p0,
				t->text + t->gap.p1,
				(p - t->gap.p0)*sizeof(Rune));
		t->gap.p1 = p + RLEN(t->gap);
		t->gap.p0 = p;
	}
	assert(text_invariants(t));
}

/* Move the gap for 't' to 'p', ensure we can grow by at least 'n' */
static void
setgap(Text *t, ulong p, int n)
{
	int	extra;

	assert(text_invariants(t));
	movegap(t,p);
	assert(text_invariants(t));

	extra = n - RLEN(t->gap);
	if (extra <= 0)
		return;

	/* Expand the gap */
	extra += GAPSIZE;
	t->alloced += extra;
	t->text = srealloc(t->text, t->alloced * sizeof(Rune) );

	/* Add this to the gap */
	memmove (  t->text + t->gap.p1 + extra,
			t->text + t->gap.p1,
			(t->alloced - extra - t->gap.p1)*sizeof(Rune) );
	t->gap.p1 += extra;
	assert(text_invariants(t));
}

