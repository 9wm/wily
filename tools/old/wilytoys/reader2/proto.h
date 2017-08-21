#ifndef PROTO_H
#define PROTO_H

/* getmsg.c */
rdWin *getActiveWin(void);

/* reader.c */
void closeWin(rdWin *w);
void freeWin(rdWin *w);
rdWin *getArtWin(int user, void *userp, char *title, char *tools, char *text, rdWin *oldwin);
rdWin *getListWin(int user, void *userp, char *title, char *tools, rdWin *oldwin, char **items);
void highlightItem(rdWin *w, rdItemRange r);
void warpToWin(rdWin *w);
rdItemRange itemNumber(rdWin *w, ulong p0, ulong p1);
int rdAddItem(rdWin *w, char *text);
int rdChangeItem(rdWin *w, int item, char *text);
int rdDelItem(rdWin *w, int item);
void rdGotoWin(rdWin *w);
void rdInclude(rdWin *w, char *str, size_t len);
void rdSetMulti(int list, int art);
void rdSetRescanTimer(int secs);
rdWin *readerLoading(int user, void *userp, int isart, char *title);
int readerInit();
int readerMainLoop(void);
rdWin *userpWin(void *ptr);
void winReflectCmd(rdWin *w, char *cmd, char *arg);
rdWin *getBlankListWin(int user, void *userp);
rdWin *getBlankArtWin(int user, void *userp);
int setWinUser(rdWin *w, int user, void *userp);
int setWinTitle(rdWin *w, char *title);
int setWinTools(rdWin *w, char *tools);
int setWinList(rdWin *w, char **items);
int setWinArt(rdWin *w, char *art);

/* reader clients: mail.c, news.c */
void user_cmdArt(rdWin *w, char *cmd, ulong p0, ulong p1, char *arg);
void user_cmdList(rdWin *w, char *cmd, ulong p0, ulong p1, rdItemRange items, char *arg);
void user_listSelection(rdWin *w, rdItemRange r);
void user_delWin(rdWin *w);

#ifdef WILYMAIL_H

/* mail.c */
void dorescan(void);

#endif

/* utils.c */
ulong atoul(char *str);
char *sstrdup(char *str);

/* addr.c */
void parseaddr(char *addr, int len, char **email, char **name);

#endif /* !PROTO_H */
