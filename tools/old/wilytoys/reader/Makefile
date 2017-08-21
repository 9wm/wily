W = /home/steve/src/9/orig/wily-0.9.5
O = /home/steve/src/9/orig/orig-0.9.5
CFLAGS = -Xc -g -DUSE_SELECT
CPPFLAGS = -I. -I$W/include -I$W
LDFLAGS = -L$W/libmsg -L$W/libXg
LDLIBS = -lmsg -lXg -lnsl -lsocket
PATCHES = includepatches libmsgpatches wilypatches winpatches

PROGS = mreader browser
OBJS = getmsg.o mail.o reader.o membuf.o mbox.o solaris.o utils.o

all: $(PROGS)

mreader: $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
clean:
	$(RM) *.o core
nuke:
	$(RM) *.o core $(PROGS)

cleanpatches:
	$(RM) $(PATCHES)

patches: $(PATCHES)

includepatches:
	makepatch include > $@

libmsgpatches:
	makepatch libmsg > $@

wilypatches:
	makepatch wily > $@

winpatches:
	makepatch win > $@

