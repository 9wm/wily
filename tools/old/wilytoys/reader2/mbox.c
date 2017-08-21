/*
 * mbox.c
 */

#include "mailheaders.h"
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

	assert(nptr);
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
	assert(*nptr == '\n');
	assert(nptr[1]);
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

	assert(s0);
	assert(s1);
	assert(c);
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
	int l;

	assert(h);
	assert(h->name.s0 && h->name.s1);
	l = h->name.s1 - h->name.s0;

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

	assert(nhdrs >= 0);
	assert(hdrs);
	(void)freehdrlist(nhiddenhdrs, hiddenhdrs);	/* clear out existing list */
	if ((hiddenhdrs = (char **)realloc((void *)hiddenhdrs, (nhdrs+1)*sizeof(char *))) == 0) {
		DPRINT("Failed to allocate new hidden hdr list");
		return 1;
	}
	for (n = 0; n < nhdrs; n++) {
		assert(hdrs[n]);
		if ((hiddenhdrs[n] = strdup(hdrs[n])) == 0) {
			DPRINT("Failed to copy hidden hdr item");
			return freehdrlist(n, hiddenhdrs);
		}
	}
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

	assert(s0);
	DPRINT("Adding new message");
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

	assert(ptr);
	assert(strncmp(ptr,"From ",5) == 0);
	newmsg(ptr);			/* resets nhdrs and hdrs, too */
	if ((nptr = strchr(ptr,'\n')) == 0)	/* skip From line */
		panic("From line broken");
	nptr = readhdrs(nptr+1);
	assert(nptr);
	assert(hdrs);
	msg->nhdrs = nhdrs;
	msg->hdrs = hdrs;
	msg->body.s0 = nptr;
	if ((contentlength = getlength()) == 0)
		nptr = findend(nptr);
	else
		nptr += contentlength;
	assert(nptr);
	msg->whole.s1 = msg->body.s1 = nptr;
	fillmsg(msg);		/* update from,date,subject fields */
	mb_split(ptr, ++nptr);
	return nptr;
}

static int
same(mString a, mString b)
{
	assert(a.s0 && a.s1 && b.s0 && b.s1);
	if ((a.s1 - a.s0) != (b.s1 - b.s0))
		return 0;
	return (strncmp(a.s0,b.s0,a.s1-a.s0) == 0);
}

static int
strsame(char *s, mString b)
{
	mString a;

	assert(s && b.s0 && b.s1);
	a.s0 = s;
	a.s1 = s + strlen(s);
	return same(a,b);
}

static void
fillmsg(mMsg *msg)
{
	assert(msg);
	fillhdr("From",msg,&msg->from);
	fillhdr("Date",msg,&msg->date);
	fillhdr("Subject",msg,&msg->subject);
}

static void
fillhdr(char *name, mMsg *msg, mString **ptr)
{
	int x;
	static mString empty;

	assert(name);
	assert(msg);
	assert(ptr);
	for (x = 0; x < msg->nhdrs; x++)
		if (strsame(name,msg->hdrs[x]->name)) {
			*ptr = &(msg->hdrs[x]->value);
			assert((*ptr)->s0 && (*ptr)->s1);
			return;
		}
	/* Sigh. This sort of thing is very irritating. */
	empty.s0 = empty.s1 = msg->whole.s1;
	*ptr = &empty;
	/* (void)fprintf(stderr,"Message does  not have a '%s' header\n",name); */
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
	if ((msg->body.s0+l+5) >= currmbox->mbox_end) {
		DPRINT("WARNING: Content-Length: field points to past end of box");
		return 0;		/* too long */
	}
	if (strncmp("\nFrom ",(msg->body.s0+l),6) == 0)
		return l;
	else {
		DPRINT("WARNING: Content-Length field doesn't find start of next msg");
		return 0;
	}
}

static char *
findend(char *ptr)
{
	assert(ptr);
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

	assert(filename);
	DPRINT("Reading file called...");
	DPRINT(filename);
	DPRINT(readonly? "(readonly mode)" : "(writable)");
	DPRINT(mustexist? "(must exist)" : "(normal mbox)");
	if ((currmbox = (mMbox *)malloc(sizeof(*currmbox))) == 0) {
		DPRINT("Could not allocate mbox structure");
		return 0;
	}
	memset(currmbox,0,sizeof(*currmbox));
	currmbox->readonly = readonly;
	if ((currmbox->name = strdup((char *)filename)) == 0) {
		DPRINT("Could not copy filename");
		goto panic2;
	}
	if (load_mbox(currmbox, 0) && empty_mbox(currmbox, filename, mustexist)) {
		DPRINT("mbox failed to load");
		goto panic1;
	}

	for (ptr = currmbox->mbox_start; ptr < currmbox->mbox_end; )
		if ((ptr = readmsg(ptr)) == 0) {
			DPRINT("Failed to read a message - aborting mbox read");
			goto panic1;
		}
	DPRINT("mbox read successfully");
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

	assert(mbox);
	DPRINT("Extending mbox");
	if (load_mbox(mbox,1)) {
		DPRINT("Failed to read mbox");
		return 1;
	}
	for (ptr = mbox->mbox_start; ptr < mbox->mbox_end; )
		if ((ptr = readmsg(ptr)) == 0) {
			DPRINT("Failed to read message");
			return 1;
		}
	DPRINT("mbox extended ok");
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

	assert(mbox);
	assert(mbox->name);
	assert(strlen(mbox->name)+5 < MAXPATHLEN);
	DPRINT("Updating mbox");
	if (mbox->readonly || mbox->ndel == 0) {
		DPRINT(mbox->readonly? "mbox is readonly - not written" : "No changes");
		return 0;
	}
	(void)sprintf(lockname,"%s.lock",mbox->name);
	(void)sprintf(newname,"%s.new",mbox->name);
	for (ntries = 0; ntries < maxtries; ntries++) {
		if (link(mbox->name,lockname) == 0)
			goto locked;
		DPRINT("Could not get lock - sleeping");
		sleep(1);
	}
	(void)fprintf(stderr,"Could not get lockfile %s - skipping update\n", lockname);
	return 1;
locked:
	DPRINT("Mailbox now locked");
	if (stat(mbox->name,&st) == -1) {
		DPRINT("mailbox has vanished");
		goto broken;
	}
	if (st.st_size < mbox->size) {
		fprintf(stderr,"Mail file corrupted - no changes made\n");
		goto broken;
	}
	if ((fd = open(newname,O_RDWR|O_CREAT|O_TRUNC,st.st_mode)) < 0) {
		DPRINT("Could not open or fchmod new file...");
		DPRINT(newname);
		goto broken;
	}
	DPRINT("Writing messages to new file");
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
	DPRINT("Messages written");
	if (st.st_size > mbox->size) {
		static char buf[COPYSIZE];
		DPRINT("mbox has grown since last read - copying new msgs");
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
		DPRINT("New msgs copied");
	}
	DPRINT("Replacing old mbox with new mbox");
	assert(rename(newname,mbox->name) == 0);
	assert(stat(mbox->name, &st) == 0);
	mbox->mtime = st.st_mtime;
	mbox->size = st.st_size;
	goto unlock;
broken:
	r = 1;
	if (fd >= 0)
		close(fd);
	unlink(newname);
unlock:
	DPRINT("Unlocking mbox");
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
	DPRINT("Update complete");
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
