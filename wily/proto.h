/*******************************************
 *	Declarations of major functions and macros
 *******************************************/

#ifndef SAM_CODE
void	error(char *, ...);
#endif
void fatal(char*,...);

/* builtins.c */
Bool	builtin			(View *, char *, char*);

/* click.c */
Range	text_doubleclick	(Text *, ulong);
Range	text_expand		(Text *, Range, char *);
ulong	text_startofword	(Text *, ulong);

/* col.c */
Tile*		tile_col			(Tile*);
void		col_new			(View*v, char *arg);
void		col_del		(Tile*);

/* data.c */
Text*	data_body	(Data*);
Text*	data_tag		(Data*);
char**	data_names	(Data*d);
Bool		data_isdirty	(Data *d);
int		data_putall	(void);
int		data_backupall (void);
int		data_put		(Data *d, char *path);
int		data_del		(Data*);
int		data_backup	(Data *d);

/* env.c */
void		env_init		(char **);
void		pathcontract	(char*, char *);

/* event.c */
void		event_wellknown	(int);
int		event_outputstart	(int , int , char*, char*, View *);
void		keytab_init		(void);
void		dofd				(ulong, int, char*);
void		kill_all			(char *s);
void		kill_list			(void);

/* exec.c */
void		ex_init			(void);
void		run				(View *, char *, char *);

/* file.c */
View *	data_open		(char*, Bool);
int		data_get			(Data *d, char *);

/* include.c */
View*	openinclude		(View *v, Range r);

/* keyboard.c */
void		dokeyboard		(View *, Rune);
void		view_pagedown	(View *, Bool );
void		view_linesdown	(View *v, int n, Bool down);

/* label.c */
void		data_addcontext	(Data*, char*, char*);
void		data_getlabel		(Data*, char*);
void		data_setlabel		(Data*, char *);
View *	data_find			(char*);

/* line.c */
int		text_linenumber	(Text *t, ulong );
Bool		text_findline		(Text *, Range *, ulong);
Range	text_lastline		(Text *);
ulong	text_nl			(Text *, ulong, int);
ulong	text_startOfLine	(Text *, ulong);

/* mouse.c */
void		domouse			(View*, Mouse*);

/* msg.c */
void		mbuf_init		(Mbuf*);
void		mbuf_clear		(Mbuf*);
int		partialmsg		(Mbuf *, int , int , char*);
Bool		data_sendreplace	(Data *,Range r, Rstring );
Bool		data_sendgoto		(Data *,Range r, char *);
Bool		data_sendexec		(Data *,char*, char *);
void		data_fdstop		(int );

/* path.c */
void		labelclean			(char*);
void		label2path		(char*,char*);
void		envexpand		(char*, char*);
View*	openlabel			(char*,Bool);

/* place.c */
Tile*		findcol			(char*);
void		placedcol			(char*, Tile*);

/* point.c */
Point	buttonpos		(Tile*tile);

/* sam.c */
Bool		text_regexp		(Text *, Rstring , Range*, Bool );
Bool		text_utfregexp	(Text *, char*, Range*, Bool );
long		Tchars			(Text *, Rune *, ulong, ulong);

/* scroll.c */
void		scroll_update		(Scroll *);
void		scroll_init		(void);
Scroll *	scroll_alloc		(Bitmap *, Rectangle);
void		scroll_setrects		(Scroll*, Bitmap *, Rectangle);
void		scroll_set			(Scroll *, ulong , ulong , ulong );

/* search.c */
Bool		text_look			(Text*, Range*, Range);
Bool		text_findliteralutf	(Text*, Range*, char*);
Bool		text_findwordutf	(Text*, Range*, char*);
Bool		text_findliteral	(Text*, Range*, Rstring);
Bool		text_findword		(Text*, Range*, Rstring);
Bool		text_search		(Text*, Range*, char *, Range);

/* select.c */
Range		vselect		(View *, Mouse *);

/* tag.c */
void		tag_set			(Text *, char*s);
void		tag_rmtool		(Text *, char *);
void		tag_addtool		(Text *, char *);
void		tag_modified		(Text *, ulong);
void		tag_reset			(Text *);
void		tag_setlabel		(Text *, char *);
void		tag_settools		(Text *, char *);
char*	tag_gettools		(Text *);
void		tag_addrunning	(Text *t, char *cmd);

/* tagmatch.c */
void		tag_init			(char *filename);
char*	tag_match		(char*label);

/* text.c */
int		text_read			(Text *, int fd, int len);
int		text_write_range	(Text *, Range, int);
void		text_copy		(Text *, Range, Rune *);
Text*	text_alloc			(Data *, Bool);
Range	text_replace		(Text *, Range, Rstring);
void		text_free			(Text *);

/* text2.c */
void		text_allread 		(Text*t);

Data*	text_data			(Text*);
View*	text_view		(Text*t);
ulong	text_length		(Text*);
Bool		text_needsbackup	(Text*);

View*	text_body 		(Text*);

void		text_setneedsbackup(Text*t, Bool b);
Bool		text_badrange		(Text *t, Range r);
void		text_addview		(Text*, View*);
int		text_rmview		(Text*, View*);

int		text_write		(Text *, char *fname);
int		text_fd			(Text*, Range);

Rstring	text_autoindent	(Text *t, ulong p);

int		back_height		(Text *t, ulong, Font *, int, int);
Range	text_all			(Text*t);
void		text_fillbutton	(Text*t, Fcode f);
ulong	text_ncopy		(Text *, Rune*, ulong , ulong );
Range	text_replaceutf	(Text*, Range, char*);
char*	text_duputf		(Text*, Range);
int		text_copyutf		(Text *, Range, char *);

Bool		text_refreshdir	(Text*t);
void		text_formatdir		(Text *t, char**);

/* tile.c */
Bool		tile_hidden		(Tile*tile);
View*	tile_body			(Tile*tile);
void		ereshaped		(Rectangle);
void		tile_del			(Tile *);
void		tile_grow			(Tile *, int );
void		tile_move		(Tile *, Point);
void		tile_setfont		(Tile*, char*);
void		tile_show		(Tile *);
View*	tile_tag			(Tile*);
void		wily_init			(void);

/* undo.c */
void		undo_record		(Text*, Range, Rstring);
Range	undo_undo		(Text*, Bool);
Range	undo_redo		(Text*, Bool);
void		undo_free		(Text*);
void		undo_reset		(Text*);
void		undo_start		(Text*);
void		undo_break		(Text*);
void		undo_mark		(Text*);
Bool		undo_atmark		(Text*);

/* util.c */
void		dirnametrunc		(char*);
void		addcontext		(char*, char*, char*);
void		olabel			(char*out, char*label);
int		statcmp			(Stat*a, Stat*b);
Bool		isdir				(char*path);
int		backup_name		(char *orig, char *back);
char *	columnate		(int, int, Font *, char **);
void		noutput			(char *context, char *base, int n);
void		add_slash			(char*);
int		distance			(Point , Point );
Bool		frame_isfull		(Frame*);
void		frgetmouse		(void);
char*	mybasename		(char*f);
Range	paste			(Text *, Range);
Rstring	rstring			(Rune*r0, Rune*r1);
char*	select_get		(void);
void		select_put		(char*);
void		snarf			(Text *, Range);
int		stripnulls			(char *buf, int len);
ulong	texttoutf			(char *, Rune *, Rune *);
ulong	utftotext			(Rune *, char *, char *);
int 		diag				(char*,char*, ...);
void		cleanup_and_abort	(int);
void		cleanup_and_die	(int);
int		fillutfchar		(char*,int,char*,int);
int		unfullutfbytes		(char*,int);
Rstring 	utf2rstring		(char*utf);

/* vgeom.c */
void		filled			(View*, Range );
void		extend_selection	(View*, Range );
void		view_fillbutton	(View*v, Fcode f);
void 	fill				(View*);
void 	view_setscroll		(View*);
int		snapheight		(View*v, int h);
void		view_reshaped	(View*, Rectangle);
int		view_lastlinepos	(View*v);
int		view_stripwhitespace (View*v);

/* view.c */
void		view_getdot		(View *v, char*buf, Bool isLine);
void		view_paste		(View*v);
Range	view_expand		(View *v, Range r, char *s);
View*	view_new		(Font *, Bool, Text *, Tile *);
int		view_delete		(View *v);
Bool		view_invariants	(View*v);

Data*	view_data		(View*);
View*	view_body		(View*);
Tile*		view_win			(View*);
Tile*		view_tile			(View*);
Text*	view_text		(View*);
Range	view_getsel		(View*);
int		view_height		(View*);

void		view_cut			(View*, Range r);
void		view_append		(View*, char *, int);
void		view_pipe		(View*, Bool *, char *, int);
void		view_setfont		(View*, char*arg);
void		view_border		(View*, Bool);
void		view_select		(View*, Range);
void		view_warp		(View*, Range);
void		view_setlastselection(View *v);
void		viewlist_refresh	(View*);
void		viewlist_replace	(View*, Range, Rstring);

/* vsearch.c */
void		b3				(View*, Range r);
void		view_look		(View*, char*);
Bool		view_goto		(View**, Range *r, char *s);

/* vshow.c */
void		view_show		(View*, Range);
void		view_scroll		(View *, Mouse *);
void		view_hscroll		(View*v, Bool left);

/* wily.c */
void		addrunning		(char *cmd);
void		rmrunning		(char *cmd);

/* win.c */
int		win_del			(Tile*);
Tile*		tile_win			(Tile*);
void 	win_clone		(Tile*);
void		win_anchor		(Tile*, char *);
void		win_new			(char*, Text*, Text*);

#define RECTOK(r) (Dx(r)>=0 && Dy(r) >= 0)
#define cls(r) bitblt(&screen, (r).min, &screen, (r), Zero)
#define SETSIZE(buf, n, desired) {if(n<desired){ n=(desired)*1.5; buf = srealloc(buf, n);}}
#define STRSAME !strcmp
