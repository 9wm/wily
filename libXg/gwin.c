/* Copyright (c) 1992 AT&T - All rights reserved. */
#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#ifndef XtSpecificationRelease
#define R3
#define XtPointer caddr_t
#define XtOffsetOf(s_type,field) XtOffset(s_type*,field)
#define XtExposeCompressMultiple TRUE
#endif

#include "GwinP.h"

/* Forward declarations */
static void Realize(Widget, XtValueMask *, XSetWindowAttributes *);
static void Resize(Widget);
static void Redraw(Widget, XEvent *, Region);
static void Mappingaction(Widget, XEvent *, String *, Cardinal*);
static void Keyaction(Widget, XEvent *, String *, Cardinal*);
static void Mouseaction(Widget, XEvent *, String *, Cardinal*);
static String SelectSwap(Widget, String);

/* Data */

#define Offset(field) XtOffsetOf(GwinRec, gwin.field)

static XtResource resources[] = {
	{XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
		Offset(foreground), XtRString, (XtPointer)XtDefaultForeground},
	{XtNfont,  XtCFont, XtRFontStruct, sizeof(XFontStruct *),
		Offset(font),XtRString, (XtPointer)XtDefaultFont},
	{XtNscrollForwardR, XtCScrollForwardR, XtRBoolean, sizeof(Boolean),
		Offset(forwardr), XtRImmediate, (XtPointer)TRUE},
	{XtNreshaped, XtCReshaped, XtRFunction, sizeof(Reshapefunc),
		Offset(reshaped), XtRFunction, (XtPointer) NULL},
	{XtNgotchar, XtCGotchar, XtRFunction, sizeof(Charfunc),
		Offset(gotchar), XtRFunction, (XtPointer) NULL},
	{XtNgotmouse, XtCGotmouse, XtRFunction, sizeof(Mousefunc),
		Offset(gotmouse), XtRFunction, (XtPointer) NULL},
	{XtNselection, XtCSelection, XtRString, sizeof(String),
		Offset(selection), XtRString, (XtPointer) NULL},
	{XtNp9font, XtCP9font, XtRString, sizeof(String),
		Offset(p9font), XtRString, (XtPointer) NULL},
	{XtNp9fixed, XtCP9fixed, XtRString, sizeof(String),
		Offset(p9fixed), XtRString, (XtPointer) NULL},
	{XtNcomposeMod, XtCComposeMod, XtRInt, sizeof(int),
		Offset(compose), XtRImmediate, (XtPointer) 0}
};
#undef Offset

static XtActionsRec actions[] = {
	{"key", Keyaction},
	{"mouse", Mouseaction},
	{"mapping", Mappingaction}
};

static char tms[] =
	"<Key> : key() \n\
	<Motion> : mouse() \n\
	<BtnDown> : mouse() \n\
	<BtnUp> : mouse() \n\
	<Mapping> : mapping() \n";

/* Class record declaration */

GwinClassRec gwinClassRec = {
  /* Core class part */
   {
    /* superclass         */    (WidgetClass)&widgetClassRec,
    /* class_name         */    "Gwin",
    /* widget_size        */    sizeof(GwinRec),
    /* class_initialize   */    NULL,
    /* class_part_initialize*/  NULL,
    /* class_inited       */    FALSE,
    /* initialize         */    NULL,
    /* initialize_hook    */    NULL,
    /* realize            */    Realize,
    /* actions            */    actions,
    /* num_actions        */    XtNumber(actions),
    /* resources          */    resources,
    /* num_resources      */    XtNumber(resources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion    */    TRUE,
    /* compress_exposure  */    XtExposeCompressMultiple,
    /* compress_enterleave*/    TRUE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    Resize,
    /* expose             */    Redraw,
    /* set_values         */    NULL,
    /* set_values_hook    */    NULL,
    /* set_values_almost  */    XtInheritSetValuesAlmost,
    /* get_values_hook    */    NULL,
    /* accept_focus       */    XtInheritAcceptFocus,
    /* version            */    XtVersion,
    /* callback_offsets   */    NULL,
    /* tm_table           */    tms,
    /* query_geometry       */  XtInheritQueryGeometry,
    /* display_accelerator  */  NULL,
    /* extension            */  NULL
   },
  /* Gwin class part */
   {
    /* select_swap	  */    SelectSwap,
   }
};

/* Class record pointer */
WidgetClass gwinWidgetClass = (WidgetClass) &gwinClassRec;

static XModifierKeymap *modmap;
static int keypermod;

static void
Realize(Widget w, XtValueMask *valueMask, XSetWindowAttributes *attrs)
{
	XtValueMask		mask;

	*valueMask |= CWBackingStore;
	attrs->backing_store = Always;

	XtCreateWindow(w, InputOutput, (Visual *)0, *valueMask, attrs);
	XtSetKeyboardFocus(w->core.parent, w);
	if (modmap = XGetModifierMapping(XtDisplay(w)))
		keypermod = modmap->max_keypermod;

	Resize(w);
}

static void
Resize(Widget w)
{
	if(XtIsRealized(w))
		(*(XtClass(w)->core_class.expose))(w, (XEvent *)NULL, (Region)NULL);
}

static void
Redraw(Widget w, XEvent *e, Region r)
{
	Reshapefunc f;

	f = ((GwinWidget)w)->gwin.reshaped;
	if(f)
		(*f)(w->core.x, w->core.y,
			w->core.x+w->core.width, w->core.y+w->core.height);
}

static void
Mappingaction(Widget w, XEvent *e, String *p, Cardinal *np)
{
	if (modmap)
		XFreeModifiermap(modmap);
	modmap = XGetModifierMapping(e->xany.display);
	if (modmap)
		keypermod = modmap->max_keypermod;
}

#define STUFFCOMPOSE() \
				f = ((GwinWidget)w)->gwin.gotchar; \
				if (f) \
					for (c = 0; c < composing; c++) \
						(*f)(compose[c])

static void
Keyaction(Widget w, XEvent *e, String *p, Cardinal *np)
{
	static unsigned char compose[5];
	static int composing = -2;

	int c, minmod;
	KeySym k, mk;
	Charfunc f;
	Modifiers md;

	/*
	 * I tried using XtGetActionKeysym, but it didn't seem to
	 * do case conversion properly
	 * (at least, with Xterminal servers and R4 intrinsics)
	 */
	if(e->xany.type != KeyPress)
		return;
	XtTranslateKeycode(e->xany.display, (KeyCode)e->xkey.keycode,
		        e->xkey.state, &md, &k);
	/*
	 * The following song and dance is so we can have our chosen
	 * modifier key behave like a compose key, i.e, press and release
	 * and then type the compose sequence, like Plan 9.  We have
	 * to find out which key is the compose key first 'though.
	 */
	if (IsModifierKey(k) && ((GwinWidget)w)->gwin.compose
			&& composing == -2 && modmap) {
		minmod = (((GwinWidget)w)->gwin.compose+2)*keypermod;
		for (c = minmod; c < minmod+keypermod; c++) {
			XtTranslateKeycode(e->xany.display,
					modmap->modifiermap[c],
	        			e->xkey.state, &md, &mk);
			if (k == mk) {
				composing = -1;
				break;
			}
		}
		return;
	}
	/* Handle Multi_key separately, since it isn't a modifier */
	if(k == XK_Multi_key) {
		composing = -1;
		return;
	}
	if(k == NoSymbol)
		return;
	if(k&0xFF00){
		switch(k){
		case XK_BackSpace:
		case XK_Tab:
		case XK_Escape:
		case XK_Delete:
		case XK_KP_0:
		case XK_KP_1:
		case XK_KP_2:
		case XK_KP_3:
		case XK_KP_4:
		case XK_KP_5:
		case XK_KP_6:
		case XK_KP_7:
		case XK_KP_8:
		case XK_KP_9:
		case XK_KP_Divide:
		case XK_KP_Multiply:
		case XK_KP_Subtract:
		case XK_KP_Add:
		case XK_KP_Decimal:
			k &= 0x7F;
			break;
		case XK_Linefeed:
			k = '\r';
			break;
		case XK_KP_Enter:
		case XK_Return:
			k = '\n';
			break;
		case XK_Next:
			k = 0x80;	/* (VIEW- scroll down)*/
			break;
		case XK_Prior:
			k = 0x81; /* PREVIEW -- "Scroll back" */
			break;
		case XK_Left:
			k = 0x82;	/* LeftArrow */
			break;
		case XK_Right:
			k = 0x83;	/* RightArrow */
			break;
		case XK_Down:
			k = 0x84;	/* LeftArrow */
			break;
		case XK_Up:
			k = 0x85;	/* LeftArrow */
			break;
		case XK_Home:
			k = 0x86;	/* Home */
			break;
		case XK_End:
			k = 0x87;	/* End */
			break;
		default:
			return;	/* not ISO-1 or tty control */
		}
	}
	/* Compensate for servers that call a minus a hyphen */
	if(k == XK_hyphen)
		k = XK_minus;
	/* Do control mapping ourselves if translator doesn't */
	if((e->xkey.state&ControlMask) && !(md&ControlMask))
		k &= 0x9f;
	if(k == NoSymbol)
		return;
	/* Check to see if we are in a composition sequence */
	if (!((GwinWidget)w)->gwin.compose && (e->xkey.state & Mod1Mask)
			&& composing == -2)
		composing = -1;
	if (composing > -2) {
		compose[++composing] = k;
		if ((*compose == 'X') && (composing > 0)) {
			if ((k < '0') || (k > 'f') ||
					((k > '9') && (k < 'a'))) {
				STUFFCOMPOSE();
				c = (unsigned short)k;
				composing = -2;
			} else if (composing == 4) {
				c = (int)unicode(compose);
				if (c == -1) {
					STUFFCOMPOSE();
					c = (unsigned short)compose[4];
				}
				composing = -2;
			}
		} else if (composing == 1) {
			c = (int)latin1(compose);
			if (c == -1) {
				STUFFCOMPOSE();
				c = (unsigned short)compose[1];
			}
			composing = -2;
		}
	} else {
		if (composing >= 0) {
			composing++;
			STUFFCOMPOSE();
		}
		c = (unsigned short)k;
		composing = -2;
	}

	if (composing >= -1)
		return;

	f = ((GwinWidget)w)->gwin.gotchar;
	if(f)
		(*f)(c);
}

static void
LoseSel(Widget w, Atom *sel)
{
	GwinWidget gw = (GwinWidget)w;
	
	if(gw->gwin.selection){
		XtFree(gw->gwin.selection);
		gw->gwin.selection = 0;
	}
}

static void
Mouseaction(Widget w, XEvent *e, String *p, Cardinal *np)
{
	int s;
	XButtonEvent *be;
	XMotionEvent *me;
	Gwinmouse m;
	Mousefunc f;

	switch(e->type){
	case ButtonPress:
		be = (XButtonEvent *)e;
		m.xy.x = be->x;
		m.xy.y = be->y;
		m.msec = be->time;
		s = be->state;	/* the previous state */
		switch(be->button){
		case 1:	s |= Button1Mask; break;
		case 2:	s |= Button2Mask; break;
		case 3:	s |= Button3Mask; break;
		}
		break;
	case ButtonRelease:
		be = (XButtonEvent *)e;
		m.xy.x = be->x;
		m.xy.y = be->y;
		m.msec = be->time;
		s = be->state;
		switch(be->button){
		case 1:	s &= ~Button1Mask; break;
		case 2:	s &= ~Button2Mask; break;
		case 3:	s &= ~Button3Mask; break;
		}
		break;
	case MotionNotify:
		me = (XMotionEvent *)e;
		s = me->state;
		m.xy.x = me->x;
		m.xy.y = me->y;
		m.msec = me->time;
		break;
	default:
		return;
	}
	m.buttons = 0;
	if(s & Button1Mask) m.buttons |= 1;
	if(s & Button2Mask) m.buttons |= 2;
	if(s & Button3Mask) m.buttons |= 4;
	f = ((GwinWidget)w)->gwin.gotmouse;
	if(f)
		(*f)(&m);
}

static void
SelCallback(Widget w, XtPointer cldata, Atom *sel, Atom *seltype,
	XtPointer val, unsigned long *len, int *fmt)
{
	String s;
	int n;
	GwinWidget gw = (GwinWidget)w;

	if(gw->gwin.selection)
		XtFree(gw->gwin.selection);
	if(*seltype != XA_STRING)
		n = 0;
	else
		n = (*len) * (*fmt/8);
	s = (String)XtMalloc(n+1);
	if(n > 0)
		memcpy(s, (char *)val, n);
	s[n] = 0;
	gw->gwin.selection = s;
	XtFree(val);
}

static Boolean
SendSel(Widget w, Atom *sel, Atom *target, Atom *rtype, XtPointer *ans,
		unsigned long *anslen, int *ansfmt)
{
	GwinWidget gw = (GwinWidget)w;
	static Atom targets = 0;
	XrmValue src, dst;
	char *s;

	if(*target == XA_STRING){
		s = gw->gwin.selection;
		if(!s)
			s = "";
		*rtype = XA_STRING;
		*ans = (XtPointer) XtNewString(s);
		*anslen = strlen(*ans);
		*ansfmt = 8;
		return TRUE;
	}
#ifndef R3
	if(targets == 0){
		src.addr = "TARGETS";
		src.size = strlen(src.addr)+1;
		dst.size = sizeof(Atom);
		dst.addr = (XtPointer) &targets;
		XtConvertAndStore(w, XtRString, &src, XtRAtom, &dst);
	}
	if(*target == targets){
		*rtype = XA_ATOM;
		*ans = (XtPointer) XtNew(Atom);
		*(Atom*) *ans = XA_STRING;
		*anslen = 1;
		*ansfmt = 32;
		return TRUE;
	}
#endif
	return FALSE;
}

static String
SelectSwap(Widget w, String s)
{
	GwinWidget gw;
	String ans;

	gw = (GwinWidget)w;

	if(!gw->gwin.selection){
#ifdef R3
	XtGetSelectionValue(w, XA_PRIMARY, XA_STRING, SelCallback, 0,
			CurrentTime);
#else
	XtGetSelectionValue(w, XA_PRIMARY, XA_STRING, SelCallback, 0,
			XtLastTimestampProcessed(XtDisplay(w)));
#endif
	while(gw->gwin.selection == 0)
		XtAppProcessEvent(XtWidgetToApplicationContext(w) , XtIMAll);
	}
	ans = gw->gwin.selection;
	gw->gwin.selection = XtMalloc(strlen(s)+1);
	strcpy(gw->gwin.selection, s);
#ifdef R3
	XtOwnSelection(w, XA_PRIMARY, CurrentTime, SendSel, LoseSel, NULL);
#else
	XtOwnSelection(w, XA_PRIMARY, XtLastTimestampProcessed(XtDisplay(w)),
			SendSel, LoseSel, NULL);
#endif
	return ans;
}

/* The returned answer should be free()ed when no longer needed */
String
GwinSelectionSwap(Widget w, String s)
{
	XtCheckSubclass(w, gwinWidgetClass, NULL);
	return (*((GwinWidgetClass) XtClass(w))->gwin_class.select_swap)(w, s);
}

static void
own_selection(Widget w)
{
#ifdef R3
	XtOwnSelection(w, XA_PRIMARY, CurrentTime, SendSel, LoseSel, NULL);
#else
	XtOwnSelection(w, XA_PRIMARY, XtLastTimestampProcessed(XtDisplay(w)),
			SendSel, LoseSel, NULL);
#endif
}

static void
get_selection(Widget w)
{
#ifdef R3
	XtGetSelectionValue(w, XA_PRIMARY, XA_STRING, SelCallback, 0,
			CurrentTime);
#else
	XtGetSelectionValue(w, XA_PRIMARY, XA_STRING, SelCallback, 0,
			XtLastTimestampProcessed(XtDisplay(w)));
#endif
}

/* Get current selection. Returned string isn't yours to free or
 * keep - copy it and forget it.
 */
char*
Gwinselect_get(Widget w)
{
	GwinWidget gw = (GwinWidget)w;

	if(!gw->gwin.selection){
		get_selection(w);
		while(gw->gwin.selection == 0)
			XtAppProcessEvent(XtWidgetToApplicationContext(w) , XtIMAll);
	}
	own_selection(w);
	return gw->gwin.selection;
}

/* Set current selection to 's' */
void
Gwinselect_put(Widget w,char*s)
{
	GwinWidget gw = (GwinWidget)w;
	if(gw->gwin.selection)
		XtFree(gw->gwin.selection);
	gw->gwin.selection = XtMalloc(strlen(s)+1);
	strcpy(gw->gwin.selection, s);
	own_selection(w);
}

