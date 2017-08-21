/*
 * x.c - this is actually the source for x, y, g and v. There's not a lot
 * of point having separate programs...
 */

#include "range.h"

Bool	debug = false;

static const char *fmt = "#%lu,#%lu\n";

void
fn_x(ulong p0, ulong p1)
{
	ulong q0, q1;

	while (p0 < p1 && run_regexp(p0, p1, &q0, &q1)) {
		printf(fmt,q0,q1);
		p0 = q1;
	}
}

void
fn_y(ulong p0, ulong p1)
{
	ulong q0, q1;

	while (p0 < p1 && run_regexp(p0, p1, &q0, &q1)) {
		if (p0 < q0)
			printf(fmt,p0,q0);
		p0 = q1;
	}
	if (p0 < p1)
		printf(fmt,p0,p1);
}

void
fn_g(ulong p0, ulong p1)
{
	ulong q0, q1;

	if (p0 < p1 && run_regexp(p0, p1, &q0, &q1))
		printf(fmt,p0,p1);
}

void
fn_v(ulong p0, ulong p1)
{
	ulong q0, q1;

	if (p0 < p1 && run_regexp(p0, p1, &q0, &q1)==0)
		printf(fmt,p0,p1);
}

int
main(int argc, char *argv[])
{
	char *p, *re;
	size_t len;
	Subrange *r;
	void (*fn)(ulong p0, ulong p1);

	/* This isn't wonderful: assume that the regexp is delimited by // */
	if (argc < 2 || *(re = argv[1]) != '/'  || (len = strlen(re)) < 2) {
		fprintf(stderr,"Usage: x /regexp/\n");
		exit(1);
	}
	if (re[--len] == '/')
		re[len] = 0;
	re++;		/* skip / */
	if (len == 0) {
		fprintf(stderr,"null regexp\n");
		exit(1);
	}
	if (init_regexp(re)) {
		fprintf(stderr,"Invalid regexp\n");
		exit(1);
	}
	p = strrchr(argv[0], '/');
	p = p? p+1 : argv[0];
	switch (*p) {
		case 'x' :	fn = fn_x;	break;
		case 'y' :	fn = fn_y;	break;
		case 'g' :	fn = fn_g;	break;
		case 'v' :	fn = fn_v;	break;
		default:
			fprintf(stderr,"Uknown program name!\n");
			exit(1);
	}
	read_info(0);		/* Don't reverse ranges */
	Finit(&runefile, tmpfilename);
	write_info();
	while (r = next_range())
		(*fn)(r->p0, r->p1);
	exit(0);
}
