#
#   goahead-linux-default.mk -- Makefile to build Embedthis GoAhead for linux
#

PRODUCT           := goahead
VERSION           := 3.1.0
BUILD_NUMBER      := 2
PROFILE           := default
ARCH              := $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/')
OS                := linux
CC                := /usr/bin/gcc
LD                := /usr/bin/ld
CONFIG            := $(OS)-$(ARCH)-$(PROFILE)
LBIN              := $(CONFIG)/bin

BIT_PACK_EST      := 1

CFLAGS            += -fPIC   -w
DFLAGS            += -D_REENTRANT -DPIC  $(patsubst %,-D%,$(filter BIT_%,$(MAKEFLAGS))) -DBIT_PACK_EST=$(BIT_PACK_EST) 
IFLAGS            += -I$(CONFIG)/inc
LDFLAGS           += '-Wl,--enable-new-dtags' '-Wl,-rpath,$$ORIGIN/' '-rdynamic'
LIBPATHS          += -L$(CONFIG)/bin
LIBS              += -lpthread -lm -lrt -ldl

DEBUG             := debug
CFLAGS-debug      := -g
DFLAGS-debug      := -DBIT_DEBUG
LDFLAGS-debug     := -g
DFLAGS-release    := 
CFLAGS-release    := -O2
LDFLAGS-release   := 
CFLAGS            += $(CFLAGS-$(DEBUG))
DFLAGS            += $(DFLAGS-$(DEBUG))
LDFLAGS           += $(LDFLAGS-$(DEBUG))

BIT_ROOT_PREFIX   := 
BIT_BASE_PREFIX   := $(BIT_ROOT_PREFIX)/usr/local
BIT_DATA_PREFIX   := $(BIT_ROOT_PREFIX)/
BIT_STATE_PREFIX  := $(BIT_ROOT_PREFIX)/var
BIT_APP_PREFIX    := $(BIT_BASE_PREFIX)/lib/$(PRODUCT)
BIT_VAPP_PREFIX   := $(BIT_APP_PREFIX)/$(VERSION)
BIT_BIN_PREFIX    := $(BIT_ROOT_PREFIX)/usr/local/bin
BIT_INC_PREFIX    := $(BIT_ROOT_PREFIX)/usr/local/include
BIT_LIB_PREFIX    := $(BIT_ROOT_PREFIX)/usr/local/lib
BIT_MAN_PREFIX    := $(BIT_ROOT_PREFIX)/usr/local/share/man
BIT_SBIN_PREFIX   := $(BIT_ROOT_PREFIX)/usr/local/sbin
BIT_ETC_PREFIX    := $(BIT_ROOT_PREFIX)/etc/$(PRODUCT)
BIT_WEB_PREFIX    := $(BIT_ROOT_PREFIX)/var/www/$(PRODUCT)-default
BIT_LOG_PREFIX    := $(BIT_ROOT_PREFIX)/var/log/$(PRODUCT)
BIT_SPOOL_PREFIX  := $(BIT_ROOT_PREFIX)/var/spool/$(PRODUCT)
BIT_CACHE_PREFIX  := $(BIT_ROOT_PREFIX)/var/spool/$(PRODUCT)/cache
BIT_SRC_PREFIX    := $(BIT_ROOT_PREFIX)$(PRODUCT)-$(VERSION)


ifeq ($(BIT_PACK_EST),1)
TARGETS           += $(CONFIG)/bin/libest.so
endif
TARGETS           += $(CONFIG)/bin/ca.crt
TARGETS           += $(CONFIG)/bin/libgo.so
TARGETS           += $(CONFIG)/bin/goahead
TARGETS           += $(CONFIG)/bin/goahead-test
TARGETS           += $(CONFIG)/bin/gopass

unexport CDPATH

ifndef SHOW
.SILENT:
endif

all build compile: prep $(TARGETS)

.PHONY: prep

prep:
	@echo "      [Info] Use "make SHOW=1" to trace executed commands."
	@if [ "$(CONFIG)" = "" ] ; then echo WARNING: CONFIG not set ; exit 255 ; fi
	@if [ "$(BIT_APP_PREFIX)" = "" ] ; then echo WARNING: BIT_APP_PREFIX not set ; exit 255 ; fi
	@[ ! -x $(CONFIG)/bin ] && mkdir -p $(CONFIG)/bin; true
	@[ ! -x $(CONFIG)/inc ] && mkdir -p $(CONFIG)/inc; true
	@[ ! -x $(CONFIG)/obj ] && mkdir -p $(CONFIG)/obj; true
	@[ ! -f $(CONFIG)/inc/bit.h ] && cp projects/goahead-linux-default-bit.h $(CONFIG)/inc/bit.h ; true
	@[ ! -f $(CONFIG)/inc/bitos.h ] && cp src/bitos.h $(CONFIG)/inc/bitos.h ; true
	@if ! diff $(CONFIG)/inc/bit.h projects/goahead-linux-default-bit.h >/dev/null ; then\
		echo cp projects/goahead-linux-default-bit.h $(CONFIG)/inc/bit.h  ; \
		cp projects/goahead-linux-default-bit.h $(CONFIG)/inc/bit.h  ; \
	fi; true

clean:
	rm -rf $(CONFIG)/bin/libest.so
	rm -rf $(CONFIG)/bin/ca.crt
	rm -rf $(CONFIG)/bin/libgo.so
	rm -rf $(CONFIG)/bin/goahead
	rm -rf $(CONFIG)/bin/goahead-test
	rm -rf $(CONFIG)/bin/gopass
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

#
#   est.h
#
$(CONFIG)/inc/est.h: $(DEPS_1)
	@echo '      [File] $(CONFIG)/inc/est.h'
	mkdir -p "$(CONFIG)/inc"
	cp "src/deps/est/est.h" "$(CONFIG)/inc/est.h"

#
#   bit.h
#
$(CONFIG)/inc/bit.h: $(DEPS_2)

#
#   bitos.h
#
DEPS_3 += $(CONFIG)/inc/bit.h

$(CONFIG)/inc/bitos.h: $(DEPS_3)
	@echo '      [File] $(CONFIG)/inc/bitos.h'
	mkdir -p "$(CONFIG)/inc"
	cp "src/bitos.h" "$(CONFIG)/inc/bitos.h"

#
#   estLib.o
#
DEPS_4 += $(CONFIG)/inc/bit.h
DEPS_4 += $(CONFIG)/inc/est.h
DEPS_4 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/estLib.o: \
    src/deps/est/estLib.c $(DEPS_4)
	@echo '   [Compile] src/deps/est/estLib.c'
	$(CC) -c -o $(CONFIG)/obj/estLib.o -fPIC $(DFLAGS) $(IFLAGS) src/deps/est/estLib.c

ifeq ($(BIT_PACK_EST),1)
#
#   libest
#
DEPS_5 += $(CONFIG)/inc/est.h
DEPS_5 += $(CONFIG)/obj/estLib.o

$(CONFIG)/bin/libest.so: $(DEPS_5)
	@echo '      [Link] libest'
	$(CC) -shared -o $(CONFIG)/bin/libest.so $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/estLib.o $(LIBS)
endif

#
#   ca-crt
#
DEPS_6 += src/deps/est/ca.crt

$(CONFIG)/bin/ca.crt: $(DEPS_6)
	@echo '      [File] $(CONFIG)/bin/ca.crt'
	mkdir -p "$(CONFIG)/bin"
	cp "src/deps/est/ca.crt" "$(CONFIG)/bin/ca.crt"

#
#   goahead.h
#
$(CONFIG)/inc/goahead.h: $(DEPS_7)
	@echo '      [File] $(CONFIG)/inc/goahead.h'
	mkdir -p "$(CONFIG)/inc"
	cp "src/goahead.h" "$(CONFIG)/inc/goahead.h"

#
#   js.h
#
$(CONFIG)/inc/js.h: $(DEPS_8)
	@echo '      [File] $(CONFIG)/inc/js.h'
	mkdir -p "$(CONFIG)/inc"
	cp "src/js.h" "$(CONFIG)/inc/js.h"

#
#   action.o
#
DEPS_9 += $(CONFIG)/inc/bit.h
DEPS_9 += $(CONFIG)/inc/goahead.h
DEPS_9 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/action.o: \
    src/action.c $(DEPS_9)
	@echo '   [Compile] src/action.c'
	$(CC) -c -o $(CONFIG)/obj/action.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/action.c

#
#   alloc.o
#
DEPS_10 += $(CONFIG)/inc/bit.h
DEPS_10 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/alloc.o: \
    src/alloc.c $(DEPS_10)
	@echo '   [Compile] src/alloc.c'
	$(CC) -c -o $(CONFIG)/obj/alloc.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/alloc.c

#
#   auth.o
#
DEPS_11 += $(CONFIG)/inc/bit.h
DEPS_11 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/auth.o: \
    src/auth.c $(DEPS_11)
	@echo '   [Compile] src/auth.c'
	$(CC) -c -o $(CONFIG)/obj/auth.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/auth.c

#
#   cgi.o
#
DEPS_12 += $(CONFIG)/inc/bit.h
DEPS_12 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/cgi.o: \
    src/cgi.c $(DEPS_12)
	@echo '   [Compile] src/cgi.c'
	$(CC) -c -o $(CONFIG)/obj/cgi.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/cgi.c

#
#   crypt.o
#
DEPS_13 += $(CONFIG)/inc/bit.h
DEPS_13 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/crypt.o: \
    src/crypt.c $(DEPS_13)
	@echo '   [Compile] src/crypt.c'
	$(CC) -c -o $(CONFIG)/obj/crypt.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/crypt.c

#
#   file.o
#
DEPS_14 += $(CONFIG)/inc/bit.h
DEPS_14 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/file.o: \
    src/file.c $(DEPS_14)
	@echo '   [Compile] src/file.c'
	$(CC) -c -o $(CONFIG)/obj/file.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/file.c

#
#   fs.o
#
DEPS_15 += $(CONFIG)/inc/bit.h
DEPS_15 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/fs.o: \
    src/fs.c $(DEPS_15)
	@echo '   [Compile] src/fs.c'
	$(CC) -c -o $(CONFIG)/obj/fs.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/fs.c

#
#   goahead.o
#
DEPS_16 += $(CONFIG)/inc/bit.h
DEPS_16 += $(CONFIG)/inc/goahead.h
DEPS_16 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/goahead.o: \
    src/goahead.c $(DEPS_16)
	@echo '   [Compile] src/goahead.c'
	$(CC) -c -o $(CONFIG)/obj/goahead.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/goahead.c

#
#   http.o
#
DEPS_17 += $(CONFIG)/inc/bit.h
DEPS_17 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/http.o: \
    src/http.c $(DEPS_17)
	@echo '   [Compile] src/http.c'
	$(CC) -c -o $(CONFIG)/obj/http.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/http.c

#
#   js.o
#
DEPS_18 += $(CONFIG)/inc/bit.h
DEPS_18 += $(CONFIG)/inc/js.h
DEPS_18 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/js.o: \
    src/js.c $(DEPS_18)
	@echo '   [Compile] src/js.c'
	$(CC) -c -o $(CONFIG)/obj/js.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/js.c

#
#   jst.o
#
DEPS_19 += $(CONFIG)/inc/bit.h
DEPS_19 += $(CONFIG)/inc/goahead.h
DEPS_19 += $(CONFIG)/inc/js.h

$(CONFIG)/obj/jst.o: \
    src/jst.c $(DEPS_19)
	@echo '   [Compile] src/jst.c'
	$(CC) -c -o $(CONFIG)/obj/jst.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/jst.c

#
#   options.o
#
DEPS_20 += $(CONFIG)/inc/bit.h
DEPS_20 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/options.o: \
    src/options.c $(DEPS_20)
	@echo '   [Compile] src/options.c'
	$(CC) -c -o $(CONFIG)/obj/options.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/options.c

#
#   osdep.o
#
DEPS_21 += $(CONFIG)/inc/bit.h
DEPS_21 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/osdep.o: \
    src/osdep.c $(DEPS_21)
	@echo '   [Compile] src/osdep.c'
	$(CC) -c -o $(CONFIG)/obj/osdep.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/osdep.c

#
#   rom-documents.o
#
DEPS_22 += $(CONFIG)/inc/bit.h
DEPS_22 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/rom-documents.o: \
    src/rom-documents.c $(DEPS_22)
	@echo '   [Compile] src/rom-documents.c'
	$(CC) -c -o $(CONFIG)/obj/rom-documents.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/rom-documents.c

#
#   route.o
#
DEPS_23 += $(CONFIG)/inc/bit.h
DEPS_23 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/route.o: \
    src/route.c $(DEPS_23)
	@echo '   [Compile] src/route.c'
	$(CC) -c -o $(CONFIG)/obj/route.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/route.c

#
#   runtime.o
#
DEPS_24 += $(CONFIG)/inc/bit.h
DEPS_24 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/runtime.o: \
    src/runtime.c $(DEPS_24)
	@echo '   [Compile] src/runtime.c'
	$(CC) -c -o $(CONFIG)/obj/runtime.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/runtime.c

#
#   socket.o
#
DEPS_25 += $(CONFIG)/inc/bit.h
DEPS_25 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/socket.o: \
    src/socket.c $(DEPS_25)
	@echo '   [Compile] src/socket.c'
	$(CC) -c -o $(CONFIG)/obj/socket.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/socket.c

#
#   upload.o
#
DEPS_26 += $(CONFIG)/inc/bit.h
DEPS_26 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/upload.o: \
    src/upload.c $(DEPS_26)
	@echo '   [Compile] src/upload.c'
	$(CC) -c -o $(CONFIG)/obj/upload.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/upload.c

#
#   est.h
#
src/deps/est/est.h: $(DEPS_27)

#
#   est.o
#
DEPS_28 += $(CONFIG)/inc/bit.h
DEPS_28 += $(CONFIG)/inc/goahead.h
DEPS_28 += src/deps/est/est.h
DEPS_28 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/est.o: \
    src/ssl/est.c $(DEPS_28)
	@echo '   [Compile] src/ssl/est.c'
	$(CC) -c -o $(CONFIG)/obj/est.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/ssl/est.c

#
#   matrixssl.o
#
DEPS_29 += $(CONFIG)/inc/bit.h
DEPS_29 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/matrixssl.o: \
    src/ssl/matrixssl.c $(DEPS_29)
	@echo '   [Compile] src/ssl/matrixssl.c'
	$(CC) -c -o $(CONFIG)/obj/matrixssl.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/ssl/matrixssl.c

#
#   openssl.o
#
DEPS_30 += $(CONFIG)/inc/bit.h
DEPS_30 += $(CONFIG)/inc/bitos.h
DEPS_30 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/openssl.o: \
    src/ssl/openssl.c $(DEPS_30)
	@echo '   [Compile] src/ssl/openssl.c'
	$(CC) -c -o $(CONFIG)/obj/openssl.o $(CFLAGS) $(DFLAGS) $(IFLAGS) -Isrc/deps/est src/ssl/openssl.c

#
#   libgo
#
DEPS_31 += $(CONFIG)/inc/bitos.h
DEPS_31 += $(CONFIG)/inc/goahead.h
DEPS_31 += $(CONFIG)/inc/js.h
DEPS_31 += $(CONFIG)/obj/action.o
DEPS_31 += $(CONFIG)/obj/alloc.o
DEPS_31 += $(CONFIG)/obj/auth.o
DEPS_31 += $(CONFIG)/obj/cgi.o
DEPS_31 += $(CONFIG)/obj/crypt.o
DEPS_31 += $(CONFIG)/obj/file.o
DEPS_31 += $(CONFIG)/obj/fs.o
DEPS_31 += $(CONFIG)/obj/goahead.o
DEPS_31 += $(CONFIG)/obj/http.o
DEPS_31 += $(CONFIG)/obj/js.o
DEPS_31 += $(CONFIG)/obj/jst.o
DEPS_31 += $(CONFIG)/obj/options.o
DEPS_31 += $(CONFIG)/obj/osdep.o
DEPS_31 += $(CONFIG)/obj/rom-documents.o
DEPS_31 += $(CONFIG)/obj/route.o
DEPS_31 += $(CONFIG)/obj/runtime.o
DEPS_31 += $(CONFIG)/obj/socket.o
DEPS_31 += $(CONFIG)/obj/upload.o
DEPS_31 += $(CONFIG)/obj/est.o
DEPS_31 += $(CONFIG)/obj/matrixssl.o
DEPS_31 += $(CONFIG)/obj/openssl.o

ifeq ($(BIT_PACK_EST),1)
    LIBS_31 += -lest
endif

$(CONFIG)/bin/libgo.so: $(DEPS_31)
	@echo '      [Link] libgo'
	$(CC) -shared -o $(CONFIG)/bin/libgo.so $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/action.o $(CONFIG)/obj/alloc.o $(CONFIG)/obj/auth.o $(CONFIG)/obj/cgi.o $(CONFIG)/obj/crypt.o $(CONFIG)/obj/file.o $(CONFIG)/obj/fs.o $(CONFIG)/obj/goahead.o $(CONFIG)/obj/http.o $(CONFIG)/obj/js.o $(CONFIG)/obj/jst.o $(CONFIG)/obj/options.o $(CONFIG)/obj/osdep.o $(CONFIG)/obj/rom-documents.o $(CONFIG)/obj/route.o $(CONFIG)/obj/runtime.o $(CONFIG)/obj/socket.o $(CONFIG)/obj/upload.o $(CONFIG)/obj/est.o $(CONFIG)/obj/matrixssl.o $(CONFIG)/obj/openssl.o $(LIBS_31) $(LIBS_31) $(LIBS) -lest

#
#   goahead
#
DEPS_32 += $(CONFIG)/bin/libgo.so
DEPS_32 += $(CONFIG)/inc/bitos.h
DEPS_32 += $(CONFIG)/inc/goahead.h
DEPS_32 += $(CONFIG)/inc/js.h
DEPS_32 += $(CONFIG)/obj/goahead.o

LIBS_32 += -lgo
ifeq ($(BIT_PACK_EST),1)
    LIBS_32 += -lest
endif

$(CONFIG)/bin/goahead: $(DEPS_32)
	@echo '      [Link] goahead'
	$(CC) -o $(CONFIG)/bin/goahead $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/goahead.o $(LIBS_32) $(LIBS_32) $(LIBS) -lest $(LDFLAGS)

#
#   test.o
#
DEPS_33 += $(CONFIG)/inc/bit.h
DEPS_33 += $(CONFIG)/inc/goahead.h
DEPS_33 += $(CONFIG)/inc/js.h
DEPS_33 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/test.o: \
    test/test.c $(DEPS_33)
	@echo '   [Compile] test/test.c'
	$(CC) -c -o $(CONFIG)/obj/test.o $(CFLAGS) $(DFLAGS) $(IFLAGS) test/test.c

#
#   goahead-test
#
DEPS_34 += $(CONFIG)/bin/libgo.so
DEPS_34 += $(CONFIG)/inc/bitos.h
DEPS_34 += $(CONFIG)/inc/goahead.h
DEPS_34 += $(CONFIG)/inc/js.h
DEPS_34 += $(CONFIG)/obj/test.o

LIBS_34 += -lgo
ifeq ($(BIT_PACK_EST),1)
    LIBS_34 += -lest
endif

$(CONFIG)/bin/goahead-test: $(DEPS_34)
	@echo '      [Link] goahead-test'
	$(CC) -o $(CONFIG)/bin/goahead-test $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/test.o $(LIBS_34) $(LIBS_34) $(LIBS) -lest $(LDFLAGS)

#
#   gopass.o
#
DEPS_35 += $(CONFIG)/inc/bit.h
DEPS_35 += $(CONFIG)/inc/goahead.h
DEPS_35 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/gopass.o: \
    src/utils/gopass.c $(DEPS_35)
	@echo '   [Compile] src/utils/gopass.c'
	$(CC) -c -o $(CONFIG)/obj/gopass.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/utils/gopass.c

#
#   gopass
#
DEPS_36 += $(CONFIG)/bin/libgo.so
DEPS_36 += $(CONFIG)/inc/bitos.h
DEPS_36 += $(CONFIG)/inc/goahead.h
DEPS_36 += $(CONFIG)/inc/js.h
DEPS_36 += $(CONFIG)/obj/gopass.o

LIBS_36 += -lgo
ifeq ($(BIT_PACK_EST),1)
    LIBS_36 += -lest
endif

$(CONFIG)/bin/gopass: $(DEPS_36)
	@echo '      [Link] gopass'
	$(CC) -o $(CONFIG)/bin/gopass $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/gopass.o $(LIBS_36) $(LIBS_36) $(LIBS) -lest $(LDFLAGS)

#
#   version
#
version: $(DEPS_37)
	@echo 3.1.0-2

#
#   stop
#
stop: $(DEPS_38)
	

#
#   installBinary
#
DEPS_39 += stop

installBinary: $(DEPS_39)
	mkdir -p "$(BIT_APP_PREFIX)"
	mkdir -p "$(BIT_VAPP_PREFIX)"
	mkdir -p "$(BIT_ETC_PREFIX)"
	mkdir -p "$(BIT_WEB_PREFIX)"
	mkdir -p "$(BIT_APP_PREFIX)"
	rm -f "$(BIT_APP_PREFIX)/latest"
	ln -s "3.1.0" "$(BIT_APP_PREFIX)/latest"
	mkdir -p "$(BIT_VAPP_PREFIX)/bin"
	cp "$(CONFIG)/bin/goahead" "$(BIT_VAPP_PREFIX)/bin/goahead"
	mkdir -p "$(BIT_BIN_PREFIX)"
	rm -f "$(BIT_BIN_PREFIX)/goahead"
	ln -s "$(BIT_VAPP_PREFIX)/bin/goahead" "$(BIT_BIN_PREFIX)/goahead"
	cp "$(CONFIG)/bin/ca.crt" "$(BIT_VAPP_PREFIX)/bin/ca.crt"
	cp "$(CONFIG)/bin/libest.so" "$(BIT_VAPP_PREFIX)/bin/libest.so"
	cp "$(CONFIG)/bin/libgo.so" "$(BIT_VAPP_PREFIX)/bin/libgo.so"
	mkdir -p "$(BIT_VAPP_PREFIX)/doc/man/man1"
	cp "doc/man/goahead.1" "$(BIT_VAPP_PREFIX)/doc/man/man1/goahead.1"
	mkdir -p "$(BIT_MAN_PREFIX)/man1"
	rm -f "$(BIT_MAN_PREFIX)/man1/goahead.1"
	ln -s "$(BIT_VAPP_PREFIX)/doc/man/man1/goahead.1" "$(BIT_MAN_PREFIX)/man1/goahead.1"
	cp "doc/man/gopass.1" "$(BIT_VAPP_PREFIX)/doc/man/man1/gopass.1"
	rm -f "$(BIT_MAN_PREFIX)/man1/gopass.1"
	ln -s "$(BIT_VAPP_PREFIX)/doc/man/man1/gopass.1" "$(BIT_MAN_PREFIX)/man1/gopass.1"
	cp "doc/man/webcomp.1" "$(BIT_VAPP_PREFIX)/doc/man/man1/webcomp.1"
	rm -f "$(BIT_MAN_PREFIX)/man1/webcomp.1"
	ln -s "$(BIT_VAPP_PREFIX)/doc/man/man1/webcomp.1" "$(BIT_MAN_PREFIX)/man1/webcomp.1"
	mkdir -p "$(BIT_WEB_PREFIX)/web/bench"
	cp "src/web/bench/1b.html" "$(BIT_WEB_PREFIX)/web/bench/1b.html"
	cp "src/web/bench/4k.html" "$(BIT_WEB_PREFIX)/web/bench/4k.html"
	cp "src/web/bench/64k.html" "$(BIT_WEB_PREFIX)/web/bench/64k.html"
	mkdir -p "$(BIT_WEB_PREFIX)/web"
	cp "src/web/index.html" "$(BIT_WEB_PREFIX)/web/index.html"
	mkdir -p "$(BIT_ETC_PREFIX)"
	cp "src/auth.txt" "$(BIT_ETC_PREFIX)/auth.txt"
	cp "src/route.txt" "$(BIT_ETC_PREFIX)/route.txt"

#
#   start
#
start: $(DEPS_40)
	

#
#   install
#
DEPS_41 += stop
DEPS_41 += installBinary
DEPS_41 += start

install: $(DEPS_41)
	

#
#   uninstall
#
DEPS_42 += stop

uninstall: $(DEPS_42)
	rm -fr "$(BIT_VAPP_PREFIX)"
	rmdir -p "$(BIT_ETC_PREFIX)" 2>/dev/null ; true
	rmdir -p "$(BIT_WEB_PREFIX)" 2>/dev/null ; true
	rm -f "$(BIT_APP_PREFIX)/latest"
	rmdir -p "$(BIT_APP_PREFIX)" 2>/dev/null ; true

