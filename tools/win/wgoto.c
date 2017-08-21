#include <u.h>
#include <libc.h>
#include <msg.h>

#include <stdio.h>
#include <stdarg.h>

#include <unistd.h>

int	wilyfd;
Handle *handle;
Id id;

void error(const char *, ...);
char *getfilename(char *);
char *readfd(int);

#define STRINGIZE2(x) #x
#define STRINGIZE(x) STRINGIZE2(x)
#ifdef __GNUC__
	#define here " at " __FILE__ ":" STRINGIZE(__LINE__) " in " __FUNCTION__ "()"
#else
	#define here " at " __FILE__ ":" STRINGIZE(__LINE__)
#endif

#define PROGRAMNAME		"wgoto"
#define VERSIONNAME		"%R%.%L% of %D%"

char programname[]	= PROGRAMNAME;
char versioninfo[]	= PROGRAMNAME " " VERSIONNAME;
char usageinfo[]	= "usage: " PROGRAMNAME " [-v] address";

char whatinfo[]		= "@(#)" PROGRAMNAME " " VERSIONNAME;

void
main(int argc, char **argv)
{
	int c;
	extern char *optarg;
	extern int optind;
	extern int opterr;
	char *msg;
	char *address;
	Range r;

	/* parse arguments */
	opterr = 0;
	while ((c = getopt(argc, argv, "v")) != -1) {
		switch (c) {
		case 'v':
			fprintf(stderr, "%s\n", versioninfo);
			exit(0);
		default:
			fprintf(stderr, "%s\n", usageinfo);
			exit(1);
			break;
		}
	}
	if (argv[optind] == 0 || argv[optind + 1] != 0) {
		fprintf(stderr, "%s\n", usageinfo);
		exit(1);
	}
	address = argv[optind];
	
	/* open connection */
	wilyfd = client_connect();
	if (wilyfd < 0) {
		error("client_connect() failed" here);
		exit(1);
	}
	handle = rpc_init(wilyfd);

	/* get address */
	msg = rpc_goto(handle, &id, &r, strdup(address), 1);
	if (msg != 0) {
		error("rpc_goto() failed" here ": %s", msg);
		exit(1);
	}
	if (r.p0 > r.p1) {
		error("unable to find %s", address);
		exit(1);
	}
	
	exit(0);
}

/*
 * error(fmt, ...):
 *
 * Print program name, ": ", message, and a newline to stderr.
 */
void
error(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", programname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
