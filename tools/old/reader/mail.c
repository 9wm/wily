/*
 * some notes on how this is arranged.
 * call readerInit(title, msgs, 0, 1, 0) to create things.
 * call readerMainLoop().
 * callbacks:
 *	user_listSelection(w,n);
 *		open new article win with getArtWin(title,text,0,0);
 *	user_cmdExec(w,builtin,cp0,cp1,cmd,p0,p1,arg);
 */
#include "headers.h"
#include "mbox.h"
#include "mail.h"


static void build_msg_list();
mWin *allocMWin(int kind, int num);
void freeMWin(mWin *w);
mWin *findMWin(void *ptr);
int artDisplayed(rdWin *w, int n);
void mDelete(rdWin *w, int n, char *arg);
void mUndelete(rdWin *w, int n, char *arg);
void mComp(rdWin *w, int n, char *arg);
void mExit(rdWin *w, int n, char *arg);
void mQuit(rdWin *w, int n, char *arg);
void mReply(rdWin *w, int n, char *arg);
void mAbort(rdWin *w, int n, char *arg);
void mDeliver(rdWin *w, int n, char *arg);
void mSavefile(rdWin *w, int n, char *arg);
void mSave(rdWin *w, int n, char *arg);
void dodeliver(rdWin *w, char *filename);
void mMultiart(rdWin *w, int n, char *arg);
void mIncludeall(rdWin *w, int n, char *arg);
void mInclude(rdWin *w, int n, char *arg);
void doinclude(rdWin *w, int n, char *arg, int all);


mMbox *mbox;
static char mboxname[FILENAME_MAX];
mWin *mwindows;
mWin *listwin;

static char **msgs;
static char *savefile;
static int multiart = 0;

int
main(int argc, char *argv[])
{
	mWin *mw;
	int ma = 0;

	printf("kill %d\n",getpid());
	fflush(stdout);
	if (argc >=2 && strcmp(argv[1], "-ma")==0) {
		ma = 1;
		argc--;
		argv++;
	}
	multiart = ma;
	if (argc >= 2)
		strcpy(mboxname, argv[1]);
	else
		sprintf(mboxname,"/var/mail/%s", getenv("USER"));
	if ((mbox = read_mbox((const char *)mboxname, 0)) == 0) {
		perror(mboxname);
		exit(1);
	}
	mw = listwin = allocMWin(mMsgList, 0);
	build_msg_list();
	if (readerInit(0, (char *)mw, mboxname, msgs, 0, ma, 0))
		exit(1);
	readerMainLoop();
	printf("Finished\n");
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
user_listSelection(rdWin *w, int n)
{
	static char title[300];
	mMsg *m = mbox->msgs[n];
	mString *from = m->from;
	mString *body = &m->whole;
	static char *text;
	static size_t textlen;
	size_t len;
	mWin *mw;

	if (artDisplayed(w, n))
		return;
	mw = allocMWin(mDispArt,n);
	sprintf(title," Mail/%d/%.*s ",n+1, from->s1 - from->s0, from->s0);
	title[50] = 0;		/* XXX - to make sure it's not too long. */
	len = body->s1 - body->s0;
	if (len >= textlen) {
		textlen = len + 1;
		text = srealloc(text, textlen);
	}
	memcpy(text, body->s0, len);
	text[len] = 0;
	getArtWin(n, mw, title, text, 0, 0);
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
	{ "quit", MCANY, mQuit },
	{ "exit", MCANY, mExit },
	{ "reply", MCNOTCOMP, mReply },
	{ "delete", MCNOTCOMP, mDelete },
	{ "undelete", MCMSGLIST, mUndelete },
	{ "comp", MCMSGLIST, mComp },
	{ "abort", MCCOMPART, mAbort },
	{ "deliver", MCCOMPART, mDeliver },
	{ "savefile", MCANY, mSavefile },
	{ "save", MCANY, mSave },
	{ "multiart", MCANY, mMultiart}, 
	{ "inc", MCCOMPART, mInclude}, 
	{ "incall", MCCOMPART, mIncludeall}, 
	{ 0, 0, 0 }
};

void
user_cmdList(rdWin *w, char *cmd, ulong p0, ulong p1, int n, char *arg)
{
	int c;

	printf("user_cmdList(w,%s,%lu,%lu, msg %d, %s)\n",
		cmd, p0, p1, n+1, arg);
	fflush(stdout);

	for (c = 0; mcmds[c].cmd; c++)
		if (strcmp(cmd, mcmds[c].cmd) == 0)
			break;
	if (mcmds[c].cmd == 0) {
		printf("%s reflected\n",cmd);
		fflush(stdout);
		winReflectCmd(w,cmd,arg);
		return;
	}
	/* check that the command is valid in a list like this */
	if ((mcmds[c].context & MCMSGLIST) == 0) {
		fprintf(stderr,"%s: only valid within the message list\n", cmd);
		return;
	}
	(*mcmds[c].fn)(w, n, arg);
	return;
}

void
user_cmdArt(rdWin *w, char *cmd, ulong p0, ulong p1, char *arg)
{
	int c;
	mWin *mw;
	unsigned short context;

	printf("user_cmdArt(w, %s, %lu, %lu, %s)\n", cmd, p0, p1, arg);
	fflush(stdout);

	for (c = 0; mcmds[c].cmd; c++)
		if (strcmp(cmd, mcmds[c].cmd) == 0)
			break;
	if (mcmds[c].cmd == 0) {
		printf("%s reflected\n",cmd);
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
	(*mcmds[c].fn)(w, mw->msg, arg);
	return;
}

void
mDelete(rdWin *w, int n, char *arg)
{
	mWin *mw;
	rdWin *ow;

	if ((mw = findMWin(w->userp)) == 0)
		return;
	if (mw->mbox->msgs[n]->deleted)
		return;
	mw->mbox->msgs[n]->deleted = 1;
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
	build_msg_list();
	changeItems(userpWin(listwin), msgs, 0);
	return;
}

void
mUndelete(rdWin *w, int n, char *arg)
{
	mWin *mw;

	if ((mw = findMWin(w->userp)) == 0)
		return;
	if (!mw->mbox->msgs[n]->deleted)
		return;
	mw->mbox->msgs[n]->deleted = 0;
	mw->mbox->ndel--;
	build_msg_list();
	changeItems(userpWin(listwin), msgs, 0);
	return;
}

void
mReply(rdWin *w, int n, char *arg)
{
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
	getArtWin(n, mw, title, body, 0, 0);
	return;
}

void
mAbort(rdWin *w, int n, char *arg)
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
mDeliver(rdWin *w, int n, char *arg)
{
	mWin *mw;
	char *filename = tmpnam((char *)0);

	if ((mw = findMWin(w->userp)) == 0)
		return;
	if (mw->kind != mCompArt)
		return;
	fprintf(stderr,"Attempting to deliver message");
	rdBodyToFile(w, filename);
	dodeliver(w,filename);
	return;
}

void
mQuit(rdWin *w, int n, char *arg)
{
	update_mbox(mbox);
	mExit(w, n, arg);
}


void
mExit(rdWin *w, int n, char *arg)
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
mComp(rdWin *w, int n, char *arg)
{
	mWin *mw = allocMWin(mCompArt, n);
	char *title = "Compose abort deliver ";
	static char body[300];		/* XXX */
	char *me = getenv("USER");

	if (!me || !*me)
		me = "";

	sprintf(body,"From: %s\nTo: \nSubject: \n\n", me);
	getArtWin(n, mw, title, body, 0, 0);
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
	printf("command: '%s'\n",cmd);
	fflush(stdout);
	system(cmd);
	closeWin(w);
	freeMWin(mw);
	return;
}

void
mSavefile(rdWin *w, int n, char *arg)
{
	if (savefile)
		free(savefile);
	savefile = sstrdup(arg);
}

void
mSave(rdWin *w, int n, char *arg)
{
	FILE *fp;
	mWin *mw;
	mMsg *m;
	mString *text;
	size_t len;

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
	m = mw->mbox->msgs[n];
	text = &m->whole;
	len = text->s1 - text->s0;

	if (fwrite(text->s0, (size_t)1, len, fp) != len)
		perror("fwrite");
	fputc('\n',fp);
	printf("Written message %d to %s\n", n, savefile);
	fflush(stdout);
	fclose(fp);
}

void
mMultiart(rdWin *w, int n, char *arg)
{
	multiart = !multiart;
	rdSetMulti(0, multiart);
}

void
mInclude(rdWin *w, int n, char *arg)
{
	doinclude(w,n,arg,0);
}

void
mIncludeall(rdWin *w, int n, char *arg)
{
	doinclude(w,n,arg,1);
}


void
doinclude(rdWin *w, int n, char *arg, int all)
{
	mWin *mw;
	rdWin *ow;
	int msgnum;		/* message we're going to include */
	mMsg *m;
	mString *s;
	size_t len;

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
