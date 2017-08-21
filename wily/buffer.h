typedef struct Bnode Bnode;
typedef struct Buffer Buffer;

struct Buffer {
	Bnode *head;
};

struct Bnode {
	Bnode 	*next, *prev;
	ulong	off;
	Range	gap;
	Rune	buf[BUFFERSIZE];
};

#define bnsize(n) ( BUFFERSIZE- RLEN(n->gap))
