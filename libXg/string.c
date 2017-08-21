/* Copyright (c) 1992 AT&T - All rights reserved. */
#include <libc.h>
#include <libg.h>
#include "libgint.h"

enum	{ Max = 128 };

Point
string(Bitmap *b, Point p, Font *ft, char *s, Fcode f)
{
	int x, y, wid, i, j, n, nti, cf;
	XChar2b cbuf[Max];
	unsigned short fbuf[Max];
	XTextItem16 ti[Max];
	Rune r;
	GC g;

	x = p.x;
	y = p.y;
	if (b->flag&SHIFT){
		x -= b->r.min.x;
		y -= b->r.min.y;
	}
	y += ft->ascent;
	g = _getfillgc(f, b, ~0);

	while (*s) {
		n = cachechars(ft, &s, cbuf, Max, &wid, fbuf);
		if (n <= 0) {
			s += chartorune(&r, s);
			continue;
		}
		nti = 0;
		cf = fbuf[0];			/* first font */
		ti[0].chars = cbuf;
		ti[0].delta = 0;
		ti[0].font = (xFont)ft->subf[cf].f->id;
		for (i = 1, j = 1; i < n; i++, j++) {
			if (fbuf[i] != cf) {	/* font change */
				cf = fbuf[i];
				ti[nti++].nchars = j;
				ti[nti].chars = &cbuf[i];
				ti[nti].delta = 0;
				ti[nti].font = (xFont)ft->subf[cf].f->id;
				j = 0;
			}
		}
		ti[nti++].nchars = j;
		XDrawText16(_dpy, (Drawable)b->id, g, x, y, ti, nti);
		x += wid;
	}
	p.x = (b->flag&SHIFT) ? x + b->r.min.x : x;
	return p;
}
