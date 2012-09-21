#
#   Makefile - Makefile for Embedthis GoAhead
#
#   You can use this Makefile and build via "make" with a default configuration. Alternatively,
#	you can build using the "bit" tool for for a fully configurable build. If you wish to 
#	cross-compile, you should use "bit". Bit is part of Ejscript available at: http://ejscript.org
#
#   Modify compiler and linker default definitions here:
#
#       export ARCH      = CPU architecture (x86, x64, ppc, ...)
#       export OS        = Operating system (linux, macosx, windows, vxworks, ...)
#       export CC        = Compiler to use 
#       export LD        = Linker to use
#       export PROFILE   = Debug or release profile. For example: debug
#       export CONFIG    = Output directory for built items. Defaults to OS-ARCH-PROFILE
#       export CFLAGS    = Add compiler options. For example: -Wall
#       export DFLAGS    = Add compiler defines. Overrides bit.h defaults. For example: -DBIT_PACK_SSL=0
#       export IFLAGS    = Add compiler include directories. For example: -I/extra/includes
#       export LDFLAGS   = Add linker options
#       export LIBPATHS  = Add linker library search directories. For example: -L/libraries
#       export LIBS      = Add linker libraries. For example: -lpthreads

#
#	Useful definitions:
#
#	export DFLAGS += -DBIT_PAM=1		# To enable PAM-based authorization
#	export DFLAGS += -DBIT_DIGEST=0		# To disable digest authorization
#	export DFLAGS += -DBIT_CGI=0		# To disable CGI request support
#	export DFLAGS += -DBIT_JAVASCRIPT=0	# To disable JavaScript
#	export DFLAGS += -DBIT_UPLOAD=0		# To disable file upload support
#	export DFLAGS += -DBIT_SSL=0		# To disable SSL
#

NAME    := goahead
OS      := $(shell uname | sed 's/CYGWIN.*/windows/;s/Darwin/macosx/' | tr '[A-Z]' '[a-z]')
MAKE    := $(shell if which gmake >/dev/null; then echo gmake ; else echo make ; fi)
EXT     := mk

ifeq ($(OS),windows)
ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
    ARCH?=x64
else
    ARCH?=x86
endif
    MAKE:= projects/windows.bat $(ARCH)
    EXT := nmake
endif

#
#	Chain to per-O/S makefiles under projects
#
all compile:
	$(MAKE) -f projects/$(NAME)-$(OS).$(EXT) $@

clean clobber:
	$(MAKE) -f projects/$(NAME)-$(OS).$(EXT) $@

#
#   Convenience targets when building with Bit
#
build configure generate test package:
	@bit $@

#
#   Complete release rebuild using bit
#
rebuild:
	ku rm -fr $(OS)-*-debug -fr $(OS)-*-release
	$(MAKE) -f projects/$(NAME)-$(OS).$(EXT)
	$(OS)-*-debug/bin/bit --release configure build
	rm -fr $(OS)-*-debug
	bit install

version:
	@bit -q version
