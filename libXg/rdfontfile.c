#include <u.h>
#include <libc.h>
#include <libg.h>
#include "libgint.h"
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

static char*
skip(char *s)
{
	while (*s==' ' || *s=='\n' || *s=='\t')
		s++;
	return s;
}

Font *
rdfontfile(char *name, int ldepth)
{
	Font *fnt;
	Cachesubf *c;
	int fd, i;
	char *buf, *s, *t;
	struct stat sbuf;
	unsigned long min, max;

	fd = open(name, O_RDONLY);
	if (fd < 0)
		return 0;
	if (fstat(fd, &sbuf) < 0)
	{
    Err0:
		close(fd);
		return 0;
	}
	buf = (char *)malloc(sbuf.st_size+1);
	if (buf == 0)
		goto Err0;
	buf[sbuf.st_size] = 0;
	i = read(fd, buf, sbuf.st_size);
	close(fd);
	if (i != sbuf.st_size)
	{
    Err1:
		free(buf);
		return 0;
	}

	s = buf;
	fnt = (Font *)malloc(sizeof(Font));
	if (fnt == 0)
		goto Err1;
	memset((void*)fnt, 0, sizeof(Font));
	fnt->name = (char *)malloc(strlen(name)+1);
	if (fnt->name==0)
	{
    Err2:
		free(fnt->name);
		free(fnt);
		goto Err1;
	}
	strcpy(fnt->name, name);
	fnt->height = strtol(s, &s, 0);
	s = skip(s);
	fnt->ascent = strtol(s, &s, 0);
	s = skip(s);
	if (fnt->height<=0 || fnt->ascent<=0)
		goto Err2;
	fnt->width = 0;
	fnt->ldepth = ldepth;

	fnt->id = 0;
	fnt->nsubf = 0;
	fnt->subf = 0;

	do {
		min = strtol(s, &s, 0);
		s = skip(s);
		max = strtol(s, &s, 0);
		s = skip(s);
		if(*s==0 || min>=65536 || max>=65536 || min>max)
		{
    Err3:
			ffree(fnt);
			return 0;
		}
		if (fnt->subf)
			fnt->subf = (Cachesubf *)realloc(fnt->subf, (fnt->nsubf+1)*sizeof(Cachesubf));
		else
			fnt->subf = (Cachesubf *)malloc(sizeof(Cachesubf));
		if (fnt->subf == 0)
		{
			/* realloc manual says fnt->subf may have been destroyed */
			fnt->nsubf = 0;
			goto Err3;
		}
		c = &fnt->subf[fnt->nsubf];
		c->min = min;
		c->max = max;
		t = s;
		while (*s && *s!=' ' && *s!='\n' && *s!='\t')
			s++;
		*s++ = 0;
		c->f = (Subfont *)0;
		c->name = (char *)malloc(strlen(t)+1);
		if (c->name == 0)
		{
			free(c);
			goto Err3;
		}
		strcpy(c->name, t);
		s = skip(s);
		fnt->nsubf++;
	} while(*s);
	free(buf);
	return fnt;
}

void
ffree(Font *f)
{
	int i;
	Cachesubf *c;
	unsigned char *b;

	for (i=0; i<f->nsubf; i++){
		c = f->subf+i;
		if (c->f)
			subffree(c->f);
		free(c->name);
		free(c);
	}
	free(f->subf);
	free(f);
}
