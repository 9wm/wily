/*
 * reader.c - functions for handling news/article reader windows.
 */

#include "headers.h"

Bool debug = false;
Bool frommessage = true;


/*
 * The windows currently active.
 */

rdWin *windows = 0;

/*
 * General wily comms stuff.
 */

Mqueue realwilyq;
Mqueue *wilyq = &realwilyq;
Fd wilyfd;

/*
 * Amount of time we wait before causing a rescan event, in seconds.
 * If zero, then we don't rescan.
 */

static int rdRescanTimer = 0;
Bool rdAlarmEvent = false;
static void (*old_alarm_handler)(int) = 0;

/*
 * default contents of tags
 */

char *rdListTagStr = "  ";
char *rdArtTagStr = "  ";

static rdWin *allocWin(rdWinType kind, const char *title);
static int connectWin(rdWin *w, char *filename);
static rdWin *getWin(int user, void *userp, rdWinType kind);
static void clearWin(rdWin *w);
static void freeItems(rdWin *w);
static int initWin(int user, void *userp, rdWin *w, char *title, char *tools);
static void anyEvent(rdWin *w);
static void winDelete(rdWin *w, int which, ulong p0, ulong p1);
static void winInsert(rdWin *w, int which, ulong p, char *str);
static void updateBody(rdWin *w, ulong p0, ulong p1, char *str);
static void winGoto(rdWin *w, ulong p0, char *str);
static void winExec(rdWin *w, char *cmd, char *arg);
static void DelWin(rdWin *w, char *arg);
static int rdBuiltin(rdWin *w, char *cmd, char *str);
static void addrEvent(rdWin *w);
static void bodyEvent(rdWin *w);
static size_t countlines(char *str, size_t len);
static void prefixlines(char *str, size_t len, char *prefix, size_t plen, char *buf);
static void alarm_handler(int signal);
static void set_sig(void);
static void updateItems(rdWin *w, rdItem *i, int len);

/*
 * allocWin(kind, title) - create a new, blank window of a given kind.
 */

static rdWin *
allocWin(rdWinType kind, const char *title)
{
	rdWin *w = salloc(sizeof(*w));

	assert(title);
	w->title = sstrdup((char *)title);
	w->wintype = kind;
	w->id = -1;
	w->m = 0;
	w->items = 0;
	w->body = 0;
	w->taglen = w->bodylen = 0;
	w->next = windows;
	windows = w;
	return w;
}

/*
 * connectWin(w, filename) - connect to wily, to create a new window.
 * If filename is true, then we get wily to open the given file. Otherwise,
 * we ask for a new, blank window.
 * Returns 0 for success.
 */

static int
connectWin(rdWin *w, char *filename)
{
	int fd;

	assert(w);
	if (filename == 0)
		filename = "New";
	if (rpc_new(wilyq, &w->id, filename, false) < 0) {
		DPRINT("Could not get new window from wily");
		freeWin(w);
		return -1;
	}
	return 0;
}

/*
 * setWinUser(w, user, userp) - change the user info for this window.
 */

int
setWinUser(rdWin *w, int user, void *userp)
{
	assert(w);
	w->user = user;
	w->userp = userp;
	return 0;
}

/*
 * randomTitle() - generate a one-off string to be the title for
 * this window, until we're given one by the user program.
 * XXX this is a static string, which is copied when it's written
 * into the window structure.
 */

static char *
randomTitle(void)
{
	int pid = getpid();
	static int num;
	static char title[30];			/* XXX - usual guess */

	sprintf(title,"+rdwin%d-%d", pid, num++);
	return title;
}

/*
 * setWinTitle(w, title) - set the title of a window.
 */

int
setWinTitle(rdWin *w, char *title)
{
	assert(w);
	assert(title);

	if (rpc_setname(wilyq, w->id, title)) {
		DPRINT("Wily couldn't set window title");
		return 1;
	}
	if (w->title)
		free(w->title);
	assert(w->title = sstrdup(title));
	return 0;
}

/*
 * setWinTools(w, tools) - set the tools for a window.
 */

int
setWinTools(rdWin *w, char *tools)
{
	assert(w);
	assert(tools);

	if (rpc_settag(wilyq, w->id, tools)) {
		DPRINT("wily couldn't set window tools");
		return 1;
	}
	w->taglen = strlen(tools);
	return 0;
}

/*
 * getBlankArtWin(user, userp) - grab an article window.
 */

rdWin *
getBlankArtWin(int user, void *userp)
{
	return getWin(user, userp, rdArticle);
}

/*
 * getBlankListWin(user, userp) - grab a list window.
 */

rdWin *
getBlankListWin(int user, void *userp)
{
	return getWin(user, userp, rdList);
}

/*
 * getWin(user, userp, kind) - create a new window.
 */

rdWin *
getWin(int user, void *userp, rdWinType kind)
{
	rdWin *w;
	char *title;

	assert(title = randomTitle());
	assert(w = allocWin(kind, title));
	if (connectWin(w,title))
		return 0;
	w->user = user;
	w->userp = userp;
	return w;
}


/*
 * discard all the items in the window's list
 */

static void
freeItems(rdWin *w)
{
	rdItem *i, *j;

	assert(w);
	for (i = w->items; i; i = j) {
		j = i->next;
		free(i);
	}
	w->items = 0;
}

/*
 * loadItems(w, items) - load an array of items into the list, and into the window.
 */

static int
loadItems(rdWin *w, char **items)
{
	rdItem *i, *j = 0;
	char *sep = "\n";
	ulong p0 = 0;
	ulong len, seplen = strlen(sep);
	char *buf = 0;
	int n;

	assert(w);
	assert(items);
	while (*items) {
		i = salloc(sizeof(*i));
		i->next = 0;
		i->p0 = p0;
		len = strlen(*items);
		if (j)
			j->next = i;
		else
			w->items = i;
		j = i;
		if (rpc_insert(wilyq, w->id, p0, *items)) {
			DPRINT("Failed to insert list item into wily");
			freeItems(w);
			return -1;
		}
		p0 += len;
		if (rpc_insert(wilyq, w->id, p0, sep)) {
			DPRINT("Failed to insert list separator into wily");
			freeItems(w);
			return -1;
		}
		i->p1 = (p0 += seplen);
		items++;
	}
	w->bodylen = p0;
	return 0;
}

/*
 * setWinList(w, items) - update the contents of a list window.
 */

int
setWinList(rdWin *w, char **items)
{
	assert(w);
	assert(items);

	if (w->wintype != rdList) {
		DPRINT("setWinList() called for non-list window");
		return 1;
	}
	freeItems(w);
	if (w->bodylen) {
		if (rpc_delete(wilyq, w->id, 0, w->bodylen)) {
			DPRINT("Could not delete list body");
			return 1;
		}
		w->bodylen = 0;
	}
	return loadItems(w, items);
}


/*
 * setWinArt(w,art) - replace the contents of article window w with text art.
 */

int
setWinArt(rdWin *w, char *art)
{
	assert(w);
	assert(art);
	assert(w->wintype == rdArticle);

	if (w->bodylen) {
		if (rpc_delete(wilyq, w->id, 0, w->bodylen)) {
			DPRINT("Failed to delete existing contents of window");
			return 1;
		}
	}
	w->bodylen = strlen(art);
	if (rpc_insert(wilyq, w->id, 0, art)) {
		DPRINT("Failed to insert new body into window");
		return 1;
	}
	return 0;
}

/*
 * getArtWin(title, text, filename) - get an article window, and load it with
 * either the given text, or from the given file.
 */

rdWin *
getArtWin(int user, void *userp, char *title, char *tools, char *text, rdWin *oldwin)
{
	rdWin *w;

	if (oldwin) {
		w = oldwin;
		setWinUser(w, user, userp);
	} else
		assert(w = getBlankArtWin(user, userp));
	if (title && setWinTitle(w, title))
		return 0;
	if (tools && setWinTools(w, tools))
		return 0;
	if (text && setWinArt(w, text))
		return 0;
	return w;
}

/*
 * get a List window, and load it with the items in the array (which
 * must be null-terminated).
 */

rdWin *
getListWin(int user, void *userp, char *title, char *tools, rdWin *oldwin, char **items)
{
	rdWin *w;

	if (oldwin) {
		w = oldwin;
		setWinUser(w, user, userp);
	} else
		assert(w = getBlankListWin(user, userp));
	if (title && setWinTitle(w, title))
		return 0;
	if (tools && setWinTools(w, tools))
		return 0;
	if (items && setWinList(w, items))
		return 0;
	return w;
}

 /*
 * readerLoading(user, userp, int isart, title) - create a window of the
 * desired type and title that indicates that the program has started and is
 * working at something...
 */

rdWin *
readerLoading(int user, void *userp, int isart, char *title)
{
	rdWin *w;

	assert(title);
	assert(w = isart? getBlankArtWin(user, userp) : getBlankListWin(user, userp));
	assert(setWinTitle(w,title)==0 && setWinTools(w,"Working...")==0);
	return w;
}

/*
 * readerInit() - general initialisation.
 */

int
readerInit(void)
{
	if ((wilyfd = get_connect()) < 0) {
		DPRINT("Could not connect to wily");
		return 1;
	}
	mq_init(wilyq, wilyfd);
	return 0;
}

/*
 * readerMainLoop() - get messages from wily, and act upon them.
 */

int
readerMainLoop(void)
{
	rdWin *w;
	Msg *m;
	int istag;

	while (windows) {
		if ((w = getActiveWin()) == 0) {
			fprintf(stderr,"No message available\n");
			return 1;
		}
		m = w->m;
		assert(m);
		switch (m->mtype) {
			char *cmd, *arg;
			ulong p0, p1;
			case EMexec:
				decode_exec_e(m, &cmd, &arg);
				winExec(w, cmd, arg);
				break;
			case EMgoto:
				decode_goto_e(m, &p0, &arg);
				winGoto(w, p0, arg);
				break;
			case EMinsert:
				istag = IsBody;
				decode_insert_e(m, &p0, &arg);
				winInsert(w, istag, p0, arg);
				break;
			case EMdelete:
				istag = IsBody;
				decode_delete_e(m, &p0, &p1);
				winDelete(w, istag, p0, p1);
				break;
			default:
				fprintf(stderr,"Unknown msg type!\n");
				exit(1);
		}
	}
	return 1;
}

/*
 * delete item n from the list
 */

int
rdDelItem(rdWin *w, int item)
{
	rdItem **pi;
	rdItem *i;
	int len;

	assert(w);
	if (w->wintype != rdList || item < 0) {
		DPRINT("Invalid window or item for delete");
		return 1;
	}
	for (pi = &w->items; *pi && item; pi = &((*pi)->next))
		item--;
	if (item) {
		DPRINT("Could not find item to delete it");
		return 1;		/* not found */
	}
	i = *pi;
	len = i->p1 - i->p0;
	if (rpc_delete(wilyq, w->id, i->p0, i->p1) < 0) {
		DPRINT("Could not delete item from list");
		return 1;
	}
	*pi =  i->next;
	free(i);
	updateItems(w, *pi, -len);
	return 0;
}

/*
 * add a new item to the end of the list
 */
int
rdAddItem(rdWin *w, char *text)
{
	rdItem **pi;
	rdItem *i;
	int len;
	char *sep = "\n";

	assert(w);
	assert(text);
	len = (int)strlen(text);
	if (w->wintype != rdList || !text || !*text) {
		DPRINT("addItem() call for non-list window");
		return 1;
	}
	i = salloc(sizeof(*i));
	for (pi = &w->items; *pi; pi = &((*pi)->next));
	*pi = i;
	i->p0 = w->bodylen;
	i->p1 = i->p0 + len;
	i->next = 0;
	if (rpc_insert(wilyq, w->id, i->p0, text) || rpc_insert(wilyq, w->id, i->p1++, sep)) {
		DPRINT("Could not insert item into window");
		return 1;
	}
	len += strlen(sep);
	w->bodylen += len;
	return 0;
}

/*
 * change the text of an item
 */

int
rdChangeItem(rdWin *w, int item, char *text)
{
	rdItem **pi;
	rdItem *i;
	int len, oldlen, newlen;
	char *sep = "\n";

	assert(w);
	assert(text);
	newlen = strlen(text);
	if (w->wintype != rdList || !text || !*text) {
		DPRINT("changeItem() called for non-list window");
		return 1;
	}
	for (pi = &w->items; *pi && item; pi = &((*pi)->next))
		item--;
	if (item) {
		DPRINT("item not found");
		return 1;
	}
	i = *pi;
	oldlen = i->p1 - i->p0;
	if (rpc_delete(wilyq, w->id, i->p0, i->p1) ||
		rpc_insert(wilyq, w->id, i->p0, text) ||
		rpc_insert(wilyq, w->id, i->p0 + newlen, sep)) {
		DPRINT("Updates to wily window failed");
		return 1;
	}
	newlen += strlen(sep);
	i->p1 = i->p0 + newlen;
	len = newlen - oldlen;
	updateItems(w, i->next, len);
	return 0;
}

static void
updateItems(rdWin *w, rdItem *i, int len)
{
	assert(w);
	w->bodylen += len;
	while (i) {
		i->p0 += len;
		i->p1 += len;
		i = i->next;
	}
}

/*
 * text has been deleted
 */

static void
winDelete(rdWin *w, int which, ulong p0, ulong p1)
{
	rdItem *i;
	ulong l = p1 - p0;

	assert(w);
	assert(p0 <= p1);
	if (which == IsTag) {
		w->taglen -= l;
		return;
	}
	if (w->wintype == rdArticle) {
		w->bodylen -= l;
		return;
	}
	for (i = w->items; i; i = i->next)
		if (i->p0 <= p0 && p1 <= i->p1)
			break;
	if (!i)
		return;
	i->p1 -= l;
	for (i = i->next; i; i = i->next) {
		i->p0 -= l;
		i->p1 -= l;
	}
	updateBody(w, p0, p1, (char *)0);
	return;
}

/*
 * text has been inserted
 */

static void
winInsert(rdWin *w, int which, ulong p, char *str)
{
	rdItem *i;
	int l;

	assert(w);
	assert(str);
	l = strlen(str);
	if (which == IsTag) {
		w->taglen += l;
		return;
	}
	if (w->wintype == rdArticle) {
		w->bodylen += l;
		return;
	}
	for (i = w->items; i; i = i->next)
		if (i->p0 <= p && p <= i->p1)
			break;
	if (!i)
		return;
	i->p1 += l;
	for (i = i->next; i; i = i->next) {
		i->p0 += l;
		i->p1 += l;
	}
	updateBody(w, p, (ulong)l, str);
	return;
}

/*
 * updateBody(w,p0,p1,str) - insert p1 chars of str at p0, or remove the text
 * between p0 and p1.
 * XXX - this is *utterly* revolting, and horrendously slow. But it's quick
 * and simple, and will do for now.
 */

static void
updateBody(rdWin *w, ulong p0, ulong p1, char *str)
{
	char *newbuf;
	ulong newlen;

	assert(w);
	if (w->body == 0)
		return;
	if (str)
		newlen = w->bodylen + p1;
	else
		newlen = w->bodylen - (p1 - p0);
	newbuf = salloc(newlen);
	strncpy(newbuf, w->body, p0);
	if (str) {
		strncpy(newbuf + p0, str, p1);
		strcpy(newbuf + p0 + p1, w->body + p0);
	} else {
		strcpy(newbuf + p0, w->body + p1);
	}
	free(w->body);
	w->body = newbuf;
	w->bodylen = newlen;
}

rdItemRange
itemNumber(rdWin *w, ulong p0, ulong p1)
{
	int n;
	rdItem *i;
	rdItemRange r;

	assert(w);
	r.first = r.last = -1;
	r.i0 = r.i1 = 0;
	if (w->wintype != rdList || p1 < p0) {
		DPRINT("itemNumber asked for in non-list window");
		return r;
	}
	for (n = 0, i = w->items; i; i = i->next, n++) {
		if (i->p0 <= p0 && p0 <= i->p1) {
			r.first = r.last = n;
			r.i0 = r.i1 = i;
		}
		if (i->p0 <= p1 && p1 <= i->p1) {
			r.last = n;
			r.i1 = i;
			return r;
		}
	}
	DPRINT("Can't find item in list window");
	return r;		/* sigh */
}

/*
 * Move the cursor to a particular window.
 */

void
warpToWin(rdWin *w)
{
	assert(w);
	/* XXX not implemented yet. */
}

void
highlightItem(rdWin *w, rdItemRange r)
{
	static char addr[80];			/* XXX - overkill */
	rdItem *i;
	ulong p0, p1;
	int n;

	assert(w);
	if (!r.i0 || !r.i1) {
		for (i = w->items, n = 0; n <= r.last; n++, i = i->next) {
			if (n == r.first)
				p0 = i->p0;
			if (n == r.last)
				p1 = i->p1;
		}
	} else {
		p0 = r.i0->p0;
		p1 = r.i1->p1;
	}

	sprintf(addr,"#%lu,#%lu", p0, p1);
	if (rpc_goto(wilyq, w->id, addr)) {
		DPRINT("highlighting failed");
	}
}

/*
 * A B3 event has occured.
 */

static void
winGoto(rdWin *w, ulong p0, char *str)
{
	/* If this is a list,
	then it counts as a selection. Otherwise,  we just reflect it back to wily as a normal
	B3 event. */

	assert(w);
	assert(str);
	if (w->wintype == rdList) {
		rdItemRange r = itemNumber(w, p0, p0);
		if (r.first == -1) {
			DPRINT("Goto occurred outside range of items");
			return;		/* sigh */
		}
		highlightItem(w, r);
		user_listSelection(w,r);
		return;
	} else {
		if (rpc_goto(wilyq, w->id, str)) {
			DPRINT("could not send goto to wily");
		}
	}
	return;
}


/*
 * A B2(B1) command has been invoked.
 */

static void
winExec(rdWin *w, char *cmd, char *arg)
{
	ulong p0, p1;
	/*
	 * Lots of options here:
	*	- B2 with "|<>" - reflect back to wily.
	*	- B2 cmd recognised by the reader routines - Del is about it. Execute
	*	  them ourselves.
	*	- B2 not recognised by the reader routines - pass them onto the user,
	*	  after first getting the position of the argument within the body, if
	*	 necessary.
	*	- B2B1 recognised by the reader routines - none spring to mind, but
	*	  I might add some. Execute them ourselves.
	*	- B2B1 not recognised - pass onto user.
	*/

	assert(w);
	assert(cmd);
	assert(arg);
	DPRINT("Command received...");
	DPRINT(cmd);
	if (!*cmd || strstr(cmd,"|<>")) {
		DPRINT("Reflecting command");
		winReflectCmd(w,cmd, arg);
		return;
	}
	if (rdBuiltin(w,cmd,arg) == 0) {
		DPRINT("command is reader builtin - handled");
		return;
	}
	if (rpc_addr(wilyq, w->id, ".", &p0, &p1)) {
		DPRINT("could not get address of dot");
		return;
	}

	if (w->wintype == rdList) {
		rdItemRange r = itemNumber(w, p0, p1);

		if (r.first == -1) {
			p0 = p1 = 0;
		} else {
			highlightItem(w, r);
			p0 = r.i0->p0;
			p1 = r.i1->p1;
		}
		user_cmdList(w,cmd,p0,p1,r,arg);
		return;
	}
	user_cmdArt(w, cmd, p0, p1, arg);
	return;
}


void
winReflectCmd(rdWin *w, char *cmd, char *arg)
{
	if (rpc_exec(wilyq, w->id,cmd,arg)) {
		DPRINT("could not reflect command to wily");
	}
}

/*
 * delete this particular window from the list.
 */

static void
DelWin(rdWin *w, char *arg)
{
	assert(w);
	user_delWin(w);
	closeWin(w);
}

void
closeWin(rdWin *w)
{
	assert(w);
	/* get wily to close the pane */
	if (rpc_exec(wilyq, w->id, "Del", "")) {
		DPRINT("Could not close window");
	}

	/* free up resources */
	free(w->title);
	free(w->body);
	if (w->wintype == rdList)
		freeItems(w);
	/* XXX - currently leak the stateinfo stuff */
	freeWin(w);
	return;
}

void
freeWin(rdWin *w)
{
	rdWin **wp = &windows;

	assert(w);
	/* remove from the window chain */
	while ((*wp) != w)
		wp = &((*wp)->next);
	*wp = w->next;
	if (windows == 0) {
		DPRINT("All windows have been closed - exiting");
		exit(0);
	}
	return;
}

static struct {
	char *cmd;
	void (*fn)(rdWin *, char *);
} builtins[] = {
	{ "Del",	DelWin },
	{ 0, 0 }
};

/*
 * check for builtin commands understood by the reader routines.
 */

static int
rdBuiltin(rdWin *w, char *cmd, char *str)
{
	int i;

	assert(w);
	assert(cmd);
	for (i = 0; builtins[i].cmd; i++)
		if (strcmp(builtins[i].cmd, cmd) == 0) {
			(*builtins[i].fn)(w, str);
			return 0;
		}
	return 1;
}



/*
 * "redisplay" a window that's already open.
 */

void
rdGotoWin(rdWin *w)
{
	if (rpc_goto(wilyq, w->id, ".")) {
		DPRINT("Could not redisplay open window");
	}
}

rdWin *
userpWin(void *ptr)
{
	rdWin *w = windows;

	while (w && w->userp != ptr)
		w = w->next;
	return w;
}

/*
 * Some routines which attempt to extract the body of a window,
 * and store it in a file. if p0==p1==0, assume all the body is wanted.
 */

void
rdBodyToFile(rdWin *w, char *filename)
{
	FILE *fp;
	char *buf;

	assert(w);
	assert(filename);
	buf = salloc(w->bodylen+1);
	if (rpc_settag(wilyq, w->id, "Sending... ")) {
		DPRINT("Could not change tag");
		return;
	}
	if (rpc_read(wilyq, w->id, 0, w->bodylen, buf)) {
		DPRINT("Could not retrieve body text");
		return;
	}
	if ((fp = fopen(filename, "w"))) {
		(void)fprintf(fp,"%s",buf);
		fclose(fp);
	} else {
		DPRINT("Could not write body text to a file");
	}
	free(buf);
	return;
}


static void
alarm_handler(int signal)
{
	if (signal != SIGALRM) {
		DPRINT("non-alarm signal received!");
		return;
	}
	set_sig();
	rdAlarmEvent = true;
	if (old_alarm_handler != SIG_DFL && old_alarm_handler != SIG_IGN) {
		DPRINT("Passing alarm onto previous handler");
		(*old_alarm_handler)(signal);
	}
	alarm(rdRescanTimer);
}

static void
set_sig(void)
{
	(void)signal(SIGALRM, alarm_handler);
}

void
rdSetRescanTimer(int secs)
{
	rdRescanTimer = secs;
	if (old_alarm_handler == 0) {
		if ((old_alarm_handler = signal(SIGALRM, alarm_handler)) == SIG_ERR) {
			perror("signal");
			rdRescanTimer = 0;
			return;
		}
	}
	alarm(rdRescanTimer);
}

void
rdInclude(rdWin *w, char *str, size_t len)
{
	size_t nlines;
	char *buf;
	char *prefix = "> ";
	size_t nlen, plen = strlen(prefix);
	ulong p0, p1;

	assert(w);
	assert(str);
	nlines = countlines(str, len);
	if (rpc_addr(wilyq, w->id, ".", &p0, &p1)) {
		DPRINT("Could not get address of dot");
		return;
	}
	nlen = len + nlines*plen + 1;
	buf = salloc(nlen);
	prefixlines(str, len, prefix, plen, buf);
	if (rpc_insert(wilyq, w->id, p1, buf)) {
		DPRINT("Could not insert included text");
	}
	w->bodylen += nlen;
	free(buf);
}

static size_t
countlines(char *str, size_t len)
{
	size_t nlines = 0;

	assert(str);
	while (len--)
		if (*str++ == '\n')
			nlines++;
	return nlines;
}

static void
prefixlines(char *str, size_t len, char *prefix, size_t plen, char *buf)
{
	assert(str && prefix);
	while (len) {
		strncpy(buf,prefix,plen);
		buf += plen;
		while (len-- && (*buf++ = *str++) != '\n')
			;
	}
	*buf = 0;
}
