/*******************************************
 *	fork, setup environment, exec children
 *******************************************/

#include "wily.h"
#include <ctype.h>
#include <sys/wait.h>
#include <signal.h>

static char *	historyfile;
static char *	shell;

static int		ex_run		(View*, char *);
static int		pipe_views	(char**cmd, View**vout, View**vin);
static int		openpipes	(int*out, int *err, Bool );
static int		ex_parent	(int , int , char*, char*, View *, int );
static void	ex_child		(int , int , char *, char *, View *);
static void	history		(char *cmd);
static void	childenv		(char *,char*);
static void	childfds		(int fderr, int fdout, View *vin);
static void	reap			(void);
static void	signal_init	(void) ;
static void	well_known_init(void) ;

/*
 * Initialize whatever we need
 * for dealing with external processes
 */
void
ex_init(void) {
	keytab_init();
	well_known_init();
	signal_init();

	historyfile = getenv("HISTORY");
	if(!historyfile)
		historyfile = getenv("history");

	shell =  getenv("SHELL");
	if(!shell)
		shell=DEFAULTSHELL;
}

/*
 * Run 'cmd' with arg 'arg' in View 'v'.
 *
 * 'arg' may be null, 'cmd' must be nonnull.
 */
void
run(View *v, char *cmd, char *arg) {
	char	*buf, *buf2;
	char	*a2;

	cmd += strspn(cmd, whitespace);
	if(!*cmd)
		return;

	/*
	 * For builtins, we want to separate cmd and arg,
	 * and if we're passed a pointer to 'arg', we must pass
	 * one on to builtin, even if the arg is just whitespace.
	 */
	if(arg) {
		buf = salloc( strlen(cmd) + strlen(arg) + 2);
		sprintf(buf, "%s %s", cmd, arg);
	} else {
		buf = strdup(cmd);
	}
	buf2 = strdup(buf);	/* before we scribble over buf */

	cmd = strtok(buf, whitespace);
	assert(*cmd && !isspace(*cmd));

	a2 = strtok(0, "");
	if(!a2)
		a2 = arg;

	if(!builtin(v, cmd, a2))
		ex_run(v, buf2);

	free (buf);
	free (buf2);
}

/*********************************************************************
	Static Functions
*********************************************************************/

/* Execute 'cmd', which was selected in 'v'.
 * PRE: the first character of cmd is not 0 or whitespace.
 * Return 0 for success.
 */
static int
ex_run(View*v, char *cmd) {
	View		*vout, *vin;	/* Views for output/input */
	int		pout[2];		/* pipe for stdout */
	int		perr[2];		/* pipe for stderr */
	Path		label;
	int		pid;
	
	if(pipe_views(&cmd, &vout, &vin))
		return -1;
	
	if(openpipes(pout, perr, (Bool)vout))
		return -1;

	data_getlabel(view_data(v), label);
	
	switch(pid=fork()) {
	case -1:	/* fork failed */
		close(perr[0]); close(perr[1]);
		close(pout[0]); close(pout[1]);
		return -1;
	default:	/* parent */
		close(perr[1]);
		close(pout[1]);
		return ex_parent(perr[0], pout[0], label, cmd, vout, pid);
	case 0:	/* child */
		close(perr[0]);
		close(pout[0]);
		ex_child(perr[1], pout[1], label, cmd, vin);
		
		/* ex_child doesn't return */
		assert(false);
		exit(1);
		return -1;
	}
}

/*
 * Update 'cmd', 'vout' and 'vin' 
 * appropriately depending on whether 'cmd' is a piping command,
 * i.e. starts with | < or >.
 * Return 0 for success.
 */
static int
pipe_views(char**cmd, View**vout, View**vin) {
	char	op;
	View*vpipe;
	
	op = **cmd;
	*vout = 0;
	*vin = 0;
	if(!( op == '|' || op == '<' || op == '>')){
		return 0;
	}
	
	/* Pipe operations are on the body of the last selection */
	if(!(vpipe = view_body(last_selection)))
		return -1;

	/* Make 'cmd' now point to the command, after '|' and
	 * optional whitespace.
	 */
	*cmd += 1 + strspn((*cmd)+1, whitespace);

	/* don't execute an empty command */
	if (!strlen(*cmd))
		return -1;

	if (op == '|' || op == '<')
		*vout = vpipe;
	if (op == '|' || op == '>')
		 *vin= vpipe;
	return 0;
}

/* Open pipes for stdout and stderr.
 * If and only if this is a '|' or '>' operation, stdout
 * will be distinct from stderr.
 * 
 * Return 0 for success.
 *
 * If we don't succeed, make sure
 * we don't leave any open file descriptors.
 */
static int
openpipes(int*out, int *err, Bool is_pipe_operation){
	if (pipe(err) < 0) {
		diag(0, "pipe");
		return -1;
	}

	if (is_pipe_operation) {
		if (pipe(out) < 0) {
			diag(0, "pipe");
			close(err[0]);
			close(err[1]);
			return -1;
		}
	} else {
		out[0] = err[0];
		out[1] = err[1];
	}
	return 0;
}
	
/*
 * Parent-side accounting for a recently started external process.
 *
 * 'fderr', 'fdout' are file descriptors for stdout and stderr
 * 'label' is the label of the window we started in.
 * 'cmd' is the command being executed
 * 'vout', if non-null, is the View to send stdout to.
 * 'pid' is the process id of the child process.
 *
 * Return 0 for success.
 */
static int
ex_parent(int fderr, int fdout, char*label, char*cmd, View *vout, int pid) {
	/* fderr == fdout if and only if vout == 0 */
	assert( (vout==0) == (fderr == fdout));

	reap();
	event_outputstart(fderr, pid, cmd, label,0);
	if(vout)
		event_outputstart(fdout, pid, cmd, label, vout);
	return 0;
}

/*
 * Child-side accounting for a recently started external process.
 *
 * 'fderr', 'fdout' are file descriptors for stdout and stderr
 * 'label' is the label of the window we started in.
 * 'cmd' is the command being executed
 * 'vin', if non-null, is the View to get stdin from.
 *
 * Does not return.
 * Don't use normal diag stuff -- we're a separate process.
 * After we've set up stderr, just fprintf(stderr,...)
 */
static void
ex_child(int fderr, int fdout, char *label, char *cmd, View *vin) {
	Path	dir;
	Path	path;
	
	childfds(fderr, fdout, vin);
	
	/* become process group leader */
	if(setsid()<0)
		perror("setsid");

	label2path(path, label);
	strcpy(dir,path);
	dirnametrunc(dir);
	
	/* Executing the command without being in the right directory
	 * would be _bad_.
	 */
	if(chdir(dir)){
		Path	buf;
		
		sprintf(buf, "chdir(%s)", dir);
		perror(buf);
		exit(1);
	}

	childenv(label, path);
	history(cmd);
	execl(shell, shell, "-c", cmd, 0);
	perror(shell);
	exit(1);
}

/* Record 'cmd' in the history file */
static void
history(char *cmd) {
	FILE *fp;

	if(!historyfile)
		return;

	fp = fopen(historyfile, "a");
	fprintf(fp,"%s\n", cmd);
	fclose(fp);
}

/* Add some stuff to the environment of our child */
static void
childenv(char *label,char*path) {
	Path	buf;

	sprintf(buf, "WILYLABEL=%s", label);
	(void)putenv(strdup(buf));
	sprintf(buf, "WILYPATH=%s", path);
	(void)putenv(strdup(buf));
	sprintf(buf, "w=%s", path);
	(void)putenv(strdup(buf));
}

/*
 * Set up default file descriptors for a child process.
 *
 * Replace 'stderr' and 'stdout' with 'fderr' and 'fdout'.
 *
 * If 'vin' is not null, replace 'stdin' with a file descriptor 
 * that will give the contents of the selection in 'vin',
 * otherwise, 'stdin' is set to '/dev/null'.
 */
static void
childfds(int fderr, int fdout, View *vin) {
	int	j;
	int	fdin;

	/* redirect fdout and fderr */
	if (dup2(fderr, 2) < 0 || dup2(fdout, 1) < 0) {
		perror("dup2");
		exit(1);
	}

	if (vin) {
		/* fd open to read current selection */
		fdin = text_fd(view_text(vin), view_getsel(vin));		
		if(fdin<0)
			exit(1);
	} else if ((fdin = open("/dev/null", O_RDONLY, 0)) < 0) {
		perror("open /dev/null");
		fdin = 0;
	}

	if (fdin) {
		if (dup2(fdin, 0) < 0) {
			perror("input dup2");
			fdin = 0;
		}
	}
	if (!fdin) {
		/* no need to panic... */
		if (close(0) < 0) {
			/* OK, panic. */
			perror("close fdin");
			exit(1);
		}
	}

	/* Don't inherit any other open fds */
	for(j=3; j<FOPEN_MAX; j++)
		close(j);
}

/* Collect any waiting processes, but don't block */
static void
reap(void) {
	int	stat_loc;

	while ( waitpid(-1, &stat_loc, WNOHANG)> 0 )
		;
}

/* Prepare to catch some signals */
static void
signal_init(void) {
	/* in case external process exits */
	signal(SIGPIPE, SIG_IGN);       

	signal(SIGHUP, cleanup_and_die); 
	signal(SIGINT, cleanup_and_die); 
	signal(SIGTERM, cleanup_and_die); 
	
	signal(SIGSEGV, cleanup_and_abort); 
	/* signal(SIGSYS, cleanup_and_abort);  */
	signal(SIGILL, cleanup_and_abort); 
}

/* Start listening to fifo in well-known location */
static void
well_known_init(void) {
	int	fd;

	if((fd = wilyfifolisten())<0) {
		diag(0, "couldn't open fifo");
	} else {
		event_wellknown(fd);
	}
}
