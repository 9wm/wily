/*
 * memory.h - defs for message memory handling.
 */

typedef struct _membuf membuf;

struct _membuf {
	char *start, *end;	/* end points to first byte after buffer */
	size_t len;
	struct _membuf *next;
};

void *mb_alloc(size_t);
void mb_free(void *);
void mb_split(void *old, void *new);
