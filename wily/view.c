/*******************************************
 *	View methods
 *******************************************/

#include "wily.h"
#include "view.h"
#include <ctype.h>

static Rectangle nullrect = { {0,0}, {0,0}};

/* Store string representation of 'dot' for 'v' into 'buf'.
 * if 'isLine', we want a line address, otherwise we want
 * a character address.
 */
void
view_getdot(View *v, char*buf, Bool isLine) {
	if(isLine) {
		sprintf(buf, " :%d,.", text_linenumber(v->t, v->sel.p0));
	} else {
		/* character address */
		sprintf(buf, " :#%lu,.", v->sel.p0);
	}
}

Range
view_expand(View *v, Range r, char *s) {
	if(RLEN(r))
		return r;
	if (RLEN(v->sel) && v->sel.p0 <= r.p0 && v->sel.p1 >= r.p1)
		return v->sel;
	else
		return text_expand(v->t, r, s);
}

/*****************************************
 	Allocate, deallocate, invariants 
 *****************************************/

/* Allocate and return a new View.  Doesn't attempt to draw it. */
View*
view_new(Font *f, Bool istag, Text *text, Tile *tile)
{
	View*v = NEW(View);
	Bitmap*b = 0;
	ulong 	length;
	ulong	sel;

	length = text_length(text);
	v->r = nullrect;
	frinit(&v->f, nullrect, f, b);
	v->visible = range(0,0);
	sel = istag? length : 0;
	v->sel = range(sel, sel);
	v->anchor = length;
	v->t = text;
	v->next = 0;
	v->selecting = false;
	v->autoindent = autoindent_enabled;
	text_addview(text, v);
	v->tile = tile;
	v->scroll = istag? 0 : scroll_alloc(b, nullrect);
	assert(view_invariants(v));
	return v;
}

/* Delete v and free its resources.  Return 0 for success */
int
view_delete(View *v){
	if (text_rmview(v->t, v))
		return -1;
	if(v==last_selection)
		view_setlastselection(0);
	frclear(&v->f);
	if(v->scroll)
		free(v->scroll);
	return 0;
}

Bool
view_invariants(View*v)
{
	Range	r;
	ulong	length;

	if(!v->f.b)
		return true;	/* all bets are off if we're not visible */

	length = text_length(v->t);
	/* The selection and visible region are sane */
	assert(ROK(v->sel));
	assert(ROK(v->visible));
	assert(v->sel.p1 <= length);
	assert(v->visible.p1 <= length);

	/* We're either displaying at least one line, or we're quite hidden */
	assert(RLEN(v->visible)==v->f.nchars);
	assert(v->f.nchars <= length);
	
	/* View height == integral number of lines
	assert(Dy(v->r) == 2*INSET + v->f.maxlines * v->f.font->height);
	 */

	if(!v->selecting) {
		/* The visible part of the View selection == the frame selection */
		r = rclip(v->sel, v->visible);
		assert( (r.p0 - v->visible.p0) == v->f.p0);
		assert( (r.p1 - v->visible.p0) == v->f.p1);
	}
	return true;
}

/*****************************************
 	Simple data hiding stuff
 *****************************************/

/* Return the 'Data' associated with 'v', or 0. */
Data*
view_data(View *v)
{
	return v ? text_data(v->t):  0;
}

/* Body associated with 'v' */
View*
view_body(View*v) {
	if(!v)
		return 0;
	return ISBODY(v) ? v : tile_body(v->tile);
}

/* The window 'v' is a part of, or 0. */
Tile*
view_win(View*v) {
	return v? tile_win(v->tile)  : 0;
}

/* The tile 'v' is part of, or 0. */
Tile*
view_tile(View*v) {
	return v? v->tile : 0;
}

Text*
view_text(View*v) {
	return v? v->t : 0;
}

Range
view_getsel(View*v) {
	return v->sel;
}

int
view_height(View*v) {
	return (v && v->f.b) ? Dy(v->r) : 0;
}

void
view_paste(View*v){
	undo_break(v->t);
	view_select(v, paste(v->t, v->sel));
	view_show(v, v->sel);
}

/*
 * Copy the range to the snarf buffer (unless it's empty).
 * Delete the text, update the View
 */
void
view_cut(View*v, Range r) {
	if(RLEN(r)){
		snarf(v->t, r);
		undo_break(v->t);
		text_replace(v->t, r, rstring(0,0));
	}
	view_show(v, range(r.p0, r.p0));
}

void
view_append(View *v, char *s, int n) {
	Text	*t;
	ulong	len;
	Range	end;
	
	t = v->t;
	len = text_length(t);
	end = range(len,len);
	s[n] = 0;
	
	text_replaceutf(v->t, end, s);
}

/* Append 'n' bytes at 's' to 'v'.  If 'first' time, we replace
 * the selection, otherwise we append to the selection
 */
void
view_pipe(View *v, Bool *first, char *s, int n)
{
	ulong	p0 = v->sel.p0;
	Range	r;

	if (*first) {
		r = v->sel;
		*first = false;
	} else {
		r = range(v->sel.p1, v->sel.p1);
	}

	s[n]=0;	/* bug - assuming no nulls in 's' */
	text_replaceutf(v->t,  r,  s);
	r = range(p0, v->sel.p1);

	view_select(v, r);
}

/*
 * Change v's font.  If 'arg' is not null, use it, otherwise
 * toggle between Fonts 'fixed' and 'font'
 */
void
view_setfont(View *v, char*arg)
{
	frfont(&v->f, (v->f.font==font)? fixed: font);
	if(ISVISIBLE(v))
		view_reshaped(v, v->r);
}

/*
 * Perform the actual drawing of the border that indicates the
 * view is the last_selection.
 */

void
view_border(View *v, Bool set)
{
	Rectangle r;
	
	assert(v);
	r = v->r;
	r.min.x += SCROLLWIDTH + 4;
	
	if (tile_hidden(view_tile(v)))
		return;
	if (set)
		border(&screen, r, SELECTEDBORDER, F);
	else {
		border(&screen, r, SELECTEDBORDER, 0);
		border(&screen, v->r, 1, F);
	}
}

/* Indicate visually that 'v' is the 'last_selection' */
void
view_setlastselection(View *v)
{
	if (v == last_selection)
		return;

	if (last_selection)
		view_border(last_selection, false);
	if (v)
		view_border(v, true);
	last_selection = v;
}

/* Set the selection in 'v' to 'r'.  Indicate on screen if necessary */
void
view_select(View*v, Range r)
{
	assert(ROK(r));
	assert(view_invariants(v));

	frselectp(&v->f, F&~D);
	v->sel = r;
	r = rclip(v->sel, v->visible);
	v->f.p0 = r.p0 - v->visible.p0;
	v->f.p1 = r.p1 - v->visible.p0;
	frselectp(&v->f, F&~D);

	assert(view_invariants(v));
}

/* Warp the cursor to selection 'r' in 'v'.
 * PRE: 'v' is visible.  Some of 'r' is visible.
 */
void
view_warp (View *v, Range r)
{
	Point pt;

	assert(view_invariants(v));
	assert(ROK(r));
	assert(RINTERSECT(r, v->visible));
	assert(r.p1 <= text_length(v->t));
	assert(ISVISIBLE(v));
	
	pt = frptofchar(&v->f, r.p0 - v->visible.p0);
	pt.y += v->f.font->height/2;	/* middle of char */
	cursorset(pt);
}

/*
 * Replace Range 'r' with 's' in 'v'.  We've already changed the underlying
 * text buffer, we now just have to change the display, and update
 * v->visible and v->sel.
 */
static void
view_replace(View *v, Range r, Rstring s)
{
	Range	q = intersect(r, v->visible);	/* visible part of change */
	int	len;
	Bool	visible = ISVISIBLE(v);

	/* view_invariants don't hold at present because
	 * v->t->length has changed but v->sel hasn't
	 */
	assert(ROK(r));
	assert (RSOK(s));
	assert(RLEN(r) || RSLEN(s));	/* or we wouldn't get here */
	assert(r.p0 <= text_length(v->t));	/* even if we're deleting text, this will hold */

	if(visible && q.p1 >= q.p0) {	
		/* some of the replaced text is visible */
		Frame		*f = &v->f;

		q.p0 -= v->visible.p0;
		q.p1 -= v->visible.p0;
		
		if(RLEN(q))
			frdelete(f, q.p0 , q.p1);
		if(RSLEN(s))
			frinsert(f, s.r0,  s.r1, q.p0);
	}

	/* adjust our counters */
	len = RSLEN(s);
	v->visible.p0 = ladjust(v->visible.p0, r,len);
	v->visible.p1 = radjust(v->visible.p1, r,len);
	v->sel.p0 = 	radjust(v->sel.p0, r,len);
	v->sel.p1 =	radjust(v->sel.p1, r,len);
	v->anchor= 	ladjust(v->anchor, r,len);

	if(visible)
		fill(v);
	assert(view_invariants(v));
}

/* Refresh v because its text has changed arbitrarily. */
static void
view_refresh(View*v)
{
	if(ISVISIBLE(v)) {
		v->sel = range(v->visible.p0, v->visible.p0);
		frdelete(&v->f, 0, v->f.nchars);
		fill(v);
	}
}

void
viewlist_refresh(View*v) {
	for(;v; v = v->next)
		view_refresh(v);
}

void
viewlist_replace(View*v, Range r, Rstring s) {
	for(;v; v = v->next)
		view_replace(v, r, s);
}
