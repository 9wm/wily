/*******************************************
 *	Simple operations to apply to every visible tile in a list.
 *******************************************/

#include "tile.h"

/********************************************************
	Simple operations to apply to every visible tile in a list.
********************************************************/

Bool
list_oksizes(Tile*list) {
	Tile*t;

	FOR_EACH_VISIBLE(list->down,0){
		assert(TILESIZE(t) >= t->base);
	}
	return true;
}

void
list_unhide(Tile*list) {
	Tile*t;

	FOR_EACH_TILE(list->down,0) {
		t->ishidden = false;
		if ( TILESIZE(t) < t->base)
			t->max = t->min + t->base;
	}
}

/* Slide the visible children of 't' along so that they abut one another */
void
list_slide(Tile *t)  {
	int	prev = t->cmin;

	FOR_EACH_VISIBLE(t->down, 0) {
		moveto(t, prev);
		prev = t->max;
	}
}

/* Return the biggest visible child in the (half-open) range */
Tile*
biggest_visible(Tile*start, Tile*end) {
	Tile	*t, *big = 0;

	FOR_EACH_VISIBLE(start, end) {
		if (!big || TILESIZE(t) > TILESIZE(big))
			big =t;
	}
	return big;
}

/* Return the last child with a visible body in the (half-open) range */
Tile*
last_visible_body(Tile*start, Tile*end) {
	Tile	*t, *last=0;
	FOR_EACH_VISIBLE(start, end) {
		if(TILESIZE(t) > t->base)
			last =t;
	}
	return last;
}

/* Return the last visible child  in the (half-open) range */
Tile*
last_visible(Tile*start, Tile*end) {
	Tile	*t, *last=0;
	FOR_EACH_VISIBLE(start, end) {
		last = t;
	}
	return last;
}

/* The total size of the visible tiles in the (half-open) range. */
int
list_size(Tile *start, Tile *end) {
	int	size=0;
	Tile	*t;
	FOR_EACH_VISIBLE(start, end) {
		size += TILESIZE(t);
	}
	return size;
}

/* Sum the base sizes of the tiles in the (half-open) range. */
int
list_basesize(Tile*start, Tile*end) {
	int	size=0;
	Tile	*t;
	FOR_EACH_VISIBLE(start, end) {
		size += t->base;
	}
	return size;
}

/* Find the child of 'parent' which contains position 'n' */
Tile*
list_find(Tile*parent, int n) {
	Tile	*t;
	FOR_EACH_VISIBLE(parent->down, 0) {
		if (n < t->max && n >= t->min)
			break;
	}
	return t;
}

/* Return the next visible tile after 't' */
Tile*
next_visible(Tile*t) {
	FOR_EACH_VISIBLE(t->right, 0) {
		break;
	}
	return t;
}

/* Return the next visible tile after 't' */
Bool
list_contains(Tile*start, Tile*end, Tile *want) {
	Tile*t;
	FOR_EACH_VISIBLE(start,end) {
		if(t==want)
			return true;
	}
	return false;
}
