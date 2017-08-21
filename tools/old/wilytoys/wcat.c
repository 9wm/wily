#include <stdio.h>
#include <unistd.h>
#include <u.h>
#include <libc.h>
#include <util.h>
#include <libg.h>
#include <libmsg.h>

void
e(char *s)
{
	perror(s);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char cwd[BUFSIZ+1], filename[BUFSIZ];
	char *file;
	int isreal;
	Fd fd;
	Mqueue q[1];
	Id id;
	int r;
	ulong len = 0;

	if (argc > 1) {
		file = argv[1];
		isreal = 1;
		if (*file == '/')
			strcpy(filename, file);
		else {
			if (getcwd(cwd,BUFSIZ) == 0)
				e("getcwd");
			sprintf(filename,"%s/%s", cwd, file);
		}
	} else {
		isreal = 0;
		sprintf(filename,"+stdin-%d",getpid());
	}
	if ((fd = get_connect()) < 0)
		e("get_connect");
	mq_init(q,fd);
	if (rpc_new(q, &id, filename, isreal))
		e("rpc_new");
	if (isreal)
		return 0;
	while ((r = fread(cwd, 1, BUFSIZ, stdin))) {
		cwd[r] = 0;
		if (rpc_insert(q, id, len, cwd))
			e("rpc_insert");
		len += r;
	}
	return 0;
}
