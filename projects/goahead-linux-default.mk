#
#   goahead-linux-default.mk -- Makefile to build Embedthis GoAhead for linux
#

PRODUCT           := goahead
VERSION           := 3.1.1
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
	rm -f "$(CONFIG)/bin/libest.so"
	rm -f "$(CONFIG)/bin/ca.crt"
	rm -f "$(CONFIG)/bin/libgo.so"
	rm -f "$(CONFIG)/bin/goahead"
	rm -f "$(CONFIG)/bin/goahead-test"
	rm -f "$(CONFIG)/bin/gopass"
	rm -f "$(CONFIG)/obj/estLib.o"
	rm -f "$(CONFIG)/obj/action.o"
	rm -f "$(CONFIG)/obj/alloc.o"
	rm -f "$(CONFIG)/obj/auth.o"
	rm -f "$(CONFIG)/obj/cgi.o"
	rm -f "$(CONFIG)/obj/crypt.o"
	rm -f "$(CONFIG)/obj/file.o"
	rm -f "$(CONFIG)/obj/fs.o"
	rm -f "$(CONFIG)/obj/goahead.o"
	rm -f "$(CONFIG)/obj/http.o"
	rm -f "$(CONFIG)/obj/js.o"
	rm -f "$(CONFIG)/obj/jst.o"
	rm -f "$(CONFIG)/obj/options.o"
	rm -f "$(CONFIG)/obj/osdep.o"
	rm -f "$(CONFIG)/obj/rom-documents.o"
	rm -f "$(CONFIG)/obj/route.o"
	rm -f "$(CONFIG)/obj/runtime.o"
	rm -f "$(CONFIG)/obj/socket.o"
	rm -f "$(CONFIG)/obj/upload.o"
	rm -f "$(CONFIG)/obj/est.o"
	rm -f "$(CONFIG)/obj/matrixssl.o"
	rm -f "$(CONFIG)/obj/nanossl.o"
	rm -f "$(CONFIG)/obj/openssl.o"
	rm -f "$(CONFIG)/obj/test.o"
	rm -f "$(CONFIG)/obj/gopass.o"

clobber: clean
	rm -fr ./$(CONFIG)



#
#   version
#
version: $(DEPS_1)
	@echo NN 3.1.1-2

#
#   est.h
#
$(CONFIG)/inc/est.h: $(DEPS_2)
	@echo '      [Copy] $(CONFIG)/inc/est.h'
	mkdir -p "$(CONFIG)/inc"
	cp "src/deps/est/est.h" "$(CONFIG)/inc/est.h"

#
#   bit.h
#
$(CONFIG)/inc/bit.h: $(DEPS_3)
	@echo '      [Copy] $(CONFIG)/inc/bit.h'

#
#   bitos.h
#
DEPS_4 += $(CONFIG)/inc/bit.h

$(CONFIG)/inc/bitos.h: $(DEPS_4)
	@echo '      [Copy] $(CONFIG)/inc/bitos.h'
	mkdir -p "$(CONFIG)/inc"
	cp "src/bitos.h" "$(CONFIG)/inc/bitos.h"

#
#   estLib.o
#
DEPS_5 += $(CONFIG)/inc/bit.h
DEPS_5 += $(CONFIG)/inc/est.h
DEPS_5 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/estLib.o: \
    src/deps/est/estLib.c $(DEPS_5)
	@echo '   [Compile] src/deps/est/estLib.c'
	$(CC) -c -o $(CONFIG)/obj/estLib.o -fPIC $(DFLAGS) $(IFLAGS) src/deps/est/estLib.c

ifeq ($(BIT_PACK_EST),1)
#
#   libest
#
DEPS_6 += $(CONFIG)/inc/est.h
DEPS_6 += $(CONFIG)/obj/estLib.o

$(CONFIG)/bin/libest.so: $(DEPS_6)
	@echo '      [Link] libest'
	$(CC) -shared -o $(CONFIG)/bin/libest.so $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/estLib.o $(LIBS)
endif

#
#   ca-crt
#
DEPS_7 += src/deps/est/ca.crt

$(CONFIG)/bin/ca.crt: $(DEPS_7)
	@echo '      [Copy] $(CONFIG)/bin/ca.crt'
	mkdir -p "$(CONFIG)/bin"
	cp "src/deps/est/ca.crt" "$(CONFIG)/bin/ca.crt"

#
#   goahead.h
#
$(CONFIG)/inc/goahead.h: $(DEPS_8)
	@echo '      [Copy] $(CONFIG)/inc/goahead.h'
	mkdir -p "$(CONFIG)/inc"
	cp "src/goahead.h" "$(CONFIG)/inc/goahead.h"

#
#   js.h
#
$(CONFIG)/inc/js.h: $(DEPS_9)
	@echo '      [Copy] $(CONFIG)/inc/js.h'
	mkdir -p "$(CONFIG)/inc"
	cp "src/js.h" "$(CONFIG)/inc/js.h"

#
#   action.o
#
DEPS_10 += $(CONFIG)/inc/bit.h
DEPS_10 += $(CONFIG)/inc/goahead.h
DEPS_10 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/action.o: \
    src/action.c $(DEPS_10)
	@echo '   [Compile] src/action.c'
	$(CC) -c -o $(CONFIG)/obj/action.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/action.c

#
#   alloc.o
#
DEPS_11 += $(CONFIG)/inc/bit.h
DEPS_11 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/alloc.o: \
    src/alloc.c $(DEPS_11)
	@echo '   [Compile] src/alloc.c'
	$(CC) -c -o $(CONFIG)/obj/alloc.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/alloc.c

#
#   auth.o
#
DEPS_12 += $(CONFIG)/inc/bit.h
DEPS_12 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/auth.o: \
    src/auth.c $(DEPS_12)
	@echo '   [Compile] src/auth.c'
	$(CC) -c -o $(CONFIG)/obj/auth.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/auth.c

#
#   cgi.o
#
DEPS_13 += $(CONFIG)/inc/bit.h
DEPS_13 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/cgi.o: \
    src/cgi.c $(DEPS_13)
	@echo '   [Compile] src/cgi.c'
	$(CC) -c -o $(CONFIG)/obj/cgi.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/cgi.c

#
#   crypt.o
#
DEPS_14 += $(CONFIG)/inc/bit.h
DEPS_14 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/crypt.o: \
    src/crypt.c $(DEPS_14)
	@echo '   [Compile] src/crypt.c'
	$(CC) -c -o $(CONFIG)/obj/crypt.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/crypt.c

#
#   file.o
#
DEPS_15 += $(CONFIG)/inc/bit.h
DEPS_15 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/file.o: \
    src/file.c $(DEPS_15)
	@echo '   [Compile] src/file.c'
	$(CC) -c -o $(CONFIG)/obj/file.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/file.c

#
#   fs.o
#
DEPS_16 += $(CONFIG)/inc/bit.h
DEPS_16 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/fs.o: \
    src/fs.c $(DEPS_16)
	@echo '   [Compile] src/fs.c'
	$(CC) -c -o $(CONFIG)/obj/fs.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/fs.c

#
#   goahead.o
#
DEPS_17 += $(CONFIG)/inc/bit.h
DEPS_17 += $(CONFIG)/inc/goahead.h
DEPS_17 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/goahead.o: \
    src/goahead.c $(DEPS_17)
	@echo '   [Compile] src/goahead.c'
	$(CC) -c -o $(CONFIG)/obj/goahead.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/goahead.c

#
#   http.o
#
DEPS_18 += $(CONFIG)/inc/bit.h
DEPS_18 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/http.o: \
    src/http.c $(DEPS_18)
	@echo '   [Compile] src/http.c'
	$(CC) -c -o $(CONFIG)/obj/http.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/http.c

#
#   js.o
#
DEPS_19 += $(CONFIG)/inc/bit.h
DEPS_19 += $(CONFIG)/inc/js.h
DEPS_19 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/js.o: \
    src/js.c $(DEPS_19)
	@echo '   [Compile] src/js.c'
	$(CC) -c -o $(CONFIG)/obj/js.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/js.c

#
#   jst.o
#
DEPS_20 += $(CONFIG)/inc/bit.h
DEPS_20 += $(CONFIG)/inc/goahead.h
DEPS_20 += $(CONFIG)/inc/js.h

$(CONFIG)/obj/jst.o: \
    src/jst.c $(DEPS_20)
	@echo '   [Compile] src/jst.c'
	$(CC) -c -o $(CONFIG)/obj/jst.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/jst.c

#
#   options.o
#
DEPS_21 += $(CONFIG)/inc/bit.h
DEPS_21 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/options.o: \
    src/options.c $(DEPS_21)
	@echo '   [Compile] src/options.c'
	$(CC) -c -o $(CONFIG)/obj/options.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/options.c

#
#   osdep.o
#
DEPS_22 += $(CONFIG)/inc/bit.h
DEPS_22 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/osdep.o: \
    src/osdep.c $(DEPS_22)
	@echo '   [Compile] src/osdep.c'
	$(CC) -c -o $(CONFIG)/obj/osdep.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/osdep.c

#
#   rom-documents.o
#
DEPS_23 += $(CONFIG)/inc/bit.h
DEPS_23 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/rom-documents.o: \
    src/rom-documents.c $(DEPS_23)
	@echo '   [Compile] src/rom-documents.c'
	$(CC) -c -o $(CONFIG)/obj/rom-documents.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/rom-documents.c

#
#   route.o
#
DEPS_24 += $(CONFIG)/inc/bit.h
DEPS_24 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/route.o: \
    src/route.c $(DEPS_24)
	@echo '   [Compile] src/route.c'
	$(CC) -c -o $(CONFIG)/obj/route.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/route.c

#
#   runtime.o
#
DEPS_25 += $(CONFIG)/inc/bit.h
DEPS_25 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/runtime.o: \
    src/runtime.c $(DEPS_25)
	@echo '   [Compile] src/runtime.c'
	$(CC) -c -o $(CONFIG)/obj/runtime.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/runtime.c

#
#   socket.o
#
DEPS_26 += $(CONFIG)/inc/bit.h
DEPS_26 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/socket.o: \
    src/socket.c $(DEPS_26)
	@echo '   [Compile] src/socket.c'
	$(CC) -c -o $(CONFIG)/obj/socket.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/socket.c

#
#   upload.o
#
DEPS_27 += $(CONFIG)/inc/bit.h
DEPS_27 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/upload.o: \
    src/upload.c $(DEPS_27)
	@echo '   [Compile] src/upload.c'
	$(CC) -c -o $(CONFIG)/obj/upload.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/upload.c

#
#   est.o
#
DEPS_28 += $(CONFIG)/inc/bit.h
DEPS_28 += $(CONFIG)/inc/goahead.h
DEPS_28 += $(CONFIG)/inc/est.h

$(CONFIG)/obj/est.o: \
    src/ssl/est.c $(DEPS_28)
	@echo '   [Compile] src/ssl/est.c'
	$(CC) -c -o $(CONFIG)/obj/est.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/ssl/est.c

#
#   matrixssl.o
#
DEPS_29 += $(CONFIG)/inc/bit.h
DEPS_29 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/matrixssl.o: \
    src/ssl/matrixssl.c $(DEPS_29)
	@echo '   [Compile] src/ssl/matrixssl.c'
	$(CC) -c -o $(CONFIG)/obj/matrixssl.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/ssl/matrixssl.c

#
#   nanossl.o
#
DEPS_30 += $(CONFIG)/inc/bit.h

$(CONFIG)/obj/nanossl.o: \
    src/ssl/nanossl.c $(DEPS_30)
	@echo '   [Compile] src/ssl/nanossl.c'
	$(CC) -c -o $(CONFIG)/obj/nanossl.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/ssl/nanossl.c

#
#   openssl.o
#
DEPS_31 += $(CONFIG)/inc/bit.h
DEPS_31 += $(CONFIG)/inc/bitos.h
DEPS_31 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/openssl.o: \
    src/ssl/openssl.c $(DEPS_31)
	@echo '   [Compile] src/ssl/openssl.c'
	$(CC) -c -o $(CONFIG)/obj/openssl.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/ssl/openssl.c

#
#   libgo
#
DEPS_32 += $(CONFIG)/inc/bitos.h
DEPS_32 += $(CONFIG)/inc/goahead.h
DEPS_32 += $(CONFIG)/inc/js.h
DEPS_32 += $(CONFIG)/obj/action.o
DEPS_32 += $(CONFIG)/obj/alloc.o
DEPS_32 += $(CONFIG)/obj/auth.o
DEPS_32 += $(CONFIG)/obj/cgi.o
DEPS_32 += $(CONFIG)/obj/crypt.o
DEPS_32 += $(CONFIG)/obj/file.o
DEPS_32 += $(CONFIG)/obj/fs.o
DEPS_32 += $(CONFIG)/obj/goahead.o
DEPS_32 += $(CONFIG)/obj/http.o
DEPS_32 += $(CONFIG)/obj/js.o
DEPS_32 += $(CONFIG)/obj/jst.o
DEPS_32 += $(CONFIG)/obj/options.o
DEPS_32 += $(CONFIG)/obj/osdep.o
DEPS_32 += $(CONFIG)/obj/rom-documents.o
DEPS_32 += $(CONFIG)/obj/route.o
DEPS_32 += $(CONFIG)/obj/runtime.o
DEPS_32 += $(CONFIG)/obj/socket.o
DEPS_32 += $(CONFIG)/obj/upload.o
DEPS_32 += $(CONFIG)/obj/est.o
DEPS_32 += $(CONFIG)/obj/matrixssl.o
DEPS_32 += $(CONFIG)/obj/nanossl.o
DEPS_32 += $(CONFIG)/obj/openssl.o

ifeq ($(BIT_PACK_EST),1)
    LIBS_32 += -lest
endif

$(CONFIG)/bin/libgo.so: $(DEPS_32)
	@echo '      [Link] libgo'
	$(CC) -shared -o $(CONFIG)/bin/libgo.so $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/action.o $(CONFIG)/obj/alloc.o $(CONFIG)/obj/auth.o $(CONFIG)/obj/cgi.o $(CONFIG)/obj/crypt.o $(CONFIG)/obj/file.o $(CONFIG)/obj/fs.o $(CONFIG)/obj/goahead.o $(CONFIG)/obj/http.o $(CONFIG)/obj/js.o $(CONFIG)/obj/jst.o $(CONFIG)/obj/options.o $(CONFIG)/obj/osdep.o $(CONFIG)/obj/rom-documents.o $(CONFIG)/obj/route.o $(CONFIG)/obj/runtime.o $(CONFIG)/obj/socket.o $(CONFIG)/obj/upload.o $(CONFIG)/obj/est.o $(CONFIG)/obj/matrixssl.o $(CONFIG)/obj/nanossl.o $(CONFIG)/obj/openssl.o $(LIBS_32) $(LIBS_32) $(LIBS) -lest

#
#   goahead
#
DEPS_33 += $(CONFIG)/bin/libgo.so
DEPS_33 += $(CONFIG)/inc/bitos.h
DEPS_33 += $(CONFIG)/inc/goahead.h
DEPS_33 += $(CONFIG)/inc/js.h
DEPS_33 += $(CONFIG)/obj/goahead.o

LIBS_33 += -lgo
ifeq ($(BIT_PACK_EST),1)
    LIBS_33 += -lest
endif

$(CONFIG)/bin/goahead: $(DEPS_33)
	@echo '      [Link] goahead'
	$(CC) -o $(CONFIG)/bin/goahead $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/goahead.o $(LIBS_33) $(LIBS_33) $(LIBS) -lest $(LDFLAGS)

#
#   test.o
#
DEPS_34 += $(CONFIG)/inc/bit.h
DEPS_34 += $(CONFIG)/inc/goahead.h
DEPS_34 += $(CONFIG)/inc/js.h
DEPS_34 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/test.o: \
    test/test.c $(DEPS_34)
	@echo '   [Compile] test/test.c'
	$(CC) -c -o $(CONFIG)/obj/test.o $(CFLAGS) $(DFLAGS) $(IFLAGS) test/test.c

#
#   goahead-test
#
DEPS_35 += $(CONFIG)/bin/libgo.so
DEPS_35 += $(CONFIG)/inc/bitos.h
DEPS_35 += $(CONFIG)/inc/goahead.h
DEPS_35 += $(CONFIG)/inc/js.h
DEPS_35 += $(CONFIG)/obj/test.o

LIBS_35 += -lgo
ifeq ($(BIT_PACK_EST),1)
    LIBS_35 += -lest
endif

$(CONFIG)/bin/goahead-test: $(DEPS_35)
	@echo '      [Link] goahead-test'
	$(CC) -o $(CONFIG)/bin/goahead-test $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/test.o $(LIBS_35) $(LIBS_35) $(LIBS) -lest $(LDFLAGS)

#
#   gopass.o
#
DEPS_36 += $(CONFIG)/inc/bit.h
DEPS_36 += $(CONFIG)/inc/goahead.h
DEPS_36 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/gopass.o: \
    src/utils/gopass.c $(DEPS_36)
	@echo '   [Compile] src/utils/gopass.c'
	$(CC) -c -o $(CONFIG)/obj/gopass.o $(CFLAGS) $(DFLAGS) $(IFLAGS) src/utils/gopass.c

#
#   gopass
#
DEPS_37 += $(CONFIG)/bin/libgo.so
DEPS_37 += $(CONFIG)/inc/bitos.h
DEPS_37 += $(CONFIG)/inc/goahead.h
DEPS_37 += $(CONFIG)/inc/js.h
DEPS_37 += $(CONFIG)/obj/gopass.o

LIBS_37 += -lgo
ifeq ($(BIT_PACK_EST),1)
    LIBS_37 += -lest
endif

$(CONFIG)/bin/gopass: $(DEPS_37)
	@echo '      [Link] gopass'
	$(CC) -o $(CONFIG)/bin/gopass $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/gopass.o $(LIBS_37) $(LIBS_37) $(LIBS) -lest $(LDFLAGS)

#
#   stop
#
stop: $(DEPS_38)

#
#   installBinary
#
DEPS_39 += stop

installBinary: $(DEPS_39)

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
	

