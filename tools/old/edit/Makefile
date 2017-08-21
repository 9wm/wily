TARGETS=e q x c
LDFLAGS=-L../libmsg -L../libXg
LDLIBS=-lmsg -lXg -lsocket -lnsl
CPPFLAGS += -I../include -I../wily
CFLAGS += -g

ROBJS = range.o sam.o regexp.o
COBJS = $(ROBJS) change.o
all: $(TARGETS)

q: q.o $(COBJS)
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) q.o $(COBJS) $(LDFLAGS) $(LDLIBS)

c: c.o $(COBJS)
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) c.o $(COBJS) $(LDFLAGS) $(LDLIBS)
	$(RM) d i a
	ln c d
	ln c i
	ln c a

x: x.o $(COBJS)
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) x.o $(COBJS) $(LDFLAGS) $(LDLIBS)
	$(RM) y g v
	ln x y
	ln x g
	ln x v


clean:
	rm -f *.o core *pure*
nuke: clean
	rm -f $(TARGETS) g v y i a d
