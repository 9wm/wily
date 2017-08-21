/* see ../Doc/C.html 
 */

typedef struct Msg Msg;
typedef int Id;		/* Window identifier */
typedef enum Bool {false, true} Bool;
typedef struct Range Range;

/* name of environment variable */
#define WILYFIFO "WILYFIFO"

enum {
	HSIZE= 12,	/* minimum length of a message fragment to be able
				to determine the length of the full message. */
	MAXERRMSG = 1024	/* maximum length of an error message */
};

typedef enum Mtype {

	/* EVENTS:  also used for event masks. */
	WEexec = 1,
	WEgoto = 2,
	WEdestroy = 4,
	WEreplace = 8,

	WEfencepost,

	/* REQUESTS AND RESPONSES */

	WRerror,
	WMlist, WRlist,
	WMnew, WRnew,
	WMattach, WRattach,
	WMsetname, WRsetname,
	WMgetname, WRgetname,
	WMsettools,WRsettools,
	WMgettools,WRgettools,
	WMread, WRread,
	WMreplace, WRreplace,
	WMexec,	 WRexec,
	WMgoto,	 WRgoto,

	WMfencepost	
} Mtype;

struct Range {
	ulong	p0,p1;
};

#define RLEN(r) ((r).p1 -(r).p0)
#define RCONTAINS(r, p) ( ((p)>=(r).p0) && ((p) < (r).p1) )
#define RINTERSECT(r0, r1) ( ((r0).p1 >= (r1).p0) &&  ((r0).p0 <= (r1).p1) )
#define RISSUBSET(q,r) (	((q).p0 >= (r).p0) && ((q).p1 <= (r).p1)	)
#define ROK(r) ((r).p1 >= (r).p0)

struct Msg {
	Mtype	t;
	Id		m;		/* message */	
	Id		w;		/* window */	
	Range	r;
	ushort	flag;
	char	*	s;
};

/* ../libmsg/connect.c */

int		client_connect	(void);
int		wilyfifolisten	(void);
int		wilyfifotalk	(void);
void		fifo_cleanup	(void);
int		wily_connect	(char*,int);

/* ../libmsg/msg.c */

int		msg_size		(Msg *m);
void		msg_flatten	(Msg*, uchar*);
int		msg_init		(Msg*, uchar*);
ulong	msg_bufsize	(uchar*);
void		msg_print	(Msg *);
void		msg_fill		(Msg*, Mtype , Id w, Range r, ushort flag, char*s) ;
#define FULLMSG(ptr,n) ( (n) >= HSIZE && (n) >= msg_bufsize(ptr) )

/* ../libmsg/rpc.c */
typedef struct Handle Handle;
extern Range	nr;		/* null range */

Handle*	rpc_init		(int);
int		rpc_fileno(Handle *h);
Bool		rpc_isconnected(Handle*);
void		rpc_freehandle (Handle*);

char*	rpc_list		(Handle*h, char **bufptr);
char*	rpc_new		(Handle*, char *, Id*, ushort);
char*	rpc_attach	(Handle*, Id, ushort);
char*	rpc_setname	(Handle*, Id, char *);
char*	rpc_getname	(Handle*, Id, char **);
char*	rpc_settools	(Handle*, Id, char *);
char*	rpc_gettools	(Handle*, Id, char **);
char*	rpc_read		(Handle*, Id, Range, char*);
char*	rpc_replace	(Handle*, Id, Range, char*);
char*	rpc_exec		(Handle*, Id , char *);
char*	rpc_goto		(Handle*, Id * , Range*, char*, Bool);

int		rpc_event	(Handle*, Msg *);
int		rpc_bounce	(Handle*, Msg *);
Bool		rpc_wouldblock(Handle*);


#define MAX(a,b)(((a)>(b))?(a):(b))
#define MIN(a,b)(((a)<(b))?(a):(b))
#define NEW(t) ((t*)salloc(sizeof(t)))

/* ../libmsg/util.c */
int		clip(int , int , int );
void		eprintf(char *, ...);
void *	salloc(int );
void *	srealloc(void *, int);
Range		intersect (Range, Range);
Range		rclip (Range, Range);
ulong	pclipr(ulong p, Range r);
Range	range(ulong , ulong );
Range	maybereverserange(ulong,ulong);
ulong	ladjust(ulong , Range , int );
ulong	radjust(ulong , Range , int );
