/*******************************************
 *	Maintain and query (regexp, tag tools) pairs
 *******************************************/

#include "wily.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

typedef struct Pair Pair;
struct Pair {
	char*	regex;
	char*	tools;
};

static Pair* pair;
static int	npairs = 0;
static int maxpairs = 0;

static Bool
match(char *regex, char *s) {
	/* Make a dummy text file, search in that */
	static Text *t=0;
	Range r;
	Bool	retval;
	
	if(!t)
		t  = text_alloc(0, false);
	r = nr;
	text_replaceutf(t, text_all(t), s);
	text_replaceutf(t, range(text_length(t),text_length(t)), "\n");
	retval =  text_utfregexp(t, regex, &r, true);
	
	return retval;
}

static void
addpair(char *regex, char* tools) {
	Pair p;
	char	*nl;
	
	p.regex = regex;
	p.tools = tools;
	if ((nl = strchr(p.tools, '\n'))) {
		*nl = 0;
	}
	if(npairs == maxpairs) {
		maxpairs = maxpairs? maxpairs*2 : maxpairs + 10;
		pair = (Pair*) srealloc(pair, maxpairs * sizeof(Pair));
	}
	pair[npairs++] = p;
}

char*
tag_match(char*label) {
	int	j;
	static Bool inprogress;
	
	if(inprogress) {
		j= npairs;
	} else {
		inprogress = true;
		for(j=0; j<npairs; j++)
			if(match(pair[j].regex, label))
				break;
		inprogress = false;
	}
	return (j<npairs)? pair[j].tools : "";
}

/* Alloc a buffer, read 'filename' into it, return the buffer.
 * Returns 0 if there's some problem.
 */
static char*
readfile(char*filename) {
	char *buf;
	struct stat statbuf;
	int	fd;
	off_t	size;
	int	nread;
	
	fd = open(filename, O_RDONLY);
	if( (fd<0) || fstat(fd, &statbuf)) {
		return 0;
	}
	
	size = statbuf.st_size;
	buf = salloc (size+10);
	while(size>0) {
		nread = read(fd, buf, size);
		if(nread <= 0) {
			perror(filename);
			free(buf);
			return 0;
		}
		size -= nread;
	}
	close(fd);
	return buf;
}

/*
 * Read 'filename', initialize 'pair' and 'npairs'.
 * 'filename' is made up of lines, each of which may
 * be blank, a comment (starts with '#'), or a pattern
 * toolset pair (separated by tabs)
 */
void
tag_init(char *filename) {
	char	*buf, *ptr, *tab;
	
	if(!(buf=readfile(filename)))
		return;
	
	for(ptr = strtok(buf, "\n"); ptr; ptr = strtok(0, "\n")) {
		/* comment or blank line */
		if (ptr[0] == '#' || isspace(ptr[0]))
			continue;
		if((tab = strchr(ptr, '\t'))) {
			*tab++ = 0;
			/* strip leading whitespace */
			tab += strspn(tab, whitespace);
			addpair(ptr, tab);
		} else {
			addpair(ptr, "");
		}
	}
	/* don't free(buf) - keep the memory in the array of strings */
}
