#ifndef WILYPOST_H
#define WILYPOST_H

void nReply(rdWin *w, nGroup *g, int first, int last, char *arg);
void nPost(rdWin *w, nGroup *g, int first, int last, char *arg);
void nFollowUp(rdWin *w, nGroup *g, int first, int last, char *arg);
void nFollRep(rdWin *w, nGroup *g, int first, int last, char *arg);
void nAbort(rdWin *w, nGroup *g, int first, int last, char *arg);
void nDeliver(rdWin *w, nGroup *g, int first, int last, char *arg);
void nSavefile(rdWin *w, nGroup *g, int first, int last, char *arg);
void nSave(rdWin *w, nGroup *g, int first, int last, char *arg);
void dodeliver(rdWin *w, char *filename);
void nIncludeall(rdWin *w, nGroup *g, int first, int last, char *arg);
void nInclude(rdWin *w, nGroup *g, int first, int last, char *arg);
void doinclude(rdWin *w, nGroup *g, int first, int last, char *arg, int all);

#endif /* !WILYPOST_H */
