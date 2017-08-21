/*******************************************
 *	~ and $ expand and contract
 *******************************************/

#include "wily.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct Abbrev Abbrev;
struct Abbrev {
	Path	env;
	Stat	buf;
};

static struct Abbrev *abbrev = 0;
static int nabbrev = 0, maxabbrev = 0;

static Bool	foundmatch	(char*dest, char*orig);
static Bool	contract		(char*dest, char*orig);
static void	newenv		(char *env, Stat*buf);
static Abbrev*	findstat		(Stat*buf);

void
env_init(char **envp) {
	char	*env, *ptr;
	Stat	buf;

	/* Add environment variables */
	while ( (env = *envp++)) {
		if (! (ptr = strchr(env, '=')) )
			continue;
		*ptr ++ = 0;
		if (strlen(ptr)&& !stat(ptr, &buf))
			newenv(env, &buf);
		*--ptr = '=';
	}
}

/* Copy shorter version of 'orig' into 'dest' */
void
pathcontract(char*dest, char *orig) {
	if(orig[0]=='/' && contract(dest, orig)) {
		;	/* we're done */
	} else {
		strcpy(dest,orig);
	}
}

/******************************************************
	static functions
******************************************************/

static Bool
foundmatch(char*dest, char*orig){
	Stat	buf;
	Abbrev	*ab;
	
	if(!stat(orig,&buf) && (ab = findstat(&buf))){
		sprintf(dest, "$%s", ab->env);
		return true;
	} else {
		return false;
	}
}

static Bool
contract(char*dest, char*orig) {
	char*lastslash;
	Bool	retval;
	
	if(foundmatch(dest, orig)) {
		return true;
	} else {
		lastslash = strrchr(orig,'/');
		if(lastslash) {
			*lastslash = '\0';
			retval = contract(dest,orig);
			*lastslash = '/';
			if(retval) {
				strcat(dest, lastslash);
			}
			return retval;
		} else {
			return false;
		}
	}
}

static void
newenv(char *env, Stat*buf) {
	Abbrev*new;
	Abbrev*old;

	/* "I told him we've already got one" Holy Grail */
	if((old = findstat(buf))){
		if(strlen(env)<strlen(old->env))
			strcpy(old->env, env);
		return;
	}

	if (nabbrev == maxabbrev) {
		maxabbrev = maxabbrev? maxabbrev*2 : maxabbrev + 2;
		abbrev = srealloc(abbrev, maxabbrev * sizeof(*abbrev));
	}
	new = &abbrev[nabbrev++];
	strcpy(new->env, env);
	new->buf = *buf;
}

static Abbrev*
findstat(Stat*buf){
	Abbrev*ab;
	
	for(ab = abbrev; ab < abbrev + nabbrev; ab++)
		if( !statcmp( buf, &ab->buf))
			return ab;
	return 0;
}

