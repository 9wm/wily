/*******************************************
 *	Handle keystroke events
 *******************************************/

#include "wily.h"
#include "view.h"

static void		view_cursor(View *v, Rune r);
static void		addrune(View*v, Rune r);
static void		tag_cr(View *v);
static void		backspace(View*v);
static void		deleteline(View*v);
static void		deleteword(View*v);
static void		esc(View*v);

void
dokeyboard(View *v, Rune r) {
	switch(r) {
	case DownArrow:
	case UpArrow:
	case Home:
	case End:
	case LeftArrow:
	case RightArrow:	view_cursor(v, r); 	break;
	
	case PageDown:	
	case PageUp:		view_pagedown(v,r==PageDown); break;
	
	case Ctrlh:
	case Backspace:	backspace(v); break;
	case Ctrlu:		deleteline(v); break;
	case Ctrlw:		deleteword(v); break;
	case Esc:			esc(v); break;
	
	case '\n':			if(!v->scroll){tag_cr(v); break; }
	default:			addrune(v,r);
	}
}

/******************************************************
	static functions
******************************************************/

/* Handle carriage-return in 'v'
 *
 * Select text as if we hit escape,
 * and either search (if selected text starts with ':')
 * or execute the selected text.
 */
static void
tag_cr(View*v) {
	char*cmd;
	
	if(!RLEN(v->sel)){
		assert(v->anchor <= v->sel.p0);
		view_select(v, range(v->anchor, v->sel.p0));
	}
	cmd = text_duputf(v->t, v->sel);
	if(cmd[0]==':'){
		b3(v, v->sel);
	} else {
		if (!data_sendexec(view_data(v),cmd, 0))
			run(v, cmd, 0);
	}
	free(cmd);
}

/* delete selection and rune before */
static void
backspace(View*v){
	Range del = v->sel;
	
	if(del.p0)
		del.p0--;
	view_cut(v, del);
}

/* delete selection and back to start of line */
static void
deleteline(View*v){
	Range del = v->sel;
	
	del.p0 = text_startOfLine(v->t, del.p0);
	view_cut(v, del);
}

/* delete back to start of word */
static void
deleteword(View*v){
	Range del = v->sel;
	
	del.p0 = text_startofword(v->t, del.p0);
	view_cut(v, del);
}

static void
esc(View*v) {
	Range del = v->sel;
	
	if ((RLEN(del))) {
		/* delete selected text */
		view_cut(v, del);	
	} else {
		/* Select from v->anchor to v->sel.p0 */
		view_select(v, range(v->anchor, v->sel.p0));
		view_setlastselection(v);
	}
}

static void
addrune(View*v, Rune r) {
	Rstring	s;

	if ( r == '\n' && v->autoindent) {
		s = text_autoindent(v->t, v->sel.p0);
	} else {
		s.r0 = &r;
		s.r1 = &r +1;
	}
	if(RLEN(v->sel)){
		snarf(v->t, v->sel);
	}
	text_replace(v->t, v->sel, s);
	view_show(v, v->sel);
}

/*
 * We've hit a cursor key.
 * 
 * Set the selection to something relative to previous sel.p0, and
 * make sure the new selection is visible.
 */
static void
view_cursor(View *v, Rune r) {
	ulong	p;
	Point	pt;

	p = v->sel.p0;
	switch(r) {
	case LeftArrow:
		if(p)
			p--;
		break;
	case RightArrow:
		if (p < text_length(v->t))
			p++;
		break;
	case DownArrow:
	case UpArrow:
		pt = frptofchar(&v->f, p - v->visible.p0);
		if (r==DownArrow)
			pt.y += v->f.font->height;
		else
			pt.y -= v->f.font->height;
		p = frcharofpt(&v->f, pt) + v->visible.p0;
		break;
	case Home:
		p = 0;
		break;
	case End:
		p = text_length(v->t);
		break;
	}
	view_select(v, range(p,p));
	view_show(v, v->sel);
}

