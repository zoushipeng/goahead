/*
    goahead.h -- GoAhead Web Server Header
  
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_GOAHEAD
#define _h_GOAHEAD 1

/************************************ Defaults ********************************/

#include    "bit.h"

#ifndef BIT_DEBUG
    #define BIT_DEBUG 0
#endif
#ifndef BIT_ASSERT
    #if BIT_DEBUG
        #define BIT_ASSERT 1
    #else
        #define BIT_ASSERT 0
    #endif
#endif
#ifndef BIT_FLOAT
    #define BIT_FLOAT 1
#endif
#ifndef BIT_ROM
    #define BIT_ROM 0
#endif

/********************************* CPU Families *******************************/
/*
    CPU Architectures
 */
#define BIT_CPU_UNKNOWN     0
#define BIT_CPU_ARM         1           /* Arm */
#define BIT_CPU_ITANIUM     2           /* Intel Itanium */
#define BIT_CPU_X86         3           /* X86 */
#define BIT_CPU_X64         4           /* AMD64 or EMT64 */
#define BIT_CPU_MIPS        5           /* Mips */
#define BIT_CPU_PPC         6           /* Power PC */
#define BIT_CPU_SPARC       7           /* Sparc */

/*
    Use compiler definitions to determine the CPU
 */
#if defined(__alpha__)
    #define BIT_CPU "ALPHA"
    #define BIT_CPU_ARCH BIT_CPU_ALPHA
#elif defined(__arm__)
    #define BIT_CPU "ARM"
    #define BIT_CPU_ARCH BIT_CPU_ARM
#elif defined(__x86_64__) || defined(_M_AMD64)
    #define BIT_CPU "x64"
    #define BIT_CPU_ARCH BIT_CPU_X64
#elif defined(__i386__) || defined(__i486__) || defined(__i585__) || defined(__i686__) || defined(_M_IX86)
    #define BIT_CPU "x86"
    #define BIT_CPU_ARCH BIT_CPU_X86
#elif defined(_M_IA64)
    #define BIT_CPU "IA64"
    #define BIT_CPU_ARCH BIT_CPU_ITANIUM
#elif defined(__mips__)
    #define BIT_CPU "MIPS"
    #define BIT_CPU_ARCH BIT_CPU_SPARC
#elif defined(__ppc__) || defined(__powerpc__) || defined(__ppc64__)
    #define BIT_CPU "PPC"
    #define BIT_CPU_ARCH BIT_CPU_PPC
#elif defined(__sparc__)
    #define BIT_CPU "SPARC"
    #define BIT_CPU_ARCH BIT_CPU_SPARC
#endif

/*
    Operating system defines. Use compiler standard defintions to sleuth.  Works for all except VxWorks which does not
    define any special symbol.  NOTE: Support for SCOV Unix, LynxOS and UnixWare is deprecated. 
 */
#if defined(__APPLE__)
    #define BIT_OS "macosx"
    #define MACOSX 1
    #define BIT_UNIX_LIKE 1
    #define BIT_WIN_LIKE 0
#elif defined(__linux__)
    #define BIT_OS "linux"
    #define LINUX 1
    #define BIT_UNIX_LIKE 1
    #define BIT_WIN_LIKE 0
#elif defined(__FreeBSD__)
    #define BIT_OS "freebsd"
    #define FREEBSD 1
    #define BIT_UNIX_LIKE 1
    #define BIT_WIN_LIKE 0
#elif defined(_WIN32)
    #define BIT_OS "windows"
    #define WINDOWS 1
    #define BIT_UNIX_LIKE 0
    #define BIT_WIN_LIKE 1
#elif defined(__OS2__)
    #define BIT_OS "os2"
    #define OS2 0
    #define BIT_UNIX_LIKE 0
    #define BIT_WIN_LIKE 0
#elif defined(MSDOS) || defined(__DOS__)
    #define BIT_OS "msdos"
    #define WINDOWS 0
    #define BIT_UNIX_LIKE 0
    #define BIT_WIN_LIKE 0
#elif defined(__NETWARE_386__)
    #define BIT_OS "netware"
    #define NETWARE 0
    #define BIT_UNIX_LIKE 0
    #define BIT_WIN_LIKE 0
#elif defined(__bsdi__)
    #define BIT_OS "bsdi"
    #define BSDI 1
    #define BIT_UNIX_LIKE 1
    #define BIT_WIN_LIKE 0
#elif defined(__NetBSD__)
    #define BIT_OS "netbsd"
    #define NETBSD 1
    #define BIT_UNIX_LIKE 1
    #define BIT_WIN_LIKE 0
#elif defined(__QNX__)
    #define BIT_OS "qnx"
    #define QNX 1
    #define BIT_UNIX_LIKE 0
    #define BIT_WIN_LIKE 0
#elif defined(__hpux)
    #define BIT_OS "hpux"
    #define HPUX 1
    #define BIT_UNIX_LIKE 1
    #define BIT_WIN_LIKE 0
#elif defined(_AIX)
    #define BIT_OS "aix"
    #define AIX 1
    #define BIT_UNIX_LIKE 1
    #define BIT_WIN_LIKE 0
#elif defined(__CYGWIN__)
    #define BIT_OS "cygwin"
    #define CYGWIN 1
    #define BIT_UNIX_LIKE 1
    #define BIT_WIN_LIKE 0
#elif defined(__VMS)
    #define BIT_OS "vms"
    #define VMS 1
    #define BIT_UNIX_LIKE 0
    #define BIT_WIN_LIKE 0
#elif defined(VXWORKS)
    /* VxWorks does not have a pre-defined symbol */
    #define BIT_OS "vxworks"
    #define BIT_UNIX_LIKE 0
    #define BIT_WIN_LIKE 0
#elif defined(ECOS)
    /* ECOS may not have a pre-defined symbol */
    #define BIT_OS "ecos"
    #define BIT_UNIX_LIKE 0
    #define BIT_WIN_LIKE 0
#endif

#if __WORDSIZE == 64 || __amd64 || __x86_64 || __x86_64__ || _WIN64
    #define BIT_64 1
    #define BIT_WORDSIZE 64
#else
    #define BIT_WORDSIZE 32
#endif

/********************************* O/S Includes *******************************/
/*
    Out-of-order definitions and includes. Order really matters in this section
 */
#if WINDOWS
    #undef      _CRT_SECURE_NO_DEPRECATE
    #define     _CRT_SECURE_NO_DEPRECATE 1
    #undef      _CRT_SECURE_NO_WARNINGS
    #define     _CRT_SECURE_NO_WARNINGS 1
    #ifndef     _WIN32_WINNT
        #define _WIN32_WINNT 0x501
    #endif
#endif

#if WINDOWS
    #include    <direct.h>
    #include    <io.h>
    #include    <sys/stat.h>
    #include    <limits.h>
    #include    <tchar.h>
    #include    <winsock2.h>
    #include    <windows.h>
    #include    <winnls.h>
    #include    <ws2tcpip.h>
    #include    <time.h>
    #include    <sys/types.h>
    #include    <stdio.h>
    #include    <stdlib.h>
    #include    <fcntl.h>
    #include    <errno.h>
    #include    <share.h>
#endif /* WINDOWS */

#if CE
    /*#include  <errno.h>*/
    #include    <limits.h>
    #include    <tchar.h>
    #include    <windows.h>
    #include    <winsock.h>
    #include    <winnls.h>
    #include    "CE/wincompat.h"
#endif /* CE */

#if NETWARE
    #include    <direct.h>
    #include    <io.h>
    #include    <sys/stat.h>
    #include    <time.h>
    #include    <sys/types.h>
    #include    <stdio.h>
    #include    <stdlib.h>
    #include    <fcntl.h>
    #include    <errno.h>
    #include    <niterror.h>
    #include    <limits.h>
    #include    <netdb.h>
    #include    <process.h>
    #include    <tiuser.h>
    #include    <sys/time.h>
    #include    <arpa/inet.h>
    #include    <sys/types.h>
    #include    <sys/socket.h>
    #include    <sys/filio.h>
    #include    <netinet/in.h>
#endif /* NETWARE */

#if SCOV5 || AIX || HPUX
    #include    <sys/types.h>
    #include    <dirent.h>
    #include    <stdio.h>
    #include    <signal.h>
    #include    <unistd.h>
    #include    "sys/socket.h"
    #include    "sys/select.h"
    #include    "netinet/in.h"
    #include    "arpa/inet.h"
    #include    "netdb.h"
#endif /* SCOV5 || AIX || HUPX */

#if LINUX
    #include    <sys/types.h>
    #include    <sys/stat.h>
    #include    <sys/param.h>
    #include    <dirent.h>
    #include    <limits.h>
    #include    <signal.h>
    #include    <stdio.h>
    #include    <stdlib.h>
    #include    <unistd.h>
    #include    <sys/socket.h>
    #include    <sys/select.h>
    #include    <netinet/in.h>
    #include    <arpa/inet.h>
    #include    <netdb.h>
    #include    <time.h>
    #include    <fcntl.h>
    #include    <errno.h>
    #include    <sys/wait.h>
#endif /* LINUX */

#if LYNX
    #include    <dirent.h>
    #include    <limits.h>
    #include    <signal.h>
    #include    <stdarg.h>
    #include    <stdio.h>
    #include    <stdlib.h>
    #include    <unistd.h>
    #include    <socket.h>
    #include    <netinet/in.h>
    #include    <arpa/inet.h>
    #include    <netdb.h>
    #include    <time.h>
    #include    <fcntl.h>
    #include    <errno.h>
#endif /* LYNX */

#if MACOSX
    #include    <dirent.h>
    #include    <limits.h>
    #include    <sys/select.h>
    #include    <sys/types.h>
    #include    <sys/stat.h>
    #include    <stdio.h>
    #include    <stdlib.h>
    #include    <unistd.h>
    #include    <sys/socket.h>
    #include    <netinet/in.h>
    #include    <arpa/inet.h>
    #include    <netdb.h>
    #include    <fcntl.h>
    #include    <errno.h>
    #include    <time.h>
    #include    <stdbool.h>
    #include    <sys/wait.h>
#endif /* MACOSX */

#if UW
    #include    <stdio.h>
#endif /* UW */

#if VXWORKS
    #ifndef _VSB_CONFIG_FILE                                                                               
        #define _VSB_CONFIG_FILE "vsbConfig.h"                                                             
    #endif
    #include    <vxWorks.h>
    #include    <iosLib.h>
    #include    <sockLib.h>
    #include    <selectLib.h>
    #include    <inetLib.h>
    #include    <ioLib.h>
    #include    <stdio.h>
    #include    <stat.h>
    #include    <time.h>
    #include    <usrLib.h>
    #include    <fcntl.h>
    #include    <errno.h>
    #include    <hostLib.h>
    #include    <dirent.h>
    #include    <sysSymTbl.h>
    #include    <loadLib.h>
    #include    <unldLib.h>
    #include    <envLib.h>
#endif /* VXWORKS */

#if SOLARIS
    #include    <sys/types.h>
    #include    <limits.h>
    #include    <stdio.h>
    #include    <stdlib.h>
    #include    <unistd.h>
    #include    <socket.h>
    #include    <sys/select.h>
    #include    <netinet/in.h>
    #include    <arpa/inet.h>
    #include    <netdb.h>
    #include    <time.h>
    #include    <fcntl.h>
    #include    <errno.h>
    #include    <dirent.h>
#endif /* SOLARIS */

#if QNX
    #include    <sys/types.h>
    #include    <stdio.h>
    #include    <sys/socket.h>
    #include    <sys/select.h>
    #include    <netinet/in.h>
    #include    <arpa/inet.h>
    #include    <netdb.h>
    #include    <stdlib.h>
    #include    <unistd.h>
    #include    <sys/uio.h>
    #include    <sys/wait.h>
    #include    <dirent.h>
#endif /* QNX */

#if ECOS
    #include    <limits.h>
    #include    <cyg/infra/cyg_type.h>
    #include    <cyg/kernel/kapi.h>
    #include    <time.h>
    #include    <network.h>
    #include    <errno.h>
#endif /* ECOS */

/*
    Included by all
 */
#include    <ctype.h>
#include    <stdarg.h>
#include    <string.h>
#include    <stdlib.h>

#if BIT_PACK_OPENSSL
/* Clashes with WinCrypt.h */
#undef OCSP_RESPONSE
#include    <openssl/ssl.h>
#include    <openssl/evp.h>
#include    <openssl/rand.h>
#include    <openssl/err.h>
#include    <openssl/dh.h>
#endif

#if BIT_FEATURE_MATRIXSSL && UNUSED
/*
   Matrixssl defines int32, uint32, int64 and uint64, but does not provide HAS_XXX to disable.
   So must include matrixsslApi.h first and then workaround.
 */
 #include    "matrixsslApi.h"

#define     HAS_INT32 1
#define     HAS_UINT32 1
#define     HAS_INT64 1
#define     HAS_UINT64 1
#define     MAX_WRITE_MSEC 500
#endif

/************************************** Defines *******************************/
#ifdef __cplusplus
extern "C" {
#endif

/*
    Standard types
 */
#ifndef HAS_BOOL
    #ifndef __cplusplus
        #if !MACOSX
            #define HAS_BOOL 1
            /**
                Boolean data type.
             */
            typedef char bool;
        #endif
    #endif
#endif

#ifndef HAS_UCHAR
    #define HAS_UCHAR 1
    /**
        Unsigned char data type.
     */
    typedef unsigned char uchar;
#endif

#ifndef HAS_SCHAR
    #define HAS_SCHAR 1
    /**
        Signed char data type.
     */
    typedef signed char schar;
#endif

#ifndef HAS_CCHAR
    #define HAS_CCHAR 1
    /**
        Constant char data type.
     */
    typedef const char cchar;
#endif

#ifndef HAS_CUCHAR
    #define HAS_CUCHAR 1
    /**
        Unsigned char data type.
     */
    typedef const unsigned char cuchar;
#endif

#ifndef HAS_USHORT
    #define HAS_USHORT 1
    /**
        Unsigned short data type.
     */
    typedef unsigned short ushort;
#endif

#ifndef HAS_CUSHORT
    #define HAS_CUSHORT 1
    /**
        Constant unsigned short data type.
     */
    typedef const unsigned short cushort;
#endif

#ifndef HAS_CVOID
    #define HAS_CVOID 1
    /**
        Constant void data type.
     */
    typedef const void cvoid;
#endif

#ifndef HAS_INT32
    #define HAS_INT32 1
    /**
        Integer 32 bits data type.
     */
    typedef int int32;
#endif

#ifndef HAS_UINT32
    #define HAS_UINT32 1
    /**
        Unsigned integer 32 bits data type.
     */
    typedef unsigned int uint32;
#endif

#ifndef HAS_UINT
    #define HAS_UINT 1
    /**
        Unsigned integer (machine dependent bit size) data type.
     */
    typedef unsigned int uint;
#endif

#ifndef HAS_ULONG
    #define HAS_ULONG 1
    /**
        Unsigned long (machine dependent bit size) data type.
     */
    typedef unsigned long ulong;
#endif

#ifndef HAS_SSIZE
    #define HAS_SSIZE 1
    #if BIT_UNIX_LIKE || VXWORKS || DOXYGEN
        /**
            Signed integer size field large enough to hold a pointer offset.
         */
        typedef ssize_t ssize;
    #else
        typedef SSIZE_T ssize;
    #endif
#endif

#ifdef __USE_FILE_OFFSET64
    #define BIT_HAS_OFF64 1
#else
    #define BIT_HAS_OFF64 0
#endif

#if BIT_UNIX_LIKE
    #define BIT_HAS_FCNTL 1
#endif

/*
    Windows uses uint for write/read counts (Ugh!)
 */
#if BIT_WIN_LIKE
    typedef uint wsize;
#else
    typedef ssize wsize;
#endif

#ifndef HAS_INT64
    #if BIT_UNIX_LIKE
        __extension__ typedef long long int int64;
    #elif VXWORKS || DOXYGEN
        /**
            Integer 64 bit data type.
         */
        typedef long long int int64;
    #elif BIT_WIN_LIKE
        typedef __int64 int64;
    #else
        typedef long long int int64;
    #endif
#endif

#ifndef HAS_UINT64
    #if BIT_UNIX_LIKE
        __extension__ typedef unsigned long long int uint64;
    #elif VXWORKS || DOXYGEN
        typedef unsigned long long int uint64;
    #elif BIT_WIN_LIKE
        typedef unsigned __int64 uint64;
    #else
        typedef unsigned long long int uint64;
    #endif
#endif

#ifndef BITSPERBYTE
    #define BITSPERBYTE (8 * sizeof(char))
#endif

#ifndef BITS
    #define BITS(type)  (BITSPERBYTE * (int) sizeof(type))
#endif

#if BIT_FLOAT
    #ifndef MAXFLOAT
        #if BIT_WIN_LIKE
            #define MAXFLOAT  DBL_MAX
        #else
            #define MAXFLOAT  FLT_MAX
        #endif
    #endif
    #if VXWORKS
        #define isnan(n)  ((n) != (n))
        #define isnanf(n) ((n) != (n))
        #define isinf(n)  ((n) == (1.0 / 0.0) || (n) == (-1.0 / 0.0))
        #define isinff(n) ((n) == (1.0 / 0.0) || (n) == (-1.0 / 0.0))
    #endif
    #if BIT_WIN_LIKE
        #define isNan(f) (_isnan(f))
    #elif VXWORKS || MACOSX || LINUX
        #define isNan(f) (isnan(f))
    #else
        #define isNan(f) (fpclassify(f) == FP_NAN)
    #endif
#endif

#ifndef MAXINT
#if INT_MAX
    #define MAXINT      INT_MAX
#else
    #define MAXINT      0x7fffffff
#endif
#endif

#ifndef MAXINT64
    #define MAXINT64    INT64(0x7fffffffffffffff)
#endif

#if SIZE_T_MAX
    #define MAXSIZE     SIZE_T_MAX
#elif BIT_64
    #define MAXSIZE     INT64(0xffffffffffffffff)
#else
    #define MAXSIZE     MAXINT
#endif

#if SSIZE_T_MAX
    #define MAXSSIZE     SSIZE_T_MAX
#elif BIT_64
    #define MAXSSIZE     INT64(0x7fffffffffffffff)
#else
    #define MAXSSIZE     MAXINT
#endif

#if OFF_T_MAX
    #define MAXOFF       OFF_T_MAX
#else
    #define MAXOFF       INT64(0x7fffffffffffffff)
#endif

/*
    Word size and conversions between integer and pointer.
 */
#if BIT_64
    #define ITOP(i)     ((void*) ((int64) i))
    #define PTOI(i)     ((int) ((int64) i))
    #define LTOP(i)     ((void*) ((int64) i))
    #define PTOL(i)     ((int64) i)
#else
    #define ITOP(i)     ((void*) ((int) i))
    #define PTOI(i)     ((int) i)
    #define LTOP(i)     ((void*) ((int) i))
    #define PTOL(i)     ((int64) (int) i)
#endif

#if BIT_WIN_LIKE
    #define INT64(x)    (x##i64)
    #define UINT64(x)   (x##Ui64)
    #define BIT_EXPORT  __declspec(dllexport)
#else
    #define INT64(x)    (x##LL)
    #define UINT64(x)   (x##ULL)
    #define BIT_EXPORT 
#endif

#ifndef max
    #define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
    #define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef PRINTF_ATTRIBUTE
    #if (__GNUC__ >= 3) && !DOXYGEN && BIT_DEBUG && UNUSED && KEEP
        /** 
            Use gcc attribute to check printf fns.  a1 is the 1-based index of the parameter containing the format, 
            and a2 the index of the first argument. Note that some gcc 2.x versions don't handle this properly 
         */     
        #define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
    #else
        #define PRINTF_ATTRIBUTE(a1, a2)
    #endif
#endif

/*
    Optimize expression evaluation code depending if the value is likely or not
 */
#undef likely
#undef unlikely
#if (__GNUC__ >= 3)
    #define likely(x)   __builtin_expect(!!(x), 1)
    #define unlikely(x) __builtin_expect(!!(x), 0)
#else
    #define likely(x)   (x)
    #define unlikely(x) (x)
#endif

#if !__UCLIBC__ && !CYGWIN && __USE_XOPEN2K
    #define BIT_HAS_SPINLOCK    1
#endif

#if BIT_HAS_DOUBLE_BRACES || MACOSX || LINUX
    #define  NULL_INIT    {{0}}
#else
    #define  NULL_INIT    {0}
#endif

/*********************************** Fixups ***********************************/

#ifndef R_OK
    #define R_OK    4
    #define W_OK    2
#if BIT_WIN_LIKE
    #define X_OK    R_OK
#else
    #define X_OK    1
#endif
    #define F_OK    0
#endif

#ifndef O_RDONLY
    #if ECOS
        #define     O_RDONLY  1
    #else
        #define     O_RDONLY  1
    #endif
#endif

#ifndef O_BINARY
    #if ECOS 
        #define O_BINARY 2
    #else
        #define O_BINARY 0
    #endif
#endif

#ifndef SOCKET_ERROR
    #define SOCKET_ERROR -1
#endif

#if MACSOX
    #define LD_LIBRARY_PATH "DYLD_LIBRARY_PATH"
#else
    #define LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#endif

#if VXWORKS
    /* Old VxWorks can't do array[] */
    #define BIT_FLEX 0
#else
    #define BIT_FLEX
#endif

#if WINDOWS
    #define getcwd  _getcwd
    #define tempnam _tempnam
    #define open    _open
    #define close   _close
    #define read    _read
    #define write   _write
    #define chdir   _chdir
    #define lseek   _lseek
    #define unlink  _unlink
    #define stat    _stat
#endif

/**
    Signed file offset data type. Supports large files greater than 4GB in size on all systems.
 */
typedef int64 EgFilePos;
typedef int64 EgDateTime;

#if VXWORKS
    typedef int GSockLenArg;
#else
    typedef socklen_t GSockLenArg;
#endif

#if DOXYGEN || WINDOWS
    typedef int socklen_t;
#endif

#if VXWORKS
    #define fcntl(a, b, c)
    #if _WRS_VXWORKS_MAJOR < 6
        #define NI_MAXHOST 128
        extern STATUS access(char *path, int mode);
        struct sockaddr_storage { char pad[1024]; };
    #endif
#endif /* VXWORKS */

#if ECOS
    #define     LIBKERN_INLINE          /* to avoid kernel inline functions */
    #if BIT_CGI
        #error "Ecos does not support CGI. Disable BIT_CGI"
    #endif
#endif /* ECOS */

#if BIT_WIN_LIKE
    #ifndef EWOULDBLOCK
        #define EWOULDBLOCK WSAEWOULDBLOCK
    #endif
    #ifndef ENETDOWN
        #define ENETDOWN    WSAENETDOWN
    #endif
    #ifndef ECONNRESET
        #define ECONNRESET  WSAECONNRESET
    #endif
#endif

#if LINUX && !defined(_STRUCT_TIMEVAL)
    struct timeval
    {
        time_t  tv_sec;     /* Seconds.  */
        time_t  tv_usec;    /* Microseconds.  */
    };
    #define _STRUCT_TIMEVAL 1
#endif

#if QNX
    typedef long fd_mask;
    #define NFDBITS (sizeof (fd_mask) * NBBY)   /* bits per mask */
#endif

#if MACOSX
    typedef int32_t fd_mask;
#endif
#if WINDOWS
    typedef fd_set fd_mask;
#endif

/*
    Copyright. The software license requires that this not be modified or removed.
 */
#define EMBEDTHIS_GOAHEAD_COPYRIGHT T(\
    "Copyright (c) Embedthis Software Inc., 1993-2012. All Rights Reserved." \
    "Copyright (c) GoAhead Software Inc., 2012. All Rights Reserved." \
    )

/************************************ Unicode *********************************/
#if UNICODE
    #if !BIT_WIN_LIKE
        #error "Unicode only supported on Windows or Windows CE"
    #endif
    typedef ushort char_t;
    typedef ushort uchar_t;

    /*
        To convert strings to UNICODE. We have a level of indirection so things like T(__FILE__) will expand properly.
     */
    #define T(x)      __TXT(x)
    #define __TXT(s)  L ## s

    /*
        Text size of buffer macro. A buffer bytes will hold (size / char size) characters. 
     */
    #define TSZ(x) (sizeof(x) / sizeof(char_t))

    #define gaccess     _waccess
    #define gasctime    _wasctime
    #define gatoi(s)    wcstol(s, NULL, 10)
    #define gchdir      _wchdir
    #define gchmod      _wchmod
    #define gclose      close
    #define gcreat      _wcreat
    #define gctime      _wctime
    #define gexecvp     _wexecvp
    #define gfgets      fgetws
    #define gfindclose  _findclose
    #define gfinddata_t _wfinddata_t
    #define gfindfirst  _wfindfirst
    #define gfindnext   _wfindnext
    #define gfopen      _wfopen
    #define gfprintf    fwprintf
    #define gfputs      fputws
    #define gfscanf     fwscanf
    #define ggetcwd     _wgetcwd
    #define ggetenv     _wgetenv
    #define ggets       _getws
    #define gisalnum    iswalnum
    #define gisalpha    iswalpha
    #define glseek      lseek
    #define gmkdir      _wmkdir
    #define gprintf     wprintf
    #define gread       read
    #define grename     _wrename
    #define grmdir      _wrmdir
    #define gsprintf    swprintf
    #define gsscanf     swscanf
    #define gstat       _wstat
    #define gstrcat     wcscat
    #define gstrchr     wcschr
    #define gstrcmp     wcscmp
    #define gstrcpy     wcscpy
    #define gstrcspn    wcscspn
    #define gstricmp    wcsicmp
    #define gstrlen     wcslen
    #define gstrncat    wcsncat
    #define gstrncmp    wcsncmp
    #define gstrncpy    wcsncpy
    #define gstrnset    wcsnset
    #define gstrrchr    wcsrchr
    #define gstrspn     wcsspn
    #define gstrstr     wcsstr
    #define gstrtok     wcstok
    #define gstrtol     wcstol
    #define gtempnam    _wtempnam
    #define gtmpnam     _wtmpnam
    #define gtolower    towlower
    #define gtoupper    towupper
    #define gunlink     _wunlink
    #define gvsprintf   vswprintf
    #define gwrite      write

    #if CE
        #define gisspace    isspace
        #define gisdigit    isdigit
        #define gisxdigit   isxdigit
        #define gisupper    isupper
        #define gislower    islower
        #define gisprint    isprint
    #else
        #define gremove     _wremove
        #define gisspace    iswspace
        #define gisdigit    iswdigit
        #define gisxdigit   iswxdigit
        #define gisupper    iswupper
        #define gislower    iswlower
    #endif  /* CE */
    
    typedef struct _stat GStat;
    
#else /* !UNICODE */

    #define T(s)        s
    #define TSZ(x)      (sizeof(x))
    typedef char        char_t;
    #if WINDOWS
        typedef uchar   uchar_t;
    #endif
    #if VXWORKS
        #define gchdir      vxchdir
        #define gmkdir      vxmkdir
        #define grmdir      vxrmdir
    #elif BIT_UNIX_LIKE
        #define gchdir      chdir
        #define gmkdir(s)   mkdir(s,0755)
        #define grmdir      rmdir
    #else
        #define gchdir      chdir
        #define gmkdir      mkdir
        #define grmdir      rmdir
    #endif
    #define gaccess     access
    #define gasctime    asctime
    #define gatoi       atoi
    #define gchmod      chmod
    #define gclose      close
    #define gclosedir   closedir
    #define gcreat      creat
    #define gctime      ctime
    #define gexecvp     execvp
    #define gfgets      fgets
    #define gfindclose  _findclose
    #define gfinddata_t _finddata_t
    #define gfindfirst  _findfirst
    #define gfindnext   _findnext
    #define gfopen      fopen
    #define gfprintf    fprintf
    #define gfputs      fputs
    #define gfscanf     fscanf
    #define ggetcwd     getcwd
    #define ggetenv     getenv
    #define ggets       gets
    #define gisalnum    isalnum
    #define gisalpha    isalpha
    #define gisdigit    isdigit
    #define gislower    islower
    #define gisspace    isspace
    #define gisupper    isupper
    #define gisxdigit   isxdigit
    #define gloadModule loadModule
    #define glseek      lseek
    #define gopendir    opendir
    #define gprintf     printf
    #define gread       read
    #define greaddir    readdir
    #define gremove     remove
    #define grename     rename
    #define gsprintf    sprintf
    #define gsscanf     sscanf
    #define gstat       stat
    #define gstrcat     strcat
    #define gstrchr     strchr
    #define gstrcmp     strcmp
    #define gstrcpy     strcpy
    #define gstrcspn    strcspn
    #define gstricmp    strcmpci
    #define gstrlen     strlen
    #define gstrncat    strncat
    #define gstrncmp    strncmp
    #define gstrncpy    strncpy
    #define gstrnset    strnset
    #define gstrrchr    strrchr
    #define gstrspn     strspn
    #define gstrstr     strstr
    #define gstrtok     strtok
    #define gstrtol     strtol
    #define gtempnam    tempnam
    #define gtmpnam     tmpnam
    #define gtolower    tolower
    #define gtoupper    toupper
    #define gunlink     unlink
    #define gvsprintf   vsprintf
    #define gwrite      write
    typedef struct stat GStat;
#endif /* !UNICODE */

extern char_t *gAscToUni(char_t *ubuf, char *str, ssize nBytes);
extern char *gUniToAsc(char *buf, char_t *ustr, ssize nBytes);

#if CE
extern int gwriteUniToAsc(int fid, void *buf, ssize len);
extern int greadAscToUni(int fid, void **buf, ssize len);
#endif

#if !LINUX
    extern char_t *basename(char_t *name);
#endif /* !LINUX */

/************************************* Main ***********************************/

#define BIT_MAX_ARGC 32
#if VXWORKS
    #define MAIN(name, _argc, _argv, _envp)  \
        static int innerMain(int argc, char **argv, char **envp); \
        int name(char *arg0, ...) { \
            va_list args; \
            char *argp, *largv[BIT_MAX_ARGC]; \
            int largc = 0; \
            va_start(args, arg0); \
            largv[largc++] = #name; \
            if (arg0) { \
                largv[largc++] = arg0; \
            } \
            for (argp = va_arg(args, char*); argp && largc < BIT_MAX_ARGC; argp = va_arg(args, char*)) { \
                largv[largc++] = argp; \
            } \
            return innerMain(largc, largv, NULL); \
        } \
        static int innerMain(_argc, _argv, _envp)
#elif BIT_WIN_LIKE && UNICODE
    #define MAIN(name, _argc, _argv, _envp)  \
        APIENTRY WinMain(HINSTANCE inst, HINSTANCE junk, LPWSTR command, int junk2) { \
            char *largv[BIT_MAX_ARGC]; \
            extern int main(); \
            char *mcommand[VALUE_MAX_STRING]; \
            int largc; \
            wtom(mcommand, sizeof(dest), command, -1);
            largc = egParseArgs(mcommand, &largv[1], BIT_MAX_ARGC - 1); \
            largv[0] = #name; \
            gsetAppInstance(inst); \
            main(largc, largv, NULL); \
        } \
        int main(argc, argv, _envp)
#elif BIT_WIN_LIKE
    #define MAIN(name, _argc, _argv, _envp)  \
        APIENTRY WinMain(HINSTANCE inst, HINSTANCE junk, char *command, int junk2) { \
            extern int main(); \
            char *largv[BIT_MAX_ARGC]; \
            int largc; \
            largc = egParseArgs(command, &largv[1], BIT_MAX_ARGC - 1); \
            largv[0] = #name; \
            main(largc, largv, NULL); \
        } \
        int main(_argc, _argv, _envp)
#else
    #define MAIN(name, _argc, _argv, _envp) int main(_argc, _argv, _envp)
#endif

extern int egParseArgs(char *args, char **argv, int maxArgc);

#if WINDOWS
    extern void egSetInst(HINSTANCE inst);
    extern HINSTANCE egGetInst();
#endif

/************************************ Tunables ********************************/

#define G_SMALL_HASH          31          /* General small hash size */

/************************************* Error **********************************/

#define G_L                 T(__FILE__), __LINE__
#define G_ARGS_DEC          char_t *file, int line
#define G_ARGS              file, line

#if BIT_DEBUG && BIT_DEBUG_LOG
    #define gassert(C)     if (C) ; else gassertError(G_L, T("%s"), T(#C))
    extern void gassertError(G_ARGS_DEC, char_t *fmt, ...);
#else
    #define gassert(C)     if (1) ; else
#endif

#if BIT_DEBUG_LOG
    #define LOG trace
    extern void traceRaw(char_t *buf);
    extern int traceOpen();
    extern void traceClose();
    typedef void (*TraceHandler)(int level, char_t *msg);
    extern TraceHandler traceSetHandler(TraceHandler handler);
    extern void traceSetPath(char_t *path);
#else
    #define TRACE if (0) trace
    #define ERROR if (0) error
    #define traceOpen() 
    #define traceClose()
#endif

extern void error(char_t *fmt, ...);
extern void trace(int lev, char_t *fmt, ...);

/************************************* Value **********************************/
/*
    These values are not prefixed so as to aid code readability
 */
typedef enum {
    undefined   = 0,
    byteint     = 1,
    shortint    = 2,
    integer     = 3,
    hex         = 4,
    percent     = 5,
    octal       = 6,
    big         = 7,
    flag        = 8,
    floating    = 9,
    string      = 10,
    bytes       = 11,
    symbol      = 12,
    errmsg      = 13
} vtype_t;

typedef struct {
    union {
        char    flag;
        char    byteint;
        short   shortint;
        char    percent;
        long    integer;
        long    hex;
        long    octal;
        long    big[2];
#if BIT_FLOAT
        double  floating;
#endif
        char_t  *string;
        char    *bytes;
        char_t  *errmsg;
        void    *symbol;
    } value;

    vtype_t     type;
    uint        valid       : 8;
    uint        allocated   : 8;        /* String was allocated */
} value_t;

#define value_numeric(t)    (t >= byteint && t <= big)
#define value_str(t)        (t >= string && t <= bytes)
#define value_ok(t)         (t > undefined && t <= symbol)

#define VALUE_ALLOCATE      0x1
#define VALUE_VALID         { {0}, integer, 1 }
#define VALUE_INVALID       { {0}, undefined, 0 }

extern value_t valueInteger(long value);
extern value_t valueString(char_t *value, int flags);
extern value_t valueSymbol(void *value);
extern value_t valueErrmsg(char_t *value);
extern void valueFree(value_t *v);

/************************************* Ringq **********************************/
/*
    A ring queue allows maximum utilization of memory for data storage and is
    ideal for input/output buffering. This module provides a highly effecient
    implementation and a vehicle for dynamic strings.
  
    WARNING:  This is a public implementation and callers have full access to
    the queue structure and pointers.  Change this module very carefully.
  
    This module follows the open/close model.
  
    Operation of a ringq where rq is a pointer to a ringq :
  
        rq->buflen contains the size of the buffer.
        rq->buf will point to the start of the buffer.
        rq->servp will point to the first (un-consumed) data byte.
        rq->endp will point to the next free location to which new data is added
        rq->endbuf will point to one past the end of the buffer.
  
    Eg. If the ringq contains the data "abcdef", it might look like :
  
    +-------------------------------------------------------------------+
    |   |   |   |   |   |   |   | a | b | c | d | e | f |   |   |   |   |
    +-------------------------------------------------------------------+
      ^                           ^                       ^               ^
      |                           |                       |               |
    rq->buf                    rq->servp               rq->endp      rq->enduf
       
    The queue is empty when servp == endp.  This means that the queue will hold
    at most rq->buflen -1 bytes.  It is the fillers responsibility to ensure
    the ringq is never filled such that servp == endp.
  
    It is the fillers responsibility to "wrap" the endp back to point to
    rq->buf when the pointer steps past the end. Correspondingly it is the
    consumers responsibility to "wrap" the servp when it steps to rq->endbuf.
    The ringqPutc and ringqGetc routines will do this automatically.
 */

typedef struct {
    uchar   *buf;               /* Holding buffer for data */
    uchar   *servp;             /* Pointer to start of data */
    uchar   *endp;              /* Pointer to end of data */
    uchar   *endbuf;            /* Pointer to end of buffer */
    ssize   buflen;             /* Length of ring queue */
    ssize   maxsize;            /* Maximum size */
    int     increment;          /* Growth increment */
} ringq_t;

extern int ringqOpen(ringq_t *rq, int increment, int maxsize);
extern void ringqClose(ringq_t *rq);
extern ssize ringqLen(ringq_t *rq);
extern int ringqPutc(ringq_t *rq, char_t c);
extern int ringqInsertc(ringq_t *rq, char_t c);
extern ssize ringqPutStr(ringq_t *rq, char_t *str);
extern int ringqGetc(ringq_t *rq);

#if UNICODE || DOXYGEN
    extern int ringqPutcA(ringq_t *rq, char c);
    extern int ringqInsertcA(ringq_t *rq, char c);
    extern int ringqPutStrA(ringq_t *rq, char *str);
    extern int ringqGetcA(ringq_t *rq);
#else
    #define ringqPutcA ringqPutc
    #define ringqInsertcA ringqInsertc
    #define ringqPutStrA ringqPutStr
    #define ringqGetcA ringqGetc
#endif /* UNICODE */

extern ssize ringqPutBlk(ringq_t *rq, uchar *buf, ssize len);
extern ssize ringqPutBlkMax(ringq_t *rq);
extern void ringqPutBlkAdj(ringq_t *rq, ssize size);
extern ssize ringqGetBlk(ringq_t *rq, uchar *buf, ssize len);
extern ssize ringqGetBlkMax(ringq_t *rq);
extern void ringqGetBlkAdj(ringq_t *rq, ssize size);
extern void ringqFlush(ringq_t *rq);
extern void ringqAddNull(ringq_t *rq);

/******************************* Malloc Replacement ***************************/
/*
    Block classes are: 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 
 */
typedef struct {
    union {
        void    *next;                          /* Pointer to next in q */
        int     size;                           /* Actual requested size */
    } u;
    int         flags;                          /* Per block allocation flags */
} gType;

#define G_SHIFT         4                   /* Convert size to class */
#define G_ROUND         ((1 << (B_SHIFT)) - 1)
#define G_MAX_CLASS     13                  /* Maximum class number + 1 */
#define G_MALLOCED      0x80000000          /* Block was malloced */
#define G_DEFAULT_MEM   (64 * 1024)         /* Default memory allocation */
#define G_MAX_FILES     (512)               /* Maximum number of files */
#define G_FILL_CHAR     (0x77)              /* Fill byte for buffers */
#define G_FILL_WORD     (0x77777777)        /* Fill word for buffers */
#define B_MAX_BLOCKS    (64 * 1024)         /* Maximum allocated blocks */

/*
    Flags. The integrity value is used as an arbitrary value to fill the flags.
 */
#define G_INTEGRITY         0x8124000       /* Integrity value */
#define G_INTEGRITY_MASK    0xFFFF000       /* Integrity mask */
#define G_USE_MALLOC        0x1             /* Okay to use malloc if required */
#define G_USER_BUF          0x2             /* User supplied buffer for mem */

extern void gcloseAlloc();
extern int  gopenAlloc(void *buf, int bufsize, int flags);

#if !BIT_REPLACE_MALLOC
    extern char_t *gstrdupNoAlloc(char_t *s);
    extern char *gstrdupANoAlloc(char *s);
    #define galloc(num) malloc(num)
    #define gfree(p) if (p) { free(p); } else
    #define grealloc(p, num) realloc(p, num)
    #define gstrdup(s) gstrdupNoAlloc(s)
    #define gstrdupA(s) gstrdupANoAlloc(s)
    #define gstrdup(s) gstrdupNoAlloc(s)

#else /* BIT_REPLACE_MALLOC */
    #if UNICODE
        extern char *gstrdupA(char *s);
        #define gstrdupA(p) gstrdupA(p)
    #else
        #define gstrdupA gstrdup
    #endif
    extern void *galloc(ssize size);
    extern void gfree(void *mp);
    extern void *grealloc(void *buf, ssize newsize);
    extern char_t *gstrdup(char_t *s);
#endif /* BIT_REPLACE_MALLOC */

extern char_t *gallocAscToUni(char  *cp, ssize alen);
extern char *gallocUniToAsc(char_t *unip, ssize ulen);

/******************************* Symbol Table *********************************/
/*
    The symbol table record for each symbol entry
 */
typedef struct sym_t {
    struct sym_t    *forw;                  /* Pointer to next hash list */
    value_t         name;                   /* Name of symbol */
    value_t         content;                /* Value of symbol */
    int             arg;                    /* Parameter value */
    int             bucket;                 /* Bucket index */
} sym_t;

typedef int sym_fd_t;                       /* Returned by symOpen */

extern sym_fd_t symOpen(int hash_size);
extern void     symClose(sym_fd_t sd);
extern sym_t    *symLookup(sym_fd_t sd, char_t *name);
extern sym_t    *symEnter(sym_fd_t sd, char_t *name, value_t v, int arg);
extern int      symDelete(sym_fd_t sd, char_t *name);
extern void     symWalk(sym_fd_t sd, void (*fn)(sym_t *symp));
extern sym_t    *symFirst(sym_fd_t sd);
extern sym_t    *symNext(sym_fd_t sd, sym_t *last);
extern int      symSubOpen();
extern void     symSubClose();

/************************************ Socket **********************************/
/*
    Socket flags 
 */
#define SOCKET_EOF              0x1     /* Seen end of file */
#define SOCKET_CONNECTING       0x2     /* Connect in progress */
#define SOCKET_PENDING          0x4     /* Message pending on this socket */
#define SOCKET_FLUSHING         0x8     /* Background flushing */
#define SOCKET_ASYNC            0x10    /* Use async connect */
#define SOCKET_BLOCK            0x20    /* Use blocking I/O */
#define SOCKET_LISTENING        0x40    /* Socket is server listener */
#define SOCKET_CLOSING          0x80    /* Socket is closing */
#define SOCKET_CONNRESET        0x100   /* Socket connection was reset */
#define SOCKET_OWN_BUFFERS      0x200   /* Not using inBuf/outBuf ringq */

#define SOCKET_PORT_MAX         0xffff  /* Max Port size */

/*
    Socket error values
 */
#define SOCKET_WOULDBLOCK       1       /* Socket would block on I/O */
#define SOCKET_RESET            2       /* Socket has been reset */
#define SOCKET_NETDOWN          3       /* Network is down */
#define SOCKET_AGAIN            4       /* Issue the request again */
#define SOCKET_INTR             5       /* Call was interrupted */
#define SOCKET_INVAL            6       /* Invalid */

/*
    Handler event masks
 */
#define SOCKET_READABLE         0x2     /* Make socket readable */ 
#define SOCKET_WRITABLE         0x4     /* Make socket writable */
#define SOCKET_EXCEPTION        0x8     /* Interested in exceptions */

typedef void    (*socketHandler_t)(int sid, int mask, void* data);
typedef int     (*socketAccept_t)(int sid, char *ipaddr, int port, int listenSid);

typedef struct {
    ringq_t         inBuf;                  /* Input ring queue */
    ringq_t         outBuf;                 /* Output ring queue */
    ringq_t         lineBuf;                /* Line ring queue */
    socketAccept_t  accept;                 /* Accept handler */
    socketHandler_t handler;                /* User I/O handler */
    char            *ip;                    /* Server listen address or remote client address */
    void            *handler_data;          /* User handler data */
    int             handlerMask;            /* Handler events of interest */
    int             sid;                    /* Index into socket[] */
    int             port;                   /* Port to listen on */
    int             flags;                  /* Current state flags */
    int             sock;                   /* Actual socket handle */
    int             fileHandle;             /* ID of the file handler */
    int             interestEvents;         /* Mask of events to watch for */
    int             currentEvents;          /* Mask of ready events (FD_xx) */
    int             selectEvents;           /* Events being selected */
    int             saveMask;               /* saved Mask for socketFlush */
    int             error;                  /* Last error */
#if BIT_PACK_SSL
    int             secure;                 /* Socket is using SSL */
#endif
} socket_t;

extern socket_t     **socketList;           /* List of open sockets */

extern int      socketAddress(struct sockaddr *addr, int addrlen, char *ip, int ipLen, int *port);
extern bool     socketAddressIsV6(char_t *ip);
extern void     socketClose();
extern void     socketCloseConnection(int sid);
extern int      socketConnect(char *host, int port, int flags);
extern void     socketCreateHandler(int sid, int mask, socketHandler_t handler, void* arg);
extern void     socketDeleteHandler(int sid);
extern int      socketEof(int sid);
extern ssize    socketCanWrite(int sid);
extern void     socketSetBufferSize(int sid, int in, int line, int out);
extern int      socketFlush(int sid);
extern ssize    socketGets(int sid, char_t **buf);
extern int      socketGetPort(int sid);
extern int      socketInfo(char_t *ip, int port, int *family, int *protocol, struct sockaddr_storage *addr, 
                    GSockLenArg *addrlen);
extern ssize    socketInputBuffered(int sid);
extern bool     socketIsV6(int sid);
extern int      socketOpen();
extern int      socketListen(char *host, int port, socketAccept_t accept, int flags);
extern int      socketParseAddress(char_t *ipAddrPort, char_t **pip, int *pport, int defaultPort);
extern void     socketProcess(int hid);
extern ssize    socketRead(int sid, char *buf, ssize len);
extern int      socketReady(int hid);
extern ssize    socketWrite(int sid, char *buf, ssize len);
extern ssize    socketWriteString(int sid, char_t *buf);
extern int      socketSelect(int hid, int timeout);
extern int      socketGetHandle(int sid);
extern int      socketSetBlock(int sid, int flags);
extern int      socketGetBlock(int sid);
extern int      socketAlloc(char *host, int port, socketAccept_t accept, int flags);
extern void     socketFree(int sid);
extern int      socketGetError();
extern socket_t *socketPtr(int sid);
extern int      socketWaitForEvent(socket_t *sp, int events, int *errCode);
extern void     socketRegisterInterest(socket_t *sp, int handlerMask);
extern ssize    socketGetInput(int sid, char *buf, ssize toRead, int *errCode);

/*********************************** Runtime **********************************/

extern int gallocHandle(void ***map);
extern int gallocEntry(void ***list, int *max, int size);
extern int gfreeHandle(void ***map, int handle);
extern int gcaselesscmp(char_t *s1, char_t *s2);
extern bool gcaselessmatch(char_t *s1, char_t *s2);
extern int gcmp(char_t *s1, char_t *s2);
extern ssize gfmtAlloc(char_t **s, ssize n, char_t *fmt, ...);
extern ssize gfmtStatic(char_t *s, ssize n, char_t *fmt, ...);
extern ssize gfmtValloc(char_t **s, ssize n, char_t *fmt, va_list arg);
extern ssize glen(char_t *s1);
extern bool gmatch(char_t *s1, char_t *s2);
extern int gopen(char_t *path, int oflag, int mode);
extern int gncaselesscmp(char_t *s1, char_t *s2, ssize n);
extern int gncmp(char_t *s1, char_t *s2, ssize n);
extern char_t *gstrlower(char_t *string);
extern char_t *gstrupper(char_t *string);
extern char_t *gstritoa(int n, char_t *string, int width);
extern uint gstrtoi(char_t *s);
extern char_t *gtok(char_t *str, char_t *delim, char_t **last);

extern uint hextoi(char_t *hexstring);

//  MOB NAMING (cap first char after 'g')
typedef void (GSchedProc)(void *data, int id);
extern int gSchedCallback(int delay, GSchedProc *proc, void *arg);
extern void gUnschedCallback(int id);
extern void gReschedCallback(int id, int delay);
extern void gSchedProcess();

/********************************** Defines ***********************************/

#define MAX_PORT_LEN            10          /* max digits in port number */
#define WEBS_SYM_INIT           64          /* Hash size for form table */
#define WEBS_SESSION_HASH       31          /* Hash size for session stores */
#define WEBS_SESSION_PRUNE      60          /* Prune sessions every minute */
#define WEBS_SESSION            "-goahead-session-"

/* 
    Request flags
 */
#define WEBS_LOCAL_PAGE         0x1         /* Request for local webs page */ 
#define WEBS_KEEP_ALIVE         0x2         /* HTTP/1.1 keep alive */
#define WEBS_COOKIE             0x8         /* Cookie supplied in request */
#define WEBS_IF_MODIFIED        0x10        /* If-modified-since in request */
#define WEBS_POST_REQUEST       0x20        /* Post request operation */
#define WEBS_LOCAL_REQUEST      0x40        /* Request from this system */
#define WEBS_HOME_PAGE          0x80        /* Request for the home page */ 
#define WEBS_JS                 0x100       /* Javscript request */ 
#define WEBS_HEAD_REQUEST       0x200       /* Head request */
#define WEBS_CLEN               0x400       /* Request had a content length */
#define WEBS_FORM               0x800       /* Request is a form */
#define WEBS_POST_DATA          0x2000      /* Already appended post data */
#define WEBS_CGI_REQUEST        0x4000      /* cgi-bin request */
#define WEBS_SECURE             0x8000      /* connection uses SSL */
#define WEBS_BASIC_AUTH         0x10000     /* Basic authentication request */
#define WEBS_DIGEST_AUTH        0x20000     /* Digest authentication request */
#define WEBS_HEADER_DONE        0x40000     /* Already output the HTTP header */
#define WEBS_HTTP11             0x80000     /* Request is using HTTP/1.1 */
#define WEBS_RESPONSE_TRACED    0x100000    /* Started tracing the response */

/*
    URL handler flags
 */
#define WEBS_HANDLER_FIRST  0x1         /* Process this handler first */
#define WEBS_HANDLER_LAST   0x2         /* Process this handler last */

/* 
    Read handler flags and state
 */
#define WEBS_BEGIN          0x1         /* Beginning state */
#define WEBS_HEADER         0x2         /* Ready to read first line */
#define WEBS_POST           0x4         /* POST without content */
#define WEBS_POST_CLEN      0x8         /* Ready to read content for POST */
#define WEBS_PROCESSING     0x10        /* Processing request */
#define WEBS_KEEP_TIMEOUT   15000       /* Keep-alive timeout (15 secs) */
#define WEBS_TIMEOUT        60000       /* General request timeout (60) */

/* Forward declare */
#if BIT_AUTH
struct User;
struct Uri;
#endif

#if BIT_SESSIONS
struct Session;
#endif

/* 
    Per socket connection webs structure
 */
typedef struct websRec {
    ringq_t         header;             /* Header dynamic string */
    time_t          since;              /* Parsed if-modified-since time */
    sym_fd_t        cgiVars;            /* CGI standard variables */
    sym_fd_t        cgiQuery;           /* CGI decoded query string */
    time_t          timestamp;          /* Last transaction with browser */
    int             timeout;            /* Timeout handle */
    char_t          ipaddr[32];         /* Connecting ipaddress */
    char_t          ifaddr[32];         /* Local interface ipaddress */
    char_t          type[64];           /* Mime type */
    char_t          *authType;          /* Authorization type (Basic/DAA) */
    char_t          *authDetails;       /* Http header auth details */
    char_t          *dir;               /* Directory containing the page */
    char_t          *path;              /* Path name without query */
    char_t          *url;               /* Full request url */
    char_t          *host;              /* Requested host */
    char_t          *lpath;             /* Document path name */
    char_t          *query;             /* Request query */
    char_t          *decodedQuery;      /* Decoded request query */
    char_t          *method;            /* HTTP request method */
    char_t          *password;          /* Authorization password */
    char_t          *userName;          /* Authorization username */
    char_t          *cookie;            /* Request cookie string */
    char_t          *responseCookie;    /* Outgoing cookie */
    char_t          *authResponse;      /* Outgoing auth header */
    char_t          *userAgent;         /* User agent (browser) */
    char_t          *protocol;          /* Protocol (normally HTTP) */
    char_t          *protoVersion;      /* Protocol version */
    int             sid;                /* Socket id (handler) */
    int             listenSid;          /* Listen Socket id */
    int             port;               /* Request port number */
    int             state;              /* Current state */
    int             flags;              /* Current flags -- see above */
    int             code;               /* Response status code */
    int             clen;               /* Content length */
    int             wid;                /* Index into webs */
    char_t          *cgiStdin;          /* filename for CGI stdin */
    int             docfd;              /* Document file descriptor */
    ssize           numbytes;           /* Bytes to transfer to browser */
    ssize           written;            /* Bytes actually transferred */
    void            (*writeSocket)(struct websRec *wp);
#if BIT_SESSIONS
    struct Session  *session;           /* Session record */
#endif
#if BIT_AUTH
    struct User     *user;              /* User auth record */
    //  MOB - rename. Uri is confusing with appweb
    struct Uri      *uri;               /* Uri auth record */
#if BIT_DIGEST_AUTH
    char_t          *realm;             /* Realm field supplied in auth header */
    char_t          *nonce;             /* opaque-to-client string sent by server */
    char_t          *digestUri;         /* URI found in digest header */
    char_t          *opaque;            /* opaque value passed from server */
    char_t          *nc;                /* nonce count */
    char_t          *cnonce;            /* check nonce */
    char_t          *qop;               /* quality operator */
#endif
#endif
#if BIT_PACK_OPENSSL
    SSL             *ssl;
    BIO             *bio;
#elif BIT_PACK_MATRIXSSL
    sslConn_t       *sslConn;
#endif
} websRec;

typedef websRec *webs_t;
typedef websRec websType;

/*
    URL handler structure. Stores the leading URL path and the handler function to call when the URL path is seen.
 */ 
typedef struct {
    int     (*handler)(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, 
            char_t *query);                 /* Callback URL handler function */
    char_t  *webDir;                        /* Web directory if required */
    char_t  *urlPrefix;                     /* URL leading prefix */
    ssize   len;                            /* Length of urlPrefix for speed */
    int     arg;                            /* Argument to provide to handler */
    int     flags;                          /* Flags */
} websUrlHandlerType;

/* 
    Webs statistics
    MOB - remove these?
 */
typedef struct {
    long            errors;                 /* General errors */
    long            redirects;
    long            net_requests;
    long            activeNetRequests;
    long            activeBrowserRequests;
    long            timeouts;
    long            access;                 /* Access violations */
    long            localHits;
    long            remoteHits;
    long            formHits;
    long            cgiHits;
    long            handlerHits;
} websStatsType;

extern websStatsType websStats;             /* Web access stats */

/* 
    Error code list
 */
typedef struct {
    int     code;                           /* HTTP error code */
    char_t  *msg;                           /* HTTP error message */
} websErrorType;

/* 
    Mime type list
 */
typedef struct {
    char_t  *type;                          /* Mime type */
    char_t  *ext;                           /* File extension */
} websMimeType;

/*
    File information structure.
 */
typedef struct {
    unsigned long   size;                   /* File length */
    int             isDir;                  /* Set if directory */
    time_t          mtime;                  /* Modified time */
} websStatType;

/*
    Compiled Rom Page Index
 */
typedef struct {
    char_t          *path;                  /* Web page URL path */
    uchar           *page;                  /* Web page data */
    int             size;                   /* Size of web page in bytes */
    int             pos;                    /* Current read position */
} websRomPageIndexType;


/*
    Globals
 */
extern websRomPageIndexType websRomPageIndex[];
extern websMimeType     websMimeList[];     /* List of mime types */
extern sym_fd_t         websMime;           /* Set of mime types */
extern webs_t           *webs;              /* Session list head */
extern int              websMax;            /* List size */
extern char_t           websHost[64];       /* Name of this host */
extern char_t           websIpaddr[64];     /* IP address of this host */
extern char_t           *websHostUrl;       /* URL for this host */
extern char_t           *websIpaddrUrl;     /* URL for this host */
extern int              websPort;           /* Port number */
extern int              websDebug;          /* Run in debug mode */

/**
    Decode base 64 blocks up to a NULL or equals
 */
#define WEBS_DECODE_TOKEQ 1

extern int websAccept(int sid, char *ipaddr, int port, int listenSid);
extern int websAlloc(int sid);
extern void websAspClose();
extern int websAspDefine(char_t *name, int (*fn)(int ejid, webs_t wp, int argc, char_t **argv));
extern int websAspOpen();
extern int websAspRequest(webs_t wp, char_t *lpath);
extern char_t *websCalcNonce(webs_t wp);
extern char_t *websCalcOpaque(webs_t wp);
extern char_t *websCalcDigest(webs_t wp);
extern char_t *websCalcUrlDigest(webs_t wp);
extern int websCgiHandler(webs_t wp, char_t *urlPrefix, char_t *dir, int arg, char_t *url, char_t *path, char_t *query);
extern void websCgiCleanup();
extern int websCheckCgiProc(int handle);
extern void websClose();
extern void websCloseListen();
extern void websCloseServer();
extern char_t *websDecode64(char_t *string);
extern char_t *websDecode64Block(char_t *s, ssize *len, int flags);
extern void websDecodeUrl(char_t *token, char_t *decoded, ssize len);
extern void websDefaultOpen();
extern void websDefaultClose();
extern int websDefaultHandler(webs_t wp, char_t *urlPrefix, char_t *dir, int arg, char_t *url, char_t *path, char_t *query);
extern int websDefaultHomePageHandler(webs_t wp, char_t *urlPrefix, char_t *dir, int arg, char_t *url, char_t *path, char_t *query);
extern void websDone(webs_t wp, int code);
extern char_t *websEncode64(char_t *string);
extern char_t *websEncode64Block(char_t *s, ssize len);
extern char *websEscapeHtml(cchar *html);
extern void websError(webs_t wp, int code, char_t *msg, ...);
extern char_t *websErrorMsg(int code);
extern int websEval(char_t *cmd, char_t **rslt, void* chan);
extern void websFooter(webs_t wp);
extern void websFormClose();
extern int websFormDefine(char_t *name, void (*fn)(webs_t wp, char_t *path, char_t *query));
extern int websFormHandler(webs_t wp, char_t *urlPrefix, char_t *dir, int arg, char_t *url, char_t *path, char_t *query);
extern void websFormOpen();
extern void websFree(webs_t wp);
extern char_t *websGetCgiCommName();
extern char_t *websGetDateString(websStatType* sbuf);
extern char_t *websGetDefaultDir();
extern char_t *websGetDefaultPage();
extern char_t *websGetHostUrl();
extern char_t *websGetIpaddrUrl();
extern char_t *websGetPassword();
extern int websGetPort();
extern char_t *websGetPublishDir(char_t *path, char_t **urlPrefix);
extern char_t *websGetRealm();
extern ssize websGetRequestBytes(webs_t wp);
extern char_t *websGetRequestDir(webs_t wp);
extern int websGetRequestFlags(webs_t wp);
extern char_t *websGetRequestIpaddr(webs_t wp);
extern char_t *websGetRequestLpath(webs_t wp);
extern char_t *websGetRequestPath(webs_t wp);
extern char_t *websGetRequestPassword(webs_t wp);
extern char_t *websGetRequestType(webs_t wp);
extern ssize websGetRequestWritten(webs_t wp);
extern char_t *websGetVar(webs_t wp, char_t *var, char_t *def);
extern int websCompareVar(webs_t wp, char_t *var, char_t *value);
extern void websHeader(webs_t wp);
extern int websLaunchCgiProc(char_t *cgiPath, char_t **argp, char_t **envp, char_t *stdIn, char_t *stdOut);
extern char *websMD5(char_t *s);
extern char *websMD5binary(char_t *buf, ssize length, char_t *prefix);
extern char_t *websNormalizeUriPath(char_t *path);
extern int websOpen();
extern int websOpenListen(char *ip, int port, int sslPort);
extern int websOpenServer(char *ip, int port, int sslPort, char_t *documents);
extern void websPageClose(webs_t wp);
extern int websPageIsDirectory(char_t *lpath);
extern int websPageOpen(webs_t wp, char_t *lpath, char_t *path, int mode, int perm);
extern ssize websPageReadData(webs_t wp, char *buf, ssize nBytes);
extern void websPageSeek(webs_t wp, EgFilePos offset);
extern int websPageStat(webs_t wp, char_t *lpath, char_t *path, websStatType *sbuf);
extern int websPublish(char_t *urlPrefix, char_t *path);
extern void websReadEvent(webs_t wp);
extern void websRedirect(webs_t wp, char_t *url);
extern void websResponse(webs_t wp, int code, char_t *msg, char_t *redirect);
extern void websRewriteRequest(webs_t wp, char_t *url);
extern int websRomOpen();
extern void websRomClose();
extern int websRomPageOpen(webs_t wp, char_t *path, int mode, int perm);
extern void websRomPageClose(int fd);
extern ssize websRomPageReadData(webs_t wp, char *buf, ssize len);
extern int websRomPageStat(char_t *path, websStatType *sbuf);
extern long websRomPageSeek(webs_t wp, EgFilePos offset, int origin);
extern void websSetDefaultDir(char_t *dir);
extern void websSetDefaultPage(char_t *page);
extern void websSetEnv(webs_t wp);
extern void websSetHost(char_t *host);
extern void websSetIpaddr(char_t *ipaddr);
extern void websSetPassword(char_t *password);
extern void websSetRequestBytes(webs_t wp, ssize bytes);
extern void websSetRequestLpath(webs_t wp, char_t *lpath);
extern void websSetRequestPath(webs_t wp, char_t *dir, char_t *path);
extern void websSetRequestSocketHandler(webs_t wp, int mask, void (*fn)(webs_t wp));
extern char_t *websGetRequestUserName(webs_t wp);
extern void websServiceEvents(int *finished);
extern void websSetRequestWritten(webs_t wp, ssize written);
extern void websSetTimeMark(webs_t wp);
extern void websSetVar(webs_t wp, char_t *var, char_t *value);
extern int websSolutionHandler(webs_t wp, char_t *urlPrefix, char_t *dir, int arg, char_t *url, char_t *path, char_t *query);
extern int websTestVar(webs_t wp, char_t *var);
extern void websTimeout(void *arg, int id);
extern void websTimeoutCancel(webs_t wp);
extern int websUrlHandlerDefine(char_t *urlPrefix, char_t *webDir, int arg, int (*fn)(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query), int flags);
extern void websUrlHandlerClose();
extern int websUrlHandlerDelete(int (*fn)(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query));
extern int websUrlHandlerOpen();
extern int websUrlHandlerRequest(webs_t wp);
extern int websUrlParse(char_t *url, char_t **buf, char_t **host, char_t **path, char_t **port, char_t **query, char_t **proto, char_t **tag, char_t **ext);
extern char_t *websUrlType(char_t *webs, char_t *buf, int charCnt);
extern void websWriteHeaders(webs_t wp, int code, ssize nbytes, char_t *redirect);
extern ssize websWriteHeader(webs_t wp, char_t *fmt, ...);
extern ssize websWrite(webs_t wp, char_t *fmt, ...);
extern ssize websWriteBlock(webs_t wp, char_t *buf, ssize nChars);
extern ssize websWriteDataNonBlock(webs_t wp, char *buf, ssize nChars);
extern int websValid(webs_t wp);

#if BIT_JAVASCRIPT
extern int websAspWrite(int ejid, webs_t wp, int argc, char_t **argv);
#endif

/*************************************** SSL ***********************************/
#if BIT_PACK_SSL

extern int websSSLOpen();
extern int websSSLIsOpen();
extern void websSSLClose();
extern int websSSLAccept(webs_t wp, char_t *buf, ssize len);
extern ssize websSSLWrite(webs_t wp, char_t *buf, ssize len);
extern ssize websSSLGets(webs_t wp, char_t **buf);
extern ssize websSSLRead(webs_t wp, char_t *buf, ssize len);
extern int  websSSLEof(webs_t wp);
extern void websSSLFree(webs_t wp);
extern void websSSLFlush(webs_t wp);
extern int websSSLSetKeyFile(char_t *keyFile);
extern int websSSLSetCertFile(char_t *certFile);
extern void websSSLSocketEvent(int sid, int mask, void *iwp);

extern int sslOpen();
extern void sslClose();
extern int sslAccept(webs_t wp);
extern void sslFree(webs_t wp);
extern ssize sslRead(webs_t wp, char *buf, ssize len);
extern ssize sslWrite(webs_t wp, char *buf, ssize len);
extern void sslWriteClosureAlert(webs_t wp);
extern void sslFlush(webs_t wp);

#endif /* BIT_PACK_SSL */

/************************************** Sessions *******************************/

#if BIT_SESSIONS

struct User;

typedef struct Session {
    char        *id;                    /**< Session ID key */
    struct User *user;                  /**< User reference */
    time_t      lifespan;               /**< Session inactivity timeout (msecs) */
    sym_fd_t    cache;                  /**< Cache of session variables */
} Session;

/*
    Flags for httpSetCookie
 */
#define WEBS_COOKIE_SECURE   0x1         /**< Flag for websSetCookie for secure cookies (https only) */
#define WEBS_COOKIE_HTTP     0x2         /**< Flag for websSetCookie for http cookies (http only) */

extern void websSetCookie(webs_t wp, char_t *name, char_t *value, char_t *path, char_t *domain, time_t lifespan, int flags);

extern Session *websAllocSession(webs_t wp, char_t *id, time_t lifespan);
extern Session *websGetSession(webs_t wp, int create);
extern char_t *websGetSessionVar(webs_t wp, char_t *name, char_t *defaultValue);
extern int websSetSessionVar(webs_t wp, char_t *name, char_t *value);
extern char *websGetSessionID(webs_t wp);
#endif

/*************************************** Auth **********************************/
#if BIT_AUTH
#define AM_MAX_WORD     32

//  MOB - prefix and move to goahead.h
typedef struct User {
    char_t  *name;
    char_t  *realm;
    char_t  *password;
    int     enable;
    char_t  *roles;
    char_t  *capabilities;
} User;

typedef struct Role {
    int     enable;
    char_t  *capabilities;
} Role;

#if UNUSED
#define AM_BASIC    1
#define AM_DIGEST   2
#define AM_FORM     3
#define AM_CUSTOM   4
#endif

/*
    Login reason codes
 */
#define AM_LOGIN_REQUIRED   1
#define AM_BAD_PASSWORD     2
#define AM_BAD_USERNAME     3

typedef void (*AmLogin)(webs_t wp, int why);
typedef bool (*AmVerify)(webs_t wp);

//  MOB - rename to Location
typedef struct Uri {
    char        *prefix;
    char        *realm;
    ssize       prefixLen;
    int         enable;
#if BIT_PACK_SSL
    int         secure;
#endif
    AmLogin     login;
    AmVerify    verify;
    char_t      *loginUri;
    char_t      *capabilities;
} Uri;

//  MOB - sort
extern int amOpen(char_t *path);
extern void amClose();
extern int amAddUser(char_t *realm, char_t *name, char_t *password, char_t *roles);
extern int amSetPassword(char_t *name, char_t *password);
extern int amVerifyUser(char_t *name, char_t *password);
extern int amEnableUser(char_t *name, int enable);
extern int amRemoveUser(char_t *name);
extern int amAddUserRole(char_t *name, char_t *role);
extern int amRemoveUserRole(char_t *name, char_t *role);
extern bool amUserHasCapability(char_t *name, char_t *capability);
extern bool amFormLogin(webs_t wp, char_t *userName, char_t *password);

extern int amAddRole(char_t *role, char_t *capabilities);
extern int amRemoveRole(char_t *role);
extern int amAddRoleCapability(char_t *role, char_t *capability);
extern int amRemoveRoleCapability(char_t *role, char_t *capability);

extern int amAddUri(char_t *realm, char_t *name, char_t *capabilities, char_t *loginUri, AmLogin login, AmVerify verify);
extern int amRemoveUri(char_t *uri);
extern bool amVerifyUri(webs_t wp);
extern bool amCan(webs_t wp, char_t *capability);
//  ASP
extern bool can(char_t *capability);

extern int amGetSessionUser(webs_t wp);

#endif /* BIT_AUTH */

/************************************ Legacy **********************************/

#if BIT_LEGACY
    #define a_assert gassert
    typedef GStat gstat_t;
    #define emfSchedProc GSchedProc
    #define emfSchedCallback gSchedCallback
    #define emfUnschedCallback gUnschedCallback
    #define emfReschedCallback gReschedCallback
    #define emfSchedProcess gSchedProcess

    #define hAlloc gAlloc
    #define hAllocEntry gAllocEntry
    #define hFree gFree

    #define ascToUni gAscToUni
    #define uniToAsc gUniToAsc

    #define bopen gcloseAlloc
    #define bopen gopenAlloc
    #define bstrdupNoBalloc gstrdupNoAlloc
    #define bstrdupANoBalloc gstrdupANoAlloc
    #define balloc galloc
    #define bfree(p) gfree
    #define bfreeSafe(p) gfree(p)
    #define brealloc grealloc
    #define bstrdup gstrdup
    #define bstrdupA gstrdupA

    #define strlower gstrlower
    #define strupper gstrupper
    #define stritoa gstritoa
    #define WEBS_ASP WEBS_JS

    #define fmtValloc gfmtValloc
    #define fmtAlloc gfmtAlloc
    #define fmtStatic gfmtStatic
#endif

#ifdef __cplusplus
}
#endif
#endif /* _h_GOAHEAD */

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis GoAhead open source license or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
