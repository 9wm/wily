/*******************************************
 *	geometry stuff for tiles and views
 *******************************************/

#include "tile.h"
#include "view.h"

/* Set 'cmin' and 'cmax' for any child of 'list'  */
void
setcminmax(Tile *list, int*cmin, int*cmax) {
	if(list) {
		*cmin = (list->ori == V)? list->tag->r.max.y + tagheight:list->min;
		*cmax = list->max;
	} else {
		*cmin = 0;
		*cmax = screen.r.max.x;
	}
}

/* Return the Rectangle to enclose 't' */
Rectangle
rectangle(Tile*t) {
	if(t->ori==H) {
		return Rect(t->min, t->up->tag->r.max.y, 
				t->max, t->up->max);
	} else if (t->up) {
		return Rect(t->up->min, t->min, t->up->max, t->max);
	} else {
		return Rect(0, t->min, t->cmax, t->max);
	}
}

/* Return a point somewhere in the middle of 'tile's button */
Point
buttonpos(Tile*tile) {
	return add(tile->tag->r.min, Pt(SCROLLWIDTH/2, SCROLLWIDTH/2));
}

/* Return the tile containing p (or 0) */
Tile*
point2tile(Tile *tile, Point p) {
	int	pos;
	Tile	*t;

	assert(tile);
	if(tile->body || ptinrect(p, tile->tag->r))
		return tile;
	pos = tile->ori==V ? p.x : p.y ;
	for (t =tile ->down; t; t=t->right) {
		if (!tile_hidden(t) && pos < t->max && pos >= t->min)
			break;
	}
	if (t && !tile_hidden(t) && pos > t->min)
		return point2tile(t,p);
	return tile;
}

/* Return the view containing p (or 0) */
View *
point2view(Point p) {
	Tile	*t;
	View	*v;

	t = point2tile(wily, p);
	if (ptinrect(p, t->tag->r))
		v=  t->tag;
	else
		v = t->body;
	assert(IMPLIES(v, ptinrect(p, v->r)));
	return v;
}

