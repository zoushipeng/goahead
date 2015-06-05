#
#   goahead-freebsd-static.mk -- Makefile to build Embedthis GoAhead for freebsd
#

NAME                  := goahead
VERSION               := 3.4.5
PROFILE               ?= static
ARCH                  ?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/')
CC_ARCH               ?= $(shell echo $(ARCH) | sed 's/x86/i686/;s/x64/x86_64/')
OS                    ?= freebsd
CC                    ?= gcc
CONFIG                ?= $(OS)-$(ARCH)-$(PROFILE)
BUILD                 ?= build/$(CONFIG)
LBIN                  ?= $(BUILD)/bin
PATH                  := $(LBIN):$(PATH)

ME_COM_COMPILER       ?= 1
ME_COM_EST            ?= 0
ME_COM_LIB            ?= 1
ME_COM_MATRIXSSL      ?= 0
ME_COM_MBEDTLS        ?= 0
ME_COM_NANOSSL        ?= 0
ME_COM_OPENSSL        ?= 1
ME_COM_OSDEP          ?= 1
ME_COM_SSL            ?= 1
ME_COM_VXWORKS        ?= 0
ME_COM_WINSDK         ?= 1

ME_COM_OPENSSL_PATH   ?= "/usr/lib"

ifeq ($(ME_COM_EST),1)
    ME_COM_SSL := 1
endif
ifeq ($(ME_COM_LIB),1)
    ME_COM_COMPILER := 1
endif
ifeq ($(ME_COM_OPENSSL),1)
    ME_COM_SSL := 1
endif

CFLAGS                += -g -w
DFLAGS                +=  $(patsubst %,-D%,$(filter ME_%,$(MAKEFLAGS))) -DME_COM_COMPILER=$(ME_COM_COMPILER) -DME_COM_EST=$(ME_COM_EST) -DME_COM_LIB=$(ME_COM_LIB) -DME_COM_MATRIXSSL=$(ME_COM_MATRIXSSL) -DME_COM_MBEDTLS=$(ME_COM_MBEDTLS) -DME_COM_NANOSSL=$(ME_COM_NANOSSL) -DME_COM_OPENSSL=$(ME_COM_OPENSSL) -DME_COM_OSDEP=$(ME_COM_OSDEP) -DME_COM_SSL=$(ME_COM_SSL) -DME_COM_VXWORKS=$(ME_COM_VXWORKS) -DME_COM_WINSDK=$(ME_COM_WINSDK) 
IFLAGS                += "-I$(BUILD)/inc"
LDFLAGS               += 
LIBPATHS              += -L$(BUILD)/bin
LIBS                  += -ldl -lpthread -lm

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

ME_ROOT_PREFIX        ?= 
ME_BASE_PREFIX        ?= $(ME_ROOT_PREFIX)/usr/local
ME_DATA_PREFIX        ?= $(ME_ROOT_PREFIX)/
ME_STATE_PREFIX       ?= $(ME_ROOT_PREFIX)/var
ME_APP_PREFIX         ?= $(ME_BASE_PREFIX)/lib/$(NAME)
ME_VAPP_PREFIX        ?= $(ME_APP_PREFIX)/$(VERSION)
ME_BIN_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/bin
ME_INC_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/include
ME_LIB_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/lib
ME_MAN_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/share/man
ME_SBIN_PREFIX        ?= $(ME_ROOT_PREFIX)/usr/local/sbin
ME_ETC_PREFIX         ?= $(ME_ROOT_PREFIX)/etc/$(NAME)
ME_WEB_PREFIX         ?= $(ME_ROOT_PREFIX)/var/www/$(NAME)
ME_LOG_PREFIX         ?= $(ME_ROOT_PREFIX)/var/log/$(NAME)
ME_SPOOL_PREFIX       ?= $(ME_ROOT_PREFIX)/var/spool/$(NAME)
ME_CACHE_PREFIX       ?= $(ME_ROOT_PREFIX)/var/spool/$(NAME)/cache
ME_SRC_PREFIX         ?= $(ME_ROOT_PREFIX)$(NAME)-$(VERSION)


TARGETS               += $(BUILD)/bin/goahead
TARGETS               += $(BUILD)/bin/goahead-test
TARGETS               += $(BUILD)/bin/gopass
ifeq ($(ME_COM_SSL),1)
    TARGETS           += $(BUILD)/.install-certs-modified
endif

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
	@[ ! -x $(BUILD)/bin ] && mkdir -p $(BUILD)/bin; true
	@[ ! -x $(BUILD)/inc ] && mkdir -p $(BUILD)/inc; true
	@[ ! -x $(BUILD)/obj ] && mkdir -p $(BUILD)/obj; true
	@[ ! -f $(BUILD)/inc/me.h ] && cp projects/goahead-freebsd-static-me.h $(BUILD)/inc/me.h ; true
	@if ! diff $(BUILD)/inc/me.h projects/goahead-freebsd-static-me.h >/dev/null ; then\
		cp projects/goahead-freebsd-static-me.h $(BUILD)/inc/me.h  ; \
	fi; true
	@if [ -f "$(BUILD)/.makeflags" ] ; then \
		if [ "$(MAKEFLAGS)" != "`cat $(BUILD)/.makeflags`" ] ; then \
			echo "   [Warning] Make flags have changed since the last build" ; \
			echo "   [Warning] Previous build command: "`cat $(BUILD)/.makeflags`"" ; \
		fi ; \
	fi
	@echo "$(MAKEFLAGS)" >$(BUILD)/.makeflags

clean:
	rm -f "$(BUILD)/obj/action.o"
	rm -f "$(BUILD)/obj/alloc.o"
	rm -f "$(BUILD)/obj/auth.o"
	rm -f "$(BUILD)/obj/cgi.o"
	rm -f "$(BUILD)/obj/cgitest.o"
	rm -f "$(BUILD)/obj/crypt.o"
	rm -f "$(BUILD)/obj/est.o"
	rm -f "$(BUILD)/obj/estLib.o"
	rm -f "$(BUILD)/obj/file.o"
	rm -f "$(BUILD)/obj/fs.o"
	rm -f "$(BUILD)/obj/goahead.o"
	rm -f "$(BUILD)/obj/gopass.o"
	rm -f "$(BUILD)/obj/http.o"
	rm -f "$(BUILD)/obj/js.o"
	rm -f "$(BUILD)/obj/jst.o"
	rm -f "$(BUILD)/obj/openssl.o"
	rm -f "$(BUILD)/obj/options.o"
	rm -f "$(BUILD)/obj/osdep.o"
	rm -f "$(BUILD)/obj/rom-documents.o"
	rm -f "$(BUILD)/obj/route.o"
	rm -f "$(BUILD)/obj/runtime.o"
	rm -f "$(BUILD)/obj/socket.o"
	rm -f "$(BUILD)/obj/test.o"
	rm -f "$(BUILD)/obj/upload.o"
	rm -f "$(BUILD)/bin/goahead"
	rm -f "$(BUILD)/bin/goahead-test"
	rm -f "$(BUILD)/bin/gopass"
	rm -f "$(BUILD)/.install-certs-modified"
	rm -f "$(BUILD)/bin/libgo.a"
	rm -f "$(BUILD)/bin/libgoahead-openssl.a"

clobber: clean
	rm -fr ./$(BUILD)

#
#   est.h
#
DEPS_1 += src/est/est.h

$(BUILD)/inc/est.h: $(DEPS_1)
	@echo '      [Copy] $(BUILD)/inc/est.h'
	mkdir -p "$(BUILD)/inc"
	cp src/est/est.h $(BUILD)/inc/est.h

#
#   me.h
#

$(BUILD)/inc/me.h: $(DEPS_2)

#
#   osdep.h
#
DEPS_3 += src/osdep/osdep.h
DEPS_3 += $(BUILD)/inc/me.h

$(BUILD)/inc/osdep.h: $(DEPS_3)
	@echo '      [Copy] $(BUILD)/inc/osdep.h'
	mkdir -p "$(BUILD)/inc"
	cp src/osdep/osdep.h $(BUILD)/inc/osdep.h

#
#   goahead.h
#
DEPS_4 += src/goahead.h
DEPS_4 += $(BUILD)/inc/me.h
DEPS_4 += $(BUILD)/inc/osdep.h

$(BUILD)/inc/goahead.h: $(DEPS_4)
	@echo '      [Copy] $(BUILD)/inc/goahead.h'
	mkdir -p "$(BUILD)/inc"
	cp src/goahead.h $(BUILD)/inc/goahead.h

#
#   js.h
#
DEPS_5 += src/js.h
DEPS_5 += $(BUILD)/inc/goahead.h

$(BUILD)/inc/js.h: $(DEPS_5)
	@echo '      [Copy] $(BUILD)/inc/js.h'
	mkdir -p "$(BUILD)/inc"
	cp src/js.h $(BUILD)/inc/js.h

#
#   action.o
#
DEPS_6 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/action.o: \
    src/action.c $(DEPS_6)
	@echo '   [Compile] $(BUILD)/obj/action.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/action.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/action.c

#
#   alloc.o
#
DEPS_7 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/alloc.o: \
    src/alloc.c $(DEPS_7)
	@echo '   [Compile] $(BUILD)/obj/alloc.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/alloc.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/alloc.c

#
#   auth.o
#
DEPS_8 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/auth.o: \
    src/auth.c $(DEPS_8)
	@echo '   [Compile] $(BUILD)/obj/auth.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/auth.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/auth.c

#
#   cgi.o
#
DEPS_9 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/cgi.o: \
    src/cgi.c $(DEPS_9)
	@echo '   [Compile] $(BUILD)/obj/cgi.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/cgi.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/cgi.c

#
#   cgitest.o
#

$(BUILD)/obj/cgitest.o: \
    test/cgitest.c $(DEPS_10)
	@echo '   [Compile] $(BUILD)/obj/cgitest.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/cgitest.o $(LDFLAGS) $(IFLAGS) test/cgitest.c

#
#   crypt.o
#
DEPS_11 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/crypt.o: \
    src/crypt.c $(DEPS_11)
	@echo '   [Compile] $(BUILD)/obj/crypt.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/crypt.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/crypt.c

#
#   est.o
#
DEPS_12 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/est.o: \
    src/goahead-est/est.c $(DEPS_12)
	@echo '   [Compile] $(BUILD)/obj/est.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/est.o $(LDFLAGS) $(IFLAGS) src/goahead-est/est.c

#
#   est.h
#

src/est/est.h: $(DEPS_13)

#
#   estLib.o
#
DEPS_14 += src/est/est.h

$(BUILD)/obj/estLib.o: \
    src/est/estLib.c $(DEPS_14)
	@echo '   [Compile] $(BUILD)/obj/estLib.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/estLib.o $(LDFLAGS) $(IFLAGS) src/est/estLib.c

#
#   file.o
#
DEPS_15 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/file.o: \
    src/file.c $(DEPS_15)
	@echo '   [Compile] $(BUILD)/obj/file.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/file.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/file.c

#
#   fs.o
#
DEPS_16 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/fs.o: \
    src/fs.c $(DEPS_16)
	@echo '   [Compile] $(BUILD)/obj/fs.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/fs.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/fs.c

#
#   goahead.o
#
DEPS_17 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/goahead.o: \
    src/goahead.c $(DEPS_17)
	@echo '   [Compile] $(BUILD)/obj/goahead.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/goahead.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/goahead.c

#
#   gopass.o
#
DEPS_18 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/gopass.o: \
    src/utils/gopass.c $(DEPS_18)
	@echo '   [Compile] $(BUILD)/obj/gopass.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/gopass.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/utils/gopass.c

#
#   http.o
#
DEPS_19 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/http.o: \
    src/http.c $(DEPS_19)
	@echo '   [Compile] $(BUILD)/obj/http.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/http.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/http.c

#
#   js.o
#
DEPS_20 += $(BUILD)/inc/js.h

$(BUILD)/obj/js.o: \
    src/js.c $(DEPS_20)
	@echo '   [Compile] $(BUILD)/obj/js.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/js.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/js.c

#
#   jst.o
#
DEPS_21 += $(BUILD)/inc/goahead.h
DEPS_21 += $(BUILD)/inc/js.h

$(BUILD)/obj/jst.o: \
    src/jst.c $(DEPS_21)
	@echo '   [Compile] $(BUILD)/obj/jst.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/jst.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/jst.c

#
#   openssl.o
#
DEPS_22 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/openssl.o: \
    src/goahead-openssl/openssl.c $(DEPS_22)
	@echo '   [Compile] $(BUILD)/obj/openssl.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/openssl.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/goahead-openssl/openssl.c

#
#   options.o
#
DEPS_23 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/options.o: \
    src/options.c $(DEPS_23)
	@echo '   [Compile] $(BUILD)/obj/options.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/options.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/options.c

#
#   osdep.o
#
DEPS_24 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/osdep.o: \
    src/osdep.c $(DEPS_24)
	@echo '   [Compile] $(BUILD)/obj/osdep.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/osdep.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/osdep.c

#
#   rom-documents.o
#
DEPS_25 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/rom-documents.o: \
    src/rom-documents.c $(DEPS_25)
	@echo '   [Compile] $(BUILD)/obj/rom-documents.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/rom-documents.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/rom-documents.c

#
#   route.o
#
DEPS_26 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/route.o: \
    src/route.c $(DEPS_26)
	@echo '   [Compile] $(BUILD)/obj/route.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/route.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/route.c

#
#   runtime.o
#
DEPS_27 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/runtime.o: \
    src/runtime.c $(DEPS_27)
	@echo '   [Compile] $(BUILD)/obj/runtime.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/runtime.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/runtime.c

#
#   socket.o
#
DEPS_28 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/socket.o: \
    src/socket.c $(DEPS_28)
	@echo '   [Compile] $(BUILD)/obj/socket.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/socket.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/socket.c

#
#   test.o
#
DEPS_29 += $(BUILD)/inc/goahead.h
DEPS_29 += $(BUILD)/inc/js.h

$(BUILD)/obj/test.o: \
    test/test.c $(DEPS_29)
	@echo '   [Compile] $(BUILD)/obj/test.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/test.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" test/test.c

#
#   upload.o
#
DEPS_30 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/upload.o: \
    src/upload.c $(DEPS_30)
	@echo '   [Compile] $(BUILD)/obj/upload.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/upload.o $(LDFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/upload.c

ifeq ($(ME_COM_EST),1)
#
#   libest
#
DEPS_31 += $(BUILD)/inc/osdep.h
DEPS_31 += $(BUILD)/inc/est.h
DEPS_31 += $(BUILD)/obj/estLib.o

$(BUILD)/bin/libest.a: $(DEPS_31)
	@echo '      [Link] $(BUILD)/bin/libest.a'
	ar -cr $(BUILD)/bin/libest.a "$(BUILD)/obj/estLib.o"
endif

ifeq ($(ME_COM_SSL),1)
ifeq ($(ME_COM_EST),1)
#
#   est
#
ifeq ($(ME_COM_EST),1)
    DEPS_32 += $(BUILD)/bin/libest.a
endif
DEPS_32 += $(BUILD)/obj/est.o

$(BUILD)/bin/libgoahead-estssl.a: $(DEPS_32)
	@echo '      [Link] $(BUILD)/bin/libgoahead-estssl.a'
	ar -cr $(BUILD)/bin/libgoahead-estssl.a "$(BUILD)/obj/est.o"
endif
endif

ifeq ($(ME_COM_SSL),1)
ifeq ($(ME_COM_OPENSSL),1)
#
#   openssl
#
DEPS_33 += $(BUILD)/obj/openssl.o

$(BUILD)/bin/libgoahead-openssl.a: $(DEPS_33)
	@echo '      [Link] $(BUILD)/bin/libgoahead-openssl.a'
	ar -cr $(BUILD)/bin/libgoahead-openssl.a "$(BUILD)/obj/openssl.o"
endif
endif

#
#   libgo
#
DEPS_34 += $(BUILD)/inc/osdep.h
ifeq ($(ME_COM_SSL),1)
ifeq ($(ME_COM_OPENSSL),1)
    DEPS_34 += $(BUILD)/bin/libgoahead-openssl.a
endif
endif
ifeq ($(ME_COM_SSL),1)
ifeq ($(ME_COM_EST),1)
    DEPS_34 += $(BUILD)/bin/libgoahead-estssl.a
endif
endif
DEPS_34 += $(BUILD)/inc/goahead.h
DEPS_34 += $(BUILD)/inc/js.h
DEPS_34 += $(BUILD)/obj/action.o
DEPS_34 += $(BUILD)/obj/alloc.o
DEPS_34 += $(BUILD)/obj/auth.o
DEPS_34 += $(BUILD)/obj/cgi.o
DEPS_34 += $(BUILD)/obj/crypt.o
DEPS_34 += $(BUILD)/obj/file.o
DEPS_34 += $(BUILD)/obj/fs.o
DEPS_34 += $(BUILD)/obj/http.o
DEPS_34 += $(BUILD)/obj/js.o
DEPS_34 += $(BUILD)/obj/jst.o
DEPS_34 += $(BUILD)/obj/options.o
DEPS_34 += $(BUILD)/obj/osdep.o
DEPS_34 += $(BUILD)/obj/rom-documents.o
DEPS_34 += $(BUILD)/obj/route.o
DEPS_34 += $(BUILD)/obj/runtime.o
DEPS_34 += $(BUILD)/obj/socket.o
DEPS_34 += $(BUILD)/obj/upload.o

$(BUILD)/bin/libgo.a: $(DEPS_34)
	@echo '      [Link] $(BUILD)/bin/libgo.a'
	ar -cr $(BUILD)/bin/libgo.a "$(BUILD)/obj/action.o" "$(BUILD)/obj/alloc.o" "$(BUILD)/obj/auth.o" "$(BUILD)/obj/cgi.o" "$(BUILD)/obj/crypt.o" "$(BUILD)/obj/file.o" "$(BUILD)/obj/fs.o" "$(BUILD)/obj/http.o" "$(BUILD)/obj/js.o" "$(BUILD)/obj/jst.o" "$(BUILD)/obj/options.o" "$(BUILD)/obj/osdep.o" "$(BUILD)/obj/rom-documents.o" "$(BUILD)/obj/route.o" "$(BUILD)/obj/runtime.o" "$(BUILD)/obj/socket.o" "$(BUILD)/obj/upload.o"

#
#   goahead
#
DEPS_35 += $(BUILD)/bin/libgo.a
DEPS_35 += $(BUILD)/inc/goahead.h
DEPS_35 += $(BUILD)/inc/js.h
DEPS_35 += $(BUILD)/obj/goahead.o

ifeq ($(ME_COM_OPENSSL),1)
    LIBS_35 += -lgoahead-openssl
    LIBPATHS_35 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_35 += -lssl
    LIBPATHS_35 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_35 += -lcrypto
    LIBPATHS_35 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_EST),1)
    LIBS_35 += -lest
endif
ifeq ($(ME_COM_EST),1)
    LIBS_35 += -lgoahead-estssl
endif
LIBS_35 += -lgo
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_35 += -lgoahead-openssl
    LIBPATHS_35 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/goahead: $(DEPS_35)
	@echo '      [Link] $(BUILD)/bin/goahead'
	$(CC) -o $(BUILD)/bin/goahead $(LDFLAGS) $(LIBPATHS)  "$(BUILD)/obj/goahead.o" $(LIBPATHS_35) $(LIBS_35) $(LIBS_35) $(LIBS) $(LIBS) 

#
#   goahead-test
#
DEPS_36 += $(BUILD)/bin/libgo.a
DEPS_36 += $(BUILD)/inc/goahead.h
DEPS_36 += $(BUILD)/inc/js.h
DEPS_36 += $(BUILD)/obj/test.o

ifeq ($(ME_COM_OPENSSL),1)
    LIBS_36 += -lgoahead-openssl
    LIBPATHS_36 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_36 += -lssl
    LIBPATHS_36 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_36 += -lcrypto
    LIBPATHS_36 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_EST),1)
    LIBS_36 += -lest
endif
ifeq ($(ME_COM_EST),1)
    LIBS_36 += -lgoahead-estssl
endif
LIBS_36 += -lgo
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_36 += -lgoahead-openssl
    LIBPATHS_36 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/goahead-test: $(DEPS_36)
	@echo '      [Link] $(BUILD)/bin/goahead-test'
	$(CC) -o $(BUILD)/bin/goahead-test $(LDFLAGS) $(LIBPATHS)  "$(BUILD)/obj/test.o" $(LIBPATHS_36) $(LIBS_36) $(LIBS_36) $(LIBS) $(LIBS) 

#
#   gopass
#
DEPS_37 += $(BUILD)/bin/libgo.a
DEPS_37 += $(BUILD)/inc/goahead.h
DEPS_37 += $(BUILD)/inc/js.h
DEPS_37 += $(BUILD)/obj/gopass.o

ifeq ($(ME_COM_OPENSSL),1)
    LIBS_37 += -lgoahead-openssl
    LIBPATHS_37 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_37 += -lssl
    LIBPATHS_37 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_37 += -lcrypto
    LIBPATHS_37 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_EST),1)
    LIBS_37 += -lest
endif
ifeq ($(ME_COM_EST),1)
    LIBS_37 += -lgoahead-estssl
endif
LIBS_37 += -lgo
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_37 += -lgoahead-openssl
    LIBPATHS_37 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/gopass: $(DEPS_37)
	@echo '      [Link] $(BUILD)/bin/gopass'
	$(CC) -o $(BUILD)/bin/gopass $(LDFLAGS) $(LIBPATHS)  "$(BUILD)/obj/gopass.o" $(LIBPATHS_37) $(LIBS_37) $(LIBS_37) $(LIBS) $(LIBS) 

ifeq ($(ME_COM_SSL),1)
#
#   install-certs
#
DEPS_38 += src/certs/samples/ca.crt
DEPS_38 += src/certs/samples/ca.key
DEPS_38 += src/certs/samples/ec.crt
DEPS_38 += src/certs/samples/ec.key
DEPS_38 += src/certs/samples/roots.crt
DEPS_38 += src/certs/samples/self.crt
DEPS_38 += src/certs/samples/self.key
DEPS_38 += src/certs/samples/test.crt
DEPS_38 += src/certs/samples/test.key

$(BUILD)/.install-certs-modified: $(DEPS_38)
	@echo '      [Copy] $(BUILD)/bin'
	mkdir -p "$(BUILD)/bin"
	cp src/certs/samples/ca.crt $(BUILD)/bin/ca.crt
	cp src/certs/samples/ca.key $(BUILD)/bin/ca.key
	cp src/certs/samples/ec.crt $(BUILD)/bin/ec.crt
	cp src/certs/samples/ec.key $(BUILD)/bin/ec.key
	cp src/certs/samples/roots.crt $(BUILD)/bin/roots.crt
	cp src/certs/samples/self.crt $(BUILD)/bin/self.crt
	cp src/certs/samples/self.key $(BUILD)/bin/self.key
	cp src/certs/samples/test.crt $(BUILD)/bin/test.crt
	cp src/certs/samples/test.key $(BUILD)/bin/test.key
	touch "$(BUILD)/.install-certs-modified"
endif

#
#   stop
#

stop: $(DEPS_39)

#
#   installBinary
#

installBinary: $(DEPS_40)
	mkdir -p "$(ME_APP_PREFIX)" ; \
	rm -f "$(ME_APP_PREFIX)/latest" ; \
	ln -s "$(VERSION)" "$(ME_APP_PREFIX)/latest" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/goahead $(ME_VAPP_PREFIX)/bin/goahead ; \
	mkdir -p "$(ME_BIN_PREFIX)" ; \
	rm -f "$(ME_BIN_PREFIX)/goahead" ; \
	ln -s "$(ME_VAPP_PREFIX)/bin/goahead" "$(ME_BIN_PREFIX)/goahead" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/roots.crt $(ME_VAPP_PREFIX)/bin/roots.crt ; \
	mkdir -p "$(ME_WEB_PREFIX)" ; \
	cp src/web/index.html $(ME_WEB_PREFIX)/index.html ; \
	cp src/web/favicon.ico $(ME_WEB_PREFIX)/favicon.ico ; \
	mkdir -p "$(ME_ETC_PREFIX)" ; \
	cp src/auth.txt $(ME_ETC_PREFIX)/auth.txt ; \
	cp src/route.txt $(ME_ETC_PREFIX)/route.txt ; \
	mkdir -p "$(ME_VAPP_PREFIX)/doc/man/man1" ; \
	cp doc/dist/man/goahead.1 $(ME_VAPP_PREFIX)/doc/man/man1/goahead.1 ; \
	mkdir -p "$(ME_MAN_PREFIX)/man1" ; \
	rm -f "$(ME_MAN_PREFIX)/man1/goahead.1" ; \
	ln -s "$(ME_VAPP_PREFIX)/doc/man/man1/goahead.1" "$(ME_MAN_PREFIX)/man1/goahead.1" ; \
	cp doc/dist/man/gopass.1 $(ME_VAPP_PREFIX)/doc/man/man1/gopass.1 ; \
	mkdir -p "$(ME_MAN_PREFIX)/man1" ; \
	rm -f "$(ME_MAN_PREFIX)/man1/gopass.1" ; \
	ln -s "$(ME_VAPP_PREFIX)/doc/man/man1/gopass.1" "$(ME_MAN_PREFIX)/man1/gopass.1" ; \
	cp doc/dist/man/webcomp.1 $(ME_VAPP_PREFIX)/doc/man/man1/webcomp.1 ; \
	mkdir -p "$(ME_MAN_PREFIX)/man1" ; \
	rm -f "$(ME_MAN_PREFIX)/man1/webcomp.1" ; \
	ln -s "$(ME_VAPP_PREFIX)/doc/man/man1/webcomp.1" "$(ME_MAN_PREFIX)/man1/webcomp.1"

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
#   installPrep
#

installPrep: $(DEPS_43)
	if [ "`id -u`" != 0 ] ; \
	then echo "Must run as root. Rerun with "sudo"" ; \
	exit 255 ; \
	fi

#
#   uninstall
#
DEPS_44 += stop

uninstall: $(DEPS_44)
	rm -fr "$(ME_WEB_PREFIX)" ; \
	rm -fr "$(ME_VAPP_PREFIX)" ; \
	rmdir -p "$(ME_ETC_PREFIX)" 2>/dev/null ; true ; \
	rmdir -p "$(ME_WEB_PREFIX)" 2>/dev/null ; true ; \
	rm -f "$(ME_APP_PREFIX)/latest" ; \
	rmdir -p "$(ME_APP_PREFIX)" 2>/dev/null ; true

#
#   version
#

version: $(DEPS_45)
	echo $(VERSION)

