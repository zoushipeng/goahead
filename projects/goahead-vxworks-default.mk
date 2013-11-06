#
#   goahead-vxworks-default.mk -- Makefile to build Embedthis GoAhead for vxworks
#

PRODUCT            := goahead
VERSION            := 3.1.3
BUILD_NUMBER       := 0
PROFILE            := default
ARCH               := $(shell echo $(WIND_HOST_TYPE) | sed 's/-.*//')
CPU                := $(subst X86,PENTIUM,$(shell echo $(ARCH) | tr a-z A-Z))
OS                 := vxworks
CC                 := cc$(subst x86,pentium,$(ARCH))
LD                 := link
CONFIG             := $(OS)-$(ARCH)-$(PROFILE)
LBIN               := $(CONFIG)/bin

BIT_PACK_EST       := 1
BIT_PACK_SSL       := 1

ifeq ($(BIT_PACK_EST),1)
    BIT_PACK_SSL := 1
endif
ifeq ($(BIT_PACK_LIB),1)
    BIT_PACK_COMPILER := 1
endif
ifeq ($(BIT_PACK_MATRIXSSL),1)
    BIT_PACK_SSL := 1
endif
ifeq ($(BIT_PACK_NANOSSL),1)
    BIT_PACK_SSL := 1
endif
ifeq ($(BIT_PACK_OPENSSL),1)
    BIT_PACK_SSL := 1
endif

BIT_PACK_COMPILER_PATH    := cc$(subst x86,pentium,$(ARCH))
BIT_PACK_DOXYGEN_PATH     := doxygen
BIT_PACK_DSI_PATH         := dsi
BIT_PACK_EST_PATH         := est
BIT_PACK_LIB_PATH         := ar
BIT_PACK_LINK_PATH        := link
BIT_PACK_MAN_PATH         := man
BIT_PACK_MAN2HTML_PATH    := man2html
BIT_PACK_MATRIXSSL_PATH   := /usr/src/matrixssl
BIT_PACK_NANOSSL_PATH     := /usr/src/nanossl
BIT_PACK_OPENSSL_PATH     := /usr/src/openssl
BIT_PACK_PMAKER_PATH      := pmaker
BIT_PACK_SSL_PATH         := ssl
BIT_PACK_UTEST_PATH       := utest
BIT_PACK_VXWORKS_PATH     := $(WIND_BASE)
BIT_PACK_ZIP_PATH         := zip

export WIND_HOME          := $(WIND_BASE)/..
export PATH               := $(WIND_GNU_PATH)/$(WIND_HOST_TYPE)/bin:$(PATH)

CFLAGS             += -fno-builtin -fno-defer-pop -fvolatile -w
DFLAGS             += -DVXWORKS -DRW_MULTI_THREAD -D_GNU_TOOL -DCPU=PENTIUM $(patsubst %,-D%,$(filter BIT_%,$(MAKEFLAGS))) -DBIT_PACK_EST=$(BIT_PACK_EST) -DBIT_PACK_SSL=$(BIT_PACK_SSL) 
IFLAGS             += "-I$(CONFIG)/inc -I$(WIND_BASE)/target/h -I$(WIND_BASE)/target/h/wrn/coreip"
LDFLAGS            += '-Wl,-r'
LIBPATHS           += -L$(CONFIG)/bin
LIBS               += -lgcc

DEBUG              := debug
CFLAGS-debug       := -g
DFLAGS-debug       := -DBIT_DEBUG
LDFLAGS-debug      := -g
DFLAGS-release     := 
CFLAGS-release     := -O2
LDFLAGS-release    := 
CFLAGS             += $(CFLAGS-$(DEBUG))
DFLAGS             += $(DFLAGS-$(DEBUG))
LDFLAGS            += $(LDFLAGS-$(DEBUG))

BIT_ROOT_PREFIX    := deploy
BIT_BASE_PREFIX    := $(BIT_ROOT_PREFIX)
BIT_DATA_PREFIX    := $(BIT_VAPP_PREFIX)
BIT_STATE_PREFIX   := $(BIT_VAPP_PREFIX)
BIT_BIN_PREFIX     := $(BIT_VAPP_PREFIX)
BIT_INC_PREFIX     := $(BIT_VAPP_PREFIX)/inc
BIT_LIB_PREFIX     := $(BIT_VAPP_PREFIX)
BIT_MAN_PREFIX     := $(BIT_VAPP_PREFIX)
BIT_SBIN_PREFIX    := $(BIT_VAPP_PREFIX)
BIT_ETC_PREFIX     := $(BIT_VAPP_PREFIX)
BIT_WEB_PREFIX     := $(BIT_VAPP_PREFIX)/web
BIT_LOG_PREFIX     := $(BIT_VAPP_PREFIX)
BIT_SPOOL_PREFIX   := $(BIT_VAPP_PREFIX)
BIT_CACHE_PREFIX   := $(BIT_VAPP_PREFIX)
BIT_APP_PREFIX     := $(BIT_BASE_PREFIX)
BIT_VAPP_PREFIX    := $(BIT_APP_PREFIX)
BIT_SRC_PREFIX     := $(BIT_ROOT_PREFIX)/usr/src/$(PRODUCT)-$(VERSION)


ifeq ($(BIT_PACK_EST),1)
TARGETS            += $(CONFIG)/bin/libest.out
endif
TARGETS            += $(CONFIG)/bin/ca.crt
TARGETS            += $(CONFIG)/bin/libgo.out
TARGETS            += $(CONFIG)/bin/goahead.out
TARGETS            += $(CONFIG)/bin/goahead-test.out
TARGETS            += $(CONFIG)/bin/gopass.out

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
	@if [ "$(WIND_BASE)" = "" ] ; then echo WARNING: WIND_BASE not set. Run wrenv.sh. ; exit 255 ; fi
	@if [ "$(WIND_HOST_TYPE)" = "" ] ; then echo WARNING: WIND_HOST_TYPE not set. Run wrenv.sh. ; exit 255 ; fi
	@if [ "$(WIND_GNU_PATH)" = "" ] ; then echo WARNING: WIND_GNU_PATH not set. Run wrenv.sh. ; exit 255 ; fi
	@[ ! -x $(CONFIG)/bin ] && mkdir -p $(CONFIG)/bin; true
	@[ ! -x $(CONFIG)/inc ] && mkdir -p $(CONFIG)/inc; true
	@[ ! -x $(CONFIG)/obj ] && mkdir -p $(CONFIG)/obj; true
	@[ ! -f $(CONFIG)/inc/bitos.h ] && cp src/bitos.h $(CONFIG)/inc/bitos.h ; true
	@if ! diff $(CONFIG)/inc/bitos.h src/bitos.h >/dev/null ; then\
		cp src/bitos.h $(CONFIG)/inc/bitos.h  ; \
	fi; true
	@[ ! -f $(CONFIG)/inc/bit.h ] && cp projects/goahead-vxworks-default-bit.h $(CONFIG)/inc/bit.h ; true
	@if ! diff $(CONFIG)/inc/bit.h projects/goahead-vxworks-default-bit.h >/dev/null ; then\
		cp projects/goahead-vxworks-default-bit.h $(CONFIG)/inc/bit.h  ; \
	fi; true
	@if [ -f "$(CONFIG)/.makeflags" ] ; then \
		if [ "$(MAKEFLAGS)" != " ` cat $(CONFIG)/.makeflags`" ] ; then \
			echo "   [Warning] Make flags have changed since the last build: "`cat $(CONFIG)/.makeflags`"" ; \
		fi ; \
	fi
	@echo $(MAKEFLAGS) >$(CONFIG)/.makeflags

clean:
	rm -f "$(CONFIG)/bin/libest.out"
	rm -f "$(CONFIG)/bin/ca.crt"
	rm -f "$(CONFIG)/bin/libgo.out"
	rm -f "$(CONFIG)/bin/goahead.out"
	rm -f "$(CONFIG)/bin/goahead-test.out"
	rm -f "$(CONFIG)/bin/gopass.out"
	rm -f "$(CONFIG)/obj/estLib.o"
	rm -f "$(CONFIG)/obj/action.o"
	rm -f "$(CONFIG)/obj/alloc.o"
	rm -f "$(CONFIG)/obj/auth.o"
	rm -f "$(CONFIG)/obj/cgi.o"
	rm -f "$(CONFIG)/obj/crypt.o"
	rm -f "$(CONFIG)/obj/file.o"
	rm -f "$(CONFIG)/obj/fs.o"
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
	rm -f "$(CONFIG)/obj/goahead.o"
	rm -f "$(CONFIG)/obj/test.o"
	rm -f "$(CONFIG)/obj/gopass.o"

clobber: clean
	rm -fr ./$(CONFIG)



#
#   version
#
version: $(DEPS_1)
	@echo 3.1.3-0

#
#   est.h
#
$(CONFIG)/inc/est.h: $(DEPS_2)
	@echo '      [Copy] $(CONFIG)/inc/est.h'
	mkdir -p "$(CONFIG)/inc"
	cp src/deps/est/est.h $(CONFIG)/inc/est.h

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
	cp src/bitos.h $(CONFIG)/inc/bitos.h

#
#   estLib.o
#
DEPS_5 += $(CONFIG)/inc/bit.h
DEPS_5 += $(CONFIG)/inc/est.h
DEPS_5 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/estLib.o: \
    src/deps/est/estLib.c $(DEPS_5)
	@echo '   [Compile] $(CONFIG)/obj/estLib.o'
	$(CC) -c -o $(CONFIG)/obj/estLib.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/deps/est/estLib.c

ifeq ($(BIT_PACK_EST),1)
#
#   libest
#
DEPS_6 += $(CONFIG)/inc/est.h
DEPS_6 += $(CONFIG)/inc/bit.h
DEPS_6 += $(CONFIG)/inc/bitos.h
DEPS_6 += $(CONFIG)/obj/estLib.o

$(CONFIG)/bin/libest.out: $(DEPS_6)
	@echo '      [Link] $(CONFIG)/bin/libest.out'
	$(CC) -r -o $(CONFIG)/bin/libest.out $(LDFLAGS) $(LIBPATHS) "$(CONFIG)/obj/estLib.o" $(LIBS) 
endif

#
#   ca-crt
#
DEPS_7 += src/deps/est/ca.crt

$(CONFIG)/bin/ca.crt: $(DEPS_7)
	@echo '      [Copy] $(CONFIG)/bin/ca.crt'
	mkdir -p "$(CONFIG)/bin"
	cp src/deps/est/ca.crt $(CONFIG)/bin/ca.crt

#
#   goahead.h
#
$(CONFIG)/inc/goahead.h: $(DEPS_8)
	@echo '      [Copy] $(CONFIG)/inc/goahead.h'
	mkdir -p "$(CONFIG)/inc"
	cp src/goahead.h $(CONFIG)/inc/goahead.h

#
#   js.h
#
$(CONFIG)/inc/js.h: $(DEPS_9)
	@echo '      [Copy] $(CONFIG)/inc/js.h'
	mkdir -p "$(CONFIG)/inc"
	cp src/js.h $(CONFIG)/inc/js.h

#
#   action.o
#
DEPS_10 += $(CONFIG)/inc/bit.h
DEPS_10 += $(CONFIG)/inc/goahead.h
DEPS_10 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/action.o: \
    src/action.c $(DEPS_10)
	@echo '   [Compile] $(CONFIG)/obj/action.o'
	$(CC) -c -o $(CONFIG)/obj/action.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/action.c

#
#   alloc.o
#
DEPS_11 += $(CONFIG)/inc/bit.h
DEPS_11 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/alloc.o: \
    src/alloc.c $(DEPS_11)
	@echo '   [Compile] $(CONFIG)/obj/alloc.o'
	$(CC) -c -o $(CONFIG)/obj/alloc.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/alloc.c

#
#   auth.o
#
DEPS_12 += $(CONFIG)/inc/bit.h
DEPS_12 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/auth.o: \
    src/auth.c $(DEPS_12)
	@echo '   [Compile] $(CONFIG)/obj/auth.o'
	$(CC) -c -o $(CONFIG)/obj/auth.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/auth.c

#
#   cgi.o
#
DEPS_13 += $(CONFIG)/inc/bit.h
DEPS_13 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/cgi.o: \
    src/cgi.c $(DEPS_13)
	@echo '   [Compile] $(CONFIG)/obj/cgi.o'
	$(CC) -c -o $(CONFIG)/obj/cgi.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/cgi.c

#
#   crypt.o
#
DEPS_14 += $(CONFIG)/inc/bit.h
DEPS_14 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/crypt.o: \
    src/crypt.c $(DEPS_14)
	@echo '   [Compile] $(CONFIG)/obj/crypt.o'
	$(CC) -c -o $(CONFIG)/obj/crypt.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/crypt.c

#
#   file.o
#
DEPS_15 += $(CONFIG)/inc/bit.h
DEPS_15 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/file.o: \
    src/file.c $(DEPS_15)
	@echo '   [Compile] $(CONFIG)/obj/file.o'
	$(CC) -c -o $(CONFIG)/obj/file.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/file.c

#
#   fs.o
#
DEPS_16 += $(CONFIG)/inc/bit.h
DEPS_16 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/fs.o: \
    src/fs.c $(DEPS_16)
	@echo '   [Compile] $(CONFIG)/obj/fs.o'
	$(CC) -c -o $(CONFIG)/obj/fs.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/fs.c

#
#   http.o
#
DEPS_17 += $(CONFIG)/inc/bit.h
DEPS_17 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/http.o: \
    src/http.c $(DEPS_17)
	@echo '   [Compile] $(CONFIG)/obj/http.o'
	$(CC) -c -o $(CONFIG)/obj/http.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/http.c

#
#   js.o
#
DEPS_18 += $(CONFIG)/inc/bit.h
DEPS_18 += $(CONFIG)/inc/js.h
DEPS_18 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/js.o: \
    src/js.c $(DEPS_18)
	@echo '   [Compile] $(CONFIG)/obj/js.o'
	$(CC) -c -o $(CONFIG)/obj/js.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/js.c

#
#   jst.o
#
DEPS_19 += $(CONFIG)/inc/bit.h
DEPS_19 += $(CONFIG)/inc/goahead.h
DEPS_19 += $(CONFIG)/inc/js.h

$(CONFIG)/obj/jst.o: \
    src/jst.c $(DEPS_19)
	@echo '   [Compile] $(CONFIG)/obj/jst.o'
	$(CC) -c -o $(CONFIG)/obj/jst.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/jst.c

#
#   options.o
#
DEPS_20 += $(CONFIG)/inc/bit.h
DEPS_20 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/options.o: \
    src/options.c $(DEPS_20)
	@echo '   [Compile] $(CONFIG)/obj/options.o'
	$(CC) -c -o $(CONFIG)/obj/options.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/options.c

#
#   osdep.o
#
DEPS_21 += $(CONFIG)/inc/bit.h
DEPS_21 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/osdep.o: \
    src/osdep.c $(DEPS_21)
	@echo '   [Compile] $(CONFIG)/obj/osdep.o'
	$(CC) -c -o $(CONFIG)/obj/osdep.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/osdep.c

#
#   rom-documents.o
#
DEPS_22 += $(CONFIG)/inc/bit.h
DEPS_22 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/rom-documents.o: \
    src/rom-documents.c $(DEPS_22)
	@echo '   [Compile] $(CONFIG)/obj/rom-documents.o'
	$(CC) -c -o $(CONFIG)/obj/rom-documents.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/rom-documents.c

#
#   route.o
#
DEPS_23 += $(CONFIG)/inc/bit.h
DEPS_23 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/route.o: \
    src/route.c $(DEPS_23)
	@echo '   [Compile] $(CONFIG)/obj/route.o'
	$(CC) -c -o $(CONFIG)/obj/route.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/route.c

#
#   runtime.o
#
DEPS_24 += $(CONFIG)/inc/bit.h
DEPS_24 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/runtime.o: \
    src/runtime.c $(DEPS_24)
	@echo '   [Compile] $(CONFIG)/obj/runtime.o'
	$(CC) -c -o $(CONFIG)/obj/runtime.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/runtime.c

#
#   socket.o
#
DEPS_25 += $(CONFIG)/inc/bit.h
DEPS_25 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/socket.o: \
    src/socket.c $(DEPS_25)
	@echo '   [Compile] $(CONFIG)/obj/socket.o'
	$(CC) -c -o $(CONFIG)/obj/socket.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/socket.c

#
#   upload.o
#
DEPS_26 += $(CONFIG)/inc/bit.h
DEPS_26 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/upload.o: \
    src/upload.c $(DEPS_26)
	@echo '   [Compile] $(CONFIG)/obj/upload.o'
	$(CC) -c -o $(CONFIG)/obj/upload.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/upload.c

#
#   est.o
#
DEPS_27 += $(CONFIG)/inc/bit.h
DEPS_27 += $(CONFIG)/inc/goahead.h
DEPS_27 += $(CONFIG)/inc/est.h

$(CONFIG)/obj/est.o: \
    src/ssl/est.c $(DEPS_27)
	@echo '   [Compile] $(CONFIG)/obj/est.o'
	$(CC) -c -o $(CONFIG)/obj/est.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/ssl/est.c

#
#   matrixssl.o
#
DEPS_28 += $(CONFIG)/inc/bit.h
DEPS_28 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/matrixssl.o: \
    src/ssl/matrixssl.c $(DEPS_28)
	@echo '   [Compile] $(CONFIG)/obj/matrixssl.o'
	$(CC) -c -o $(CONFIG)/obj/matrixssl.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/ssl/matrixssl.c

#
#   nanossl.o
#
DEPS_29 += $(CONFIG)/inc/bit.h

$(CONFIG)/obj/nanossl.o: \
    src/ssl/nanossl.c $(DEPS_29)
	@echo '   [Compile] $(CONFIG)/obj/nanossl.o'
	$(CC) -c -o $(CONFIG)/obj/nanossl.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/ssl/nanossl.c

#
#   openssl.o
#
DEPS_30 += $(CONFIG)/inc/bit.h
DEPS_30 += $(CONFIG)/inc/bitos.h
DEPS_30 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/openssl.o: \
    src/ssl/openssl.c $(DEPS_30)
	@echo '   [Compile] $(CONFIG)/obj/openssl.o'
	$(CC) -c -o $(CONFIG)/obj/openssl.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/ssl/openssl.c

#
#   libgo
#
DEPS_31 += $(CONFIG)/inc/est.h
DEPS_31 += $(CONFIG)/inc/bit.h
DEPS_31 += $(CONFIG)/inc/bitos.h
DEPS_31 += $(CONFIG)/obj/estLib.o
ifeq ($(BIT_PACK_EST),1)
    DEPS_31 += $(CONFIG)/bin/libest.out
endif
DEPS_31 += $(CONFIG)/inc/goahead.h
DEPS_31 += $(CONFIG)/inc/js.h
DEPS_31 += $(CONFIG)/obj/action.o
DEPS_31 += $(CONFIG)/obj/alloc.o
DEPS_31 += $(CONFIG)/obj/auth.o
DEPS_31 += $(CONFIG)/obj/cgi.o
DEPS_31 += $(CONFIG)/obj/crypt.o
DEPS_31 += $(CONFIG)/obj/file.o
DEPS_31 += $(CONFIG)/obj/fs.o
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
DEPS_31 += $(CONFIG)/obj/nanossl.o
DEPS_31 += $(CONFIG)/obj/openssl.o

ifeq ($(BIT_PACK_MATRIXSSL),1)
    LIBS_31 += -lmatrixssl
    LIBPATHS_31 += -L$(BIT_PACK_MATRIXSSL_PATH)
endif
ifeq ($(BIT_PACK_NANOSSL),1)
    LIBS_31 += -lssls
    LIBPATHS_31 += -L$(BIT_PACK_NANOSSL_PATH)/bin
endif
ifeq ($(BIT_PACK_OPENSSL),1)
    LIBS_31 += -lssl
    LIBPATHS_31 += -L$(BIT_PACK_OPENSSL_PATH)
endif
ifeq ($(BIT_PACK_OPENSSL),1)
    LIBS_31 += -lcrypto
    LIBPATHS_31 += -L$(BIT_PACK_OPENSSL_PATH)
endif

$(CONFIG)/bin/libgo.out: $(DEPS_31)
	@echo '      [Link] $(CONFIG)/bin/libgo.out'
	$(CC) -r -o $(CONFIG)/bin/libgo.out $(LDFLAGS) $(LIBPATHS)    "$(CONFIG)/obj/action.o" "$(CONFIG)/obj/alloc.o" "$(CONFIG)/obj/auth.o" "$(CONFIG)/obj/cgi.o" "$(CONFIG)/obj/crypt.o" "$(CONFIG)/obj/file.o" "$(CONFIG)/obj/fs.o" "$(CONFIG)/obj/http.o" "$(CONFIG)/obj/js.o" "$(CONFIG)/obj/jst.o" "$(CONFIG)/obj/options.o" "$(CONFIG)/obj/osdep.o" "$(CONFIG)/obj/rom-documents.o" "$(CONFIG)/obj/route.o" "$(CONFIG)/obj/runtime.o" "$(CONFIG)/obj/socket.o" "$(CONFIG)/obj/upload.o" "$(CONFIG)/obj/est.o" "$(CONFIG)/obj/matrixssl.o" "$(CONFIG)/obj/nanossl.o" "$(CONFIG)/obj/openssl.o" $(LIBPATHS_31) $(LIBS_31) $(LIBS_31) $(LIBS) 

#
#   goahead.o
#
DEPS_32 += $(CONFIG)/inc/bit.h
DEPS_32 += $(CONFIG)/inc/goahead.h
DEPS_32 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/goahead.o: \
    src/goahead.c $(DEPS_32)
	@echo '   [Compile] $(CONFIG)/obj/goahead.o'
	$(CC) -c -o $(CONFIG)/obj/goahead.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/goahead.c

#
#   goahead
#
DEPS_33 += $(CONFIG)/inc/est.h
DEPS_33 += $(CONFIG)/inc/bit.h
DEPS_33 += $(CONFIG)/inc/bitos.h
DEPS_33 += $(CONFIG)/obj/estLib.o
ifeq ($(BIT_PACK_EST),1)
    DEPS_33 += $(CONFIG)/bin/libest.out
endif
DEPS_33 += $(CONFIG)/inc/goahead.h
DEPS_33 += $(CONFIG)/inc/js.h
DEPS_33 += $(CONFIG)/obj/action.o
DEPS_33 += $(CONFIG)/obj/alloc.o
DEPS_33 += $(CONFIG)/obj/auth.o
DEPS_33 += $(CONFIG)/obj/cgi.o
DEPS_33 += $(CONFIG)/obj/crypt.o
DEPS_33 += $(CONFIG)/obj/file.o
DEPS_33 += $(CONFIG)/obj/fs.o
DEPS_33 += $(CONFIG)/obj/http.o
DEPS_33 += $(CONFIG)/obj/js.o
DEPS_33 += $(CONFIG)/obj/jst.o
DEPS_33 += $(CONFIG)/obj/options.o
DEPS_33 += $(CONFIG)/obj/osdep.o
DEPS_33 += $(CONFIG)/obj/rom-documents.o
DEPS_33 += $(CONFIG)/obj/route.o
DEPS_33 += $(CONFIG)/obj/runtime.o
DEPS_33 += $(CONFIG)/obj/socket.o
DEPS_33 += $(CONFIG)/obj/upload.o
DEPS_33 += $(CONFIG)/obj/est.o
DEPS_33 += $(CONFIG)/obj/matrixssl.o
DEPS_33 += $(CONFIG)/obj/nanossl.o
DEPS_33 += $(CONFIG)/obj/openssl.o
DEPS_33 += $(CONFIG)/bin/libgo.out
DEPS_33 += $(CONFIG)/obj/goahead.o

ifeq ($(BIT_PACK_MATRIXSSL),1)
    LIBS_33 += -lmatrixssl
    LIBPATHS_33 += -L$(BIT_PACK_MATRIXSSL_PATH)
endif
ifeq ($(BIT_PACK_NANOSSL),1)
    LIBS_33 += -lssls
    LIBPATHS_33 += -L$(BIT_PACK_NANOSSL_PATH)/bin
endif
ifeq ($(BIT_PACK_OPENSSL),1)
    LIBS_33 += -lssl
    LIBPATHS_33 += -L$(BIT_PACK_OPENSSL_PATH)
endif
ifeq ($(BIT_PACK_OPENSSL),1)
    LIBS_33 += -lcrypto
    LIBPATHS_33 += -L$(BIT_PACK_OPENSSL_PATH)
endif

$(CONFIG)/bin/goahead.out: $(DEPS_33)
	@echo '      [Link] $(CONFIG)/bin/goahead.out'
	$(CC) -o $(CONFIG)/bin/goahead.out $(LDFLAGS) $(LIBPATHS)    "$(CONFIG)/obj/goahead.o" $(LIBPATHS_33) $(LIBS_33) $(LIBS_33) $(LIBS) -Wl,-r 

#
#   test.o
#
DEPS_34 += $(CONFIG)/inc/bit.h
DEPS_34 += $(CONFIG)/inc/goahead.h
DEPS_34 += $(CONFIG)/inc/js.h
DEPS_34 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/test.o: \
    test/test.c $(DEPS_34)
	@echo '   [Compile] $(CONFIG)/obj/test.o'
	$(CC) -c -o $(CONFIG)/obj/test.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" test/test.c

#
#   goahead-test
#
DEPS_35 += $(CONFIG)/inc/est.h
DEPS_35 += $(CONFIG)/inc/bit.h
DEPS_35 += $(CONFIG)/inc/bitos.h
DEPS_35 += $(CONFIG)/obj/estLib.o
ifeq ($(BIT_PACK_EST),1)
    DEPS_35 += $(CONFIG)/bin/libest.out
endif
DEPS_35 += $(CONFIG)/inc/goahead.h
DEPS_35 += $(CONFIG)/inc/js.h
DEPS_35 += $(CONFIG)/obj/action.o
DEPS_35 += $(CONFIG)/obj/alloc.o
DEPS_35 += $(CONFIG)/obj/auth.o
DEPS_35 += $(CONFIG)/obj/cgi.o
DEPS_35 += $(CONFIG)/obj/crypt.o
DEPS_35 += $(CONFIG)/obj/file.o
DEPS_35 += $(CONFIG)/obj/fs.o
DEPS_35 += $(CONFIG)/obj/http.o
DEPS_35 += $(CONFIG)/obj/js.o
DEPS_35 += $(CONFIG)/obj/jst.o
DEPS_35 += $(CONFIG)/obj/options.o
DEPS_35 += $(CONFIG)/obj/osdep.o
DEPS_35 += $(CONFIG)/obj/rom-documents.o
DEPS_35 += $(CONFIG)/obj/route.o
DEPS_35 += $(CONFIG)/obj/runtime.o
DEPS_35 += $(CONFIG)/obj/socket.o
DEPS_35 += $(CONFIG)/obj/upload.o
DEPS_35 += $(CONFIG)/obj/est.o
DEPS_35 += $(CONFIG)/obj/matrixssl.o
DEPS_35 += $(CONFIG)/obj/nanossl.o
DEPS_35 += $(CONFIG)/obj/openssl.o
DEPS_35 += $(CONFIG)/bin/libgo.out
DEPS_35 += $(CONFIG)/obj/test.o

ifeq ($(BIT_PACK_MATRIXSSL),1)
    LIBS_35 += -lmatrixssl
    LIBPATHS_35 += -L$(BIT_PACK_MATRIXSSL_PATH)
endif
ifeq ($(BIT_PACK_NANOSSL),1)
    LIBS_35 += -lssls
    LIBPATHS_35 += -L$(BIT_PACK_NANOSSL_PATH)/bin
endif
ifeq ($(BIT_PACK_OPENSSL),1)
    LIBS_35 += -lssl
    LIBPATHS_35 += -L$(BIT_PACK_OPENSSL_PATH)
endif
ifeq ($(BIT_PACK_OPENSSL),1)
    LIBS_35 += -lcrypto
    LIBPATHS_35 += -L$(BIT_PACK_OPENSSL_PATH)
endif

$(CONFIG)/bin/goahead-test.out: $(DEPS_35)
	@echo '      [Link] $(CONFIG)/bin/goahead-test.out'
	$(CC) -o $(CONFIG)/bin/goahead-test.out $(LDFLAGS) $(LIBPATHS)    "$(CONFIG)/obj/test.o" $(LIBPATHS_35) $(LIBS_35) $(LIBS_35) $(LIBS) -Wl,-r 

#
#   gopass.o
#
DEPS_36 += $(CONFIG)/inc/bit.h
DEPS_36 += $(CONFIG)/inc/goahead.h
DEPS_36 += $(CONFIG)/inc/bitos.h

$(CONFIG)/obj/gopass.o: \
    src/utils/gopass.c $(DEPS_36)
	@echo '   [Compile] $(CONFIG)/obj/gopass.o'
	$(CC) -c -o $(CONFIG)/obj/gopass.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(BIT_PACK_MATRIXSSL_PATH)" "-I$(BIT_PACK_MATRIXSSL_PATH)/matrixssl" "-I$(BIT_PACK_NANOSSL_PATH)/src" "-I$(BIT_PACK_OPENSSL_PATH)/include" src/utils/gopass.c

#
#   gopass
#
DEPS_37 += $(CONFIG)/inc/est.h
DEPS_37 += $(CONFIG)/inc/bit.h
DEPS_37 += $(CONFIG)/inc/bitos.h
DEPS_37 += $(CONFIG)/obj/estLib.o
ifeq ($(BIT_PACK_EST),1)
    DEPS_37 += $(CONFIG)/bin/libest.out
endif
DEPS_37 += $(CONFIG)/inc/goahead.h
DEPS_37 += $(CONFIG)/inc/js.h
DEPS_37 += $(CONFIG)/obj/action.o
DEPS_37 += $(CONFIG)/obj/alloc.o
DEPS_37 += $(CONFIG)/obj/auth.o
DEPS_37 += $(CONFIG)/obj/cgi.o
DEPS_37 += $(CONFIG)/obj/crypt.o
DEPS_37 += $(CONFIG)/obj/file.o
DEPS_37 += $(CONFIG)/obj/fs.o
DEPS_37 += $(CONFIG)/obj/http.o
DEPS_37 += $(CONFIG)/obj/js.o
DEPS_37 += $(CONFIG)/obj/jst.o
DEPS_37 += $(CONFIG)/obj/options.o
DEPS_37 += $(CONFIG)/obj/osdep.o
DEPS_37 += $(CONFIG)/obj/rom-documents.o
DEPS_37 += $(CONFIG)/obj/route.o
DEPS_37 += $(CONFIG)/obj/runtime.o
DEPS_37 += $(CONFIG)/obj/socket.o
DEPS_37 += $(CONFIG)/obj/upload.o
DEPS_37 += $(CONFIG)/obj/est.o
DEPS_37 += $(CONFIG)/obj/matrixssl.o
DEPS_37 += $(CONFIG)/obj/nanossl.o
DEPS_37 += $(CONFIG)/obj/openssl.o
DEPS_37 += $(CONFIG)/bin/libgo.out
DEPS_37 += $(CONFIG)/obj/gopass.o

ifeq ($(BIT_PACK_MATRIXSSL),1)
    LIBS_37 += -lmatrixssl
    LIBPATHS_37 += -L$(BIT_PACK_MATRIXSSL_PATH)
endif
ifeq ($(BIT_PACK_NANOSSL),1)
    LIBS_37 += -lssls
    LIBPATHS_37 += -L$(BIT_PACK_NANOSSL_PATH)/bin
endif
ifeq ($(BIT_PACK_OPENSSL),1)
    LIBS_37 += -lssl
    LIBPATHS_37 += -L$(BIT_PACK_OPENSSL_PATH)
endif
ifeq ($(BIT_PACK_OPENSSL),1)
    LIBS_37 += -lcrypto
    LIBPATHS_37 += -L$(BIT_PACK_OPENSSL_PATH)
endif

$(CONFIG)/bin/gopass.out: $(DEPS_37)
	@echo '      [Link] $(CONFIG)/bin/gopass.out'
	$(CC) -o $(CONFIG)/bin/gopass.out $(LDFLAGS) $(LIBPATHS)    "$(CONFIG)/obj/gopass.o" $(LIBPATHS_37) $(LIBS_37) $(LIBS_37) $(LIBS) -Wl,-r 

#
#   stop
#
stop: $(DEPS_38)

#
#   installBinary
#
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

#
#   run
#
run: $(DEPS_43)
	cd src; goahead -v ; cd ..
