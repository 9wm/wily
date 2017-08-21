#
#	$Header: /u/cvs/wtcl/Makefile,v 1.2 1996/11/12 23:33:55 cvs Exp $
#
CC=gcc
W=$(TMPDIR)/wily-0.13.19/
WINC=-I$W -I$W/include -I$W/libXg -I$W/sam/include
INC=-I/usr/local/include $(WINC)
WLD=-L$W/sam/libXg -L$W/libmsg -lmsg -lXg
CFLAGS=-Wall $(INC)
LDFLAGS=-L/usr/local/lib -ltk -ltcl -lX11 -lm $(WLD)
T=wtcl wtk
S=wtcl.c wtk.c wily.c
O=$(S:.c=.o)

all : $T

wtcl : wtcl.o wily.o
	$(CC) -o $@ wtcl.o wily.o $(LDFLAGS)
	
wtk : wtk.o wily.o
	$(CC) -o $@ wtk.o wily.o $(LDFLAGS)
	
clean :
	rm -f $T $O core
