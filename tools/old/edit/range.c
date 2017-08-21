#include <stdio.h>
#include <stdlib.h>
#include "range.h"
static Subrange *ranges, *range_ptr;

static Subrange *shrink_ranges(int rev, Subrange *res);

/*
 * This defines the number of Runes we're willing to have
 * in a single message to wily.
 */

#define MAXCHG		4000

/*
 * Functions for handling the ranges. Returns a subrange with
 * the min and max range of the whole set of ranges. Returns
 * 0 if no ranges were read.
 * Accepts either single ranges (from e, x) or pairs or ranges (from c).
 * Attempts to allow you to chain instances of c/i/a/d, but that's not
 * working properly yet.
 */

Subrange *
read_ranges(int rev)
{
	int nranges = 0;
	static Subrange res;
	Subrange *r, *l;
	ulong p0, p1, q0, q1;
	char buf[FILENAME_MAX+1];

	while (fgets(buf, FILENAME_MAX, stdin)) {
		int npairs = sscanf(buf,"#%lu,#%lu #%lu,#%lu\n", &p0, &p1, &q0, &q1);
		switch (npairs) {
			default:
				return shrink_ranges(rev, nranges? &res : 0);
			case 4:
				if (nranges) {
					if (q0 < res.q0)
						res.q0 = q0;
					if (q1 > res.q1)
						res.q1 = q1;
				} else {
					res.q0 = q0;
					res.q1 = q1;
				}
				/* FALLTHROUGH */
			case 2:
				if (nranges) {
					if (p0 < res.p0)
						res.p0 = p0;
					if (p1 > res.p1)
						res.p1 = p1;
				} else {
					res.p0 = p0;
					res.p1 = p1;
				}
				break;
		}
		r = salloc(sizeof(*r));
		if (rev) {
			r->p0 = p0;
			r->p1 = p1;
			r->q0 = q0;
			r->q1 = q1;
		} else {
			if (npairs == 4) {
				r->p0 = q0;
				r->p1 = q1;
			} else {
				r->p0 = p0;
				r->p1 = p1;
			}
		}
		r->next = 0;
		if (nranges++) {
			l->next = r;
			l = r;
		} else
			l = ranges = r;
	}
	return shrink_ranges(rev, nranges? &res : 0);
}

Subrange *
next_range(void)
{
	Subrange *res = range_ptr;

	if (range_ptr)
		range_ptr = range_ptr->next;
	return res;
}

/*
 * If rev==0, then just return the min/max subrange that we've been given
 * (this is old, should just return a worked/failed flag).
 * If rev==1, then we're in "q", and will be sending the changes back to
 * wily. Reads the list of ranges, attempts to coalese them into single
 * updates, and reverses the order.
 */

static Subrange *
shrink_ranges(int rev, Subrange *res)
{
	Subrange *newranges = 0, *r0, *r1;
	range_ptr = ranges;
	if (rev == 0)
		return res;
	r0 = next_range();
	while (r0) {
		while ((r1 = next_range()) && (r1->q1 - r0->q0) < MAXCHG) {
			/* can fit this change into the current message */
			r0->p1 = r1->p1;
			r0->q1 = r1->q1;
			free(r1);
		}
		/* message is now too big to fit next range in. */
		r0->next = newranges;
		newranges = r0;
		r0 = r1;
	}
	range_ptr = ranges = newranges;
	return res;
}
