/*
 * stuff needed for the mail client.
 * smk
 */

#include "headers.h"
#include "mbox.h"

typedef struct _mWin mWin;

/*
 * We know of several different kinds of window: mailbox lists,
 * message lists, displayed articles and messages being composed. 
 */

enum {
	mMboxList,			/* list of mailboxes */
	mMsgList,			/* list of msgs in a mailbox */
	mDispArt,			/* a displayed article within a mailbox */
	mCompArt			/* a new article being composed */
};

struct _mWin {
	int	kind;		/* what kind of window it is */
	int msg;		/* message number, if mDispArt, or mCompArt */
	mMbox *mbox;	/* the message box we're associated with */
	struct _mWin *next;
};

/*
 * These are the commands that the mailer recognises, and the
 * contexts that they are valid.
 */

#define MCMSGLIST	01
#define MCDISPART	02
#define MCCOMPART	04
#define MCNOTCOMP	(MCMSGLIST|MCDISPART)
#define MCANY		(MCMSGLIST|MCDISPART|MCCOMPART)

#define MNOARG		00
#define MARG			01

struct mCmd {
	char *cmd;
	unsigned short context;
	unsigned short req;
	void (*fn)(rdWin *, int, int, char *);	/* pane, first msg, last msg, arg */
};

/*
 * The following must be the pathname of a program capable
 * of taking a complete message on stdin, with no arguments,
 * and delivering it.
 */

#define MTU	"/usr/lib/sendmail -t"

/*
 * Where do you keep your mail? Don't currently support anything other
 * than directly-accessible mail files....
 */

#define SPOOLDIR	"/var/mail"

/*
 * How long we wait before rescanning.
 */

#define RESCAN_TIME	60

/*
 * Environment variable and default filename for savefile's
 * default parameter.
 */

#define SAVEFILE_ENV	"SAVEFILE"
#define SAVEFILE_DEF	"mbox"		/* in $HOME, if possible */
