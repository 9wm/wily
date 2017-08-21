/*******************************************
 *	Data read/write files, directories
 *******************************************/

#include "wily.h"
#include "data.h"

static int		data_getstat		(Data*, char*, char*, Stat*);
static void	data_opennew		(Data*, char*, char*path);
static int		data_getdir		(Data*, char*, char*, Stat*);
static int		data_getfile		(Data*, char*, char*, Stat*);
static void	data_settag		(Data*, char*);
static Data *	data_alloc		(void);

/*
 * Open a new 'data' to represent 'label'.  Either read 'label', or if
 * it doesn't exist and 'create' is set, return an empty window to
 * represent where we _will_ write it.
 *
 * Return (View*)0 if 'label' exists but we can't read it, or it doesn't
 * exist and we don't want to create it.
 */
View*
data_open(char*label, Bool create) {
	Data	*d;
	Stat	buf;
	Path	path;
	Bool	failure;
	
	label2path(path, label);

	d = data_alloc();
	failure = false;
	
	if(stat(path,&buf)) { /* doesn't exist -- yet */
		if(create)
			data_opennew(d, label, path);
		else
			failure = true;
	} else {
		failure = data_getstat(d, label, path,&buf);
	}
	
	if(failure){
		data_listremove(d);
		free(d);
		return 0;
	} else {
		if(d->names)
			add_slash(path);
		data_settag(d, path);
		win_new(path, d->tag, d->t); 
		return text_view(d->t);
	}
}

/*
 * Read file or directory 'label' into 'd'. 
 * Return 0 for success.
 */
int
data_get(Data *d, char *label) {
	Stat	buf;
	Path	path;
	
	if(data_backup(d))
		return -1;		/* If we did this, we'd lose data */

	if(!label)
		label = d->label;	/* is strcpy(a,a) bad? */
	label2path(path,label);
	if(stat(path,&buf)){
		diag(0, "Couldn't stat [%s (%s)]", path, label);
		return -1;
	} else {
		return data_getstat(d, label, path, &buf);
	}
}

/* Load 'd' with 'path', which has stat buffer 'buf'.
 * Returns 0 if we loaded succesfully.
 */
static int
data_getstat(Data*d, char* label, char*path, Stat*buf) {
	int	failed;
	
	cursorswitch(&boxcursor);
	bflush();
	undo_reset(d->t);
	failed =  S_ISDIR(buf->st_mode) ?
		data_getdir(d, label, path, buf) :
		data_getfile(d, label, path, buf);

	undo_start(d->t);
	if(!failed)
		undo_mark(d->t);
	
	cursorswitch(cursor);
	return failed;
}

/* Load 'd' with new file 'path'. */
static void
data_opennew(Data*d, char*label, char*path) {
	/* d */
	pathcontract(d->label, label);
	strcpy(d->cachedlabel, d->label);
	d->has_stat = false;
	data_setbackup(d, path);
	dirnames_free(d->names);
	d->names = 0;

	/* d->t */
	text_replace(d->t, text_all(d->t), rstring(0,0));
	undo_start(d->t);
	undo_mark(d->t);
	
	/* d->tag */
	tag_setlabel(d->tag, d->label);
	tag_rmtool(d->tag, "Put");
}

/* Load 'd' with directory 'path', which has stat buffer 'buf'.
 * Returns 0 if we loaded succesfully.
 */
static int
data_getdir(Data*d, char*label, char*path, Stat*buf) {
	DIR*	dirp;
	
	if((dirp = opendir(path)) == NULL) {
		diag(0, "opendir [%s]", path);
		return -1;	/* failure - modify nothing */
	}
	
	/* d */
	pathcontract(d->label,label);
	add_slash(d->label);
	strcpy(d->cachedlabel, d->label);
	
	d->has_stat = true;
	d->stat = *buf;
	data_setbackup(d,0);
	dirnames_free(d->names);
	d->names = dirnames(dirp, path);

	/* d->t */
	text_formatdir(d->t, d->names);
	
	/* d->tag */
	tag_rmtool(d->tag, "Put");
	tag_addtool(d->tag, "Get");
	tag_setlabel(d->tag, d->label);
	return 0;
}

/* Load 'd' with file 'path', which has stat buffer 'buf'.
 * Returns 0 if we loaded succesfully.
 */
static int
data_getfile(Data*d, char*label, char*path, Stat*buf) {
	int	fd;
	extern Bool utfHadNulls;
	if((fd = open(path, O_RDONLY)) == -1) {
		diag(path, "open [%s]", path);
		return -1;	/* failure - modify nothing */
	}
	
	/* d */
	pathcontract(d->label, label);
	strcpy(d->cachedlabel, d->label);

	d->has_stat = true;
	d->stat = *buf;
	data_setbackup(d,path);
	dirnames_free(d->names);
	d->names = 0;

	/* d->t */
	text_read(d->t, fd, buf->st_size);
	close(fd);
	if (utfHadNulls) {
		diag(path, "removed nulls from [%s]", path);
		data_setbackup(d,0);
	}

	/* d->tag */
	tag_rmtool(d->tag, "Put");
	tag_rmtool(d->tag, "Get");
	tag_setlabel(d->tag, d->label);
	return 0;
}

/* Fill 'buf' with appropriate stuff for d's tag */
static void
data_settag(Data*d, char *path) {
	Path	buf;
	char*common;
	char*specific;
	
	common = d->names? dirtools : filetools;
	specific = tag_match(path);
	
	sprintf (buf, "%s %s | %s %s ", d->label, 
		d->names? "Get":"", 
		filetools, specific);
	tag_set(d->tag, buf);
}

static Data *
data_alloc(void) {
	Data	*d;
	static Id id;
	
	d = NEW(Data);
	d->id = id++;
	d->next = dataroot;
	dataroot = d;
	d->t = text_alloc(d, true);
	d->tag = text_alloc(d, false);
	d->names = 0;
	d->backupto = 0;
	d->fd = 0;
	d->emask = 0;
	d->has_stat = false;
	
	strcpy(d->label, "");
	strcpy(d->cachedlabel, "");
	
	return d;
}
