/*********************************************************
	Set up and handle interactions with all the libXg
	events that aren't mouse,timer or keyboard.
*********************************************************/

#include "wily.h"
#include <signal.h>

typedef struct Key Key;

/* Different uses for a libXg event key */
typedef enum Keytype {
	Kfree, 	/* not in use */
	Klisten, 	/* connection requests */
	Kout, 	/* output to stderr or a window */
	Kmsg	/* from external connections */
} Keytype; 

/* Info about a libXg event key */
struct Key {
	Keytype 	t;
	int		fd;		/* file descriptor*/
	ulong	key;		/* libXg event key */
	
	/* 
	 * Buffer for incomplete messages.
	 * Only in use if k->t == Kmsg.
	 */
	Mbuf	buf;		

	/*
	 * Info about the program sending us output.
	 * Only valid if k->t == Kout.
	 */
	int	pid;
	Path	cmd;	/* shortened name of executing command */
	Path	olabel;	/* where output should go */

	/* If output should replace some View's selection ( |, > ).
	 * Note that when we use |cmd or >cmd, we'll get TWO
	 * event Keys, one for stderr (with k->v == 0), and
	 * one for stdout, (with k->v set).
	 */
	View		*v;
	Bool		first;
};

/*
 * Because libXg can only keep track of a small number of
 * event keys, so do we.  We keep the Keys in a static array.
 * The Key at position 'n' in keytab has libXg event key 2^n.
 *
 * This is a bit wasteful, and a bit inefficient.  Some event keys
 * will never be used here.  Too bad.
 */
enum {
	MAXKEYS =FOPEN_MAX
};
static Key		keytab[MAXKEYS];

static Key*	key_find(ulong key);
static void	outmsg(Key*k, int n, char*s);
static void	key_del(Key *k);
static Key*	key_new(ulong key, int fd, Keytype t);
static Key*	key_findcmd(char*cmd);
static void	oneword(char*s, char*l);
static void	output(Key*k, int n, char*s);
static void	ex_accept(int n, char*s);

/* Initialise keytab */
void
keytab_init(void) {
	Key	*k;
	ulong	key = 1;

	for(k = keytab; k < keytab + MAXKEYS; k++) {
		k->t = Kfree;
		k->key = key;
		key *= 2;
		mbuf_init(&k->buf);
	}
}

/*
 * Start listening for output on 'fd'.
 * The output is from the (long) command 'cmd', process 'pid'.
 * The context is associated with 'label'.
 * If 'v' is set, we are piping the output to this View.
 */
int
event_outputstart(int fd, int pid, char*cmd, char*label, View *v){
	ulong key;
	Key	*k;
	
	if (!(key = estart(0, fd, 0))) {
		diag(0, "estart");
		return -1;
	}
	k = key_new(key, fd, Kout);
	k->pid = pid;
	k->v = v;

	/* Every 'pipe' key has another 'stderr' key, we only
	 * display the 'cmd' associated with the 'stderr' key.
	 */
	if(k->v) {
		k->first = true;
	} else {
		oneword(k->cmd, cmd);
		olabel(k->olabel, label);
		addrunning(k->cmd);
	}
	return 0;
}

/* Start listening for connection attempts on 'fd' */
void
event_wellknown(int fd){
	ulong key;
	
	if(!(key = estart(0, fd, 0)))
		error("estart");
	key_new(key,fd, Klisten);
}

/* 
 * Handle 'n' bytes of data in 's' which arrived from 
 * libXg event key 'key'.
 */
void
dofd(ulong key, int n, char*s) {
	Key	*k = key_find(key);

	if(!n) {
		key_del(k);
		return;
	}

	switch(k->t){
	case Klisten:	ex_accept(n,s);	break;
	case Kout:	output(k, n, s);	break;
	case Kmsg:	outmsg(k, n, s);	break;
	default:		error("bad key type %d", k->t);		break;
	}
}

/*
 * Kill all the external processes matching
 * the arguments.
 */
void
kill_all(char*s) {
	Key	*k = 0;
	const char *sep = " \t\n";

	for (s = strtok(s, sep); s; s = strtok(NULL, sep)) {
		if ((k = key_findcmd(s))) {
			/* only output keys have a prog attached */
			assert(k->t == Kout); 
			if(kill(- k->pid, SIGKILL))
				diag(0, "kill %s", s);
		}
	}
}

/* Output a list of possible 'Kill' commands. */
void
kill_list(void){
	Key	*k;
	
	for(k = keytab; k < keytab + MAXKEYS; k++)
		if(k->t == Kout && !k->v)
			diag(0, "Kill %s", k->cmd);
}

/******************************************************
	static functions
******************************************************/
static void
outmsg(Key*k, int n, char*s) {
	if(partialmsg(&k->buf, k->fd, n, s)){
		diag(0, "Received bad message, closing connection");
		key_del(k);
	}
}

/* Free any resources that 'k' was tying up. */
static void
key_del(Key *k) {
	close(k->fd);
	estop(k->key);

	switch(k->t) {
	case Kout: 	if(!k->v)rmrunning(k->cmd); break;
	case Kmsg:	data_fdstop(k->fd); break;
	case Klisten:	error("Klisten closed");
	default:		error("bad key type %d", k->t);
	}
	k->t = Kfree;
}

/* Allocate and return a new Key with the given key, fd and type */
static Key*
key_new(ulong key, int fd, Keytype t) {
	Key	*k;

	k = key_find(key);
	assert(k->t == Kfree);
	k->fd = fd;
	k->t = t;
	return k;
}

/* Find the output Key with the given 'cmd' */
static Key*
key_findcmd(char*cmd)
{
	Key	*k;

	for(k = keytab; k < keytab + MAXKEYS; k++)
		if (k->t == Kout && STRSAME(cmd, k->cmd))
			return k;
	return 0;
}

/* Put a one-word version of long command 'l' into 's' */
static void
oneword(char*s, char*l){
	strcpy(s,l);
	
	/* break at first whitespace */
	if( (s = strpbrk(s, whitespace)) )
		*s='\0';
}

/* Copy the 'n' bytes of data in 's' to the window or output directory
 * corresponding to 'k'
 */
static void
output(Key*k, int n, char*s)  {
	if(k->v)
		view_pipe(k->v, &k->first, s, n);
	else
		noutput(k->olabel, s, n);
}

/* Accept the connection we're told about with 's' and 'n' */
static void
ex_accept(int n, char*s) {
	int	fd;
	ulong	key;
	Key		*k;

	if((fd =wily_connect(s,n))<0) {
		diag(0, "failed connection attempt");
		return;
	}
	
	if (!(key= estart(0,fd,0))){
		diag(0, "couldn't estart to accept connection");
		close(fd);
		return;
	}
	k = key_new(key, fd, Kmsg);
	mbuf_clear(&k->buf);
}

/*
 * Find the Key info given the libXg event key.
 * 
 * ASSUMPTION: integers at least 32 bit.
 */
static Key*
key_find(ulong key) {
	Key	*k;

	for(k = keytab; k < keytab + MAXKEYS; k++)
		if(k->key == key)
			return k;
	
	/* We assume key_find will always work */
	assert(false);	
	return 0;
}

