#
#   goahead-linux-static.sh -- Build It Shell Script to build Embedthis GoAhead
#

PRODUCT="goahead"
VERSION="3.1.0"
BUILD_NUMBER="1"
PROFILE="static"
ARCH="x86"
ARCH="`uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/'`"
OS="linux"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="/usr/bin/gcc"
LD="/usr/bin/ld"
CFLAGS="-fPIC -O2  -w"
DFLAGS="-D_REENTRANT -DPIC"
IFLAGS="-I${CONFIG}/inc"
LDFLAGS="-Wl,--enable-new-dtags -Wl,-rpath,\$ORIGIN/ -Wl,-rpath,\$ORIGIN/../bin -rdynamic"
LIBPATHS="-L${CONFIG}/bin"
LIBS="-lpthread -lm -lrt -ldl"

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

${CC} -c -o ${CONFIG}/obj/estLib.o -fPIC -O2 ${DFLAGS} -I${CONFIG}/inc src/deps/est/estLib.c

/usr/bin/ar -cr ${CONFIG}/bin/libest.a ${CONFIG}/obj/estLib.o

rm -rf ${CONFIG}/bin/ca.crt
cp -r src/deps/est/ca.crt ${CONFIG}/bin/ca.crt

rm -rf ${CONFIG}/inc/goahead.h
cp -r src/goahead.h ${CONFIG}/inc/goahead.h

rm -rf ${CONFIG}/inc/js.h
cp -r src/js.h ${CONFIG}/inc/js.h

${CC} -c -o ${CONFIG}/obj/action.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/action.c

${CC} -c -o ${CONFIG}/obj/alloc.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/alloc.c

${CC} -c -o ${CONFIG}/obj/auth.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/auth.c

${CC} -c -o ${CONFIG}/obj/cgi.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/cgi.c

${CC} -c -o ${CONFIG}/obj/crypt.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/crypt.c

${CC} -c -o ${CONFIG}/obj/file.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/file.c

${CC} -c -o ${CONFIG}/obj/fs.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/fs.c

${CC} -c -o ${CONFIG}/obj/http.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/http.c

${CC} -c -o ${CONFIG}/obj/js.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/js.c

${CC} -c -o ${CONFIG}/obj/jst.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/jst.c

${CC} -c -o ${CONFIG}/obj/options.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/options.c

${CC} -c -o ${CONFIG}/obj/osdep.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/osdep.c

${CC} -c -o ${CONFIG}/obj/rom-documents.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/rom-documents.c

${CC} -c -o ${CONFIG}/obj/route.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/route.c

${CC} -c -o ${CONFIG}/obj/runtime.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/runtime.c

${CC} -c -o ${CONFIG}/obj/socket.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/socket.c

${CC} -c -o ${CONFIG}/obj/upload.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/upload.c

${CC} -c -o ${CONFIG}/obj/est.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/ssl/est.c

${CC} -c -o ${CONFIG}/obj/matrixssl.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/ssl/matrixssl.c

${CC} -c -o ${CONFIG}/obj/openssl.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/ssl/openssl.c

/usr/bin/ar -cr ${CONFIG}/bin/libgo.a ${CONFIG}/obj/action.o ${CONFIG}/obj/alloc.o ${CONFIG}/obj/auth.o ${CONFIG}/obj/cgi.o ${CONFIG}/obj/crypt.o ${CONFIG}/obj/file.o ${CONFIG}/obj/fs.o ${CONFIG}/obj/http.o ${CONFIG}/obj/js.o ${CONFIG}/obj/jst.o ${CONFIG}/obj/options.o ${CONFIG}/obj/osdep.o ${CONFIG}/obj/rom-documents.o ${CONFIG}/obj/route.o ${CONFIG}/obj/runtime.o ${CONFIG}/obj/socket.o ${CONFIG}/obj/upload.o ${CONFIG}/obj/est.o ${CONFIG}/obj/matrixssl.o ${CONFIG}/obj/openssl.o

${CC} -c -o ${CONFIG}/obj/goahead.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/goahead.c

${CC} -o ${CONFIG}/bin/goahead ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/goahead.o -lgo ${LIBS} -lest -lgo -lpthread -lm -lrt -ldl -lest ${LDFLAGS}

${CC} -c -o ${CONFIG}/obj/test.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/test.c

${CC} -o ${CONFIG}/bin/goahead-test ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/test.o -lgo ${LIBS} -lest -lgo -lpthread -lm -lrt -ldl -lest ${LDFLAGS}

#  Omit build script undefined
