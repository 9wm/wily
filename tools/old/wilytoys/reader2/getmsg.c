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

	DPRINT("Waiting for next wily message");
	while ((m = mq_next(wilyq, 0)) == 0) {
		if (rdAlarmEvent) {
			DPRINT("A rescan alarm occurred");
			dorescan();
			rdAlarmEvent = false;
			continue;
		}
		DPRINT("No message received from wily");
		return 0;
	}
	id = msg_id(m);
	for (w = windows; w; w  = w->next)
		if (id == w->id) {
			w->m = m;
			return w;
		}
	DPRINT("No window found matching message's Id");
	return 0;
}
