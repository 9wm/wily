/*
 * c.c - this handles the c, i, a and d commands.
 */

#include "range.h"

Bool debug = false;

static Rune *rarg0, *rarg1;

/*
 * Convert the supplied arg into Runes which can be inserted into the
 * new tmp file at appropriate points.
 */

static void
argtorune(char *utfarg)
{
	int len = strlen(utfarg);

	/* XXX - this is a guess - I'm assuming that  n bytes of UTF
	won't take more than n Runes to store, but I'm allocing more
	here out of paranoia. */

	rarg0 = salloc(len*(sizeof(Rune)+1));
	for (rarg1 = rarg0; len--; rarg1++)
		if (*(uchar *)utfarg < Runeself)
			*rarg1 = *utfarg++;
		else
			utfarg += chartorune(rarg1, utfarg);
	return;
}


/*
 * change functions are simple - we're given the text to be
 * changed in, *r0 and *r1, and we change the two pairs of
 * pointers to what is to be written to the file, or to null,
 * if the given range isn't to be written.
 */

static int
fn_c(Rune **r0, Rune **r1, Rune **r2, Rune **r3)
{
	*r0 = rarg0;
	*r1 = rarg1;
	*r2 = *r3 = 0;
	return 1;
}

static int
fn_i(Rune **r0, Rune **r1, Rune **r2, Rune **r3)
{
	*r2 = *r0;
	*r3 = *r1;
	*r0 = rarg0;
	*r1 = rarg1;
	return 1;
}

static int
fn_a(Rune **r0, Rune **r1, Rune **r2, Rune **r3)
{
	*r2 = rarg0;
	*r3 = rarg1;
	return 1;
}

static int
fn_d(Rune **r0, Rune **r1, Rune **r2, Rune **r3)
{
	*r0 = *r1 = *r2 = *r3 = 0;
	return 1;
}


int
main(int argc, char *argv[])
{
	char *p;
	size_t len;
	Subrange *r;
	int (*fn)(Rune **r0, Rune **r1, Rune **r2, Rune **r3);

	p = strrchr(argv[0], '/');
	p = p? p+1 : argv[0];
	switch (*p) {
		case 'c' :	fn = fn_c;	break;
		case 'i' :		fn = fn_i;	break;
		case 'd' :	fn = fn_d;	break;
		case 'a' :	fn = fn_a;	break;
		default:
			fprintf(stderr,"Uknown program name!\n");
			exit(1);
	}
	if (*p != 'd') {
		if (argc < 2) {
			fprintf(stderr,"%c: need text argument\n", *p);
			exit(1);
		}
		argtorune(argv[1]);
	}
	read_info(0);		/* don't reverse ranges */
	do_changes(fn);
	list_changes();
	exit(0);
}
