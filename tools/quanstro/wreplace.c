#include <unistd.h>
#include <u.h>
#include <libc.h>
#include <libg.h>
#include <msg.h>
#include <signal.h>
#include <sys/limits.h>

volatile Bool control_c = false;

enum {
	BLOCK = 8 * 1024
};

void
e(const char *s)
{
	perror(s);
	exit(1);
}

void
catch(int sig){
	signal(sig, catch);
	control_c = true;
}

Bool
map_stream(int fd, Handle* h, long id, const char* range){
	char block[BLOCK];
	ulong len;
	long r;
	const char*s;
	Range R;

	s=rpc_goto(h, (Id*)&id, &R, (char*)range, true);
	if (s){
		fprintf(stderr, "rpc_goto -> %s\n", s);
		return false;
	}

	fprintf(stderr, "%ld\n", id);
	for (len = 0; (r = read(fd, block, BLOCK))>0; len += r){
		block[r] = 0;
		if (rpc_replace(h, id, R, block)){
			fprintf(stderr, "rpc_insert(q,%ld,%ld,%ld, <mem>) fails\n", id, len, len);
			return 0;
		}
		R.p0 = R.p1 = len;
	}
	return 0;
}

long getLong(const char* s){
	char* r;
	ulong l;

	l = strtoul(s,&r,0);
	if (*r){
		e("want long integer");
	}
	return l;
}

void
main(int c, char**v){
	int fd;
	int f;
	Handle* h;
	Bool taetig;
	Bool status;
	long id;
	const char* range;

	signal(SIGHUP, catch);
	signal(SIGINT, catch);
	signal(SIGQUIT, catch);
	signal(SIGTERM, catch);

	fd = client_connect();
	if (-1 == fd){
		e("can't open wilyfifo");
	}
	h = rpc_init(fd);

	status = true;
	taetig = false;
	for (v++; *v; v++){
		id = -1;
		range = ":,";

		if (0 == strcmp(*v, "-i")){
			v++;
			if (*v){
				id = getLong(*v++);
			}
		}
		if (0 == strcmp(*v, "-r")){
			v++;
			if (*v){
				range = *v++;
			}
		}
		if (-1 == id){
			e("missing id");
		}
		if (!*v){
			break;
		}
		taetig = true;
		if (0 == strcmp(*v, "-")){
			status |= map_stream(0, h, id, range);
		} else {
			f = open(*v, 0, 0666);
			if (-1 == f){
				fprintf(stderr, "can't open file %s\n", *v);
			} else {
				status |= map_stream(f, h, id, range);
				close(f);
			}
		}
		if (control_c){
			goto out;
		}
	}
	if (!taetig){
		if (-1 == id){
			e("missing id");
		}
		status |= map_stream(0, h, id, range);
	}
out:
	status |= control_c;
	rpc_freehandle(h);
	if (status){
		exit(0);
	}
	exit(1);
}

