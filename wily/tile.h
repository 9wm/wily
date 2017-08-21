#include "wily.h"

typedef enum Ori { H, V } Ori;
/* Windows and Wily itself have V orientation,
 * columns have H orientation
 */
struct Tile {
	View	*tag;		/* every tile has a tag */
	View	*body;		/* window tiles also have a body */

	int	min,max;		/* starting and ending location */
	int	base;		/* Minimum size of tile.  */
	Ori	ori;			/* orientation of _our_ min and max */
	Bool	ishidden;		/* is this tile currently hidden? */
	
	Tile	*up, *down;	/* parent, child */
	Tile	*left, *right;	/* siblings */
	int	cmin, cmax;	/* start and end locations of children */
};

#define TILESIZE(tile)  ((tile)->max - (tile)->min)
#define LISTSIZE(list)  ((list)->cmax - (list)->cmin)
#define ISWIN(tile) ( (tile)->body!= 0 )

/* loop over every visible child in the halfopen range [start,end) */
#define FOR_EACH_VISIBLE(start,end)\
		for(t=(start); t!=(end);t=t->right)\
		if (!t->ishidden)
#define FOR_EACH_TILE(start,end)\
		for(t=(start); t!=(end);t=t->right)

extern int tagheight;	/* height of a tag */
extern Tile	*wily;

Bool		tile_invariant(Tile *tile);
Bool		list_invariant(Tile *list);
View *	point2view(Point pt);
void		tile_reshaped(Tile *t);
Tile*		point2tile(Tile *tile, Point p);
void		list_unhide(Tile*list);
Tile*		last_visible_body(Tile*start, Tile*end);
void		list_reshaped(Tile *l, Tile *t);
Tile*		newparent(Tile*tile, Point p);
Tile*		point2tile(Tile *tile, Point p);
View *	point2view(Point p);
void		list_unhide(Tile*list);
void		findplace(Tile*list, int *min, int *max);
void		list_slide(Tile *t) ;
Tile*		biggest_visible(Tile*start, Tile*end);
Tile*		last_visible_body(Tile*start, Tile*end);
Tile*		last_visible(Tile*start, Tile*end);
int		list_size(Tile *start, Tile *end);
int		list_basesize(Tile*start, Tile*end);
Tile*		list_find(Tile*parent, int n);
Tile*		next_visible(Tile*t);
void		setcminmax(Tile *list, int*cmin, int*cmax);
void		moveto(Tile*t, int pos);
Rectangle	rectangle(Tile*t);
int		adjust_sizes_in_range(Tile*start, Tile*end, int available);
Tile*		tile_new(Ori, int, int, int, Tile*, Text *, Text*);
int		tile_minsize(Tile*t);
void		list_add(Tile *list, Tile *tile);
Bool		list_oksizes(Tile*list);
Bool		list_contains(Tile*start, Tile*end, Tile *want);
