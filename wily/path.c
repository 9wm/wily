/*******************************************
 *	Expand abbreviations for pathnames
 *******************************************/

#include "wily.h"
#include <pwd.h>

static char*	gettilde(const char*s);

/*
 * Expand 'orig' into 'dest', using 'expansion'.
 * Expansion must take a char*, and return a static  char*,
 * i.e. one we don't have to free (and can't keep).
 */
static void
expand(char*dest, char*orig, char*(*expansion)(const char*)){
	Path	key;
	char	*val;
	char	*slash;
	
	assert(orig[0]=='$' || orig[0]=='~');
	strcpy(key,orig+1);
	if( (slash=strchr(key,'/')) ) {
		*slash = 0;
	}
	val = (*expansion)(key);
	if(slash)
		*slash = '/';
	if(val){
		sprintf(dest, "%s%s", val, slash? slash: "");
	} else {
		strcpy(dest, orig);
	}
}

/* Convert a 'label' to a 'path'.  Paths are always absolute. */
void
label2path(char*path, char*label) {
	if(!label){
		strcpy(path,wilydir);
		return;
	}
	switch(label[0]) {
	case '$':	expand(path, label, getenv); break;
	case '~':	expand(path, label, gettilde); break;
	case '/':	strcpy(path, label); break;
	default:	sprintf(path, "%s%s", wilydir, label); break;
	}
	labelclean(path);
}

/*********************************************************
	static functions
*********************************************************/

/* Clean up 'label' by removing components with '.' or '..' */
void
labelclean(char*label) {
	char	*slash, *from, *to, c;
	char	*back;
	
	slash = strchr(label,'/');
	if(slash) {
		from = to = slash+1;
	} else {
		return;
	}
	
	while (from[0] != '\0') {
		assert(to[-1] == '/');
		switch(from[0]){
		case '/':		/* ignore "//" */
			from++;
			continue;
		case '.':
			switch(from[1]){
			case '\0':
			case '/':		/* Ignore "./" */
				from++;
				continue;
			case '.':
				if (from[2] == '\0' || from[2] == '/') {
					from += 2;
					to[-1] = '\0';
					back = strrchr(label, '/');
					if(back){
						to = back+1;
					} else {
						to[-1] ='/';
					}
					continue;
				}
			}
		default:
			do {
				c = (*to++ = *from++);
			} while (c != '\0' && c != '/');
			from--;
		}
	}
	*to='\0';
}

static char*
gettilde(const char*s) {
	struct passwd *pw;

	pw = strlen(s) ? getpwnam(s) : getpwuid(getuid());
	return pw ? pw->pw_dir : 0;
}

