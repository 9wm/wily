/*
 * e.c - set up editing file for wily.
 * some parts of this are swiped straight from gary's win.c.
 */

#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <u.h>
#include <libc.h>
#include <assert.h>
#include "libmsg.h"

int	towily, fromwily;	/* file descriptors */

static char tmpfilename[FILENAME_MAX+1];
static ulong tmpp0, tmpp1;

Msg	m;
Bool	debug = false;
Bool frommessage = true;
static int gottext = 0;

/* The lines that we asked for have arrived.
 */
void
handleread(Msg *m)
{
	char	*s, *t;
	int	len;
	FILE *fp;
	Rune r;

	 tmpnam(tmpfilename);
	tmpp0 = m->p[0];
	tmpp1 = m->p[1];
	s = m->s[0];
	t = s + strlen(s);
	if ((fp = fopen(tmpfilename,"w")) == 0) {
		perror(tmpfilename);
		exit(1);
	}
	while (s < t) {
		if (*(uchar *)s < (ulong)Runeself)
			r = *s++;
		else
			s += chartorune(&r, s);
		if (fwrite(&r, sizeof(r), 1, fp) != 1) {
			perror(tmpfilename);
			remove(tmpfilename);
			exit(1);
		}
	}
	fclose(fp);
	gottext = 1;
	return;
}

/* Handle all the messages coming from wily.
 */
void
domsg(Msg *m)
{
	switch(m->mtype) {
	case 'R':
		handleread(m);
		break;
	default:
		/* ignore whatever we don't specifically handle */
		if(debug) {
			printf("message received and ignored: ");
			msg_print(m);
		}
		break;	
	}
}

/* Open fifos to wily.
 */
void
connect_wily(char *file, char *addr)
{
	char	envbuf[300];

	msg_init(&m);

	towily = fromwily = get_connect(file);

	if(towily<0) {
		perror("connect failed\n");
		exit(1);
	}
	DPRINT("in %d, out %d\n", fromwily, towily);

	wm_read(towily, true, 0, 0, addr);
}

static void
rcv_text(void)
{
	char buf[BUFSIZ];

	int nread;

	while (gottext == 0) {
		nread = read(fromwily,buf,BUFSIZ);

		if (nread <= 0) {
			perror("e read");
			exit(1);
		}
		if (empty_msg_buf(buf, nread, &m, domsg))
			fprintf(stderr,"error from empty_msg_buf\n");
	}
}

int
main(int argc, char**argv)
{
	char	buf[BUFSIZ];
	extern char *optarg;
	extern int optind;
	char *file, *addr;
	int	c;

	while ((c = getopt(argc,	argv, "d")) != EOF){
		switch(c){
		case 'd':
			debug = true;
			break;
		case '?':
			fprintf(stderr, "%s [-d] [prog...]\n", argv[0]);
			exit(1);
		}
	}
	if(debug)
		setbuf(stdout,0);

	file = argv[optind];
	if (!file || !*file) {
		fprintf(stderr,"%s: no file!\n", argv[0]);
		exit(1);
	}
	if (addr = strchr(file, ':'))
		*addr++ = 0;
	else {
		/* XXX - really, we'd like to be able to ask wily what dot is, and
		read that explicitly, because doing a read of "." on file
		/path/name, if it's not already open, causes an eventual
		pane_openpath() which opens /path/. - daft, isn't it? */
		addr = ".";
	}

	connect_wily(file, addr);
	rcv_text();
	printf("%s\n%s\n%s\n0\n%d\n#0,#%d\n", file, addr, tmpfilename,
		tmpp0, tmpp1 - tmpp0);
	return 0;
}
