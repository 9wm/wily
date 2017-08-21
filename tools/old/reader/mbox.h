/*
 * mbox.h - definitions for mailbox-handling library.
 */

#ifndef MBOX_H
#define MBOX_H

#include <sys/types.h>

typedef struct mstring mString;
typedef struct mhdr mHdr;
typedef struct mmsg mMsg;
typedef struct mhdrline mHdrLine;
typedef struct mmbox mMbox;

struct mstring {	/* can include several \n characters */
	char *s0;	/* first char in string */
	char *s1;	/* char after last newline */
};

struct mhdr {		/* includes ws if hdr is multiple lines */
	mString name;	/* s1 points to colon */
	mString value;	/* s0 pts to first non-ws, s1 to after \n */
	int hide;	/* don't display if true */
};			/* name,s0,value.s2 gives whole header */

struct mmsg {
	int nhdrs;	/* number of headers */
	mHdr **hdrs;	/* headers themselves */
	mString *from;	/* points to value fields from mHdrs */
	mString *date;
	mString *subject;
	mString whole;	/* All of the message (headers and body) */
	mString body;	/* All of the body (not including first blank line) */
	int deleted;	/* marked for deletion */
	int read;	/* if has been read */
	ulong pos;	/* origin in window, if it's been read before. */
};

struct mhdrline {
	char *s0,*s1;	/* buffer containing accumulated text */
	ulong p0, p1;	/* positions in hdrw for this piece of text */
};

struct mmbox {
	char *mbox_start;	/* area of memory that the mailbox is */
	char *mbox_end;		/* loaded into */
	int nmsgs;		/* number of messages */
	int amsgs;		/* size of msgs array */
	mMsg **msgs;		/* the messages themselves */
	int ndel;		/* number of deleted messages */
	int readonly;
	time_t mtime;		/* time it was last modified */
	off_t size;		/* how big it was, last time we checked */
	char *name;		/* the filename */
};

mMbox *read_mbox(const char *filename, int readonly);

#endif /* ! MBOX_H */
