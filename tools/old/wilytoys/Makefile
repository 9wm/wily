W = /home/steve/src/9/orig/wily-0.9.8
CFLAGS = -Xc -g
CPPFLAGS = -I$W -I$W/include -I$W/libXg 
LDFLAGS = -L$W/libXg -L$W/libmsg
LDLIBS = -lmsg -lXg -lnsl -lsocket

PROGS = bold wcat

all: $(PROGS)

install:
	cp $(PROGS) $(HOME)/bin/Wily

nuke:
	$(RM) $(PROGS)
