/*
 * routines for making changes to the temporary file. Actually,
 * the changes are made to a new temporary file.
 */

#include "range.h"

static Subrange *changes;		/* list of changes that we've made */
static Subrange *last;

static FILE *tmpfp;
static char newtmpfile[FILENAME_MAX+1];

static ulong copyfile(ulong p0, ulong p1, FILE *fp);
static ulong save_change(ulong p0, Rune *r0, Rune *r1, Rune *r2, Rune *r3, FILE *fp);

static void
add_change(ulong p0, ulong p1, ulong q0, ulong q1)
{
	Subrange *r;

	r = salloc(sizeof(*r));
	r->p0 = p0;
	r->p1 = p1;
	r->q0 = q0;
	r->q1 = q1;
	r->next = 0;
	if (changes) {
		last->next = r;
		last = r;
	} else
		changes = last = r;
	return;
}

void
do_changes(int (*changed)(Rune **r0, Rune **r1, Rune **r2, Rune **r3))
{
	ulong origpos = 0, pos = 0, len, newpos;
	Subrange *r;

	if (tmpfp == 0) {
		tmpnam(newtmpfile);
		if ((tmpfp = fopen(newtmpfile,"w")) == 0) {
			perror(newtmpfile);
			exit(1);
		}
		Finit(&runefile, tmpfilename);
	}

	/* for each range, copy the unchanged part of the file between
	the last range and this one, then pass the range to the fn, and
	if it's changed the text. write the two returned strings to the file */

	while (r = next_range()) {
		Rune *r0, *r1, *r2, *r3;
		if (origpos < r->p0) {
			pos += copyfile(origpos, r->p0, tmpfp);
			origpos = r->p1;
		}
		r0 = runefile.getcbuf + r->p0;
		r1 = runefile.getcbuf + r->p1;
		if ((*changed)(&r0, &r1, &r2, &r3)) {
			modified = 1;
			newpos = save_change(pos,r0, r1, r2, r3, tmpfp);
			add_change(r->p0, r->p1, pos, newpos);
			pos = newpos;
		}
	}
	/* copy anything left after the last range */
	if (origpos < runefile.nrunes)
		copyfile(origpos, runefile.nrunes, tmpfp);
	fclose(tmpfp);
	if (modified) {
		remove(tmpfilename);
		strcpy(tmpfilename, newtmpfile);
	}
	return;
}


/*
 * copy a section of file, unchanged.
 */

static ulong
copyfile(ulong p0, ulong p1, FILE *fp)
{
	fwrite(runefile.getcbuf + p0, p1-p0, RUNESIZE, fp);
	return (p1 - p0);
}

/*
 * a section of the file has been changed. write the changed
 * section to the file.
 */

static ulong
save_change(ulong p0, Rune *r0, Rune *r1, Rune *r2, Rune *r3, FILE *fp)
{
	ulong len;

	if (r0) {
		len = r1 - r0;
		if  (len)
			fwrite(r0, len , RUNESIZE, fp);
		p0 += len;
	}
	if (r2) {
		len = r3 - r2;
		if  (len)
			fwrite(r2, len , RUNESIZE, fp);
		p0 += len;
	}
	return p0;
}

void
write_info(void)
{
	printf("%s\n%s\n%s\n%d\n%d\n", origfile, origaddr, tmpfilename,
		modified, base);
}

/*
 * produce file info and a list of the changes
 */

void
list_changes(void)
{
	Subrange *r = changes;

	write_info();
	for (; r; r = r->next)
		printf("#%lu,#%lu #%lu,#%lu\n", r->p0, r->p1, r->q0, r->q1);
	return;
}
