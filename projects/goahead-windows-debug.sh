#
#   goahead-windows-debug.sh -- Build It Shell Script to build Embedthis GoAhead
#

export PATH="$(SDK)/Bin:$(VS)/VC/Bin:$(VS)/Common7/IDE:$(VS)/Common7/Tools:$(VS)/SDK/v3.5/bin:$(VS)/VC/VCPackages;$(PATH)"
export INCLUDE="$(INCLUDE);$(SDK)/Include:$(VS)/VC/INCLUDE"
export LIB="$(LIB);$(SDK)/Lib:$(VS)/VC/lib"

ARCH="x86"
ARCH="`uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/'`"
OS="windows"
PROFILE="debug"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="cl.exe"
LD="link.exe"
CFLAGS="-nologo -GR- -W3 -Zi -Od -MDd -w"
DFLAGS="-D_REENTRANT -D_MT -DBIT_DEBUG"
IFLAGS="-I${CONFIG}/inc"
LDFLAGS="-nologo -nodefaultlib -incremental:no -debug -machine:x86"
LIBPATHS="-libpath:${CONFIG}/bin"
LIBS="ws2_32.lib advapi32.lib user32.lib kernel32.lib oldnames.lib msvcrt.lib shell32.lib"

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

"${CC}" -c -Fo${CONFIG}/obj/action.obj -Fd${CONFIG}/obj/action.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/action.c

"${CC}" -c -Fo${CONFIG}/obj/alloc.obj -Fd${CONFIG}/obj/alloc.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/alloc.c

"${CC}" -c -Fo${CONFIG}/obj/auth.obj -Fd${CONFIG}/obj/auth.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/auth.c

"${CC}" -c -Fo${CONFIG}/obj/cgi.obj -Fd${CONFIG}/obj/cgi.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/cgi.c

"${CC}" -c -Fo${CONFIG}/obj/crypt.obj -Fd${CONFIG}/obj/crypt.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/crypt.c

"${CC}" -c -Fo${CONFIG}/obj/file.obj -Fd${CONFIG}/obj/file.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/file.c

"${CC}" -c -Fo${CONFIG}/obj/http.obj -Fd${CONFIG}/obj/http.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/http.c

"${CC}" -c -Fo${CONFIG}/obj/js.obj -Fd${CONFIG}/obj/js.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/js.c

"${CC}" -c -Fo${CONFIG}/obj/jst.obj -Fd${CONFIG}/obj/jst.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/jst.c

"${CC}" -c -Fo${CONFIG}/obj/options.obj -Fd${CONFIG}/obj/options.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/options.c

"${CC}" -c -Fo${CONFIG}/obj/rom-documents.obj -Fd${CONFIG}/obj/rom-documents.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/rom-documents.c

"${CC}" -c -Fo${CONFIG}/obj/rom.obj -Fd${CONFIG}/obj/rom.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/rom.c

"${CC}" -c -Fo${CONFIG}/obj/route.obj -Fd${CONFIG}/obj/route.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/route.c

"${CC}" -c -Fo${CONFIG}/obj/runtime.obj -Fd${CONFIG}/obj/runtime.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/runtime.c

"${CC}" -c -Fo${CONFIG}/obj/socket.obj -Fd${CONFIG}/obj/socket.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/socket.c

"${CC}" -c -Fo${CONFIG}/obj/upload.obj -Fd${CONFIG}/obj/upload.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/upload.c

"${CC}" -c -Fo${CONFIG}/obj/matrixssl.obj -Fd${CONFIG}/obj/matrixssl.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/ssl/matrixssl.c

"${CC}" -c -Fo${CONFIG}/obj/openssl.obj -Fd${CONFIG}/obj/openssl.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/ssl/openssl.c

"${LD}" -dll -out:${CONFIG}/bin/libgo.dll -entry:_DllMainCRTStartup@12 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/action.obj ${CONFIG}/obj/alloc.obj ${CONFIG}/obj/auth.obj ${CONFIG}/obj/cgi.obj ${CONFIG}/obj/crypt.obj ${CONFIG}/obj/file.obj ${CONFIG}/obj/http.obj ${CONFIG}/obj/js.obj ${CONFIG}/obj/jst.obj ${CONFIG}/obj/options.obj ${CONFIG}/obj/rom-documents.obj ${CONFIG}/obj/rom.obj ${CONFIG}/obj/route.obj ${CONFIG}/obj/runtime.obj ${CONFIG}/obj/socket.obj ${CONFIG}/obj/upload.obj ${CONFIG}/obj/matrixssl.obj ${CONFIG}/obj/openssl.obj ${LIBS}

rm -rf ${CONFIG}/inc/*.h
cp -r *.h ${CONFIG}/inc/*.h

"${CC}" -c -Fo${CONFIG}/obj/goahead.obj -Fd${CONFIG}/obj/goahead.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc src/goahead.c

"${LD}" -out:${CONFIG}/bin/goahead.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/goahead.obj libgo.lib ${LIBS}

"${CC}" -c -Fo${CONFIG}/obj/test.obj -Fd${CONFIG}/obj/test.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/test.c

"${LD}" -out:${CONFIG}/bin/goahead-test.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/test.obj libgo.lib ${LIBS}

"${CC}" -c -Fo${CONFIG}/obj/gopass.obj -Fd${CONFIG}/obj/gopass.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc utils/gopass.c

"${LD}" -out:${CONFIG}/bin/gopass.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/gopass.obj libgo.lib ${LIBS}

"${CC}" -c -Fo${CONFIG}/obj/webcomp.obj -Fd${CONFIG}/obj/webcomp.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc utils/webcomp.c

"${LD}" -out:${CONFIG}/bin/webcomp.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/webcomp.obj ${LIBS}

"${CC}" -c -Fo${CONFIG}/obj/cgitest.obj -Fd${CONFIG}/obj/cgitest.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc test/cgitest.c

"${LD}" -out:test/cgi-bin/cgitest.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/cgitest.obj ${LIBS}

