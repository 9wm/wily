/*******************************************
 *	tile data structure
 *******************************************/

#include "wily.h"

typedef enum TileType { TILE_WIN, TILE_PARENT } TileType;
typedef struct TileSet TileSet;

struct Tile {
	Rectangle	r, tagr, contentsr;
	View	*tag;		/* every tile has a tag */
	int	pos;			/* position of this tile within a set */
	int	size;			/* current actual size (maybe zero) */
	int 	desiredSize; 	/* remember a good size if we're minimized */
	int	minSize;		/* minimum usable size */
	
	/* Depending on 'type', we might contain a 'body', or
	 * a set of children
	 */
	TileType	type;
	union {
		View*	body;
		TileSet	children;
	} c;	/* contents -- my Kingdom for anonymous unions*/
	Tile	*parent;		/* null for main window */
};

#define HIDDEN(t) ((t)->size==0)

tile_paint(Tile*t, Rect r) {
	view_paint(tag, r);
	if (t->type == TILE_WIN) {
		view_paint(t->contents.body, r2);
	} else {
		tileSetPaint(t->contents.children, r2);
	}
}

/** TileSet contains a list of tiles, which might be arranged
 * horizontally (e.g. columns within main wily win)
 * or vertically (e.g. windows within a column).
 **/
struct TileSet {
	Tile	**tile;
	int	ntiles, maxtiles;
	int	totalSize;
	Rectangle	r;
	Bool	isHorizontal;
}

#define TILESIZE(tile)  ((tile)->max - (tile)->min)
#define LISTSIZE(list)  ((list)->cmax - (list)->cmin)
#define ISWIN(tile) ( (tile)->type== TILE_WIN )


extern int tagheight;	/* height of a tag */
extern Tile	*wily;

