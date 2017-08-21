#include <u.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/param.h>	/* for MAXPATHLEN */
#include <sys/un.h>
#include <netinet/in.h>
#include <stdio.h>
#include <dirent.h>
#include <pwd.h>
#include <msg.h>
/* ../include/msg.h */

enum {
	MAXTRIES = 10	/* number of names to try for fifo */
};
static int		findname(char *);
static void	addenv(char*key, char *val);
static char	*myfifo=0;

/* Return file descriptor open for reading connection requests,
 * returns negative value for failure.
 * Sets environment variable WILYFIFO
 */
int
wilyfifolisten(void)
{
	int	fd=-1;
	char	name[MAXPATHLEN];

	if (findname(name))
		return -1;

	if(mkfifo(name, 0600)) {
		fprintf(stderr, "try\n\trm %s\n", name);
		perror(name);
		return -1;
	}

	fd = open(name, O_RDONLY|O_NONBLOCK);

	if (fd<0) {
		perror(name);
		return -1;
	}
	/* dummy fd to hold the read fd open */
	if (open(name, O_WRONLY) < 0) {
		perror(name);
		return -1;
	}

	addenv(WILYFIFO, name);
	myfifo = strdup(name);

	return fd;
}

/* remove the fifo if we can.
 */
void
fifo_cleanup(void)
{
	if(myfifo)
		if (unlink(myfifo))
			perror(myfifo);
}

/* Return file descriptor open for write to wily, or a negative number.
 */
int
wilyfifotalk(void)
{
	char	name[MAXPATHLEN];

	if(findname(name))
		return -1;
	return open(name, O_WRONLY);
}

/* Return file descriptor connected to wily, or <0 on failure.

 * Opens a UNIX-domain socket for listening, connects to wily
 * and sends it the name of the socket, then accepts a connection
 * on our listening socket.
 *
 * Returns -1 for failure.
 *
 * TODO - this is where we could compare version numbers,
 * to make sure both client and server are speaking the same
 * language.
 */
int
client_connect(void)
{
	int s , fd, size;
	struct sockaddr_un addr;
	int	len;
	int	nwritten;
	char	*path;

	/* create a socket */
	if (! (s= socket(AF_UNIX, SOCK_STREAM, 0)))
		return -1;

	/* bind it to a unix-domain at a temporary address */
	addr.sun_family = AF_UNIX;
	tmpnam(addr.sun_path);
	path = strdup(addr.sun_path);
	len = strlen(addr.sun_path);

	if (bind(s, (struct sockaddr *) &addr, sizeof addr) < 0){
		perror("bind");
		return -1;
	}

	listen(s, 1);		/* Get ready  for wily to talk to us */

	fd = wilyfifotalk();		/* fifo to wily */
	if(fd<0)
		return -1;
	nwritten = write(fd, addr.sun_path, len);
	close(fd);
	if(nwritten !=len){
		perror("write to wily");
		return -1;
	}

	size = sizeof(addr);
	fd = accept(s, (struct sockaddr *) &addr, &size);
	close(s);
	if(unlink(path))
		perror(path);
	free(path);
	return fd;
}

/* Given 'addrname' of length 'n', (from some client), connect to it,
 * and return the file descriptor, or -1
 */
int
wily_connect(char*addrname, int n)
{
	int s;
	struct sockaddr_un addr;

	/* create a socket */
	s= socket(AF_UNIX, SOCK_STREAM, 0);
	if (s<0)
		return -1;

	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, addrname, n);
	addr.sun_path[n]='\0';

	if( connect(s, (struct sockaddr*)&addr, sizeof addr))
		return -1;
	return s;
}

/* Find a name to use as a wily fifo.  Use $WILYFIFO if
 * set, otherwise use wily$USER$DISPLAY in $TMPDIR or /tmp,
 *
 * Copy the name into buf.  Return 0 for success.
 */
static int
findname(char *buf)
{
	char	*name = 0;
	struct passwd *pw;
	char	*disp;
	char	*dir;

	if ((name = getenv(WILYFIFO))){
		strcpy(buf, name);
		return 0;
	}

	if(!(pw = getpwuid( getuid() )) ) {
		perror("getpwuid or getuid");
		return -1;
	}
	if(!(disp = getenv("DISPLAY"))) {
		fprintf(stderr, "$DISPLAY not set");
		return -1;
	}
	if(!(dir = getenv("TMPDIR"))) {
		dir = "/tmp";
	}
	sprintf(buf, "%s/wily%s%s", dir, pw->pw_name, disp);
	return 0;
}

static void
addenv(char*key, char *val)
{
	char *buf;

	buf = salloc(strlen(key) + strlen(val) + 2);
	sprintf(buf, "%s=%s", key, val);
	putenv(buf);
}

