# Generated automatically from Makefile.in by configure.
SHELL			= /bin/sh

srcdir			= .

prefix			= /usr/local
exec_prefix		= ${prefix}
bindir			= $(exec_prefix)/bin
mandir			= $(prefix)/man

INSTALL			= ./install-sh -c
INSTALL_DATA	= ${INSTALL} -m 644

CC				= gcc
CFLAGS			= -g -O -I.. -I$(srcdir)/../include -I$(srcdir)/../sam/include
DEFS			=  -DHAVE_LIBNSL=1 -DHAVE_LIBSOCKET=1 -DHAVE_DEV_PTMX=1 -DHAVE_SYS_PTEM_H=1 
LIBS			= -lsocket -lnsl  ../libmsg/libmsg.a  ../sam/libXg/libXg.a
LDFLAGS			= 

RC				= /usr/pgrad/gary/bin/sun4d/rc

SRCS			= win.c wgoto.c wreplace.c Tag.rc mktags.rc Man.rc
BINS			= win   wgoto   wreplace   Tag    mktags    Man    
MANS			= win.1 wgoto.1 wreplace.1 Tag.1  mktags.1  Man.1

DISTFILES		= \
	README INSTALL \
	configure configure.in Makefile.in install-sh mkinstalldirs \
	$(SRCS) $(MANS)
.SUFFIXES		:
.SUFFIXES		: .c .rc
.c				:
	$(CC) $(CFLAGS) $(DEFS) $(LDFLAGS) -o $* $*.c $(LIBS)
.rc				:
	( echo '#!' $(RC) ; cat $*.rc; ) >$* ; \
	chmod a+x $*

all				: $(BINS)

install			: all installdirs
	for BIN in $(BINS) ; do \
		$(INSTALL) $$BIN $(bindir)/$$BIN ; \
	done
	for MAN in $(MANS) ; do \
		$(INSTALL_DATA) $$MAN $(mandir)/man1/$$MAN ; \
	done

installdirs		: mkinstalldirs
	$(srcdir)/mkinstalldirs $(bindir) $(mandir)/man1

clean			:
	rm -f $(BINS)

distclean		: clean
	rm -f Makefile config.cache config.status config.log 

SCCSFILES		= $(SRCS) $(MANS)
SCCSDIR			= SCCS
admin			:
	-test -d $(SCCSDIR) && rm -rfi $(SCCSDIR)
	mkdir $(SCCSDIR)
	for FILE in $(SCCSFILES) ; do \
		echo $$FILE: ; \
		admin -i $$FILE $(SCCSDIR)/s.$$FILE ; \
	done
	rm -i $(SCCSFILES)
get				:
	for FILE in $(SCCSFILES) ; do \
		echo $$FILE: ; \
		get $(SCCSDIR)/s.$$FILE ; \
	done
edit			:
	for FILE in $(SCCSFILES) ; do \
		echo $$FILE: ; \
		get -e $(SCCSDIR)/s.$$FILE ; \
	done
delta			:
	for FILE in $(SCCSFILES) ; do \
		echo $$FILE: ; \
		delta -y '' $(SCCSDIR)/s.$$FILE ; \
	done

dist			:
	./mkdist $(DISTFILES)
