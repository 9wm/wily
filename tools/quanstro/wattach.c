#include <unistd.h>
#include <u.h>
#include <libc.h>
#include <libg.h>
#include <msg.h>
#include <signal.h>
#include <sys/limits.h>

volatile Bool control_c = false;

void
catch(int sig){
	signal(sig, catch);
	control_c = true;
}

void exits(const char*whine){
	if (*whine){
		fprintf(stderr, "%s\n", whine);
		exit(1);
	}
	exit(0);
}

const char* 
decode(Msg* m){
	switch(m->t){
	default:
		return "unknown message type";
	case WEexec:
		if (!strcmp(m->s, "Del")){ exit(1);}
		printf("exec %5.5d %s\n", m->w, m->s);
		break;
	case WEgoto:
		printf("goto %5.5d {%lu,%lu} %s\n", m->w, m->r.p0, m->r.p1, m->s);
		break;
	case WEdestroy:
		printf("dest %5.5d \n", m->w);
		break;
	case WEreplace:
		printf("repl %5.5d {%lu,%lu} %s\n", m->w, m->r.p0, m->r.p1, m->s);
		break;
	}
	fflush(stdout);
	return 0;
}

void
main(int c, char**v){
	Msg m;
	int fd;
	Handle *h;
	Id id;
	char* r;
	const char* s;

	c--;
	v++;
	if (c != 1){
		exits("usage: wattach <windowid>");
	}

	id = strtoul(*v, &r, 0);
	if (*r){
		exits("doesn't look integerish");
	}

	signal(SIGHUP, catch);
	signal(SIGINT, catch);
	signal(SIGQUIT, catch);
	signal(SIGTERM, catch);

	fd = client_connect();
	if (-1 == fd){
		exits("can't open wilyfifo");
	}
	h = rpc_init(fd);

	s = rpc_attach(h, id, WEexec | WEgoto | WEreplace | WEdestroy);
	if (s){
		exits(s);
	}

	while (0 == rpc_event(h,&m)){
		decode(&m);
		free(m.s);
		if (control_c){
			break;
		}
	}

/*	rpc_detach(h, id);*/
	rpc_freehandle(h);
	exit(control_c ? 0 : -1);
}
