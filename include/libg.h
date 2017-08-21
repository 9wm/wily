/* Copyright (c) 1992 AT&T - All rights reserved. */
#ifndef _LIBG_H
#define _LIBG_H

/*
 *  Like Plan9's libg.h, but suitable for inclusion on non-Plan9 machines
 */

enum{ EMAXMSG = 128+8192 };	/* max event size */

/*
 * Types
 */

typedef	struct	Bitmap		Bitmap;
typedef struct	Point		Point;
typedef struct	Rectangle 	Rectangle;
typedef struct	Cursor		Cursor;
typedef struct	Mouse		Mouse;
typedef struct	Menu		Menu;
typedef struct	Font		Font;
typedef struct	Fontchar	Fontchar;
typedef struct	Subfont		Subfont;
typedef struct	Cachesubf	Cachesubf;
typedef struct	Event		Event;
typedef struct	RGB		RGB;

struct	Point
{
	int	x;
	int	y;
};

struct Rectangle
{
	Point min;
	Point max;
};

struct	Bitmap
{
	Rectangle r;		/* rectangle in data area, local coords */
	Rectangle clipr;	/* clipping region */
	int	ldepth;
	int	id;		/* as known by the X server */
	Bitmap	*cache;		/* zero; distinguishes bitmap from layer */
	int	flag;		/* flag used by X implementation of libg */
};

struct	Mouse
{
	int		buttons; /* bit array: LMR=124 */
	Point		xy;
	unsigned long	msec;
};

struct	Cursor
{
	Point		offset;
	unsigned char	clr[2*16];
	unsigned char	set[2*16];
	int		id;	/* init to zero; used by library */
};

struct Menu
{
	char	**item;
	char	*(*gen)(int);
	int	lasthit;
};

/*
 * Subfonts:
 *
 * The "true width", cwidth, is the sum of the left and right
 * bearings of the character - the number of pixels actually
 * occupied by the glyph.  The width, supplied by the server and
 * stored in field "width", is the number of pixels to advance to
 * the right beyond the origin of the current character to the origin
 * of the next character.
 */

struct	Fontchar
{
	short		cwidth;		/* width of glyph */
	unsigned char	top;		/* first non-zero scan-line */
	unsigned char	bottom;		/* last non-zero scan-line */
	signed char	left;		/* offset of baseline */
	unsigned char	width;		/* advance to next char's origin */
};

struct	Subfont
{
	short		minrow;	/* first character row in font (for X subfonts) */
	short		mincol;	/* first character col in font (for X subfonts) */
	short		minchar; /* first char code in subfont */
	short		maxchar; /* last char code in subfont */
	short		width;	/* number of chars in row */
	short		n;	/* number of chars in font */
	unsigned char	height;	/* height of bitmap */
	char		ascent;	/* top of bitmap to baseline */
	Fontchar	*info;	/* n+1 character descriptors */
	int		id;	/* of font */
};

struct	Cachesubf
{
	Rune		min;	/* rune value of 0th char in subfont */
	Rune		max;	/* rune value+1 of last char in subfont */
	char		*name;
	Subfont		*f;	/* attached subfont */
};

struct Font
{
	char		*name;
	unsigned char	height;	/* max height of bitmap, interline spacing */
	char		ascent;	/* top of bitmap to baseline */
	char		width;	/* widest so far; used in caching only */
	char		ldepth;	/* of images */
	short		id;	/* of font */
	unsigned short		nsubf;	/* number of subfonts */
	Cachesubf	*subf;	/* as read from file */
};

struct	Event
{
	int		kbdc;
	Mouse		mouse;
	int		n;		/* number of characters in mesage */
	unsigned char	data[EMAXMSG];	/* message from an arbitrary file descriptor */
};

struct RGB
{
	unsigned long	red;
	unsigned long	green;
	unsigned long	blue;
};

/*
 * Codes for bitblt etc.
 *
 *	       D
 *	     0   1
 *         ---------
 *	 0 | 1 | 2 |
 *     S   |---|---|
 * 	 1 | 4 | 8 |
 *         ---------
 *
 *	Usually used as D|S; DorS is so tracebacks are readable.
 */
typedef
enum	Fcode
{
	Zero		= 0x0,
	DnorS		= 0x1,
	DandnotS	= 0x2,
	notS		= 0x3,
	notDandS	= 0x4,
	notD		= 0x5,
	DxorS		= 0x6,
	DnandS		= 0x7,
	DandS		= 0x8,
	DxnorS		= 0x9,
	D		= 0xA,
	DornotS		= 0xB,
	S		= 0xC,
	notDorS		= 0xD,
	DorS		= 0xE,
	F		= 0xF
} Fcode;

/*
 * Miscellany
 */

typedef void	 (*Errfunc)(char *);

extern Point	 add(Point, Point);
extern Point	 sub(Point, Point);
extern Point	 mul(Point, int);
extern Point	 divpt(Point, int);
extern Rectangle rsubp(Rectangle, Point);
extern Rectangle raddp(Rectangle, Point);
extern Rectangle inset(Rectangle, int);
extern Rectangle rmul(Rectangle, int);
extern Rectangle rdiv(Rectangle, int);
extern Rectangle rshift(Rectangle, int);
extern int	rectinrect(Rectangle , Rectangle );
extern Rectangle rcanon(Rectangle);
extern Bitmap*	 balloc(Rectangle, int);
extern void	 bfree(Bitmap*);
extern int	 rectclip(Rectangle*, Rectangle);
extern void	 xtbinit(Errfunc, char*, int*, char**, char**);
extern void	 bclose(void);
extern void	 berror(char*);
extern void	 bitblt(Bitmap*, Point, Bitmap*, Rectangle, Fcode);
extern void	 copymasked(Bitmap*, Point, Bitmap*, Bitmap*, Rectangle);
extern int	 bitbltclip(void*);
extern Subfont*	 getsubfont(char*);
extern Font	*rdfontfile(char*, int);
extern void	 ffree(Font*);
extern Font	*mkfont(Subfont*);
extern void	 subffree(Subfont*);
extern int	 cachechars(Font*, char**, void*, int, int*, unsigned short*);
extern Point	 string(Bitmap*, Point, Font*, char*, Fcode);
extern void	 segment(Bitmap*, Point, Point, int, Fcode);
extern void	 point(Bitmap*, Point, int, Fcode);
extern void	 arc(Bitmap*, Point, Point, Point, int, Fcode);
extern void	 circle(Bitmap*, Point, int, int, Fcode);
extern void	 disc(Bitmap*, Point, int, int, Fcode);
extern void	 ellipse(Bitmap*, Point, int, int, int, Fcode);
extern void	 polysegment(Bitmap *, int, Point *, int, Fcode);
extern long	 strwidth(Font*, char*);
extern Point	 strsize(Font*, char*);
extern long	 charwidth(Font*, Rune);
extern void	 texture(Bitmap*, Rectangle, Bitmap*, Fcode);
extern void	 wrbitmap(Bitmap*, int, int, unsigned char*);
extern void	 rdbitmap(Bitmap*, int, int, unsigned char*);
extern void	 wrbitmapfile(int, Bitmap*);
extern Bitmap*	 rdbitmapfile(int);
extern int	 ptinrect(Point, Rectangle);
extern int	 rectXrect(Rectangle, Rectangle);
extern int	 eqpt(Point, Point);
extern int	 eqrect(Rectangle, Rectangle);
extern void	 border(Bitmap*, Rectangle, int, Fcode);
extern void	 cursorswitch(Cursor*);
extern void	 cursorset(Point);
extern Rectangle bscreenrect(Rectangle*);
extern void	 bflush(void);
extern int	 clipline(Rectangle, Point*, Point*);
extern int	 clipr(Bitmap*, Rectangle);
extern int	 scrpix(int*,int*);

extern void	 einit(unsigned long);
extern unsigned long estart(unsigned long, int, int);
extern void estop(unsigned long);
extern unsigned long etimer(unsigned long, long);
extern void estoptimer(unsigned long);
extern unsigned long event(Event*);
extern unsigned long eread(unsigned long, Event*);
extern Mouse	 emouse(void);
extern int	 ekbd(void);
extern int	 ecanread(unsigned long);
extern int	 ecanmouse(void);
extern int	 ecankbd(void);
extern void	 ereshaped(Rectangle);	/* supplied by user */
extern void	 eflush(unsigned long);
extern int	 menuhit(int, Mouse*, Menu*);
extern Rectangle getrect(int, Mouse*);
extern unsigned long rgbpix(Bitmap*, RGB);
extern void	 rdcolmap(Bitmap*, RGB*);
extern void	 wrcolmap(Bitmap*, RGB*);

/* Extra functions supplied by libXg */
extern int	snarfswap(char*, int, char**);
char*		select_get(void);
void			select_put(char*);
extern int	scrollfwdbut(void);

enum{
	Emouse		= 1,
	Ekeyboard	= 2
};

extern Point	 Pt(int, int);
extern Rectangle Rect(int, int, int, int);
extern Rectangle Rpt(Point, Point);


#define	Dx(r)	((r).max.x-(r).min.x)
#define	Dy(r)	((r).max.y-(r).min.y)

extern	Bitmap	screen;
extern	Font	*font, *fixed;

#define	BGSHORT(p)		(((p)[0]<<0) | ((p)[1]<<8))
#define	BGLONG(p)		((BGSHORT(p)<<0) | (BGSHORT(p+2)<<16))
#define	BPSHORT(p, v)		((p)[0]=(v), (p)[1]=((v)>>8))
#define	BPLONG(p, v)		(BPSHORT(p, (v)), BPSHORT(p+2, (v)>>16))

#endif
