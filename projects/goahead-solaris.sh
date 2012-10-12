#
#   goahead-solaris.sh -- Build It Shell Script to build Embedthis GoAhead
#

ARCH="x86"
ARCH="$(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')"
OS="solaris"
PROFILE="debug"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="gcc"
LD="C:/cygwin/bin/ld.exe"
CFLAGS="-Wall -fPIC -g -mtune=generic"
DFLAGS="-D_REENTRANT -DPIC -DBIT_DEBUG"
IFLAGS="-I${CONFIG}\inc"
LDFLAGS="-g"
LIBPATHS="-L${CONFIG}\bin"
LIBS="-llxnet -lrt -lsocket -lpthread -lm -ldl"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin

[ ! -f ${CONFIG}/inc/bit.h ] && cp projects/goahead-${OS}-bit.h ${CONFIG}/inc/bit.h
if ! diff ${CONFIG}/inc/bit.h projects/goahead-${OS}-bit.h >/dev/null ; then
	cp projects/goahead-${OS}-bit.h ${CONFIG}/inc/bit.h
fi

rm -rf ${CONFIG}/inc/goahead.h
cp -r goahead.h ${CONFIG}/inc/goahead.h

rm -rf ${CONFIG}/inc/js.h
cp -r js.h ${CONFIG}/inc/js.h

${CC} -c -o ${CONFIG}\obj\action.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc action.c

${CC} -c -o ${CONFIG}\obj\auth.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc auth.c

${CC} -c -o ${CONFIG}\obj\cgi.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc cgi.c

${CC} -c -o ${CONFIG}\obj\crypt.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc crypt.c

${CC} -c -o ${CONFIG}\obj\file.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc file.c

${CC} -c -o ${CONFIG}\obj\galloc.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc galloc.c

${CC} -c -o ${CONFIG}\obj\http.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc http.c

${CC} -c -o ${CONFIG}\obj\js.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc js.c

${CC} -c -o ${CONFIG}\obj\jst.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc jst.c

${CC} -c -o ${CONFIG}\obj\matrixssl.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc matrixssl.c

${CC} -c -o ${CONFIG}\obj\openssl.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc openssl.c

${CC} -c -o ${CONFIG}\obj\options.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc options.c

${CC} -c -o ${CONFIG}\obj\rom-documents.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc rom-documents.c

${CC} -c -o ${CONFIG}\obj\rom.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc rom.c

${CC} -c -o ${CONFIG}\obj\route.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc route.c

${CC} -c -o ${CONFIG}\obj\runtime.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc runtime.c

${CC} -c -o ${CONFIG}\obj\socket.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc socket.c

${CC} -c -o ${CONFIG}\obj\upload.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc upload.c

C:/cygwin/bin/ar.exe -cr ${CONFIG}\bin\libgo.a ${CONFIG}\obj\action.o ${CONFIG}\obj\auth.o ${CONFIG}\obj\cgi.o ${CONFIG}\obj\crypt.o ${CONFIG}\obj\file.o ${CONFIG}\obj\galloc.o ${CONFIG}\obj\http.o ${CONFIG}\obj\js.o ${CONFIG}\obj\jst.o ${CONFIG}\obj\matrixssl.o ${CONFIG}\obj\openssl.o ${CONFIG}\obj\options.o ${CONFIG}\obj\rom-documents.o ${CONFIG}\obj\rom.o ${CONFIG}\obj\route.o ${CONFIG}\obj\runtime.o ${CONFIG}\obj\socket.o ${CONFIG}\obj\upload.o

${CC} -c -o ${CONFIG}\obj\goahead.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc goahead.c

${CC} -o ${CONFIG}\bin\goahead ${LDFLAGS} ${LIBPATHS} ${CONFIG}\obj\goahead.o -lgo ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}\obj\test.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc test/test.c

${CC} -o ${CONFIG}\bin\goahead-test ${LDFLAGS} ${LIBPATHS} ${CONFIG}\obj\test.o -lgo ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}\obj\gopass.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc utils/gopass.c

${CC} -o ${CONFIG}\bin\gopass ${LDFLAGS} ${LIBPATHS} ${CONFIG}\obj\gopass.o -lgo ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}\obj\webcomp.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc utils/webcomp.c

${CC} -o ${CONFIG}\bin\webcomp ${LDFLAGS} ${LIBPATHS} ${CONFIG}\obj\webcomp.o ${LIBS} ${LDFLAGS}

${CC} -c -o ${CONFIG}\obj\cgitest.o -Wall -fPIC ${LDFLAGS} -mtune=generic ${DFLAGS} -I${CONFIG}\inc test/cgitest.c

${CC} -o test/cgi-bin/cgitest ${LDFLAGS} ${LIBPATHS} ${CONFIG}\obj\cgitest.o ${LIBS} ${LDFLAGS}

