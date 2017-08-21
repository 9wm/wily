/*******************************************
 *	Handle messages from remote processes
 *******************************************/

#include "wily.h"
#include "data.h"
static char*	badrange ="bad range";
static char*	unknown ="unknown message type";

static char*	data_attach(Data*d, int fd, ushort emask);
static void	data_changelabel(Data *d,char*label);
static char*	data_getname(Data*d);
static char *	data_list(void);
static int		msg_send(Msg*m, int fd);
static void	msg_new(Msg*m);
static void	msg_fillfd(Msg*m, int fd);
static Bool	msg_form_and_send(Mtype,  Id, Id, Range, ushort, char	*, int);
static void	msg_process(Msg*m, int fd);
static void	msg_error(Msg*m, char*s);

/****************************************************************
	Unpack and handle incoming messages
****************************************************************/
void
mbuf_init(Mbuf*m){
	m->buf = 0;
	m->alloced = 0;
	m->n = 0;
}

void
mbuf_clear(Mbuf*m){
	m->n = 0;
}

/* 
 * Handle the message contained in 'buf' (which might have
 * part of an old message) and 's', which contains 'n' new bytes.
 * Send any replies back along 'fd'.
 *
 * Return 0 unless we get a badly formatted message.
  */
int
partialmsg(Mbuf *m, int fd, int n, char*s){
	char	*ptr;
	int	left;

	/* copy to m->buf */
	SETSIZE(m->buf, m->alloced, m->n + n);
	memcpy(m->buf + m->n, s, n);
	m->n += n;

	/* process any whole messages */
	ptr = m->buf;
	left = m->n;
	while (FULLMSG(ptr,left)) {
		Msg		msg;
		int	eaten = msg_bufsize(ptr);

		if(msg_init(&msg, ptr)){
			return -1;
		}
		ptr += eaten;
		left -= eaten;
		msg_process(&msg, fd);
	}

	/* move any incomplete messages to front of buf->buf */
	if (left && left != m->n)
		memmove(m->buf, ptr, left);
	m->n = left;
	return 0;
}

/* 
 * Stop any data from sending events to 'fd',
 * which is probably closed.
 */
void
data_fdstop(int fd) {
	Data	*d;

	for (d=dataroot; d; d = d->next) {
		if(d->fd == fd){
			d->fd = 0;
			d->emask = 0;
		}
	}
}

/****************************************************************
	Send out messages in response to user events
****************************************************************/
/*
 * If someone's monitoring this window for 'replace' events,
 * send them one.  If we fail to send the message, or don't need
 * to send the message, return false.
 */
Bool
data_sendreplace(Data *d,Range r, Rstring s) {
	char	*buf;
	Bool	retval;

	if(! ( d && d->emask & WEreplace) )
		return false;

	buf = salloc(RSLEN(s) * UTFmax);
	buf[texttoutf(buf, s.r0, s.r1)] = '\0';
	retval = msg_form_and_send(WEreplace, 0, d->id, r, 0, buf, d->fd);
	free(buf);
	return retval;
}

/* see data_sendreplace */
Bool
data_senddestroy(Data *d) {
	if(! ( d && d->emask & WEdestroy) )
		return false;
	return msg_form_and_send(WEdestroy, 0, d->id, nr, 0, 0, d->fd);
}

/* see data_sendreplace */
Bool
data_sendgoto(Data *d,Range r, char *s) {
	if(! ( d && d->emask & WEgoto) )
		return false;
	return msg_form_and_send(WEgoto, 0, d->id, r, 0, s, d->fd);
}

/* see data_sendreplace */
Bool
data_sendexec(Data *d,char*cmd, char *arg) {
	Bool	retval;
	char	*s;

	if(! ( d && d->emask & WEexec) )
		return false;

	if(arg) {
		s = salloc( strlen(cmd) + strlen(arg) + 2);
		sprintf(s, "%s %s", cmd, arg);
	} else {
		s = cmd;
	}
	retval = msg_form_and_send(WEexec, 0, d->id, nr, 0, s, d->fd);
	if(arg)
		free(s);
	return retval;
}

/**********************************************
	Static functions
 **********************************************/

static char*
data_attach(Data*d, int fd, ushort emask) {
	if(d->fd)
		return "This window already attached";
	else {
		d->fd = fd;
		d->emask = emask;
		return 0;
	}
}

/* Change the d->label to 'label', update d->path.
 * Only called in response to a remote message.
 */
static void
data_changelabel(Data *d,char*label) {
	strcpy(d->label, label);
	tag_setlabel(d->tag, d->label);
}

static char*
data_getname(Data*d){
	static Path buf;
	
	label2path(buf, d->label);
	return buf;
}

/* Return newline-separated list of tab-separated (winname,id) tuples
 */
static char *
data_list(void) {
	static char	*buf=0;
	static int		alloced=0;
	int	size = 0;
	Data	*d;
	char	*ptr;
	Path	path;

	/* calculate the size of buffer required */
	size =0;
	for(d=dataroot; d; d=d->next) {
		label2path(path, d->label);
		size += strlen(path) + 15;
	}
	
	if(size > alloced) {
		alloced = size;
		buf = srealloc(buf, alloced);
	}
	
	ptr = buf;
	for(d=dataroot; d; d=d->next) {
		label2path(path, d->label);
		sprintf(ptr, "%s\t%d\n", path, d->id);
		ptr += strlen(ptr);
	}
	return buf;
}

/* Return 0 for success */
static int
msg_send(Msg*m, int fd) {
	static uchar*buf=0;
	static int	alloced=0;
	ulong	size = msg_size(m);

	SETSIZE(buf, alloced, size);
	msg_flatten(m, buf);
	return write(fd,buf,size)!=size;	/* todo - handle partial writes? */
}

static void
msg_new(Msg*m) {
	View	*v;
	Data	*d;
	
	if(!(v = data_find(m->s)))
		v = data_open(m->s, true);
	d = view_data(v);

	if(!m->flag)
		data_setbackup(d,0);	
	m->w = d->id;
}

static void
handleattach(Msg*m, Data*d, int fd){
	m->s = data_attach(d, fd, m->flag);
	if(m->s)
		m->t = WRerror;
}

static void
handleread(Msg*m, Text *t ) {
	if (text_badrange(t,m->r)) {
		msg_error(m, badrange);
	} else {
		m->s = text_duputf(t, m->r);
	}
}

static void
handlereplace(Msg*m, Text *t ) {
	if (text_badrange(t,m->r)) {
		msg_error(m, badrange);
	} else {
		text_replaceutf(t, m->r, m->s);
	}
}

static void
handlegoto(Msg*m, View*v) {
	Bool	show;
	Range	r;
	
	show = (m->t == WEgoto) || m->flag;

	/* We get the search start position 
	 * either from 'm' or 'dot' in the view.
	 */
	r = m->r;
	if(r.p0>r.p1)
		r = view_getsel(v);
	
	if(view_goto(&v, &r, m->s)){
		if (show) {
			view_show(v, r);
			view_select(v, r);
			view_warp(v, r);
		}
		m->r = r;
		m->w = text_data(view_text(v))->id;
	} else {
		/* indicate search failure */
		m->r.p0 = -1;
		m->r.p1 = 0;
	}
}

static void
msg_error(Msg*m, char*s){
	m->s = s;
	m->t = WRerror;
}

/* Process a message which arrived on 'fd'.  Modifies 'm'. */
static void
msg_fillfd(Msg*m, int fd) {
	Data	*d=0;
	View	*v;

	/* WMlist or WMnew don't need to be associated with a valid window.
	 */
	switch(m->t) {
	case WMlist:	m->s = data_list();	return;
	case	WMnew:	msg_new(m); return;
	default:	break;
	}

	if(!(d=data_findid(m->w))) {
		msg_error(m, "No window with this id");
		return;
	}
	
	v = text_view(d->t);

	switch(m->t) {
	case	WMattach:	handleattach(m,d,fd); 		break;
	case	WMsetname:	data_changelabel(d, m->s);	break;
	case WMgetname:	m->s = data_getname(d);	break;
	case	WMsettools:	tag_settools(d->tag, m->s);	break;
	case	WMgettools:	m->s=tag_gettools(d->tag);	break;
	case	WMread:		handleread(m, d->t); 		break;
	case WEreplace:							break;
	case	WMreplace:	handlereplace(m,d->t);		break;
	case	WMexec:							/* fall through */
	case	WEexec:		run(v, m->s, 0);			break;
	case	WMgoto:							/* fall through */
	case	WEgoto:		handlegoto(m,v);			break;
	default:			msg_error(m, unknown); 	break;
	}
}

static Bool
msg_form_and_send(Mtype t,  Id m, Id w, Range	r, ushort	flag, char	*s, int fd) {
	Msg	msg;

	msg.t = t;
	msg.m = m;
	msg.w = w;
	msg.r = r;
	msg.flag = flag;
	msg.s = s;

	return ! msg_send(&msg, fd);
}

/* Process a message which arrived on 'fd' */
static void
msg_process(Msg*m, int fd) {
	Bool	isbounce;

	isbounce = m->t < WEfencepost;
	
	msg_fillfd(m, fd);

	if(!isbounce){
		if(m->t != WRerror)
			m->t ++;
		(void)msg_send(m,fd);
	} else if (m->t == WRerror) {
		diag(0, "error from bounced event %s", m->s);
		msg_print(m);
	}
}

