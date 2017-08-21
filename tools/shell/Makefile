# 9term
CC=gcc
WILY=$(h)/src/wily
BINDIR=$(h)/obj/sun4d/wily
CFLAGS=-g -Wall -I. -I$(WILY)/include -I$(WILY)/sam/include -I$(BINDIR)
LIBS=-L$(BINDIR)/libmsg -lmsg -lsocket -lnsl -lintl

TARGETS=	Wgoto Wexec Wins 
OBJECTS=$(TARGETS:%=%.o)

$(TARGETS):

Wins:	Wins.o
	gcc -o Wins Wins.o $(LIBS) 
Wexec:	Wexec.o
	gcc -o Wexec Wexec.o $(LIBS) 
Wgoto:	Wgoto.o
	gcc -o Wgoto Wgoto.o $(LIBS) 
clean:
	- rm $(OBJECTS)
nuke:
	- rm $(OBJECTS) $(TARGETS)
