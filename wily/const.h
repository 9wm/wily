/*******************************************
 *	Constant declarations
 *******************************************/

#include <sys/param.h>	/* for MAXPATHLEN */
#ifndef DEFAULTSHELL
#define DEFAULTSHELL "/bin/sh"
#endif
#ifndef FOPEN_MAX
#define FOPEN_MAX NOFILE
#endif
#ifndef MAXPATH
#define MAXPATH MAXPATHLEN
#endif

enum {
	/* memory */
	GAPSIZE = 512,			/* buffer gap */
	BUFFERSIZE = 10240,		/* when copying a whole file */
	FILLCHUNK = 512,			/* when filling a View */
	
	/* geometry */
	SCROLLWIDTH = 13,
	MINWIDTH = 50,			/* ... of a window or column */
	SELECTEDBORDER = 3,
	SMALLDISTANCE = 5*5,		/* 5 pixels squared */
	INSET = 4,

	/* mouse buttons */
	LEFT =1, MIDDLE=2, RIGHT=4,	

	/* keys */
	Backspace =	0x7f,
	PageDown = 	0x80,		
	PageUp = 	0x81,
	LeftArrow =	0x82,
	RightArrow =	0x83,
	DownArrow = 0x84,
	UpArrow = 	0x85,
	Home =		0x86	,
	End =		0x87,
	NEWLINE = 	'\n',
	Ctrlh = 		010,
	Ctrlu = 		025,
	Ctrlw =  		027,
	Esc = 		27,

	/* time delays (ms) */
	DOUBLECLICK = 500,
	SCROLLTIME = 100
};
