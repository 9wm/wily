#include "range.h"

int	towily, fromwily;	/* file descriptors */




Msg	m;
Bool	debug = false;
Bool frommessage = true;



/*
 * load the relevant section of the file, convert it to UTF, and supply
 * the length of the resulting UTF string.
 */

static char *
load_data(ulong p0, ulong p1, int *len)
{
	ulong length = p1 - p0;
	size_t sz;
	static FILE *fp;
	Rune r;
	char *s;
	int n;

	/* XXX We make a guess about how much space the UTF version
	  of the text will require... */

	sz = length * (sizeof(Rune)+1);
	s = utffile = srealloc(utffile, sz);
	
	if (!fp && (fp = fopen(tmpfilename,"r")) == 0) {
		perror(tmpfilename);
		exit(1);
	}
	fseek(fp, p0 * sizeof(Rune), SEEK_SET);
	for (n = 0; n < length; n++) {
		fread(&r, sizeof(Rune), 1, fp);
		if ((uchar)r < Runeself)
			*s++ = r;
		else
			s += runetochar(s, &r);
	}
	*s = 0;
	*len = s - utffile;
	DPRINT("Read %d bytes\n",*len);
	return utffile;
}

static void
send_data(void)
{
	Subrange *r;
	char *buf;
	int len;

	if (!modified) {
		DPRINT("Not modified - not making changes\n");
		return;
	}
	while (r = next_range()) {
		r->p0 += base;
		r->p1 += base;
		buf = load_data(r->q0, r->q1, &len);
		DPRINT("Removing old data\n");
		wm_delete(towily, true, r->p0, r->p1);
		DPRINT("Inserting new data\n");
		wm_insert(towily, true, r->p0, len, buf);
	}
	return;
}


/* Open fifos to wily.
 */
void
connect_wily(char *file, char *addr)
{
	char	envbuf[300];

	msg_init(&m);

	DPRINT("Connecting to wily\n");
	towily = fromwily = get_connect(file);

	if(towily<0) {
		perror("connect failed\n");
		exit(1);
	}
	DPRINT("in %d, out %d\n", fromwily, towily);

	DPRINT("Selecting given address\n");
	wm_goto(towily, true, 0, 0, addr);
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

	read_info(1);		/* reverse and coalese ranges */
	if (!modified) {
		DPRINT("Not changed - just removing file\n");
		remove(tmpfilename);
		return 0;
	}
	connect_wily(origfile, origaddr);
	send_data();
	remove(tmpfilename);
	return 0;
}
