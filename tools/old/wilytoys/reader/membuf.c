/*
 * memory.c - custom memory handler for mail messages.
 * XXX - This handler does *not* return buffers aligned for
 * anything larger than a byte!
 */

#include "headers.h"
#include <assert.h>

#define MINBUF	512		/* limit for splitting */

static membuf *busy, *freed;


static void *find_free(size_t len);

/*
 * allocate a new buffer.
 */

void *mb_alloc(size_t len)
{
	void *ptr;
	membuf *m;

	assert(len);
	if ((ptr = find_free(len)))
		return ptr;
	m = salloc(sizeof(*m));
	m->len = len;
	m->start = salloc(len);
	m->end = m->start + len;
	m->next = busy;
	busy = m;
	return m->start;
}

/*
 * grab an existing buffer from the free list, if there is one
 * large enough.
 */

static void *
find_free(size_t len)
{
	membuf **pm = &freed;
	membuf **smallest = 0;
	membuf *m, *nm;

	for (;*pm; pm = &((*pm)->next))
		if ((*pm)->len >= len && (!smallest || (*pm)->len < (*smallest)->len))
			smallest = pm;
	if (smallest == 0)
		return 0;		/* no buffer big enough */
	m = *smallest;
	/* found one - big enough to split? */
	if (m->len > len+MINBUF) {
		/* yep */
		nm = salloc(sizeof(*nm));
		nm->len = len;
		nm->start = m->start;
		nm->len = len;
		nm->end = (m->start += len);
		m->len -= len;
		nm->next = busy;
		busy = nm;
		return nm->start;
	}
	/* nope - remove this one from the free queue, and return it */
	*smallest = m->next;
	m->next = busy;
	busy = m;
	return m->start;
}

/*
 * A previously allocated buffer contains more than one mail message.
 * break the buffer into two, at the point indicated.
 */

void
mb_split(void *old, void *new)
{
	membuf *m, *nm;
	size_t len;

	assert(old);
	assert(new);
	assert (old < new);
	len = (char *)new - (char *)old;
	for (m = busy; m; m = m->next)
		if (m->start == old)
			break;
	assert(m);
	assert(m->len >= len);
	if (m->len == len)
		return;
	nm = salloc(sizeof(*nm));
	nm->len = m->len - len;
	nm->end = m->end;
	m->end = nm->start = new;
	nm->len = m->len - len;
	m->len = len;
	nm->next = m->next;
	m->next = nm;
}

/*
 * discard the memory buffer, by placing it on the free list.
 */

void
mb_free(void *ptr)
{
	membuf **m;
	membuf *p;

	assert(ptr);
	for (m = &busy; *m; m = &((*m)->next))
		if ((*m)->start == ptr)
			break;
	assert(*m);
	p = *m;
	*m = p->next;		/* remove from the busy list */
	/* search the free list for an adjacent buffer. If we find one,
	combine the two. */
	for (m = &freed; *m; m = &((*m)->next)) {
		if ((*m)->end == p->start) {
			(*m)->end = p->end;
			(*m)->len += p->len;
			free(p);
			return;
		}
		if ((*m)->start == p->end) {
			(*m)->start = p->start;
			(*m)->len += p->len;
			free(p);
			return;
		}
	}
	/* didn't find one - just append the new buffer onto the free list. */
	*m = p;
	p->next = 0;
	return;
}
