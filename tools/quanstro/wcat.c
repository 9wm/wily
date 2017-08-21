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
map_file(const char* file, Handle* q, const char* title){
	char* p;
	char filename[PATH_MAX+1];
	char cwd[PATH_MAX+1];
	Id id;

	if (!title){
		if (*file == '/'){
			strcpy(filename, file);
		} else {
			p = getcwd(cwd, PATH_MAX);
			if (!p){
				fprintf(stderr, "can't get cwd()");
				return false;
			}
		}
	} else {
		sprintf(filename, "+%s", title);
	}

	sprintf(filename,"%s/%s", cwd, file);
	if (rpc_new(q, filename, &id)){
		fprintf(stderr, "rpc_new(q, %s, 0, id) fails\n", filename);
		return false;
	}
	fprintf(stderr, "%d\n", id);
	return true;
}

Bool
map_stream(int fd, Handle* q, const char* title){
	char block[BLOCK];
	char winname[50];
	Id id;
	ulong len;
	long r;
	Range R;
	const char* s;

	if (!title){
		sprintf(winname,"+stdin-%d",getpid());
	} else {
		sprintf(winname, "+%s", title);
	}

	if (rpc_new(q, winname, &id)){
		fprintf(stderr, "rpc_new(q, %s, 0, id) fails\n", winname);
		return false;
	}
	fprintf(stderr, "%d\n", id);
	R.p0 = 0;
	R.p1 = 0;
	for (len = 0; (r = read(fd, block, BLOCK))>0; len += r){
		block[r] = 0;
		s = rpc_replace(q, id, R, block);
		if (s){
			fprintf(stderr, "rpc_replace(q,%d,{%lu,%lu}, <mem>) fails with %s\n", id, len, len, s);
			return false;
		}

		s = rpc_goto(q, &id, &R, (char*)":$", false);
		if (s){
			fprintf(stderr, "rpc_goto(q,%d,{%lu,%lu}, ':$') fails with %s\n", id, R.p0, R.p0, s);
			return false;
		}
	}
	return true;
}

void
main(int c, char**v){
	int fd;
	Handle *h;
	Bool taetig;
	Bool status;
	char* title;

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
	title = 0;
	for (v++; *v; v++){
		if (0 == strcmp(*v, "-t")){
			v++;
			if (*v){
				title = *v;
			}
			continue;
		}
		taetig = true;
		if (0 == strcmp(*v, "-")){
			status |= map_stream(0, h, title);
		} else {
			status |= map_file(*v, h, title);
		}
		if (control_c){
			goto out;
		}
	}
	if (!taetig){
		status |= map_stream(0, h, title);
	}
out:
	status |= control_c;
	rpc_freehandle(h);
	if (status){
		exit(0);
	}
	exit(1);
}
