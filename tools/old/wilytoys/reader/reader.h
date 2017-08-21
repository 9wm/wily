/*
 * reader.h - declarations for a mail/news reader toolkit for wily.
 */

/*
 * We support two kinds of window  - a list (mailboxes, newsgroups, subject lines)
 * and an article display. There can be more than one instance of each.
 */

typedef enum _rdWinType {
	rdList,
	rdArticle
} rdWinType;

/*
 * This makes the messages clearer
 */

enum {
	IsTag = 0,
	IsBody = 1
};


typedef struct _rdWin rdWin;
typedef struct _rdItem rdItem;
typedef struct _rdItemRange rdItemRange;
typedef struct _Msgq Msgq;

extern int		rdMultiList;
extern int		rdMultiArt;

struct _rdItem {
	ulong			p0, p1;			/* address of this item in the window. */
	struct _rdItem	*next;			/* next item in the list */
};

struct _rdItemRange {
	int first, last;					/* inclusive */
	rdItem *i0, *i1;
};

struct _rdWin {
	rdWinType		wintype;
	int				user;			/* user info */
	void				*userp;			/* ditto */
	Id				id;
	struct _rdWin	*next;			/* list storage */
	char			*title;			/* what we think the title of this window is */
	ulong			taglen;			/* length of tag, in characters */
	ulong			bodylen;			/* length of body, in characters */
	Msg			*m;				/* message from wily */
	char			*body;			/* UTF text of an article window */
	rdItem			*items;			/* The items in a list */
	int				protect;			/* The user is writing to this window */
};

extern rdWin *windows;
extern Mqueue *wilyq;
extern Bool rdAlarmEvent;
