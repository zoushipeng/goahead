#
#   goahead-solaris-default.sh -- Build It Shell Script to build Embedthis GoAhead
#

PRODUCT="goahead"
VERSION="3.1.0"
BUILD_NUMBER="1"
PROFILE="default"
ARCH="x86"
ARCH="`uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/'`"
OS="solaris"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="/usr/bin/gcc"
LD="/usr/bin/ld"
CFLAGS="-fPIC -O2 -w"
DFLAGS="-D_REENTRANT -DPIC"
IFLAGS="-I${CONFIG}/inc"
LDFLAGS=""
LIBPATHS="-L${CONFIG}/bin"
LIBS="-llxnet -lrt -lsocket -lpthread -lm -ldl"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin

[ ! -f ${CONFIG}/inc/bit.h ] && cp projects/goahead-${OS}-${PROFILE}-bit.h ${CONFIG}/inc/bit.h
[ ! -f ${CONFIG}/inc/bitos.h ] && cp ${SRC}/src/bitos.h ${CONFIG}/inc/bitos.h
if ! diff ${CONFIG}/inc/bit.h projects/goahead-${OS}-${PROFILE}-bit.h >/dev/null ; then
	cp projects/goahead-${OS}-${PROFILE}-bit.h ${CONFIG}/inc/bit.h
fi

rm -rf ${CONFIG}/inc/bitos.h
cp -r src/bitos.h ${CONFIG}/inc/bitos.h

rm -rf ${CONFIG}/inc/est.h
cp -r src/deps/est/est.h ${CONFIG}/inc/est.h

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/estLib.o -fPIC -O2 ${DFLAGS} -I${CONFIG}/inc src/deps/est/estLib.c

${LDFLAGS}${LDFLAGS}${CC} -shared -o ${CONFIG}/bin/libest.so ${LIBPATHS} ${CONFIG}/obj/estLib.o ${LIBS}

rm -rf ${CONFIG}/bin/ca.crt
cp -r src/deps/est/ca.crt ${CONFIG}/bin/ca.crt

rm -rf ${CONFIG}/inc/goahead.h
cp -r src/goahead.h ${CONFIG}/inc/goahead.h

rm -rf ${CONFIG}/inc/js.h
cp -r src/js.h ${CONFIG}/inc/js.h

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/action.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/action.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/alloc.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/alloc.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/auth.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/auth.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/cgi.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/cgi.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/crypt.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/crypt.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/file.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/file.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/fs.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/fs.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/http.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/http.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/js.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/js.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/jst.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/jst.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/options.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/options.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/osdep.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/osdep.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/rom-documents.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/rom-documents.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/route.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/route.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/runtime.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/runtime.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/socket.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/socket.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/upload.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/upload.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/est.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/ssl/est.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/matrixssl.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/ssl/matrixssl.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/openssl.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/ssl/openssl.c

${LDFLAGS}${LDFLAGS}${CC} -shared -o ${CONFIG}/bin/libgo.so ${LIBPATHS} ${CONFIG}/obj/action.o ${CONFIG}/obj/alloc.o ${CONFIG}/obj/auth.o ${CONFIG}/obj/cgi.o ${CONFIG}/obj/crypt.o ${CONFIG}/obj/file.o ${CONFIG}/obj/fs.o ${CONFIG}/obj/http.o ${CONFIG}/obj/js.o ${CONFIG}/obj/jst.o ${CONFIG}/obj/options.o ${CONFIG}/obj/osdep.o ${CONFIG}/obj/rom-documents.o ${CONFIG}/obj/route.o ${CONFIG}/obj/runtime.o ${CONFIG}/obj/socket.o ${CONFIG}/obj/upload.o ${CONFIG}/obj/est.o ${CONFIG}/obj/matrixssl.o ${CONFIG}/obj/openssl.o ${LIBS} -lest

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/goahead.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/goahead.c

${LDFLAGS}${LDFLAGS}${CC} -o ${CONFIG}/bin/goahead ${LIBPATHS} ${CONFIG}/obj/goahead.o -lgo ${LIBS} -lest -lgo -llxnet -lrt -lsocket -lpthread -lm -ldl -lest 

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/test.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/test.c

${LDFLAGS}${LDFLAGS}${CC} -o ${CONFIG}/bin/goahead-test ${LIBPATHS} ${CONFIG}/obj/test.o -lgo ${LIBS} -lest -lgo -llxnet -lrt -lsocket -lpthread -lm -ldl -lest 

#  Omit build script undefined
