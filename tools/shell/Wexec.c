#include <u.h>
#include <libc.h>
#include <msg.h>
#include <assert.h>
#include <stdio.h>

static void
ehandle(char*msg) {
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

int
main(int argc, char**argv) {
	int		fd;
	Handle	*handle;
	char 		*cmd;
	char		*idstr;
	Id		id;
	char		*errstr;
	
	if( argc != 3 ) {
		ehandle("usage: Wexec cmd id");
	}
	
	cmd = argv[1];
	idstr = argv[2];
	id = atoi(idstr);
		
	/* Obtain file descriptor */
	fd = client_connect();
	if (fd<0) {
		ehandle("client_connect");
	}
	
	/* Obtain handle */
	handle = rpc_init(fd);
	assert(handle != 0);
	
	/* send the command */

	errstr =  rpc_exec (handle, id, cmd);
	if(errstr) {
		ehandle(errstr);
	}

	return 0;
}
