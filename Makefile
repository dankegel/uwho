#
# Unix Makefile for uwho
# Read INSTALL first.
# @(#)Makefile	2.7 12/2/92
#

all: uwho

# CONFIGURATION
# A) Select an OS and OS / networking flavor.  (VMS choices untested...)
ARCH = -DUNIX -DSUNOS
# ARCH = -DUNIX -DHPUX
# ARCH = -DUNIX -DNEXT
# ARCH = /define=VMS,WOLLENGONG
# ARCH = /define=VMS,MULTINET
# ARCH = /define=VMS,UCX
# ARCH = -DMSDOS -DNOVELL_LWP

# B) Select a C compiler and options.
CC = gcc
CFLAGS = -Wall -DANSI $(ARCH) -g
LINTFLAGS = $(ARCH)	
LINKS =

# C) Select destination directories for the executable, data, and manual files.
BIN = bin
ETC = etc
MAN = man
#BIN = /usr/local/bin
#ETC = /etc/local
#MAN = /usr/local/man
# Select manual section and extension for the manual files.
MANSECT = 1
MANEXT = 1

# D) Select the host and protocol to use when none is given in a query.
DEF_HOST = horton
#DEF_HOST_PROTOCOL = uwho_PROTO_PH
DEF_HOST_PROTOCOL = uwho_PROTO_WHOIS

# E) Select a pager type to feed the user the output page by page.
# POPEN_PAGER only works if popen() and multitasking are available;
# DUMB_PAGER uses pg.c and raw.c to implement a simple polled pager;
# NO_PAGER will certainly work even if the above two are broken.
# (Warning: POPEN_PAGER would be a security risk if used to paginate
#  the output of an anonymous service, as 'more' allows shell escapes.)
#PAGER = POPEN_PAGER
#PAGER_OBJ = 
#PAGER_H = 

PAGER = DUMB_PAGER
PAGER_OBJ = pg.o raw.o
PAGER_H = pg.h

#PAGER = NO_PAGER
#PAGER_OBJ = 
#PAGER_H =

# Select arguments for the uwho -update during 'make install'.
# These needn't be the same hosts as used during normal operation,
# since they are not compiled into any object files.
WHOIS_SERVER = sipb.mit.edu
PH_SERVER = nwu.edu

# Copy executables and data files to proper places.
# Run uwho -update whois-server ph-server to get first copy of server lists.
install: uwho uwho.doc
	cd $(BIN); rm -f uwho 
	cp uwho $(BIN)
	cd $(BIN); chmod 755 uwho 
	- mkdir $(ETC)
	- mkdir $(MAN) $(MAN)/man$(MANSECT)
	$(BIN)/uwho -update $(WHOIS_SERVER) $(PH_SERVER)
	- rm -f $(MAN)/*/uwho.*
	cp uwho.1 $(MAN)/man$(MANSECT)/uwho.$(MANEXT)
	chmod 644 $(MAN)/man$(MANSECT)/uwho.$(MANEXT)
	# Following line fails on HP's; you'll have to catman yourself.
	- catman -M $(MAN)
	echo Now continue in the INSTALL document, and modify crontab
	echo and optionally inetd.conf.

clean:
	- rm -f *.o uwho core

lint: lint.out

lint.out: main.c uwho.c uwho.h mystring.c mystring.h Makefile
	lint $(LINTFLAGS) main.c uwho.c mystring.c > lint.out
	
# ** Unix build **
uwho: main.o uwho.o mystring.o $(PAGER_OBJ)
	$(CC) $(CFLAGS) main.o uwho.o mystring.o $(PAGER_OBJ) -o uwho $(LINKS)
uwho.o: uwho.c tcpip.h uwho.h mystring.h
	$(CC) $(CFLAGS) -c uwho.c
raw.o: raw.c raw.h
	$(CC) $(CFLAGS) -c raw.c
pg.o: pg.c pg.h
	$(CC) $(CFLAGS) -c pg.c
main.o: main.c uwho.h tcpip.h mystring.h $(PAGER_H) Makefile
	$(CC) $(CFLAGS)	\
	-D$(PAGER) \
	-DUWHO_DIR=\"$(ETC)\" 	\
	-DDEF_HOST=\"$(DEF_HOST)\"	\
	-DDEF_HOST_PROTOCOL=$(DEF_HOST_PROTOCOL)	\
	-c main.c
mystring.o: mystring.c mystring.h
	$(CC) $(CFLAGS) -c mystring.c
uwho.doc: uwho.1
	nroff -man uwho.1 > uwho.doc

# Source archive
FILES = INSTALL Makefile README lwp.lnk main.c makefile.lwp makefile.nfs \
	makefile.tcp makefile.tk4 missing.h multinet.com mystring.c mystring.h \
	patchlev.h pcnfs.lnk pctcp.lnk pg.c pg.h ptk40.lnk \
	raw.c raw.h rawbsd.c rawmsdos.c rawvms.c tcpip.h \
	ucx.com uwho.1 uwho.c uwho.doc uwho.h vmsdef.h wolleng.com

uwho.tar.Z: $(FILES)
	tar cf - $(FILES) | compress > uwho.tar.Z

what.out: $(FILES)
	what $(FILES) > what.out
