/*******************************************
 *	Decide where to place new windows
 *******************************************/

#include "wily.h"
#include "tile.h"

enum {TSIZE=401};
static Tile* placetable[TSIZE];	/* hashed array of column tiles */

static Bool		iserror(char*label);
static Bool		validcolumn(Tile*c);
static unsigned long	hashpath(char *path);
static Tile *			find(char *label);

/* Save the window column for later lookup. */
void
placedcol(char*label, Tile *c) {
	if (label && !iserror(label))
		placetable[hashpath(label)] = c;
}

/* Return the column to use for a new window with label 'path' */
Tile*
findcol(char*label) {
	Tile	*c;

	if(!wily->down){
		col_new(wily->tag,0);
		return wily->down;
	}

	if(iserror(label))
		return last_visible(wily->down, 0);

	if (!(c= find(label))) {
		c = biggest_visible(wily->down, 0);
		assert(c);
		placedcol(label, c);
	}
	return c;
}

/******************************************************
	static functions
******************************************************/

static Bool
iserror(char*label) {
	static char *errors = "+Errors";
	int	elen = strlen(errors);
	int	len = strlen(label);

	return (len >= elen && !strncmp(label + (len - elen), errors, elen));
}

/* Is 'c' still a valid pointer for a visible column? */
static Bool
validcolumn(Tile*c)
{
	return list_contains(wily->down, 0, c);
}

/* Hash from directory name into 'placetable' */
static unsigned long
hashpath(char *path)
{
	char *s;
	unsigned long hash = 0;
	Path fullpath;

	assert(path);

	label2path(fullpath, path);
	path = fullpath;
	for (s = strrchr(path, '/'); path < s; path++)
		hash = (hash << 5) ^ *path;

	return hash % TSIZE;
}

static Tile *
find(char *label)
{
	Tile *c;
	if (label) {
		c = placetable[hashpath(label)];
		if (validcolumn(c))
			return c;
	}
	return 0;
}

