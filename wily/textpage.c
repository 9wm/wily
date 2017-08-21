enum {
	PAGESIZE = BUFSIZE;
}

struct TextPage {
	TextPage	*next, *prev;
	Rune	*buf;	/* Points to PAGESIZE bytes */
	Range	gap;
	int		nrunes;	/* active bytes in this page */
	ulong	base;	
	Bool		isLineCountValid;
	int		LineCount;
}


/* Read the contents of 't' from 'fd', which should have 'len' bytes.
 * Return 0 for success, -1 for failure
 */
int
text_read(Text *t, int fd, int len) {
	
	int	desired, nread;
	char	buf[RUNES_PER_PAGE];
	extern int utftotext_unconverted;
	int	offset;
	TextPage	*page;
	
	text_clear(t);
	while(len > 0) {
		desired = MIN(len, sizeof(buf) - leftover);
		nread = read(fd, buf + leftover, desired);
		if(nread<=0)
			return -1;
		len -= nread;
		total = leftover + nread;
		page = text_appendPage(t, buf, total, &leftover);
		if (leftover ) {
			memcpy(buf, buf + nconverted, leftover);
		}
	}
	if (leftover) {
		warn("incomplete runes");
	}
}

text_appendPage(Text*t, char*buf, int total, int*leftover) {
	Page* p = newPage();
	
	/* Make sure we can fit all these bytes on one page */
	assert(total <= RUNES_PER_PAGE);
	
	p->nrunes = utftotext(p->buf, buf, total);
	*leftover = utftotext_unconverted;
	p->gap.p0 = p->nrunes;
	p->gap.p1 = RUNES_PER_PAGE;
	
	if(t->tail) {
		t->tail->next = p;
		p->prev = t->tail;
	} else {
		t->tail = t->head = p;
	}
}

Page*
newPage() {
	Page*p = oalloc(sizeof(Page));
	p->buf = pagealloc();
	p->gap = Range(
	return p;
}

int
text_write_range(Text*t, Range r, int fd) {
	Page *p;
	
	for (p = firstInRange(t,r); p && page_contains(p,r); p= p->next)
		page_writeRange(p,r,fd);
	}
}

void
text_copy(Text*t, Range r, Rune*buf) {
	Page	*p;
	
	for (p = firstInRange(t,r); p && page_contains(p,r); p= p->next)
		buf = page_copy(p, r, buf);
}

/* Replace data in range 'r' of 't' with 's'.  Update displays.
 * Return the range of the inserted text.
 */
Range
text_replace(Text *t, Range r, Rstring s){
	Page*p;
	
	for(p = t->head; p; p = p->next)
		if (p->base + p->len > r.p0)
			break;
	if(!p) {
	}
	
	if(p->len + RSLEN(s) - RLEN(r) <= RUNES_PER_PAGE) {
		
	}
	
	if(RLEN(r))
		text_delete(t,r);
	text_updatePages(t);
	if(RSLEN(s))
		text_insert(t, r.p0, s);
	text_updatePages(t);
}

void
text_updatePages(Text*t) {
	Page*p;
	ulong base = 0;
	for(p = t->head; p; p=p->next){
		p->base = base;
		base += p->nrunes;
	}
}

void
text_delete(Text*t, Range r) {
	Page	*p, *next;
	
	p = firstInRange(t,r);
	while(p && page_contains(p,r)){
		if (page_containedIn(p,r)) {
			next = p->next;
			text_rmpage(t,p);
			p = next;
		} else {
			page_delete(p,r);
			p = p->next;
		}
	}
}

void
text_insert(Text*t, ulong p, Rstring s){
}

/** Is 'p' contained completely in 'r'? **/
Bool
page_containedIn(Page*p, Range r) {
	
}

/** Completely remove 'p' from 't' **/
void
text_rmpage(Text*t, Page *p) {
}

/** Delete 'r' from 'p' **/
void
page_delete(Page*p, Range r) {
	/* move and extend p's gap */
	
}

/** Returns first Page in 't' which contains 'r', (or returns 0) **/
Page*
firstInRange(Text*t, Range r){
	Page *p;
	
	for(p = t->head; p ; p = p->next)
		if (page_contains(p,r))
			break;
	return p;
}

Boolean
page_contains(Page*p, Range r) {
	 ulong	start,end;

	 start = p->base;
	 end = p->base + nrunes;
	 
	 /* contains r.p0 or contains r.p1 */
	 return !(r.p0>=end || r.p1 <start);
}

Page*
page_writeRange(Page*p, Range r, int fd) {
	Rstring	before, after;
	
	gapTranslate(p, r, &before, &after);
	return utfwrite(before.r0, before.r1, fd) || utfwrite(after.r0, after.r1, fd);
}

Rune*
page_copy(Page*p, Range r, Rune *r) {
	Rstring	before, after;
	int		len;
	
	gapTranslate(p, r, &before, &after);

	len = before.r1 - before.r0;
	memcpy(r, before.r0, len*sizeof(Rune));
	r += len;
	
	len = after.r1 - after.r0;
	memcpy(r, after.r0, len*sizeof(Rune));
	r += len;
	
	return r;
}

