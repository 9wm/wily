/*******************************************
 *	b3-type stuff in a view
 *******************************************/

#include "wily.h"
#include "view.h"

static Bool	view_literal(View**vp, Range*r, char*s);
static Bool	view_gotofile(View**vp, Range *r, char *s);

/* If 'label' is the label of some existing file, return View for it. */
View*
openlabel(char*label, Bool create) {
	View*v;
	Path	contracted;
	
	pathcontract(contracted,label);
	
	if ( (v=data_find(contracted)) ) {
		tile_show(v->tile);
		return v;
	} else if ( (v=data_open(contracted,create)) )
		return v;
	else
		return 0;
}

/* Look (builtin) for 'arg' in the context of 'v' */
void
view_look (View *v, char *arg) {
	Bool		found = false;
	ulong	p;
	Range	r;

	assert(ISBODY(v));

	if( ! text_length(v->t))
		return ;

	p = (v->sel.p0 + 1) % text_length(v->t);
	r = range(p, p);

	if(arg) {
		found = text_findliteralutf(v->t, &r, arg);
	} else if (RLEN(v->sel) ){
		found = text_look(v->t, &r, v->sel);
	} else {
		found = false;
	}

	if(found){
		view_show(v,r);
		view_select(v, r);
		view_setlastselection(v);
	}
}

/*
 * Search for address 's'.  Use *vp for context, *r as the starting point
 * for searches.  May create a new window, but doesn't otherwise affect
 * the screen.  If it finds something, return true, and fill in *vp and
 * and *r with the location of where we found something.
 *
 * 's' may be of the form path:addr, :addr, path or a literal, searched
 * for in that order.
 *
 * NB modifies 's'
 */
Bool
view_goto(View**vp, Range *r, char *s) {
	return view_gotofile(vp,r,s) || view_literal(vp, r,s);
}

/* We've clicked the 'goto' button, selecting 'r' in 'v'
 * If this window is under external control, just send the event,
 * otherwise expand the selection, and 'goto' it, in the context of 'v'.
 */
void
b3(View *v, Range r) {
	char	*s;
	View *oldv;
	Range expanded;
	Data	*d;
	View*found;
	
	/* Try to send simply expanded version to remote process */
	expanded = view_expand(v, r, notaddress);
	if (!RLEN(expanded))
		return;	/* empty click nowhere -- ignore */
	s = text_duputf(v->t, expanded);
	d = view_data(v);
	oldv = v;
	
	/* Send to remote process? */
	if(data_sendgoto(d,expanded, s))
		goto cleanup;
	
	
	if (view_gotofile(&v, &expanded, s)) { /* Simple file? */
		r = expanded;
	} else if ( (found = openinclude(v,r)) ) {
		v = found;
		r = found->sel;
	} else if (view_literal(&v, &expanded, s)) { /* Literal? */
		r = expanded;
	} else {	/* found nothing */
		goto cleanup;
	}
	
	view_show(v,r);
	view_select(v,r);
	view_setlastselection(v);

	/* warp unless b3 in the tag jumps to the body. */
	if (oldv != tile_tag(view_win(v)))
		view_warp(v,r);
	
cleanup:
	free(s);
}

/* Search for literal 's' in '*vp' starting at '*r'.
 * If we find a match, update 'vp' and 'r' and return true,
 * otherwise, return false.
 */
static Bool
view_literal(View**vp, Range*r, char*s) {
	View*v;
	Text	*t;
	Range	tmp;
	
	v = *vp;
	tmp = *r;
	/* Only makes sense if 'v' is a body or we can find a useful body */
	if(!ISBODY(v)){
		if ((v = view_body(v)) || (v = view_body(last_selection))) {
			tmp = v->sel;
		} else {
			return false;
		}
	}
	assert(ISBODY(v));
	
	t = v->t;
	if(text_findliteralutf(t, &tmp, s)) {
		*vp = v;
		*r = tmp;
		return true;
	}
	return false;
}

/*
 * Search for address 'a'.  Use *vp for context, *r as the starting point
 * for searches.  May create a new window, but doesn't otherwise affect
 * the screen.  If it finds something, update *vp and *r and return
 * true, otherwise return false and leave vp and r alone.
 *
 * 's' may be of the form path:addr, or path, or :addr.
 */
static Bool
view_gotofile(View**vp, Range *r, char *a) {
	Path	s;
	View	*v = *vp;
	View	*v2;
	char *colon;	/* preserve the colon */
	Path	label;

	strcpy(s,a);		/* BUG - fails horribly if 'a' is large */
	colon = strchr(s, ':');
	if (colon) {
		*colon=0;
	}

	/* look for 's' as a file or directory */
	if(strlen(s)) {
		/* 'path' or 'path:addr' */
		data_addcontext(view_data(v), label, s);
		if( (v2=openlabel(label, false))){
			if(colon) {
				Range oldr = v2->sel;
				
				if(!text_search(v2->t, r, colon+1, v2->sel)) {
					*r = oldr;
				}
			} else {
				*r = v2->sel;
			}
			*vp = v2;
			return true;
		} else 
			return false;	/* bad path */
	}
	assert(!strlen(s));

	/* :addr by itself then */
	if(!ISBODY(v)){
		if( ! ((v = view_body(v)) || (v = view_body(last_selection))) )
			return false;
	}
	*r = v->sel;
	if (text_search(v->t, r, colon+1, v->sel)) {
		*vp = v;
 		return true;
	} 
	return false;
}

