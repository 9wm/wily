/*******************************************
 *	Display scroll_bars
 *******************************************/

#include "wily.h"

static Bitmap *dkgrey_o, *dkgrey_e;

struct Scroll {
	Rectangle	r;
	Bitmap	*b;
	ulong	thumb, extent, max;
};

static Rectangle	getthumb(Scroll *, ulong , ulong , ulong );

/* Initialize bitmaps for scrollbars.  Called once, on system entry. */
void
scroll_init(void) {
	int	x, y;
	int dx = 4, dy = 2;

	dkgrey_o = balloc(Rect(0, 0, dx, dy), 0);
	bitblt(dkgrey_o, dkgrey_o->r.min, dkgrey_o, dkgrey_o->r, F);
	for (x = 0; x < dx; x += 2)
		for (y = (x % 4) / 2; y < dy; y += 2)
			point(dkgrey_o, Pt(x, y), 0, Zero);
			
	dkgrey_e = balloc(Rect(0, 0, dx, dy), 0);
	bitblt(dkgrey_e, dkgrey_e->r.min, dkgrey_e, dkgrey_e->r, F);
	for (x = 1; x < dx; x += 2)
		for (y = (x % 4) / 2; y < dy; y += 2)
			point(dkgrey_e, Pt(x, y), 0, Zero);
}

void
scroll_setrects(Scroll*s, Bitmap *b, Rectangle r) {
	if(!s)
		return;
	s->r = r;
	s->b = b;
	s->thumb = s->extent = s->max = 0;
	if(b) {
		bitblt(b, r.min, b, r, Zero);
		border(b, r, 1, F);
	}
	assert(Dx(s->r)<=SCROLLWIDTH);
}

Scroll *
scroll_alloc(Bitmap *b, Rectangle r) {
	Scroll	*s;

	s = NEW(Scroll);
	scroll_setrects(s,b,r);
	return s;
}

void
scroll_set(Scroll *s, ulong thumb, ulong extent, ulong max) {
	Rectangle	q, oldr, newr, above, below;

	oldr = getthumb(s, s->extent, s->max, s->thumb);
	above = below = s->r;
	above.max.y = oldr.min.y;
	below.min.y = oldr.max.y;
	
	newr = getthumb(s, extent, max, thumb);

	/* Sections that before were NOT in the thumb, but now are */
	q = newr;
	if (rectclip(&q, above))
		bitblt(s->b, q.min, s->b, q, Zero);
	q = newr;
	if (rectclip(&q, below))
		bitblt(s->b, q.min, s->b, q, Zero);

	above = below = s->r;
	above.max.y = newr.min.y;
	below.min.y = newr.max.y;

	/* Sections that before WERE in the thumb, but now are NOT */
	q = oldr;
	if (rectclip(&q, above)) {
		if (s->r.min.x % 2 == 0)
			texture(s->b, q, dkgrey_e, S);
		else
			texture(s->b, q, dkgrey_o, S);
	}
	q = oldr;
	if (rectclip(&q, below)) {
		if (s->r.min.x % 2 == 0)
			texture(s->b, q, dkgrey_e, S);
		else
			texture(s->b, q, dkgrey_o, S);
	}
	s->thumb = thumb;
	s->extent = extent;
	s->max = max;
}

static ulong
div_down(unsigned long long p, unsigned long long q) {
	return p / q;	
}

static ulong
div_up(ulong p, ulong q) {
	return (p + q - 1) / q;
}

static Rectangle
getthumb(Scroll *s, ulong extent, ulong max, ulong thumb)
{
	Rectangle	r;
	unsigned long long length;

	r = inset(s->r, 1);
	assert (Dx(s->r)<= SCROLLWIDTH);
	
	length = Dy(r);
	if (extent < max) {
		r.min.y = r.min.y + div_down(length * thumb, max);
		if (thumb < max) {
			r.max.y = r.min.y + div_up(length * extent, max);
		} else
			r.min.y = r.max.y;
	}
	
	return r;
}

