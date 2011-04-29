#
#	@file Makefile		@brief	DIA Dockapp
#
#	Copyright (c) 2009 - 2011 by Lutz Sammer.  All Rights Reserved.
#
#	Contributor(s):
#
#	License: AGPLv3
#
#	This program is free software: you can redistribute it and/or modify
#	it under the terms of the GNU Affero General Public License as
#	published by the Free Software Foundation, either version 3 of the
#	License.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU Affero General Public License for more details.
#
#	$Id$
#----------------------------------------------------------------------------

VERSION	=	"1.04"
GIT_REV =	$(shell git describe --always 2>/dev/null)

CC=	gcc
OPTIM=	-march=native -O2 -fomit-frame-pointer
CFLAGS= $(OPTIM) -W -Wall -W -g -pipe \
	-DVERSION='$(VERSION)'  $(if $(GIT_REV), -DGIT_REV='"$(GIT_REV)"')
#STATIC= --static
LIBS=	$(STATIC) `pkg-config --libs $(STATIC) \
	xcb-icccm xcb-shape xcb-image xcb-aux xcb` -lpthread

OBJS=	wmdia.o
FILES=	Makefile README Changelog AGPL-3.0.txt wmdia.doxyfile wmdia.xpm \
	wmdia.1 \
	diashow.sh playvideo.sh set-command.sh set-tooltip.sh showpicture.sh

all:	wmdia

wmdia:	$(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

wmdia.o:	wmdia.xpm Makefile

#----------------------------------------------------------------------------
#	Developer tools

doc:	$(SRCS) $(HDRS) wmdia.doxyfile
	(cat wmdia.doxyfile; \
	echo 'PROJECT_NUMBER=${VERSION} $(if $(GIT_REV), (GIT-$(GIT_REV)))') \
	| doxygen -

indent:
	for i in $(OBJS:.o=.c) $(HDRS); do \
		indent $$i; unexpand -a $$i > $$i.up; mv $$i.up $$i; \
	done

clean:
	-rm *.o *~

clobber:	clean
	-rm -rf wmdia www/html

dist:
	tar cjf wmc2d-`date +%F-%H`.tar.bz2 --transform 's,^,wmdia/,' \
		$(FILES) $(OBJS:.o=.c)

install:	all
	strip --strip-unneeded -R .comment wmdia
	install -s wmdia /usr/local/bin/
	install -D wmdia.1 /usr/local/share/man/man1/wmdia.1

help:
	@echo "make all|doc|indent|clean|clobber|dist|install|help"
