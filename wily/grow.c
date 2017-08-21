/*******************************************
 *	grow tiles by different amounts
 *******************************************/

#include "tile.h"
/************************************************************
        Tile growth functions:

        Modify tile->min, tile->max, possibly min and max for tile's
        siblings.

        After these functions, every visible tile will have the _size_
        which we want, but the tiles may not abutt, and may not fit into
        tile->up.  We rely on list_reshaped to tidy these things up.
************************************************************/

/* grow tile a little */
static void
gsome(Tile*tile) {
	tile->min -= tile->base*2;
	tile->max += tile->base*2;
}

/* grow tile lots */
static void
gmost(Tile*tile) {
	Tile	*t, *up = tile->up;
	int	space = 0;	/* space either above or below 'tile' */

	for (t = up->down; t; t=t->right) {
		if (t != tile) {
			space += t->base;
			t->max = t->min + t->base;
		} else {
			t->min = space + up->cmin;
			space = 0;	/* start counting space _above_ tile */
		}
	}
	tile->max = up->cmax - space;
}

/* grow tile way lots */
static void
gall(Tile *tile) {
	Tile	*t, *up = tile->up;
	for (t = up->down; t; t=t->right) {
		if (t != tile) 
			t->ishidden = true;
	}
	tile->min = up->cmin;
	tile->max = up->cmax;
}

/************************************************************
	End of Tile growth functions:
************************************************************/

/* Grow 'tile' by some amount */
void
tile_grow(Tile *tile, int buttons) {
	assert(list_invariant(tile->up));

	list_unhide(tile->up);

	/*
	Adjust the placement of tile, and possibly tile's siblings.
	Don't worry too much about placing them.
	Relies on list_reshaped to do the placement and redisplay.
	*/

	switch (buttons) {
	case 1:	gsome(tile);	break;
	case 2:	gmost(tile);	break;	
	case 4:	gall(tile);		break;
	default:	gsome(tile);	break;
	}

	list_reshaped(tile->up, tile);

	assert(list_invariant(tile->up));
}


