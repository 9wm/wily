/*
 * solaris.c - the system-dependent routines for Solaris 2.
 */

#include "headers.h"
#include "mbox.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * load_mbox(mbox) - open the named file, and make it available
 * as a block of memory between mbox_start and mbox_end.
 */

#ifdef USE_MMAP
int
load_mbox(mMbox *mbox)
{
	mode_t mode = mbox->readonly? O_RDONLY : O_RDWR;
	int fd = open(mbox->name,mode);
	caddr_t addr;
	size_t len;
	int prot, flags;
	off_t off;
	struct stat st;

	if (fd < 0)
		return 1;
	if (fstat(fd,&st) == -1)
		return 1;
	len = (size_t)st.st_size;
	mbox->size = st.st_size;
	mbox->mtime = st.st_mtime;
	addr = 0;
	prot = PROT_READ;
	off = 0;
	flags = MAP_SHARED;

	if ((mbox->mbox_start = mmap(addr,len,prot,flags,fd,off)) == MAP_FAILED)
		return 1;
	close(fd);
	mbox->mbox_end = mbox->mbox_start + len;
	return 0;
}

#else /* USE_MMAP */

int
load_mbox(mMbox *mbox)
{
	const char *mode = mbox->readonly? "r" : "r+";
	FILE *fp = fopen(mbox->name,mode);
	char *addr = 0;
	size_t len;
	struct stat st;

	if (fp == 0)
		goto broken;
	if (stat(mbox->name,&st) == -1)
		goto broken;
	len = (size_t)st.st_size;
	mbox->size = st.st_size;
	mbox->mtime = st.st_mtime;

	if ((addr = malloc(len+1)) == 0)
		goto broken;
	addr[len] = 0;		/* handy for debugging */
	if (fread(addr, (size_t)1, len, fp) != len)
		goto broken;
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

#endif /* USE_MMAP */
