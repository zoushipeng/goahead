#
#   goahead-macosx.mk -- Makefile to build Embedthis GoAhead for macosx
#

ARCH     ?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')
OS       ?= macosx
CC       ?= /usr/bin/clang
LD       ?= /usr/bin/ld
PROFILE  ?= debug
CONFIG   ?= $(OS)-$(ARCH)-$(PROFILE)

CFLAGS   += -Wno-deprecated-declarations -w
DFLAGS   += 
IFLAGS   += -I$(CONFIG)/inc
LDFLAGS  += '-Wl,-rpath,@executable_path/' '-Wl,-rpath,@loader_path/'
LIBPATHS += -L$(CONFIG)/bin
LIBS     += -lpthread -lm -ldl

CFLAGS-debug    := -DBIT_DEBUG -g
CFLAGS-release  := -O2
LDFLAGS-debug   := -g
LDFLAGS-release := 
CFLAGS          += $(CFLAGS-$(PROFILE))
LDFLAGS         += $(LDFLAGS-$(PROFILE))

all: prep \
        $(CONFIG)/bin/libgo.a \
        $(CONFIG)/bin/goahead \
        $(CONFIG)/bin/goahead-test \
        $(CONFIG)/bin/gopass \
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
	rm -rf $(CONFIG)/bin/libgo.a
	rm -rf $(CONFIG)/bin/goahead
	rm -rf $(CONFIG)/bin/goahead-test
	rm -rf $(CONFIG)/bin/gopass
	rm -rf $(CONFIG)/bin/webcomp
	rm -rf test/cgi-bin/cgitest
	rm -rf $(CONFIG)/obj/auth.o
	rm -rf $(CONFIG)/obj/cgi.o
	rm -rf $(CONFIG)/obj/crypt.o
	rm -rf $(CONFIG)/obj/file.o
	rm -rf $(CONFIG)/obj/galloc.o
	rm -rf $(CONFIG)/obj/http.o
	rm -rf $(CONFIG)/obj/js.o
	rm -rf $(CONFIG)/obj/matrixssl.o
	rm -rf $(CONFIG)/obj/openssl.o
	rm -rf $(CONFIG)/obj/options.o
	rm -rf $(CONFIG)/obj/proc.o
	rm -rf $(CONFIG)/obj/rom-documents.o
	rm -rf $(CONFIG)/obj/rom.o
	rm -rf $(CONFIG)/obj/route.o
	rm -rf $(CONFIG)/obj/runtime.o
	rm -rf $(CONFIG)/obj/socket.o
	rm -rf $(CONFIG)/obj/template.o
	rm -rf $(CONFIG)/obj/upload.o
	rm -rf $(CONFIG)/obj/goahead.o
	rm -rf $(CONFIG)/obj/test.o
	rm -rf $(CONFIG)/obj/gopass.o
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
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/auth.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc auth.c

$(CONFIG)/obj/cgi.o: \
        cgi.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/cgi.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc cgi.c

$(CONFIG)/obj/crypt.o: \
        crypt.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/crypt.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc crypt.c

$(CONFIG)/obj/file.o: \
        file.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/file.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc file.c

$(CONFIG)/obj/galloc.o: \
        galloc.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/galloc.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc galloc.c

$(CONFIG)/obj/http.o: \
        http.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/http.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc http.c

$(CONFIG)/obj/js.o: \
        js.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/js.h
	$(CC) -c -o $(CONFIG)/obj/js.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc js.c

$(CONFIG)/obj/matrixssl.o: \
        matrixssl.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/matrixssl.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc matrixssl.c

$(CONFIG)/obj/openssl.o: \
        openssl.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/openssl.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc openssl.c

$(CONFIG)/obj/options.o: \
        options.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/options.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc options.c

$(CONFIG)/obj/proc.o: \
        proc.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/proc.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc proc.c

$(CONFIG)/obj/rom-documents.o: \
        rom-documents.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/rom-documents.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc rom-documents.c

$(CONFIG)/obj/rom.o: \
        rom.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/rom.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc rom.c

$(CONFIG)/obj/route.o: \
        route.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/route.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc route.c

$(CONFIG)/obj/runtime.o: \
        runtime.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/runtime.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc runtime.c

$(CONFIG)/obj/socket.o: \
        socket.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/socket.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc socket.c

$(CONFIG)/obj/template.o: \
        template.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h
	$(CC) -c -o $(CONFIG)/obj/template.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc template.c

$(CONFIG)/obj/upload.o: \
        upload.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/upload.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc upload.c

$(CONFIG)/bin/libgo.a:  \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/auth.o \
        $(CONFIG)/obj/cgi.o \
        $(CONFIG)/obj/crypt.o \
        $(CONFIG)/obj/file.o \
        $(CONFIG)/obj/galloc.o \
        $(CONFIG)/obj/http.o \
        $(CONFIG)/obj/js.o \
        $(CONFIG)/obj/matrixssl.o \
        $(CONFIG)/obj/openssl.o \
        $(CONFIG)/obj/options.o \
        $(CONFIG)/obj/proc.o \
        $(CONFIG)/obj/rom-documents.o \
        $(CONFIG)/obj/rom.o \
        $(CONFIG)/obj/route.o \
        $(CONFIG)/obj/runtime.o \
        $(CONFIG)/obj/socket.o \
        $(CONFIG)/obj/template.o \
        $(CONFIG)/obj/upload.o
	/usr/bin/ar -cq $(CONFIG)/bin/libgo.a $(CONFIG)/obj/auth.o $(CONFIG)/obj/cgi.o $(CONFIG)/obj/crypt.o $(CONFIG)/obj/file.o $(CONFIG)/obj/galloc.o $(CONFIG)/obj/http.o $(CONFIG)/obj/js.o $(CONFIG)/obj/matrixssl.o $(CONFIG)/obj/openssl.o $(CONFIG)/obj/options.o $(CONFIG)/obj/proc.o $(CONFIG)/obj/rom-documents.o $(CONFIG)/obj/rom.o $(CONFIG)/obj/route.o $(CONFIG)/obj/runtime.o $(CONFIG)/obj/socket.o $(CONFIG)/obj/template.o $(CONFIG)/obj/upload.o

$(CONFIG)/obj/goahead.o: \
        goahead.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/goahead.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc goahead.c

$(CONFIG)/bin/goahead:  \
        $(CONFIG)/bin/libgo.a \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/goahead.o
	$(CC) -o $(CONFIG)/bin/goahead -arch x86_64 $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/goahead.o $(LIBS) -lgo

$(CONFIG)/obj/test.o: \
        test/test.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h
	$(CC) -c -o $(CONFIG)/obj/test.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc test/test.c

$(CONFIG)/bin/goahead-test:  \
        $(CONFIG)/bin/libgo.a \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/test.o
	$(CC) -o $(CONFIG)/bin/goahead-test -arch x86_64 $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/test.o $(LIBS) -lgo

$(CONFIG)/obj/gopass.o: \
        utils/gopass.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/gopass.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc utils/gopass.c

$(CONFIG)/bin/gopass:  \
        $(CONFIG)/bin/libgo.a \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/gopass.o
	$(CC) -o $(CONFIG)/bin/gopass -arch x86_64 $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/gopass.o $(LIBS) -lgo

$(CONFIG)/obj/webcomp.o: \
        utils/webcomp.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/webcomp.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc utils/webcomp.c

$(CONFIG)/bin/webcomp:  \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/webcomp.o
	$(CC) -o $(CONFIG)/bin/webcomp -arch x86_64 $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/webcomp.o $(LIBS)

$(CONFIG)/obj/cgitest.o: \
        test/cgitest.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/cgitest.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc test/cgitest.c

test/cgi-bin/cgitest:  \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/cgitest.o
	$(CC) -o test/cgi-bin/cgitest -arch x86_64 $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/cgitest.o $(LIBS)

