/*******************************************
 *	view scrolling
 *******************************************/

#include "wily.h"
#include "view.h"

void
view_linesdown(View *v, int n, Bool down) {
	Mouse	m;
	Rectangle	r;

	if(! (v = tile_body(view_win(v))) )
		return;

	r = v->r;
	m.xy.x = r.min.x;
	m.xy.y = r.min.y + v->f.font->height * n;
	m.buttons = down? RIGHT : LEFT;
	view_scroll(v, &m);
}

void
view_pagedown(View *v, Bool down) {
	Mouse	m;
	Rectangle	r;

	if(! (v = tile_body(view_win(v))) )
		return;

	r = v->r;
	m.xy.x = r.min.x;
	m.xy.y = (r.min.y + r.max.y) /2;
	m.buttons = down? RIGHT : LEFT;
	view_scroll(v, &m);
}

/* Make 'n' the first rune displayed in 'v' */
static void
view_set(View*v, ulong n) {
	ulong	ndelete;

	assert(view_invariants(v));
	assert(v->scroll);
	assert(n<= text_length(v->t));

	if(n == v->visible.p0)
		return;

	ndelete = RCONTAINS(v->visible, n) ? 
			n - v->visible.p0 : 
			v->f.nchars;
	v->visible.p0 = n;
	if (ISVISIBLE(v)) {
		frdelete(&v->f, 0, ndelete);
		fill(v);
	}

	assert(view_invariants(v));
}

static Bool
tag_isfull(Frame*f) {
	Point	pt = frptofchar(f, f->nchars);
	Point	max = f->r.max;
	return max.x - pt.x < 30;
}

/* Try to get a tag to show as much as possible of 'r' */
static void
tag_show(View*v, Range r) {
	Frame	*f = &v->f;

	assert(ISTAG(v));
	assert(r.p1 <= text_length(v->t));

	/* If we're seeing all of r,
	 *  or if all that we're seeing is in r, we're happy
	 */
	if(RISSUBSET(r, v->visible) || RISSUBSET( v->visible,r))
		goto done;

	/* Otherwise, it must be the case that EITHER
	 * there's some of 'r' invisible off to the right or the left
	 */
	assert(r.p1 > v->visible.p1 || r.p0 < v->visible.p0);

	if (r.p1 > v->visible.p1) {
		/* remove stuff until we can see r.p1 */
		while(r.p1 > v->visible.p1) {
			frdelete(f, 0, f->nchars);
			v->visible.p0 += 5;
			fill(v);
		}

		assert(r.p1 <= v->visible.p1);
	} else {
		/* add stuff until we can see r.p0 */
		frdelete(f, 0, f->nchars);
		v->visible.p0 = r.p0;
		fill(v);

		assert(r.p0 >= v->visible.p0);
	}

done:
	while (!tag_isfull(f) && v->visible.p0) {
		frdelete(f, 0, f->nchars);
		v->visible.p0--;
		fill(v);
	}
}

/* Make sure 'v' is showing at least some of 'r' */
void
view_show(View *v, Range r) {
	assert(view_invariants(v));
	assert(ROK(r));
	assert(r.p1 <= text_length(v->t));

	if (v->scroll) {
		tile_show(v->tile);	
	}
	assert(ISVISIBLE(v));
	if(!v->scroll) {
		tag_show(v,r);
		return;
	}

	if(!RISSUBSET(r, v->visible)) {
		ulong	dest;

		/* try to position range in the middle of the screen */
		dest = text_nl(v->t, r.p0, -((int)v->f.maxlines)/2);
		view_set(v, dest);

		if (!RINTERSECT(r, v->visible)) {
			/* lines are too long, quit being fancy, start with line contain 'r' */
			dest = text_startOfLine(v->t, r.p0);
			view_set(v,dest);
		}
		if (!RINTERSECT(r, v->visible)) {
			/* goddamm!  we can't even fit a whole line.  no more mr nice guy */
			view_set(v,r.p0);
			/* Now _that_ oughta do it! */
		}
	}

	assert(RINTERSECT(r, v->visible));
	assert(view_invariants(v));
	assert(ISVISIBLE(v));
}

/* Handle a mouse-click in 'v's scrollbar */
void
view_scroll(View *v, Mouse *m) {
	long long	y;
	ulong n,base = v->visible.p0;
	int	lineheight = v->f.font->height;
	ulong	runepos;

	assert(m->buttons);
	
	if (m->xy.y < v->f.r.min.y)
		m->xy.y = v->f.r.min.y;
	y = clip(m->xy.y - v->f.r.min.y, 0, Dy(v->f.r));
	if (y < v->f.font->height && m->buttons != MIDDLE){
		y = lineheight;
		m->xy.y += lineheight;
	}

	runepos = frcharofpt(&v->f, m->xy);
	/*
	 * With left and middle buttons, we try to scroll back such that
	 * we start at the start of a line.  Sometimes that isn't possible,
	 * if lines are too big, so we jump again to some fallback position.
	 * TODO:  work this stuff out offscreen.
	 */
	switch(m->buttons){
	case RIGHT:
		view_set(v, base + runepos);
		break;
	case LEFT:
		n = back_height(v->t, v->visible.p0,  v->f.font, Dx(v->f.r), m->xy.y - v->f.r.min.y);
		view_set(v, n);
		break;
	default:
		runepos = (text_length(v->t) * y) / Dy(v->f.r);
		n = text_startOfLine(v->t, runepos);
		view_set(v, n);
		if (runepos > v->visible.p1) {
			view_set(v, runepos);
		}
	}
}

/* Scroll 'v' to the left or right a bit, if possible */
void
view_hscroll(View*v, Bool left) {
	ulong	old = v->visible.p0;
	assert(!v->scroll);

	assert(ISTAG(v));
	
	if(left){
		if(v->visible.p0) 
			v->visible.p0--;
	} else {
		if(v->visible.p1 < text_length(v->t))
			v->visible.p0++;
	}

	if (old != v->visible.p0){
		frdelete(&v->f, 0, v->f.nchars);
		fill(v);
	}
}

