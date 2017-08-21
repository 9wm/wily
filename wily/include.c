/*******************************************
 *	Expand b3-clicks to "include" files, e.g. <stdio.h>
 *******************************************/

#include "wily.h"

static char *	pathfind (const char *paths, const char *file);
static Bool	is_includebrackets(char left, char right);

/*
 * The user has selected 'r' in 'v'.
 * If possible, open an appropriate include file, return a View
 * representing its body.
 *
 * If no include file is appropriate, return 0.
 */
View*
openinclude(View *v, Range r) {
	Range	expanded;
	Path		buf, pbuf;
	int		len;
	Text		*t;
	char		*s;
	
	t = view_text(v);
	
	expanded = text_expand(t, r, notinclude);
	len = RLEN(expanded);
	if( len > (MAXPATH*UTFmax) || len < 2)
		return false;
	len = text_copyutf(t, expanded, buf);
	
	if (!is_includebrackets(buf[0], buf[len-1]))
		return false;
	
	buf[len-1] = 0;
	s = pathfind(getenv("INCLUDES"),  buf+1);
	if(!s) {
		sprintf(pbuf, "/usr/include/%s", buf+1);
		s = pbuf;
	}
	return openlabel(s, false);
}

/**********************************************************
	static functions
**********************************************************/

static Bool
is_includebrackets(char left, char right) {
	return (left == '"' && right == '"') ||
		(left == '<' && right == '>');
}

static const char *
nextstr (const char *p, const char *c, int *n){
	int i;

	if (!p || !*p)
		return 0;

	*n = i = strcspn (p, c);	/* XXX - utf */
	if (p[i])
		i += 1;					/* strspn (p+i, c); ? */
	return p+i;
}

static char *
pathfind (const char *paths, const char *file)
{
	const char *p,*ptmp;
	int flen;
	int plen;

	if (!paths || !file)
		return 0;

	flen = strlen(file);
	p = paths;
	while((ptmp = nextstr(p, ":", &plen))!= 0) {
		int fd;
		char *tmp = malloc(plen+1+flen+1);

		if (tmp) {
			sprintf(tmp, "%.*s/%s", plen, p, file);
			if ((fd = open(tmp, 0)) < 0) {
				free(tmp);
			} else {
				close(fd);
				return tmp;
			}
		}
		p = ptmp;
	}
	return 0;
}

