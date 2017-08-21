#include "headers.h"
#include "mbox.h"
#include "mail.h"


static void build_msg_list();
static char *getarttext(mMsg *m);
mWin *allocMWin(int kind, int num);
void freeMWin(mWin *w);
mWin *findMWin(void *ptr);
int artDisplayed(rdWin *w, int n);
void mDelete(rdWin *w, int first, int last, char *arg);
void mUndelete(rdWin *w, int first, int last, char *arg);
void mComp(rdWin *w, int first, int last, char *arg);
void mExit(rdWin *w, int first, int last, char *arg);
void mQuit(rdWin *w, int first, int last, char *arg);
void mReply(rdWin *w, int first, int last, char *arg);
void mAbort(rdWin *w, int first, int last, char *arg);
void mDeliver(rdWin *w, int first, int last, char *arg);
void mSavefile(rdWin *w, int first, int last, char *arg);
void mSave(rdWin *w, int first, int last, char *arg);
void dodeliver(rdWin *w, char *filename);
void mMultiart(rdWin *w, int first, int last, char *arg);
void mAllheaders(rdWin *w, int first, int last, char *arg);
void mIncludeall(rdWin *w, int first, int last, char *arg);
void mInclude(rdWin *w, int first, int last, char *arg);
void doinclude(rdWin *w, int first, int last, char *arg, int all);
void mCommit(rdWin *w, int first, int last, char *arg);
void mRescan(rdWin *w, int first, int last, char *arg);


mMbox *mbox;
static char mboxname[MAXPATHLEN];
mWin *mwindows;
mWin *listwin;

static char **msgs;
static char *savefile;
static int multiart = 0;
static int show_all_headers = 0;

/*
 * These are headers that we'd rather not see when the message gets displayed.
 */

static char *hidden_hdrs[] = {
	"Content-Length",
	"Content-Transfer-Encoding",
	"Content-Type",
	"In-Reply-To",
	"Message-Id",
	"Mime-Version",
	"Original-Sender",
	"Received",
	"Sender",
	"Status",
	"X-Lines",
	"X-Mailer",
	"X-Newsreader"
};
static int nhidden_hdrs = sizeof(hidden_hdrs)/sizeof(hidden_hdrs[0]);

int
main(int argc, char *argv[])
{
	mWin *mw;
	int ma = 0;
	int mustexist = 0;
	int timer = RESCAN_TIME;

	fflush(stdout);
	if (argc >=2 && strcmp(argv[1], "-ma")==0) {
		ma = 1;
		argc--;
		argv++;
	}
	multiart = ma;
	if (argc >= 2) {
		mustexist = 1;
		strcpy(mboxname, argv[1]);
		timer = 0;	/* don't rescan named mboxes */
	} else {
		char *u = getenv("USER");

		if (!u || !*u) {
			fprintf(stderr,"Need $USER to find mailbox name\n");
			exit(1);
		}
		sprintf(mboxname,"%s/%s", SPOOLDIR, u);
	}
	savefile = getenv(SAVEFILE_ENV);
	if (!savefile || !*savefile) {
		char s[MAXPATHLEN];
		char *h;
		if ((h = getenv("HOME")) == 0 || !*h)
			savefile = sstrdup("mbox");	/* what planet are we on? */
		else {
			sprintf(s,"%s/mbox",h);
			savefile = sstrdup(s);
		}
	} else
		savefile = sstrdup(savefile);

	set_hdr_filter(nhidden_hdrs, hidden_hdrs);
	if ((mbox = read_mbox((const char *)mboxname, 0, mustexist)) == 0) {
		perror(mboxname);
		exit(1);
	}
	mw = listwin = allocMWin(mMsgList, 0);
	build_msg_list();
	if (readerInit(0, (char *)mw, mboxname, msgs, 0, ma, 0))
		exit(1);
	rdSetRescanTimer(timer);
	readerMainLoop();
	exit(0);
}

mWin *
allocMWin(int kind, int num)
{
	mWin *w = salloc(sizeof(*w));
	mWin **p = &mwindows;

	w->kind = kind;
	w->msg = (kind == mDispArt || kind == mCompArt)? num : -1;
	w->mbox = mbox;
	w->next = 0;
	while (*p)
		p = &((*p)->next);
	*p = w;
	return w;
}

void
freeMWin(mWin *w)
{
	mWin **p = &mwindows;

	while (*p != w)
		p = &((*p)->next);
	*p = w->next;
	free(w);
}

mWin *
findMWin(void *ptr)
{
	mWin *mw = mwindows;

	while (mw && (mWin *)ptr != mw)
		mw = mw->next;
	return mw;
}

static void
chkhdr(mString *s, int *len, char **rest)
{
	int l;
	*rest = "";			/* default */
	if (s == 0) {
		*len = 0;
		return;
	}
	l = s->s1 - s->s0;
	if (l > 20) {
		l = 20;
		*rest = "...";
	} else if (l > 0)
		l--;		/* skip newline */
	*len = l;
}

static void
build_msg_list(void)
{
	int n;
	mMsg *m;
	ulong l;
	int lfrom, ldate, lsubject;
	char *rfrom, *rdate, *rsubject;
	mString *from;
	mString *date;
	mString *subject;
	int dc;

	if (msgs)
		for (n = 0; msgs[n]; n++)
			free(msgs[n]);
	msgs = (char **)srealloc(msgs, (mbox->nmsgs+1) * sizeof(char *));
	for (n = 0; n < mbox->nmsgs; n++) {
		m = mbox->msgs[n];
		dc = m->deleted? 'D' : ' ';
		from = m->from;
		date = m->date;
		subject = m->subject;
		chkhdr(from, &lfrom, &rfrom);
		chkhdr(date, &ldate, &rdate);
		chkhdr(subject, &lsubject, &rsubject);
		l = lfrom + ldate + lsubject + 10;
		msgs[n] = salloc(l);
		sprintf(msgs[n],"%c%d %.*s%s %.*s%s", dc, n+1, lfrom, from->s0,
			rfrom, lsubject, subject->s0, rsubject);
	}
	msgs[n] = 0;
	return;
}

void
user_listSelection(rdWin *w, rdItemRange range)
{
	static char title[300];			/* XXX standard silly guess */
	int n = range.first;
	mMsg *m = mbox->msgs[n];
	mString *from = m->from;
	mWin *mw;
	char *text;

	if (artDisplayed(w, n))
		return;
	mw = allocMWin(mDispArt,n);
	sprintf(title," %s/%d", mbox->name, n+1);
	title[50] = 0;		/* XXX - to make sure it's not too long. */
	text = getarttext(m);
	getArtWin(n, mw, title, text, 0, 0, 0);
}

static char *
getarttext(mMsg *m)
{
	static char *text;
	static size_t textlen;
	size_t len;
	mString *body;

	body = &m->whole;
	len = body->s1 - body->s0;
	if (len >= textlen) {
		textlen = len + 1;
		text = srealloc(text, textlen);
	}
	if (show_all_headers) {
		memcpy(text, body->s0, len);
		text[len] = 0;
	} else {
		char *p = text;
		int n, l;
		mHdr *h;

		for (n = 0; n < m->nhdrs; n++) {
			h = m->hdrs[n];
			if (h->hide)
				continue;
			l = h->value.s1 - h->name.s0;
			memcpy(p, h->name.s0, l);
			p += l;
		}
		*p++ = '\n';		/* make sure there's a separating line */
		body = &m->body;
		len = body->s1 - body->s0;
		memcpy(p, body->s0, len);
		p[len] = 0;
	}
	return text;
}

int
artDisplayed(rdWin *w, int n)
{
	mWin *mw;

	for (mw = mwindows; mw; mw = mw->next)
		if (mw->kind == mDispArt && mw->msg == n)
			if ((w = userpWin(mw))) {
				rdGotoWin(w);
				return 1;
			}
	return 0;
}

static struct mCmd mcmds[] = {
	{ "quit", MCANY, MNOARG, mQuit },
	{ "exit", MCANY, MNOARG, mExit },
	{ "reply", MCNOTCOMP, MARG, mReply },
	{ "delete", MCNOTCOMP, MARG, mDelete },
	{ "undelete", MCMSGLIST, MARG, mUndelete },
	{ "comp", MCMSGLIST, MNOARG, mComp },
	{ "abort", MCCOMPART, MARG, mAbort },
	{ "deliver", MCCOMPART, MARG, mDeliver },
	{ "savefile", MCANY, MARG, mSavefile },
	{ "save", MCANY, MARG, mSave },
	{ "multiart", MCANY, MARG, mMultiart}, 
	{ "allheaders", MCANY, MARG, mAllheaders}, 
	{ "inc", MCCOMPART, MARG, mInclude}, 
	{ "incall", MCCOMPART, MARG, mIncludeall}, 
	{ "commit", MCANY, MNOARG, mCommit },
	{ "rescan", MCANY, MNOARG, mRescan },
	{ 0, 0, 0 }
};

void
user_cmdList(rdWin *w, char *cmd, ulong p0, ulong p1, rdItemRange r, char *arg)
{
	int c;

	fflush(stdout);

	for (c = 0; mcmds[c].cmd; c++)
		if (strcmp(cmd, mcmds[c].cmd) == 0)
			break;
	if (mcmds[c].cmd == 0) {
		fflush(stdout);
		winReflectCmd(w,cmd,arg);
		return;
	}
	/* check that the command is valid in a list like this */
	if ((mcmds[c].context & MCMSGLIST) == 0) {
		fprintf(stderr,"%s: only valid within the message list\n", cmd);
		return;
	}
	/* check we've been given an appropriate argument */
	if (mcmds[c].req == MARG && r.first == -1) {
		fprintf(stderr,"%s: needs a message\n", cmd);
		return;
	}
	(*mcmds[c].fn)(w, r.first, r.last, arg);
	return;
}

void
user_cmdArt(rdWin *w, char *cmd, ulong p0, ulong p1, char *arg)
{
	int c;
	mWin *mw;
	unsigned short context;

	fflush(stdout);

	for (c = 0; mcmds[c].cmd; c++)
		if (strcmp(cmd, mcmds[c].cmd) == 0)
			break;
	if (mcmds[c].cmd == 0) {
		fflush(stdout);
		winReflectCmd(w,cmd,arg);
		return;
	}

	/* check that the command is valid in this window */
	if ((mw = findMWin(w->userp)) == 0) {
		fprintf(stderr,"%s: invalid window argument\n",cmd);
		return;
	}
	context = (mw->kind == mDispArt)? MCDISPART : MCCOMPART;
	if ((mcmds[c].context & context) == 0) {
		fprintf(stderr,"%s: not valid in that window\n", cmd);
		return;
	}
	(*mcmds[c].fn)(w, mw->msg, mw->msg, arg);
	return;
}

void
mDelete(rdWin *w, int first, int last, char *arg)
{
	mWin *mw;
	rdWin *ow;
	int n, i;
	rdWin *lp = userpWin(listwin);

	for (i = first; i <= last; i++) {
		n = i;
		if ((mw = findMWin(w->userp)) == 0)
			return;
		if (mw->mbox->msgs[n]->deleted)
			continue;
		mw->mbox->msgs[n]->deleted = 1;
		*msgs[n] = 'D';
		rdChangeItem(lp, n, msgs[n]);
		mw->mbox->ndel++;
		if (mw->kind != mDispArt)		/* might be the msg list */
			for (mw = mwindows; mw; mw = mw->next)
				if (mw->kind == mDispArt && mw->msg == n)
					break;
		if (mw) {
			if ((ow = userpWin(mw)))		/* this msg is displayed; remove the pane */
				closeWin(ow);
			freeMWin(mw);
		}
	}
	return;
}

void
mUndelete(rdWin *w, int first, int last, char *arg)
{
	mWin *mw;
	int n, i;
	rdWin *lp = userpWin(listwin);

	for (i = first; i <= last; i++) {
		n = i;
		if ((mw = findMWin(w->userp)) == 0)
			return;
		if (!mw->mbox->msgs[n]->deleted)
			continue;
		mw->mbox->msgs[n]->deleted = 0;
		mw->mbox->ndel--;
		*msgs[n] = ' ';
		rdChangeItem(lp, n, msgs[n]);
	}
	return;
}

void
mReply(rdWin *w, int first, int last, char *arg)
{
	int n = first;		/* last ignored */
	mWin *mw = allocMWin(mCompArt, n);
	char *title = "Reply abort deliver ";
	static char body[300];		/* XXX */
	mString *from = mbox->msgs[n]->from;
	mString *subject = mbox->msgs[n]->subject;
	int lfrom, lsubject;
	char *me = getenv("USER");
	char *re = "Re: ";

	if (!me || !*me)
		me = "";
	lfrom = (from->s1 - from->s0);		/* include newline */
	lsubject = (subject->s1 - subject->s0);
	if (lsubject > 4 && strncmp(subject->s0, re, 4)==0)
		re = "";

	sprintf(body,"From: %s\nTo: %.*sSubject: %s%.*s\n",
		me, lfrom, from->s0, re, lsubject, subject->s0);
	if (getArtWin(n, mw, title, body, 0, 0, 1))
		return;			/* XXX - should clear up */
	return;
}

void
mAbort(rdWin *w, int first, int last, char *arg)
{
	mWin *mw;

	if ((mw = findMWin(w->userp)) == 0)
		return;
	if (mw->kind != mCompArt)
		return;
	fprintf(stderr,"Message aborted\n");
	closeWin(w);
	freeMWin(mw);
	return;
}

void
mDeliver(rdWin *w, int first, int last, char *arg)
{
	mWin *mw;
	char *filename = tmpnam((char *)0);

	if ((mw = findMWin(w->userp)) == 0)
		return;
	if (mw->kind != mCompArt)
		return;
	rdBodyToFile(w, filename);
	dodeliver(w,filename);
	return;
}

void
mQuit(rdWin *w, int first, int last, char *arg)
{
	update_mbox(mbox);
	mExit(w, first, last, arg);
}


void
mExit(rdWin *w, int first, int last, char *arg)
{
	mWin *mw;
	rdWin *rw;


	/* close msg windows first */
	for (mw = mwindows; mw; mw = mw->next)
		if (mw->kind == mCompArt || mw->kind == mDispArt)
			if ((rw = userpWin(mw)))
				closeWin(rw);
	for (mw = mwindows; mw; mw = mw->next)
		if (mw->kind != mCompArt && mw->kind != mDispArt)
			if ((rw = userpWin(mw)))
				closeWin(rw);
	exit(0);		/* not actually needed - reader will exit for us */
}

void
mComp(rdWin *w, int first, int last, char *arg)
{
	int n = first;			/* last ignored */
	mWin *mw = allocMWin(mCompArt, n);
	char *title = "Compose abort deliver ";
	static char body[300];		/* XXX */
	char *me = getenv("USER");

	if (!me || !*me)
		me = "";

	sprintf(body,"From: %s\nTo: \nSubject: \n\n", me);
	if (getArtWin(n, mw, title, body, 0, 0, 1))
		return;			/* XXX - should clear up */
	return;
}

void
dodeliver(rdWin *w, char *filename)
{
	static char cmd[100];		/* XXX another guess */
	mWin *mw;

	if ((mw = findMWin(w->userp)) == 0)
		return;
	sprintf(cmd,"%s < %s", MTU, filename);
	fflush(stdout);
	system(cmd);
	closeWin(w);
	freeMWin(mw);
	return;
}

void
mSavefile(rdWin *w, int first, int last, char *arg)
{
	if (savefile)
		free(savefile);
	savefile = sstrdup(arg);
}

void
mSave(rdWin *w, int first, int last, char *arg)
{
	FILE *fp;
	mWin *mw;
	mMsg *m;
	mString *text;
	size_t len;
	int n;

	if (savefile == 0) {
		fprintf(stderr,"No savefile yet: 'savefile filename'\n");
		return;
	}
	if ((fp = fopen(savefile,"a")) == 0) {
		perror(savefile);
		return;
	}

	if ((mw = findMWin(w->userp)) == 0)
		return;
	for (n = first; n <= last; n++) {
		m = mw->mbox->msgs[n];
		text = &m->whole;
		len = text->s1 - text->s0;
	
		if (fwrite(text->s0, (size_t)1, len, fp) != len)
			perror("fwrite");
		fputc('\n',fp);
		printf("Written message %d to %s\n", n+1, savefile);	/* count from 1 */
	}
	fflush(stdout);
	fclose(fp);
}

void
mMultiart(rdWin *w, int first, int last, char *arg)
{
	multiart = !multiart;
	rdSetMulti(0, multiart);
}

void
mAllheaders(rdWin *w, int first, int last, char *arg)
{
	show_all_headers = !show_all_headers;
}

void
mInclude(rdWin *w, int first, int last, char *arg)
{
	doinclude(w,first,last,arg,0);
}

void
mIncludeall(rdWin *w, int first, int last, char *arg)
{
	doinclude(w,first,last,arg,1);
}


void
doinclude(rdWin *w, int first, int last, char *arg, int all)
{
	mWin *mw;
	rdWin *ow;
	int msgnum;		/* message we're going to include */
	mMsg *m;
	mString *s;
	size_t len;
	int n;

	if ((mw = findMWin(w->userp)) == 0)
		return;
	if (mw->kind != mCompArt)
		return;
	msgnum = mw->msg;	/* default value */
	if (*arg) {
		if ((msgnum = atoi(arg)) == 0) {
			fprintf(stderr,"'%s' is an invalid message number\n", arg);
			return;
		} else {
			msgnum--;
		}
	}
	if (msgnum >= mw->mbox->nmsgs) {
		fprintf(stderr,"Only %d messages\n", mw->mbox->nmsgs);
		return;
	}
	m = mw->mbox->msgs[msgnum];
	s = all? &(m->whole) : &(m->body);
	len = s->s1 - s->s0;
	rdInclude(w, s->s0, len);
}

void
mCommit(rdWin *w, int first, int last, char *arg)
{
	update_mbox(mbox);
	build_msg_list();
	changeItems(userpWin(listwin), msgs, 0);
}

void
dorescan(void)
{
	mRescan((rdWin *)0, 0, 0, (char *)0);
}

void
mRescan(rdWin *w, int first, int last, char *arg)
{
	rdWin *lp = userpWin(listwin);
	int n = mbox->nmsgs;

	extend_mbox(mbox);
	if (n >= mbox->nmsgs)
		return;
	build_msg_list();
	while (n < mbox->nmsgs)
		rdAddItem(lp, msgs[n++]);
}
