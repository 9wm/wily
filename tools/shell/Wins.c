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
	char 		*wins;
	char		*errstr;
	
	/* Obtain file descriptor */
	fd = client_connect();
	if (fd<0) {
		ehandle("client_connect");
	}
	
	/* Obtain handle */
	handle = rpc_init(fd);
	assert(handle != 0);
	
	/* Grab the list */
	errstr = rpc_list(handle, &wins);
	if(errstr) {
		ehandle(errstr);
	}

	/* Print it */
	printf("%s\n", wins);
	
	return 0;
}
