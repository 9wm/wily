/*
 * newsrc.h - handling the list of newsgroups/articles that we've read.
 */

#define SUBSEP		':'
#define NOSUBSEP	'!'
#define RANGESEP	','

#define NEWSRC	".testnewsrc"
#define LOCKFILE	".testnewsrc.lock"

nGroup *read_newsrc(void);
void unlock_newsrc(void);
int update_newsrc(nGroup *groups);
void newsrcMarkRead(nGroup *g, int i);
void newsrcMarkUnread(nGroup *g, int i);
