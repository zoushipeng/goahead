#
#   goahead-macosx.mk -- Build It Makefile to build Embedthis GoAhead for macosx
#

ARCH     := $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')
OS       := macosx
PROFILE  := debug
CONFIG   := $(OS)-$(ARCH)-$(PROFILE)
CC       := /usr/bin/clang
LD       := /usr/bin/ld
CFLAGS   := -Wno-deprecated-declarations -g -w
DFLAGS   := -DBIT_DEBUG
IFLAGS   := -I$(CONFIG)/inc
LDFLAGS  := '-Wl,-rpath,@executable_path/' '-Wl,-rpath,@loader_path/' '-g'
LIBPATHS := -L$(CONFIG)/bin
LIBS     := -lpthread -lm -ldl

all: prep \
        $(CONFIG)/bin/goahead \
        $(CONFIG)/bin/goahead-test \
        $(CONFIG)/bin/webcomp \
        test/cgi-bin/cgitest

.PHONY: prep

prep:
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
	rm -rf $(CONFIG)/obj/access.o
	rm -rf $(CONFIG)/obj/balloc.o
	rm -rf $(CONFIG)/obj/cgi.o
	rm -rf $(CONFIG)/obj/crypt.o
	rm -rf $(CONFIG)/obj/db.o
	rm -rf $(CONFIG)/obj/default.o
	rm -rf $(CONFIG)/obj/form.o
	rm -rf $(CONFIG)/obj/goahead.o
	rm -rf $(CONFIG)/obj/handler.o
	rm -rf $(CONFIG)/obj/http.o
	rm -rf $(CONFIG)/obj/js.o
	rm -rf $(CONFIG)/obj/matrixssl.o
	rm -rf $(CONFIG)/obj/openssl.o
	rm -rf $(CONFIG)/obj/rom-documents.o
	rm -rf $(CONFIG)/obj/rom.o
	rm -rf $(CONFIG)/obj/runtime.o
	rm -rf $(CONFIG)/obj/security.o
	rm -rf $(CONFIG)/obj/socket.o
	rm -rf $(CONFIG)/obj/ssl.o
	rm -rf $(CONFIG)/obj/template.o
	rm -rf $(CONFIG)/obj/um.o
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

$(CONFIG)/obj/access.o: \
        access.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/access.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include access.c

$(CONFIG)/obj/balloc.o: \
        balloc.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/balloc.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include balloc.c

$(CONFIG)/obj/cgi.o: \
        cgi.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/cgi.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include cgi.c

$(CONFIG)/obj/crypt.o: \
        crypt.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/crypt.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include crypt.c

$(CONFIG)/obj/db.o: \
        db.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/db.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include db.c

$(CONFIG)/obj/default.o: \
        default.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/default.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include default.c

$(CONFIG)/obj/form.o: \
        form.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/form.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include form.c

$(CONFIG)/obj/goahead.o: \
        goahead.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/goahead.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include goahead.c

$(CONFIG)/obj/handler.o: \
        handler.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/handler.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include handler.c

$(CONFIG)/obj/http.o: \
        http.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/http.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include http.c

$(CONFIG)/obj/js.o: \
        js.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/js.h
	$(CC) -c -o $(CONFIG)/obj/js.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include js.c

$(CONFIG)/obj/matrixssl.o: \
        matrixssl.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/matrixssl.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include matrixssl.c

$(CONFIG)/obj/openssl.o: \
        openssl.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/openssl.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include openssl.c

$(CONFIG)/obj/rom-documents.o: \
        rom-documents.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/rom-documents.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include rom-documents.c

$(CONFIG)/obj/rom.o: \
        rom.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/rom.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include rom.c

$(CONFIG)/obj/runtime.o: \
        runtime.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/runtime.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include runtime.c

$(CONFIG)/obj/security.o: \
        security.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/security.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include security.c

$(CONFIG)/obj/socket.o: \
        socket.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/socket.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include socket.c

$(CONFIG)/obj/ssl.o: \
        ssl.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/ssl.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include ssl.c

$(CONFIG)/obj/template.o: \
        template.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h
	$(CC) -c -o $(CONFIG)/obj/template.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include template.c

$(CONFIG)/obj/um.o: \
        um.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h
	$(CC) -c -o $(CONFIG)/obj/um.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include um.c

$(CONFIG)/bin/goahead:  \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h \
        $(CONFIG)/obj/access.o \
        $(CONFIG)/obj/balloc.o \
        $(CONFIG)/obj/cgi.o \
        $(CONFIG)/obj/crypt.o \
        $(CONFIG)/obj/db.o \
        $(CONFIG)/obj/default.o \
        $(CONFIG)/obj/form.o \
        $(CONFIG)/obj/goahead.o \
        $(CONFIG)/obj/handler.o \
        $(CONFIG)/obj/http.o \
        $(CONFIG)/obj/js.o \
        $(CONFIG)/obj/matrixssl.o \
        $(CONFIG)/obj/openssl.o \
        $(CONFIG)/obj/rom-documents.o \
        $(CONFIG)/obj/rom.o \
        $(CONFIG)/obj/runtime.o \
        $(CONFIG)/obj/security.o \
        $(CONFIG)/obj/socket.o \
        $(CONFIG)/obj/ssl.o \
        $(CONFIG)/obj/template.o \
        $(CONFIG)/obj/um.o
	$(CC) -o $(CONFIG)/bin/goahead -arch x86_64 $(LDFLAGS) $(LIBPATHS) -L../packages-macosx-x64/openssl/openssl-1.0.1b $(CONFIG)/obj/access.o $(CONFIG)/obj/balloc.o $(CONFIG)/obj/cgi.o $(CONFIG)/obj/crypt.o $(CONFIG)/obj/db.o $(CONFIG)/obj/default.o $(CONFIG)/obj/form.o $(CONFIG)/obj/goahead.o $(CONFIG)/obj/handler.o $(CONFIG)/obj/http.o $(CONFIG)/obj/js.o $(CONFIG)/obj/matrixssl.o $(CONFIG)/obj/openssl.o $(CONFIG)/obj/rom-documents.o $(CONFIG)/obj/rom.o $(CONFIG)/obj/runtime.o $(CONFIG)/obj/security.o $(CONFIG)/obj/socket.o $(CONFIG)/obj/ssl.o $(CONFIG)/obj/template.o $(CONFIG)/obj/um.o $(LIBS) -lssl -lcrypto

$(CONFIG)/obj/test.o: \
        test/test.c \
        $(CONFIG)/inc/bit.h \
        $(CONFIG)/inc/goahead.h \
        $(CONFIG)/inc/js.h
	$(CC) -c -o $(CONFIG)/obj/test.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include test/test.c

$(CONFIG)/bin/goahead-test:  \
        $(CONFIG)/obj/access.o \
        $(CONFIG)/obj/balloc.o \
        $(CONFIG)/obj/cgi.o \
        $(CONFIG)/obj/crypt.o \
        $(CONFIG)/obj/db.o \
        $(CONFIG)/obj/default.o \
        $(CONFIG)/obj/form.o \
        $(CONFIG)/obj/handler.o \
        $(CONFIG)/obj/http.o \
        $(CONFIG)/obj/js.o \
        $(CONFIG)/obj/matrixssl.o \
        $(CONFIG)/obj/openssl.o \
        $(CONFIG)/obj/rom-documents.o \
        $(CONFIG)/obj/rom.o \
        $(CONFIG)/obj/runtime.o \
        $(CONFIG)/obj/security.o \
        $(CONFIG)/obj/socket.o \
        $(CONFIG)/obj/ssl.o \
        $(CONFIG)/obj/template.o \
        $(CONFIG)/obj/um.o \
        $(CONFIG)/obj/test.o
	$(CC) -o $(CONFIG)/bin/goahead-test -arch x86_64 $(LDFLAGS) $(LIBPATHS) -L../packages-macosx-x64/openssl/openssl-1.0.1b $(CONFIG)/obj/access.o $(CONFIG)/obj/balloc.o $(CONFIG)/obj/cgi.o $(CONFIG)/obj/crypt.o $(CONFIG)/obj/db.o $(CONFIG)/obj/default.o $(CONFIG)/obj/form.o $(CONFIG)/obj/handler.o $(CONFIG)/obj/http.o $(CONFIG)/obj/js.o $(CONFIG)/obj/matrixssl.o $(CONFIG)/obj/openssl.o $(CONFIG)/obj/rom-documents.o $(CONFIG)/obj/rom.o $(CONFIG)/obj/runtime.o $(CONFIG)/obj/security.o $(CONFIG)/obj/socket.o $(CONFIG)/obj/ssl.o $(CONFIG)/obj/template.o $(CONFIG)/obj/um.o $(CONFIG)/obj/test.o $(LIBS) -lssl -lcrypto

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

