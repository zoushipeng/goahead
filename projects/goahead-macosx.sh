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

${CC} -c -o ${CONFIG}/obj/auth.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include auth.c

${CC} -c -o ${CONFIG}/obj/cgi.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include cgi.c

${CC} -c -o ${CONFIG}/obj/crypt.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include crypt.c

${CC} -c -o ${CONFIG}/obj/file.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include file.c

${CC} -c -o ${CONFIG}/obj/galloc.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include galloc.c

${CC} -c -o ${CONFIG}/obj/http.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include http.c

${CC} -c -o ${CONFIG}/obj/js.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include js.c

${CC} -c -o ${CONFIG}/obj/matrixssl.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include matrixssl.c

${CC} -c -o ${CONFIG}/obj/openssl.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include openssl.c

${CC} -c -o ${CONFIG}/obj/options.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include options.c

${CC} -c -o ${CONFIG}/obj/proc.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include proc.c

${CC} -c -o ${CONFIG}/obj/rom-documents.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include rom-documents.c

${CC} -c -o ${CONFIG}/obj/rom.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include rom.c

${CC} -c -o ${CONFIG}/obj/route.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include route.c

${CC} -c -o ${CONFIG}/obj/runtime.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include runtime.c

${CC} -c -o ${CONFIG}/obj/socket.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include socket.c

${CC} -c -o ${CONFIG}/obj/template.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include template.c

${CC} -c -o ${CONFIG}/obj/upload.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include upload.c

${CC} -dynamiclib -o ${CONFIG}/bin/libgo.dylib -arch x86_64 ${LDFLAGS} ${LIBPATHS} -L../packages-macosx-x64/openssl/openssl-1.0.1b -install_name @rpath/libgo.dylib ${CONFIG}/obj/auth.o ${CONFIG}/obj/cgi.o ${CONFIG}/obj/crypt.o ${CONFIG}/obj/file.o ${CONFIG}/obj/galloc.o ${CONFIG}/obj/http.o ${CONFIG}/obj/js.o ${CONFIG}/obj/matrixssl.o ${CONFIG}/obj/openssl.o ${CONFIG}/obj/options.o ${CONFIG}/obj/proc.o ${CONFIG}/obj/rom-documents.o ${CONFIG}/obj/rom.o ${CONFIG}/obj/route.o ${CONFIG}/obj/runtime.o ${CONFIG}/obj/socket.o ${CONFIG}/obj/template.o ${CONFIG}/obj/upload.o ${LIBS} -lssl -lcrypto

${CC} -c -o ${CONFIG}/obj/goahead.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include goahead.c

${CC} -o ${CONFIG}/bin/goahead -arch x86_64 ${LDFLAGS} ${LIBPATHS} -L../packages-macosx-x64/openssl/openssl-1.0.1b ${CONFIG}/obj/auth.o ${CONFIG}/obj/cgi.o ${CONFIG}/obj/crypt.o ${CONFIG}/obj/file.o ${CONFIG}/obj/galloc.o ${CONFIG}/obj/goahead.o ${CONFIG}/obj/http.o ${CONFIG}/obj/js.o ${CONFIG}/obj/matrixssl.o ${CONFIG}/obj/openssl.o ${CONFIG}/obj/options.o ${CONFIG}/obj/proc.o ${CONFIG}/obj/rom-documents.o ${CONFIG}/obj/rom.o ${CONFIG}/obj/route.o ${CONFIG}/obj/runtime.o ${CONFIG}/obj/socket.o ${CONFIG}/obj/template.o ${CONFIG}/obj/upload.o ${LIBS} -lssl -lcrypto

${CC} -c -o ${CONFIG}/obj/test.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include test/test.c

${CC} -o ${CONFIG}/bin/goahead-test -arch x86_64 ${LDFLAGS} ${LIBPATHS} -L../packages-macosx-x64/openssl/openssl-1.0.1b ${CONFIG}/obj/auth.o ${CONFIG}/obj/cgi.o ${CONFIG}/obj/crypt.o ${CONFIG}/obj/file.o ${CONFIG}/obj/galloc.o ${CONFIG}/obj/http.o ${CONFIG}/obj/js.o ${CONFIG}/obj/matrixssl.o ${CONFIG}/obj/openssl.o ${CONFIG}/obj/options.o ${CONFIG}/obj/proc.o ${CONFIG}/obj/rom-documents.o ${CONFIG}/obj/rom.o ${CONFIG}/obj/route.o ${CONFIG}/obj/runtime.o ${CONFIG}/obj/socket.o ${CONFIG}/obj/template.o ${CONFIG}/obj/upload.o ${CONFIG}/obj/test.o ${LIBS} -lssl -lcrypto

${CC} -c -o ${CONFIG}/obj/webcomp.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include utils/webcomp.c

${CC} -o ${CONFIG}/bin/webcomp -arch x86_64 ${LDFLAGS} ${LIBPATHS} -L../packages-macosx-x64/openssl/openssl-1.0.1b ${CONFIG}/obj/webcomp.o ${LIBS} -lssl -lcrypto

${CC} -c -o ${CONFIG}/obj/cgitest.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/cgitest.c

${CC} -o test/cgi-bin/cgitest -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/cgitest.o ${LIBS}

