# Makefile for xgrep utility.		Ver. 1.0.0

# Definitions.
MAKEFLAGS = --no-print-directory
ProjName = xgrep
BinName = xgrep
Install1 = /usr/local
ObjDir = obj
SrcDir = src
ObjFile = $(ObjDir)/$(BinName).o
LibDir = lib
RootLibDir =
CXL = $(RootLibDir)/cxlib
CXLib = $(CXL)/lib/libcx.a
XRE = $(RootLibDir)/xrelib
XRELib = $(XRE)/lib/libxre.a
InclFlags = -I$(CXL)/include -I$(XRE)/include
Libs = $(CXLib) $(XRELib) -lc -lm

BinDir = bin
ManDir = share/man
ManPage = $(BinName).1

# Options and arguments to the C compiler.
CC = cc
CFLAGS = -std=c99 -funsigned-char -Wall -Wextra -Wunused\
 -Wno-comment -Wno-missing-field-initializers -Wno-missing-braces -Wno-parentheses\
 -Wno-pointer-sign -Wno-unused-parameter $(COPTS)\
 -O2 $(InclFlags)

# Targets.
.PHONY: all build-msg uninstall install clean

all: build-msg $(BinName)

build-msg:
	@if [ ! -f $(BinName) ]; then \
		echo "Building $(ProjName)..." 1>&2;\
	fi

$(BinName): $(ObjDir) $(ObjFile)
	@for lib in "$(CXLib)" "$(XRELib)"; do \
		[ -f "$$lib" ] || { echo "Error: Library '$$lib' does not exist" 1>&2; exit 1; };\
	done
	$(CC) $(LDFLAGS) -o $@ $(ObjFile) $(Libs)
	strip $@

$(ObjDir):
	@if [ ! -d $@ ]; then \
		mkdir $@ && echo "Created directory '$@'." 1>&2;\
	fi

$(ObjFile): $(SrcDir)/$(BinName).c
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/$(BinName).c

uninstall:
	@echo 'Uninstalling...' 1>&2;\
	if [ -n "$(INSTALL)" ] && [ -f "$(INSTALL)/$(BinName)" ]; then \
		destDir="$(INSTALL)";\
	elif [ -x "$(Install1)/$(BinDir)/$(BinName)" ]; then \
		destDir=$(Install1);\
	else \
		destDir=;\
	fi;\
	if [ -z "$$destDir" ]; then \
		echo "'$(BinName)' binary not found." 1>&2;\
	else \
		for f in "$$destDir"/$(BinDir)/$(BinName) "$$destDir"/$(ManDir)/man1/$(ManPage); do \
			rm -f "$$f" 2>/dev/null && echo "Deleted '$$f'" 1>&2;\
		done;\
	fi;\
	echo 'Done.' 1>&2

install: uninstall
	@echo 'Beginning installation...' 1>&2;\
	umask 022;\
	myID=`id -u`;\
	comp='-C' permCheck=true destDir=$(INSTALL);\
	if [ -z "$$destDir" ]; then \
		destDir=$(Install1);\
	else \
		[ $$myID -ne 0 ] && permCheck=false;\
		if [ ! -d "$$destDir" ]; then \
			mkdir "$$destDir" || exit $$?;\
		fi;\
		destDir=`cd "$$destDir"; pwd`;\
	fi;\
	if [ $$permCheck = false ]; then \
		own= grp= owngrp=;\
		dmode=755 fmode=644;\
	else \
		if [ `uname -s` = Darwin ]; then \
			fmt=-f perm='%p';\
		else \
			fmt=-c perm='%a';\
		fi;\
		eval `stat $$fmt 'own=%u grp=%g' "$$destDir"`;\
		if [ $$own -ne $$myID ] && [ $$myID -ne 0 ]; then \
			owngrp=;\
		else \
			owngrp="-o $$own -g $$grp";\
		fi;\
		eval `stat $$fmt $$perm "$$destDir" |\
		 sed 's/^/000/; s/^.*\(.\)\(.\)\(.\)\(.\)$$/p3=\1 p2=\2 p1=\3 p0=\4/'`;\
		dmode=$$p3$$p2$$p1$$p0 fmode=$$p3$$(($$p2 & 6))$$(($$p1 & 6))$$(($$p0 & 6));\
		[ $$p3 -gt 0 ] && comp=;\
	fi;\
	[ -f $(BinName) ] || { echo "Error: File '`pwd`/$(BinName)' does not exist" 1>&2; exit 1; };\
	[ -d "$$destDir"/$(BinDir) ] || install -v -d $$owngrp -m $$dmode "$$destDir"/$(BinDir) 1>&2;\
	install -v $$owngrp -m $$dmode $(BinName) "$$destDir"/$(BinDir) 1>&2;\
	[ -d "$$destDir"/$(ManDir)/man1 ] || install -v -d $$owngrp -m $$dmode "$$destDir"/$(ManDir)/man1 1>&2;\
	install -v $$comp $$owngrp -m $$fmode doc/$(ManPage) "$$destDir"/$(ManDir)/man1 1>&2;\
	echo "Done.  xgrep files installed in '$$destDir'." 1>&2

clean:
	@rm -f $(BinName) $(ObjDir)/*.o
