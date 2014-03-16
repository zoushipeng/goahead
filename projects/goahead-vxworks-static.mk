#
#   goahead-vxworks-static.mk -- Makefile to build Embedthis GoAhead for vxworks
#

NAME                  := goahead
VERSION               := 3.3.1
PROFILE               ?= static
ARCH                  ?= $(shell echo $(WIND_HOST_TYPE) | sed 's/-.*//')
CPU                   ?= $(subst X86,PENTIUM,$(shell echo $(ARCH) | tr a-z A-Z))
OS                    ?= vxworks
CC                    ?= cc$(subst x86,pentium,$(ARCH))
LD                    ?= link
CONFIG                ?= $(OS)-$(ARCH)-$(PROFILE)
LBIN                  ?= $(CONFIG)/bin
PATH                  := $(LBIN):$(PATH)

ME_EXT_EST            ?= 1
ME_EXT_SSL            ?= 1

ME_EXT_COMPILER_PATH  ?= cc$(subst x86,pentium,$(ARCH))
ME_EXT_DOXYGEN_PATH   ?= doxygen
ME_EXT_DSI_PATH       ?= dsi
ME_EXT_EST_PATH       ?= src/paks/est/estLib.c
ME_EXT_LIB_PATH       ?= ar
ME_EXT_LINK_PATH      ?= link
ME_EXT_MAN_PATH       ?= man
ME_EXT_MAN2HTML_PATH  ?= man2html
ME_EXT_MATRIXSSL_PATH ?= /usr/src/matrixssl
ME_EXT_NANOSSL_PATH   ?= /usr/src/nanossl
ME_EXT_OPENSSL_PATH   ?= /usr/src/openssl
ME_EXT_OSDEP_PATH     ?= src/paks/osdep
ME_EXT_PMAKER_PATH    ?= pmaker
ME_EXT_SSL_PATH       ?= ssl
ME_EXT_UTEST_PATH     ?= utest
ME_EXT_VXWORKS_PATH   ?= $(WIND_BASE)
ME_EXT_WINSDK_PATH    ?= winsdk
ME_EXT_ZIP_PATH       ?= zip

export WIND_HOME      ?= $(WIND_BASE)/..
export PATH           := $(WIND_GNU_PATH)/$(WIND_HOST_TYPE)/bin:$(PATH)

CFLAGS                += -fno-builtin -fno-defer-pop -fvolatile -w
DFLAGS                += -DVXWORKS -DRW_MULTI_THREAD -D_GNU_TOOL -DCPU=PENTIUM $(patsubst %,-D%,$(filter ME_%,$(MAKEFLAGS))) -DME_EXT_EST=$(ME_EXT_EST) -DME_EXT_SSL=$(ME_EXT_SSL) 
IFLAGS                += "-I$(CONFIG)/inc -I$(WIND_BASE)/target/h -I$(WIND_BASE)/target/h/wrn/coreip"
LDFLAGS               += '-Wl,-r'
LIBPATHS              += -L$(CONFIG)/bin
LIBS                  += -lgcc

DEBUG                 ?= debug
CFLAGS-debug          ?= -g
DFLAGS-debug          ?= -DME_DEBUG
LDFLAGS-debug         ?= -g
DFLAGS-release        ?= 
CFLAGS-release        ?= -O2
LDFLAGS-release       ?= 
CFLAGS                += $(CFLAGS-$(DEBUG))
DFLAGS                += $(DFLAGS-$(DEBUG))
LDFLAGS               += $(LDFLAGS-$(DEBUG))

ME_ROOT_PREFIX        ?= deploy
ME_BASE_PREFIX        ?= $(ME_ROOT_PREFIX)
ME_DATA_PREFIX        ?= $(ME_VAPP_PREFIX)
ME_STATE_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_BIN_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_INC_PREFIX         ?= $(ME_VAPP_PREFIX)/inc
ME_LIB_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_MAN_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_SBIN_PREFIX        ?= $(ME_VAPP_PREFIX)
ME_ETC_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_WEB_PREFIX         ?= $(ME_VAPP_PREFIX)/web
ME_LOG_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_SPOOL_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_CACHE_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_APP_PREFIX         ?= $(ME_BASE_PREFIX)
ME_VAPP_PREFIX        ?= $(ME_APP_PREFIX)
ME_SRC_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/src/$(NAME)-$(VERSION)


ifeq ($(ME_EXT_EST),1)
    TARGETS           += $(CONFIG)/bin/libest.a
endif
TARGETS               += $(CONFIG)/bin/ca.crt
TARGETS               += $(CONFIG)/bin/libgo.a
TARGETS               += $(CONFIG)/bin/goahead.out
TARGETS               += $(CONFIG)/bin/goahead-test.out
TARGETS               += $(CONFIG)/bin/gopass.out

unexport CDPATH

ifndef SHOW
.SILENT:
endif

all build compile: prep $(TARGETS)

.PHONY: prep

prep:
	@echo "      [Info] Use "make SHOW=1" to trace executed commands."
	@if [ "$(CONFIG)" = "" ] ; then echo WARNING: CONFIG not set ; exit 255 ; fi
	@if [ "$(ME_APP_PREFIX)" = "" ] ; then echo WARNING: ME_APP_PREFIX not set ; exit 255 ; fi
	@if [ "$(WIND_BASE)" = "" ] ; then echo WARNING: WIND_BASE not set. Run wrenv.sh. ; exit 255 ; fi
	@if [ "$(WIND_HOST_TYPE)" = "" ] ; then echo WARNING: WIND_HOST_TYPE not set. Run wrenv.sh. ; exit 255 ; fi
	@if [ "$(WIND_GNU_PATH)" = "" ] ; then echo WARNING: WIND_GNU_PATH not set. Run wrenv.sh. ; exit 255 ; fi
	@[ ! -x $(CONFIG)/bin ] && mkdir -p $(CONFIG)/bin; true
	@[ ! -x $(CONFIG)/inc ] && mkdir -p $(CONFIG)/inc; true
	@[ ! -x $(CONFIG)/obj ] && mkdir -p $(CONFIG)/obj; true
	@[ ! -f $(CONFIG)/inc/osdep.h ] && cp src/paks/osdep/osdep.h $(CONFIG)/inc/osdep.h ; true
	@if ! diff $(CONFIG)/inc/osdep.h src/paks/osdep/osdep.h >/dev/null ; then\
		cp src/paks/osdep/osdep.h $(CONFIG)/inc/osdep.h  ; \
	fi; true
	@[ ! -f $(CONFIG)/inc/me.h ] && cp projects/goahead-vxworks-static-me.h $(CONFIG)/inc/me.h ; true
	@if ! diff $(CONFIG)/inc/me.h projects/goahead-vxworks-static-me.h >/dev/null ; then\
		cp projects/goahead-vxworks-static-me.h $(CONFIG)/inc/me.h  ; \
	fi; true
	@if [ -f "$(CONFIG)/.makeflags" ] ; then \
		if [ "$(MAKEFLAGS)" != " ` cat $(CONFIG)/.makeflags`" ] ; then \
			echo "   [Warning] Make flags have changed since the last build: "`cat $(CONFIG)/.makeflags`"" ; \
		fi ; \
	fi
	@echo $(MAKEFLAGS) >$(CONFIG)/.makeflags

clean:
	rm -f "$(CONFIG)/bin/libest.a"
	rm -f "$(CONFIG)/bin/ca.crt"
	rm -f "$(CONFIG)/bin/libgo.a"
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
	echo 3.3.1

#
#   est.h
#
$(CONFIG)/inc/est.h: $(DEPS_2)
	@echo '      [Copy] $(CONFIG)/inc/est.h'
	mkdir -p "$(CONFIG)/inc"
	cp src/paks/est/est.h $(CONFIG)/inc/est.h

#
#   me.h
#
$(CONFIG)/inc/me.h: $(DEPS_3)
	@echo '      [Copy] $(CONFIG)/inc/me.h'

#
#   osdep.h
#
DEPS_4 += $(CONFIG)/inc/me.h

$(CONFIG)/inc/osdep.h: $(DEPS_4)
	@echo '      [Copy] $(CONFIG)/inc/osdep.h'
	mkdir -p "$(CONFIG)/inc"
	cp src/paks/osdep/osdep.h $(CONFIG)/inc/osdep.h

#
#   estLib.o
#
DEPS_5 += $(CONFIG)/inc/me.h
DEPS_5 += $(CONFIG)/inc/est.h
DEPS_5 += $(CONFIG)/inc/osdep.h

$(CONFIG)/obj/estLib.o: \
    src/paks/est/estLib.c $(DEPS_5)
	@echo '   [Compile] $(CONFIG)/obj/estLib.o'
	 -c -o $(CONFIG)/obj/estLib.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/paks/est/estLib.c

ifeq ($(ME_EXT_EST),1)
#
#   libest
#
DEPS_6 += $(CONFIG)/inc/est.h
DEPS_6 += $(CONFIG)/inc/me.h
DEPS_6 += $(CONFIG)/inc/osdep.h
DEPS_6 += $(CONFIG)/obj/estLib.o

$(CONFIG)/bin/libest.a: $(DEPS_6)
	@echo '      [Link] $(CONFIG)/bin/libest.a'
	 -cr $(CONFIG)/bin/libest.a "$(CONFIG)/obj/estLib.o"
endif

#
#   ca-crt
#
DEPS_7 += src/paks/est/ca.crt

$(CONFIG)/bin/ca.crt: $(DEPS_7)
	@echo '      [Copy] $(CONFIG)/bin/ca.crt'
	mkdir -p "$(CONFIG)/bin"
	cp src/paks/est/ca.crt $(CONFIG)/bin/ca.crt

#
#   bitos.h
#
$(CONFIG)/inc/bitos.h: $(DEPS_8)
	@echo '      [Copy] $(CONFIG)/inc/bitos.h'
	mkdir -p "$(CONFIG)/inc"
	cp src/bitos.h $(CONFIG)/inc/bitos.h

#
#   goahead.h
#
$(CONFIG)/inc/goahead.h: $(DEPS_9)
	@echo '      [Copy] $(CONFIG)/inc/goahead.h'
	mkdir -p "$(CONFIG)/inc"
	cp src/goahead.h $(CONFIG)/inc/goahead.h

#
#   js.h
#
$(CONFIG)/inc/js.h: $(DEPS_10)
	@echo '      [Copy] $(CONFIG)/inc/js.h'
	mkdir -p "$(CONFIG)/inc"
	cp src/js.h $(CONFIG)/inc/js.h

#
#   action.o
#
DEPS_11 += $(CONFIG)/inc/me.h
DEPS_11 += $(CONFIG)/inc/goahead.h
DEPS_11 += $(CONFIG)/inc/osdep.h

$(CONFIG)/obj/action.o: \
    src/action.c $(DEPS_11)
	@echo '   [Compile] $(CONFIG)/obj/action.o'
	 -c -o $(CONFIG)/obj/action.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/action.c

#
#   alloc.o
#
DEPS_12 += $(CONFIG)/inc/me.h
DEPS_12 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/alloc.o: \
    src/alloc.c $(DEPS_12)
	@echo '   [Compile] $(CONFIG)/obj/alloc.o'
	 -c -o $(CONFIG)/obj/alloc.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/alloc.c

#
#   auth.o
#
DEPS_13 += $(CONFIG)/inc/me.h
DEPS_13 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/auth.o: \
    src/auth.c $(DEPS_13)
	@echo '   [Compile] $(CONFIG)/obj/auth.o'
	 -c -o $(CONFIG)/obj/auth.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/auth.c

#
#   cgi.o
#
DEPS_14 += $(CONFIG)/inc/me.h
DEPS_14 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/cgi.o: \
    src/cgi.c $(DEPS_14)
	@echo '   [Compile] $(CONFIG)/obj/cgi.o'
	 -c -o $(CONFIG)/obj/cgi.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/cgi.c

#
#   crypt.o
#
DEPS_15 += $(CONFIG)/inc/me.h
DEPS_15 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/crypt.o: \
    src/crypt.c $(DEPS_15)
	@echo '   [Compile] $(CONFIG)/obj/crypt.o'
	 -c -o $(CONFIG)/obj/crypt.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/crypt.c

#
#   file.o
#
DEPS_16 += $(CONFIG)/inc/me.h
DEPS_16 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/file.o: \
    src/file.c $(DEPS_16)
	@echo '   [Compile] $(CONFIG)/obj/file.o'
	 -c -o $(CONFIG)/obj/file.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/file.c

#
#   fs.o
#
DEPS_17 += $(CONFIG)/inc/me.h
DEPS_17 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/fs.o: \
    src/fs.c $(DEPS_17)
	@echo '   [Compile] $(CONFIG)/obj/fs.o'
	 -c -o $(CONFIG)/obj/fs.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/fs.c

#
#   http.o
#
DEPS_18 += $(CONFIG)/inc/me.h
DEPS_18 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/http.o: \
    src/http.c $(DEPS_18)
	@echo '   [Compile] $(CONFIG)/obj/http.o'
	 -c -o $(CONFIG)/obj/http.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/http.c

#
#   js.o
#
DEPS_19 += $(CONFIG)/inc/me.h
DEPS_19 += $(CONFIG)/inc/js.h
DEPS_19 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/js.o: \
    src/js.c $(DEPS_19)
	@echo '   [Compile] $(CONFIG)/obj/js.o'
	 -c -o $(CONFIG)/obj/js.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/js.c

#
#   jst.o
#
DEPS_20 += $(CONFIG)/inc/me.h
DEPS_20 += $(CONFIG)/inc/goahead.h
DEPS_20 += $(CONFIG)/inc/js.h

$(CONFIG)/obj/jst.o: \
    src/jst.c $(DEPS_20)
	@echo '   [Compile] $(CONFIG)/obj/jst.o'
	 -c -o $(CONFIG)/obj/jst.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/jst.c

#
#   options.o
#
DEPS_21 += $(CONFIG)/inc/me.h
DEPS_21 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/options.o: \
    src/options.c $(DEPS_21)
	@echo '   [Compile] $(CONFIG)/obj/options.o'
	 -c -o $(CONFIG)/obj/options.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/options.c

#
#   osdep.o
#
DEPS_22 += $(CONFIG)/inc/me.h
DEPS_22 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/osdep.o: \
    src/osdep.c $(DEPS_22)
	@echo '   [Compile] $(CONFIG)/obj/osdep.o'
	 -c -o $(CONFIG)/obj/osdep.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/osdep.c

#
#   rom-documents.o
#
DEPS_23 += $(CONFIG)/inc/me.h
DEPS_23 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/rom-documents.o: \
    src/rom-documents.c $(DEPS_23)
	@echo '   [Compile] $(CONFIG)/obj/rom-documents.o'
	 -c -o $(CONFIG)/obj/rom-documents.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/rom-documents.c

#
#   route.o
#
DEPS_24 += $(CONFIG)/inc/me.h
DEPS_24 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/route.o: \
    src/route.c $(DEPS_24)
	@echo '   [Compile] $(CONFIG)/obj/route.o'
	 -c -o $(CONFIG)/obj/route.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/route.c

#
#   runtime.o
#
DEPS_25 += $(CONFIG)/inc/me.h
DEPS_25 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/runtime.o: \
    src/runtime.c $(DEPS_25)
	@echo '   [Compile] $(CONFIG)/obj/runtime.o'
	 -c -o $(CONFIG)/obj/runtime.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/runtime.c

#
#   socket.o
#
DEPS_26 += $(CONFIG)/inc/me.h
DEPS_26 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/socket.o: \
    src/socket.c $(DEPS_26)
	@echo '   [Compile] $(CONFIG)/obj/socket.o'
	 -c -o $(CONFIG)/obj/socket.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/socket.c

#
#   upload.o
#
DEPS_27 += $(CONFIG)/inc/me.h
DEPS_27 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/upload.o: \
    src/upload.c $(DEPS_27)
	@echo '   [Compile] $(CONFIG)/obj/upload.o'
	 -c -o $(CONFIG)/obj/upload.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/upload.c

#
#   est.o
#
DEPS_28 += $(CONFIG)/inc/me.h
DEPS_28 += $(CONFIG)/inc/goahead.h
DEPS_28 += $(CONFIG)/inc/est.h

$(CONFIG)/obj/est.o: \
    src/ssl/est.c $(DEPS_28)
	@echo '   [Compile] $(CONFIG)/obj/est.o'
	 -c -o $(CONFIG)/obj/est.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/ssl/est.c

#
#   matrixssl.o
#
DEPS_29 += $(CONFIG)/inc/me.h
DEPS_29 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/matrixssl.o: \
    src/ssl/matrixssl.c $(DEPS_29)
	@echo '   [Compile] $(CONFIG)/obj/matrixssl.o'
	 -c -o $(CONFIG)/obj/matrixssl.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/ssl/matrixssl.c

#
#   nanossl.o
#
DEPS_30 += $(CONFIG)/inc/me.h

$(CONFIG)/obj/nanossl.o: \
    src/ssl/nanossl.c $(DEPS_30)
	@echo '   [Compile] $(CONFIG)/obj/nanossl.o'
	 -c -o $(CONFIG)/obj/nanossl.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/ssl/nanossl.c

#
#   openssl.o
#
DEPS_31 += $(CONFIG)/inc/me.h
DEPS_31 += $(CONFIG)/inc/osdep.h
DEPS_31 += $(CONFIG)/inc/goahead.h

$(CONFIG)/obj/openssl.o: \
    src/ssl/openssl.c $(DEPS_31)
	@echo '   [Compile] $(CONFIG)/obj/openssl.o'
	 -c -o $(CONFIG)/obj/openssl.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/ssl/openssl.c

#
#   libgo
#
DEPS_32 += $(CONFIG)/inc/est.h
DEPS_32 += $(CONFIG)/inc/me.h
DEPS_32 += $(CONFIG)/inc/osdep.h
DEPS_32 += $(CONFIG)/obj/estLib.o
ifeq ($(ME_EXT_EST),1)
    DEPS_32 += $(CONFIG)/bin/libest.a
endif
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

$(CONFIG)/bin/libgo.a: $(DEPS_32)
	@echo '      [Link] $(CONFIG)/bin/libgo.a'
	 -cr $(CONFIG)/bin/libgo.a "$(CONFIG)/obj/action.o" "$(CONFIG)/obj/alloc.o" "$(CONFIG)/obj/auth.o" "$(CONFIG)/obj/cgi.o" "$(CONFIG)/obj/crypt.o" "$(CONFIG)/obj/file.o" "$(CONFIG)/obj/fs.o" "$(CONFIG)/obj/http.o" "$(CONFIG)/obj/js.o" "$(CONFIG)/obj/jst.o" "$(CONFIG)/obj/options.o" "$(CONFIG)/obj/osdep.o" "$(CONFIG)/obj/rom-documents.o" "$(CONFIG)/obj/route.o" "$(CONFIG)/obj/runtime.o" "$(CONFIG)/obj/socket.o" "$(CONFIG)/obj/upload.o" "$(CONFIG)/obj/est.o" "$(CONFIG)/obj/matrixssl.o" "$(CONFIG)/obj/nanossl.o" "$(CONFIG)/obj/openssl.o"

#
#   goahead.o
#
DEPS_33 += $(CONFIG)/inc/me.h
DEPS_33 += $(CONFIG)/inc/goahead.h
DEPS_33 += $(CONFIG)/inc/osdep.h

$(CONFIG)/obj/goahead.o: \
    src/goahead.c $(DEPS_33)
	@echo '   [Compile] $(CONFIG)/obj/goahead.o'
	 -c -o $(CONFIG)/obj/goahead.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/goahead.c

#
#   goahead
#
DEPS_34 += $(CONFIG)/inc/est.h
DEPS_34 += $(CONFIG)/inc/me.h
DEPS_34 += $(CONFIG)/inc/osdep.h
DEPS_34 += $(CONFIG)/obj/estLib.o
ifeq ($(ME_EXT_EST),1)
    DEPS_34 += $(CONFIG)/bin/libest.a
endif
DEPS_34 += $(CONFIG)/inc/bitos.h
DEPS_34 += $(CONFIG)/inc/goahead.h
DEPS_34 += $(CONFIG)/inc/js.h
DEPS_34 += $(CONFIG)/obj/action.o
DEPS_34 += $(CONFIG)/obj/alloc.o
DEPS_34 += $(CONFIG)/obj/auth.o
DEPS_34 += $(CONFIG)/obj/cgi.o
DEPS_34 += $(CONFIG)/obj/crypt.o
DEPS_34 += $(CONFIG)/obj/file.o
DEPS_34 += $(CONFIG)/obj/fs.o
DEPS_34 += $(CONFIG)/obj/http.o
DEPS_34 += $(CONFIG)/obj/js.o
DEPS_34 += $(CONFIG)/obj/jst.o
DEPS_34 += $(CONFIG)/obj/options.o
DEPS_34 += $(CONFIG)/obj/osdep.o
DEPS_34 += $(CONFIG)/obj/rom-documents.o
DEPS_34 += $(CONFIG)/obj/route.o
DEPS_34 += $(CONFIG)/obj/runtime.o
DEPS_34 += $(CONFIG)/obj/socket.o
DEPS_34 += $(CONFIG)/obj/upload.o
DEPS_34 += $(CONFIG)/obj/est.o
DEPS_34 += $(CONFIG)/obj/matrixssl.o
DEPS_34 += $(CONFIG)/obj/nanossl.o
DEPS_34 += $(CONFIG)/obj/openssl.o
DEPS_34 += $(CONFIG)/bin/libgo.a
DEPS_34 += $(CONFIG)/obj/goahead.o

LIBS_34 += -lgo
ifeq ($(ME_EXT_EST),1)
    LIBS_34 += -lest
endif

$(CONFIG)/bin/goahead.out: $(DEPS_34)
	@echo '      [Link] $(CONFIG)/bin/goahead.out'
	 -o $(CONFIG)/bin/goahead.out $(LDFLAGS) $(LIBPATHS) "$(CONFIG)/obj/goahead.o" $(LIBPATHS_34) $(LIBS_34) $(LIBS_34) $(LIBS) -Wl,-r 

#
#   test.o
#
DEPS_35 += $(CONFIG)/inc/me.h
DEPS_35 += $(CONFIG)/inc/goahead.h
DEPS_35 += $(CONFIG)/inc/js.h
DEPS_35 += $(CONFIG)/inc/osdep.h

$(CONFIG)/obj/test.o: \
    test/test.c $(DEPS_35)
	@echo '   [Compile] $(CONFIG)/obj/test.o'
	 -c -o $(CONFIG)/obj/test.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" test/test.c

#
#   goahead-test
#
DEPS_36 += $(CONFIG)/inc/est.h
DEPS_36 += $(CONFIG)/inc/me.h
DEPS_36 += $(CONFIG)/inc/osdep.h
DEPS_36 += $(CONFIG)/obj/estLib.o
ifeq ($(ME_EXT_EST),1)
    DEPS_36 += $(CONFIG)/bin/libest.a
endif
DEPS_36 += $(CONFIG)/inc/bitos.h
DEPS_36 += $(CONFIG)/inc/goahead.h
DEPS_36 += $(CONFIG)/inc/js.h
DEPS_36 += $(CONFIG)/obj/action.o
DEPS_36 += $(CONFIG)/obj/alloc.o
DEPS_36 += $(CONFIG)/obj/auth.o
DEPS_36 += $(CONFIG)/obj/cgi.o
DEPS_36 += $(CONFIG)/obj/crypt.o
DEPS_36 += $(CONFIG)/obj/file.o
DEPS_36 += $(CONFIG)/obj/fs.o
DEPS_36 += $(CONFIG)/obj/http.o
DEPS_36 += $(CONFIG)/obj/js.o
DEPS_36 += $(CONFIG)/obj/jst.o
DEPS_36 += $(CONFIG)/obj/options.o
DEPS_36 += $(CONFIG)/obj/osdep.o
DEPS_36 += $(CONFIG)/obj/rom-documents.o
DEPS_36 += $(CONFIG)/obj/route.o
DEPS_36 += $(CONFIG)/obj/runtime.o
DEPS_36 += $(CONFIG)/obj/socket.o
DEPS_36 += $(CONFIG)/obj/upload.o
DEPS_36 += $(CONFIG)/obj/est.o
DEPS_36 += $(CONFIG)/obj/matrixssl.o
DEPS_36 += $(CONFIG)/obj/nanossl.o
DEPS_36 += $(CONFIG)/obj/openssl.o
DEPS_36 += $(CONFIG)/bin/libgo.a
DEPS_36 += $(CONFIG)/obj/test.o

LIBS_36 += -lgo
ifeq ($(ME_EXT_EST),1)
    LIBS_36 += -lest
endif

$(CONFIG)/bin/goahead-test.out: $(DEPS_36)
	@echo '      [Link] $(CONFIG)/bin/goahead-test.out'
	 -o $(CONFIG)/bin/goahead-test.out $(LDFLAGS) $(LIBPATHS) "$(CONFIG)/obj/test.o" $(LIBPATHS_36) $(LIBS_36) $(LIBS_36) $(LIBS) -Wl,-r 

#
#   gopass.o
#
DEPS_37 += $(CONFIG)/inc/me.h
DEPS_37 += $(CONFIG)/inc/goahead.h
DEPS_37 += $(CONFIG)/inc/osdep.h

$(CONFIG)/obj/gopass.o: \
    src/utils/gopass.c $(DEPS_37)
	@echo '   [Compile] $(CONFIG)/obj/gopass.o'
	 -c -o $(CONFIG)/obj/gopass.o $(CFLAGS) $(DFLAGS) "-I$(CONFIG)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/utils/gopass.c

#
#   gopass
#
DEPS_38 += $(CONFIG)/inc/est.h
DEPS_38 += $(CONFIG)/inc/me.h
DEPS_38 += $(CONFIG)/inc/osdep.h
DEPS_38 += $(CONFIG)/obj/estLib.o
ifeq ($(ME_EXT_EST),1)
    DEPS_38 += $(CONFIG)/bin/libest.a
endif
DEPS_38 += $(CONFIG)/inc/bitos.h
DEPS_38 += $(CONFIG)/inc/goahead.h
DEPS_38 += $(CONFIG)/inc/js.h
DEPS_38 += $(CONFIG)/obj/action.o
DEPS_38 += $(CONFIG)/obj/alloc.o
DEPS_38 += $(CONFIG)/obj/auth.o
DEPS_38 += $(CONFIG)/obj/cgi.o
DEPS_38 += $(CONFIG)/obj/crypt.o
DEPS_38 += $(CONFIG)/obj/file.o
DEPS_38 += $(CONFIG)/obj/fs.o
DEPS_38 += $(CONFIG)/obj/http.o
DEPS_38 += $(CONFIG)/obj/js.o
DEPS_38 += $(CONFIG)/obj/jst.o
DEPS_38 += $(CONFIG)/obj/options.o
DEPS_38 += $(CONFIG)/obj/osdep.o
DEPS_38 += $(CONFIG)/obj/rom-documents.o
DEPS_38 += $(CONFIG)/obj/route.o
DEPS_38 += $(CONFIG)/obj/runtime.o
DEPS_38 += $(CONFIG)/obj/socket.o
DEPS_38 += $(CONFIG)/obj/upload.o
DEPS_38 += $(CONFIG)/obj/est.o
DEPS_38 += $(CONFIG)/obj/matrixssl.o
DEPS_38 += $(CONFIG)/obj/nanossl.o
DEPS_38 += $(CONFIG)/obj/openssl.o
DEPS_38 += $(CONFIG)/bin/libgo.a
DEPS_38 += $(CONFIG)/obj/gopass.o

LIBS_38 += -lgo
ifeq ($(ME_EXT_EST),1)
    LIBS_38 += -lest
endif

$(CONFIG)/bin/gopass.out: $(DEPS_38)
	@echo '      [Link] $(CONFIG)/bin/gopass.out'
	 -o $(CONFIG)/bin/gopass.out $(LDFLAGS) $(LIBPATHS) "$(CONFIG)/obj/gopass.o" $(LIBPATHS_38) $(LIBS_38) $(LIBS_38) $(LIBS) -Wl,-r 

#
#   stop
#
stop: $(DEPS_39)

#
#   installBinary
#
installBinary: $(DEPS_40)

#
#   start
#
start: $(DEPS_41)

#
#   install
#
DEPS_42 += stop
DEPS_42 += installBinary
DEPS_42 += start

install: $(DEPS_42)

#
#   uninstall
#
DEPS_43 += stop

uninstall: $(DEPS_43)

#
#   run
#
run: $(DEPS_44)
	cd src; goahead -v ; cd ..
