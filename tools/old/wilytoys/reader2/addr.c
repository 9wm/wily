/*
 * addr.c - parse email addresses into something more readable.
 * smk 6/3/96.
 */

#include "headers.h"
#include <ctype.h>


static char *trimws(char *str);
static void whatsleft(char *copy, char *found, char *foundend, char **rest);
static int angle(char *copy, char **es, char **ee);
static int paren(char *copy, char **ns, char **ne);
static int quotes(char *copy, char **ns, char **ne);
static int findchars(char *copy, char **s, char **e, char c1, char c2);
static int at(char *copy, char **es, char **ee);


/*
 * parseaddr(addr, len, email, name) - parses addr into an email address and a full
 * name, based rather vaguely on RFC822:
 * if <...> found
 * 	use it for email address
 * 	use what's left as name
 * else if () or "" found
 * 	use it for name
 * 	use what's left as email
 * else if @ found
 * 	use it for email address
 * 	use what's left as name
 * else
 * 	use everything as both email address and name.
 */

void
parseaddr(char *addr, int len, char **email, char **name)
{
	static char copy[MAXFROMLINE+1];
	char *cp, *e, *n, *z;

	assert(addr && *addr && email && name);
	strncpy(copy, addr, len);
	copy[len] = 0;
	cp = trimws(copy);
	assert(*cp);
	if (angle(cp, &e, &z))
		whatsleft(cp,e,z, &n);
	else if (paren(cp, &n, &z))
		whatsleft(cp,n,z,&e);
	else if (quotes(cp, &n, &z))
		whatsleft(cp,n,z,&e);
	else if (at(cp, &e, &z))
		whatsleft(cp,e,z,&n);
	else
		e = n = copy;
	*email = trimws(e);
	*name = trimws(n);
	return;
}

static char *
trimws(char *str)
{
	char *s;
	assert(str);

	while (isspace(*str))
		str++;
	s = str + strlen(str) - 1;
	while (str < s && isspace(*s))
		*s-- = 0;
	return str;
}

/*
 * found points to the located item. foundend points to the character
 * after the end of the item (where we'd put the null char).
 * If we find any alphanumeric data between copy and found,
 * we assume that it's the other part of the address string.
 * Otherwise, we take any alphanumeric data after foundend.
 */

static void
whatsleft(char *copy, char *found, char *foundend, char **rest)
{
	char *s;

	assert(copy <= found);
	for (s = copy; s < found; s++)
		if (isalnum(*s)) {
			*rest = s;
			*(found - 1) = *foundend = 0;
			return;
		}
	for (s = foundend; *s; s++)
		if (isalnum(*s)) {
			*rest = s;
			*foundend = 0;
			return;
		}
	*rest = found;
	*foundend = 0;
	return;
}

static int
angle(char *copy, char **es, char **ee)
{
	return findchars(copy,es,ee,'<', '>');
}

static int
paren(char *copy, char **ns, char **ne)
{
	return findchars(copy,ns,ne,'(', ')');
}

static int
quotes(char *copy, char **ns, char **ne)
{
	return findchars(copy,ns,ne,'"', '"');
}

static int
findchars(char *copy, char **s, char **e, char c1, char c2)
{
	return (*s = strchr(copy,c1)) && *++(*s) && (*e = strchr(*s,c2));
}

static int
at(char *copy, char **es, char **ee)
{
	char *a;

	if ((a = strchr(copy,'@')) == 0)
		return 0;
	for (*es = a; copy < *es && !isspace(**es); --*es);
	for (*ee = a; **ee && !isspace(**ee); (*ee)++);
	return 1;
}

