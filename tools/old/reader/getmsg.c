/*
 * getmsg.c  - handle message reception from wily.
 */

#include "headers.h"

rdWin *
getActiveWin(void)
{
	rdWin *w;
	Msg *m;
	Id id;

	if ((m = mq_next(wilyq, 0)) == 0)
		return 0;
	id = msg_id(m);
	for (w = windows; w; w  = w->next)
		if (id == w->id) {
			w->m = m;
			return w;
		}
	return 0;
}
