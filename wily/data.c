/*******************************************
 *	Data methods
 *******************************************/

#include "wily.h"
#include "data.h"
#include <errno.h>

Data		*dataroot;

Text*
data_body(Data*d) {
	return d->t;
}

Text*
data_tag(Data*d){
	return d->tag;
}

/* Return the names in directory 'd', or 0 */
char**
data_names(Data*d){
	return d ? d->names : 0;
}

/* Return true if 'd' should have its resize box filled in */
Bool
data_isdirty(Data *d) {
	return d && NEEDSBACKUP(d);
}

/* Write all dirty windows.  Return 0 for success. */
int
data_putall(void) {
	Data	*d;
	int	retval = 0;

	for(d=dataroot; d; d=d->next)
		if(NEEDSBACKUP(d))
			if (data_put(d,0))
				retval = -1;
	return retval;
}

/* Backup all dirty files.  Return 0 unless we failed to backup something. */
int
data_backupall(void) {
	Data	*d;
	int	retval= 0;

	for(d = dataroot; d; d = d->next) {
		if (data_backup(d)) {
			retval = 1;
		}
	}
	return retval;
}

/*
 * Write the contents of 'd' to 'path'.
 * Return 0 if we've successfully written 'd' to its label.
 */
int
data_put(Data *d, char *label) {
	int		fd;
	Stat		buf;
	Path		path;
	Bool		statfailed, writefailed;
	
	if(!label)
		label = d->label;
	label2path(path, label);
	
	statfailed = stat(path, &buf);

	if((fd = open(path,O_RDWR|O_CREAT|O_TRUNC, 0666))<0){
		diag(path, "couldn't open \"%s\" for write", path);
		return -1;
	}
	writefailed = text_write_range(d->t, text_all(d->t), fd);
	close(fd);
	if(writefailed) {
		diag(path, "write failed: \"%s\"", path);
		return -1;
	}
	
	if (!strcmp(d->label,label) || !statcmp(&buf, &d->stat)) {
		if(!d->names) {
			tag_rmtool(d->tag, "Put");
			undo_mark(d->t);
		}
		return 0;
	}
	return -1;
}

void
data_listremove(Data *d) {
	Data	**ptr;

	for(ptr = &dataroot; *ptr != d; ptr = &((*ptr)->next))
		;
	*ptr = d->next;
}

/* Delete 'd' if we can do so without permanently losing
 * data, and return 0.  If there's a problem, return 1.
 */
int
data_del(Data*d) {
	if(data_backup(d))
		return 1;
	
	data_senddestroy(d);
	data_listremove(d);
	dirnames_free(d->names);
	if(d->backupto)
		free(d->backupto);
	free(d);
	return 0;
}

/* Backup 'd' if necessary.  Return 0 for success. */
int
data_backup(Data *d) {
	Path	fname;

	if (!NEEDSBACKUP(d))
		return 0;

	if( (backup_name(d->backupto, fname)) )
		return -1;
	if(text_write(d->t, fname))
		return -1;

	errno = 0;
	diag(fname, "backup %s %s", mybasename(fname), d->backupto);
	return 0;
}

/* Record that in emergencies, 'd' should be saved, noting
 * that it is a copy of 'bfile'
 */
void
data_setbackup(Data *d, char*bfile) {
	if(d->backupto)
		free(d->backupto);
	if(bfile && !strstr(bfile, "+Errors")) {
		d->backupto = strdup(bfile);
		text_setneedsbackup(d->t, true);
	} else {
		d->backupto = 0;
		text_setneedsbackup(d->t, false);
	};
}

Data *
data_findid(Id id) {
	Data	*d;

	for(d=dataroot; d; d=d->next)
		if(d->id==id)
			break;
	return d;
}

