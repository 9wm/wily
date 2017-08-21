#include <u.h>
#include <libc.h>
#include <util.h>
#include <libmsg.h>
#include <stdio.h>
#include <sys/param.h>

int
main(int argc, char **argv)
{
	int	fd;
	char	*fdenv;
	char	*type;
	Mtype	mt;
	int	t;
	Mqueue	q;
	Msg	*m;

	fdenv = getenv("WILYFD");
	if(!fdenv) {
		fprintf(stderr,"$WILYFD must be set\n");
		exit(1);
	}
	fd = atoi(fdenv);

	if(argc<2){
		fprintf(stderr,"usage: %s message-type [args]", argv[0]);
		exit(1);
	}
	
	type = argv[1];
	t = name2type(type);
	if(t<0) {
		fprintf(stderr,"couldn't recognise [%s] as a type of wily message\n", type);
		exit(1);
	}
	mt =t ;
	mq_init(&q, fd);
	fd_send_array(fd, mt, argv+2, argc-2);
	while( (m= mq_next(&q, 0)) ) {
		msg_print(m);
		if(m->mtype == mt || m->mtype == Merror || m->mtype == Mok)
			break;
		msg_free(m);
		free(m);
	}
	return 0;
}
