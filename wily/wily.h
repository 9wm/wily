/*******************************************
 *	Includes a bunch of header files
 *******************************************/

#ifndef WILY_H
#define WILY_H

#include <u.h>
#include <libc.h>
#include <libg.h>
#include <frame.h>
#include <msg.h>

#include <assert.h>
#define IMPLIES(a,b) (!(a)||(b))

#include <sys/types.h>
#include <sys/stat.h>
#include "const.h"

typedef struct stat Stat;
typedef char	Path[MAXPATH];
typedef Rune	RPath[MAXPATH];

typedef struct View View;	/* see view.h */
typedef struct Scroll Scroll;	/* see scroll.c */
typedef struct Text	Text;	/* see text.h */
typedef struct Data Data;	/* see data.h */
typedef struct Undo Undo;	/* see undo.c */
typedef struct Tile Tile;		/* see tile.h */
typedef struct Rstring Rstring;
typedef struct Mbuf Mbuf;	

struct Mbuf {
	char		*buf;	/* alloced initially, never freed */
	int		alloced;
	int		n;
};

struct Rstring { Rune	*r0, *r1; };	
	/* elements of the Rstring are >= r0 and < r1 */
#define RSLEN(r) ((r).r1 -(r).r0)
#define RSOK(r) ((r).r1 >= (r).r0)
#define RSFREE(r) free((r).r0)

/* GLOBALS */
#include "global.h"
#include "proto.h"

#endif
