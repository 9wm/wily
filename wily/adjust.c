/*****************************************************************
	Collection of functions to reduce the size of a tile.
	Each reduces the tile's size, and return the amount of savings.
	They appear in order of severity.
*****************************************************************/

#include "tile.h"

/* If t's body contains any blank lines, get rid of them */
static int
stripwhitespace(Tile *t, int excess) {
	int	saving,size;

	if (!t->body )
		return 0;
	
	size = TILESIZE(t);

	/* Possibly we've already mangled this tile, so it can no longer
	 * properly contain its body and tag
	 */
	if(size < view_height(t->body) + view_height(t->tag))
		return 0;

	if((saving = view_stripwhitespace(t->body))) {
		assert(saving < size);
		saving = MIN(saving, excess);
		t->max -= saving;
		assert(TILESIZE(t) >= t->base);
	}
	return saving;
}

/* Halve the amount by which t's size exceeds t->base */
static int
halve(Tile*t, int excess) {
	int	saving,size = TILESIZE(t);
	int	extra = size - t->base;

	if(extra > 0) {
		saving = MIN(extra/2, excess);
		t->max -= saving;
		return size - TILESIZE(t);
	}
	return 0;
}

/* Shrink 't' down to t->base */
static int
shrink(Tile *t, int excess) {
	int	saving = MIN(TILESIZE(t) - t->base, excess);
	t->max -= saving;
	return saving;
}

/* Hide 't':  => size = 0 */
static int
hide(Tile *t, int excess) {
	assert(TILESIZE(t) == t->base);	/* we've already done 'shrink' */
	t->ishidden = true;
	return t->base;
}

typedef int (*SizeAdjust)(Tile*, int);
SizeAdjust method []  = { 
	stripwhitespace, halve, shrink, hide, 0
};
/*****************************************************************
	End of Collection of functions to reduce the size of a tile.
*****************************************************************/

/*
 * Adjust the sizes of the tiles in [start,end) so that they add to
 * <= 'available'.
 * Return the total size.
*/
int
adjust_sizes_in_range(Tile*start, Tile*end, int available)
{
	Tile *t;
	int size,saving,excess;
	int	now;
	int	j;
	SizeAdjust	m;	/* method to adjust the size of a tile */

	assert(available >= 0);

	size = list_size(start,end);

	for(j=0; (m=method[j]); j++) {
		FOR_EACH_VISIBLE(start,end){
			assert(size == (now = list_size(start, end)));	
			excess = MAX(size - available, 0);
			saving = (*m)(t, excess);
			assert(t->ishidden || (TILESIZE(t) >= t->base));
			size -= saving;
			
			/* check our math */
			assert(size == (now = list_size(start, end)));	

			if (size <= available) {
				/* C doesn't have labelled break.  So sue me. */
				goto out;	
			}
		}
	}
out:
	/* check our math */
	assert(size == (now = list_size(start, end)));	

	assert (size <= available);	/* Even if we had to hide everything */

	/* If we've taken too much, return some size to the last visible tile */
	if(size<available && (t = last_visible(start,end))) {
		t->max += available - size;
		size = available;
		now = list_size(start,end);
		assert( size == now);
	}

	assert( size <= available);
	assert (size >= 0);
	return size;
}
