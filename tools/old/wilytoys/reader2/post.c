/*
 * This file handles posting of new articles, follow-ups to
 * existing ones, and emailed replies. It's ripped pretty
 * much as-is from the mailer.
 */
#include "newsheaders.h"

static void genpost(rdWin *w, nGroup *g, int num, char *to, char *ng, char *subj);
static char *getNewsGroups(char *text);


static char *version = "X-NewsReader: wilynews 0.1\n";
static char *post_tools = " deliver abort inc ";
static char *rep_tools = " deliver abort inc ";
static char *savefile;

static int
getWinNum(rdWin *w, nGroup *g, int first, char *arg)
{
	int n;
	nWin *nw;

	assert(w);
	assert(g);
	nw = findNWin(w->userp);
	assert(nw);
	if (nw->kind == nDispArt)
		return artnumToItem(g,nw->artnum);
	else if (arg && *arg && (n = atoi(arg)) && (n = artnumToItem(g,n)) != -1)
		return n;
	else
		return first;
}

void
nReply(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	int num;
	nArticle *a;
	char *from;

	DPRINT("Replying to an article");

	num = getWinNum(w,g,first,arg);
	assert(num >= 0);
	a = g->artptrs[num];
	assert(a);
	from = a->from;
	genpost(w, g, num, from, (char *)0, a->subj);
	return;
}

void
nFollowUp(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	int num;
	nArticle *a;
	char *groups;

	DPRINT("Following up an article");

	if (nntpCanPost() == 0) {
		fprintf(stderr,"Posting not allowed to this server\n");
		return;
	}
	num = getWinNum(w,g,first,arg);
	assert(num >= 0);
	a = g->artptrs[num];
	assert(a);
	if ((groups = getNewsGroups(a->body)) == 0) {
		DPRINT("Could not get list of groups for article");
		groups = "junk";
	}
	genpost(w, g, num, (char *)0, groups, a->subj);
	return;
}

void
nFollRep(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	int num;
	nArticle *a;
	char *groups;

	DPRINT("Following up and replying to an article");

	if (nntpCanPost() == 0) {
		fprintf(stderr,"Posting not allowed to this server\n");
		return;
	}
	num = getWinNum(w,g,first,arg);
	assert(num >= 0);
	a = g->artptrs[num];
	assert(a);
	if ((groups = getNewsGroups(a->body)) == 0) {
		DPRINT("Could not get list of groups for article");
		groups = "junk";
	}
	genpost(w, g, num, a->from, groups, a->subj);
	return;
}

void
nPost(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	DPRINT("Posting an article");

	if (nntpCanPost() == 0) {
		fprintf(stderr,"Posting not allowed to this server\n");
		return;
	}
	genpost(w,g,first, (char *)0, "", (char *)0);
	return;
}

static void
genpost(rdWin *w, nGroup *g, int num, char *to, char *newsgroups, char *subj)
{
	nWin *nw, *rnw;
	static char body[300];		/* XXX */
	char *me = getenv(FROMENV);
	char *re = subj? "Re: " : "";
	char *tohdr = to? "\nTo: " : "";
	char *nghdr = newsgroups? "\nNewsgroups: " : "";
	char *title = newsgroups? "Post" : "Reply";
	char *tools = newsgroups? post_tools : rep_tools;

	assert(w);

	nw = findNWin(w->userp);
	assert(nw);
	rnw = allocNWin(nCompArt, g, num);
	assert(rnw);
	rnw->isrep = (to != 0);
	rnw->ispost = (newsgroups != 0);
	if (!me)
		me = getenv("USER");
	if (!me || !*me) {
		DPRINT("$WILYFROM and $USER are not valid - fill in from address yourself!");
		me = "";
	}
	if (!subj || !*subj || strncmp(subj, "Re: ", 4)==0)
		re = "";

	sprintf(body,"From: %s%s%s%s%s\nSubject: %s%s\n%s\n",
		me, tohdr, to? to : "", nghdr, newsgroups? newsgroups : "", re, subj? subj : "", version);
	if (getArtWin(num, rnw, title, tools, body, 0)) {
		DPRINT("Could not get Reply window - leaving a mess in memory");
		return;			/* XXX - should clear up */
	}
	DPRINT("Reply window ready");
	return;
}

void
nAbort(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	nWin *nw;

	assert(w);
	assert(w->userp);
	DPRINT("Aborting reply/comp");
	if ((nw = findNWin(w->userp)) == 0) {
		DPRINT("Can't find associated window - aborting abort");
		return;
	}
	if (nw->kind != nCompArt) {
		DPRINT("Abort isn't applied to a composition window - ignoring");
		return;
	}
	fprintf(stderr,"Message aborted\n");
	closeWin(w);
	freeNWin(nw);
	DPRINT("Abort done");
	return;
}

void
nDeliver(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	nWin *nw;
	char *filename = tmpnam((char *)0);

	assert(w);
	assert(w->userp);
	assert(filename);
	DPRINT("Delivering message");
	if ((nw = findNWin(w->userp)) == 0) {
		DPRINT("Couldn't find associated composition window - aborting");
		return;
	}
	if (nw->kind != nCompArt) {
		DPRINT("Deliver not applied to composition window - ignored");
		return;
	}
	rdBodyToFile(w, filename);
	if (nw->isrep)
		dodeliver(w,filename);
	if (nw->ispost)
		nntpPost(filename);
	(void)remove(filename);
	DPRINT("Deliver done");
	closeWin(w);
	freeNWin(nw);
	return;
}


void
dodeliver(rdWin *w, char *filename)
{
	static char cmd[100];		/* XXX another guess */
	nWin *nw;

	assert(w);
	assert(w->userp);
	assert(MTU && *MTU);
	assert(filename);
	DPRINT("Attempting to deliver an article");
	if ((nw = findNWin(w->userp)) == 0) {
		DPRINT("Could not find nWin for this window - aborting");
		return;
	}
	sprintf(cmd,"%s < %s", MTU, filename);
	DPRINT("Delivery command is....");
	DPRINT(cmd);
	fflush(stdout);
	system(cmd);
	DPRINT("Seems to have delivered");
	return;
}

void
nSavefile(rdWin *w, nGroup *g, int first, int last, char *arg)
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
nSave(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	FILE *fp;
	nWin *nw;
	nArticle *a;
	int n;

	assert(w);
	assert(w->userp);
	assert(first >= 0);
	assert(first <= last);

	nw = findNWin(w->userp);
	assert(nw);
	assert(last <= g->nunread);
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

	for (n = first; n <= last; n++) {
		a = g->artptrs[n];
		assert(a);
	
		(void)fprintf(fp,"%s\n", a->body);
		printf("Written message %d to %s\n", n+1, savefile);	/* count from 1 */
	}
	fflush(stdout);
	fclose(fp);
}

void
nInclude(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	doinclude(w,g,first,last,arg,0);
}

void
nIncludeall(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	doinclude(w,g,first,last,arg,1);
}


void
doinclude(rdWin *w, nGroup *g, int first, int last, char *arg, int all)
{
	nWin *nw;
	rdWin *ow;
	int msgnum;		/* message we're going to include */
	nArticle *a;
	char *s;
	int n;

	assert(w);
	assert(w->userp);
	assert(arg);
	DPRINT("Including article text");
	if ((nw = findNWin(w->userp)) == 0) {
		DPRINT("Could not find nWin for rdWin - aborting");
		return;
	}
	if (nw->kind != nCompArt) {
		DPRINT("Not a composition window - include ignored");
		return;
	}
	msgnum = nw->artnum;	/* default value */
	if (*arg) {
		if ((msgnum = atoi(arg)) < 1) {
			fprintf(stderr,"'%s' is an invalid message number\n", arg);
			return;
		} else {
			msgnum = artnumToItem(g, msgnum);
		}
	}
	if (msgnum < 0 || msgnum >= g->nunread) {
		fprintf(stderr,"Invalid msg number (%d-%d)\n", g->first, g->last);
		return;
	}
	a = g->artptrs[msgnum];
	assert(a);
	s = all? a->body : a->body;		/* XXX should have one option skip headers */
	assert(s);
	rdInclude(w, s, strlen(s));
	DPRINT("Done include");
}

static char *
getNewsGroups(char *text)
{
	static char *buff;
	char *s, *t, *ng = "\nNewsgroups: ";
	size_t len = strlen(ng);

	if (buff)
		free(buff);
	if ((s = strstr(text, ng)))
		s += len;
	else if (strncmp(text, ng+1, len-1) == 0)
		s = text + len - 1;
	else
		return 0;
	if ((t = strchr(s, '\n')) == 0)
		return 0;
	len = t - s + 1;
	buff = salloc(len);
	strncpy(buff, s, len);
	buff[len-1] = 0;
	return buff;
}

