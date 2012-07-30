#
#   Makefile - Top level Makefile when using "make" to build.
#              Alternatively, use "bit" to build.
#	

OS 		:= $(shell uname | sed 's/CYGWIN.*/windows/;s/Darwin/macosx/' | tr '[A-Z]' '[a-z]')
MAKE	:= make
EXT 	:= mk

ifeq ($(OS),windows)
ifeq ($(ARCH),)
ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
	ARCH:=x64
else
	ARCH:=x86
endif
endif
	MAKE:= projects/windows.bat $(ARCH)
	EXT := nmake
endif

all compile:
	$(MAKE) -f projects/*-$(OS).$(EXT) $@

build configure generate test package:
	@bit $@

#
#   Complete rebuild of a release build
#
rebuild:
	ku rm -fr $(OS)-*-debug -fr $(OS)-*-release
	$(MAKE) -f projects/ejs-$(OS).$(EXT)
	$(OS)-*-debug/bin/bit --release configure build
	rm -fr $(OS)-*-debug
	bit install

clean clobber:
	$(MAKE) -f projects/ejs-$(OS).$(EXT) $@

version:
	@bit -q version
