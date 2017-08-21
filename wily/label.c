/*************************************************************
	Various routines to do with a Data's label and/or path
*************************************************************/

#include "wily.h"
#include "data.h"

static void	data_restat(Data*d);

#define DATALABEL(d) ((d)?(d)->label:wilydir)

void
data_addcontext(Data*d, char*dest, char*orig) {
	addcontext(dest, DATALABEL(d), orig);
	labelclean(dest);
}

/* Copy d's label (or wilydir) to 'dest' */
void
data_getlabel(Data*d, char*dest){
	strcpy(dest, DATALABEL(d));
}

/* D's label has been changed to 's',
 * update our internal structures.
 */
void
data_setlabel(Data*d, char *s) {
	assert(d);

	/* Just record the new label, only update
	 * other stuff on demand.
	 */
	strcpy(d->label, s);
	if(STRSAME(d->label, d->cachedlabel)) {
		if(!data_isdirty(d)) {
			tag_rmtool(d->tag, "Put");
		}
	} else {
		tag_addtool(d->tag, "Get");
		tag_addtool(d->tag, "Put");
	}
}

/* Return pointer to View with same 'label', or null. */
View *
data_find(char*label) {
	Data	*d;
	Stat	buf;
	Path	path;

	/* Search for data with same label */
	for(d=dataroot; d; d=d->next) {
		if (STRSAME(d->label, label))
			return text_view(d->t);
	}

	/* Search for data with same stat buffer */
	label2path(path,label);
	if(stat(path,&buf))
		return 0;
	
	for(d=dataroot; d; d=d->next) {
		data_restat(d);
		if (d->has_stat && !statcmp(&buf, &d->stat))
			return text_view(d->t);
	}
	
	return 0;
}

/* If 'label' has changed under us, update 
 * some other information.
 */
static void
data_restat(Data*d) {
	if(strcmp(d->label, d->cachedlabel)) {
		Path	path;
		
		strcpy(d->cachedlabel, d->label);
		label2path(path, d->label);
		d->has_stat = !stat(path, &d->stat);
	}
}

