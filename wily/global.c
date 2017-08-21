#include "wily.h"

Bool 	show_dot_files = false;
Bool 	autoindent_enabled = true;

/* cwd for wily process */
Path		wilydir;

/* widget with most recent b1 click */
View *	last_selection;

/* 'last_focus' is a cache of the view we are pointing at,
 * (we use point to type).  We update this only when we start
 * typing after moving the pointer, or if the view is deleted.
 */
View	*last_focus;

/* "stop" characters for various text expansions.
 * heuristic guesses only.
 */
char *notfilename = 	"!\"#%&'()*;<=>?@`[\\]^{|}";
char *notinclude = 		"!#%&'()*;=?@`[\\]^{|}";
char *notdoubleclick=	"!\"#$%&'()*+,-./:;<=>?@`[\\]^{|}~";
char *notcommand=	"!\"#%&'()*;:=?@`[\\]^{}";
char *notaddress=		"!\"%&'()+;<=>?@`[]{|}";

char *whitespace = 		" \t\v\n";
