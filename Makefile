#
#	@file Makefile		DIA Dockapp
#
#	Copyright (c) 2009 by Lutz Sammer.  All Rights Reserved.
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

VERSION	=	"1.02"
GIT_REV =	$(shell git describe --always 2>/dev/null)

CC=	gcc
#OPTIM=	-march=native -O2 -fomit-frame-pointer
OPTIM=	-O2 -fomit-frame-pointer
CFLAGS= $(OPTIM) -W -Wall -W -g -pipe \
	-DVERSION='$(VERSION)'  $(if $(GIT_REV), -DGIT_REV='"$(GIT_REV)"')
#STATIC= --static
LIBS=	$(STATIC) `pkg-config --libs $(STATIC) \
	xcb-icccm xcb-shape xcb-image xcb-aux xcb` -lpthread

OBJS=	wmdia.o
FILES=	Makefile README Changelog AGPL-3.0.txt wmdia.xpm \
	diashow.sh playvideo.sh set-command.sh set-tooltip.sh

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
	-rm wmdia

dist:
	tar cjCf .. www/wmdia-`date +%F-%H`.tar.bz2 \
		$(addprefix wmdia/, $(FILES) $(HDRS) $(OBJS:.o=.c))

install:
	install -s wmdia /usr/local/bin/

help:
	@echo "make all|doc|indent|clean|clobber|dist|install|help"
