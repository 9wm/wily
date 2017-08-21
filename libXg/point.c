/* Copyright (c) 1992 AT&T - All rights reserved. */
#include <libc.h>
#include <libg.h>
#include "libgint.h"

void
point(Bitmap *b, Point p, int v, Fcode f)
{
	int x, y;
	GC g;

	x = p.x;
	y = p.y;
	if(b->flag&SHIFT){
		x -= b->r.min.x;
		y -= b->r.min.y;
	}
	g = _getfillgc(f, b, v);
	XDrawPoint(_dpy, (Drawable)b->id, g, x, y);
}
