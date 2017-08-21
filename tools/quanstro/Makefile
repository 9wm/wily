W = $(HOME)/src/plan9/wily/wily-0.11.1
CPPFLAGS = -I$W -I$W/include -I$W/libXg -I$W/sam/include
LDFLAGS = -L$W/sam/libXg -L$W/libmsg
LDLIBS = -lmsg -lXg 
# gmake CFLAGS = -g -Wall -ansi
CFLAGS = -g -Wall -ansi $(CPPFLAGS) $(LDLIBS)
CC=gcc
PROGS = wcat windows wreplace wattach wread

all: $(PROGS) 

install:
	cp $(PROGS) $(binpath)

clean:
	rm $(PROGS)
