/*******************************************
 *	view size and reshaping
 *******************************************/

#include "wily.h"
#include "view.h"

static Rectangle	snap(View*v, Rectangle r);
static Rectangle	resizebox(Rectangle r);
static void		rfill(Rectangle r, Fcode f);
static void		button_set(View *v);
static void		setrects(View*v, Rectangle r);

void
view_fillbutton(View*v, Fcode f){
	rfill(resizebox(v->r), f);
}

/*
PRE: v->visible.p0 is correct.  The text which is currently
displayed in the frame is correct, but there might not be
enough.

POST:  The frame is displaying everything it should,
and v->visible.p1 is set.
*/
void
fill(View *v) {
	Frame	*f = &v->f;
	ulong	p; 	/* text position of last visible rune */
	Rune	buf[FILLCHUNK];
	int		n;
	Text		*t = v->t;

	/* view_invariants may not hold at this point */

	if(!f->b)
		return;

	/* Add runes until we exhaust the text or fill the frame */
	p = v->visible.p0 + f->nchars;
	while (!frame_isfull(f) && (n = text_ncopy(t, buf, p, FILLCHUNK)) ) {
		frinsert(f, buf, buf+n, f->nchars);
		p += n;
	}
	
	v->visible.p1 = v->visible.p0 + f->nchars;

	if(v->scroll)
		scroll_set(v->scroll,  v->visible.p0,
				f->nchars, text_length(v->t)
				);
	else
		button_set(v);

	/* Ensure that if v->sel is in the visible area, it is selected */
	frselectp(f, F&~D);
	f->p0 = clip(v->sel.p0 - v->visible.p0, 0, f->nchars);
	f->p1 = clip(v->sel.p1 - v->visible.p0, 0, f->nchars);
	frselectp(f, F&~D);

	assert(view_invariants(v));
}

int
snapheight(View*v, int h) {
	int	lines;
	int	fh = v->f.font->height;
	int	brdr = 2*INSET;
	
	if (v->scroll) {
		lines = (h - brdr) / fh;
		if (lines == 0)
			return 0;
		else
			return h;
	} else
		return brdr + fh;
}

/* Try to redraw 'v' inside 'r' */
void
view_reshaped(View*v, Rectangle r) {
	Frame	*f;
	
	assert(view_invariants(v));

	r = snap(v,r);
	setrects(v, r);
	if (ISVISIBLE(v)) {
		if(text_refreshdir(v->t)){
			v->visible.p0 = v->sel.p0 = v->sel.p1 = 0;
			frdelete(&v->f, 0, v->f.nchars);
		}
		fill(v);

		/* Clean up any trailing junk. */
		f = &v->f;

		/* Last line. */
		r.max = r.min = frptofchar(f, f->nchars + 1);
		r.max.x = f->r.max.x;
		r.max.y += f->font->height;
		if (r.min.x != r.max.x)
			cls(r);

		/* Below last line. */
		r.min.x = f->r.min.x;
		r.min.y = r.max.y;
		r.max.y = v->r.max.y; 
		if (r.min.y != r.max.y)
			cls(r);

		view_border(v, v == last_selection);
	}
}

/* Return point just after the last line in 'v' */
int
view_lastlinepos(View*v) {
	Frame	*f = &v->f;
	Point	p = frptofchar(f, f->nchars);
	int	y =  p.y + f->font->height;

	return MIN(y, f->r.max.y);
}

/* Return the amount by which 'v' could be squeezed */
int
view_stripwhitespace(View*v) {
	Frame	*f;
	int		blanklines;

	if(v && ISVISIBLE(v)) {
		f = &v->f;
		assert(Dy(v->r) >= f->maxlines * f->font->height);
		blanklines = f->maxlines - f->nlines;
		if(blanklines > 0) {
			return blanklines * f->font->height;
		}
	}
	return 0;
}

static Rectangle
snap(View*v, Rectangle r) {
	r.max.y = r.min.y + snapheight(v, Dy(r));
	return  r;
}

/* Fill 'r' according to 'f' */
static void
rfill(Rectangle r, Fcode f) {
	r = inset(r,2);
	bitblt(&screen, r.min, &screen, r, f);
}

static Rectangle
resizebox(Rectangle r) {
	r = inset(r, INSET);
	r.max.x = r.min.x + SCROLLWIDTH;
	return r;
}

static void
button_set(View *v) {
	Rectangle	r;

	assert(ISTAG(v));	/* we're a tag */
	r = resizebox(v->r);
	border(&screen, r, 1, F);
	if(data_isdirty(view_data(tile_body(v->tile))))
		rfill(r, F);
}

/* Set the rectangles for 'v', v's frame and v's scrollbar.
 * Assumes that 'r' is already correct.
 * If we can be displayed, set up the frame and scrollbar.
 */
static void
setrects(View*v, Rectangle r) {
	Frame	*f = &v->f;
	Font		*ft = f->font;
	Rectangle	scrollr, framer;
	Bitmap	*b;

	assert(Dx(r) >= MINWIDTH);	/* Or our tile is bizarre */

	scrollr = inset(r, INSET);
	scrollr.max.x = scrollr.min.x + SCROLLWIDTH;

	framer = inset(r, INSET);
	framer.min.x += SCROLLWIDTH;
	framer.min.x += INSET;

	/* If 'r' is too small, we're hidden: use null bitmap */ 
	b = Dy(r) < tagheight ? 0 : &screen;

	v->r = r;
	scroll_setrects(v->scroll, b, scrollr);
	frclear(f);
	frinit(f, framer, ft, b);
}

