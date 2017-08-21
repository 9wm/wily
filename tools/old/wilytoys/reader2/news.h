/*
 * news.h - definitions of newsgroups and news articles. This is a simplistic
 * view - we ignore concepts like cross-posting. :-}
 */

#ifndef NEWS_H
#define NEWS_H

typedef struct _nGroup nGroup;
typedef struct _nArticle nArticle;
typedef struct _nWin nWin;
typedef struct _nRange nRange;
typedef struct _nString nString;

/*
 * Kinds of window we understand.
 */

enum {
	nGroupList,			/* list of newsgroups */
	nArtList,			/* list of articles within a group */
	nDispArt,			/* a displayed article, within a group */
	nCompArt			/* a new article being composed */
};

struct _nString {
	char *s0, *s1;
};

struct _nRange {
	int n0, n1;
	struct _nRange *next;
};

struct _nWin {
	int kind;			/* what kind of window it is */
	nGroup *group;		/* null for nGroupList */
	struct _nWin *gwin;	/* For articles, this is the userp of the group list window */
	int artnum;			/* valid for nDispArt and nCompArt */
	int itemnum;		/* 0 - (n-1), where n==number of items in list */
	int isrep;			/* true if nCompArt and replying by email */
	int ispost;			/* true if nCompArt and following up */
	struct _nWin *next;
};


struct _nGroup {
	char *name;		/* newsgroup name */
	int first, last;		/* range of articles in this group */
	int nunread;		/* number of articles left in this group to be read */
	nArticle *arts;		/* actual articles - null until fetched */
	nArticle **artptrs;	/* art ptrs, in sync with items */
	char **items;		/* list of items we pass to the window */
	nRange *read;		/* list of articles that we've read, from the .newsrc */
	int canpost;			/* true if posting is allowed to this group */
	int issub;			/* true if we're subscribed to this group */
	struct _nGroup *next;
};

/*
 * In the following structure, if nString.s0 == nString.s1 == 0, then
 *  we haven't downloaded the relevant section yet.
 */

struct _nArticle {
	int num;			/* article number within group */
	int visible;			/* true if the article is currently in a window */
	nGroup *group;		/* group this article is from */
	int read;			/* true if we've read it */
	char *body;		/* whole body of article */
	int bodylen;		/* length of body */
	char *from;		/* sender - points into hdr */
	char *subj;		/* subject - points into hdr */
	struct _nArticle *next;
};

/*
 * These are the commands that the news reader recognises, and the
 * contexts that they are valid.
 */

#define NCMSGLIST	0x1
#define NCDISPART	0x2
#define NCCOMPART	0x4
#define NCGRPLIST	0x8
#define NCNOTCOMP	(NCMSGLIST|NCDISPART|NCGRPLIST)
#define NCANY		(NCMSGLIST|NCDISPART|NCCOMPART|NCGRPLIST)

#define NNOARG		0x0
#define NARG			0x1

struct nCmd {
	char *cmd;
	unsigned short context;
	unsigned short req;
	void (*fn)(rdWin *, nGroup *, int, int, char *);	/* pane, first msg, last msg, arg */
};

/*
 * MTU is your mailer program. It must be capable of reading a complete
 * mail message, with headers, on stdin. No arguments are added to the
 * command.
 */

#define MTU	"/usr/lib/sendmail -t"

/*
 * FROMENV gives the name of the environment variable to use to
 * obtain your real email address - it'll place this in the From: field
 * of mail messages and news articles.
 */

#define FROMENV	"WILYFROM"

/*
 * LOADDISP determines how much feedback you get when the
 * newsreader is initialising. It'll display the name of every
 * LOADDISPth group it asks the server about, unless it's
 * 0, in which case you don't get any feedback at all.
 */

#define LOADDISP	5

nWin *allocNWin(int kind, nGroup *g, int num);
void freeNWin(nWin *w);
nWin *findNWin(void *ptr);
int artnumToItem(nGroup *g, int artnum);

#endif /* !NEWS_H */
