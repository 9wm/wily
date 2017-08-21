/*
 * newsrc.c - loading, parsing and updating the list of newsgoups/articles
 * that have been read.
 */

#include "newsheaders.h"

static void init_filenames(void);
static FILE *lock_newsrc(void);
static nGroup *newsrcLine(FILE *fp);
static char *newsrcGroup(FILE *fp);
static nRange *newsrcRanges(FILE *fp);
static nRange *newsrcRange(FILE *fp, int *end);
static void newsrcCompress(nGroup *g);

static char newsrcname[MAXPATHLEN+1];
static char newsrctmp[MAXPATHLEN+1];
static char newsrcold[MAXPATHLEN+1];
static char lockname[MAXPATHLEN+1];

static int newsrcline;

static void
init_filenames(void)
{
	char *h = getenv("HOME");

	if (!h || !*h) {
		fprintf(stderr,"$HOME is not set\n");
		exit(1);			/* XXX- should really get pwdent */
	}
	sprintf(newsrcname,"%s/%s", h, NEWSRC);
	sprintf(newsrctmp,"%s/%s.tmp", h, NEWSRC);
	sprintf(newsrcold,"%s/%s.old", h, NEWSRC);
	sprintf(lockname,"%s/%s", h, LOCKFILE);
	DPRINT("newsrc is...");
	DPRINT(newsrcname);
	DPRINT("lockfile is...");
	DPRINT(lockname);
	return;
}

static FILE *
lock_newsrc(void)
{
	FILE *fp;

	DPRINT("Locking .newsrc...");
	if (link(newsrcname, lockname)) {
		perror(".newsrc link");
		fprintf(stderr,"Could not lock %s with link to %s\n", newsrcname, lockname);
		return 0;
	}
	DPRINT("Newsrc locked - opening");
	if ((fp = fopen(newsrcname,"r")) == 0) {
		DPRINT("could not open newsrc");
		unlock_newsrc();
	}
	atexit(unlock_newsrc);
	return fp;
}

void
unlock_newsrc(void)
{
	DPRINT("Unlocking newsrc");
	remove(lockname);
	DPRINT("Newsrc unlocked");
}

nGroup *
read_newsrc(void)
{
	FILE *fp;
	nGroup *groups = 0, *g;
	nGroup **p;

	DPRINT("Reading newsrc");
	newsrcline = 1;
	init_filenames();
	if ((fp = lock_newsrc()) == 0) {
		DPRINT("Could not lock newsrc");
		return 0;
	}
	for (p = &groups; (g = newsrcLine(fp)); p = &g->next)
		*p = g;
	DPRINT("Closing newsrc");
	fclose(fp);
	return groups;
}

static nGroup *
newsrcLine(FILE *fp)
{
	nGroup *g = salloc(sizeof(*g));
	int c;

	memset(g,0,sizeof(*g));
	if ((g->name = newsrcGroup(fp)) == 0)
		goto broken;
	switch ((c = fgetc(fp))) {
		case SUBSEP:
			g->issub = 1;
			break;
		case NOSUBSEP:
			g->issub = 0;
			break;
		default:
			DPRINT("Don't recognise group name delimiter in line");
			goto broken;
	}
	g->read = newsrcRanges(fp);
	newsrcline++;
	return g;
broken:
	if (g->name)
		free(g->name);
	free(g);
	return 0;
}

static char *
newsrcGroup(FILE *fp)
{
	int c, l = 0;
	static char str[MAXPATHLEN+1];

	while ((c = fgetc(fp)) != SUBSEP && c != NOSUBSEP) {
		if (c == EOF || l == MAXPATHLEN) {
			if (l)
				fprintf(stderr,"%s corrupt: line %d\n", newsrcname, newsrcline);
			return 0;
		}
		str[l++] = c;
	}
	assert(l);
	ungetc(c, fp);
	str[l] = 0;
	return sstrdup(str);
}

static nRange *
newsrcRanges(FILE *fp)
{
	nRange *ranges = 0, *r;
	nRange **p;
	int c, end = 0;

	while (isspace((c = fgetc(fp))) && c!='\n') ;
	if (c == '\n')
		return 0;
	ungetc(c,fp);
	for (p = &ranges; r = newsrcRange(fp, &end); p = &r->next) {
		*p = r;
		if (end)
			break;
	}
	assert(ranges);
	return ranges;
}

static nRange *
newsrcRange(FILE *fp, int *end)
{
	nRange *r = salloc(sizeof(*r));
	int c, l;
	static char num[80];			/* XXX wild guess */

	r->next = 0;
	for (l = 0; isdigit((c = fgetc(fp))); num[l++] = c) ;
	num[l] = 0;
	r->n0 = r->n1 = atoi(num);
	if (c == '-') {
		for (l = 0; isdigit((c = fgetc(fp))); num[l++] = c) ;
		num[l] = 0;
		r->n1= atoi(num);
	}
	if (c != RANGESEP && c != '\n')
		while ((c = fgetc(fp)) != EOF && c != '\n');
	if (c != RANGESEP)
		*end = 1;
	return r;
}

int
update_newsrc(nGroup *groups)
{
	nGroup *g;
	nRange *r;
	FILE *fp;

	assert(groups);
	DPRINT("Updating newsrc");
	newsrcCompress(groups);
	if ((fp = fopen(newsrctmp,"w")) == 0) {
		perror(newsrctmp);
		return 1;
	}
	for (g = groups; g; g = g->next) {
		char *s = "";
		char *fmt = g->read? "%s%c " : "%s%c";
		fprintf(fp, fmt, g->name, g->issub? SUBSEP : NOSUBSEP);
		for (r = g->read; r; r= r->next, s = ",")
			if (r->n0 == r->n1)
				fprintf(fp, "%s%d",s,r->n0);
			else
				fprintf(fp,"%s%d-%d",s,r->n0, r->n1);
		fputc('\n',fp);
	}
	if (fclose(fp)) {
		perror(newsrctmp);
		remove(newsrctmp);
		return 1;
	}
	if (rename(newsrcname, newsrcold) || rename(newsrctmp, newsrcname)) {
		perror("renaming .newsrc");
		return 1;
	}
	DPRINT("Newsrc updated");
	return 0;
}

/*
 * Some routines for handling the list of ranges of read articles.
 */

void
newsrcMarkRead(nGroup *g, int i)
{
	nRange **p;
	nRange *r;

	assert(g);
	for (p = &g->read; *p && (*p)->n0 < i; p = &((*p)->next)) ;

	/* have now reached the end of the list, the range containing
	i, or the range that comes after i. */

	if (!*p || i < (*p)->n0) {
		/* does insert at end, insert at start, or insert before a range */
		r = salloc(sizeof(*r));
		r->n0 = r->n1 = i;
		r->next = *p;
		*p = r;
		return;
	}
	/* only remaining case is that the current range already contains i */
	assert(*p);
	assert((*p)->n0 <= i && i <= (*p)->n1);
	return;
}

void
newsrcMarkUnread(nGroup *g, int i)
{
	nRange **p;
	nRange *r, *n;

	assert(g);
	for (p = &g->read; *p && (*p)->n0 < i; p = &((*p)->next)) {
		r = *p;
		if (r->n0 == i && i == r->n1) {
			*p = r->next;
			free(r);
			return;
		} else if (r->n0 == i) {
			r->n0++;
			return;
		} else if (r->n1 == i) {
			r->n1--;
			return;
		} else if (r->n0 < i && i < r->n1) {
			n = salloc(sizeof(*n));
			n->n1 = r->n1;
			n->n0 = i + 1;
			n->next = r->next;
			r->n1 = i - 1;
			r->next = n;
			return;
		}
	}
	/* i isn't in any of the items in the list */
	return;
}

/*
 * compress the list, by combining adjacent ranges
 */

static void
newsrcCompress(nGroup *g)
{
	nRange *r, *n;

	assert(g);
	for (; g; g = g->next)
		for (r = g->read; r; r = r->next)
			for (n = r->next; n && n->n0 <= r->n1 + 1; n = r->next) {
				r->n1 = n->n1;
				r->next = n->next;
				free(n);
			}
	return;
}
