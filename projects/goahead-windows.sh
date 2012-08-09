#
#   goahead-windows.sh -- Build It Shell Script to build Embedthis GoAhead
#

export PATH="$(SDK)/Bin:$(VS)/VC/Bin:$(VS)/Common7/IDE:$(VS)/Common7/Tools:$(VS)/SDK/v3.5/bin:$(VS)/VC/VCPackages;$(PATH)"
export INCLUDE="$(INCLUDE);$(SDK)/Include:$(VS)/VC/INCLUDE"
export LIB="$(LIB);$(SDK)/Lib:$(VS)/VC/lib"

ARCH="x86"
ARCH="$(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')"
OS="windows"
PROFILE="debug"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="cl.exe"
LD="link.exe"
CFLAGS="-nologo -GR- -W3 -Zi -Od -MDd"
DFLAGS="-D_REENTRANT -D_MT -DBIT_DEBUG"
IFLAGS="-I${CONFIG}/inc"
LDFLAGS="-nologo -nodefaultlib -incremental:no -debug -machine:x86"
LIBPATHS="-libpath:${CONFIG}/bin"
LIBS="ws2_32.lib advapi32.lib user32.lib kernel32.lib oldnames.lib msvcrt.lib shell32.lib"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin

[ ! -f ${CONFIG}/inc/bit.h ] && cp projects/goahead-${OS}-bit.h ${CONFIG}/inc/bit.h
if ! diff ${CONFIG}/inc/bit.h projects/goahead-${OS}-bit.h >/dev/null ; then
	cp projects/goahead-${OS}-bit.h ${CONFIG}/inc/bit.h
fi

rm -rf ${CONFIG}/inc/goahead.h
cp -r goahead.h ${CONFIG}/inc/goahead.h

rm -rf ${CONFIG}/inc/js.h
cp -r js.h ${CONFIG}/inc/js.h

"${CC}" -c -Fo${CONFIG}/obj/access.obj -Fd${CONFIG}/obj/access.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc access.c

"${CC}" -c -Fo${CONFIG}/obj/balloc.obj -Fd${CONFIG}/obj/balloc.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc balloc.c

"${CC}" -c -Fo${CONFIG}/obj/cgi.obj -Fd${CONFIG}/obj/cgi.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc cgi.c

"${CC}" -c -Fo${CONFIG}/obj/crypt.obj -Fd${CONFIG}/obj/crypt.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc crypt.c

"${CC}" -c -Fo${CONFIG}/obj/db.obj -Fd${CONFIG}/obj/db.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc db.c

"${CC}" -c -Fo${CONFIG}/obj/default.obj -Fd${CONFIG}/obj/default.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc default.c

"${CC}" -c -Fo${CONFIG}/obj/form.obj -Fd${CONFIG}/obj/form.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc form.c

"${CC}" -c -Fo${CONFIG}/obj/goahead.obj -Fd${CONFIG}/obj/goahead.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc goahead.c

"${CC}" -c -Fo${CONFIG}/obj/handler.obj -Fd${CONFIG}/obj/handler.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc handler.c

"${CC}" -c -Fo${CONFIG}/obj/http.obj -Fd${CONFIG}/obj/http.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc http.c

"${CC}" -c -Fo${CONFIG}/obj/js.obj -Fd${CONFIG}/obj/js.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc js.c

"${CC}" -c -Fo${CONFIG}/obj/matrixssl.obj -Fd${CONFIG}/obj/matrixssl.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc matrixssl.c

"${CC}" -c -Fo${CONFIG}/obj/openssl.obj -Fd${CONFIG}/obj/openssl.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc openssl.c

"${CC}" -c -Fo${CONFIG}/obj/rom-documents.obj -Fd${CONFIG}/obj/rom-documents.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc rom-documents.c

"${CC}" -c -Fo${CONFIG}/obj/rom.obj -Fd${CONFIG}/obj/rom.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc rom.c

"${CC}" -c -Fo${CONFIG}/obj/runtime.obj -Fd${CONFIG}/obj/runtime.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc runtime.c

"${CC}" -c -Fo${CONFIG}/obj/security.obj -Fd${CONFIG}/obj/security.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc security.c

"${CC}" -c -Fo${CONFIG}/obj/socket.obj -Fd${CONFIG}/obj/socket.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc socket.c

"${CC}" -c -Fo${CONFIG}/obj/ssl.obj -Fd${CONFIG}/obj/ssl.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc ssl.c

"${CC}" -c -Fo${CONFIG}/obj/template.obj -Fd${CONFIG}/obj/template.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc template.c

"${CC}" -c -Fo${CONFIG}/obj/um.obj -Fd${CONFIG}/obj/um.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc um.c

"${LD}" -out:${CONFIG}/bin/goahead.exe -entry:WinMainCRTStartup -subsystem:Windows ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/access.obj ${CONFIG}/obj/balloc.obj ${CONFIG}/obj/cgi.obj ${CONFIG}/obj/crypt.obj ${CONFIG}/obj/db.obj ${CONFIG}/obj/default.obj ${CONFIG}/obj/form.obj ${CONFIG}/obj/goahead.obj ${CONFIG}/obj/handler.obj ${CONFIG}/obj/http.obj ${CONFIG}/obj/js.obj ${CONFIG}/obj/matrixssl.obj ${CONFIG}/obj/openssl.obj ${CONFIG}/obj/rom-documents.obj ${CONFIG}/obj/rom.obj ${CONFIG}/obj/runtime.obj ${CONFIG}/obj/security.obj ${CONFIG}/obj/socket.obj ${CONFIG}/obj/ssl.obj ${CONFIG}/obj/template.obj ${CONFIG}/obj/um.obj ${LIBS}

"${CC}" -c -Fo${CONFIG}/obj/test.obj -Fd${CONFIG}/obj/test.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/test.c

"${LD}" -out:${CONFIG}/bin/goahead-test.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/access.obj ${CONFIG}/obj/balloc.obj ${CONFIG}/obj/cgi.obj ${CONFIG}/obj/crypt.obj ${CONFIG}/obj/db.obj ${CONFIG}/obj/default.obj ${CONFIG}/obj/form.obj ${CONFIG}/obj/handler.obj ${CONFIG}/obj/http.obj ${CONFIG}/obj/js.obj ${CONFIG}/obj/matrixssl.obj ${CONFIG}/obj/openssl.obj ${CONFIG}/obj/rom-documents.obj ${CONFIG}/obj/rom.obj ${CONFIG}/obj/runtime.obj ${CONFIG}/obj/security.obj ${CONFIG}/obj/socket.obj ${CONFIG}/obj/ssl.obj ${CONFIG}/obj/template.obj ${CONFIG}/obj/um.obj ${CONFIG}/obj/test.obj ${LIBS}

"${CC}" -c -Fo${CONFIG}/obj/webcomp.obj -Fd${CONFIG}/obj/webcomp.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc utils/webcomp.c

"${LD}" -out:${CONFIG}/bin/webcomp.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/webcomp.obj ${LIBS}

"${CC}" -c -Fo${CONFIG}/obj/cgitest.obj -Fd${CONFIG}/obj/cgitest.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/cgitest.c

"${LD}" -out:test/cgi-bin/cgitest.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/cgitest.obj ${LIBS}

