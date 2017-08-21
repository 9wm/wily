#include <u.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>

#include <msg.h>

/* ../include/msg.h */
ulong
ladjust(ulong val, Range r, int len)
{
	if( val>r.p1)
		 return val + len - RLEN(r);
	else if ( val>r.p0)
		 return r.p0;
	else
		return val;
}

ulong
radjust(ulong val, Range r, int len)
{
	if( val >r.p1)
		 return val + len - RLEN(r);
	else if ( val>=r.p0)
		return r.p0 + len;
	else
		return val;
}

/* Clip a value to be between two other values */
int
clip(int orig, int low, int high)
{
	assert(low<= high);

	if(orig<low)
		orig=low;
	if(orig>high)
		orig=high;
	return orig;
}

Range
rclip(Range r, Range c)
{
	return range( pclipr(r.p0, c), pclipr(r.p1, c));
}

ulong
pclipr(ulong p, Range r)
{
	return clip(p, r.p0, r.p1);
}

Range
intersect (Range a, Range b)
{
	return range(MAX(a.p0, b.p0), MIN(a.p1, b.p1));
}

Range
range(ulong p0, ulong p1)
{
	Range r;

	r.p0 = p0;
	r.p1 = p1;
	return r;
}

/* "Safe" realloc.   Currently all it does is crash cleanly
 * if it runs out of memory.
 */
void *
srealloc(void *orig, int size)
{
	void *p;

	assert(size>=0);

	size = size? size: 2;
	p = orig? realloc(orig, size) : malloc(size);

	if (!p){
		perror("alloc");
		abort();
	}
	return p;
}

/* "Safe" alloc.   Currently all it does is crash cleanly
 * if it runs out of memory.
 */
void *
salloc(int size)
{
	void *p;

	assert(size>=0);
	/* make sure we'll have something to free */
	size = size? size : sizeof(int);	
	p = malloc(size);
	if (!p)
		abort();	/* todo */
	return p;
}

/* Print to stderr
 */
void
eprintf(char *fmt, ...)
{
	va_list args;

	va_start(args,fmt);
	vfprintf(stderr, fmt, args);
}

