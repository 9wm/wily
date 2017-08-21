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
	char 		*path;
	char		*idstr;
	Id		id;
	char		*errstr;
	ushort	selectit;
	Range	rangefound;
	
	if( argc != 3 ) {
		ehandle("usage: Wexec path id");
	}
	
	path = argv[1];
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

	selectit = 1;
	errstr =  rpc_goto (handle, &id, &rangefound, path, selectit);
	if(errstr) {
		ehandle(errstr);
	} else {
		printf("%d %lu, %lu\n", id, rangefound.p0, rangefound.p1);
	}
	return 0;
}
