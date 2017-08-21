/*
 * solaris.c - the system-dependent routines for Solaris 2.
 */

#include "mailheaders.h"

#include <sys/stat.h>
#include <fcntl.h>

/*
 * load_mbox(mbox,rescan) - open the named file, and make it available
 * as a block of memory between mbox_start and mbox_end. If rescan
 * is true, we've already the mbox, and we want to see if there're more
 * messages in the file.
 */

int
load_mbox(mMbox *mbox, int rescan)
{
	const char *mode = mbox->readonly? "r" : "r+";
	FILE *fp = fopen(mbox->name,mode);
	char *addr = 0;
	size_t len;
	struct stat st;

	if (fp == 0) {
		DPRINT("Could not open file");
		goto broken;
	}
	if (stat(mbox->name,&st) == -1) {
		DPRINT("Could not stat file");
		goto broken;
	}
	if (rescan) {
		if (st.st_size < mbox->size) {
			fprintf(stderr,"%s corrupt\n", mbox->name);
			goto broken;
		}
		if (st.st_mtime == mbox->mtime) {
			DPRINT("file hasn't changed");
			fclose(fp);
			mbox->mbox_start = mbox->mbox_end = 0;
			return 0;
		}
		len = (size_t)(st.st_size - mbox->size);
		if (fseek(fp, mbox->size, SEEK_SET) == -1) {
			DPRINT("Could not seek to start of new messages");
		}
	} else
		len = (size_t)st.st_size;
	mbox->size = st.st_size;
	mbox->mtime = st.st_mtime;

	if ((addr = mb_alloc(len+1)) == 0) {
		DPRINT("Could not allocate space for new messages");
		goto broken;
	}
	addr[len] = 0;		/* handy for debugging */
	if (fread(addr, (size_t)1, len, fp) != len) {
		DPRINT("Could not read new messages");
		goto broken;
	}
	fclose(fp);
	mbox->mbox_start = addr;
	mbox->mbox_end = mbox->mbox_start + len;
	return 0;
broken:
	if (addr)
		free(addr);
	fclose(fp);
	return 1;
}


/* we failed to load the mbox. Check that it exists, if we need it to.
 */

int
empty_mbox(mMbox *box, const char *filename, int mustexist)
{
	int readable = !access(filename, R_OK);
	int exists = !access(filename, F_OK);

	if (readable || (exists && !readable) || (mustexist && !exists)) {
		fprintf(stderr,"%s not %s\n",filename, readable? "read" : exists? "readable" : "found");
		return 1;
	}
	/* file doesn't exist, but that's ok - it doesn't need to in this case, so
	create an empty mbox */
	box->size = 0;
	box->mbox_start = box->mbox_end = 0;		/* XXX - likely to flush out some bugs */
	return 0;
}
