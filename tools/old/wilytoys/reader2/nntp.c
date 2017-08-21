/*
 * NNTP client code.
 */

#include "newsheaders.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>

#define NNTPMAXRESP	512
#define NNTPMAXPARAMS	256

#define GetChar() ((nntpBufPtr < nntpBufLen)? nntpBuf[nntpBufPtr++] : nntpFillBuf())

static void nntpErr(int quit, char *msg);
static int iptoaddr(char *host, char **addr, size_t *len);
static int gethost(char **addr, size_t *len);
static int getport(void);
static int getServerResp(void);
static int nntpFillBuf(void);
static char *getLine(int *len);
static int getServerLine(void);
static int splitServerLine(void);
static int getLongText(void);
static int sendCommand(const char *cmd, ...);
static nGroup *getGroupLine(char **str);
static int getFromHdrs(nGroup *g);
static int doXhdr(nGroup *g, char *arg);
static char *nextXhdr(char *s, int *n, char **t);
static int getSubjHdrs(nGroup *g);
static int haveRead(nGroup *g, int num);
static int nextArtLine(FILE *fp, char **lineptr);


/*
 * Messages sent to the server.
 */

static const char
	*articleComm = "ARTICLE",
	*bodyComm = "BODY",
	*headComm = "HEAD",
	*statComm = "STAT",
	*groupComm = "GROUP",
	*listComm = "LIST",
	*newgroupsComm = "NEWGROUPS",
	*newnewsComm = "NEWNEWS",
	*nextComm = "NEXT",
	*postComm = "POST",
	*quitComm = "QUIT",
	*xhdrComm = "XHDR";


/*
 * NNTP server status
 */

static nGroup *currgrp;
static nArticle *currartp;
static int currartnum;
static int canpost;

/*
 * connection to NNTP server.
 */

static int nntpfd;

/*
 * Response information
 */

static char nntpBuf[BUFSIZ];
static int nntpBufLen;
static int nntpBufPtr;
static char nntpRespLine[NNTPMAXRESP+1];
static char *nntpRespWords[NNTPMAXPARAMS];
static char *nntpText;
static int nntpRespLen;
static int nntpRespNParams;
static int nntpStatus;
static int nntpTextMax;
static int nntpTextLen;

/*
 * Display an NNTP error message.
 */

static void
nntpErr(int quit, char *msg)
{
	int n;

	fprintf(stderr,"NNTP server error: %s\n", msg);
	for (n = 0; n < nntpRespNParams; n++)
		fprintf(stderr,"%s ", nntpRespWords[n]);
	fputc('\n',stderr);
	if (quit)
		exit(1);
}

/*
 * Functions to connect to the NNTP server.
 */

static int
iptoaddr(char *host, char **addr, size_t *len)
{
	unsigned long l = inet_addr((const char *)host);

	if (l == (unsigned long)-1) {
		perror("bad NNTP address");
		return -1;
	}
	*addr = (char *)&l;
	*len = sizeof(l);
	return 0;
}

static int
gethost(char **addr, size_t *len)
{
	static struct hostent *hp;
	char *host = getenv("NNTPSERVER");

	if (!host || !*host) {
		DPRINT("NNTPSERVER not set - using 'news'");
		host = "news";		/* desparate guess */
	}
	if ((hp = gethostbyname(host)) == 0) {
		DPRINT("Could not get host address");
		if (isdigit(*host))  {
			DPRINT("Treating hostname as IP address");
			return iptoaddr(host, addr, len);		/* assume a.b.c.d */
		}
		perror(host);
		return 1;
	}
	assert(hp);
	*addr = hp->h_addr;
	*len = hp->h_length;
	return 0;
}

static int
getport(void)
{
	struct servent *sp;
	char *srv = "nntp";

	if ((sp = getservbyname(srv, "tcp")) == 0) {
		perror("getservbyname");
		exit(1);
	} else
		return sp->s_port;
}


int
nntpConnect()
{
	int s;
	struct sockaddr_in saddr;
	char *haddr;
	size_t h_len;
	int port;

	if (gethost(&haddr, &h_len))
		return -1;
	memcpy((char *)&saddr.sin_addr, haddr, h_len);
	if ((port = getport()) == -1)
		return -1;
	saddr.sin_port = htons((short)port);

	if ((s = socket( AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket: %m\n");
		return -1;
	}
	saddr.sin_family = AF_INET;

	if (connect (s, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
		perror("connect: %m\n");
		return -1;
	}
	nntpfd = s;
	if (getServerResp())
		return -1;
	if (nntpStatus == nnServerOkPost || nntpStatus == nnServerOkNoPost)
		canpost = nntpStatus == nnServerOkPost;
	else
		nntpErr(1,"Failed to connect");		/* doesn't return */
	return 0;
}


/*
 * Get a response from the server.
 */

static int
getServerResp(void)
{
	nntpRespLen = nntpRespNParams = nntpTextLen = 0;
	if (getServerLine()) {
		DPRINT("Could not read line from server");
		return 1;
	}
	if (splitServerLine()) {
		DPRINT("Could not split line from server");
		return 1;
	}
	return 0;
}

static int
nntpFillBuf(void)
{
	int r;

	nntpBufLen = nntpBufPtr = 0;
	do {
		r = read(nntpfd, nntpBuf, BUFSIZ);
	} while (r < 0 && r == EINTR);
	if (r == 0) {
		DPRINT("EOF from nntp server");
		return EOF;
	}
	if (r < 0) {
		perror("NNTP read");
		exit(1);
	}
	nntpBufLen = r;
	nntpBufPtr = 1;
	return *nntpBuf;
}

static char *
getLine(int *len)
{
	static char *str;
	static int max;
	int l = 0, c = 0, cr = 0;

	while ((c = GetChar()) != EOF) {
		if (l >= max) {
			max = l + NNTPMAXRESP;
			str = srealloc(str, max);
		}
		str[l++] = c;
		if (c == '\r')
			cr = 1;
		else if (cr && c == '\n') {
			str[--l] = 0;
			str[l-1] = '\n';
			*len = l;
			return str;
		} else
			cr = 0;
	}
	return 0;
}

static int
getServerLine(void)
{
	int l;
	char *s = getLine(&l);

	if (!s || !*s)
		return 1;
	s[--l] = 0;	/* chop newline */
	if (l >= NNTPMAXRESP) {
		DPRINT("NNTP line chopped");
	}
	l = l < NNTPMAXRESP? l : NNTPMAXRESP;
	strncpy(nntpRespLine,s,l);
	nntpRespLine[l] = 0;
	nntpRespLen = l;
	return 0;
}

static int
splitServerLine(void)
{
	char *s = nntpRespLine;

	assert(s);
	do {
		nntpRespWords[nntpRespNParams++] = s;
		if ((s = strchr(s,' ')))
			*s++ = 0;
	} while (s);
	nntpStatus = atoi(nntpRespWords[0]);
	assert(nntpStatus);
	return 0;
}

static int
getLongText(void)
{
	int l;
	char *s;

	nntpTextLen = 0;
	while ((s = getLine(&l))) {
		if (*s == '.' && s[1] == '\n')
			return 0;
		if (nntpTextLen + l > nntpTextMax) {
			nntpTextMax = nntpTextLen + l + BUFSIZ;
			nntpText = srealloc(nntpText, nntpTextMax);
		}
		strcpy(nntpText + nntpTextLen, s);
		nntpTextLen += l;
	}
	nntpErr(1,"EOF in middle of textual response!");
}

/*
 * Send a command to the NNTP server.
 */

static int
sendCommand(const char *cmd, ...)
{
	static char cmdbuf[512];
	int l = 0;
	char *s;
	va_list alist;

	va_start(alist, cmd);
	strcpy(cmdbuf,cmd);
	l = strlen(cmd);
	while ((s = va_arg(alist, char *))) {
		cmdbuf[l++] = ' ';		/* XXX - doesn't check limit of string length */
		strcpy(cmdbuf+l, s);
		l += strlen(s);
	}
	va_end(alist);
	cmdbuf[l++] = '\r';
	cmdbuf[l++] = '\n';
	if (write(nntpfd, cmdbuf, l) != l) {
		perror("write");
		exit(1);
	}
	return 0;
}

/*
 * Get a listing of all the groups that the server supports.
 */

nGroup *
nntpListGroups(void)
{
	nGroup *groups = 0, *g;
	char *s;

	if (sendCommand(listComm, (char *)0))
		return 0;
	if (getServerResp())
		return 0;
	if (nntpStatus != nnListFollows)
		nntpErr(1,"Could not get newsgroup list");
	if (getLongText())
		return 0;
	for (s = nntpText; s; ) {
		if ((g = getGroupLine(&s))) {
			g->next = groups;
			groups = g;
		} else
			break;
	}
	return groups;
}

static nGroup *
getGroupLine(char **str)
{
	char *last, *first, *p;
	nGroup *g;

	assert(str);
	if ((last = strchr(*str,' ')) == 0)
		return 0;
	*last++ = 0;
	if ((first = strchr(last,' ')) == 0)
		return 0;
	*first++ = 0;
	if ((p = strchr(first,' ')) == 0)
		return 0;
	*p++ = 0;
	g = salloc(sizeof(*g));
	g->name = sstrdup(*str);
	g->first = atoi(first);
	g->last = atoi(last);
	g->canpost = (*p == 'y' || *p == 'Y');		/* Y probably isn't valid but.... */
	g->arts = 0;
	g->read = 0;
	g->next = 0;
	if ((*str = strchr(p,'\n')))
		(*str)++;
	return g;
}

void
nntpQuit(void)
{
	(void)sendCommand(quitComm,(char *)0);
	(void)getServerResp();
	close(nntpfd);
	return;
}

/*
 * Select a particular group, and also retrieve the info about that group.
 */

int
nntpSetGroup(nGroup *g)
{
	assert(g);
	if (sendCommand(groupComm,g->name,0))
		return 1;
	if (getServerResp())
		return 1;
	if (nntpStatus == nnGroupSel) {
		currgrp = g;
		currartp = 0;		/* not defined yet */
		currartnum = g->first = atoi(nntpRespWords[2]);
		g->last = atoi(nntpRespWords[3]);
		return 0;
	}
	nntpErr(0,"Could not select group");
	return 1;
}

/*
 * Fill in the From and Subject fields for a group's articles.
 */

int
nntpGroupHdrs(nGroup *g)
{
	assert(g);
	if (g->nunread == 0)
		return 0;
	if (g != currgrp && nntpSetGroup(g))
		return 1;
	if (getFromHdrs(g))
		return 1;
	if (getSubjHdrs(g))
		return 1;
	return 0;
}

static int
getFromHdrs(nGroup *g)
{
	char *s;
	nArticle **ap = &g->arts;
	nArticle *a;
	char *from;
	int num;

	assert(g);
	ap = &g->arts;
	if (doXhdr(g, "from"))
		return 1;
	*ap = 0;
	for (s = nextXhdr(nntpText, &num, &from); s; s = nextXhdr(s, &num, &from)) {
		a = salloc(sizeof(*a));
		a->num = num;
		a->visible = 0;
		a->group = g;
		a->read = haveRead(g, a->num);
		a->body = 0;
		a->from = from;
		a->subj = 0;
		a->next = 0;
		*ap = a;
		ap = &a->next;
	}
	return 0;
}

static int
doXhdr(nGroup *g, char *arg)
{
	static char range[80];		/* XXX - more guesses */

	sprintf(range,"%d-%d", g->first, g->last);
	if (sendCommand(xhdrComm,arg,range,0))
		return 1;
	if (getServerResp())
		return 1;
	if (nntpStatus != nnXhdrOk)
		return 1;
	if (getLongText())
		return 1;
	return 0;
}

static char *
nextXhdr(char *s, int *n, char **t)
{
		char *text, *num = s;

		if (s >= nntpText + nntpTextLen)
			return 0;
		if ((text = strchr(s, ' ')) == 0)
			return 0;
		*text++ = 0;
		if ((s = strchr(text,'\n')) == 0)
			return 0;
		*s++ = 0;
		*n = atoi(num);
		*t = sstrdup(text);
		return s;
}


static int
getSubjHdrs(nGroup *g)
{
	int num;
	char *s, *subj;
	nArticle *a = g->arts;

	assert(g);
	a = g->arts;
	if (a == 0)
		return 0;
	if (doXhdr(g, "subject"))
		return 1;
	for (s = nextXhdr(nntpText, &num, &subj); s; s = nextXhdr(s, &num, &subj)) {
		if (num < a->num)
			continue;		/* new article appeared from nowhere */
		while (a && a->num < num)
			a = a->next;		/* article has been cancelled */
		if (a == 0)
			return 0;
		a->subj = subj;
		a = a->next;
	}
	return 0;
}

/*
 * Read the text for an article.
 */

int
nntpGetArticle(nArticle *a)
{
	static char arg[80];		/* XXX */

	assert(a);
	if (a->body)
		return 0;		/* already fetched */
	if (a->group != currgrp && nntpSetGroup(a->group))
		return 1;
	sprintf(arg,"%d", a->num);
	if (sendCommand(articleComm, arg, 0))
		return 1;
	if (getServerResp())
		return 1;
	if (getLongText())
		return 1;
	a->body = sstrdup(nntpText);
	a->bodylen = nntpTextLen;
	return 0;
}

static int
haveRead(nGroup *g, int num)
{
	nRange *r;

	assert(g);
	for (r = g->read; r ; r = r->next)
		if (r->n0 <= num && num <= r->n1)
			return 1;
	return 0;
}

/*
 * Post an article to the server
 */

int
nntpCanPost(void)
{
	return canpost;
}

int
nntpPost(char *filename)
{
	FILE *fp;
	char *line;
	int len;
	assert(filename);
	assert(canpost);

	if ((fp = fopen(filename,"r")) == 0) {
		(void)fprintf(stderr,"Cannot read %s to post!\n", filename);
		return 1;
	}
	if (sendCommand(postComm,0))
		goto broken;
	if (getServerResp())
		goto broken;
	if (nntpStatus != nnSendArticle) {
		nntpErr(0,"Server refused article");
		goto broken;
	}
	while ((len = nextArtLine(fp, &line)) > 0)
		if (write(nntpfd, line, len) != len) {
			perror("write");
			goto broken;
		}
	if (len < 0)
		goto broken;
	(void)fclose(fp);
	if (write(nntpfd, ".\r\n",3) != 3) {
		perror("write");
		return 1;
	}
	if (getServerResp())
		return 1;
	if (nntpStatus == nnPostedOk) {
		DPRINT("Article posted");
	} else {
		nntpErr(0,"Article not posted");
	}
	return 0;
broken:
	(void)fclose(fp);
	return 1;
}

static int
nextArtLine(FILE *fp, char **lineptr)
{
	static char text[NNTPMAXRESP+1];
	int len = 0;
	int c;

	*lineptr = text;
	while ((c = fgetc(fp)) != EOF) {
		text[len++] = c;
		if (c == '\n') {
			text[len-1]  = '\r';
			text[len++] = '\n';
			text[len] = 0;
			break;
		}
		if (len == NNTPMAXRESP-1) {
			/* line has got too long to put the \r\n sequence in - sent it
			now. */
			text[len] = 0;
			return len;
		}
	}
	if (len == 3 && text[0] == '.') {
		len++;
		strcpy(text,"..\r\n");
	}
	return len;
}
