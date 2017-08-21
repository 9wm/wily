/*******************************************
 *	Read and format directories
 *******************************************/

#include "wily.h"
#include "data.h"

enum {
	MAXNAMES = 10240		/* initial number of names in a directory */
};

static int
mycmp (const void *a, const void *d) {
	return strcmp(*(char**)a, *(char**)d);
}

/* A few OS's have file systems that know which files in a directory are dirs. */
#ifdef DT_DIR
#define CHECKDIR(dp)	((dp)->d_type == DT_DIR)
#else
#define CHECKDIR(dp)	(0)
#endif

/* 
 * Read 'dirp', return null-terminated array of strings representing the
 * files in the directory.  Both the strings and the array need to be
 * freed later (use dirnames_free()).
 *
 * Returns 0 on error.
 */
char **
dirnames (DIR *dirp, char *path) {
	struct stat		statbuf;
	struct dirent	*direntp;
	char		*name;
	char		**list;
	int		nfiles;
	int		maxnames = MAXNAMES;

	/* Read the directory into a big buffer */

	rewinddir(dirp);	/* Workaround for FreeBSD. */

	list = (char**) salloc(maxnames * sizeof(char*));

	/* temporarily chdir so we can stat */
	if(chdir(path)) {
		diag(path, "couldn't chdir");
		return 0;
	}
	nfiles = 0;
	while ((direntp = readdir(dirp))) {
		name = direntp->d_name;
		if( name[0] == '.' &&  (!show_dot_files || name[1]=='\0')) {
			continue;
		}
		if (!(nfiles+2 < maxnames) ) {
			maxnames *= 2;
			list = (char**) srealloc(list, maxnames * sizeof(char*));
		}

		if (CHECKDIR(direntp) ||
				(stat(name, &statbuf) >= 0 && S_ISDIR(statbuf.st_mode))) {
			Path	buf;

			sprintf(buf, "%s/", name);
			list[nfiles++] = strdup(buf);
		} else {
			list[nfiles++] = strdup(name);
		}
	}
	/* return to wilydir */
	if(chdir(wilydir)) {
		diag(wilydir, "couldn't chdir back to wilydir [%s]", wilydir);
	}
	closedir(dirp);

	qsort(list, nfiles, sizeof(char*), mycmp);

	list[nfiles] = 0;
	return list;
}

/* Keep consistent with dirnames() */
void
dirnames_free(char**names) {
	char	**s;

	if(!names)
		return;
	for(s=names; *s; s++)
		free(*s);
	free(names);
}


