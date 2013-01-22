#
#   goahead-macosx-debug.sh -- Build It Shell Script to build Embedthis GoAhead
#

ARCH="x64"
ARCH="`uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/'`"
OS="macosx"
PROFILE="debug"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="/usr/bin/clang"
LD="/usr/bin/ld"
CFLAGS="-w"
DFLAGS="-DBIT_DEBUG"
IFLAGS="-I${CONFIG}/inc"
LDFLAGS="-Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -g"
LIBPATHS="-L${CONFIG}/bin"
LIBS="-lpthread -lm -ldl"

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

${CC} -c -o ${CONFIG}/obj/estLib.o -arch x86_64 ${DFLAGS} -I${CONFIG}/inc src/deps/est/estLib.c

${CC} -dynamiclib -o ${CONFIG}/bin/libest.dylib -arch x86_64 ${LDFLAGS} -compatibility_version 3.1.0 -current_version 3.1.0 ${LIBPATHS} -install_name @rpath/libest.dylib ${CONFIG}/obj/estLib.o ${LIBS}

rm -rf ${CONFIG}/bin/ca.crt
cp -r src/deps/est/ca.crt ${CONFIG}/bin/ca.crt

rm -rf ${CONFIG}/inc/goahead.h
cp -r src/goahead.h ${CONFIG}/inc/goahead.h

rm -rf ${CONFIG}/inc/js.h
cp -r src/js.h ${CONFIG}/inc/js.h

${CC} -c -o ${CONFIG}/obj/action.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/action.c

${CC} -c -o ${CONFIG}/obj/alloc.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/alloc.c

${CC} -c -o ${CONFIG}/obj/auth.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/auth.c

${CC} -c -o ${CONFIG}/obj/cgi.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/cgi.c

${CC} -c -o ${CONFIG}/obj/crypt.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/crypt.c

${CC} -c -o ${CONFIG}/obj/file.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/file.c

${CC} -c -o ${CONFIG}/obj/fs.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/fs.c

${CC} -c -o ${CONFIG}/obj/http.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/http.c

${CC} -c -o ${CONFIG}/obj/js.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/js.c

${CC} -c -o ${CONFIG}/obj/jst.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/jst.c

${CC} -c -o ${CONFIG}/obj/options.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/options.c

${CC} -c -o ${CONFIG}/obj/rom-documents.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/rom-documents.c

${CC} -c -o ${CONFIG}/obj/route.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/route.c

${CC} -c -o ${CONFIG}/obj/runtime.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/runtime.c

${CC} -c -o ${CONFIG}/obj/socket.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/socket.c

${CC} -c -o ${CONFIG}/obj/upload.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/upload.c

${CC} -c -o ${CONFIG}/obj/est.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/ssl/est.c

${CC} -c -o ${CONFIG}/obj/matrixssl.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/ssl/matrixssl.c

${CC} -c -o ${CONFIG}/obj/openssl.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/est src/ssl/openssl.c

${CC} -dynamiclib -o ${CONFIG}/bin/libgo.dylib -arch x86_64 ${LDFLAGS} -compatibility_version 3.1.0 -current_version 3.1.0 ${LIBPATHS} -install_name @rpath/libgo.dylib ${CONFIG}/obj/action.o ${CONFIG}/obj/alloc.o ${CONFIG}/obj/auth.o ${CONFIG}/obj/cgi.o ${CONFIG}/obj/crypt.o ${CONFIG}/obj/file.o ${CONFIG}/obj/fs.o ${CONFIG}/obj/http.o ${CONFIG}/obj/js.o ${CONFIG}/obj/jst.o ${CONFIG}/obj/options.o ${CONFIG}/obj/rom-documents.o ${CONFIG}/obj/route.o ${CONFIG}/obj/runtime.o ${CONFIG}/obj/socket.o ${CONFIG}/obj/upload.o ${CONFIG}/obj/est.o ${CONFIG}/obj/matrixssl.o ${CONFIG}/obj/openssl.o ${LIBS} -lest

${CC} -c -o ${CONFIG}/obj/goahead.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/goahead.c

${CC} -o ${CONFIG}/bin/goahead -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/goahead.o -lgo ${LIBS} -lest

${CC} -c -o ${CONFIG}/obj/test.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/test.c

${CC} -o ${CONFIG}/bin/goahead-test -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/test.o -lgo ${LIBS} -lest

