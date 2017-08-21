/*
 * Both tags and bodies use a View as a display widget.  The difference
 * between them is that bodies have scrollbars, whereas tags don't (but
 * tags automatically grow as required).  A View uses a Text to keep
 * track of the text that is currently not on screen, and a Frame to
 * display text.  A view keeps track of the visible area, the selected
 * area, and the place where we began typing (if we're in the middle of
 * typing).
 *
 * The View controls all aspects of viewing and selecting text, but has
 * no knowledge of how we store all the text.

 * If a view is not visible, v->r.max.y == v->r.min.y, and it's frame
 * will have been frcleared, i.e. not usable
 */

struct View {
	Rectangle		r;		/* of whole view */
	Text			*t;
	Frame		f;
	Range		visible, sel;		/* visible, selected area */
	ulong		anchor;	/* where we most recently started typing */
	Scroll		*scroll;	/* 0 for tag */
	Tile			*tile;
	View			*next;	/* list of views displaying same Data */
	Bool			selecting;	/* we're busy dragging out a selection */
	Bool			autoindent;	/* autoindent in this view? */
};
#define ISTAG(v) 	((v) && !(v)->scroll)
#define ISBODY(v)	( (v) && (v)->scroll)
#define ISVISIBLE(v) ( (v)->f.b!=0 )

/* Return true if 'p' is contained in 'v's scrollbar */
#define POINTINSCROLL(p,v) ( (p).x < (v)->r.min.x + SCROLLWIDTH + INSET )
