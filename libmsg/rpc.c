#include <u.h>
#include <libc.h>
#include <msg.h>
#include <assert.h>

/* see ../include/msg.h */

/* Send messages, receive replies, queue events until we're asked for them.
 */

enum {
	Qmax = 20
};
typedef struct MsgQ MsgQ;
typedef struct MsgSet MsgSet;

struct MsgQ {
	Msg	msg[Qmax];
	int	read,write;
};

static void	msgq_init	(MsgQ*);
static Bool	msgq_empty	(MsgQ*);
static Bool	msgq_full	(MsgQ*);
static Msg	msgq_pop	(MsgQ*);
static void	msgq_push	(MsgQ*,Msg);

struct MsgSet {
	Msg	msg		[Qmax];
	Bool	inuse	[Qmax];
	int	n;
};

static void msgset_init	(MsgSet*);
static Bool msgset_full	(MsgSet*);
static Bool msgset_find	(MsgSet*, Id, Msg*);
static void msgset_push	(MsgSet*,Msg);

struct Handle {
	int	fd;	/* connection to wily */
	char	err[MAXERRMSG];
	Id	id;	/* of last message */

	/* temporary buffer for packing and unpacking messages */
	char	*buf;
	int	n;
	int	alloced;

	MsgQ	event;
	MsgSet	reply;
};

static char* 	rpc		(Handle*, Msg*);
static int		getbytes	(Handle*);
static char*	simple(Handle*h, Mtype t, Id w, Range r, char *s,ushort);

Range nr = {0,0};

/* PRE: fd an open connection to wily
 * POST: return a handle ready for RPCs
 */
Handle*
rpc_init(int fd)
{
	Handle	*h = NEW(Handle);

	assert(fd>=0);

	h->fd = fd;
	h->alloced = 128;
	h->buf = salloc(h->alloced);
	h->n = 0;
	h->id = 0;
	msgset_init(&h->reply);
	msgq_init(&h->event);	

	return h;
}

int
rpc_fileno(Handle *h)
{
	return h->fd;
}

Bool
rpc_isconnected(Handle*h)
{
	return h->fd>=0;
}

void
rpc_freehandle(Handle*h)
{
	free(h->buf);
	close(h->fd);
	free(h);
}

Bool
rpc_wouldblock(Handle*h)
{
	return msgq_empty(&h->event);
}

/* Get the next event pending on 'h', store it in 'm'
 * Return 0 for success, -1 if we've lost the connection.
 */
int
rpc_event(Handle*h, Msg *m)
{
	while (msgq_empty(&h->event))
		if(getbytes(h))
			return -1;
	*m = msgq_pop(&h->event);
	return 0;
}

/* Bounce an event back: we didn't want it.
 */
int
rpc_bounce(Handle*h, Msg *m)
{
	int	size;

	if (m->t > WEfencepost) {
		fprintf(stderr,"can only bounce events\n");
		return -1;
	}
	
	/* send the request */
	size = msg_size(m);
	if (h->n + size > h->alloced) {
		h->alloced = h->n + size + 128;
		h->buf = srealloc(h->buf, h->alloced);
	}
	msg_flatten(m, h->buf + h->n);
	if(write(h->fd, h->buf+h->n, size) != size){
		perror ("rpc write");
		close(h->fd);
		h->fd = -1;
		return -1;
	}
	return 0;
}

char*
rpc_fill(Handle*h, Msg *msg, Mtype t, Id w, Range r, ushort flag, char*s) {
	msg_fill(msg, t, w, r, flag, s);
	return rpc(h, msg);
}

/* Each of the rpc_* functions returns either 0 for success,
 * or an error message in static storage, which can be used until
 * you call another function using the same handle.
 */

char*
rpc_list(Handle*h, char **bufptr)
{
	Msg	m;
	char	*err;

	if (!(err=rpc_fill(h,&m, WMlist, 0, nr, 0, 0)))
		*bufptr = m.s;
	return err;
}

char*
rpc_new(Handle*h, char *s, Id*id, ushort backup )
{
	Msg	m;
	char	*err;

	if (!(err=rpc_fill(h,&m, WMnew, 0, nr, backup, s))) {
		*id = m.w;
		free(m.s);
	}
	return err;
}

char*
rpc_read(Handle*h, Id w, Range r, char*buf)
{
	Msg	m;
	char	*err;

	if (!(err=rpc_fill(h,&m, WMread, w, r, 0, 0))) {
		strcpy(buf, m.s);
		free(m.s);
	}
	return err;
}

char*
rpc_goto(Handle*h, Id *w, Range*r, char*s, Bool setdot)
{
	Msg	m;
	char	*err;

	if (!(err=rpc_fill(h,&m, WMgoto, *w, *r, setdot, s))) {
		*w = m.w;
		*r = m.r;
		free(m.s);
	}
	return err;
}

char*
rpc_getname(Handle*h, Id w, char**s){
	Msg	m;
	char	*err;

	if (!(err=rpc_fill(h,&m, WMgetname, w, nr, 0,0))) {
		*s = m.s;	/* let the client free it later */
	}
	return err;
}

char*
rpc_gettools(Handle*h, Id w, char**s) {
	Msg	m;
	char	*err;

	if (!(err=rpc_fill(h,&m, WMgettools, w, nr, 0,0))) {
		*s = m.s;	/* let the client free it later */
	}
	return err;
}

char*
rpc_replace(Handle*h, Id w, Range r, char*s)
{
	return simple(h, WMreplace, w, r, s, 0);
}

char*
rpc_exec(Handle*h, Id w, char *s)
{
	return simple(h, WMexec, w, nr, s, 0);
}

char*
rpc_attach(Handle*h, Id w, ushort mask)
{
	return simple(h, WMattach, w, nr, 0, mask);
}

char*
rpc_setname(Handle*h, Id w, char *name)
{
	return simple(h, WMsetname, w, nr, name, 0);
}

char*
rpc_settools(Handle*h, Id w, char *name)
{
	return simple(h, WMsettools, w, nr, name, 0);
}

/************************************************************/

/* rpc(h, &m)
	if successful, return 0, fill in m, m->s will need to be freed
	if not, return a static string, m contains garbage.
 */
static char*
rpc(Handle*h, Msg*m)
{
	int	size;

	if(h->fd<0)
		return "handle not connected";

	m->m  =  h->id++;	/* message sequence */

	size = msg_size(m);
	if (h->n + size > h->alloced) {
		h->alloced = h->n + size + 128;
		h->buf = srealloc(h->buf, h->alloced);
	}

	/* send the request */
	msg_flatten(m, h->buf + h->n);
	if(write(h->fd, h->buf+h->n, size) != size){
		perror ("rpc write");
		close(h->fd);
		h->fd = -1;
	}

	/* wait for the reply */
	while (!msgset_find(&h->reply, m->m, m))
		if(getbytes(h))
			return "lost connection";

	if (m->t == WRerror) {
		strcpy(h->err, m->s);
		free(m->s);
		return h->err;
	} else
		return 0;
}

/* RPC which doesn't need to return any more information
 * than an error message.
 */
static char*
simple(Handle*h, Mtype t, Id w, Range r, char *s, ushort flag)
{
	char	*err;
	Msg	m;

	m.t = t;
	m.w = w;
	m.r = r;
	m.s = s;
	m.flag = flag;

	if (!(err=rpc(h,&m)))
		free(m.s);
	return err;
}

/* Read from the fd, form some messages, store them.
 * Returns 0 for success, -1 if we've lost the connection somehow.
 */
static int
getbytes	(Handle*h)
{
	int	left,nread;
	char	*ptr;
	int	size;

	/* try to fill our buffer */
	if(h->fd<0)
		return -1;	/* not connected */

	if ((nread = read(h->fd, h->buf + h->n, h->alloced - h->n)) < 1)
		return -1;
	h->n += nread;

	/* interpret any full messages */
	ptr = h->buf;
	left = h->n;

	while(FULLMSG(ptr, left)) {
		Msg	m;

		size = msg_bufsize(ptr);

		/* eat one message */
		if(msg_init(&m, ptr)) {
			perror("msg_init failed:  closing connection");
			close(h->fd);
			h->fd = -1;	/* poison */
			return -1;
		}
		ptr += size;
		left -= size;
		m.s = strdup(m.s);

		/* put it somewhere */
		if (m.t < WEfencepost) {
			MsgQ	*q = &h->event;
			
			if (msgq_full(q)) {
				fprintf(stderr, "rpc event queue overflow\n");
				free(m.s);
			} else {
				msgq_push(q,m);
			}
		} else {
			MsgSet	*set= &h->reply;

			if (msgset_full(set)) {
				fprintf(stderr, "rpc reply set overflow\n");
				free(m.s);
			} else {
				msgset_push(set,m);
			}
		}
	}

	/* move any partial messages to front of buffer */
	if (left && left != h->n)
		memmove(h->buf, ptr, left);
	h->n = left;

	/* make sure we have room for the next message */
	if (left > HSIZE) {
		size = msg_bufsize(h->buf);
		if (size > h->alloced) {
			h->alloced = size + 128;
			h->buf = srealloc(h->buf, h->alloced);
		}
	}
	return 0;
}

/**********************************
	 MsgQ functions
***********************************/
static void
msgq_init(MsgQ *q)
{
	q->read= q->write = 0;
}

static Bool
msgq_full(MsgQ *q)
{
	return (q->write +1) % Qmax == q->read;
}

static Bool
msgq_empty(MsgQ *q)
{
	return q->write == q->read;
}

static Msg
msgq_pop(MsgQ *q)
{
	Msg	m;

	assert(!msgq_empty(q));

	m = q->msg[q->read];
	q->read = (q->read+1) % Qmax;
	return m;
}

static void
msgq_push(MsgQ *q, Msg m)
{
	assert(!msgq_full(q));

	q->msg[q->write] = m;
	q->write = (q->write+1) % Qmax;
}

/**********************************
	 MsgSet functions
***********************************/
static void
msgset_init(MsgSet *s)
{
	int	j;
	for(j=0; j< Qmax; j++)
		s->inuse[j]=false;
	s->n = 0;
}

static void
msgset_push(MsgSet *s, Msg m)
{
	int	j;

	assert(!msgset_full(s));

	for(j=0; j< Qmax; j++)
		if (!s->inuse[j]) {
			s->msg[j] = m;
			s->inuse[j] = true;
			s->n++;
			return;
		}
	assert(false);
}

static Bool
msgset_find(MsgSet *s, Id m, Msg *msg)
{
	int	j;

	if(!s->n)
		return false;

	for(j=0; j< Qmax; j++)
		if (s->inuse[j] && s->msg[j].m ==m){
			s->inuse[j] = false;
			s->n--;
			*msg =  s->msg[j];
			return true;
		}
	return false;
}

static Bool
msgset_full(MsgSet*s)
{
	return s->n == Qmax;
}

