/*******************************************
 *	Functions on columns
 *******************************************/

#include "tile.h"

extern char* columntools;

/* Return the column enclosing 'tile', or 0 */
Tile*
tile_col(Tile*tile){
	if(tile && tile->ori != H)
		tile = tile->up;
	return tile;
}

/* Create and display a new column. */
void
col_new(View*v, char *arg) {
	int	min, max;
	Tile	*col;
	Text	*tagt;

	findplace(wily, &min, &max);
	tagt = text_alloc(0, false);
	text_replaceutf(tagt, nr,  columntools);
	col = tile_new(H, min, max, MINWIDTH, wily, tagt, 0);
	list_add(wily, col);
}

/* Delete as many of the windows of 'tile' as possible.  Return 0
 * if we got them all.
 */
static int
col_delwins(Tile*tile) {
	Tile	*t, *next;
	int	problem = 0;

	for(t = tile->down; t; t= next) {
		next = t->right;
		if (win_del(t))
			problem = 1;
	}
	return problem;
}

void
col_del(Tile*t) {
	if(t && !col_delwins(t)) {
		tile_del(t);
	}
}
