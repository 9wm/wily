#include "wily.h"

Bool 	show_dot_files = false;
Bool 	autoindent_enabled = true;

/* cwd for wily process */
Path		wilydir;

/* widget with most recent b1 click */
View *	last_selection;

/* "stop" characters for various text expansions.
 * heuristic guesses only.
 */
char *notfilename = 	"!\"#%&'()*;<=>?@`[\\]^{|}";
char *notinclude = 		"!#%&'()*;=?@`[\\]^{|}";
char *notdoubleclick=	"!\"#$%&'()*+,-./:;<=>?@`[\\]^{|}~";
char *notcommand=	"!\"#%&'()*;:=?@`[\\]^{}";
char *notaddress=		"!\"%&'()+;<=>?@`[]{|}";

char *whitespace = 		" \t\v\n";
