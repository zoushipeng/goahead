#
#   goahead-linux-debug.sh -- Build It Shell Script to build Embedthis GoAhead
#

ARCH="x86"
ARCH="`uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/'`"
OS="linux"
PROFILE="debug"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="/usr/bin/gcc"
LD="/usr/bin/ld"
CFLAGS="-fPIC   -w"
DFLAGS="-D_REENTRANT -DPIC -DBIT_DEBUG"
IFLAGS="-I${CONFIG}/inc"
LDFLAGS="-Wl,--enable-new-dtags -Wl,-rpath,\$ORIGIN/ -Wl,-rpath,\$ORIGIN/../bin -rdynamic -g"
LIBPATHS="-L${CONFIG}/bin"
LIBS="-lpthread -lm -lrt -ldl"

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

${CC} -c -o ${CONFIG}/obj/action.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/action.c

${CC} -c -o ${CONFIG}/obj/alloc.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/alloc.c

${CC} -c -o ${CONFIG}/obj/auth.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/auth.c

${CC} -c -o ${CONFIG}/obj/cgi.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/cgi.c

${CC} -c -o ${CONFIG}/obj/crypt.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/crypt.c

${CC} -c -o ${CONFIG}/obj/file.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/file.c

${CC} -c -o ${CONFIG}/obj/http.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/http.c

${CC} -c -o ${CONFIG}/obj/js.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/js.c

${CC} -c -o ${CONFIG}/obj/jst.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/jst.c

${CC} -c -o ${CONFIG}/obj/options.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/options.c

${CC} -c -o ${CONFIG}/obj/rom-documents.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/rom-documents.c

${CC} -c -o ${CONFIG}/obj/rom.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/rom.c

${CC} -c -o ${CONFIG}/obj/route.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/route.c

${CC} -c -o ${CONFIG}/obj/runtime.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/runtime.c

${CC} -c -o ${CONFIG}/obj/socket.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/socket.c

${CC} -c -o ${CONFIG}/obj/upload.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/upload.c

${CC} -c -o ${CONFIG}/obj/matrixssl.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/ssl/matrixssl.c

${CC} -c -o ${CONFIG}/obj/openssl.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/ssl/openssl.c

${CC} -shared -o ${CONFIG}/bin/libgo.so ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/action.o ${CONFIG}/obj/alloc.o ${CONFIG}/obj/auth.o ${CONFIG}/obj/cgi.o ${CONFIG}/obj/crypt.o ${CONFIG}/obj/file.o ${CONFIG}/obj/http.o ${CONFIG}/obj/js.o ${CONFIG}/obj/jst.o ${CONFIG}/obj/options.o ${CONFIG}/obj/rom-documents.o ${CONFIG}/obj/rom.o ${CONFIG}/obj/route.o ${CONFIG}/obj/runtime.o ${CONFIG}/obj/socket.o ${CONFIG}/obj/upload.o ${CONFIG}/obj/matrixssl.o ${CONFIG}/obj/openssl.o ${LIBS}

rm -rf ${CONFIG}/inc/*.h
cp -r *.h ${CONFIG}/inc/*.h

${CC} -c -o ${CONFIG}/obj/goahead.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/goahead.c

${CC} -o ${CONFIG}/bin/goahead ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/goahead.o -lgo ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}/obj/test.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/test.c

${CC} -o ${CONFIG}/bin/goahead-test ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/test.o -lgo ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}/obj/gopass.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc utils/gopass.c

${CC} -o ${CONFIG}/bin/gopass ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/gopass.o -lgo ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}/obj/webcomp.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc utils/webcomp.c

${CC} -o ${CONFIG}/bin/webcomp ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/webcomp.o ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}/obj/cgitest.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/cgitest.c

${CC} -o test/cgi-bin/cgitest ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/cgitest.o ${LIBS} ${LDFLAGS}

