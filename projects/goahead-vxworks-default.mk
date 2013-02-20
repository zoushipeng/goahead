#
#   goahead-vxworks-default.mk -- Makefile to build Embedthis GoAhead for vxworks
#

export WIND_BASE := $(WIND_BASE)
export WIND_HOME := $(WIND_BASE)/..
export WIND_PLATFORM := $(WIND_PLATFORM)

PRODUCT         := goahead
VERSION         := 3.1.0
BUILD_NUMBER    := 1
PROFILE         := default
ARCH            := $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/')
OS              := vxworks
CC              := ccpentium
LD              := /usr/bin/ld
CONFIG          := $(OS)-$(ARCH)-$(PROFILE)
LBIN            := $(CONFIG)/bin

BIT_ROOT_PREFIX       := deploy
BIT_BASE_PREFIX       := $(BIT_ROOT_PREFIX)
BIT_DATA_PREFIX       := $(BIT_VAPP_PREFIX)
BIT_STATE_PREFIX      := $(BIT_VAPP_PREFIX)
BIT_BIN_PREFIX        := $(BIT_VAPP_PREFIX)
BIT_INC_PREFIX        := $(BIT_VAPP_PREFIX)/inc
BIT_LIB_PREFIX        := $(BIT_VAPP_PREFIX)
BIT_MAN_PREFIX        := $(BIT_VAPP_PREFIX)
BIT_SBIN_PREFIX       := $(BIT_VAPP_PREFIX)
BIT_ETC_PREFIX        := $(BIT_VAPP_PREFIX)
BIT_WEB_PREFIX        := $(BIT_VAPP_PREFIX)/web
BIT_LOG_PREFIX        := $(BIT_VAPP_PREFIX)
BIT_SPOOL_PREFIX      := $(BIT_VAPP_PREFIX)
BIT_CACHE_PREFIX      := $(BIT_VAPP_PREFIX)
BIT_APP_PREFIX        := $(BIT_BASE_PREFIX)
BIT_VAPP_PREFIX       := $(BIT_APP_PREFIX)
BIT_SRC_PREFIX        := $(BIT_ROOT_PREFIX)/usr/src/$(PRODUCT)-$(VERSION)

CFLAGS          += -fno-builtin -fno-defer-pop -fvolatile -w
DFLAGS          += -D_REENTRANT -DVXWORKS -DRW_MULTI_THREAD -D_GNU_TOOL -DCPU=PENTIUM $(patsubst %,-D%,$(filter BIT_%,$(MAKEFLAGS)))
IFLAGS          += -I$(CONFIG)/inc -I$(WIND_BASE)/target/h -I$(WIND_BASE)/target/h/wrn/coreip
LDFLAGS         += '-Wl,-r'
LIBPATHS        += -L$(CONFIG)/bin
LIBS            += 

DEBUG           := debug
CFLAGS-debug    := -g
DFLAGS-debug    := -DBIT_DEBUG
LDFLAGS-debug   := -g
DFLAGS-release  := 
CFLAGS-release  := -O2
LDFLAGS-release := 
CFLAGS          += $(CFLAGS-$(DEBUG))
DFLAGS          += $(DFLAGS-$(DEBUG))
LDFLAGS         += $(LDFLAGS-$(DEBUG))

unexport CDPATH

all compile: prep \
        $(CONFIG)/bin/libest.out \
        $(CONFIG)/bin/ca.crt \
        $(CONFIG)/bin/libgo.out \
        $(CONFIG)/bin/goahead.out \
        $(CONFIG)/bin/goahead-test.out \
        $(CONFIG)/bin/gopass.out

.PHONY: prep

prep:
	@if [ "$(CONFIG)" = "" ] ; then echo WARNING: CONFIG not set ; exit 255 ; fi
	@if [ "$(BIT_APP_PREFIX)" = "" ] ; then echo WARNING: BIT_APP_PREFIX not set ; exit 255 ; fi
	@[ ! -x $(CONFIG)/bin ] && mkdir -p $(CONFIG)/bin; true
	@[ ! -x $(CONFIG)/inc ] && mkdir -p $(CONFIG)/inc; true
	@[ ! -x $(CONFIG)/obj ] && mkdir -p $(CONFIG)/obj; true
	@[ ! -f $(CONFIG)/inc/bit.h ] && cp projects/goahead-vxworks-default-bit.h $(CONFIG)/inc/bit.h ; true
	@[ ! -f $(CONFIG)/inc/bitos.h ] && cp src/bitos.h $(CONFIG)/inc/bitos.h ; true
	@if ! diff $(CONFIG)/inc/bit.h projects/goahead-vxworks-default-bit.h >/dev/null ; then\
		echo cp projects/goahead-vxworks-default-bit.h $(CONFIG)/inc/bit.h  ; \
		cp projects/goahead-vxworks-default-bit.h $(CONFIG)/inc/bit.h  ; \
	fi; true

clean:
	rm -rf $(CONFIG)/bin/libest.out
	rm -rf $(CONFIG)/bin/ca.crt
	rm -rf $(CONFIG)/bin/libgo.out
	rm -rf $(CONFIG)/bin/goahead.out
	rm -rf $(CONFIG)/bin/goahead-test.out
	rm -rf $(CONFIG)/bin/gopass.out
	rm -rf $(CONFIG)/obj/estLib.o
	rm -rf $(CONFIG)/obj/action.o
	rm -rf $(CONFIG)/obj/alloc.o
	rm -rf $(CONFIG)/obj/auth.o
	rm -rf $(CONFIG)/obj/cgi.o
	rm -rf $(CONFIG)/obj/crypt.o
	rm -rf $(CONFIG)/obj/file.o
	rm -rf $(CONFIG)/obj/fs.o
	rm -rf $(CONFIG)/obj/goahead.o
	rm -rf $(CONFIG)/obj/http.o
	rm -rf $(CONFIG)/obj/js.o
	rm -rf $(CONFIG)/obj/jst.o
	rm -rf $(CONFIG)/obj/options.o
	rm -rf $(CONFIG)/obj/osdep.o
	rm -rf $(CONFIG)/obj/rom-documents.o
	rm -rf $(CONFIG)/obj/route.o
	rm -rf $(CONFIG)/obj/runtime.o
	rm -rf $(CONFIG)/obj/socket.o
	rm -rf $(CONFIG)/obj/upload.o
	rm -rf $(CONFIG)/obj/est.o
	rm -rf $(CONFIG)/obj/matrixssl.o
	rm -rf $(CONFIG)/obj/openssl.o
	rm -rf $(CONFIG)/obj/test.o
	rm -rf $(CONFIG)/obj/gopass.o

clobber: clean
	rm -fr ./$(CONFIG)

$(CONFIG)/inc/est.h: 
	mkdir -p "$(CONFIG)/inc"
	cp "src/deps/est/est.h" "$(CONFIG)/inc/est.h"

$(CONFIG)/inc/bit.h: 

$(CONFIG)/inc/bitos.h: \
    $(CONFIG)/inc/bit.h
	mkdir -p "$(CONFIG)/inc"
	cp "src/bitos.h" "$(CONFIG)/inc/bitos.h"

$(CONFIG)/obj/estLib.o: \
    src/deps/est/estLib.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/est.h \
    $(CONFIG)/inc/bitos.h
	$(CC) -c -o $(CONFIG)/obj/estLib.o -fno-builtin -fno-defer-pop -fvolatile $(DFLAGS) $(IFLAGS) src/deps/est/estLib.c

$(CONFIG)/bin/libest.out: \
    $(CONFIG)/inc/est.h \
    $(CONFIG)/obj/estLib.o
	$(CC) -r -o $(CONFIG)/bin/libest.out $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/estLib.o 

$(CONFIG)/bin/ca.crt: \
    src/deps/est/ca.crt
	mkdir -p "$(CONFIG)/bin"
	cp "src/deps/est/ca.crt" "$(CONFIG)/bin/ca.crt"

$(CONFIG)/inc/goahead.h: 
	mkdir -p "$(CONFIG)/inc"
	cp "src/goahead.h" "$(CONFIG)/inc/goahead.h"

$(CONFIG)/inc/js.h: 
	mkdir -p "$(CONFIG)/inc"
	cp "src/js.h" "$(CONFIG)/inc/js.h"

$(CONFIG)/obj/action.o: \
    src/action.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h \
    $(CONFIG)/inc/bitos.h
	$(CC) -c -o $(CONFIG)/obj/action.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/action.c

$(CONFIG)/obj/alloc.o: \
    src/alloc.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/alloc.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/alloc.c

$(CONFIG)/obj/auth.o: \
    src/auth.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/auth.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/auth.c

$(CONFIG)/obj/cgi.o: \
    src/cgi.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/cgi.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/cgi.c

$(CONFIG)/obj/crypt.o: \
    src/crypt.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/crypt.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/crypt.c

$(CONFIG)/obj/file.o: \
    src/file.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/file.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/file.c

$(CONFIG)/obj/fs.o: \
    src/fs.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/fs.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/fs.c

$(CONFIG)/obj/goahead.o: \
    src/goahead.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h \
    $(CONFIG)/inc/bitos.h
	$(CC) -c -o $(CONFIG)/obj/goahead.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/goahead.c

$(CONFIG)/obj/http.o: \
    src/http.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/http.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/http.c

$(CONFIG)/obj/js.o: \
    src/js.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/js.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/js.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/js.c

$(CONFIG)/obj/jst.o: \
    src/jst.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h \
    $(CONFIG)/inc/js.h
	$(CC) -c -o $(CONFIG)/obj/jst.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/jst.c

$(CONFIG)/obj/options.o: \
    src/options.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/options.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/options.c

$(CONFIG)/obj/osdep.o: \
    src/osdep.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/osdep.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/osdep.c

$(CONFIG)/obj/rom-documents.o: \
    src/rom-documents.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/rom-documents.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/rom-documents.c

$(CONFIG)/obj/route.o: \
    src/route.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/route.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/route.c

$(CONFIG)/obj/runtime.o: \
    src/runtime.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/runtime.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/runtime.c

$(CONFIG)/obj/socket.o: \
    src/socket.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/socket.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/socket.c

$(CONFIG)/obj/upload.o: \
    src/upload.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/upload.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/upload.c

src/deps/est/est.h: 

$(CONFIG)/obj/est.o: \
    src/ssl/est.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h \
    src/deps/est/est.h \
    $(CONFIG)/inc/bitos.h
	$(CC) -c -o $(CONFIG)/obj/est.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/ssl/est.c

$(CONFIG)/obj/matrixssl.o: \
    src/ssl/matrixssl.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/matrixssl.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/ssl/matrixssl.c

$(CONFIG)/obj/openssl.o: \
    src/ssl/openssl.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/bitos.h \
    $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/openssl.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/ssl/openssl.c

$(CONFIG)/bin/libgo.out: \
    $(CONFIG)/inc/bitos.h \
    $(CONFIG)/inc/goahead.h \
    $(CONFIG)/inc/js.h \
    $(CONFIG)/obj/action.o \
    $(CONFIG)/obj/alloc.o \
    $(CONFIG)/obj/auth.o \
    $(CONFIG)/obj/cgi.o \
    $(CONFIG)/obj/crypt.o \
    $(CONFIG)/obj/file.o \
    $(CONFIG)/obj/fs.o \
    $(CONFIG)/obj/goahead.o \
    $(CONFIG)/obj/http.o \
    $(CONFIG)/obj/js.o \
    $(CONFIG)/obj/jst.o \
    $(CONFIG)/obj/options.o \
    $(CONFIG)/obj/osdep.o \
    $(CONFIG)/obj/rom-documents.o \
    $(CONFIG)/obj/route.o \
    $(CONFIG)/obj/runtime.o \
    $(CONFIG)/obj/socket.o \
    $(CONFIG)/obj/upload.o \
    $(CONFIG)/obj/est.o \
    $(CONFIG)/obj/matrixssl.o \
    $(CONFIG)/obj/openssl.o
	$(CC) -r -o $(CONFIG)/bin/libgo.out $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/action.o $(CONFIG)/obj/alloc.o $(CONFIG)/obj/auth.o $(CONFIG)/obj/cgi.o $(CONFIG)/obj/crypt.o $(CONFIG)/obj/file.o $(CONFIG)/obj/fs.o $(CONFIG)/obj/goahead.o $(CONFIG)/obj/http.o $(CONFIG)/obj/js.o $(CONFIG)/obj/jst.o $(CONFIG)/obj/options.o $(CONFIG)/obj/osdep.o $(CONFIG)/obj/rom-documents.o $(CONFIG)/obj/route.o $(CONFIG)/obj/runtime.o $(CONFIG)/obj/socket.o $(CONFIG)/obj/upload.o $(CONFIG)/obj/est.o $(CONFIG)/obj/matrixssl.o $(CONFIG)/obj/openssl.o 

$(CONFIG)/bin/goahead.out: \
    $(CONFIG)/bin/libgo.out \
    $(CONFIG)/inc/bitos.h \
    $(CONFIG)/inc/goahead.h \
    $(CONFIG)/inc/js.h \
    $(CONFIG)/obj/goahead.o
	$(CC) -o $(CONFIG)/bin/goahead.out $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/goahead.o $(LDFLAGS)

$(CONFIG)/obj/test.o: \
    test/test.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h \
    $(CONFIG)/inc/js.h \
    $(CONFIG)/inc/bitos.h
	$(CC) -c -o $(CONFIG)/obj/test.o $(CFLAGS) $(DFLAGS) $(IFLAGS) test/test.c

$(CONFIG)/bin/goahead-test.out: \
    $(CONFIG)/bin/libgo.out \
    $(CONFIG)/inc/bitos.h \
    $(CONFIG)/inc/goahead.h \
    $(CONFIG)/inc/js.h \
    $(CONFIG)/obj/test.o
	$(CC) -o $(CONFIG)/bin/goahead-test.out $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/test.o $(LDFLAGS)

$(CONFIG)/obj/gopass.o: \
    src/utils/gopass.c\
    $(CONFIG)/inc/bit.h \
    $(CONFIG)/inc/goahead.h \
    $(CONFIG)/inc/bitos.h
	$(CC) -c -o $(CONFIG)/obj/gopass.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/utils/gopass.c

$(CONFIG)/bin/gopass.out: \
    $(CONFIG)/bin/libgo.out \
    $(CONFIG)/inc/bitos.h \
    $(CONFIG)/inc/goahead.h \
    $(CONFIG)/inc/js.h \
    $(CONFIG)/obj/gopass.o
	$(CC) -o $(CONFIG)/bin/gopass.out $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/gopass.o $(LDFLAGS)

version: 
	@echo 3.1.0-1

stop: 
	

installBinary: stop


start: 
	

install: stop installBinary start
	

uninstall: stop


