#include <u.h>
#include <libc.h>
#include <msg.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <signal.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#if HAVE_SYS_PTEM_H
#include <sys/stropts.h>
#endif

int shellpid;

int master;
int slave;
char *slavename;
const int ttymode = 0600;
#define ctrl(c) (c & 0x1f)
const char intchar = ctrl('c');
const char eofchar = ctrl('d');

int	wilyfifo;
Handle *handle;
Id id;

Bool shelloutput = false;
ulong length;
ulong outputpoint;

void error(const char *, ...);
char *getshell(void);
char *flatlist(char **);
void execshell(char **);
void sigexit(int);
void sigchld(int);
void forkshell(char **);
void ipcinit(char *);
void ipcloop(void);
ulong getlength(void);
char *gettagname(char *);
void handleshelloutput(void);
Bool isbuiltin(char *);
void handleWEexec(Msg *);
void handleshellinput(Msg *);
void handleWEreplace(Msg *);
void handlemsg(void);
void openmaster(void);
void openslave(void);
void setslaveattr(void);

#define stringize2(x) #x
#define stringize(x) stringize2(x)
#if defined(__GNUC__)
	#define here " at " __FILE__ ":" stringize(__LINE__) " in " __FUNCTION__ "()"
#else
	#define here " at " __FILE__ ":" stringize(__LINE__)
#endif

#define PROGRAMNAME		"win"
#define VERSIONNAME		"%R%.%L% of %D%"

char programname[]	= PROGRAMNAME;
char versioninfo[]	= PROGRAMNAME " " VERSIONNAME;
char usageinfo[]	= "usage: " PROGRAMNAME " [-v] [-t tagname] [command [argument ...]]";

char whatinfo[]		= "@(#)" PROGRAMNAME " " VERSIONNAME;

void
main(int argc, char **argv)
{
	int c;
	extern char *optarg;
	extern int optind;
	extern int opterr;
	char *tagname;

	/* parse arguments */
	tagname = 0;
	opterr = 0;
	while ((c = getopt(argc, argv, "vt:")) != -1) {
		switch (c) {
		case 'v':
			fprintf(stderr, "%s\n", versioninfo);
			exit(0);
		case 't':
			tagname = optarg;
			break;
		default:
			fprintf(stderr, "%s\n", usageinfo);
			exit(1);
			break;
		}
	}

	/* determine tag name */
	if (tagname == 0)
		if (argv[optind] != 0)
			tagname = gettagname(argv[optind]);
		else
			tagname = "+win";
	
	/* open pty and fork shell */
	forkshell(&argv[optind]);
	
	/* handle ipc */
	ipcinit(tagname);
	outputpoint = length = getlength();
	ipcloop();

}

/*
 * error(fmt, ...):
 *
 * Print "win: ", message, and a newline to stderr.
 */
void
error(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "win: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

/*
 * getshell():
 *
 * Return the name of the shell.
 *
 * Return value is a pointer to malloc()-ed storage.
 */
char *
getshell(void)
{
	char *path, *shell;
	
	path = getenv("SHELL");
	if (path == 0)
		path = "/bin/sh";

	shell = malloc(strlen(path) + 1);
	if (shell == 0) {
		error("malloc() failed" here);
		exit(1);
	}
	
	strcpy(shell, path);
	
	return shell;
}

/*
 * flatlist(v):
 *
 * Return a space-separated flattened representation of the 
 * string list v.
 *
 * Return value is a pointer to malloc()-ed storage.
 */
char *
flatlist(char **v)
{
	char *s;
	size_t len;
	size_t i;

	len = strlen(v[0]);
	for (i = 1; v[i] != 0; ++i) {
		++len;
		len += strlen(v[i]);
	}
			
	s = malloc(len + 1);
	if (s == 0) {
		error("malloc() failed" here);
		exit(1);
	}
		
	s[0] = 0;
	strcat(s, v[0]);
	for (i = 1; v[i] != 0; ++i) {
		strcat(s, " ");
		strcat(s, v[i]);
	}
	
	return s;
}

/*
 * execshell(argv):
 *
 * Exec a shell to execute the command in argv.
 * The shell should understand -i and -c.
 */
void
execshell(char **argv)
{
	char *arg0, *arg1, *arg2;
	
	arg0 = getshell();
	if (*argv == 0) {
		arg1 = "-i";
		arg2 = 0;
	} else {
		arg1 = "-c";
		arg2 = flatlist(argv);
	}
			
	execl(arg0, arg0, arg1, arg2, 0);
	error("execl(%s, ...) failed" here, arg0);
	exit(1);
}

/*
 * sigexit(sig)
 *
 * Signal handler that simply exits.
 */
void
sigexit(int sig)
{
	exit(0);
}

/*
 * sigchld(sig)
 *
 * Signal handler that exits if the shell has exited.
 */
void
sigchld(int sig)
{
	signal(sig, sigchld);
	if (wait(0) == shellpid)
		exit(0);
}

/*
 * forkshell(argv):
 *
 * Open the master pty; fork.
 * In the master: set signal handlers.
 * In the child: set signal handlers; close the master pty;
 * start a new process group; open the slave pty; make the slave
 * the controlling terminal (if open() did not already); set the 
 * attributes of the slave; put "TERM=win" into the environment; 
 * exec the shell.
 */
void
forkshell(char **argv)
{
	openmaster();
	
	switch (shellpid = fork()) {
	
	case -1:
	
		error("fork() failed" here);
		exit(1);
		
	case 0:
	
		signal(SIGCHLD, SIG_DFL);
		signal(SIGINT,  SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTSTP, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTTOU, SIG_IGN);
		
		close(master);
		
		if (setsid() < 0) {
			error("setsid() failed" here);
			exit(1);
		}
		
		openslave();
		setslaveattr();
	
		#if defined(TIOCSCTTY)
		/* e.g., on Digital UNIX */
		ioctl(slave, TIOCSCTTY, (void *) 0);
		#endif
		
		if(dup2(slave, 0) < 0 || dup2(slave, 1) < 0 || dup2(slave, 2) < 0) {
			error("dup2() failed" here);
			exit(1);
		}
		close(slave);

		putenv("TERM=win");

		execshell(argv);

	default:
	
		signal(SIGHUP,  sigexit);
		signal(SIGINT,  sigexit);
		signal(SIGQUIT, sigexit);
		signal(SIGTERM, sigexit);
		signal(SIGCHLD, sigchld);
		
	}
}

/*
 * ipcinit(tagname):
 *
 * Initialize IPC to wily.
 */
void
ipcinit(char *tagname)
{
	char *msg;
	
	wilyfifo = client_connect();
	if (wilyfifo < 0) {
		error("client_connect() failed" here);
		exit(1);
	}
	
	handle = rpc_init(wilyfifo);
	
	msg = rpc_new(handle, tagname, &id, 0);
	if (msg != 0) {
		error("rpc_new() failed" here ": %s", msg);
		exit(1);
	}
	
	msg = rpc_attach(handle, id, WEexec|WEreplace|WEdestroy);
	if (msg != 0) {
		error("rpc_attach() failed" here ": %s", msg);
		exit(1);
	}
	
}

/*
 * ipcloop():
 *
 * Handle IPC with wily.
 */
void
ipcloop(void)
{
	fd_set readfds;
	int nready;

	for (;;) {

		while (!rpc_wouldblock(handle))
			handlemsg();
			
		FD_ZERO(&readfds);
		FD_SET(wilyfifo, &readfds);
		FD_SET(master, &readfds);
		nready = select(FD_SETSIZE, &readfds, 0, 0, 0);
		if (nready < 0) {
			error("select() failed" here);
			exit(1);
		}
		if (FD_ISSET(master, &readfds))
			handleshelloutput();
		else if (FD_ISSET(wilyfifo, &readfds))
			handlemsg();
		else {
			error("select() returned in error" here);
			exit(1);
		}
			
	}
}

/*
 * getlength():
 *
 * Return the length of the body being handled in Runes.
 */
ulong
getlength(void)
{
	Range r;
	char *msg;
	
	msg = rpc_goto(handle, &id, &r, strdup(":,"), 0);
	if (msg != 0) {
		error("rpc_goto() failed " here ": %s", msg);
		exit(1);
	}
	return r.p1;
}

/*
 * gettagname(shorttagname):
 *
 * Return the tag name to use.
 *
 * Return value is a pointer to malloc()-ed storage.
 */
char *
gettagname(char *shorttagname)
{
	char *tagname;
	
	tagname = malloc(strlen(shorttagname) + 2);
	if (tagname == 0) {
		error("malloc() failed" here);
		exit(1);
	}
	
	sprintf(tagname, "+%s", shorttagname);

	return tagname;
}

/*
 * handleshelloutput():
 *
 * Read output from the shell and pass it to wily.
 */
void
handleshelloutput(void)
{
	char buf[BUFSIZ + 1];
	int nread;
	char *msg;
	
	nread = read(master, buf, BUFSIZ);
	if (nread < 0) {
		error("read() failed in shellinput()" here);
		exit(1);
	}
	
	if (nread == 0)
		exit(0);
		
	buf[nread] = '\0';
	msg = rpc_replace(handle, id, range(outputpoint, outputpoint), buf);
	if (msg != 0) {
		error("rpc_replace() failed" here ": %s", msg);
		exit(1);
	}
		
	shelloutput = true;
}

/*
 * isbuiltin(s):
 *
 * Check to see if a command is a builtin command.
 *
 * Return true if it is, otherwise false.
 */
Bool
isbuiltin(char *cmd)
{
	char *whitespace = " \t\v\n";
	char *first;
	
	first = cmd + strspn(cmd, whitespace);
	
	return
		*first == '|' ||
		*first == '<' ||
		*first == '>' ||
		isupper(*first);
}

/*
 * handleexec(m):
 *
 * Handle a WEexec message m.
 */
void
handleWEexec(Msg *m)
{
	size_t nbytes;
	int addnewline;
	char *cmd;
	char *msg;
	
	if (isbuiltin(m->s)) {
		rpc_bounce(handle, m);
		return;
	}
	
	nbytes = strlen(m->s);
	if (nbytes == 0)
		return;
		
	addnewline = (m->s[nbytes - 1] != '\n');
	cmd = malloc(nbytes + addnewline + 1);
	if (cmd == 0) {
		error("malloc() failed" here);
		exit(1);
	}
	strcpy(cmd, m->s);
	if (addnewline)
		strcat(cmd, "\n");
			
	msg = rpc_replace(handle, id, range(length, length), cmd);
	if (msg != 0) {
		error("rpc_replace() failed " here ": %s", msg);
		exit(1);
	}
		
	free(cmd);
}

/*
 * handleshellinput(m):
 *
 * Send everything from the output point to the last newline 
 * or ctrl-d in the message m to the shell, and adjust the 
 * output point.
 */
void
handleshellinput(Msg *m)
{
	char *lastnewline, *lastintchar, *lasteofchar, *last;
	ulong lastpoint;
	char *buf;
	size_t len;

	lastnewline = strrchr(m->s, '\n');
	lastintchar = strrchr(m->s, intchar);
	lasteofchar = strrchr(m->s, eofchar);

	last = lastnewline;
	if (lastintchar != 0 && (last == 0 || last < lastintchar))
		last = lastintchar;
	if (lasteofchar != 0 && (last == 0 || last < lasteofchar))
		last = lasteofchar;
		
	*last = '\0';
	lastpoint = m->r.p0 + utflen(m->s) + 1;

	buf = malloc(UTFmax * (lastpoint - outputpoint + 1));
	if (buf == 0) {
		error("malloc() failed" here);
		exit(1);
	}
		
	rpc_read(handle, id, range(outputpoint, lastpoint), buf);
	
	len = strlen(buf);
	if (write(master, buf, len) != len) {
		error("write() failed" here);
		exit(1);
	}
	
	free(buf);
	
	outputpoint = lastpoint;

}
		
/*
 * handleWEreplace(m):
 *
 * Handle a WEreplace message m.
 */
void
handleWEreplace(Msg *m)
{
	length = length - RLEN(m->r) + utflen(m->s);

	if (shelloutput) {
	
		shelloutput = false;
		
		outputpoint = m->r.p0 - RLEN(m->r) + utflen(m->s);
		
		#if 0
		{
			/*
             * This doesn't work. It's an idea for implementing scrolling
             * on output. What we need is msg.c to execute view_show()
             * but not view_select() or view_warp(). However, making that
             * modification would case view_show() to happen even if you
             * are not setting dot. What we need to do is separate the
             * functions show, select, and warp.
			 */
			char *msg;
			char buf[100];
			Range r;
			sprintf(buf, ":#%lu", outputpoint);
			msg = rpc_goto(handle, &id, &r, strdup(buf), 0);
			if (msg != 0) {
				error("rpc_goto() failed " here ": %s", msg);
				exit(1);
			}
		}
		#endif
	
	} else {
	
		if (outputpoint > m->r.p0)
			outputpoint = outputpoint - RLEN(m->r) + utflen(m->s);

		if (m->r.p0 + utflen(m->s) > outputpoint && 
			(strchr(m->s, '\n') || strchr(m->s, intchar) || strchr(m->s, eofchar)))
			handleshellinput(m);
	}

}
		
/*
 * handlemsg():
 *
 * Handle a message from wily.
 */
void
handlemsg(void)
{
	Msg	m;

	if (rpc_event(handle, &m) != 0) {
		error("rpc_event() failed" here);
		exit(1);
	}

	switch (m.t) {
	case WEdestroy:
		exit(0);
	case WEexec:
		handleWEexec(&m);
		break;
	case WEreplace:
		handleWEreplace(&m);
		break;
	default:
		error("unexpected message type" here);
	}
}

/*
 * openmaster():
 *
 * Open the master end of a pty; determine the name of 
 * the slave end but do not open it.
 */
void
openmaster(void)
{
	#if HAVE__GETPTY

	/* e.g., on IRIX */
	
	{
		extern char *_getpty();

		slavename = _getpty(&master, O_RDWR, ttymode, 0);
		if (slavename == 0) {
			error("_getpty() failed" here);
			exit(1);
		}
	}

	#elif HAVE_DEV_PTMX
	
	/* e.g., on Digital UNIX or Solaris */

	{
		extern char *ptsname();
		extern int grantpt();
		extern int unlockpt();
		
		master = open("/dev/ptmx", O_RDWR);
		if (master < 0) {
			error("open() failed" here);
			exit(1);
		}
		if (grantpt(master) < 0) {
			error("grantpt() failed" here);
			exit(1);
		}
		if (unlockpt(master) < 0) {
			error("unlockpt() failed" here);
			exit(1);
		}
		
		fchmod(master, ttymode);

		slavename = ptsname(master);
		if (slavename == 0) {
			error("ptsname() failed" here);
			exit(1);
		}				

	}

	#else
	
	/* e.g., on BSD */

	{
		static char name[]= "/dev/ptyXX";
		char *c1, *c2;

		master = -1;
		c1 = "pqrstuvwxyzABCDE";
		do {
			c2 = "0123456789abcdef";
			do {
				sprintf(name, "/dev/pty%c%c", *c1, *c2);
				master = open(name, O_RDWR);
			} while (master < 0 && *++c2 != 0);
		} while (master < 0 && *++c1 != 0);
		if (master < 0) {
			error("unable to open master pty" here);
			exit(1);
		}
		
		fchmod(master, ttymode);
		
		sprintf(name, "/dev/tty%c%c", *c1, *c2);
		slavename = name;
	}

	#endif
}

/*
 * openslave():
 *
 * Open the slave pty.
 */
void
openslave(void)
{	
	slave = open(slavename, O_RDWR);
	if (slave < 0) {
		error("open() failed" here);
		exit(1);
	}
	
	fchmod(slave, ttymode);
		
	#if HAVE_SYS_PTEM_H
	/* e.g., on Solaris */
	ioctl(slave, I_PUSH, "ptem");
	ioctl(slave, I_PUSH, "ldterm");
	#endif
}

/*
 * setslaveattr()
 *
 * Set the slave attributes.
 */
void
setslaveattr(void)
{
	struct termios attr;
	int i;
	
	#if defined(TIOCSETD) && defined(TERMIODISC)
	/* e.g., on Ultrix */
	{
		/* use termios line discipline */
		int ldisc = TERMIODISC;
		ioctl(slave, TIOCSETD, (void *) &ldisc);
	}
	#endif

	if (tcgetattr(slave, &attr) < 0) {
		error("tcgetattr() failed" here);
		exit(1);
	}

	attr.c_iflag = 0;
	attr.c_oflag = ~OPOST;
	attr.c_lflag = ISIG | ICANON;

	for (i = 0; i < sizeof(attr.c_cc) / sizeof(*attr.c_cc); ++i)
		attr.c_cc[i] = 0;
	attr.c_cc[VINTR] = intchar;
	attr.c_cc[VEOF]  = eofchar;

	if (tcsetattr(slave, TCSANOW, &attr) < 0) {
		error("tcsetattr() failed" here);
		exit(1);
	}
}
