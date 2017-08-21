#include <u.h>
#include <libc.h>
#include <util.h>
#include <libmsg.h>
#include <stdio.h>
#include <sys/param.h>

void
main(int argc, char **argv)
{
	int	fd;
	char	buf[1024];

	/* connect to wily */
	fd = get_connect();
	if(fd<0) {
		perror("get_connect");
		exit(1);
	}
	/* make it file descriptor 3 */
	sprintf(buf,"WILYFD=%d", fd);
	if(putenv(buf)){
		perror("putenv");
		exit(1);
	}
	
	/* start up the other program */
	argv++;
	execvp(argv[0],argv);
}
