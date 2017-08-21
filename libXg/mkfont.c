/* Copyright (c) 1992 AT&T - All rights reserved. */
#include <libc.h>
#include <libg.h>
#include <string.h>

/*
 * Cobble fake font using existing subfont
 */
Font*
mkfont(Subfont *subfont)
{
	Font *font;
	unsigned char *gbuf;
	Cachesubf *c;
	char *cp;

	font = (Font *)malloc(sizeof(Font));
	if(font == 0)
		return 0;
	memset((void*)font, 0, sizeof(Font));
	cp = "<synthetic>";
	font->name = (char *)malloc(strlen(cp)+1);
	if (font->name == 0) {
		free(font);
		return 0;
	}
	strcpy(font->name, cp);
	font->nsubf = 1;
	font->subf = (Cachesubf *)malloc(font->nsubf * sizeof(Cachesubf));
	if(font->subf == 0)
	{
		free(font->name);
		free(font);
		return 0;
	}
	memset((void*)font->subf, 0, font->nsubf*sizeof(Cachesubf));
	font->height = subfont->height;
	font->ascent = subfont->ascent;
	font->ldepth = screen.ldepth;
	c = font->subf;
	subfont->minchar = subfont->mincol;	/* base font at first char */
	c->min = subfont->minchar;
	c->max = subfont->maxchar;
	c->name = 0;	/* noticed by freeup() */
	font->subf[0].f = subfont;
	return font;
}
