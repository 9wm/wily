/*
 * Simplistic driver for an NNTP news client for wily.
 */

#include "newsheaders.h"
#define MAXFROMLEN MAXEMAIL
#define MAXSUBJLEN MAXSUBJECT

static int getGroupInfo(void);
static void countUnread(nGroup *g);
static int getGroupListItems(void);
static int groupItems(nGroup *g);
static void freeGroupItems(nGroup *g);
static int getLen(char *str, unsigned int max);
static void selectGroup(nWin *nw, rdWin *rdw, int item);
static void selectArticle(nWin *nw, rdWin *rdw, int item);
static void markread(nArticle *a, int read);
static void nQuit(rdWin *w, nGroup *g, int first, int last, char *arg);
static void nExit(rdWin *w, nGroup *g, int first, int last, char *arg);
static void nNext(rdWin *w, nGroup *g, int first, int last, char *arg);
static void nPrev(rdWin *w, nGroup *g, int first, int last, char *arg);
static void nArtMarkRead(rdWin *w, nGroup *g, int first, int last, char *arg);
static void nArtMarkUnread(rdWin *w, nGroup *g, int first, int last, char *arg);
static void nCatchup(rdWin *w, nGroup *g, int first, int last, char *arg);
static void markartsread(nGroup *g, int first, int last, int read);
static int nextGroup(rdWin *w, int next, int *num);
static nGroup *itemToGroup(int item);
static void displayGroupList(nGroup *g, nWin *newnw, rdWin *newrdw);
static void displayArticle(rdWin *w, nGroup *g, int i);


static char **grouplistitems;
static nGroup **grpptrs;
static nGroup *groups;
static int ngroups;
static nWin *listwin;
static rdWin *rdlistwin;
static nWin *nwindows;

static int whichfrom = WHICHFROM;

static char *grouplist_tools = " next quit post exit ";
static char *artlist_tools = " next prev catchup post ";
static char *artdisp_tools = " next prev followup reply follrep save ";

static struct nCmd ncmds[] = {
	{ "quit", NCANY, NNOARG, nQuit },
	{ "exit", NCANY, NNOARG, nExit },
	{ "next", NCDISPART, NNOARG, nNext },
	{ "prev", NCDISPART, NNOARG, nPrev },
	{ "markread", NCMSGLIST, NNOARG, nArtMarkRead },
	{ "markunread", NCMSGLIST, NNOARG, nArtMarkUnread },
	{ "catchup", NCMSGLIST, NNOARG, nCatchup },
	{ "reply", NCDISPART, NNOARG, nReply },
	{ "post", NCANY, NNOARG, nPost },
	{ "followup", NCDISPART, NNOARG, nFollowUp },
	{ "follrep", NCDISPART, NNOARG, nFollRep },
	{ "savefile", NCANY, NARG, nSavefile },
	{ "save", NCDISPART, NNOARG, nSave },
	{ "deliver", NCCOMPART, NNOARG, nDeliver },
	{ "abort", NCCOMPART, NNOARG, nAbort },
	{ "inc", NCCOMPART, NNOARG, nInclude },
	{ "incall", NCCOMPART, NNOARG, nIncludeall },
	{ 0, 0, 0, 0 },
};

int
main(int argc, char *argv[])
{
	nWin *nw;

	if (readerInit() || (rdlistwin = readerLoading(0,(void *)0, 0, "News")) == 0) {
		fprintf(stderr,"Could not initialise reader\n");
		exit(1);
	}
	if ((groups = read_newsrc()) == 0) {
		fprintf(stderr,"Couldn't read .newsrc\n");
		return 1;
	}
	unlock_newsrc();
	if (nntpConnect()) {
		fprintf(stderr,"Could not connect to news server\n");
		return 1;
	}
	if (getGroupInfo()) {
		fprintf(stderr,"Could not get group information\n");
		return 1;
	}
	if (getGroupListItems())
		return 1;
	DPRINT("Initialising news reader");
	nw = listwin = allocNWin(nGroupList,0,0);
	if (setWinUser(rdlistwin, 0, (void *)nw) || setWinTitle(rdlistwin, "News") ||
		setWinTools(rdlistwin, grouplist_tools) || setWinList(rdlistwin, grouplistitems)) {
		DPRINT("Could not start up main list window");
		exit(1);
	}
	DPRINT("Entering main loop");
	readerMainLoop();
	DPRINT("Main loop exited!");
	nntpQuit();
	return 0;
}

/*
 * Find out how many articles there are in each group that we haven't read.
 */

static int
getGroupInfo(void)
{
	nGroup *g;
	int x = 0;

	(void)rdAddItem(rdlistwin, "Scanning...");
	for (g = groups; g; g = g->next) {
		if (!g->issub || nntpSetGroup(g) || g->last==0 || (g->last < g->first))
			g->first = g->last = g->nunread = 0;
		else
			countUnread(g);
		if (LOADDISP && g->issub && x-- == 0) {
			(void)rdChangeItem(rdlistwin, 0, g->name);
			x = LOADDISP;
		}
	}
	fputc('\n',stderr);
	return 0;
}

static void
countUnread(nGroup *g)
{
	int f, l, p0 = 0, p1 = 0, p2 = 0;
	nRange *r;

	assert(g);
	f = g->first;
	l = g->last;
	for (r = g->read; r; r = r->next)
		if (f <= r->n0 && r->n1 <= l)
			p1 += r->n1 - r->n0 + 1;
		else if (r->n0 <= f && f <= r->n1)
			p0 = r->n1 - f + 1;
		else if (r->n0 <= l && l <= r->n1)
			p2 = l - r->n0 + 1;
	g->nunread = (l - f + 1) - (p0 + p1 + p2);
	if (g->nunread < 0)
		g->nunread = 0;
}

/*
 * create the list of strings that we display in a group's subject list window.
 */

static int
groupItems(nGroup *g)
{
	int n, i;
	nArticle *a;

	assert(g);
	if (g->items) {
		freeGroupItems(g);
		g->items = 0;
	}
	if ((n = g->nunread) == 0)
		return 0;
	g->items = salloc((n+1) * sizeof(g->items[0]));
	g->artptrs = salloc((n+1) * sizeof(g->artptrs[0]));
	for (a = g->arts, i = 0; a && i < n; a = a->next) {
		int l, froml, subjl;
		char *from, *email, *name;
		if (a->read)
			continue;
		g->artptrs[i] = a;
		parseaddr(a->from, strlen(a->from), &email, &name);
		from = (whichfrom == FROMEMAIL)? email : name;
		froml = getLen(from, MAXFROMLEN);
		subjl = getLen(a->subj, MAXSUBJLEN);
		l = 10 + froml + subjl;
		g->items[i] = salloc(l);
		sprintf(g->items[i],"%c%-5d %.*s %.*s", a->read? 'R' : ' ',
			a->num, froml, from, subjl, a->subj);
		i++;
	}
	g->items[i] = 0;
	g->artptrs[i] = 0;
	return 0;
}

/*
 * Convert between item numbers in an artlist and article numbers.
 */

static int
itemToArtnum(nGroup *g, int item)
{
	assert(g && g->artptrs && g->artptrs[item]);
	return g->artptrs[item]->num;
}

int
artnumToItem(nGroup *g, int artnum)
{
	int item;

	assert(g && g->artptrs);
	for (item = 0; g->artptrs[item]; item++)
		if (g->artptrs[item]->num == artnum)
			return item;
	return -1;
}

/*
 * Get the listing of all groups
 */

static int
getGroupListItems(void)
{
	nGroup *g;
	int n;

	for (ngroups = 0, g = groups; g; g = g->next)
		if (g->nunread)
			ngroups++;
	n = ngroups + 1;
	grouplistitems = salloc(n * sizeof(*grouplistitems));
	grpptrs = salloc(n * sizeof(*grpptrs));
	for (n = 0, g = groups; g; g = g->next) {
		if (g->nunread == 0)
			continue;
		grouplistitems[n] = salloc(strlen(g->name) + 30);
		sprintf(grouplistitems[n],"%s %d articles", g->name, g->nunread);
		grpptrs[n] = g;
		n++;
	}
	grouplistitems[n] = 0;
	grpptrs[n] = 0;
	return 0;
}

static void
freeGroupItems(nGroup *g)
{
	int i;

	assert(g);
	if (g->items == 0)
		for (i = 0; g->items[i]; i++)
			free(g->items[i]);
	free(g->items);
	free(g->artptrs);
	g->items = 0;
	g->artptrs = 0;
}

static int
getLen(char *str, unsigned int max)
{
	int l;

	assert(str);
	l = strlen(str);
	return l < max? l : max;
}

void
dorescan(void)
{
	return;
}

void
user_cmdArt(rdWin *w, char *cmd, ulong p0, ulong p1, char *arg)
{
	int c;
	nWin *nw;
	unsigned short context;

	fflush(stdout);

	assert(w);
	assert(cmd);
	assert(arg);
	for (c = 0; ncmds[c].cmd; c++)
		if (strcmp(cmd, ncmds[c].cmd) == 0)
			break;
	if (ncmds[c].cmd == 0) {
		fflush(stdout);
		winReflectCmd(w,cmd,arg);
		return;
	}

	/* check that the command is valid in this window */
	if ((nw = findNWin(w->userp)) == 0) {
		fprintf(stderr,"%s: invalid window argument\n",cmd);
		return;
	}
	switch (nw->kind) {
		case nGroupList:	context = NCGRPLIST;	break;
		case nArtList:		context = NCMSGLIST;	break;
		default:
		case nDispArt:		context = NCDISPART;	break;
		case nCompArt:		context = NCCOMPART;	break;
	}
	if ((ncmds[c].context & context) == 0) {
		fprintf(stderr,"%s: not valid in that window\n", cmd);
		return;
	}
	DPRINT("executing command...");
	DPRINT(cmd);
	(*ncmds[c].fn)(w, nw->group, nw->artnum, nw->artnum, arg);
	DPRINT("Command complete");
	return;
}

void
user_cmdList(rdWin *w, char *cmd, ulong p0, ulong p1, rdItemRange r, char *arg)
{
	int c;
	unsigned short context;
	nWin *nw;

	assert(w);
	assert(cmd);
	assert(arg);
	nw = findNWin(w->userp);

	fflush(stdout);

	for (c = 0; ncmds[c].cmd; c++)
		if (strcmp(cmd, ncmds[c].cmd) == 0)
			break;
	if (ncmds[c].cmd == 0) {
		fflush(stdout);
		winReflectCmd(w,cmd,arg);
		return;
	}
	/* check that the command is valid in a list like this */
	context = (nw->kind == nGroupList)? NCGRPLIST : NCMSGLIST;
	if ((ncmds[c].context & context) == 0) {
		fprintf(stderr,"%s: only valid within  list windows\n", cmd);
		return;
	}
	/* check we've been given an appropriate argument */
	if (ncmds[c].req == NARG && r.first == -1) {
		fprintf(stderr,"%s: needs a message\n", cmd);
		return;
	}
	DPRINT("Calling command...");
	DPRINT(cmd);
	(*ncmds[c].fn)(w, nw->group, r.first, r.last, arg);
	DPRINT("Command complete");
	return;
}

void
user_listSelection(rdWin *w, rdItemRange r)
{
	nWin *nw;
	int item = r.first;
	assert(w);

	assert(nw = findNWin(w->userp));
	switch (nw->kind) {
		case nGroupList:
			selectGroup(nw, w, item);
			break;
		case nArtList:
			selectArticle(nw, w, item);
			break;
		default:
			/* Shouldn't reach here */
			assert(false);
	}
	return;
}

void
user_delWin(rdWin *w)
{
	nWin *nw;
	nArticle *a;

	assert(w);
	nw = findNWin(w->userp);
	assert(nw);

/*
 * XXX We *dont't* free the group and article information. Instead,
 * we burn memory, assuming that swap space is cheaper than
 * downloading the info each time. I'm probably going to hell
 * for this.
 * 
 * 	switch (nw->kind) {
 * 		case nArtList:
 * 			freeGroupItems(nw->group);
 * 			break;
 * 		case nDispArt:
 * 			for (a = nw->group->arts; a; a = a->next)
 * 				if (a->num == nw->artnum) {
 * 					free(a->body);
 * 					a->body = 0;
 * 					a->bodylen = 0;
 * 					break;
 * 				}
 * 			break;
 * 	}
 */
	freeNWin(nw);
}

/*
 * canWarp(isart, g, artnum, nw, rdw) - search for an open rdWin that
 * contains displayed article g/artnum (is isart) or artlist of group g.
 * If we find one, return true, and set nw/rdw. Otherwise, return false.
 */

static int
canWarp(int isart, nGroup *g, int artnum, nWin **nw, rdWin **rdw)
{
	nWin *nwin;
	rdWin *rdwin;

	for (nwin = nwindows; nwin; nwin = nwin->next)
		if ((nwin->kind == nArtList || nwin->kind == nDispArt) &&
			nwin->group == g && (isart == 0 || nwin->artnum == artnum))
			if ((rdwin = userpWin(nwin))) {
				*nw = nwin;
				*rdw = rdwin;
				return 1;
			}
	*nw = 0;
	*rdw = 0;
	return 0;
}

static nGroup *
itemToGroup(int item)
{
	assert(item >= 0 && item <= ngroups);
	assert(grpptrs && grpptrs[item]);
	return grpptrs[item];
}

static void
selectGroup(nWin *nw, rdWin *rdw, int item)
{
	nGroup *g;
	nWin *newnw;
	rdWin *newrdw;

	assert(g = itemToGroup(item));
	if (canWarp(0, g, 0, &newnw, &newrdw)) {
		warpToWin(newrdw);
		return;
	}
	displayGroupList(g,newnw, newrdw);
	return;
}

static void
displayGroupList(nGroup *g, nWin *newnw, rdWin *newrdw)
{
	if (newnw == 0)
		assert(newnw = allocNWin(nArtList, g, 0));
	if (newrdw == 0)
		assert(newrdw = getBlankListWin(0,(void *)newnw));
	newnw->group = g;
	setWinTitle(newrdw, g->name);
	setWinTools(newrdw, artlist_tools);
	if (g->items == 0) {
		if (nntpGroupHdrs(g)) {
			DPRINT("Could not get group headers");
			return;
		}
		if (groupItems(g)) {
			DPRINT("Could not get group items");
			return;
		}
	}
	setWinList(newrdw, g->items);
	return;
}

static void
selectArticle(nWin *nw, rdWin *rdw, int item)
{
	nGroup *g;
	int artnum;
	nWin *newnw;
	rdWin *newrdw;
	nArticle *a;

	assert(nw && nw->kind == nArtList);
	g = nw->group;
	artnum = itemToArtnum(g, item);
	if (canWarp(1, g, artnum, &newnw, &newrdw)) {
		warpToWin(newrdw);
		return;
	}
	assert(newnw = allocNWin(nDispArt, g, artnum));
	assert(newrdw = getBlankArtWin(item, (void *)newnw));
	displayArticle(newrdw, g, item);
	return;
}

static void
markread(nArticle *a, int read)
{
	nGroup *g = a->group;
	int i;
	nWin *nw;
	rdWin *w;

	assert(a);
	g = a->group;
	if (a->read == read)
		return;
	a->read = read;
	if  (read)
		newsrcMarkRead(g, a->num);
	else
		newsrcMarkUnread(g, a->num);
	i = artnumToItem(g, a->num);
	assert(i >= 0);
	g->items[i][0] = read? 'R' : ' ';
	for (nw = nwindows; nw; nw = nw->next)
		if (nw->kind == nArtList && nw->group == g)
			break;
	if (nw == 0) {
		DPRINT("No window found to mark article as read in");
		return;		/* user might have closed the window */
	}
	w = userpWin(nw);
	assert(w);
	rdChangeItem(w, i, g->items[i]);
}

nWin *
allocNWin(int kind, nGroup *g, int num)
{
	nWin *n = salloc(sizeof(*n));
	nWin **p = &nwindows;

	n->kind = kind;
	n->group = g;
	n->artnum = num;
	n->isrep = n->ispost = 0;
	n->next = 0;
	while (*p)
		p = &((*p)->next);
	*p = n;
	return n;
}

void
freeNWin(nWin *w)
{
	nWin **p = &nwindows;

	assert(w);
	while (*p != w) {
		assert (*p);
		p = &((*p)->next);
	}
	*p = w->next;
	free(w);
}

nWin *
findNWin(void *ptr)
{
	nWin *nw = nwindows;

	assert(ptr);
	while (nw && (nWin *)ptr != nw)
		nw = nw->next;
	return nw;
}


static void
nQuit(rdWin *w, nGroup *g, int first, int last, char *arg)
{	
	DPRINT("Quitting");
	update_newsrc(groups);
	nExit(w, g, first, last, arg);
}

static void
nExit(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	nWin *nw;
	rdWin *rw;

	assert(w);
	assert(arg);
	unlock_newsrc();
	/* close msg windows first */
	for (nw = nwindows; nw; nw = nw->next)
		if (nw->kind == nCompArt || nw->kind == nDispArt)
			if ((rw = userpWin(nw)))
				closeWin(rw);
	for (nw = nwindows; nw; nw = nw->next)
		if (nw->kind != nCompArt && nw->kind != nDispArt)
			if ((rw = userpWin(nw)))
				closeWin(rw);
	exit(0);		/* not actually needed - reader will exit for us */
}


static int
groupNum(nGroup *grp)
{
	int n;

	for (n = 0; n < ngroups; n++)
		if (grpptrs[n] == grp)
			return n;
	assert(false);		/* shouldn't run off the end */
}

/*
 * Scan the list of groups for one which had some unread articles in it.
 */

static nGroup *
findUnreadGroup(nGroup *grp, int fwd)
{
	int n = groupNum(grp);

	if ((fwd && n >= ngroups-1) || (!fwd && !n))
		return 0;		/* out of groups */
	n += fwd? 1 : -1;
	assert(n >= 0 && n < ngroups);
	assert(grpptrs[n]);
	return grpptrs[n];
}

/*
 * XXX - this function deals with numbers that refer to an article's position
 * in the list window (0-n), not article numbers (xxx-yyy).
 */

static int
nextArt(rdWin *w, nGroup **ng, int *num, int next)
{
	nWin *artwin, *gwin;
	rdWin *rwin;
	nGroup *g = *ng, *nextgroup;
	int max, item;

	assert(w);
	assert(artwin = findNWin(w->userp));
	max = g->nunread;
	item = artwin->itemnum;
	if ((next && item >= max-1) || (!next && !item)) {
		/* walking off one end of this newsgroup onto another (in either direction) */
		nextgroup = findUnreadGroup(g, next);
		if (nextgroup == 0)
			return -1;
		(void)canWarp(0, g, 0, &gwin, &rwin);	/* locate the group window */
		displayGroupList(nextgroup, gwin, rwin);
		artwin->group = nextgroup;
		/* item number of article is first(last) in group */
		if (next)
			item = 0;
		else
			item = nextgroup->nunread - 1;
		*ng = nextgroup;
	} else {
		/* still within this group - move to next article */
		item += next? 1 : -1;
	}
	*num = item;
	return 0;
}

/*
 * displayArticle(w,g,i) - display article which is item i in group g, in
 * window w.
 */

static void
displayArticle(rdWin *w, nGroup *g, int i)
{
	nArticle *a;
	int artnum;
	nWin *nw;
	static char title[200];			/* XXX standard guess */

	assert(w);
	assert(g);
	assert(nw = findNWin(w->userp));

	artnum = itemToArtnum(g,i);
	a = g->artptrs[i];
	if (a->body == 0)
		if (nntpGetArticle(a)) {
			DPRINT("Could not retrieve article");
			return;
		}
	sprintf(title,"%s/%d", g->name, artnum);
	setWinTitle(w, title);
	setWinTools(w, artdisp_tools);
	setWinArt(w, a->body);
	nw->itemnum = i;
	markread(a, 1);
	return;
}


void
nNext(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	assert(w);

	if (nextArt(w, &g, &first, 1)) {
		DPRINT("No more to select");
		return;
	}
	displayArticle(w,g,first);
}

void
nPrev(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	assert(w);

	if (nextArt(w, &g, &first, 0)) {
		DPRINT("No more to select");
		return;
	}
	displayArticle(w,g,first);
}

void
nArtMarkRead(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	markartsread(g, first, last, 1);
}

void
nArtMarkUnread(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	markartsread(g, first, last, 0);
}

void
nCatchup(rdWin *w, nGroup *g, int first, int last, char *arg)
{
	nWin *nw, *t;
	rdWin *rdw;
	nGroup *nextgroup;

	assert(g);
	markartsread(g, 0, g->nunread-1, 1);

	/* Close any articles displayed from this group */
	for (nw = nwindows; nw; nw = t) {
		t = nw->next;
		if (nw->kind == nDispArt && nw->group == g)
			if ((rdw = userpWin(nw)) != 0) {
				closeWin(rdw);
				freeNWin(nw);
			}
	}

	/* Move onto next group, if there is one. */
	if ((nextgroup = findUnreadGroup(g, 1)) == 0)
		return;
	displayGroupList(nextgroup, findNWin(w->userp), w);
	return;
}

static void
markartsread(nGroup *g, int first, int last, int read)
{
	int i;
	nArticle *a;

	assert(g);
	assert(0 <= first && first <= last);
	assert(read == 0 || read == 1);

	for (i = first; i <= last; i++) {
		DPRINT("Marking article as (un)read");
		if (g->artptrs[i])
			markread(g->artptrs[i], read);
	}
	return;
}
