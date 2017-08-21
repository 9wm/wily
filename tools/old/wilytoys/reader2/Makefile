W = /home/steve/src/9/orig/wily-0.9.8
O = /home/steve/src/9/orig/orig-0.9.8
# CC = /opt/CenterLine/bin/proof cc
CFLAGS = -Xc -g -DUSE_SELECT #-DDEBUG
CPPFLAGS = -I. -I$W/include -I$W
LDFLAGS = -L$W/libmsg -L$W/libXg
LDLIBS = -lmsg -lXg -lnsl -lsocket
PATCHES = includepatches libmsgpatches wilypatches winpatches

# You might not have all of the PROGs listed here - don't worry about
# it - they're not ready yet, and it just means that I've forgotten to
# strip them from the Makefile before shipping...
PROGS = mreader nreader
COBJS = getmsg.o reader.o utils.o addr.o
MOBJS = mail.o membuf.o mbox.o solaris.o
NOBJS = news.o nntp.o newsrc.o post.o

all: $(PROGS)

mreader: $(COBJS) $(MOBJS)
	$(LINK.c) -o $@ $(COBJS) $(MOBJS) $(LDLIBS)
nreader: $(COBJS) $(NOBJS)
	$(LINK.c) -o $@ $(COBJS) $(NOBJS) $(LDLIBS)
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

