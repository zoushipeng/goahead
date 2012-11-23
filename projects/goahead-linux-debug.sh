#
#   goahead-linux-debug.sh -- Build It Shell Script to build Embedthis GoAhead
#

ARCH="x86"
ARCH="`uname -m | sed 's/i.86/x86/;s/x86_64/x64/'`"
OS="linux"
PROFILE="debug"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="gcc"
LD="/usr/bin/ld"
CFLAGS="-Wall -fPIC -g -Wno-unused-result"
DFLAGS="-D_REENTRANT -DPIC -DBIT_DEBUG"
IFLAGS="-I${CONFIG}/inc"
LDFLAGS="-Wl,--enable-new-dtags -Wl,-rpath,\$ORIGIN/ -Wl,-rpath,\$ORIGIN/../bin -rdynamic -g"
LIBPATHS="-L${CONFIG}/bin"
LIBS="-lpthread -lm -lrt -ldl"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin

[ ! -f ${CONFIG}/inc/bit.h ] && cp projects/goahead-${OS}-${PROFILE}-bit.h ${CONFIG}/inc/bit.h
if ! diff ${CONFIG}/inc/bit.h projects/goahead-${OS}-${PROFILE}-bit.h >/dev/null ; then
	cp projects/goahead-${OS}-${PROFILE}-bit.h ${CONFIG}/inc/bit.h
fi

rm -rf ${CONFIG}/inc/goahead.h
cp -r goahead.h ${CONFIG}/inc/goahead.h

rm -rf ${CONFIG}/inc/js.h
cp -r js.h ${CONFIG}/inc/js.h

${CC} -c -o ${CONFIG}/obj/action.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc action.c

${CC} -c -o ${CONFIG}/obj/alloc.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc alloc.c

${CC} -c -o ${CONFIG}/obj/auth.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc auth.c

${CC} -c -o ${CONFIG}/obj/cgi.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc cgi.c

${CC} -c -o ${CONFIG}/obj/crypt.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc crypt.c

${CC} -c -o ${CONFIG}/obj/file.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc file.c

${CC} -c -o ${CONFIG}/obj/http.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc http.c

${CC} -c -o ${CONFIG}/obj/js.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc js.c

${CC} -c -o ${CONFIG}/obj/jst.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc jst.c

${CC} -c -o ${CONFIG}/obj/matrixssl.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc matrixssl.c

${CC} -c -o ${CONFIG}/obj/openssl.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc openssl.c

${CC} -c -o ${CONFIG}/obj/options.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc options.c

${CC} -c -o ${CONFIG}/obj/rom-documents.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc rom-documents.c

${CC} -c -o ${CONFIG}/obj/rom.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc rom.c

${CC} -c -o ${CONFIG}/obj/route.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc route.c

${CC} -c -o ${CONFIG}/obj/runtime.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc runtime.c

${CC} -c -o ${CONFIG}/obj/socket.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc socket.c

${CC} -c -o ${CONFIG}/obj/upload.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc upload.c

${CC} -shared -o ${CONFIG}/bin/libgo.so ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/action.o ${CONFIG}/obj/alloc.o ${CONFIG}/obj/auth.o ${CONFIG}/obj/cgi.o ${CONFIG}/obj/crypt.o ${CONFIG}/obj/file.o ${CONFIG}/obj/http.o ${CONFIG}/obj/js.o ${CONFIG}/obj/jst.o ${CONFIG}/obj/matrixssl.o ${CONFIG}/obj/openssl.o ${CONFIG}/obj/options.o ${CONFIG}/obj/rom-documents.o ${CONFIG}/obj/rom.o ${CONFIG}/obj/route.o ${CONFIG}/obj/runtime.o ${CONFIG}/obj/socket.o ${CONFIG}/obj/upload.o ${LIBS}

${CC} -c -o ${CONFIG}/obj/goahead.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc goahead.c

${CC} -o ${CONFIG}/bin/goahead ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/goahead.o -lgo ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}/obj/test.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/test.c

${CC} -o ${CONFIG}/bin/goahead-test ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/test.o -lgo ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}/obj/gopass.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc utils/gopass.c

${CC} -o ${CONFIG}/bin/gopass ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/gopass.o -lgo ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}/obj/webcomp.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc utils/webcomp.c

${CC} -o ${CONFIG}/bin/webcomp ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/webcomp.o ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}/obj/cgitest.o -mtune=generic ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/cgitest.c

${CC} -o test/cgi-bin/cgitest ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/cgitest.o ${LIBS} ${LDFLAGS}

