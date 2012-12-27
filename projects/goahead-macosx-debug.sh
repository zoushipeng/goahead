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
[ ! -f ${CONFIG}/inc/bitos.h ] && cp src/bitos.h ${CONFIG}/inc/bitos.h
if ! diff ${CONFIG}/inc/bit.h projects/goahead-${OS}-${PROFILE}-bit.h >/dev/null ; then
	cp projects/goahead-${OS}-${PROFILE}-bit.h ${CONFIG}/inc/bit.h
fi

rm -rf ${CONFIG}/inc/bitos.h
cp -r src/bitos.h ${CONFIG}/inc/bitos.h

rm -rf ${CONFIG}/inc/goahead.h
cp -r src/goahead.h ${CONFIG}/inc/goahead.h

rm -rf ${CONFIG}/inc/js.h
cp -r src/js.h ${CONFIG}/inc/js.h

${CC} -c -o ${CONFIG}/obj/action.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/action.c

${CC} -c -o ${CONFIG}/obj/alloc.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/alloc.c

${CC} -c -o ${CONFIG}/obj/auth.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/auth.c

${CC} -c -o ${CONFIG}/obj/cgi.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/cgi.c

${CC} -c -o ${CONFIG}/obj/crypt.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/crypt.c

${CC} -c -o ${CONFIG}/obj/file.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/file.c

${CC} -c -o ${CONFIG}/obj/http.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/http.c

${CC} -c -o ${CONFIG}/obj/js.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/js.c

${CC} -c -o ${CONFIG}/obj/jst.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/jst.c

${CC} -c -o ${CONFIG}/obj/options.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/options.c

${CC} -c -o ${CONFIG}/obj/rom-documents.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/rom-documents.c

${CC} -c -o ${CONFIG}/obj/rom.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/rom.c

${CC} -c -o ${CONFIG}/obj/route.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/route.c

${CC} -c -o ${CONFIG}/obj/runtime.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/runtime.c

${CC} -c -o ${CONFIG}/obj/socket.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/socket.c

${CC} -c -o ${CONFIG}/obj/upload.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/upload.c

${CC} -c -o ${CONFIG}/obj/matrixssl.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/ssl/matrixssl.c

${CC} -c -o ${CONFIG}/obj/openssl.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/ssl/openssl.c

${CC} -dynamiclib -o ${CONFIG}/bin/libgo.dylib -arch x86_64 ${LDFLAGS} -compatibility_version 3.1.0 -current_version 3.1.0 ${LIBPATHS} -install_name @rpath/libgo.dylib ${CONFIG}/obj/action.o ${CONFIG}/obj/alloc.o ${CONFIG}/obj/auth.o ${CONFIG}/obj/cgi.o ${CONFIG}/obj/crypt.o ${CONFIG}/obj/file.o ${CONFIG}/obj/http.o ${CONFIG}/obj/js.o ${CONFIG}/obj/jst.o ${CONFIG}/obj/options.o ${CONFIG}/obj/rom-documents.o ${CONFIG}/obj/rom.o ${CONFIG}/obj/route.o ${CONFIG}/obj/runtime.o ${CONFIG}/obj/socket.o ${CONFIG}/obj/upload.o ${CONFIG}/obj/matrixssl.o ${CONFIG}/obj/openssl.o ${LIBS}

${CC} -c -o ${CONFIG}/obj/goahead.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/goahead.c

${CC} -o ${CONFIG}/bin/goahead -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/goahead.o -lgo ${LIBS}

${CC} -c -o ${CONFIG}/obj/test.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/test.c

${CC} -o ${CONFIG}/bin/goahead-test -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/test.o -lgo ${LIBS}

${CC} -c -o ${CONFIG}/obj/gopass.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc utils/gopass.c

${CC} -o ${CONFIG}/bin/gopass -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/gopass.o -lgo ${LIBS}

${CC} -c -o ${CONFIG}/obj/webcomp.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc utils/webcomp.c

${CC} -o ${CONFIG}/bin/webcomp -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/webcomp.o ${LIBS}

${CC} -c -o ${CONFIG}/obj/cgitest.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/cgitest.c

${CC} -o test/cgi-bin/cgitest -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/cgitest.o ${LIBS}

