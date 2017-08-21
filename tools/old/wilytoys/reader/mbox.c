/*
 * mbox.c
 */

#include "headers.h"
#include "mbox.h"
#include <fcntl.h>
#include <sys/stat.h>

#define COPYSIZE	8192
#define INCR		10

static mMbox *currmbox;		/* current mbox being processed */
static mMsg *msg;		/* current message */
static int nhdrs;		/* number of current headers read so far. */
static int ahdrs;		/* number of allocated hhdrs */
static mHdr **hdrs;		/* current list */
static char **hiddenhdrs;	/* names of header fields we don't want to see */
static int nhiddenhdrs;

extern int load_mbox(mMbox *, int);
static char *readhdrs(char *ptr);
static void mkhdr(char *s0, char *s1);
static void checkhide(mHdr *h);
static int freehdrlist(int n, char **h);
static void newmsg(char *s0);
static char *readmsg(char *ptr);
static int same(mString a, mString b);
static int strsame(char *s, mString b);
static void fillmsg(mMsg *msg);
static void fillhdr(char *name, mMsg *msg, mString **ptr);
static ulong getlength(void);
static char *findend(char *ptr);
static void freemsg(mMbox *mbox, int n);

/*
 * readhdrs(ptr) - read the headers in a message, looking for
 * the terminating newline. Returns pointer to first char in body.
 */

static char *
readhdrs(char *ptr)
{
	char *nptr = ptr;

	/* invariant is that nptr pts to first char of next header.
	when it doesn't (nptr==\n), we've reached the blank line at
	then end of the hdrs */
	while (*nptr != '\n') {
		if ((nptr = strchr(nptr,'\n')) == 0)
			panic("No newlines in readhrds()");
		if (nptr[1]==' ' || nptr[1]=='\t') {	/* multi-line hdr */
			nptr++;
			continue;
		} else {
			mkhdr(ptr, ++nptr);
			ptr = nptr;
			continue;
		}
	}
	return ++nptr;		/* skip blank line */
}


/*
 * mkhdr(s0,s1) - add a new header to the current list.
 */

static void
mkhdr(char *s0, char *s1)
{
	size_t l;
	char *c = strchr(s0, ':');
	mHdr *h;

	if (c == 0)
		panic("no colon in hdr");
	if (nhdrs >= ahdrs) {
		ahdrs += INCR;
		l = ahdrs * sizeof(*hdrs);
		hdrs = (mHdr **)(hdrs? realloc(hdrs,l) : malloc(l));
		if (hdrs == 0)
			panic("resizing hdr list");
	}
	h = hdrs[nhdrs++] = (mHdr *)malloc(sizeof(*h));
	if (h == 0)
		panic("adding new hdr");
	h->name.s0 = s0;
	h->name.s1 = c++;
	while (*c == ' ' || *c == '\t')
		c++;
	h->value.s0 = c;
	h->value.s1 = s1;
	checkhide(h);
}

static void
checkhide(mHdr *h)
{
	int n;
	int l = h->name.s1 - h->name.s0;

	for (n = 0; n < nhiddenhdrs; n++)
		if (strncmp(hiddenhdrs[n], h->name.s0, l) == 0) {
			h->hide = 1;
			return;
		}
	h->hide = 0;			/* default to showing header */
}

int
set_hdr_filter(int nhdrs, char **hdrs)
{
	int n;

	(void)freehdrlist(nhiddenhdrs, hiddenhdrs);	/* clear out existing list */
	if ((hiddenhdrs = (char **)realloc((void *)hiddenhdrs, (nhdrs+1)*sizeof(char *))) == 0)
		return 1;
	for (n = 0; n < nhdrs; n++)
		if ((hiddenhdrs[n] = strdup(hdrs[n])) == 0)
			return freehdrlist(n, hiddenhdrs);
	hiddenhdrs[n] = 0;
	nhiddenhdrs = n;
	return 0;
}

static int
freehdrlist(int n, char **h)
{
	while (n--)
		free (*h++);
	return 1;
}

static void
newmsg(char *s0)
{
	size_t n;

	if (currmbox->nmsgs >= currmbox->amsgs) {
		currmbox->amsgs += INCR;
		n = currmbox->amsgs*sizeof(*currmbox->msgs);
		currmbox->msgs = (mMsg **)(currmbox->msgs? realloc(currmbox->msgs,n) : malloc(n));
		if (currmbox->msgs == 0)
			panic("newmsg msg");
	}
	msg = currmbox->msgs[currmbox->nmsgs++] = (mMsg *)malloc(sizeof(*msg));
	if (msg == 0)
		panic("adding new msg");
	memset(msg,0,sizeof(*msg));
	msg->whole.s0 = s0;
	if ((hdrs = (mHdr **)malloc((ahdrs = INCR)*sizeof(*hdrs)))==0)
		panic("newmsg hdrs");
	nhdrs = 0;
}

/*
 * readmsg(ptr) - read a complete message. Assumes we're already pointing
 * to the F of the "From "
 */

static char *
readmsg(char *ptr)
{
	char *nptr;
	ulong contentlength;

	newmsg(ptr);			/* resets nhdrs and hdrs, too */
	if ((nptr = strchr(ptr,'\n')) == 0)	/* skip From line */
		panic("From line broken");
	nptr = readhdrs(nptr+1);
	msg->nhdrs = nhdrs;
	msg->hdrs = hdrs;
	msg->body.s0 = nptr;
	if ((contentlength = getlength()) == 0)
		nptr = findend(nptr);
	else
		nptr += contentlength;
	msg->whole.s1 = msg->body.s1 = nptr;
	fillmsg(msg);		/* update from,date,subject fields */
	mb_split(ptr, ++nptr);
	return nptr;
}

static int
same(mString a, mString b)
{
	if ((a.s1 - a.s0) != (b.s1 - b.s0))
		return 0;
	return (strncmp(a.s0,b.s0,a.s1-a.s0) == 0);
}

static int
strsame(char *s, mString b)
{
	mString a;

	a.s0 = s;
	a.s1 = s + strlen(s);
	return same(a,b);
}

static void
fillmsg(mMsg *msg)
{
	fillhdr("From",msg,&msg->from);
	fillhdr("Date",msg,&msg->date);
	fillhdr("Subject",msg,&msg->subject);
}

static void
fillhdr(char *name, mMsg *msg, mString **ptr)
{
	int x;
	static mString empty;

	for (x = 0; x < msg->nhdrs; x++)
		if (strsame(name,msg->hdrs[x]->name)) {
			*ptr = &(msg->hdrs[x]->value);
			return;
		}
	/* Sigh. This sort of thing is very irritating. */
	empty.s0 = empty.s1 = msg->whole.s1;
	*ptr = &empty;
	(void)fprintf(stderr,"Message does  not have a '%s' header\n",name);
}

static ulong
getlength(void)
{
	int x;
	ulong l;

	for (x = 0; x < nhdrs; x++)
		if (strsame("Content-Length",hdrs[x]->name))
			goto found;
	return 0;
found:
	/* use sscanf magic to skip whitespace, and stop at end */
	if (sscanf(hdrs[x]->value.s0,"%lu",&l) != 1 || l==0)
		return 0;
	if ((msg->body.s0+l+1) == currmbox->mbox_end)
		return l;		/* spot on */
	if ((msg->body.s0+l+5) >= currmbox->mbox_end)
		return 0;		/* too long */
	if (strncmp("\nFrom ",(msg->body.s0+l),6) == 0)
		return l;
	else
		return 0;
}

static char *
findend(char *ptr)
{
	for (; ptr+5 <= currmbox->mbox_end && strncmp("\nFrom ",ptr,6); ptr++)
		if ((ptr = strchr(ptr,'\n')) == 0)
			panic("findend");
	if (ptr+5 > currmbox->mbox_end)
		return currmbox->mbox_end-1;
	else
		return ptr;
}

mMbox *
read_mbox(const char *filename, int readonly, int mustexist)
{
	char *ptr;
	extern int empty_mbox(mMbox *box, const char *filename, int mustexist);

	if ((currmbox = (mMbox *)malloc(sizeof(*currmbox))) == 0)
		return 0;
	memset(currmbox,0,sizeof(*currmbox));
	currmbox->readonly = readonly;
	if ((currmbox->name = strdup(filename)) == 0)
		goto panic2;
	if (load_mbox(currmbox, 0) && empty_mbox(currmbox, filename, mustexist))
		goto panic1;

	for (ptr = currmbox->mbox_start; ptr < currmbox->mbox_end; )
		if ((ptr = readmsg(ptr)) == 0)
			goto panic1;
	return currmbox;
panic1:
	free(currmbox->name);
panic2:
	free(currmbox);
	return 0;
}

int
extend_mbox(mMbox *mbox)
{
	char *ptr;

	if (load_mbox(mbox,1))
		return 1;
	for (ptr = mbox->mbox_start; ptr < mbox->mbox_end; )
		if ((ptr = readmsg(ptr)) == 0)
			return 1;
	return 0;
}

/*
 * update_mbox(mbox) - write any changes back to the file.
 */

int
update_mbox(mMbox *mbox)
{
	static char lockname[MAXPATHLEN+1], newname[MAXPATHLEN+1];
	int ntries, maxtries = 10;
	int fd, fd2;
	int x;
	struct stat st;
	int r = 0;
	int l;
	mMsg *m;

	if (mbox->readonly || mbox->ndel == 0)
		return 0;
	(void)sprintf(lockname,"%s.lock",mbox->name);
	(void)sprintf(newname,"%s.new",mbox->name);
	for (ntries = 0; ntries < maxtries; ntries++) {
		if (link(mbox->name,lockname) == 0)
			goto locked;
		sleep(1);
	}
locked:
	if (stat(mbox->name,&st) == -1)
		goto broken;
	if (st.st_size < mbox->size) {
		fprintf(stderr,"Mail file corrupted - no changes made\n");
		goto broken;
	}
	if ((fd = open(newname,O_RDWR|O_CREAT|O_TRUNC)) < 0 ||
		fchmod(fd,st.st_mode) == -1)
		goto broken;
	for (x = 0; x < mbox->nmsgs; x++) {
		m = mbox->msgs[x];
		if (m->deleted)
			continue;
		l = m->whole.s1 - m->whole.s0 + 1;
		if (write(fd,m->whole.s0,l) != l || write(fd,"\n",1) != 1) {
			fprintf(stderr,"write truncated, changes aborted\n");
			goto broken;
		}
	}
	if (st.st_size > mbox->size) {
		static char buf[COPYSIZE];
		if ((fd2 = open(mbox->name,O_RDONLY)) < 0 ||
			lseek(fd2,mbox->size,SEEK_SET) == -1) {
			fprintf(stderr,"Can't open mbox for update!\n");
			goto broken;
		}
		while ((l = read(fd2,buf,COPYSIZE)) > 0)
			if (write(fd,buf,l) != l) {
				fprintf(stderr,"Can't copy updated mbox\n");
				close(fd2);
				goto broken;
			}
		close(fd2);
	}
	rename(newname,mbox->name);
	stat(mbox->name, &st);
	mbox->mtime = st.st_mtime;
	mbox->size = st.st_size;
	goto unlock;
broken:
	r = 1;
	if (fd >= 0)
		close(fd);
	unlink(newname);
unlock:
	unlink(lockname);
	/* now update the list of messages in the mbox */
	for (l = x = 0; x < mbox->nmsgs; x++) {
		m = mbox->msgs[x];
		if (m->deleted)
			freemsg(mbox,x);
		else
			mbox->msgs[l++] = m;
	}
	mbox->nmsgs = l;
	mbox->ndel = 0;
	return r;
}

static void
freemsg(mMbox *mbox, int n)
{
	mMsg *m = mbox->msgs[n];
	int h;

	mbox->msgs[n] = 0;
	for (h = 0; h < m->nhdrs; h++)
		free(m->hdrs[h]);
	free(m->hdrs);
	mb_free(m->whole.s0);
	free(m);
}
