#ifndef WILY_EDIT_H
#define WILY_EDIT_H

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include "sam.h"

typedef struct Subrange Subrange;
struct Subrange {
	ulong p0, p1, q0, q1;		/* p is wily ref, q is new file ref */
	struct Subrange *next;
};

Subrange *read_ranges(int rev);
Subrange *next_range(void);
void do_changes(int (*changed)(Rune **r0, Rune **r1, Rune **r2, Rune **r3));
void write_info(void);
void list_changes(void);

extern char origfile[FILENAME_MAX+1],  origaddr[FILENAME_MAX+1];
extern char tmpfilename[FILENAME_MAX+1];
extern Subrange *minmax;
extern ulong base;
extern char *utffile;
extern int modified;
extern File runefile;

#endif /* ! WILY_EDIT_H */
