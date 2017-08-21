/*******************************************
 *	the stuff a window represents (file, directory, type-script...)
 *******************************************/

#include <dirent.h>

/*
The 'Data' struct

The 'label' is the name of the window, as it appears in the window.
The label can start with:
	/	meaning an absolute path name, e.g. /bin
	$	meaning an environment variable, e.g. $HOME
	~	meaning a home-dir, e.g. ~/, ~i/
anything else is interpreted relative to the directory wily started in.

If the window is a directory, the label should 
end with a /

If we represent some file which should be backed up,
'backupto' is set to the name of the file to back up.

For data objects monitored by some external process we track
the file descriptor to that process, and an event mask
telling us what events that process is interested in.

For directories, we cache a null-terminated list of strings
representing the files in the directory.  This is so when
the window is reshaped we can reformat the list without having
to reread the directory.

If this Data object doesn't represent a directory, 'names'
==0

'label' is what currently appears in the tag.
'cachedlabel' is the value of 'label' at the last successful stat/open/...

We only update 'has_stat' and 'stat' when necessary.
*/
struct Data {
	Text		*t;
	Text		*tag;
	Data 	*next;	
	Path		label, cachedlabel;
	/* Path		path;	*/
	Bool		has_stat;
	Stat		stat;
	char		*backupto;

	/* for object connected to some external process */
	int		fd;
	ushort	emask;
	
	Id		id;		/* Unique identifier */
	
	char		**names;	/* cache of names of files in this directory, or 0 */
};

/* A data object needs to be backed up if there is some backup file associated
 * with it, and it's Text isn't clean
 */
#define NEEDSBACKUP(d) (d->backupto && !undo_atmark(d->t))

void		data_setbackup(Data *, char*);
Data *	data_findid(Id );
extern Data	*dataroot;
Bool		data_senddestroy(Data *d);
void		data_listremove(Data *d);

/* dir.c */
void		dirnames_free(char**names);
char**	dirnames (DIR *dirp, char *path);
