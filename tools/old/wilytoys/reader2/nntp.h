/*
 * Declarations of NNTP stuff. Pity there isn't a standard header file for this....
 */

#ifndef NNTP_H
#define NNTP_H

/*
 * Numeric responses from the server.
 */

typedef enum {
	nnServerOkPost			= 200,
	nnServerOkNoPost		= 201,
	nnListFollows			= 215,
	nnGroupSel				= 211,
	nnXhdrOk				= 221,
	nnSendArticle			= 340,
	nnPostedOk				= 240,
	nnPostFailed			= 441
} nnServerResp;

int nntpConnect(void);
nGroup *nntpListGroups(void);
void nntpQuit(void);
int nntpSetGroup(nGroup *g);
int nntpGroupHdrs(nGroup *g);
int nntpGetArticle(nArticle *a);
int nntpPost(char *filename);

#endif /* !NNTP_H */
