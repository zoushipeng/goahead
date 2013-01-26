#
#   goahead-solaris-static.mk -- Makefile to build Embedthis GoAhead for solaris
#

PRODUCT         ?= goahead
VERSION         ?= 3.1.0
BUILD_NUMBER    ?= 1
PROFILE         ?= static
ARCH            ?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/')
OS              ?= solaris
CC              ?= /usr/bin/gcc
LD              ?= /usr/bin/ld
CONFIG          ?= $(OS)-$(ARCH)-$(PROFILE)

CFLAGS          += -fPIC -O2 -w
DFLAGS          += -D_REENTRANT -DPIC$(patsubst %,-D%,$(filter BIT_%,$(MAKEFLAGS)))
IFLAGS          += -I$(CONFIG)/inc
LDFLAGS         += 
LIBPATHS        += -L$(CONFIG)/bin
LIBS            += -llxnet -lrt -lsocket -lpthread -lm -ldl

DEBUG           ?= release
CFLAGS-debug    := -g
CFLAGS-release  := -O2
DFLAGS-debug    := -DBIT_DEBUG
DFLAGS-release  := 
LDFLAGS-debug   := -g
LDFLAGS-release := 
CFLAGS          += $(CFLAGS-$(PROFILE))
DFLAGS          += $(DFLAGS-$(PROFILE))
LDFLAGS         += $(LDFLAGS-$(PROFILE))

all compile: prep \
        $(CONFIG)/bin/libest.a \
        $(CONFIG)/bin/ca.crt \
        $(CONFIG)/bin/libgo.a \
        $(CONFIG)/bin/goahead \
        $(CONFIG)/bin/goahead-test

.PHONY: prep

prep:
	@if [ "$(CONFIG)" = "" ] ; then echo WARNING: CONFIG not set ; exit 255 ; fi
	@[ ! -x $(CONFIG)/inc ] && mkdir -p $(CONFIG)/inc $(CONFIG)/obj $(CONFIG)/lib $(CONFIG)/bin ; true
	@[ ! -f $(CONFIG)/inc/bit.h ] && cp projects/goahead-$(OS)-$(PROFILE)-bit.h $(CONFIG)/inc/bit.h ; true
	@[ ! -f $(CONFIG)/inc/bitos.h ] && cp src/bitos.h $(CONFIG)/inc/bitos.h ; true
	@if ! diff $(CONFIG)/inc/bit.h projects/goahead-$(OS)-$(PROFILE)-bit.h >/dev/null ; then\
		echo cp projects/goahead-$(OS)-$(PROFILE)-bit.h $(CONFIG)/inc/bit.h  ; \
		cp projects/goahead-$(OS)-$(PROFILE)-bit.h $(CONFIG)/inc/bit.h  ; \
	fi; true
	@echo $(DFLAGS) $(CFLAGS) >projects/.flags

clean:
	rm -rf $(CONFIG)/bin/libest.a
	rm -rf $(CONFIG)/bin/ca.crt
	rm -rf $(CONFIG)/bin/libgo.a
	rm -rf $(CONFIG)/bin/goahead
	rm -rf $(CONFIG)/bin/goahead-test
	rm -rf $(CONFIG)/obj/estLib.o
	rm -rf $(CONFIG)/obj/action.o
	rm -rf $(CONFIG)/obj/alloc.o
	rm -rf $(CONFIG)/obj/auth.o
	rm -rf $(CONFIG)/obj/cgi.o
	rm -rf $(CONFIG)/obj/crypt.o
	rm -rf $(CONFIG)/obj/file.o
	rm -rf $(CONFIG)/obj/fs.o
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
	rm -rf $(CONFIG)/obj/goahead.o
	rm -rf $(CONFIG)/obj/test.o
	rm -rf $(CONFIG)/obj/gopass.o
	rm -rf $(CONFIG)/obj/webcomp.o
	rm -rf $(CONFIG)/obj/cgitest.o

clobber: clean
	rm -fr ./$(CONFIG)

$(CONFIG)/inc/bitos.h: 
	rm -fr $(CONFIG)/inc/bitos.h
	cp -r src/bitos.h $(CONFIG)/inc/bitos.h

$(CONFIG)/inc/est.h:  \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/bitos.h
	rm -fr $(CONFIG)/inc/est.h
	cp -r src/deps/est/est.h $(CONFIG)/inc/est.h

$(CONFIG)/obj/estLib.o: \
        src/deps/est/estLib.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/est.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/estLib.o -fPIC -O2 $(DFLAGS) -I$(CONFIG)/inc src/deps/est/estLib.c

$(CONFIG)/bin/libest.a:  \
        $(CONFIG)/inc/est.h \
        $(CONFIG)/obj/estLib.o
	$(LDFLAGS)$(LDFLAGS)/usr/bin/ar -cr $(CONFIG)/bin/libest.a $(CONFIG)/obj/estLib.o

$(CONFIG)/bin/ca.crt: 
	rm -fr $(CONFIG)/bin/ca.crt
	cp -r src/deps/est/ca.crt $(CONFIG)/bin/ca.crt

$(CONFIG)/inc/goahead.h: 
	rm -fr $(CONFIG)/inc/goahead.h
	cp -r src/goahead.h $(CONFIG)/inc/goahead.h

$(CONFIG)/inc/js.h: 
	rm -fr $(CONFIG)/inc/js.h
	cp -r src/js.h $(CONFIG)/inc/js.h

$(CONFIG)/obj/action.o: \
        src/action.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/action.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/action.c

$(CONFIG)/obj/alloc.o: \
        src/alloc.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/alloc.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/alloc.c

$(CONFIG)/obj/auth.o: \
        src/auth.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/auth.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/auth.c

$(CONFIG)/obj/cgi.o: \
        src/cgi.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/cgi.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/cgi.c

$(CONFIG)/obj/crypt.o: \
        src/crypt.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/crypt.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/crypt.c

$(CONFIG)/obj/file.o: \
        src/file.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/file.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/file.c

$(CONFIG)/obj/fs.o: \
        src/fs.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/fs.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/fs.c

$(CONFIG)/obj/http.o: \
        src/http.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/http.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/http.c

$(CONFIG)/obj/js.o: \
        src/js.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/js.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/js.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/js.c

$(CONFIG)/obj/jst.o: \
        src/jst.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/jst.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/jst.c

$(CONFIG)/obj/options.o: \
        src/options.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/options.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/options.c

$(CONFIG)/obj/osdep.o: \
        src/osdep.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/osdep.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/osdep.c

$(CONFIG)/obj/rom-documents.o: \
        src/rom-documents.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/rom-documents.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/rom-documents.c

$(CONFIG)/obj/route.o: \
        src/route.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/route.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/route.c

$(CONFIG)/obj/runtime.o: \
        src/runtime.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/runtime.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/runtime.c

$(CONFIG)/obj/socket.o: \
        src/socket.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/socket.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/socket.c

$(CONFIG)/obj/upload.o: \
        src/upload.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/upload.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/upload.c

$(CONFIG)/obj/est.o: \
        src/ssl/est.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h \
        src/deps/est/est.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/est.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/ssl/est.c

$(CONFIG)/obj/matrixssl.o: \
        src/ssl/matrixssl.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/matrixssl.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/ssl/matrixssl.c

$(CONFIG)/obj/openssl.o: \
        src/ssl/openssl.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/bitos.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/openssl.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc/deps/est src/ssl/openssl.c

$(CONFIG)/bin/libgo.a:  \
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
	$(LDFLAGS)$(LDFLAGS)/usr/bin/ar -cr $(CONFIG)/bin/libgo.a $(CONFIG)/obj/action.o $(CONFIG)/obj/alloc.o $(CONFIG)/obj/auth.o $(CONFIG)/obj/cgi.o $(CONFIG)/obj/crypt.o $(CONFIG)/obj/file.o $(CONFIG)/obj/fs.o $(CONFIG)/obj/http.o $(CONFIG)/obj/js.o $(CONFIG)/obj/jst.o $(CONFIG)/obj/options.o $(CONFIG)/obj/osdep.o $(CONFIG)/obj/rom-documents.o $(CONFIG)/obj/route.o $(CONFIG)/obj/runtime.o $(CONFIG)/obj/socket.o $(CONFIG)/obj/upload.o $(CONFIG)/obj/est.o $(CONFIG)/obj/matrixssl.o $(CONFIG)/obj/openssl.o

$(CONFIG)/obj/goahead.o: \
        src/goahead.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/goahead.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/goahead.c

$(CONFIG)/bin/goahead:  \
        $(CONFIG)/bin/libgo.a \
        $(CONFIG)/inc/bitos.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/goahead.o
	$(LDFLAGS)$(LDFLAGS)$(CC) -o $(CONFIG)/bin/goahead $(LIBPATHS) $(CONFIG)/obj/goahead.o -lgo $(LIBS) -lest -lgo -llxnet -lrt -lsocket -lpthread -lm -ldl -lest 

$(CONFIG)/obj/test.o: \
        test/test.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/test.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc test/test.c

$(CONFIG)/bin/goahead-test:  \
        $(CONFIG)/bin/libgo.a \
        $(CONFIG)/inc/bitos.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/test.o
	$(LDFLAGS)$(LDFLAGS)$(CC) -o $(CONFIG)/bin/goahead-test $(LIBPATHS) $(CONFIG)/obj/test.o -lgo $(LIBS) -lest -lgo -llxnet -lrt -lsocket -lpthread -lm -ldl -lest 

version: 
	@echo 3.1.0-1 

