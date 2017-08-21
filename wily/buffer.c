#include "buffer.h"

void
buffer_delete(Buffer *b, Range r) {
	int removed = RLEN(r);
	Bnode *n = buffer_findnode(r.p0);
	
	while(n && r.p1 > n->off)
		n = bnode_delete(n, r);

	for (;n;n=n->next)
		n->off -= removed;
}

/* Delete relevant bit of 'r' from 'n',
 * and return next node to try.
 * We have to return the next node, because
 * we might have deleted this one before we're done.
 */
Bnode*
bnode_delete(Bnode *n, Range r){
	if(r.p0<= n->off && r.p1 >= n->next->off){
		// delete the whole Bnode
	}
}

void
buffer_add(Buffer *b, ulong p, Rstring s) {
	bnode_add(buffer_findnode(p),p, s));
}

Bnode *
getBnode(Bnode *prev, Bnode *next){
	Bnode *n;
	
	n = (Bnode*)getchunk();
	n->prev = prev;
	n->next = next;
	n->off = off;
}

void
bnode_setgap(Bnode *n, ulong p) {
	assert(p>= n->off);
	assert (p <= n->off + bnsize(n));
	
	p -= n->off;
	nmove = n->gap.p0 - p;
	if (nmove>0)
		memmove(n->buf+p+RLEN(n->gap),
			n->buf + p, nmove*sizeof(Rune));
	else
		memmove (n->buf + n->gap.p0,
				n->buf + n->gap.p1,-nmove*sizeof(Rune));
	n->gap.p1 -= nmove;
	n->gap.p0 = p;
}

/* split 'n' at p */
void
bnode_split(Bnode *n, ulong p){
	Node *n2;
	int oldsize = bnsize(n);
	
	n2 = getBnode(n, n->next);
	n2->next = n->next;
	if(n->next) n->next->prev = n2;
	n->next = n2;

	bnode_setgap(n, p);
	from = n->buf + n->gap.p1;
	nmove = (BUFFERSIZE-n->gap.p1);
	memmove(n2->buf, from, nmove*sizeof(Rune));
	n2->gap = range(nmove, BUFFERSIZE);
	assert(bnsize(n) + bnsize(n2) == oldsize);
}

/* big addition gets its own chunk(s) */
void
bnode_bigadd(Bnode *n, ulong p, Rstring s){
	Bnode *head, *tail;
	
	head = tail = nil;
	
	while(RSLEN(s)){
		tail = getBnode(tail, nil, p);
		if(!head) head = tail;
		int nmove = MIN(BUFFERSIZE, RSLEN(s));
		memmove(tail->buf, s.r0, nmove*sizeof(Rune));
		tail->gap = range(BUFFERSIZE-nmove, BUFFERSIZE);
		p += nmove;
	}
	
	// splice the newly created node(s)
	// in the split created in 'n'
	bnode_split(n, oldp);
	tail->next = n->next;
	n->next->prev = tail;
	n->next = head;
	head->prev = n;
	
	for(n=n->next; n; n=n->next)
		n->off += nadd;
}

Bnode*
bnode_min(Bnode*a, Bnode*b)
{
	return (RLEN(a->gap)>RLEN(b->gap)) ? a : b;
}

void
bnode_add(Bnode *n, ulong p, Rstring s){
	Rstring olds = s;
	int	nadd = RSLEN(s);
	
	/* very big addition? */
	if(RSLEN(s) >= BUFFERSIZE/2){
		bnode_bigadd(n, p, s);
		return;
	}
	
	/* addition bigger than gap? */
	if (RLEN(n->gap)<RSLEN(s)) {
		bnode_split(n, p);
		n = bnode_min(n, n->next);
	}
	
	assert(RLEN(n->gap)>=RSLEN(s));
	bnode_setgap(n, p);
	memmove(n->buf+n->gap.p0, s.r0, nadd*sizeof(Rune));
	n->gap.p0 += nadd;
	for(n=n->next; n; n=n->next)
		n->off += nadd;
}
