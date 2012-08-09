#
#   goahead-macosx.sh -- Build It Shell Script to build Embedthis GoAhead
#

ARCH="x64"
ARCH="$(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')"
OS="macosx"
PROFILE="debug"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="/usr/bin/clang"
LD="/usr/bin/ld"
CFLAGS="-Wall -Wno-deprecated-declarations -g -Wno-unused-result -Wshorten-64-to-32"
DFLAGS="-DBIT_DEBUG"
IFLAGS="-I${CONFIG}/inc"
LDFLAGS="-Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -g"
LIBPATHS="-L${CONFIG}/bin"
LIBS="-lpthread -lm -ldl"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin

[ ! -f ${CONFIG}/inc/bit.h ] && cp projects/goahead-${OS}-bit.h ${CONFIG}/inc/bit.h
if ! diff ${CONFIG}/inc/bit.h projects/goahead-${OS}-bit.h >/dev/null ; then
	cp projects/goahead-${OS}-bit.h ${CONFIG}/inc/bit.h
fi

rm -rf ${CONFIG}/inc/goahead.h
cp -r goahead.h ${CONFIG}/inc/goahead.h

rm -rf ${CONFIG}/inc/js.h
cp -r js.h ${CONFIG}/inc/js.h

${CC} -c -o ${CONFIG}/obj/balloc.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include balloc.c

${CC} -c -o ${CONFIG}/obj/cgi.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include cgi.c

${CC} -c -o ${CONFIG}/obj/crypt.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include crypt.c

${CC} -c -o ${CONFIG}/obj/db.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include db.c

${CC} -c -o ${CONFIG}/obj/default.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include default.c

${CC} -c -o ${CONFIG}/obj/form.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include form.c

${CC} -c -o ${CONFIG}/obj/goahead.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include goahead.c

${CC} -c -o ${CONFIG}/obj/handler.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include handler.c

${CC} -c -o ${CONFIG}/obj/http.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include http.c

${CC} -c -o ${CONFIG}/obj/js.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include js.c

${CC} -c -o ${CONFIG}/obj/matrixssl.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include matrixssl.c

${CC} -c -o ${CONFIG}/obj/openssl.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include openssl.c

${CC} -c -o ${CONFIG}/obj/rom-documents.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include rom-documents.c

${CC} -c -o ${CONFIG}/obj/rom.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include rom.c

${CC} -c -o ${CONFIG}/obj/runtime.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include runtime.c

${CC} -c -o ${CONFIG}/obj/security.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include security.c

${CC} -c -o ${CONFIG}/obj/socket.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include socket.c

${CC} -c -o ${CONFIG}/obj/ssl.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include ssl.c

${CC} -c -o ${CONFIG}/obj/template.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include template.c

${CC} -c -o ${CONFIG}/obj/um.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include um.c

${CC} -o ${CONFIG}/bin/goahead -arch x86_64 ${LDFLAGS} ${LIBPATHS} -L../packages-macosx-x64/openssl/openssl-1.0.1b ${CONFIG}/obj/balloc.o ${CONFIG}/obj/cgi.o ${CONFIG}/obj/crypt.o ${CONFIG}/obj/db.o ${CONFIG}/obj/default.o ${CONFIG}/obj/form.o ${CONFIG}/obj/goahead.o ${CONFIG}/obj/handler.o ${CONFIG}/obj/http.o ${CONFIG}/obj/js.o ${CONFIG}/obj/matrixssl.o ${CONFIG}/obj/openssl.o ${CONFIG}/obj/rom-documents.o ${CONFIG}/obj/rom.o ${CONFIG}/obj/runtime.o ${CONFIG}/obj/security.o ${CONFIG}/obj/socket.o ${CONFIG}/obj/ssl.o ${CONFIG}/obj/template.o ${CONFIG}/obj/um.o ${LIBS} -lssl -lcrypto

${CC} -c -o ${CONFIG}/obj/test.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include test/test.c

${CC} -o ${CONFIG}/bin/goahead-test -arch x86_64 ${LDFLAGS} ${LIBPATHS} -L../packages-macosx-x64/openssl/openssl-1.0.1b ${CONFIG}/obj/balloc.o ${CONFIG}/obj/cgi.o ${CONFIG}/obj/crypt.o ${CONFIG}/obj/db.o ${CONFIG}/obj/default.o ${CONFIG}/obj/form.o ${CONFIG}/obj/handler.o ${CONFIG}/obj/http.o ${CONFIG}/obj/js.o ${CONFIG}/obj/matrixssl.o ${CONFIG}/obj/openssl.o ${CONFIG}/obj/rom-documents.o ${CONFIG}/obj/rom.o ${CONFIG}/obj/runtime.o ${CONFIG}/obj/security.o ${CONFIG}/obj/socket.o ${CONFIG}/obj/ssl.o ${CONFIG}/obj/template.o ${CONFIG}/obj/um.o ${CONFIG}/obj/test.o ${LIBS} -lssl -lcrypto

${CC} -c -o ${CONFIG}/obj/webcomp.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include utils/webcomp.c

${CC} -o ${CONFIG}/bin/webcomp -arch x86_64 ${LDFLAGS} ${LIBPATHS} -L../packages-macosx-x64/openssl/openssl-1.0.1b ${CONFIG}/obj/webcomp.o ${LIBS} -lssl -lcrypto

${CC} -c -o ${CONFIG}/obj/cgitest.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/cgitest.c

${CC} -o test/cgi-bin/cgitest -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/cgitest.o ${LIBS}

