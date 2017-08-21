#include <u.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <msg.h>	/* ../include/msg.h */

/* NOTE: msg_flatten and msg_init need to remain in sync.
 * Preferably the msg_flatten and msg_init functions of different
 * versions should still be able to talk to one another, i.e. the 
 * message protocol on the wire should stay the same.

	off	bytes	value
	0	2		0xfeed (cookie)
	2	2		message type
	4	4		total message length (len)
	8	2		message id
	10	2		window id
	12	4		r.p0
	16	4		r.p1
	20	2		flag
	22	len-22	null-terminated utf string
 */
enum {
	HEADERSIZE = 22,
	COOKIE = 0xfeed,
	DEBUG=0
};

static ushort get2(uchar*);
static ulong	get4(uchar*);
static void put2(uchar*, ushort);
static void put4(uchar*, ulong);

void
msg_fill(Msg*m, Mtype t, Id w, Range r, ushort flag, char*s) {
	m->t = t;
	m->w = w;
	m->r = r;
	m->flag = flag;
	m->s = s;
}

/* Return number of bytes which would be needed to
 * flatten 'm'
 */
int
msg_size(Msg *m)
{
	assert(m);
	return HEADERSIZE + 1 + (m->s? strlen(m->s) : 0);
}

/* Flatten 'm' into 'buf', which has at least msg_size(m) bytes of space.
 */
void
msg_flatten
(Msg*m, uchar*buf)
{
	if(DEBUG) {
		printf("flattening ");
		msg_print(m);
	}
	put2(buf, COOKIE);
	put2(buf+2, m->t);
	put4(buf+4, msg_size(m));
	put2(buf+8, m->m);
	put2(buf+10, m->w);
	put4(buf+12, m->r.p0);
	put4(buf+16, m->r.p1);
	put2(buf+20, m->flag);
	strcpy(buf+22, m->s?m->s:"");
}

/* Fill in the fields of 'm', using 'buf', which has 'size' bytes.
 * PRE:  at least msg_bufsize() bytes in 'buf'
 *
 * POST: m's fields are filled in.  m.s points to a null-terminated
 * string WITHIN BUF.  We return 0.
 * If there was some internal inconsistency, we return -1, and
 * the contents of 'm' are undefined.
 *
 * WARNING:  We may point to a string inside 'buf', rather than
 * copying it.  This means you should not free 'buf' until you've
 * finished with 'm'.
 */
int
msg_init (Msg*m, uchar*buf)
{
	ulong	len;

	len = get4(buf+4);

	if (get2(buf)!=COOKIE || len<HEADERSIZE) {
		return -1;
	}
	m->t = get2(buf+2);
	m->m = get2(buf+8);
	m->w = get2(buf+10);
	m->r.p0 = get4(buf+12);
	m->r.p1 = get4(buf+16);
	m->flag = get2(buf+20);
	m->s = buf+22;
	buf[len-1]='\0';	/* ENSURE m->s is null-terminated */

	if(DEBUG) {
		printf("initialized ");
		msg_print(m);
	}
	
	return 0;
}

/* Return number of bytes needed for the complete message,
 * of which the beginning is in the buffer.  There must be
 * at least 8 bytes in the buffer already, or we get garbage.
 */
ulong
msg_bufsize (uchar*buf)
{
	return get4(buf+4);
}

void
msg_print(Msg *m)
{
	printf("(");
	switch(m->t) {
	case WRerror : printf("(WRerror"); break;
	case WMlist : printf("WMlist"); break;
	case WRlist : printf("WRlist"); break;
	case WMnew : printf("WMnew"); break;
	case WRnew : printf("WRnew"); break;
	case WMattach : printf("WMattach"); break;
	case WRattach : printf("WRattach"); break;
	case WMsetname : printf("WMsetname"); break;
	case WMgetname : printf("WMgetname"); break;
	case WRsetname : printf("WRsetname"); break;
	case WRgetname : printf("WRgetname"); break;
	case WMsettools : printf("WMsettools"); break;
	case WMgettools : printf("WMgettools"); break;
	case WRsettools : printf("WRsettools"); break;
	case WRgettools : printf("WRgettools"); break;
	case WMread : printf("WMread"); break;
	case WRread : printf("WRread"); break;
	case WMreplace : printf("WMreplace"); break;
	case WRreplace : printf("WRreplace"); break;
	case WMexec : printf("WMexec"); break;
	case WRexec : printf("WRexec"); break;
	case WMgoto : printf("WMgoto"); break;
	case WRgoto : printf("WRgoto"); break;
	case WMfencepost : printf("WMfencepost"); break;
	case WEexec : printf("WEexec"); break;
	case WEreplace : printf("WEreplace"); break;
	case WEgoto : printf("WEgoto"); break;
	case WEdestroy : printf("WEdestroy"); break;
	case WEfencepost : printf("WEfencepost"); break;
	default : printf("Unknown"); break;
	}
	printf(", %d", m->m);
	printf(", %d", m->w);
	printf(", %d", m->flag);
	printf(", (%lu,%lu)", m->r.p0, m->r.p1);
	printf(", [%s]) ", m->s?m->s:"");
}

static ushort
get2(uchar*p)
{
	return (p[0]<<8) + p[1];
}

static ulong
get4(uchar*p)
{
	return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3];
}

static void
put2(uchar*p, ushort n)
{
	p[0] = (n>>8)&0xff;
	p[1] = n &0xff;
}

static void
put4(uchar*p, ulong n)
{
	p[0] = (n>>24)&0xff;
	p[1] = (n>>16)&0xff;
	p[2] = (n>>8)&0xff;
	p[3] = (n)&0xff;
}

