#include "mailheaders.h"


static void build_msg_list();
static char *getarttext(mMsg *m);
mWin *allocMWin(int kind, int num);
void freeMWin(mWin *w);
mWin *findMWin(void *ptr);
int artDisplayed(int n);
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
void mAllheaders(rdWin *w, int first, int last, char *arg);
void mIncludeall(rdWin *w, int first, int last, char *arg);
void mInclude(rdWin *w, int first, int last, char *arg);
void doinclude(rdWin *w, int first, int last, char *arg, int all);
void mCommit(rdWin *w, int first, int last, char *arg);
void mRescan(rdWin *w, int first, int last, char *arg);
void mNext(rdWin *w, int first, int last, char *arg);
void mPrev(rdWin *w, int first, int last, char *arg);
static void chkhdr(mString *s, int *len, int max, char **rest);

mMbox *mbox;
static char mboxname[MAXPATHLEN];
mWin *mwindows;
mWin *listwin;
rdWin *rdlistwin;

static char **msgs;
static char *savefile;
static int multiart = 0;
static int show_all_headers = 0;
static int whichfrom = WHICHFROM;

static char *list_tools = " delete undelete comp reply quit exit commit rescan savefile  ";
static char *art_tools = " next prev delete reply save inc ";
static char *comp_tools = " deliver abort inc incall ";
static char *version = "X-Mailer: Wilymail 0.4\n";
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

	if (readerInit()) {
		fprintf(stderr,"readerInit() failed\n");
		exit(1);
	}
	if ((rdlistwin = readerLoading(0, (void *)0, 0, "Wilymail")) == 0) {
		fprintf(stderr,"No list window\n");
		exit(1);
	}
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
	assert(mboxname);
	DPRINT("mailbox name is...");
	DPRINT(mboxname);

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
	assert(savefile);
	DPRINT("savefile is...");
	DPRINT(savefile);

	set_hdr_filter(nhidden_hdrs, hidden_hdrs);
	if ((mbox = read_mbox((const char *)mboxname, 0, mustexist)) == 0) {
		DPRINT("Failed to read mailbox");
		perror(mboxname);
		exit(1);
	}
	mw = listwin = allocMWin(mMsgList, 0);
	assert(mw);
	build_msg_list();
	if (setWinUser(rdlistwin, 0, (void *)mw) || setWinTitle(rdlistwin, mboxname) ||
		setWinTools(rdlistwin, list_tools) || setWinList(rdlistwin, msgs)) {
		DPRINT("Failed to initialise reader");
		exit(1);
	}
	rdSetRescanTimer(timer);
	readerMainLoop();
	DPRINT("readerMainLoop() returned");
	exit(0);
}

mWin *
allocMWin(int kind, int num)
{
	mWin *w = salloc(sizeof(*w));
	mWin **p = &mwindows;

	assert(kind == mMsgList || kind == mDispArt || kind == mCompArt);
	assert(num >= -1);
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

	assert(w);
	while (*p != w)
		p = &((*p)->next);
	*p = w->next;
	free(w);
}

mWin *
findMWin(void *ptr)
{
	mWin *mw = mwindows;

	assert(ptr);
	while (mw && (mWin *)ptr != mw)
		mw = mw->next;
	return mw;
}

static void
chkhdr(mString *s, int *len, int max, char **rest)
{
	int l;

	assert(len);
	assert(rest);
	*rest = "";			/* default */
	if (s == 0) {
		*len = 0;
		return;
	}
	assert(s->s0 && s->s1);
	l = s->s1 - s->s0;
	if (l > max) {
		l = max;
		*rest = "...";
	} else if (l > 0)
		l--;		/* skip newline */
	*len = l;
}

static void
parsefrom(mString *s, char **from, int *len, char **rest)
{
	char *email, *name, *str;
	int max;

	*rest = "";
	*from = "";
	*len = 0;
	if (s == 0)
		return;
	parseaddr(s->s0, s->s1-s->s0, &email, &name);
	str = (whichfrom == FROMEMAIL)? email : name;
	max = (whichfrom == FROMEMAIL)? MAXEMAIL : MAXFULLNAME;
	if (str == 0)
		return;
	*from = str;
	if ((*len = strlen(str)) > max) {
		*len = max;
		*rest = "...";
	}
	return;
}

static void
build_msg_list(void)
{
	int n;
	mMsg *m;
	ulong l;
	int lfrom, ldate, lsubject;
	char *fromstr, *rfrom, *rdate, *rsubject;
	mString *from;
	mString *date;
	mString *subject;
	int dc;

	assert(mbox);
	assert(mbox->nmsgs >= 0);
	if (msgs)
		for (n = 0; msgs[n]; n++)
			free(msgs[n]);
	msgs = (char **)srealloc(msgs, (mbox->nmsgs+1) * sizeof(char *));
	for (n = 0; n < mbox->nmsgs; n++) {
		m = mbox->msgs[n];
		assert(m);
		dc = m->deleted? 'D' : ' ';
		from = m->from;
		date = m->date;
		subject = m->subject;
		parsefrom(from, &fromstr, &lfrom, &rfrom);
		chkhdr(date, &ldate, MAXDATE, &rdate);
		chkhdr(subject, &lsubject, MAXSUBJECT, &rsubject);
		l = lfrom + ldate + lsubject + 10;
		msgs[n] = salloc(l);
		sprintf(msgs[n],"%c%d %.*s%s %.*s%s", dc, n+1, lfrom, fromstr,
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

	assert(w);				/* XXX not sure if this is right */
	DPRINT("User selected an article");
	if (artDisplayed(n)) {
		DPRINT("Article is already displayed");
		return;
	}
	DPRINT("Displaying article");
	if (w && w->wintype == rdList)
		w = 0;
	mw = allocMWin(mDispArt,n);
	sprintf(title," %s/%d", mbox->name, n+1);
	title[50] = 0;		/* XXX - to make sure it's not too long. */
	text = getarttext(m);
	assert(text);
	getArtWin(n, mw, title, art_tools, text, w);
}

static char *
getarttext(mMsg *m)
{
	static char *text;
	static size_t textlen;
	size_t len;
	mString *body;

	assert(m);
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
artDisplayed(int n)
{
	rdWin *w;
	mWin *mw;

	assert(n >= 0);
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
	{ "allheaders", MCANY, MARG, mAllheaders}, 
	{ "inc", MCCOMPART, MARG, mInclude}, 
	{ "incall", MCCOMPART, MARG, mIncludeall}, 
	{ "commit", MCANY, MNOARG, mCommit },
	{ "rescan", MCANY, MNOARG, mRescan },
	{ "next", MCANY, MNOARG, mNext },
	{ "prev", MCANY, MNOARG, mPrev },
	{ 0, 0, 0, 0 }
};

void
user_cmdList(rdWin *w, char *cmd, ulong p0, ulong p1, rdItemRange r, char *arg)
{
	int c;

	assert(w);
	assert(cmd);

	DPRINT("User entered a command in a list window...");
	DPRINT(cmd);
	for (c = 0; mcmds[c].cmd; c++)
		if (strcmp(cmd, mcmds[c].cmd) == 0)
			break;
	if (mcmds[c].cmd == 0) {
		DPRINT("Command is not recognised - reflecting");
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
	DPRINT("Calling chosen function...");
	(*mcmds[c].fn)(w, r.first, r.last, arg);
	DPRINT("Function complete");
	return;
}

void
user_cmdArt(rdWin *w, char *cmd, ulong p0, ulong p1, char *arg)
{
	int c;
	mWin *mw;
	unsigned short context;

	assert(w);
	assert(cmd);
	DPRINT("User entered a command in an article window...");
	DPRINT(cmd);

	for (c = 0; mcmds[c].cmd; c++)
		if (strcmp(cmd, mcmds[c].cmd) == 0)
			break;
	if (mcmds[c].cmd == 0) {
		DPRINT("Command not recognised - reflecting");
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
	DPRINT("Calling selected function...");
	(*mcmds[c].fn)(w, mw->msg, mw->msg, arg);
	DPRINT("Called function complete");
	return;
}

void
mDelete(rdWin *w, int first, int last, char *arg)
{
	mWin *mw;
	rdWin *ow;
	int n, i;
	rdWin *lp = userpWin(listwin);

	assert(w);
	assert(lp);
	assert(first >= 0 && last >= 0 && first <= last);
	DPRINT("Deleting articles");
	for (i = first; i <= last; i++) {
		n = i;
		if ((mw = findMWin(w->userp)) == 0) {
			DPRINT("No mWin found for rdWin - aborting deletion");
			return;
		}
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
			if ((ow = userpWin(mw)))	{	/* this msg is displayed; remove the pane */
				DPRINT("Article has open window - closing");
				closeWin(ow);
			}
			DPRINT("Article has allocated mWin - freeing");
			freeMWin(mw);
		}
	}
	DPRINT("Articles deleted");
	return;
}

void
user_delWin(rdWin *w)
{
	assert(w);
	DPRINT("Reader window was closed - freeing mWin");
	freeMWin(findMWin(w->userp));
}

void
mUndelete(rdWin *w, int first, int last, char *arg)
{
	mWin *mw;
	int n, i;
	rdWin *lp = userpWin(listwin);

	assert(w);
	assert(lp);
	DPRINT("Undeleting articles");
	for (i = first; i <= last; i++) {
		n = i;
		if ((mw = findMWin(w->userp)) == 0) {
			DPRINT("nWin not found for window - aborting");
			return;
		}
		if (!mw->mbox->msgs[n]->deleted)
			continue;
		mw->mbox->msgs[n]->deleted = 0;
		mw->mbox->ndel--;
		*msgs[n] = ' ';
		rdChangeItem(lp, n, msgs[n]);
	}
	DPRINT("Articles undeleted");
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

	assert(w);
	assert(mw);
	assert(from);
	assert(subject);

	DPRINT("Replying to an article");
	if (!me || !*me) {
		DPRINT("$USER is not valid - fill in from address yourself!");
		me = "";
	}
	lfrom = (from->s1 - from->s0);		/* include newline */
	lsubject = (subject->s1 - subject->s0);
	if (lsubject > 4 && strncmp(subject->s0, re, 4)==0)
		re = "";

	sprintf(body,"From: %s\nTo: %.*sSubject: %s%.*s%s\n",
		me, lfrom, from->s0, re, lsubject, subject->s0, version);
	if (getArtWin(n, mw, title, comp_tools, body, 0)) {
		DPRINT("Could not get Reply window - leaving a mess in memory");
		return;			/* XXX - should clear up */
	}
	DPRINT("Reply window ready");
	return;
}

void
mAbort(rdWin *w, int first, int last, char *arg)
{
	mWin *mw;

	assert(w);
	assert(w->userp);
	DPRINT("Aborting reply/comp");
	if ((mw = findMWin(w->userp)) == 0) {
		DPRINT("Can't find associated window - aborting abort");
		return;
	}
	if (mw->kind != mCompArt) {
		DPRINT("Abort isn't applied to a composition window - ignoring");
		return;
	}
	fprintf(stderr,"Message aborted\n");
	closeWin(w);
	freeMWin(mw);
	DPRINT("Abort done");
	return;
}

void
mDeliver(rdWin *w, int first, int last, char *arg)
{
	mWin *mw;
	char *filename = tmpnam((char *)0);

	assert(w);
	assert(w->userp);
	assert(filename);
	DPRINT("Delivering message");
	if ((mw = findMWin(w->userp)) == 0) {
		DPRINT("Couldn't find associated composition window - aborting");
		return;
	}
	if (mw->kind != mCompArt) {
		DPRINT("Deliver not applied to composition window - ignored");
		return;
	}
	rdBodyToFile(w, filename);
	dodeliver(w,filename);
	DPRINT("Deliver done");
	return;
}

void
mQuit(rdWin *w, int first, int last, char *arg)
{
	DPRINT("Quitting - first flushing mbox changes");
	update_mbox(mbox);
	mExit(w, first, last, arg);
}


void
mExit(rdWin *w, int first, int last, char *arg)
{
	mWin *mw;
	rdWin *rw;

	DPRINT("Exiting - closing msg windows");
	/* close msg windows first */
	for (mw = mwindows; mw; mw = mw->next)
		if (mw->kind == mCompArt || mw->kind == mDispArt) {
			if ((rw = userpWin(mw)))
				closeWin(rw);
			else {
				DPRINT("Could not find rdWin for mWin while closing");
			}
		}
	DPRINT("Closing list windows");
	for (mw = mwindows; mw; mw = mw->next)
		if (mw->kind != mCompArt && mw->kind != mDispArt) {
			if ((rw = userpWin(mw)))
				closeWin(rw);
			else {
				DPRINT("Could not find rdWin for mWin while closing");
			}
		}
	DPRINT("Managed to close everything without dying...");
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

	assert(mw);
	DPRINT("Composing new article");
	if (!me || !*me) {
		DPRINT("$USER isn't set - fill in From: field yourself!");
		me = "";
	}

	sprintf(body,"From: %s\nTo: \nSubject: \n%s\n", me, version);
	if (getArtWin(n, mw, title, comp_tools, body, 0)) {
		DPRINT("Couldn't get new composition window");
		return;			/* XXX - should clear up */
	}
	DPRINT("Composition window open");
	return;
}

void
dodeliver(rdWin *w, char *filename)
{
	static char cmd[100];		/* XXX another guess */
	mWin *mw;

	assert(w);
	assert(w->userp);
	assert(MTU && *MTU);
	assert(filename);
	DPRINT("Attempting to deliver an article");
	if ((mw = findMWin(w->userp)) == 0) {
		DPRINT("Could not find mWin for this window - aborting");
		return;
	}
	sprintf(cmd,"%s < %s", MTU, filename);
	DPRINT("Delivery command is....");
	DPRINT(cmd);
	fflush(stdout);
	system(cmd);
	closeWin(w);
	freeMWin(mw);
	DPRINT("Seems to have delivered");
	return;
}

void
mSavefile(rdWin *w, int first, int last, char *arg)
{
	assert(arg);
	if (savefile) {
		DPRINT("Savefile used to be....");
		DPRINT(savefile);
		free(savefile);
	}
	savefile = sstrdup(arg);
	assert(savefile);
	DPRINT("Savefile now set to...");
	DPRINT(savefile);
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

	assert(w);
	assert(w->userp);
	assert(first >= 0);
	assert(first <= last);
	assert(last <= mbox->nmsgs);
	DPRINT("Saving articles");

	if (savefile == 0) {
		fprintf(stderr,"No savefile yet: 'savefile filename'\n");
		return;
	}
	if ((fp = fopen(savefile,"a")) == 0) {
		perror(savefile);
		DPRINT("Could not open savefile - article not saved");
		return;
	}

	if ((mw = findMWin(w->userp)) == 0) {
		DPRINT("could not find mWin for indicated rdWin - not saved");
		return;
	}
	for (n = first; n <= last; n++) {
		m = mw->mbox->msgs[n];
		assert(m);
		text = &m->whole;
		assert(text->s1 && text->s0);
		len = text->s1 - text->s0;
	
		if (fwrite(text->s0, (size_t)1, len, fp) != len) {
			DPRINT("Failed to write the file");
			perror("fwrite");
		}
		fputc('\n',fp);
		printf("Written message %d to %s\n", n+1, savefile);	/* count from 1 */
	}
	fflush(stdout);
	fclose(fp);
}

void
mAllheaders(rdWin *w, int first, int last, char *arg)
{
	show_all_headers = !show_all_headers;
	DPRINT(show_all_headers? "All headers now on" : "All headers now off");
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

	assert(w);
	assert(w->userp);
	assert(arg);
	DPRINT("Including article text");
	if ((mw = findMWin(w->userp)) == 0) {
		DPRINT("Could not find mWin for rdWin - aborting");
		return;
	}
	if (mw->kind != mCompArt) {
		DPRINT("Not a composition window - include ignored");
		return;
	}
	msgnum = mw->msg;	/* default value */
	if (*arg) {
		if ((msgnum = atoi(arg)) < 1) {
			fprintf(stderr,"'%s' is an invalid message number\n", arg);
			return;
		} else {
			msgnum--;
		}
	}
	if (msgnum < 0 || msgnum >= mw->mbox->nmsgs) {
		fprintf(stderr,"Invalid msg number (1-%d)\n", mw->mbox->nmsgs);
		return;
	}
	m = mw->mbox->msgs[msgnum];
	assert(m);
	s = all? &(m->whole) : &(m->body);
	assert(s && s->s0 && s->s1);
	len = s->s1 - s->s0;
	rdInclude(w, s->s0, len);
	DPRINT("Done include");
}

void
mCommit(rdWin *w, int first, int last, char *arg)
{
	DPRINT("Doing commit - flushing any mailbox changes");
	update_mbox(mbox);
	DPRINT("Commit - rebuilding message list");
	build_msg_list();
	DPRINT("Commit - redrawing message list");
	setWinList(userpWin(listwin), msgs);
	DPRINT("Commit complete");
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

	assert(lp);
	DPRINT("Rescanning mailbox");
	extend_mbox(mbox);
	if (n >= mbox->nmsgs) {
		DPRINT("No new messages (might be less, though...)");
		return;
	}
	DPRINT("Rebuilding msg list");
	build_msg_list();
	DPRINT("Adding new messages to screen");
	while (n < mbox->nmsgs)
		rdAddItem(lp, msgs[n++]);
	DPRINT("Rescan complete");
}

static int
nextArt(rdWin *w, int *num, int next, int first)
{
	mWin *mw;
	int max = mbox->nmsgs-1;
	int n;

	assert(w);
	assert(w->userp);
	assert(num);
	mw = findMWin(w->userp);
	assert(mw);
	n = (mw->kind == mDispArt)? mw->msg : first;
	*num = n + (next? 1 : -1);
	return !((next && n < max) || (!next && n > 0));
}

void
mNext(rdWin *w, int first, int last, char *arg)
{
	rdItemRange r;

	if (nextArt(w, &r.first, 1, first))
		return;
	r.i0 = r.i1 = 0;
	user_listSelection(w, r);
}

void
mPrev(rdWin *w, int first, int last, char *arg)
{
	rdItemRange r;

	if (nextArt(w, &r.first, 0, first))
		return;
	r.i0 = r.i1 = 0;
	user_listSelection(w, r);
}

