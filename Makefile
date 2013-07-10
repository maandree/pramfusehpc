# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.
# 
# [GNU All Permissive License]


OPTIMISE = -g
# DO NOT go above -03, those optimisations are not guaranteed to
# produce correct code, meaning that it can introduce bugs.

CFLAGS_FUSE = $(shell pkg-config --cflags fuse)
LDFLAGS_FUSE = $(shell pkg-config --libs fuse)

CPPFLAGS = -DFUSE_USE_VERSION=29
CFLAGS = $(OPTIMISE) -std=gnu11 -Wall -Wextra -pedantic $(CFLAGS_FUSE)
LDFLAGS = $(LDFLAGS_FUSE) -lulockmgr


all:
	@mkdir -p bin
	"$(CC)" $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o bin/pramfusehpc src/program.c src/map.c

