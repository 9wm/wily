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

#define PROGRAMNAME		"wreplace"
#define VERSIONNAME		"%R%.%L% of %D%"

char programname[]	= PROGRAMNAME;
char versioninfo[]	= PROGRAMNAME " " VERSIONNAME;
char usageinfo[]	= "usage: " PROGRAMNAME " [-v] [-c] [-b] address";

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
	char *filename;
	Range r;
	Bool create, backup;

	/* parse arguments */
	opterr = 0;
	create = false;
	backup = false;
	while ((c = getopt(argc, argv, "vcb")) != -1) {
		switch (c) {
		case 'v':
			fprintf(stderr, "%s\n", versioninfo);
			exit(0);
		case 'c':
			create = true;
			break;
		case 'b':
			backup = true;
			break;
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

	/* create window if requested */
	if (create) {
		filename = getfilename(address);
		msg = rpc_new(handle, filename, &id, backup);
		if (msg != 0) {
			error("rpc_new() failed" here ": %s", msg);
			exit(1);
		}
	}
	
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
	
	/* do replacement */
	msg = rpc_replace(handle, id, r, readfd(0));
	if (msg != 0) {
		error("rpc_replace() failed " here ": %s", msg);
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

/*
 * getfilename(address):
 *
 * Return the filename part of an address or 0
 * if there is none.
 *
 * Returned value is a pointer to malloc()-ed storage.
 */
char *
getfilename(char *address)
{
	char *colon;
	char *filename;
	size_t len;
	
	colon = strchr(address, ':');
	if (colon == 0)
		len = strlen(address);
	else
		len = colon - address;
	
	filename = malloc(len + 1);
	if (filename == 0) {
		error("malloc() failed" here);
		exit(1);
	}
	
	memcpy(filename, address, len);
	filename[len] = 0;
	
	return filename;
}

/*
 * readfd(fd):
 *
 * Read from a fd into a malloc()-ed string.
 */
char *
readfd(int fd)
{
	char *buf;
	size_t bufsize, buflen;
	const size_t bufquantum = BUFSIZ;
	ssize_t nread;
	
	buflen = 0;
	bufsize = 0;
	buf = 0;

	do {
	
		if (buflen + 1 >= bufsize) {
			bufsize += bufquantum;
			buf = realloc(buf, bufsize);
			if (buf == 0) {
				error("realloc() failed" here);
				exit(1);
			}
		}
		
		nread = read(fd, buf + buflen, bufsize - buflen - 1);
		if (nread < 0) {
			error("read() failed" here);
			exit(1);
		}
		
		buflen += nread;

	} while (nread != 0);
	
	buf[buflen] = 0;
	
	return buf;
}
