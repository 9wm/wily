/* Copyright (c) 1992 AT&T - All rights reserved. */
#include <libc.h>
#include <libg.h>
#include "libgint.h"

/*
 * Only allow cursor to move within screen Bitmap
 */
void
cursorset(Point p)
{
	/* motion will be relative to window origin */
	p = sub(p, screen.r.min);
	XWarpPointer(_dpy, None, (Window)screen.id, 0, 0, 0, 0, p.x, p.y);
}
