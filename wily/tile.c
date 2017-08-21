/*******************************************
 *	Tiling window manager
 *******************************************/

#include "tile.h"

/* Is the tile hidden? */
Bool
tile_hidden(Tile *t) {
	/* We are hidden, or one of our ancestors is hidden */
	return t->ishidden || (t->up && tile_hidden(t->up));
}

Bool
tile_invariant(Tile *tile) {
	int	size ,body,tag, diff;
	Tile	*last;

	assert(tile);

	/* Can't have _both_ a body and and a child */
	assert(!(tile->body && tile->down));	

	if(tile->ishidden)
		return true;
		
	/* Tests for non-hidden tiles */
	size = TILESIZE(tile);
	assert( size >= tile->base);
	
	if (!tile->body)
		return true;
	
	/* Tests for tiles with bodies, i.e. windows */

	/*
         * The size of the tile == the sum of the sizes of its body and
         * tag, except if we're the "bottom" tile, in which case the
         * size of the tile might be a _little_ bit bigger than that
         * sum.
	 */
	body = view_height(tile->body);
	tag = view_height(tile->tag);
	last = last_visible_body(tile, 0);

	if (tile != last) {
		assert(size == body + tag);
	} else {
		/* bottom tile might have a little 'slop' */
		diff = size - (body + tag);
		assert(diff >= 0);
		assert (diff <= tag);
	}
	
	return true;
}

Bool
list_invariant(Tile *list) {
	int	prev;
	Tile	*t;
	int	diff;
	
	assert(tile_invariant(list));	/* A list is also a tile */

	assert(!list->body);			/* Lists don't have bodies */

	for (prev = list->cmin, t= list->down; t; t=t->right) {
		assert(tile_invariant(t));	/* Every child is a valid tile */
		if(t->ishidden)
			continue;

		/* Every visible tile abutts the previous one */
		assert(t->min == prev);

		prev = t->max;
	}
	if(list->down) {
		diff = list->cmax - prev;	/* slop at the bottom */
		assert(diff >= 0);
		assert(diff <= list->down->base);
	}
	return true;
}

/* "Snap" the size of 't' to some "neat" value,
 * without increasing 't's size.
 */
static void
quantize(Tile*t) {
	int	tag, body,size;

	if (!t->body)
		return;	/* only really makes sense for windows */

	size = TILESIZE(t);
	tag = snapheight(t->tag, size);
	assert(size >= tag);	/* or we should have been hidden */

	body = snapheight( t->body, size -tag);
	t->max = t->min + tag  + body;
}

/* Ensure 't' wil fit inside 'list'. */
static void
crop(Tile*t, Tile *list) {
	int	size = TILESIZE(t);
	int	listsize = LISTSIZE(list);

	assert(t->up == list);
	assert (size >= 0);

	if(size > listsize) {
		t->max = t->min + listsize;
		size = listsize;
	}

	if (t->min < list->cmin)
		moveto(t, list->cmin);
	if (t->max > list->cmax)
		t->max = list->cmax;

	assert (t->max >= t->min);
	assert (t->max <= list->cmax);
	assert (t->min >= list->cmin);
}

/* 't's shape has changed.  Redisplay it, and any of its children */
void
tile_reshaped(Tile *t) {
	Rectangle r = rectangle(t);

	assert(!t->ishidden);	/* we don't reshape hidden tiles */
	cls(r);
	view_reshaped(t->tag, r);
	if(t->body) {
		r.min.y += view_height(t->tag);
		view_reshaped(t->body, r);
	} else {
		list_reshaped(t,0);
	}
	assert(tile_invariant(t));
}

/* Add 'tile' to 'list' and redisplay the modified 'list'
 * Try to maintain 'tile's min and max, but also try to avoid hiding
 * other tiles.  Make sure that 'tile' ends up with a reasonable amount
 * of space.
 */
void
list_add(Tile *list, Tile *tile) {
	Tile	*left;
	Tile	*next;
	int	max;

	assert(!list->ishidden);
	assert(!tile->ishidden);
	assert(list_invariant(list));

	if(tile->body) {
		Path	buf;
		Data	*d;

		d = view_data(tile->body);
		data_getlabel(d, buf);
		placedcol( buf, list);
	}

	tile->up = list;
	crop(tile, list);

	if( (left = list_find(list, tile->min)) ) {
		/* Add 'tile' just to the right of 'left' */
		left->max = tile->min;
		if(TILESIZE(left) < tile->base)
			left->ishidden = true;
		else
			quantize(left);
		tile->left = left;
		tile->right = left->right;
		left->right = tile;
		if (tile->right)
			tile->right->left = tile;
	} else {
		assert(!list->down);	/* ...or we would have found a tile */
		tile->right = tile->left = 0;
		list->down = tile;
		tile->min = list->cmin;
		tile->max = list->cmax;
	}

	/* Maybe expand tile->max */
	next = next_visible(tile);
	max = next? next->min : list->cmax;
	tile->max = MAX(tile->max, max);
	tile->min = MIN(tile->max - tile->base, tile->min);
	list_reshaped(list, tile);
}

/* Adjust the position of 't', to try to make sure we don't
 * have to hide all of t's siblings.  On the other hand, don't
 * move 't' _too_ much.
 */
static void
adjust_position(Tile *t) {
	Tile	*l = t->up;
	int	size, available, before, after, minsize, diff, move;

	before = 	list_basesize(l->down, t);
	after = 	list_basesize(t->right, 0);
	size =	TILESIZE(t);

	/* We're prepared to shrink to half our asked-for size,
	 * or the minimum size for a tile of our type, whichever
	 * is bigger.
	 */
	minsize = MAX(size/2, tile_minsize(t));
	available = size - minsize;

	/* move t->min down a bit? */
	diff = l->cmin + before - t->min;
	if (diff>0) {
		move= MIN(available, diff);
		if(move > 0) {
			t->min += move;
			available -= move;
		}
	}
	
	/* move t->min up a bit? */
	diff = t->max - (l->cmax - after);
	if (diff>0) {
		move= MIN(available, diff);
		if(move > 0) {
			t->max -= move;
		}
	}
}

/* 
* Adjust the sizes of the children, redisplay them if necessary.
* Avoid moving or resizing 't'.  Ensure that 't' remains visible.
* We can assume that all of the
* tiles have the right approximate size, but that's it.
*/
void
list_reshaped(Tile *l, Tile *tile) {
	Tile	*t;
	int	cmin, cmax;
	Tile	*slop;
	int	diff;

	assert(list_oksizes(l));
	if (tile) {
		crop(tile,l);
		adjust_position(tile);
		adjust_sizes_in_range(l->down, tile, tile->min - l->cmin);
		adjust_sizes_in_range(tile->right, 0, l->cmax - tile->max);
	} else
		adjust_sizes_in_range(l->down, 0, LISTSIZE(l));

	assert(list_oksizes(l));
	assert(list_size(l->down, 0) <= LISTSIZE(l));

	FOR_EACH_VISIBLE(l->down,0) {
		quantize(t);
	}

	diff = LISTSIZE(l) - list_size(l->down, 0);
	assert(diff>=0);

	/* Even things up a little bit */
	if ((slop = last_visible_body(l->down,0)) ||
				(slop = last_visible(l->down, 0)) ) {
		slop->max += diff;
	}

	list_slide(l);

	setcminmax(l, &cmin, &cmax);
	FOR_EACH_TILE(l->down,0) {
		t->cmin = cmin;
		t->cmax = cmax;
		if(!t->ishidden)
			tile_reshaped(t);
	}
	assert(list_invariant(l));
}

/*
 * Find a good place to create a window to display 'path'.
 * Fill *col with the column to contain the window,
 * *min, *max with a suggested position within the column
 */
/* Change the font of 't' and t's children */
void
tile_setfont(Tile *t,char* arg) {
	if(t->body) {
		view_setfont( t->body, arg);
	} else {
		FOR_EACH_TILE(t->down, 0) {
			tile_setfont(t, arg);
		}
	}
}

/*
 * Unlink 'tile' from its parent/child/siblings.
 */
void
tile_unlink(Tile*tile){
	/* Unlink tile */
	if(tile->right)
		tile->right->left = tile->left;
	if(tile->left) {
		tile->left->right = tile->right;
		/* Give the preceding window all the space. */
		tile->left->max = tile->max;
	} else {
		tile->up->down = tile->right;
	}
	
	list_unhide(tile->up);
	cls(rectangle(tile));
	list_reshaped(tile->up, 0);
}

/*
 * Free up all the resources used by 'tile'
 */
void
tile_free(Tile*tile){
	/* Free tile's resources */
	free(tile->body);
	free(tile->tag);
	free(tile);
}

/*
 * Unlink 'tile' from its parent/child/siblings.
 * Free up all the resources used by 'tile'
 */
void
tile_del(Tile *tile) {
	tile_unlink(tile);
	tile_free(tile);
}

/* Find a new parent for 'tile' somewhere at point 'p' */
Tile*
newparent(Tile*tile, Point p){
	Tile	*t = point2tile(wily, p);
	assert(t);
	while((t->ori == tile->ori) || t->body)
		t = t->up;
	return t;
}

void
tile_move(Tile *tile, Point p){
	Tile	*parent;
	int	dest;

	/* Make sure we'll have someplace worth moving to */
	parent = point2tile(wily,p);
	if (parent == 0 || parent == wily)
		return;

	tile_unlink(tile);
	dest = (tile->ori == H) ? p.x : p.y;
	if (tile->min < dest && dest < tile->max)
		tile->min = dest;
	else
		moveto(tile, dest);

	parent = newparent(tile, p);
	list_add(parent, tile);
	cursorset(buttonpos(tile));
}

void
tile_show(Tile *tile) {
	if(tile->up)
		tile_show(tile->up);
	if (tile->ishidden || (TILESIZE(tile) < tile_minsize(tile)))
		tile_grow(tile, 1);
}

View*
tile_body(Tile*t) {
	return t? t->body: 0;
}

View*
tile_tag(Tile*t) {
	return t? t->tag: 0;
}

/* Fill in 'min' and 'max' with a subregion within 'tile' for creating a new tile */
static void
tile_split(Tile *tile, int *min, int *max)
{
	int	lastline, average;

	*max = tile->max;
	average = (tile->max + tile->min)/ 2;

	if(tile->body) {
		lastline = view_lastlinepos(tile->body);
		assert(lastline <= tile->max);
		*min = MIN(average, lastline);
	} else {
		*min = average;
	}
}

/* Fill 'min' and 'max' with a good place to add a tile within 'list'
 */
void
findplace(Tile*list, int *min, int *max)
{
	/* Split the largest visible tile, or use all the available space. */

	Tile*biggest;

	if ( (biggest=biggest_visible(list->down, 0)) ) {
		tile_split(biggest, min, max);
	} else {
		*max = list->cmax;
		*min = list->cmin;
	}
}

/* Change t's position, keeping its size. */
void
moveto(Tile*t, int pos)
{
	int	size = TILESIZE(t);
	t->min = pos;
	t->max = t->min + size;
}

/* Create (but don't display) a new tile */
Tile*
tile_new(Ori ori, int min, int max, int base, 
	Tile*parent, Text *tagt, Text*bodyt)
{
	Tile*tile = NEW(Tile);
	int	minsize;

	tile->ishidden = false;
	tile->ori = ori;
	tile->min = min;
	tile->max = max;
	tile->base = base;
	tile->up = parent;
	setcminmax(parent, &tile->cmin, &tile->cmax);
	tile->down = tile->left = tile->right = 0;
	tile->tag = view_new(font, true, tagt, tile);
	tile->body = bodyt ? view_new(font, false, bodyt, tile): 0;

	minsize = tile_minsize(tile);
	if(TILESIZE(tile) < minsize)
		tile->max = tile->min + minsize;
	
	return tile;
}

/* The minimum comfortable viewing size for 't' */
int
tile_minsize(Tile*t)
{
	return 3 * t->base;
}

