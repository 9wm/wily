/*******************************************
 *	Maintain Undo chains, Undo methods
 *******************************************/

#include "wily.h"
#include "text.h"

/* We currently manage our Undo chain as a linked list,
 * and separately alloc small buffers every time.
 * If we cared about efficiency, we'd make all the u->s
 * entries share bigger chunks of Runes.
 *
 * For now, it doesn't seem to be a problem, and simpler
 * is better.
 */
 
struct Undo {
	Undo	*next;
	Range	r;			/* where to replace */
	Rstring	s;			/* what to replace with */
	ulong	alloced;		/* bytes alloced at s.p0 */
};

static Bool undoing = false;	/* prevent recording undo ops */
static Range illegal_range = {10,1};

static Undo *	reverse	(Text*, Range, Rstring);
static Bool 	append	(Text*, Range r, Rstring s);
static void 	reset	(Undo**);
static void	save_state	(Text *);
static void	update_state	(Text *);
static Range	shift	(Text *, Undo **, Undo **, Bool );

static void
text_rmtool(Text*t, char*s)	{
	if(t->data)
		tag_rmtool(data_tag(t->data), s);
}

static void
text_addtool(Text*t, char*s)	{
	tag_addtool(data_tag(t->data), s);
}

/* Record information allowing us to undo the replacement of
 * 'r' in 't' with 's'.  This replacement hasn't happened yet.
 */
void
undo_record
(Text *t, Range r, Rstring s)
{
	Undo	*u;

	assert(ROK(r));
	assert(RSOK(s));
	assert(r.p1 <= t->length);

	if(undoing || t->undoing == NoUndo)
		return;

	save_state(t);

	if(!append(t, r, s)) {
		/* remove old t->undone trail */
		reset(&t->undone);

		u = reverse(t, r, s);
		u->next = t->did;
		t->did = u;
	}
	t->undoing = MoreUndo;

	update_state(t);
}

/* Undo the top operation in the undo stack, and move it to the redo
 * stack.  Keep going to mark if appropriate.
 * Return the range that was inserted.
 */
Range
undo_undo(Text *t, Bool all)
{
	Range r;
	
	if (!t->did)
		return illegal_range;

	save_state(t);
	
	do {
		r = shift(t, &t->did, &t->undone, true);
	} while (all && t->did && t->did != t->mark);
	
	update_state(t);
	return (all ? illegal_range : r);
}

/* The mirror of undo_undo */
Range
undo_redo(Text *t, Bool all)
{
	Range r;
	Undo *tmp;
	
	save_state(t);
	
	tmp = t->did;
	t->did = t->undone;
	t->undone = tmp;
	
	r = undo_undo(t, all);
	
	tmp = t->did;
	t->did = t->undone;
	t->undone = tmp;
	
	update_state(t);
	return r;
}

/* Throw away all the undo information. */
void
undo_free(Text*t) {
	reset(&t->did);
	reset(&t->undone);
}

/* Throw away all the undo information, leave in NoUndo state. */
void
undo_reset(Text*t)
{
	undo_free(t);
	t->undoing = NoUndo;
	text_rmtool(t, "Put");
	text_rmtool(t, "Undo");
	text_rmtool(t, "Redo");
}

/* Start undoing on this Text. */
void
undo_start(Text*t)
{
	t->undoing = StartUndo;
}

/*
 * Make sure we don't merge whatever's currently on top of the undo
 * stack with the next operation.
 */
void
undo_break(Text*t)
{
	if(t->undoing==MoreUndo)
		t->undoing = StartUndo;
}

/* Remember this point in the Undo history
 * for future reference.
 */
void
undo_mark(Text*t)
{
	t->mark = t->did;
	t->undoing = StartUndo;
}

/* Are we at the same point in the history as we marked earlier?
 */
Bool
undo_atmark(Text*t)
{
	return t->mark == t->did;
}

/*********************************************************
	INTERNAL STUFF
*********************************************************/
static void	tag		(Text *, Bool , Bool , char *);
static Bool	undo_eq(Undo*u, Range r, Rstring s);

/* 
 * We are about to replace the text at 'r' with 's'.  If we can record
 * the undo information for this by modifying t->undo, and/or t->undone,
 * do so, and return true.  Otherwise, return false.
 */
static Bool
append(Text *t, Range r, Rstring s)
{
	Undo 	*u;
	Rune	*buf;
	int		rlen, slen;

	if (undo_eq(t->did, r, s)) {
		/*
		 * What we're about to do is exactly what would happen by
		 * an Undo operation.  Therefore, we just move the front of
		 * the undo queue to the front of the redo queue.
		 */
		save_state(t);
		shift(t, &t->did, &t->undone, false);
		update_state(t);
		
		return true;
	}
	if (!(t->did && t->undoing==MoreUndo && t->did != t->mark))
		return false;

	u = t->did;

	if(u->r.p1 == r.p0 && RLEN(r)==0 && RSLEN(u->s)==0) {
		u->r.p1 += RSLEN(s);
		return true;
	}
	if (RLEN(u->r)==0 && RSLEN(s)==0 && u->r.p0 == r.p1) {
		slen = RSLEN(u->s);
		rlen = RLEN(r); 
		buf = salloc ( (slen + rlen) * sizeof(Rune) );
		text_copy( t, r, buf);
		memcpy(buf + rlen, u->s.r0, slen*sizeof(Rune));
		free(u->s.r0);
		u->s.r0 = buf;
		u->s.r1 = buf + slen + rlen;
		u->r.p0 = u->r.p1 = r.p0;
		return true;
	}
	return false;
}

/* Free all the nodes in the list, set the head of the list to 0.*/

static void
reset(Undo**head) {
	Undo	*u, *next;

	for(u = *head; u; u = next){
		next = u->next;
		free(u->s.r0);
		free(u);
	}
	*head = 0;
}

/*
 * We use save_state() and update_state() to compare our undo state
 * before and after operations.  These two functions use 'did' and
 * 'undone'
 */
static Undo	*did, *undone;
static Bool	state_count = 0;
static void
save_state(Text *t)
{
	if(state_count++)
		return;
	did = t->did;
	undone = t->undone;
}
static void
update_state(Text *t)
{
	if(--state_count)
		return;
	if(t->needsbackup) {
		tag(t, did!=0, t->did!=0, "Undo");
		tag(t, undone!=0, t->undone!=0, "Redo");
		tag(t, t->did == t->mark, did==t->mark, "Put");
	}
}

/*
 * If 'change_text', do the replacement at 'from'.
 *
 * Reverse the top of 'from', add it to 'to'.
 *
 * Returns the range of any text inserted, or gibberish if !change_text
 */
static Range
shift(Text *t, Undo **from, Undo **to, Bool change_text)
{
	Range	r;
	Undo	*u;
	Undo	*rev;

	u = *from;
	assert(u);
	*from = u->next;

	rev = reverse(t, u->r, u->s);

	if(change_text){
		undoing=true;
		r = text_replace(t, u->r, u->s);
		undoing = false;
	}

	free(u->s.r0);
	free(u);

	rev->next = *to;
	*to = rev;

	return r;
}

/*If the state has changed to (true/false), (add/remove) 's'
 * in all the tags associated with 't'
 */
static void
tag(Text *t, Bool before, Bool after, char *s)
{
	if(!before != !after)
		(after? text_addtool : text_rmtool)(t,s);
}

/*
 * Compare two rune strings.  Return a number >0, <0 or 0 to indicate if
 * s1 is lexically after, before or equal to s2
 */
int
rstrcmp(Rstring s1, Rstring s2)
{
	Rune	*p1, *p2;
	int		diff;

	for (	p1=s1.r0, p2 = s2.r0;  p1<s1.r1 && p2 < s2.r1; p1++, p2++)
		if( (diff = *p1 - *p2) )
			return diff;
	return RSLEN(s1) - RSLEN(s2);
}

static Bool
undo_eq(Undo*u, Range r, Rstring s)
{
	return u && 
		r.p0 == u->r.p0 && r.p1 == u->r.p1 &&
		 !rstrcmp(u->s, s);
}

/* Return undo entry to undo the replacement of 'r' in 't' with 's'
 */
static Undo *
reverse(Text*t, Range r, Rstring s)
{
	Undo	*u;

	assert(ROK(r));
	assert(RSOK(s));
	u = NEW(Undo);
	u->r.p0 = r.p0;
	u->r.p1 = r.p0 + RSLEN(s);
	u->alloced = RLEN(r);
	u->s.r0 = (Rune*)salloc(u->alloced*sizeof(Rune));
	text_copy(t, r, u->s.r0);
	u->s.r1 = u->s.r0 + RLEN(r);
	return u;
}
