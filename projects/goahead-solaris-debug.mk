#
#   goahead-solaris-debug.mk -- Makefile to build Embedthis GoAhead for solaris
#

ARCH     ?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/')
OS       ?= solaris
CC       ?= /usr/bin/gcc
LD       ?= /usr/bin/ld
PROFILE  ?= debug
CONFIG   ?= $(OS)-$(ARCH)-$(PROFILE)

CFLAGS   += -fPIC  -w
DFLAGS   += -D_REENTRANT -DPIC 
IFLAGS   += -I$(CONFIG)/inc
LDFLAGS  += '-g'
LIBPATHS += -L$(CONFIG)/bin
LIBS     += -llxnet -lrt -lsocket -lpthread -lm -ldl

CFLAGS-debug    := -DBIT_DEBUG -g
CFLAGS-release  := -O2
LDFLAGS-debug   := -g
LDFLAGS-release := 
CFLAGS          += $(CFLAGS-$(PROFILE))
LDFLAGS         += $(LDFLAGS-$(PROFILE))

all: prep \
        $(CONFIG)/bin/libgo.so \
        $(CONFIG)/bin/goahead \
        $(CONFIG)/bin/goahead-test \
        $(CONFIG)/bin/gopass \
        $(CONFIG)/bin/webcomp \
        test/cgi-bin/cgitest

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

clean:
	rm -rf $(CONFIG)/bin/libgo.so
	rm -rf $(CONFIG)/bin/goahead
	rm -rf $(CONFIG)/bin/goahead-test
	rm -rf $(CONFIG)/bin/gopass
	rm -rf $(CONFIG)/bin/webcomp
	rm -rf test/cgi-bin/cgitest
	rm -rf $(CONFIG)/obj/action.o
	rm -rf $(CONFIG)/obj/alloc.o
	rm -rf $(CONFIG)/obj/auth.o
	rm -rf $(CONFIG)/obj/cgi.o
	rm -rf $(CONFIG)/obj/crypt.o
	rm -rf $(CONFIG)/obj/file.o
	rm -rf $(CONFIG)/obj/http.o
	rm -rf $(CONFIG)/obj/js.o
	rm -rf $(CONFIG)/obj/jst.o
	rm -rf $(CONFIG)/obj/options.o
	rm -rf $(CONFIG)/obj/rom-documents.o
	rm -rf $(CONFIG)/obj/rom.o
	rm -rf $(CONFIG)/obj/route.o
	rm -rf $(CONFIG)/obj/runtime.o
	rm -rf $(CONFIG)/obj/socket.o
	rm -rf $(CONFIG)/obj/upload.o
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
	$(CC) -c -o $(CONFIG)/obj/action.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/action.c

$(CONFIG)/obj/alloc.o: \
        src/alloc.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/alloc.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/alloc.c

$(CONFIG)/obj/auth.o: \
        src/auth.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/auth.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/auth.c

$(CONFIG)/obj/cgi.o: \
        src/cgi.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/cgi.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/cgi.c

$(CONFIG)/obj/crypt.o: \
        src/crypt.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/crypt.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/crypt.c

$(CONFIG)/obj/file.o: \
        src/file.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/file.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/file.c

$(CONFIG)/obj/http.o: \
        src/http.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/http.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/http.c

$(CONFIG)/obj/js.o: \
        src/js.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/js.h
	$(CC) -c -o $(CONFIG)/obj/js.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/js.c

$(CONFIG)/obj/jst.o: \
        src/jst.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h
	$(CC) -c -o $(CONFIG)/obj/jst.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/jst.c

$(CONFIG)/obj/options.o: \
        src/options.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/options.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/options.c

$(CONFIG)/obj/rom-documents.o: \
        src/rom-documents.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/rom-documents.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/rom-documents.c

$(CONFIG)/obj/rom.o: \
        src/rom.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/rom.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/rom.c

$(CONFIG)/obj/route.o: \
        src/route.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/route.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/route.c

$(CONFIG)/obj/runtime.o: \
        src/runtime.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/runtime.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/runtime.c

$(CONFIG)/obj/socket.o: \
        src/socket.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/socket.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/socket.c

$(CONFIG)/obj/upload.o: \
        src/upload.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/upload.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/upload.c

$(CONFIG)/obj/matrixssl.o: \
        src/ssl/matrixssl.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/matrixssl.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/ssl/matrixssl.c

$(CONFIG)/obj/openssl.o: \
        src/ssl/openssl.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/bitos.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/openssl.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/ssl/openssl.c

$(CONFIG)/bin/libgo.so:  \
        $(CONFIG)/inc/bitos.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/action.o \
        $(CONFIG)/obj/alloc.o \
        $(CONFIG)/obj/auth.o \
        $(CONFIG)/obj/cgi.o \
        $(CONFIG)/obj/crypt.o \
        $(CONFIG)/obj/file.o \
        $(CONFIG)/obj/http.o \
        $(CONFIG)/obj/js.o \
        $(CONFIG)/obj/jst.o \
        $(CONFIG)/obj/options.o \
        $(CONFIG)/obj/rom-documents.o \
        $(CONFIG)/obj/rom.o \
        $(CONFIG)/obj/route.o \
        $(CONFIG)/obj/runtime.o \
        $(CONFIG)/obj/socket.o \
        $(CONFIG)/obj/upload.o \
        $(CONFIG)/obj/matrixssl.o \
        $(CONFIG)/obj/openssl.o
	$(CC) -shared -o $(CONFIG)/bin/libgo.so $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/action.o $(CONFIG)/obj/alloc.o $(CONFIG)/obj/auth.o $(CONFIG)/obj/cgi.o $(CONFIG)/obj/crypt.o $(CONFIG)/obj/file.o $(CONFIG)/obj/http.o $(CONFIG)/obj/js.o $(CONFIG)/obj/jst.o $(CONFIG)/obj/options.o $(CONFIG)/obj/rom-documents.o $(CONFIG)/obj/rom.o $(CONFIG)/obj/route.o $(CONFIG)/obj/runtime.o $(CONFIG)/obj/socket.o $(CONFIG)/obj/upload.o $(CONFIG)/obj/matrixssl.o $(CONFIG)/obj/openssl.o $(LIBS)

$(CONFIG)/obj/goahead.o: \
        src/goahead.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/goahead.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc src/goahead.c

$(CONFIG)/bin/goahead:  \
        $(CONFIG)/bin/libgo.so \
        $(CONFIG)/inc/bitos.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/goahead.o
	$(CC) -o $(CONFIG)/bin/goahead $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/goahead.o -lgo $(LIBS) $(LDFLAGS)

$(CONFIG)/obj/test.o: \
        test/test.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h
	$(CC) -c -o $(CONFIG)/obj/test.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc test/test.c

$(CONFIG)/bin/goahead-test:  \
        $(CONFIG)/bin/libgo.so \
        $(CONFIG)/inc/bitos.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/test.o
	$(CC) -o $(CONFIG)/bin/goahead-test $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/test.o -lgo $(LIBS) $(LDFLAGS)

$(CONFIG)/obj/gopass.o: \
        utils/gopass.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/gopass.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc utils/gopass.c

$(CONFIG)/bin/gopass:  \
        $(CONFIG)/bin/libgo.so \
        $(CONFIG)/inc/bitos.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/gopass.o
	$(CC) -o $(CONFIG)/bin/gopass $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/gopass.o -lgo $(LIBS) $(LDFLAGS)

$(CONFIG)/obj/webcomp.o: \
        utils/webcomp.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/webcomp.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc utils/webcomp.c

$(CONFIG)/bin/webcomp:  \
        $(CONFIG)/inc/bitos.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/webcomp.o
	$(CC) -o $(CONFIG)/bin/webcomp $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/webcomp.o $(LIBS) $(LDFLAGS)

$(CONFIG)/obj/cgitest.o: \
        test/cgitest.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/cgitest.o -fPIC $(LDFLAGS) $(DFLAGS) -I$(CONFIG)/inc test/cgitest.c

test/cgi-bin/cgitest:  \
        $(CONFIG)/inc/bitos.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/cgitest.o
	$(CC) -o test/cgi-bin/cgitest $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/cgitest.o $(LIBS) $(LDFLAGS)

