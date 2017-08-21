#include <stdio.h>
#include <u.h>
#include <libc.h>
#include <util.h>
#include <libg.h>

/*
 * XXX - current assumes that we won't be in mid-sequence when EOF is reached.
 */

#define MAGICBOLD			0x4000
#define MAGICITALIC			0x4100
#define MAGICITALICBOLD	0x4200

static long charset = MAGICITALIC;

static void
boldchar(int c, int overstrike)
{
	char str[4], *s = str;
	Rune r = c + charset;

	switch (runetochar(s, &r)) {
		case 3:	putchar(*s++);
		case 2:	putchar(*s++);
		case 1:	putchar(*s++);
	}
}

int
main(int argc, char *argv[])
{
	int i = 0, b = 0,c;
	int state = 0;
	int outchar;
	int os = 0;

	while ((c = getopt(argc, argv, "ib")) != EOF) {
		switch (c) {
			case 'b':	b++;	break;
			case 'i':	i++;	break;
			default:	exit(1);
		}
	}
	charset = b&&i? MAGICITALICBOLD : b? MAGICBOLD : MAGICITALIC;
	while ((c = getchar()) != EOF) {
		switch (state) {
			case 0:
				if (c == '_')
					state++;
				else
					putchar(c);
				break;
			case 1:
				if (c == 0x8)			/* backspace */
					state++;
				else {
					putchar('_');
					putchar(c);
					state = 0;
				}
				break;
			case 2:
				boldchar(c,0);
				state = 0;
				break;
			default:
				fprintf(stderr,"We're screwed!\n");
				exit(1);
		}
	}
	if (state)
		fprintf(stderr,"File was truncated!\n");
	return 0;
}
