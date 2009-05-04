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

GIT_REV =       "`git describe --always 2>/dev/null`"

CC=	gcc
OPTIM=	-march=native -O2 -fomit-frame-pointer
CFLAGS= $(OPTIM) -W -Wall -g -pipe \
	-DGIT_REV=\"$(GIT_REV)\"
LIBS=	`pkg-config --libs xcb-icccm xcb-shape xcb-image xcb`

OBJS=	wmdia.o
FILES=	Makefile README Changelog AGPL-3.0.txt wmdia.xpm

wmdia:	$(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

wmdia.o:	wmdia.xpm

#----------------------------------------------------------------------------
#	Developer tools

indent:
	for i in $(OBJS:.o=.c) $(HDRS); do \
		indent $$i; unexpand -a $$i > $$i.up; mv $$i.up $$i; \
	done

clean:
	-rm *.o *~

clobber:	clean
	-rm wmdia

dist:
	tar cjCf .. wmdia-`date +%F-%H`.tar.bz2 \
		$(addprefix wmdia/, $(FILES) $(OBJS:.o=.c))

install:
	install -s wmdia /usr/local/bin/
