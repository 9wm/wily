# Generated automatically from Makefile.in by configure.
SHELL=/bin/sh
srcdir=.
prefix = /usr/local
LIBS=-lsocket -lnsl 
exec_prefix = ${prefix}

# Directory in which to install scripts.
bindir = $(exec_prefix)/bin
CC=gcc
RANLIB=ranlib
INCLUDES= -I.. -I$(srcdir)/../include  -I/usr/openwin/include
OPTS=-DNDEBUG
CFLAGS= -g -O $(OPTS) $(INCLUDES)
TARGET=wily
OBJECTS= env.o include.o label.o file.o msg.o data.o line.o\
	vgeom.o vsearch.o vshow.o \
	tagmatch.o place.o event.o exec.o dir.o \
	point.o global.o cursor.o scroll.o path.o keyboard.o \
	wily.o tag.o view.o grow.o adjust.o win.o list.o col.o\
	undo.o builtins.o util.o select.o\
	mouse.o regexp.o text2.o \
	sam.o text.o click.o tile.o search.o

MYLIBS=../libmsg/libmsg.a ../libframe/libframe.a ../libXg/libXg.a
XLIBS=$(LIBS)  -L/usr/openwin/lib -R/usr/openwin/lib -lXt  -lSM -lICE -lX11  -lnsl -lsocket
all: $(TARGET)
$(TARGET): $(OBJECTS) $(MYLIBS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(MYLIBS) $(XLIBS)
pure: $(OBJECTS) $(MYLIBS)
	purify $(CC) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(MYLIBS) $(XLIBS)

$(OBJECTS): wily.h const.h proto.h ../include/msg.h

builtins.o keyboard.o mouse.o tag.o text.o text2.o tile.o view.o: view.h
keyboard.o search.o tag.o text.o text2.o undo.o view.o line.o click.o : text.h
tile.o grow.o adjust.o win.o col.o list.o:	tile.h
point.o:	tile.h view.h
sam.o regexp.o: sam.h
file.o msg.o data.o label.o :	data.h
adjust.o builtins.o keyboard.o mouse.o point.o select.o tag.o text2.o tile.o vgeom.o view.o vsearch.o vshow.o : view.h

clean:
	rm -f *.o core *pure*
nuke: clean
	rm -f $(TARGET)
dist:	nuke
	cp -r `ls |grep -v RCS` ../dist/wily
install:	$(TARGET)
	cp $(TARGET) $(bindir)
