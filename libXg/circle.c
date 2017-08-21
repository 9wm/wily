/* Copyright (c) 1992 AT&T - All rights reserved. */
#include <libc.h>
#include <libg.h>
#include "libgint.h"

void
circle(Bitmap *b, Point p, int r, int v, Fcode f)
{
	unsigned int d;
	int x, y;
	GC g;

	x = p.x - r;
	y = p.y - r;
	if (b->flag&SHIFT){
		x -= b->r.min.x;
		y -= b->r.min.y;
	}
	d = 2*r;
	g = _getfillgc(f, b, v);
	XDrawArc(_dpy, (Drawable)b->id, g, x, y, d, d, 0, 23040/* 360 deg */);
}
