/* Copyright (c) 1992 AT&T - All rights reserved. */
#include <libc.h>
#include <libg.h>
#include "libgint.h"

#define	PJW	0	/* use NUL==pjw for invisible characters */

static Cachesubf
*findsubfont(Font *f, Rune r, int *cn)
{
	int n, i, c;
	Rune rx;
	Cachesubf *csf;
	Subfont *sf;

	for (i = 0, rx = r; i < 2; i++, rx = PJW)
		for (n=0, csf=f->subf; n < f->nsubf; n++, csf++)
			if (csf->min <= rx && rx <= csf->max) {
				if (!csf->f) {
					csf->f = getsubfont(csf->name);
					if (!csf->f)
						return 0;
				}
				sf = csf->f;
				c = rx-csf->min+sf->minchar;
				c = ((c>>8)-sf->minrow)*sf->width+(c&0xff)-sf->mincol;
				if (c < 0)
					break;
					/* ignore zero width characters */
				if (sf->info[c].cwidth == 0 && sf->info[c].width == 0)
					break;
				*cn = c;
				return csf;
			}
	return 0;
}

int
cachechars(Font *f, char **s, void *cp, int max, int *wp, unsigned short *fp)
{
	int i, w, wid, charnum;
	Rune r;
	char *sp;
	Cachesubf *csf;

	sp = *s;
	wid = 0;

	for (i=0; *sp && i<max; sp+=w) {
		r = *(unsigned char *)sp;
		if (r < Runeself)
			w = 1;
		else
			w = chartorune(&r, sp);
		csf = findsubfont(f, r, &charnum);
		if (!csf)
			break;
		wid += csf->f->info[charnum].width;
		fp[i] = csf-f->subf;		/* subfont number */
		((XChar2b*)cp)[i].byte1 = charnum/csf->f->width+csf->f->minrow;
		((XChar2b*)cp)[i].byte2 = charnum%csf->f->width+csf->f->mincol;
		i++;
	}
	*s = sp;
	*wp = wid;
	return i;
}

long
charwidth(Font *f, Rune r)
{
	Cachesubf *csf;
	int charnum;

	if (r == 0)
		berror("NUL in charwidth");	/* difficult BUG */

	csf = findsubfont(f, r, &charnum);
	if (!csf)
		return 0;
	else
		return csf->f->info[charnum].width;
}

void
subffree(Subfont *f)
{
	if (f->info)
		free(f->info);	/* note: f->info must have been malloc'ed! */
	free(f);
}
