/*******************************************
 *	add and delete windows
 *******************************************/

#include "tile.h"
static void	win_place(Tile *col, Text *tag, Text *body);

/* Free the resources tied up by 'win'.  Return 0 for success. */
int
win_del(Tile *w) {
	if(!w)
		return 0;

	assert(ISWIN(w));

	/* make sure we can delete the body */
	if(view_delete(w->body)){
		return -1;
	}
	/* delete the tag, and the tile */
	view_delete(w->tag);
	tile_del(w);
	return 0;
}

/* Return the window associated with 'tile', or 0. */
Tile*
tile_win(Tile*tile) {
	return (tile && tile->body) ? tile : 0;
}

/* clone 'win' */
void
win_clone(Tile *win) {
	Text *tag, *body;
	
	assert(ISWIN(w));
	
	tag = view_text(win->tag);
	body = view_text(win->body);
	win_place(win->up, tag, body);
}

/* Create a window to represent 'path' */
void
win_new(char*path, Text*tag, Text*body) {
	Tile * col;
	
	col = findcol(path);	
	win_place(col, tag, body);
}

/* Add some text to w's tag representing the current selection */
void
win_anchor(Tile*w, char *arg) {
	char	buf[80];

	assert(ISWIN(w));

	view_getdot(w->body, buf, arg!=0);
	tag_addtool(view_text(w->tag), buf);
}

/** Create and place a new window with the given 'tag' and 'body'
 * text, in the given 'col'umn
 **/
static void
win_place(Tile *col, Text *tag, Text *body) {
	Tile	*win;
	int	max, min;

	findplace(col, &min, &max);
	win = tile_new(V,  min, max, tagheight, col, tag, body);
	list_add(col, win);
	assert(ISWIN(win));
}

