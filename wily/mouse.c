/*******************************************
 *	Handle mouse actions
 *******************************************/

#include "wily.h"
#include "view.h"
#include <ctype.h>

static void		b2			(View *, Range, Bool);
static void		dobutton		(View *, Mouse *);
static void		doscroll		(View *, Mouse *);
static void		action		(View *, Mouse *, Range, ulong);

/* PRE: a mouse button is down
 * POST: no mouse buttons down, we've probably done something.
 */
void
domouse(View	*v, Mouse *m) {
	assert(m->buttons);

	if ( POINTINSCROLL(m->xy, v) ) {
		if(v->scroll)
			doscroll(v,m);
		else
			dobutton(v, m);
	} else {
		ulong	oldbuttons = m->buttons;
		Range	r = vselect(v, m);

		action(v, m, r, oldbuttons);
	}
}

/******************************************************
	static functions
******************************************************/

static void
dobutton(View *v, Mouse *orig) {
	Mouse m;
	Tile	*tile = view_tile(v);

	assert(ISTAG(v));
	assert(orig->buttons);

	if(tile == wily)		/* can't move the main window this way */
		return;

	/* swap cursor, follow mouse until button state changes */
	cursorswitch(&boxcursor);
	do {
		m = emouse();
	} while(m.buttons == orig->buttons);
	cursorswitch(cursor);

	/* aborted by pressing a different button? */
	if(m.buttons)
		return;	

	if (distance(m.xy, orig->xy) < SMALLDISTANCE) {
		tile_grow(tile, orig->buttons);
		cursorset(buttonpos(tile));
	} else
		tile_move(tile, m.xy);
}

/*
 * Some builtins should always be applied to the window where
 * you clicked button 2.
 */
static char *locals[] = {"Look", "Put", "Get", "Undo", "Redo", 0};
static Bool
islocal(char *s) {
	char	**ptr;

	for (ptr =locals; *ptr; ptr++)
		if (!strncmp(s, *ptr, strlen(*ptr)))
			return true;
	return false;
}

/* clicked button 2 in 'v' selecting 'r'.  'ischord' is true
 * if we also clicked button 1.
 */
static void
b2(View *v, Range r, Bool ischord) {
	char	*cmd,*arg;
	View	*ls = last_selection;

	r = view_expand(v, r, notcommand);
	if(!RLEN(r))
		return;

	cmd = text_duputf(v->t, r);

	if(ischord && ls  && RLEN(ls->sel) < 200) {
		Range	rarg;

		rarg = view_expand(ls, ls->sel, notfilename);
		arg = text_duputf(ls->t, rarg);

		/* use arg for context-- with some exceptions */
		if(!islocal(cmd))
			v = ls;
	} else
		arg = 0;

	if (!data_sendexec(view_data(v),cmd, arg))
		run(v, cmd, arg);

	free(cmd);
	if(arg)
		free(arg);
}

/*
 * Currently mouse is like 'm', originally we had 'oldbuttons'.  When
 * we're finished, v's display should be correct, and no mouse buttons
 * should be down.
 */
static void
action(View *v, Mouse *m, Range r, ulong oldbuttons) {
	/* mouse button state has changed, possibly do something */
	assert(m->buttons != oldbuttons);

	if (oldbuttons&LEFT) {
		enum {Cancut = 1, Canpaste = 2} state = Cancut | Canpaste;

		while(m->buttons) {
			if(m->buttons&MIDDLE) {
				if (state&Cancut) {
					view_cut(v, v->sel);
					state = Canpaste;
				}
			} else if ( m->buttons&RIGHT) {
				if (state&Canpaste) {
					view_paste(v);
					state = Cancut;
				}
			}
			*m = emouse();
		}
	} else if (oldbuttons & MIDDLE) {
		if(m->buttons) {
			if(m->buttons&LEFT)	/* chord */
				b2(v,r,true);	
			while (m->buttons)	/* wait for button up */
				*m = emouse();
		} else {
			b2(v,r,false);
		}
	} else {
		assert((oldbuttons&RIGHT));
		if(m->buttons)	/* cancelled a b3 */
			while (m->buttons)
				*m = emouse();
		else
			b3(v, r);
	}
}

/* PRE:  'e' a mouse event in 'v', but not in v's frame, so probably in
 * the scrollbar.  'v' is a body frame.
 * POST:  we've tracked the mouse until all the buttons are down,
 * and have scrolled if we should have.
 */
static void
doscroll(View *v, Mouse	*m ) {
	ulong	buttons;
	ulong	timer;
	ulong	type;
	int		delay = DOUBLECLICK / SCROLLTIME;
	Bool		firstmouse;
	Event	e;

	assert(ptinrect(m->xy, v->r));
	assert(v->scroll);

	buttons = m->buttons;
	assert(buttons);
	type = Emouse;
	firstmouse = true;
	e.mouse = *m;
	m = &e.mouse;

	/* start waiting for timer events */
	timer = etimer(0, SCROLLTIME);
	do {
		assert(type== timer || type == Emouse);

		if(type==Emouse) {
			if (firstmouse) {
				firstmouse = false;
				view_scroll(v, m);
			}
		} else {
			if(delay)
				delay--;
			else
				view_scroll(v, m);
		}
		type = eread(Emouse|timer,&e);
	} while (!(type ==Emouse && m->buttons != buttons));
	estoptimer(timer);	
	/* stop the timer */

	/* wait for buttons up */
	while (m->buttons)
		eread(Emouse, &e);
}

