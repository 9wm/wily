/*******************************************
 *	A few utilities
 *******************************************/

#include "wily.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <dirent.h>

void
dirnametrunc(char*s){
	if((s=strrchr(s,'/')))
		*(++s) = 0;
}

/* Combine 'add' with 'context', put the result in 'dest' */
void
addcontext(char*dest, char*context, char*add){
	char*s;
	if(strchr("/$~", add[0])){
		strcpy(dest,add);
		return;
	} 
	strcpy(dest, context);
	if(!(s = strrchr(dest, '/'))){
		label2path(dest, context);
		if(!(s = strrchr(dest, '/'))){
			s = dest + strlen(dest) -1;
		}
	}
	strcpy(s+1,add);
	labelclean(dest);
}

/* Set the name of the window where output from the context
 * of 'label' will appear.
 */
void
olabel(char*out, char*label){
	addcontext(out, label, "+Errors");
}

/* Compares two stat buffers, returns 0 if they have
 * the same inode and device numbers.
 */
int
statcmp(Stat*a, Stat*b) {
	if(a->st_ino != b->st_ino || a->st_dev != b->st_dev)
		return -1;
	else
		return 0;
}

Bool
isdir(char*path) {
	struct stat buf;
	
	return  !stat(path, &buf) && S_ISDIR(buf.st_mode);
}

/*
 * Return Rstring for utf.  Either s.r0 == s.r1 == 0, 
 * or s.r0 will need to be free
 */
Rstring 
utf2rstring(char*utf)
{
	Rstring	s;
	int	len;

	if( (len = utflen(utf)) ) {
		s.r0 = salloc(len*sizeof(Rune));
		s.r1 = s.r0 + utftotext(s.r0, utf, utf+strlen(utf));
	} else
		s.r0 = s.r1 = 0;

	return s;
}

/* Write into 'back' the name of a file we can write to as a backup
 * for 'orig'.  Return 0 for success. */
int
backup_name(char *orig, char *back)
{
	Path	dir, guide;
	char	*home;
	DIR		*dirp;
	struct dirent	*direntp;
	FILE	*fp;
	int	max,n;
	int	init_guide = 0;

	if ( !(home=getenv("WILYBAK")) ) {
		if ( !(home=getenv("HOME")) ) {
			return diag(0, "getenv HOME");
		}
		sprintf(dir, "%s/.wilybak", home);
	} else
		strcpy(dir, home);

	/* Make sure the directory exists.  Create it if necessary.
	 */
	if(access(dir, W_OK) &&  (mkdir(dir, 0700)) )
		return diag(0, "couldn't create backup directory %s", dir);

	/* Find directory entry with largest number.  We will be one
	 * greater than that.
	 */
	max=0;
	if(!(dirp = opendir(dir))) {
		return diag(0, "couldn't opendir %s", dir);
	}
	rewinddir(dirp);	/* Workaround for FreeBSD. */
	while ((direntp = readdir(dirp))) {
		if ( (n=atoi(direntp->d_name)) > max)
			max = n;
	}
	closedir(dirp);
	max++;

	sprintf(back, "%s/%d", dir, max);


	/* Record what is going where */
	sprintf(guide,"%s/guide", dir);
	if(access(guide, W_OK) < 0)
		init_guide = 1;
	fp = fopen(guide, "a+");
	if(fp) {	/* if this fails, don't care all that much */
		if(init_guide)
			fprintf(fp, "diff	cp	rm *\n");
		fprintf(fp, "%3d\t%s\n", max, orig);
		fclose(fp);
	} else {
		diag(guide, "couldn't update backup guide file");
	}
	return 0;
}

void
noutput(char *context, char *base, int n)
{
	Path	errwin;
	View	*v;
	Range	r;
	char	*s;
	Text	*t;
	ulong	p;

	strcpy(errwin, context? context : wilydir);
	if((s = strrchr(errwin, '/')))
		s++;
	else
		s = errwin +strlen(errwin);
	strcpy(s, "+Errors");
	
	v = openlabel(errwin, true);
	base[n] = 0;
	t = view_text(v);
	p = text_length(t);
	r = text_replaceutf(t, range(p,p), base);
	r.p0 = r.p1;	/* most interested in the end bit */
	view_show(v, r);
}

/*
 * Given strings stored in (null-terminated) 'item', arrange them in
 * columns to fit neatly in a window with given 'totalwidth', 'tabwidth'
 * and font 'f'.  Return the finished, allocated, null-terminated
 * string.
 */
char *
columnate(int totalwidth, int tabwidth, Font *f, char **item)
{
	int	rows, columns, row, column;
	int	maxwidth;
	int	j, widest,nitems, ntabs, biggest;
	int	*width;
	char	*buf, *s, **c;
	int	remaining;


	/* count the items */
	nitems = 0;
	c= item;
	while(*c++) 
		nitems++;

	if(!nitems)
		return strdup("");

	/* width[j] - width of string j
	 * widest - index of widest string
	 * maxwidth - width of largest string plus a tab
	 */
	widest = 0;
	width = (int*)salloc(nitems*sizeof(int));
	for(j=0; j< nitems; j++) {
		width[j] = strwidth(f, item[j]);
		if (width[j]>width[widest])
			widest = j;
	}

	biggest = width[widest] + strwidth(f, "W");
	ntabs = biggest/tabwidth;
	if (biggest % tabwidth)
		ntabs++;
	maxwidth = ntabs*tabwidth;
	columns = ((totalwidth -biggest) / maxwidth) +1;
	rows = nitems / columns;
	if (nitems % columns)
		rows++;

	s = buf = (char*)salloc(nitems*(strlen(item[widest])+4));

	remaining = nitems;
	for(row=0; ; row++) {
		for(column = 0; column < columns; column++) {
			int	current, deficit;

			current = column*rows + row;
			if (current >= nitems)
				break;
			deficit = maxwidth - width[current];
			ntabs = deficit / tabwidth;
			if (deficit % tabwidth)
				ntabs++;
			if(column==columns-1)
				ntabs=0;		/* no tabs for last column */
			assert(ntabs < 48);
			strcpy(s, item[current]);
			s += strlen(item[current]);
			while(ntabs-->0)
				*s++ = '\t';
			if (!(--remaining))
				goto done;
		}
		*s++ = '\n';
	}
done:
	if(column)
		*s++ = '\n';
	*s++='\0';
	free(width);
	return buf;
}

static void
cleanup(void){
	data_backupall();
	fifo_cleanup();
}

void
cleanup_and_die(int n)
{
	cleanup();
	exit(0);
}

void
cleanup_and_abort(int n)
{
	perror("wily: something horrible happened:");
	cleanup();
	abort();
}

/* Send a diagnostic message to the appropriate place,
 * given its context.
 */
int
diag(char *context, char *fmt, ...)
{
	va_list args;
	Path	msg;
	char	*err,*s;

	s = msg;
	if ( errno && (err=strerror(errno)) ) {
		sprintf(msg, "diag: %s: ", err);
		s += strlen(s);
	}
	va_start(args,fmt);
	vsprintf(s, fmt, args);
	va_end(args);
	strcat(msg, "\n");
	assert(strlen(msg)<1024);
	noutput(context, msg, strlen(msg));
	return 1;
}

Rstring
rstring(Rune*r0, Rune*r1) {
	Rstring	s;

	s.r0 = r0;
	s.r1 = r1;
	return s;
}

char*
mybasename(char*f) {
	char	*s;
	s=strrchr(f,'/');
	return s ? s+1 : f;
}

ulong
texttoutf(char *s, Rune *r1, Rune *r2) {
	Rune	*q;
	char	*t;
	
	if (r2 <= r1)
		return 0;
	for (t = s, q = r1; q < r2; q++)
		t += runetochar(t, q);
	return t-s;
}

int
distance(Point p1, Point p2)
{
	return (p1.x-p2.x)*(p1.x-p2.x) + (p1.y-p2.y)*(p1.y-p2.y);
}

/* Error we should be able to recover from
 */
void
error(char *fmt, ...)
{
	va_list args;

	perror("wily:");
	va_start(args,fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

/* Error we cannot recover from */
void
fatal(char *fmt, ...)
{
	va_list args;

	perror("wily:");
	va_start(args,fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	cleanup_and_abort(0);
}

/*
 *	dummy mouse driver for the frame library
 */
void frgetmouse(void) {}

/* Store runes from 't' into X clipboard as UTF
 */
void
snarf(Text *t, Range r)
{
	char	*buf;

  	assert(t);
  	if(RLEN(r)){
		buf = text_duputf(t, r);
		select_put(buf);
		free(buf);
	}
}

/* Replace range 'r' of 't' with the snarf buffer, return
 * the range of the new text.
 */
Range
paste(Text *t, Range r)
{
	char	*cbuf;
	Rune	*rbuf;
	int	n;
	Rstring	s;

  	assert(t);
 	assert(ROK(r));

	cbuf = select_get();	/* not to be freed */
	rbuf = (Rune *)salloc(sizeof(Rune)*(utflen(cbuf)+1));
	n  = utftotext(rbuf, cbuf, cbuf+strlen(cbuf));
	s.r0 = rbuf;
	s.r1 = rbuf + n;
	text_replace(t, r, s);

	r.p1 = r.p0 + n;
	free(rbuf);
	return r;
}

void
add_slash(char*s)
{
	int	n;

	n = strlen(s);
	if(s[n-1] != '/'){
		s[n++]='/';
		s[n]='\0';
	}
}

Bool
frame_isfull(Frame*f)
{
	return f->nlines == f->maxlines && f->lastlinefull;
}

Bool	utfHadNulls;
int	utftotext_unconverted;	/* bytes at the end which weren't converted */

/* Convert UTF from s1 to s2 into runes, store them at r
 * Returns the number of runes stored.
 * Sets utfHadNulls to indicate that some of the UTF contained nulls,
 * which are _not_ converted into null Runes.
 */
ulong
utftotext(Rune *r, char *s1, char *s2)
{
	Rune	*q;
	char	*v;
	
	utfHadNulls = false;
	if (s2 <= s1)
		return 0;
	for (v = s1, q = r; v < s2; ) {
		if (!(*(uchar*)v)) {
			utfHadNulls = true;
			v++;
			continue;
		} else if (*(uchar *)v < Runeself) {
			*q = *v++;
		} else {
			v += chartorune(q, v);
		}
		assert(*q);
		q++;
	}
	if (v > s2) {
		int	length;
		q--;
		length  = runelen(*q);
		utftotext_unconverted = s2 - (v-length);
	}
	return q-r;
}

