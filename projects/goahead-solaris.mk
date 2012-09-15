#
#   goahead-solaris.mk -- Makefile to build Embedthis GoAhead for solaris
#

ARCH     ?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')
OS       ?= solaris
CC       ?= gcc
LD       ?= /usr/bin/ld
PROFILE  ?= debug
CONFIG   ?= $(OS)-$(ARCH)-$(PROFILE)

CFLAGS   += -fPIC -mtune=generic -w
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
        $(CONFIG)/bin/goahead \
        $(CONFIG)/bin/goahead-test \
        $(CONFIG)/bin/webcomp \
        test/cgi-bin/cgitest

.PHONY: prep

prep:
	@if [ "$(CONFIG)" = "" ] ; then echo WARNING: CONFIG not set ; exit 255 ; fi
	@[ ! -x $(CONFIG)/inc ] && mkdir -p $(CONFIG)/inc $(CONFIG)/obj $(CONFIG)/lib $(CONFIG)/bin ; true
	@[ ! -f $(CONFIG)/inc/bit.h ] && cp projects/goahead-$(OS)-bit.h $(CONFIG)/inc/bit.h ; true
	@if ! diff $(CONFIG)/inc/bit.h projects/goahead-$(OS)-bit.h >/dev/null ; then\
		echo cp projects/goahead-$(OS)-bit.h $(CONFIG)/inc/bit.h  ; \
		cp projects/goahead-$(OS)-bit.h $(CONFIG)/inc/bit.h  ; \
	fi; true

clean:
	rm -rf $(CONFIG)/bin/goahead
	rm -rf $(CONFIG)/bin/goahead-test
	rm -rf $(CONFIG)/bin/webcomp
	rm -rf test/cgi-bin/cgitest
	rm -rf $(CONFIG)/obj/auth.o
	rm -rf $(CONFIG)/obj/cgi.o
	rm -rf $(CONFIG)/obj/crypt.o
	rm -rf $(CONFIG)/obj/file.o
	rm -rf $(CONFIG)/obj/form.o
	rm -rf $(CONFIG)/obj/galloc.o
	rm -rf $(CONFIG)/obj/goahead.o
	rm -rf $(CONFIG)/obj/handler.o
	rm -rf $(CONFIG)/obj/http.o
	rm -rf $(CONFIG)/obj/js.o
	rm -rf $(CONFIG)/obj/matrixssl.o
	rm -rf $(CONFIG)/obj/openssl.o
	rm -rf $(CONFIG)/obj/rom-documents.o
	rm -rf $(CONFIG)/obj/rom.o
	rm -rf $(CONFIG)/obj/route.o
	rm -rf $(CONFIG)/obj/runtime.o
	rm -rf $(CONFIG)/obj/socket.o
	rm -rf $(CONFIG)/obj/ssl.o
	rm -rf $(CONFIG)/obj/template.o
	rm -rf $(CONFIG)/obj/upload.o
	rm -rf $(CONFIG)/obj/test.o
	rm -rf $(CONFIG)/obj/webcomp.o
	rm -rf $(CONFIG)/obj/cgitest.o

clobber: clean
	rm -fr ./$(CONFIG)

$(CONFIG)/inc/goahead.h: 
	rm -fr $(CONFIG)/inc/goahead.h
	cp -r goahead.h $(CONFIG)/inc/goahead.h

$(CONFIG)/inc/js.h: 
	rm -fr $(CONFIG)/inc/js.h
	cp -r js.h $(CONFIG)/inc/js.h

$(CONFIG)/obj/auth.o: \
        auth.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/auth.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc auth.c

$(CONFIG)/obj/cgi.o: \
        cgi.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/cgi.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc cgi.c

$(CONFIG)/obj/crypt.o: \
        crypt.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/crypt.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc crypt.c

$(CONFIG)/obj/file.o: \
        file.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/file.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc file.c

$(CONFIG)/obj/form.o: \
        form.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/form.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc form.c

$(CONFIG)/obj/galloc.o: \
        galloc.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/galloc.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc galloc.c

$(CONFIG)/obj/goahead.o: \
        goahead.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/goahead.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc goahead.c

$(CONFIG)/obj/handler.o: \
        handler.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/handler.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc handler.c

$(CONFIG)/obj/http.o: \
        http.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/http.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc http.c

$(CONFIG)/obj/js.o: \
        js.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/js.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc js.c

$(CONFIG)/obj/matrixssl.o: \
        matrixssl.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/matrixssl.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc matrixssl.c

$(CONFIG)/obj/openssl.o: \
        openssl.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/openssl.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc openssl.c

$(CONFIG)/obj/rom-documents.o: \
        rom-documents.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/rom-documents.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc rom-documents.c

$(CONFIG)/obj/rom.o: \
        rom.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/rom.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc rom.c

$(CONFIG)/obj/route.o: \
        route.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/route.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc route.c

$(CONFIG)/obj/runtime.o: \
        runtime.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/runtime.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc runtime.c

$(CONFIG)/obj/socket.o: \
        socket.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/socket.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc socket.c

$(CONFIG)/obj/ssl.o: \
        ssl.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/ssl.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc ssl.c

$(CONFIG)/obj/template.o: \
        template.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/template.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc template.c

$(CONFIG)/obj/upload.o: \
        upload.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/upload.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc upload.c

$(CONFIG)/bin/goahead:  \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/auth.o \
        $(CONFIG)/obj/cgi.o \
        $(CONFIG)/obj/crypt.o \
        $(CONFIG)/obj/file.o \
        $(CONFIG)/obj/form.o \
        $(CONFIG)/obj/galloc.o \
        $(CONFIG)/obj/goahead.o \
        $(CONFIG)/obj/handler.o \
        $(CONFIG)/obj/http.o \
        $(CONFIG)/obj/js.o \
        $(CONFIG)/obj/matrixssl.o \
        $(CONFIG)/obj/openssl.o \
        $(CONFIG)/obj/rom-documents.o \
        $(CONFIG)/obj/rom.o \
        $(CONFIG)/obj/route.o \
        $(CONFIG)/obj/runtime.o \
        $(CONFIG)/obj/socket.o \
        $(CONFIG)/obj/ssl.o \
        $(CONFIG)/obj/template.o \
        $(CONFIG)/obj/upload.o
	$(CC) -o $(CONFIG)/bin/goahead $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/auth.o $(CONFIG)/obj/cgi.o $(CONFIG)/obj/crypt.o $(CONFIG)/obj/file.o $(CONFIG)/obj/form.o $(CONFIG)/obj/galloc.o $(CONFIG)/obj/goahead.o $(CONFIG)/obj/handler.o $(CONFIG)/obj/http.o $(CONFIG)/obj/js.o $(CONFIG)/obj/matrixssl.o $(CONFIG)/obj/openssl.o $(CONFIG)/obj/rom-documents.o $(CONFIG)/obj/rom.o $(CONFIG)/obj/route.o $(CONFIG)/obj/runtime.o $(CONFIG)/obj/socket.o $(CONFIG)/obj/ssl.o $(CONFIG)/obj/template.o $(CONFIG)/obj/upload.o $(LIBS) $(LDFLAGS)

$(CONFIG)/obj/test.o: \
        test/test.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/test.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc test/test.c

$(CONFIG)/bin/goahead-test:  \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/auth.o \
        $(CONFIG)/obj/cgi.o \
        $(CONFIG)/obj/crypt.o \
        $(CONFIG)/obj/file.o \
        $(CONFIG)/obj/form.o \
        $(CONFIG)/obj/galloc.o \
        $(CONFIG)/obj/handler.o \
        $(CONFIG)/obj/http.o \
        $(CONFIG)/obj/js.o \
        $(CONFIG)/obj/matrixssl.o \
        $(CONFIG)/obj/openssl.o \
        $(CONFIG)/obj/rom-documents.o \
        $(CONFIG)/obj/rom.o \
        $(CONFIG)/obj/route.o \
        $(CONFIG)/obj/runtime.o \
        $(CONFIG)/obj/socket.o \
        $(CONFIG)/obj/ssl.o \
        $(CONFIG)/obj/template.o \
        $(CONFIG)/obj/upload.o \
        $(CONFIG)/obj/test.o
	$(CC) -o $(CONFIG)/bin/goahead-test $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/auth.o $(CONFIG)/obj/cgi.o $(CONFIG)/obj/crypt.o $(CONFIG)/obj/file.o $(CONFIG)/obj/form.o $(CONFIG)/obj/galloc.o $(CONFIG)/obj/handler.o $(CONFIG)/obj/http.o $(CONFIG)/obj/js.o $(CONFIG)/obj/matrixssl.o $(CONFIG)/obj/openssl.o $(CONFIG)/obj/rom-documents.o $(CONFIG)/obj/rom.o $(CONFIG)/obj/route.o $(CONFIG)/obj/runtime.o $(CONFIG)/obj/socket.o $(CONFIG)/obj/ssl.o $(CONFIG)/obj/template.o $(CONFIG)/obj/upload.o $(CONFIG)/obj/test.o $(LIBS) $(LDFLAGS)

$(CONFIG)/obj/webcomp.o: \
        utils/webcomp.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/webcomp.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc utils/webcomp.c

$(CONFIG)/bin/webcomp:  \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/webcomp.o
	$(CC) -o $(CONFIG)/bin/webcomp $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/webcomp.o $(LIBS) $(LDFLAGS)

$(CONFIG)/obj/cgitest.o: \
        test/cgitest.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/cgitest.o -Wall -fPIC $(LDFLAGS) -mtune=generic $(DFLAGS) -I$(CONFIG)/inc test/cgitest.c

test/cgi-bin/cgitest:  \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/cgitest.o
	$(CC) -o test/cgi-bin/cgitest $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/cgitest.o $(LIBS) $(LDFLAGS)

