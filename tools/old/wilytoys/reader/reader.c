/*
 * reader.c - functions for handling news/article reader windows.
 */

#include "headers.h"

Bool debug = false;
Bool frommessage = true;

/*
 * These determine whether we'll have one list and one article window,
 * that gets overwritten each time we change group/mailbox/article,
 * or whether we just spawn a new one.
 */

int rdMultiList = 0;
int rdMultiArt = 0;

/*
 * The windows currently active.
 */

rdWin *windows = 0;
static rdWin *Listwin = 0;
static rdWin *Artwin = 0;

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

char *rdListTagStr = " delete undelete comp reply quit exit ";
char *rdArtTagStr = " delete reply save inc ";

static rdWin *allocWin(rdWinType kind, const char *title);
static int connectWin(rdWin *w, char *filename);
static rdWin *getWin(int user, void *userp, rdWinType kind, char *title, char *filename, int protect);
static void clearWin(rdWin *w);
static void freeItems(rdWin *w);
static int loadItems(rdWin *w, char **items, int savecontents);
static int initWin(int user, void *userp, rdWin *w, char *title, char *filename, int protect);
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

	w->title = sstrdup((char *)title);
	w->wintype = kind;
	w->id = -1;
	w->m = 0;
	w->items = 0;
	w->body = 0;
	w->protect = 0;
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
	if (filename == 0)
		filename = "New";
	if (rpc_new(wilyq, &w->id, filename, false) < 0) {
		freeWin(w);
		return -1;
	}
	return 0;
}

/*
 * getWin(user, userp, kind, title, filename, protect) - grab a window of the appropriate type.
 */

static rdWin *
getWin(int user, void *userp, rdWinType kind, char *title, char *filename, int protect)
{
	rdWin *w;

	if ((kind == rdList && (!Listwin || rdMultiList)) ||
		(kind == rdArticle && (!Artwin || Artwin->protect || protect || rdMultiArt))) {
		/* we can create a new window */
		w = allocWin(kind, title);
		if (connectWin(w,filename))
			return 0;
		if (kind == rdList)
			Listwin = w;
		else if (!protect)
			Artwin = w;
	} else {
		/* have to reuse the existing window */
		w = (kind == rdList)? Listwin : Artwin;
		clearWin(w);
	}
	initWin(user, userp, w, title, filename, protect);
	return w;
}

/*
 * clearWin(w) - erase the tag and body of a window
 */

static void
clearWin(rdWin *w)
{
	if (w->taglen)
		(void) rpc_settag(wilyq, w->id, "");
	w->taglen = 0;
	w->protect = 0;
	if (w->bodylen) {
		(void) rpc_delete(wilyq, w->id, 0, w->bodylen);
		w->bodylen = 0;
		if (w->wintype == rdList)
			freeItems(w);
	}
}

/*
 * discard all the items in the window's list
 */

static void
freeItems(rdWin *w)
{
	rdItem *i, *j;

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
loadItems(rdWin *w, char **items, int savecontents)
{
	rdItem *i, *j = 0;
	char *sep = "\n";
	ulong p0 = 0;
	ulong len, seplen = strlen(sep);
	char *buf = 0;
	int n;

	if (savecontents) {
		for (len = 0, n = 0; items[n]; n++)
			len += strlen(items[n]) + seplen;
		w->body = buf = salloc(len);
	}
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
			freeItems(w);
			return -1;
		}
		p0 += len;
		if (rpc_insert(wilyq, w->id, p0, sep)) {
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
 * changeItems(w, items, savecontents) - update the contents of a list window.
 */

int
changeItems(rdWin *w, char **items, int savecontents)
{
	if (w->wintype != rdList)
		return 1;
	freeItems(w);
	if (w->bodylen) {
		if (rpc_delete(wilyq, w->id, 0, w->bodylen))
			return 1;
		w->bodylen = 0;
	}
	return loadItems(w, items, savecontents);
}

/*
 * initWin(user, userp, w, title, filename, protect) - initialise the window's tag, label, etc.
 */

static int
initWin(int user, void *userp, rdWin *w, char *title, char *filename, int protect)
{
	char tag[300];			/* XXX guess */
	char *tagstr = w->wintype == rdArticle? rdArtTagStr : rdListTagStr;
	if (rpc_setname(wilyq, w->id, title))
		return 1;
	sprintf(tag,"%s", tagstr);		/* XXX at the moment, we don't include the title */
	if (rpc_settag(wilyq, w->id, tag))
		return 1;
	w->taglen = strlen(tag);
	w->user = user;
	w->userp = userp;
	w->protect = protect;
	return 0;
}

/*
 * getArtWin(title, text, filename, savecontents, protect) - get an article window, and load it with
 * either the given text, or from the given file.
 */

int
getArtWin(int user, void *userp, char *title, char *text, char *filename, int savecontents, int protect)
{
	rdWin *w = getWin(user, userp, rdArticle, title, filename, protect);

	if (w == 0)
		return 1;
	if (text) {
		int l = strlen(text);
		w->bodylen = l;
		if (rpc_insert(wilyq, w->id, 0, text))
			return 1;
		if (savecontents) {
			w->body = salloc(l);
			strcpy(w->body, text);
		}
	}
	return 0;
}

/*
 * get a List window, and load it with the items in the array (which
 * must be null-terminated).
 */

int
getListWin(int user, void *userp, char *title, char **items, int savecontents)
{
	rdWin *w = getWin(user, userp, rdList, title, (char *)0, 0);

	if (w == 0)
		return 1;
	return loadItems(w, items, savecontents);
}

/*
 * readerInit(title, items, ml, ma) - start things off by creating
 * a list window. Tell the reader whether we'll want one or more of
 * the list and article windows.
 */

int
readerInit(int user, char *userp, char *title, char **items, int ml, int ma, int savecontents)
{
	rdMultiList = ml;
	rdMultiArt = ma;
	if ((wilyfd = get_connect()) < 0)
		return 1;
	mq_init(wilyq, wilyfd);
	return getListWin(user, userp, title, items, savecontents);
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

	if (w->wintype != rdList || item < 0)
		return 1;
	for (pi = &w->items; *pi && item; pi = &((*pi)->next))
		item--;
	if (item)
		return 1;		/* not found */
	i = *pi;
	len = i->p1 - i->p0;
	if (rpc_delete(wilyq, w->id, i->p0, i->p1) < 0)
		return 1;
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
	int len = (int)strlen(text);
	char *sep = "\n";

	if (w->wintype != rdList || !text || !*text)
		return 1;
	i = salloc(sizeof(*i));
	for (pi = &w->items; *pi; pi = &((*pi)->next));
	*pi = i;
	i->p0 = w->bodylen;
	i->p1 = i->p0 + len;
	i->next = 0;
	if (rpc_insert(wilyq, w->id, i->p0, text) || rpc_insert(wilyq, w->id, i->p1++, sep))
		return 1;
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
	int len, oldlen, newlen = strlen(text);
	char *sep = "\n";

	if (w->wintype != rdList || !text || !*text)
		return 1;
	for (pi = &w->items; *pi && item; pi = &((*pi)->next))
		item--;
	if (item)
		return 1;
	i = *pi;
	oldlen = i->p1 - i->p0;
	if (rpc_delete(wilyq, w->id, i->p0, i->p1) ||
		rpc_insert(wilyq, w->id, i->p0, text) ||
		rpc_insert(wilyq, w->id, i->p0 + newlen, sep))
		return 1;
	newlen += strlen(sep);
	i->p1 = i->p0 + newlen;
	len = newlen - oldlen;
	updateItems(w, i->next, len);
	return 0;
}

static void
updateItems(rdWin *w, rdItem *i, int len)
{
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
	int l = strlen(str);

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

	r.first = r.last = -1;
	r.i0 = r.i1 = 0;
	if (w->wintype != rdList || p1 < p0)
		return r;
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
	return r;		/* sigh */
}

void
highlightItem(rdWin *w, rdItemRange r)
{
	static char addr[80];			/* XXX - overkill */
	rdItem *i;
	ulong p0, p1;
	int n;

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
	(void)rpc_goto(wilyq, w->id, addr);
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

	if (w->wintype == rdList) {
		rdItemRange r = itemNumber(w, p0, p0);
		if (r.first == -1)
			return;		/* sigh */
		highlightItem(w, r);
		user_listSelection(w,r);
		return;
	} else
		(void)rpc_goto(wilyq, w->id, str);
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

	if (!*cmd || strstr(cmd,"|<>")) {
		winReflectCmd(w,cmd, arg);
		return;
	}
	if (rdBuiltin(w,cmd,arg) == 0)
		return;
	if (rpc_addr(wilyq, w->id, ".", &p0, &p1))
		return;

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
	(void)rpc_exec(wilyq, w->id,cmd,arg);
}

/*
 * delete this particular window from the list.
 */

static void
DelWin(rdWin *w, char *arg)
{
	closeWin(w);
}

void
closeWin(rdWin *w)
{
	/* get wily to close the pane */
	(void)rpc_exec(wilyq, w->id, "Del", "");

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

	/* remove from the window chain */
	while ((*wp) != w)
		wp = &((*wp)->next);
	*wp = w->next;
	if (w == Artwin)
		Artwin = 0;
	if (w == Listwin)
		Listwin = 0;
	if (windows == 0)
		exit(0);
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
	(void)rpc_goto(wilyq, w->id, ".");
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
	char *buf = salloc(w->bodylen+1);

	if (rpc_read(wilyq, w->id, 0, w->bodylen, buf))
		return;
	if ((fp = fopen(filename, "w"))) {
		(void)fprintf(fp,"%s",buf);
		fclose(fp);
	}
	free(buf);
	return;
}

/*
 * change current settings for multi-list and multi-art windows.
 */

void
rdSetMulti(int list, int art)
{
	rdMultiList = list;
	rdMultiArt = art;
}


static void
alarm_handler(int signal)
{
	if (signal != SIGALRM)
		return;
	set_sig();
	rdAlarmEvent = true;
	if (old_alarm_handler != SIG_DFL && old_alarm_handler != SIG_IGN)
		(*old_alarm_handler)(signal);
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
	size_t nlines = countlines(str, len);
	char *buf;
	char *prefix = "> ";
	size_t nlen, plen = strlen(prefix);
	ulong p0, p1;

	if (rpc_addr(wilyq, w->id, ".", &p0, &p1))
		return;
	nlen = len + nlines*plen + 1;
	buf = salloc(nlen);
	prefixlines(str, len, prefix, plen, buf);
	rpc_insert(wilyq, w->id, p1, buf);
	w->bodylen += nlen;
	free(buf);
}

static size_t
countlines(char *str, size_t len)
{
	size_t nlines = 0;

	while (len--)
		if (*str++ == '\n')
			nlines++;
	return nlines;
}

static void
prefixlines(char *str, size_t len, char *prefix, size_t plen, char *buf)
{
	while (len) {
		strncpy(buf,prefix,plen);
		buf += plen;
		while (len-- && (*buf++ = *str++) != '\n')
			;
	}
	*buf = 0;
}
