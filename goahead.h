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

/********************************* Tunable Constants **************************/

#define BIT_TUNE_SIZE       1       /**< Tune for size */
#define BIT_TUNE_BALANCED   2       /**< Tune balancing speed and size */
#define BIT_TUNE_SPEED      3       /**< Tune for speed, program will use memory more aggressively */

#ifndef BIT_TUNE
    #define BIT_TUNE BIT_TUNE_SIZE
#endif

#if BIT_TUNE == BIT_TUNE_SIZE || DOXYGEN
    /*
        Reduce size allocations to reduce memory usage
     */ 
    #define BUF_MAX             4096    /* General sanity check for bufs */
    #define FNAMESIZE           254     /* Max length of file names */
    #define SYM_MAX             (512)
    #define VALUE_MAX_STRING    (4096 - 48)
    #define E_MAX_ERROR         4096
    #define E_MAX_REQUEST       2048    /* Request safeguard max */
    #define SOCKET_BUFSIZ       510     /* Underlying buffer size */

#elif BIT_TUNE == BIT_TUNE_BALANCED
    #define BUF_MAX             4096
    #define FNAMESIZE           PATHSIZE
    #define SYM_MAX             (512)
    #define VALUE_MAX_STRING    (4096 - 48)
    #define E_MAX_ERROR         4096
    #define E_MAX_REQUEST       4096
    #define SOCKET_BUFSIZ       1024

#else /* BIT_TUNE == BIT_TUNE_SPEED */
    #define BUF_MAX             4096
    #define FNAMESIZE           PATHSIZE
    #define SYM_MAX             (512)
    #define VALUE_MAX_STRING    (4096 - 48)
    #define E_MAX_ERROR         4096
    #define E_MAX_REQUEST       4096
    #define SOCKET_BUFSIZ       1024
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

#if WIN
    #include    <direct.h>
    #include    <io.h>
    #include    <sys/stat.h>
    #include    <limits.h>
    #include    <tchar.h>
    #include    <windows.h>
    #include    <winnls.h>
    #include    <time.h>
    #include    <sys/types.h>
    #include    <stdio.h>
    #include    <stdlib.h>
    #include    <fcntl.h>
    #include    <errno.h>
    #include    <share.h>
#endif /* WIN */

#if CE
    /*#include  <errno.h>*/
    #include    <limits.h>
    #include    <tchar.h>
    #include    <windows.h>
    #include    <winsock.h>
    #include    <winnls.h>
    #include    "CE/wincompat.h"
    #include    <winsock.h>
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
    #define     EINTR EINUSE
    #define      WEBS   1
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
    #include    <vxWorks.h>
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

#if BIT_FEATURE_MATRIXSSL
/*
       Matrixssl defines int32, uint32, int64 and uint64, but does not provide HAS_XXX to disable.
           So must include matrixsslApi.h first and then workaround.
            */
#if WIN32
 #include   <winsock2.h>
 #include   <windows.h>
#endif
 #include    "matrixsslApi.h"

#define     HAS_INT32 1
#define     HAS_UINT32 1
#define     HAS_INT64 1
#define     HAS_UINT64 1

#include    "goahead.h"

#define MAX_WRITE_MSEC  500 /* Fail if we block more than X millisec on write */
#endif

/************************************** Defines *******************************/
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

/**
    Signed file offset data type. Supports large files greater than 4GB in size on all systems.
 */
typedef int64 filepos;

#if DOXYGEN
    typedef int socklen;
#elif VXWORKS || BLD_WIN_LIKE
    typedef int socklen;
#else
    typedef socklen_t socklen;
#endif

typedef int64 datetime;

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

#if BIT_HAS_DOUBLE_BRACES
    #define  NULL_INIT    {{0}}
#else
    #define  NULL_INIT    {0}
#endif

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

#if VXWORKS
    #define MAIN(name, _argc, _argv, _envp)  \
        static int innerMain(int argc, char **argv, char **envp); \
        int name(char *arg0, ...) { \
            va_list args; \
            char *argp, *largv[MPR_MAX_ARGC]; \
            int largc = 0; \
            va_start(args, arg0); \
            largv[largc++] = #name; \
            if (arg0) { \
                largv[largc++] = arg0; \
            } \
            for (argp = va_arg(args, char*); argp && largc < MPR_MAX_ARGC; argp = va_arg(args, char*)) { \
                largv[largc++] = argp; \
            } \
            return innerMain(largc, largv, NULL); \
        } \
        static int innerMain(_argc, _argv, _envp)
#elif BIT_WIN_LIKE && UNICODE
    #define MAIN(name, _argc, _argv, _envp)  \
        APIENTRY WinMain(HINSTANCE inst, HINSTANCE junk, LPWSTR command, int junk2) { \
            char *largv[MPR_MAX_ARGC]; \
            extern int main(); \
            char *mcommand[MPR_MAX_STRING]; \
            int largc; \
            wtom(mcommand, sizeof(dest), command, -1);
            largc = parseArgs(mcommand, &largv[1], MPR_MAX_ARGC - 1); \
            largv[0] = #name; \
            main(largc, largv, NULL); \
        } \
        int main(argc, argv, _envp)
#elif BIT_WIN_LIKE
    #define MAIN(name, _argc, _argv, _envp)  \
        APIENTRY WinMain(HINSTANCE inst, HINSTANCE junk, char *command, int junk2) { \
            extern int main(); \
            char *largv[MPR_MAX_ARGC]; \
            int largc; \
            largc = parseArgs(command, &largv[1], MPR_MAX_ARGC - 1); \
            largv[0] = #name; \
            main(largc, largv, NULL); \
        } \
        int main(_argc, _argv, _envp)
#else
    #define MAIN(name, _argc, _argv, _envp) int main(_argc, _argv, _envp)
#endif

//  MOB - prefix
extern int parseArgs(char *args, char **argv, int maxArgc);

/*********************************** Fixups ***********************************/

#if ECOS
    #define     LIBKERN_INLINE          /* to avoid kernel inline functions */
    #if BIT_CGI
        #error "Ecos does not support CGI. Disable BIT_CGI"
    #endif
#endif /* ECOS */

#if BIT_WIN_LIKE
    typedef int socklen_t;
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

#if UNICODE
    #if !BLD_WIN_LIKE
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

#if UNUSED
/*
    How many ASCII bytes are required to represent this UNICODE string?
 */
#define TASTRL(x) ((wcslen(x) + 1) * sizeof(char_t))
#endif

#else
    #define T(s)        s
    #define TSZ(x)      (sizeof(x))
#if UNUSED
    #define TASTRL(x)   (strlen(x) + 1)
#endif
    typedef char        char_t;
    #if WIN
        typedef uchar   uchar_t;
    #endif
#endif /* UNICODE */

#if UNUSED
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif
#endif

/*
    GoAhead Copyright.
    MOB - this must be included in some source somewhere
 */
#define GOAHEAD_COPYRIGHT T("Copyright (c) Embedthis Software Inc., 1993-2012. All Rights Reserved.")

/*
    Type for unicode systems
 */
#if UNICODE
#define gmain       wmain
#define gasctime    _wasctime
#define gsprintf    swprintf
#define gprintf     wprintf
#define gfprintf    fwprintf
#define gsscanf     swscanf
#define gvsprintf   vswprintf
#define gstrcpy     wcscpy
#define gstrncpy    wcsncpy
#define gstrncat    wcsncat
#define gstrlen     wcslen
#define gstrcat     wcscat
#define gstrcmp     wcscmp
#define gstrncmp    wcsncmp
#define gstricmp    wcsicmp
#define gstrchr     wcschr
#define gstrrchr    wcsrchr
#define gstrtok     wcstok
#define gstrnset    wcsnset
#define gstrrchr    wcsrchr
#define gstrspn wcsspn
#define gstrcspn    wcscspn
#define gstrstr     wcsstr
#define gstrtol     wcstol
#define gfopen      _wfopen
#define gclose      close
#define gcreat      _wcreat
#define gfgets      fgetws
#define gfputs      fputws
#define gfscanf     fwscanf
#define ggets       _getws
#define glseek      lseek
#define gunlink     _wunlink
#define gread       read
#define grename     _wrename
#define gwrite      write
#define gtmpnam     _wtmpnam
#define gtempnam    _wtempnam
#define gfindfirst  _wfindfirst
#define gfinddata_t _wfinddata_t
#define gfindnext   _wfindnext
#define gfindclose  _findclose
#define gstat       _wstat
#define gaccess     _waccess
#define gchmod      _wchmod

typedef struct _stat gstat_t;

#define gmkdir      _wmkdir
#define gchdir      _wchdir
#define grmdir      _wrmdir
#define ggetcwd     _wgetcwd
#define gtolower    towlower
#define gtoupper    towupper

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

#define gisalnum    iswalnum
#define gisalpha    iswalpha
#define gatoi(s)    wcstol(s, NULL, 10)

#define gctime      _wctime
#define ggetenv     _wgetenv
#define gexecvp     _wexecvp
#else /* !UNICODE */

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
#endif /* VXWORKS #elif LYNX || LINUX || MACOSX || SOLARIS*/

#define gclose      close
#define gclosedir   closedir
#define gchmod      chmod
#define ggetcwd     getcwd
#define glseek      lseek
#define gloadModule loadModule
#define gopendir    opendir
#define gread       read
#define greaddir    readdir
#define grename     rename
#define gstat       stat
#define gunlink     unlink
#define gwrite      write

#define gasctime    asctime
#define gsprintf    sprintf
#define gprintf     printf
#define gfprintf    fprintf
#define gsscanf     sscanf
#define gvsprintf   vsprintf

#define gstrcpy     strcpy
#define gstrncpy    strncpy
#define gstrncat    strncat
#define gstrlen     strlen
#define gstrcat     strcat
#define gstrcmp     strcmp
#define gstrncmp    strncmp
#define gstricmp    strcmpci
#define gstrchr     strchr
#define gstrrchr    strrchr
#define gstrtok     strtok
#define gstrnset    strnset
#define gstrrchr    strrchr
#define gstrspn strspn
#define gstrcspn    strcspn
#define gstrstr     strstr
#define gstrtol     strtol

#define gfopen      fopen
#define gcreat      creat
#define gfgets      fgets
#define gfputs      fputs
#define gfscanf     fscanf
#define ggets       gets
#define gtmpnam     tmpnam
#define gtempnam    tempnam
#define gfindfirst  _findfirst
#define gfinddata_t _finddata_t
#define gfindnext   _findnext
#define gfindclose  _findclose
#define gaccess     access

#define gremove     remove

#define gtolower    tolower
#define gtoupper    toupper
#define gisspace    isspace
#define gisdigit    isdigit
#define gisxdigit   isxdigit
#define gisalnum    isalnum
#define gisalpha    isalpha
#define gisupper    isupper
#define gislower    islower
#define gatoi       atoi

#define gctime      ctime
#define ggetenv     getenv
#define gexecvp     execvp
#ifndef VXWORKS
#define gmain       main
#endif /* ! VXWORKS */

//  MOB - move
#if VXWORKS
#define fcntl(a, b, c)
#endif /* VXWORKS */
#endif /* ! UNICODE */

#if WIN
#define getcwd  _getcwd
#define tempnam _tempnam
#define open    _open
#define close   _close
#define read    _read
#define write   _write
#define chdir   _chdir
#define lseek   _lseek
#define unlink  _unlink
#define localtime localtime_s
#endif


//  MOB - move
typedef struct stat gstat_t;

/******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
/*
    Error types
 */
#define E_ASSERT            0x1         /* Assertion error */
#define E_LOG               0x2         /* Log error to log file */
#define E_USER              0x3         /* Error that must be displayed */

#define E_L                 T(__FILE__), __LINE__
#define E_ARGS_DEC          char_t *file, int line
#define E_ARGS              file, line

//  MOB - should be gassert, or goassert
#if BIT_DEBUG
    #define a_assert(C)     if (C) ; else error(E_L, E_ASSERT, T("%s"), T(#C))
#else
    #define a_assert(C)     if (1) ; else
#endif

#define elementsof(X) sizeof(X) / sizeof(X[0])

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

    vtype_t         type;
    unsigned int    valid       : 8;
    unsigned int    allocated   : 8;        /* String was allocated */
} value_t;

/*
     llocation flags 
 */
#define VALUE_ALLOCATE      0x1

#define value_numeric(t)    (t >= byteint && t <= big)
#define value_str(t)        (t >= string && t <= bytes)
#define value_ok(t)         (t > undefined && t <= symbol)

#define VALUE_VALID         { {0}, integer, 1 }
#define VALUE_INVALID       { {0}, undefined, 0 }

/******************************************************************************/
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

/*
    Ring queue buffer structure
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
} bType;

#define B_SHIFT         4                   /* Convert size to class */
#define B_ROUND         ((1 << (B_SHIFT)) - 1)
#define B_MAX_CLASS     13                  /* Maximum class number + 1 */
#define B_MALLOCED      0x80000000          /* Block was malloced */
#define B_DEFAULT_MEM   (64 * 1024)         /* Default memory allocation */
#define B_MAX_FILES     (512)               /* Maximum number of files */
#define B_FILL_CHAR     (0x77)              /* Fill byte for buffers */
#define B_FILL_WORD     (0x77777777)        /* Fill word for buffers */
#define B_MAX_BLOCKS    (64 * 1024)         /* Maximum allocated blocks */

/*
    Flags. The integrity value is used as an arbitrary value to fill the flags.
 */
#define B_INTEGRITY         0x8124000       /* Integrity value */
#define B_INTEGRITY_MASK    0xFFFF000       /* Integrity mask */
#define B_USE_MALLOC        0x1             /* Okay to use malloc if required */
#define B_USER_BUF          0x2             /* User supplied buffer for mem */

/******************************* Symbol Table *********************************/
/*
    The symbol table record for each symbol entry
 */
typedef struct sym_t {
    struct sym_t    *forw;                  /* Pointer to next hash list */
    value_t         name;                   /* Name of symbol */
    value_t         content;                /* Value of symbol */
    int             arg;                    /* Parameter value */
} sym_t;

typedef int sym_fd_t;                       /* Returned by symOpen */

/*
    Script engines
    MOB - remove
 */
#define EMF_SCRIPT_JSCRIPT          0       /* javascript */
#define EMF_SCRIPT_TCL              1       /* tcl */
#define EMF_SCRIPT_EJSCRIPT         2       /* Ejscript */
#define EMF_SCRIPT_MAX              3

#define STRSPACE    T("\t \n\r\t")

/*********************************** Cron *************************************/

typedef struct {
    char_t  *minute;
    char_t  *hour;
    char_t  *day;
    char_t  *month;
    char_t  *dayofweek;
} cron_t;

extern long     cronUntil(cron_t *cp, int period, time_t testTime);
extern int      cronAlloc(cron_t *cp, char_t *str);
extern int      cronFree(cron_t *cp);

/************************************ Socket **********************************/
/*
    Socket flags 
 */
#define SOCKET_EOF              0x1     /* Seen end of file */
#define SOCKET_CONNECTING       0x2     /* Connect in progress */
#define SOCKET_BROADCAST        0x4     /* Broadcast mode */
#define SOCKET_PENDING          0x8     /* Message pending on this socket */
#define SOCKET_FLUSHING         0x10    /* Background flushing */
#define SOCKET_DATAGRAM         0x20    /* Use datagrams */
#define SOCKET_ASYNC            0x40    /* Use async connect */
#define SOCKET_BLOCK            0x80    /* Use blocking I/O */
#define SOCKET_LISTENING        0x100   /* Socket is server listener */
#define SOCKET_CLOSING          0x200   /* Socket is closing */
#define SOCKET_CONNRESET        0x400   /* Socket connection was reset */
#define SOCKET_MYOWNBUFFERS     0x800   /* Not using inBuf/outBuf ringq */

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
#define EMF_SOCKET_MESSAGE      (WM_USER+13)

typedef void    (*socketHandler_t)(int sid, int mask, void* data);
typedef int     (*socketAccept_t)(int sid, char *ipaddr, int port, int listenSid);
typedef struct {
    char            host[64];               /* Host name */
    ringq_t         inBuf;                  /* Input ring queue */
    ringq_t         outBuf;                 /* Output ring queue */
    ringq_t         lineBuf;                /* Line ring queue */
    socketAccept_t  accept;                 /* Accept handler */
    socketHandler_t handler;                /* User I/O handler */
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
    int             secure;                 /* Socket is using SSL */
} socket_t;

extern socket_t     **socketList;           /* List of open sockets */

/********************************* Prototypes *********************************/
/*
    Memory allocation
 */

extern void bclose();
extern int  bopen(void *buf, int bufsize, int flags);

#if !BIT_REPLACE_MALLOC
    extern char_t *bstrdupNoBalloc(char_t *s);
    extern char *bstrdupANoBalloc(char *s);
    #define balloc(num) malloc(num)
    #define bfree(p) if (p) { free(p); } else
    #define brealloc(p, num) realloc(p, num)
    #define bstrdup(s) bstrdupNoBalloc(s)
    #define bstrdupA(s) bstrdupANoBalloc(s)
    #define gstrdup(s) bstrdupNoBalloc(s)

#else /* BIT_REPLACE_MALLOC */
    #if UNICODE
        extern char *bstrdupA(char *s);
        #define bstrdupA(p) bstrdupA(p)
    #else /* UNICODE */
        #define bstrdupA bstrdup
    #endif /* UNICODE */
    #define gstrdup bstrdup
    extern void     *balloc(ssize size);
    extern void     bfree(void *mp);
    extern void     *brealloc(void *buf, int newsize);
    extern char_t   *bstrdup(char_t *s);
#endif /* BIT_REPLACE_MALLOC */

#define bfreeSafe(p) bfree(p)

/*
    Flags. The integrity value is used as an arbitrary value to fill the flags.
 */
#define B_USE_MALLOC        0x1             /* Okay to use malloc if required */
#define B_USER_BUF          0x2             /* User supplied buffer for mem */

#if! LINUX
extern char_t   *basename(char_t *name);
#endif /* !LINUX */

typedef void    (emfSchedProc)(void *data, int id);
extern int      emfSchedCallback(int delay, emfSchedProc *proc, void *arg);
extern void     emfUnschedCallback(int id);
extern void     emfReschedCallback(int id, int delay);
extern void     emfSchedProcess();
extern int      emfInstGet();
extern void     emfInstSet(int inst);

extern int      hAlloc(void ***map);
extern int      hAllocEntry(void ***list, int *max, int size);

extern int      hFree(void ***map, int handle);

extern int      ringqOpen(ringq_t *rq, int increment, int maxsize);
extern void     ringqClose(ringq_t *rq);
extern ssize    ringqLen(ringq_t *rq);
extern int      ringqPutc(ringq_t *rq, char_t c);
extern int      ringqInsertc(ringq_t *rq, char_t c);
extern ssize    ringqPutStr(ringq_t *rq, char_t *str);
extern int      ringqGetc(ringq_t *rq);

extern ssize    fmtValloc(char_t **s, ssize n, char_t *fmt, va_list arg);
extern ssize    fmtAlloc(char_t **s, ssize n, char_t *fmt, ...);
extern ssize    fmtStatic(char_t *s, ssize n, char_t *fmt, ...);

#if UNICODE
extern int      ringqPutcA(ringq_t *rq, char c);
extern int      ringqInsertcA(ringq_t *rq, char c);
extern int      ringqPutStrA(ringq_t *rq, char *str);
extern int      ringqGetcA(ringq_t *rq);
#else
#define ringqPutcA ringqPutc
#define ringqInsertcA ringqInsertc
#define ringqPutStrA ringqPutStr
#define ringqGetcA ringqGetc
#endif /* UNICODE */

extern ssize    ringqPutBlk(ringq_t *rq, uchar *buf, ssize len);
extern ssize    ringqPutBlkMax(ringq_t *rq);
extern void     ringqPutBlkAdj(ringq_t *rq, ssize size);
extern ssize    ringqGetBlk(ringq_t *rq, uchar *buf, ssize len);
extern ssize    ringqGetBlkMax(ringq_t *rq);
extern void     ringqGetBlkAdj(ringq_t *rq, ssize size);
extern void     ringqFlush(ringq_t *rq);
extern void     ringqAddNull(ringq_t *rq);

extern int      scriptSetVar(int engine, char_t *var, char_t *value);
extern int      scriptEval(int engine, char_t *cmd, char_t **rslt, void* chan);

extern void     socketClose();
extern void     socketCloseConnection(int sid);
extern void     socketCreateHandler(int sid, int mask, socketHandler_t handler, void* arg);
extern void     socketDeleteHandler(int sid);
extern int      socketEof(int sid);
extern ssize    socketCanWrite(int sid);
extern void     socketSetBufferSize(int sid, int in, int line, int out);
extern int      socketFlush(int sid);
extern ssize    socketGets(int sid, char_t **buf);
extern int      socketGetPort(int sid);
extern ssize    socketInputBuffered(int sid);
extern int      socketOpen();
extern int      socketOpenConnection(char *host, int port, socketAccept_t accept, int flags);
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

extern char_t   *strlower(char_t *string);
extern char_t   *strupper(char_t *string);

//  MOB - where should this be
#define glen gstrlen
extern int gopen(char_t *path, int oflag, int mode);
extern int gcaselesscmp(char_t *s1, char_t *s2);
extern bool gcaselessmatch(char_t *s1, char_t *s2);
extern bool gmatch(char_t *s1, char_t *s2);
extern int gcmp(char_t *s1, char_t *s2);
extern int gncmp(char_t *s1, char_t *s2, ssize n);
extern int gncaselesscmp(char_t *s1, char_t *s2, ssize n);
extern char_t   *stritoa(int n, char_t *string, int width);

extern sym_fd_t symOpen(int hash_size);
extern void     symClose(sym_fd_t sd);
extern sym_t    *symLookup(sym_fd_t sd, char_t *name);
extern sym_t    *symEnter(sym_fd_t sd, char_t *name, value_t v, int arg);
extern int      symDelete(sym_fd_t sd, char_t *name);
extern void     symWalk(sym_fd_t sd, void (*fn)(sym_t *symp));
extern sym_t    *symFirst(sym_fd_t sd);
extern sym_t    *symNext(sym_fd_t sd);
extern int      symSubOpen();
extern void     symSubClose();

extern void     error(E_ARGS_DEC, int flags, char_t *fmt, ...);
extern void     trace(int lev, char_t *fmt, ...);
#if BIT_DEBUG_LOG
    extern void     traceRaw(char_t *buf);
    extern int      traceOpen();
    extern void     traceClose();
    typedef void    (*TraceHandler)(int level, char_t *msg);
    extern TraceHandler traceSetHandler(TraceHandler handler);
    extern void traceSetPath(char_t *path);
    #define LOG trace
#else
    #define traceOpen() 
    #define traceClose()
    #define LOG if (0) trace
#endif


extern value_t  valueInteger(long value);
extern value_t  valueString(char_t *value, int flags);
extern value_t  valueErrmsg(char_t *value);
extern void     valueFree(value_t *v);
extern int      vxchdir(char *dirname);

extern unsigned int hextoi(char_t *hexstring);
extern unsigned int gstrtoi(char_t *s);
extern time_t    timeMsec();

extern char_t   *ascToUni(char_t *ubuf, char *str, ssize nBytes);
extern char     *uniToAsc(char *buf, char_t *ustr, ssize nBytes);
extern char_t   *ballocAscToUni(char  *cp, ssize alen);
extern char     *ballocUniToAsc(char_t *unip, ssize ulen);

#if UNUSED
//  MOB - do these exist?
extern char_t   *basicGetHost();
extern void     basicSetHost(char_t *host);
extern void     basicSetAddress(char_t *addr);
#endif

//  MOB - move
extern char *websMD5(cchar *s);
extern char *websMD5binary(cchar *buf, ssize length, cchar *prefix);

//  MOB
extern int goOpen();
extern void goClose();

/********************************** Defines ***********************************/

//  MOB - some of these are tunables

#define WEBS_HEADER_BUFINC      512         /* Header buffer size */
//  MOB - rename ASP => WEBS_JS_BUFINC
#define WEBS_ASP_BUFINC         512         /* Asp expansion increment */
#define WEBS_MAX_PASS           32          /* Size of password */
#define WEBS_BUFSIZE            960         /* websWrite max output string */
#define WEBS_MAX_HEADER         (5 * 1024)  /* Sanity check header */
#define WEBS_MAX_URL            2048        /* Maximum URL size for sanity */
#define WEBS_SOCKET_BUFSIZ      256         /* Bytes read from socket */
#define WEBS_MAX_REQUEST        2048        /* Request safeguard max */


//  MOB - prefix
#define PAGE_READ_BUFSIZE       512         /* bytes read from page files */
#define MAX_PORT_LEN            10          /* max digits in port number */
#define WEBS_SYM_INIT           64          /* initial # of sym table entries */

//  MOB move to main.bit
#define CGI_BIN                 T("cgi-bin")

/* 
    Request flags. Also returned by websGetRequestFlags()
 */
#define WEBS_LOCAL_PAGE         0x1         /* Request for local webs page */ 
#define WEBS_KEEP_ALIVE         0x2         /* HTTP/1.1 keep alive */
#define WEBS_COOKIE             0x8         /* Cookie supplied in request */
#define WEBS_IF_MODIFIED        0x10        /* If-modified-since in request */
#define WEBS_POST_REQUEST       0x20        /* Post request operation */
#define WEBS_LOCAL_REQUEST      0x40        /* Request from this system */
#define WEBS_HOME_PAGE          0x80        /* Request for the home page */ 
//  MOB - rename ASP => WEBS_JS
#define WEBS_ASP                0x100       /* ASP request */ 
#define WEBS_HEAD_REQUEST       0x200       /* Head request */
#define WEBS_CLEN               0x400       /* Request had a content length */
#define WEBS_FORM               0x800       /* Request is a form */
#define WEBS_REQUEST_DONE       0x1000      /* Request complete */
#define WEBS_POST_DATA          0x2000      /* Already appended post data */
#define WEBS_CGI_REQUEST        0x4000      /* cgi-bin request */
#define WEBS_SECURE             0x8000      /* connection uses SSL */
#define WEBS_AUTH_BASIC         0x10000     /* Basic authentication request */
#define WEBS_AUTH_DIGEST        0x20000     /* Digest authentication request */
#define WEBS_HEADER_DONE        0x40000     /* Already output the HTTP header */

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
    char_t          *dir;               /* Directory containing the page */
    char_t          *path;              /* Path name without query */
    char_t          *url;               /* Full request url */
    char_t          *host;              /* Requested host */
    char_t          *lpath;             /* Cache local path name */
    char_t          *query;             /* Request query */
    char_t          *decodedQuery;      /* Decoded request query */
    char_t          *authType;          /* Authorization type (Basic/DAA) */
    char_t          *password;          /* Authorization password */
    char_t          *userName;          /* Authorization username */
    char_t          *cookie;            /* Cookie string */
    char_t          *userAgent;         /* User agent (browser) */
    char_t          *protocol;          /* Protocol (normally HTTP) */
    char_t          *protoVersion;      /* Protocol version */
    int             sid;                /* Socket id (handler) */
    int             listenSid;          /* Listen Socket id */
    int             port;               /* Request port number */
    int             state;              /* Current state */
    int             flags;              /* Current flags -- see above */
    int             code;               /* Request result code */
    int             clen;               /* Content length */
    int             wid;                /* Index into webs */
    char_t          *cgiStdin;          /* filename for CGI stdin */
    int             docfd;              /* Document file descriptor */
    ssize           numbytes;           /* Bytes to transfer to browser */
    ssize           written;            /* Bytes actually transferred */
    void            (*writeSocket)(struct websRec *wp);
#if BIT_DIGEST_AUTH
    char_t          *realm;     /* usually the same as "host" from websRec */
    char_t          *nonce;     /* opaque-to-client string sent by server */
    char_t          *digest;    /* digest form of user password */
    char_t          *uri;       /* URI found in DAA header */
    char_t          *opaque;    /* opaque value passed from server */
    char_t          *nc;        /* nonce count */
    char_t          *cnonce;    /* check nonce */
    char_t          *qop;       /* quality operator */
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

/******************************** Prototypes **********************************/

extern int       websAccept(int sid, char *ipaddr, int port, int listenSid);
extern int       websAspDefine(char_t *name, int (*fn)(int ejid, webs_t wp, int argc, char_t **argv));
extern int       websAspRequest(webs_t wp, char_t *lpath);
extern void      websCloseListen();
extern char_t    *websDecode64(char_t *string);
extern char_t   *websDecode64Block(char_t *s, ssize *len, int flags);
extern void      websDecodeUrl(char_t *token, char_t *decoded, ssize len);
extern void      websDone(webs_t wp, int code);
extern void      websError(webs_t wp, int code, char_t *msg, ...);
extern char_t   *websErrorMsg(int code);
extern void      websFooter(webs_t wp);
extern int       websFormDefine(char_t *name, void (*fn)(webs_t wp, char_t *path, char_t *query));
extern char_t   *websGetDefaultDir();
extern char_t   *websGetDefaultPage();
extern char_t   *websGetHostUrl();
extern char_t   *websGetIpaddrUrl();
extern char_t   *websGetPassword();
extern int       websGetPort();
extern char_t   *websGetPublishDir(char_t *path, char_t **urlPrefix);
extern char_t   *websGetRealm();
extern ssize     websGetRequestBytes(webs_t wp);
extern char_t   *websGetRequestDir(webs_t wp);
extern int       websGetRequestFlags(webs_t wp);
extern char_t   *websGetRequestIpaddr(webs_t wp);
extern char_t   *websGetRequestLpath(webs_t wp);
extern char_t   *websGetRequestPath(webs_t wp);
extern char_t   *websGetRequestPassword(webs_t wp);
extern char_t   *websGetRequestType(webs_t wp);
extern ssize     websGetRequestWritten(webs_t wp);
extern char_t   *websGetVar(webs_t wp, char_t *var, char_t *def);
extern int       websCompareVar(webs_t wp, char_t *var, char_t *value);
extern void      websHeader(webs_t wp);
extern int       websOpenListen(char *ip, int port, int sslPort);
extern int       websPageOpen(webs_t wp, char_t *lpath, char_t *path, int mode, int perm);
extern void      websPageClose(webs_t wp);
extern int       websPublish(char_t *urlPrefix, char_t *path);
extern void      websRedirect(webs_t wp, char_t *url);
extern void      websSecurityDelete();
extern int       websSecurityHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, 
                    char_t *query);
extern void     websSetDefaultDir(char_t *dir);
extern void     websSetDefaultPage(char_t *page);
extern void     websSetEnv(webs_t wp);
extern void     websSetHost(char_t *host);
extern void     websSetIpaddr(char_t *ipaddr);
extern void     websSetPassword(char_t *password);
extern void     websSetRealm(char_t *realmName);
extern void     websSetRequestBytes(webs_t wp, ssize bytes);
extern void     websSetRequestFlags(webs_t wp, int flags);
extern void     websSetRequestLpath(webs_t wp, char_t *lpath);
extern void     websSetRequestPath(webs_t wp, char_t *dir, char_t *path);
extern char_t  *websGetRequestUserName(webs_t wp);
extern void     websSetRequestWritten(webs_t wp, ssize written);
extern void     websSetVar(webs_t wp, char_t *var, char_t *value);
extern int      websTestVar(webs_t wp, char_t *var);
extern void     websTimeoutCancel(webs_t wp);
extern int      websUrlHandlerDefine(char_t *urlPrefix, char_t *webDir, int arg, int (*fn)(webs_t wp, char_t *urlPrefix, 
                    char_t *webDir, int arg, char_t *url, char_t *path, char_t *query), int flags);
extern int      websUrlHandlerDelete(int (*fn)(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, 
                    char_t *path, char_t *query));
extern int      websUrlHandlerRequest(webs_t wp);
extern int      websUrlParse(char_t *url, char_t **buf, char_t **host, char_t **path, char_t **port, char_t **query, 
                    char_t **proto, char_t **tag, char_t **ext);
extern char_t   *websUrlType(char_t *webs, char_t *buf, int charCnt);
extern ssize     websWrite(webs_t wp, char_t *fmt, ...);
extern ssize     websWriteBlock(webs_t wp, char_t *buf, ssize nChars);
extern ssize     websWriteDataNonBlock(webs_t wp, char *buf, ssize nChars);
extern int       websValid(webs_t wp);
extern int       websValidateUrl(webs_t wp, char_t *path);
extern void      websSetTimeMark(webs_t wp);

/*
    The following prototypes are used by the SSL layer websSSL.c
 */
extern int      websAlloc(int sid);
extern void     websFree(webs_t wp);
extern void     websTimeout(void *arg, int id);
extern void     websReadEvent(webs_t wp);

extern char_t   *websCalcNonce(webs_t wp);
extern char_t   *websCalcOpaque(webs_t wp);
extern char_t   *websCalcDigest(webs_t wp);
extern char_t   *websCalcUrlDigest(webs_t wp);

extern int       websAspOpen();
extern void      websAspClose();
#if BIT_JAVASCRIPT
extern int       websAspWrite(int ejid, webs_t wp, int argc, char_t **argv);
#endif

extern void      websFormOpen();
extern void      websFormClose();
extern void      websDefaultOpen();
extern void      websDefaultClose();

extern int       websDefaultHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, 
                    char_t *query);
extern int       websDefaultHomePageHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, 
                    char_t *path, char_t *query);
extern int       websFormHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, 
                    char_t *query);
extern int       websCgiHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, 
                    char_t *query);
extern void      websCgiCleanup();
extern int       websCheckCgiProc(int handle);
extern char_t    *websGetCgiCommName();

extern int       websLaunchCgiProc(char_t *cgiPath, char_t **argp, char_t **envp, char_t *stdIn, char_t *stdOut);
extern void      websResponse(webs_t wp, int code, char_t *msg, char_t *redirect);
extern int       websJavaScriptEval(webs_t wp, char_t *script);
extern ssize     websPageReadData(webs_t wp, char *buf, ssize nBytes);
extern int       websPageOpen(webs_t wp, char_t *lpath, char_t *path, int mode, int perm);
extern void      websPageClose(webs_t wp);
extern void      websPageSeek(webs_t wp, filepos offset);
extern int       websPageStat(webs_t wp, char_t *lpath, char_t *path, websStatType *sbuf);
extern int       websPageIsDirectory(char_t *lpath);
extern int       websRomOpen();
extern void      websRomClose();
extern int       websRomPageOpen(webs_t wp, char_t *path, int mode, int perm);
extern void      websRomPageClose(int fd);
extern ssize     websRomPageReadData(webs_t wp, char *buf, ssize len);
extern int       websRomPageStat(char_t *path, websStatType *sbuf);
extern long      websRomPageSeek(webs_t wp, filepos offset, int origin);
extern void      websSetRequestSocketHandler(webs_t wp, int mask, 
                    void (*fn)(webs_t wp));
extern int       websSolutionHandler(webs_t wp, char_t *urlPrefix,
                    char_t *webDir, int arg, char_t *url, char_t *path, 
                    char_t *query);
extern void      websUrlHandlerClose();
extern int       websUrlHandlerOpen();
extern int       websOpen();
extern void      websClose();
extern int       websOpenServer(char *ip, int port, int sslPort, char_t *documents);
extern void      websCloseServer();
extern char_t   *websGetDateString(websStatType* sbuf);
extern void      websServiceEvents(int *finished);


//  MOB - why here
extern int      strcmpci(char_t *s1, char_t *s2);

#if CE
//  MOB - prefix
extern int      writeUniToAsc(int fid, void *buf, ssize len);
extern int      readAscToUni(int fid, void **buf, ssize len);
#endif

#if BIT_PACK_MATRIXSSL
#if UNUSED
//  MOB - sslConn_t ssl_t should not be visible in this file
typedef struct {
    sslConn_t* sslConn;
    struct websRec* wp;
} websSSL_t;
#endif
#endif

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


#if OLD && UNUSED
extern int  sslAccept(sslConn_t **cp, SOCKET fd, sslKeys_t *keys, int32 resume,
            int (*certValidator)(ssl_t *, psX509Cert_t *, int32));
extern ssize sslRead(sslConn_t *cp, char *buf, ssize len);
extern ssize sslWrite(sslConn_t *cp, char *buf, ssize len);
extern void sslWriteClosureAlert(sslConn_t *cp);
#else
extern int sslOpen();
extern void sslClose();
extern int sslAccept(webs_t wp);
extern void sslFree(webs_t wp);
extern ssize sslRead(webs_t wp, char *buf, ssize len);
extern ssize sslWrite(webs_t wp, char *buf, ssize len);
extern void sslWriteClosureAlert(webs_t wp);
extern void sslFlush(webs_t wp);
#endif

#endif /* BIT_PACK_SSL */

/*************************************** EmfDb *********************************/

//  MOB - prefix
#define     T_INT                   0
#define     T_STRING                1
#define     DB_OK                   0
#define     DB_ERR_GENERAL          -1
#define     DB_ERR_COL_NOT_FOUND    -2
#define     DB_ERR_COL_DELETED      -3
#define     DB_ERR_ROW_NOT_FOUND    -4
#define     DB_ERR_ROW_DELETED      -5
#define     DB_ERR_TABLE_NOT_FOUND  -6
#define     DB_ERR_TABLE_DELETED    -7
#define     DB_ERR_BAD_FORMAT       -8
#define     DB_CASE_INSENSITIVE     1

//  MOB - remove _s 
typedef struct dbTable_s {
    char_t  *name;
    int     nColumns;
    char_t  **columnNames;
    int     *columnTypes;
    int     nRows;
    int     **rows;
} dbTable_t;

/*
    Add a schema to the module-internal schema database
 */
extern int      dbRegisterDBSchema(dbTable_t *sTable);
extern int      dbOpen(char_t *databasename, char_t *filename, int (*gettime)(int did), int flags);
extern void     dbClose(int did);
extern int      dbGetTableId(int did, char_t *tname);
extern char_t   *dbGetTableName(int did, int tid);
extern int      dbReadInt(int did, char_t *table, char_t *column, int row, int *returnValue);
extern int      dbReadStr(int did, char_t *table, char_t *column, int row, char_t **returnValue);
extern int      dbWriteInt(int did, char_t *table, char_t *column, int row, int idata);
extern int      dbWriteStr(int did, char_t *table, char_t *column, int row, char_t *s);
extern int      dbAddRow(int did, char_t *table);
extern int      dbDeleteRow(int did, char_t *table, int rid);
extern int      dbSetTableNrow(int did, char_t *table, int nNewRows);
extern int      dbGetTableNrow(int did, char_t *table);

/*
    Dump the contents of a database to file
 */
extern int      dbSave(int did, char_t *filename, int flags);

/*
    Load the contents of a database to file
 */
extern int      dbLoad(int did, char_t *filename, int flags);
extern int      dbSearchStr(int did, char_t *table, char_t *column, char_t *value, int flags);
extern void     dbZero(int did);
extern char_t   *basicGetProductDir();
extern void     basicSetProductDir(char_t *proddir);

/******************************** User Management *****************************/

//  MOB - prefix on type name and AM_NAME
typedef enum {
    AM_NONE = 0,
    AM_FULL,
    AM_BASIC,
    AM_DIGEST,
    AM_INVALID
} accessMeth_t;

//  MOB - remove bool_t
typedef short bool_t;

#if BIT_USER_MANAGEMENT
/*
    Error Return Flags
 */
#define UM_OK               0
#define UM_ERR_GENERAL      -1
#define UM_ERR_NOT_FOUND    -2
#define UM_ERR_PROTECTED    -3
#define UM_ERR_DUPLICATE    -4
#define UM_ERR_IN_USE       -5
#define UM_ERR_BAD_NAME     -6
#define UM_ERR_BAD_PASSWORD -7

/*
    Privilege Masks
 */
//  MOB - prefix
#define PRIV_NONE   0x00
#define PRIV_READ   0x01
#define PRIV_WRITE  0x02
#define PRIV_ADMIN  0x04

/*
    User classes
 */

/*
    umOpen() must be called before accessing User Management functions
 */
extern int umOpen();

/*
    umClose() should be called before shutdown to free memory
 */
extern void umClose();

/*
    umCommit() persists the user management database
 */
extern int umCommit(char_t *filename);

/*
    umRestore() loads the user management database
 */
extern int umRestore(char_t *filename);

/*
    umUser functions use a user ID for a key
 */
extern int umAddUser(char_t *user, char_t *password, char_t *group, bool_t protect, bool_t disabled);

extern int umDeleteUser(char_t *user);

//  MOB - sort
extern char_t *umGetFirstUser();
extern char_t *umGetNextUser(char_t *lastUser);
extern bool_t umUserExists(char_t *user);
extern char_t *umGetUserPassword(char_t *user);
extern int umSetUserPassword(char_t *user, char_t *password);
extern char_t *umGetUserGroup(char_t *user);
extern int umSetUserGroup(char_t *user, char_t *password);
extern bool_t umGetUserEnabled(char_t *user);
extern int umSetUserEnabled(char_t *user, bool_t enabled);
extern bool_t umGetUserProtected(char_t *user);
extern int umSetUserProtected(char_t *user, bool_t protect);

/*
    umGroup functions use a group name for a key
 */
extern int umAddGroup(char_t *group, short privilege, accessMeth_t am, bool_t protect, bool_t disabled);

extern int umDeleteGroup(char_t *group);

extern char_t *umGetFirstGroup();
extern char_t *umGetNextGroup(char_t *lastUser);

extern bool_t umGroupExists(char_t *group);
extern bool_t umGetGroupInUse(char_t *group);

extern accessMeth_t umGetGroupAccessMethod(char_t *group);
extern int umSetGroupAccessMethod(char_t *group, accessMeth_t am);

extern bool_t umGetGroupEnabled(char_t *group);
extern int umSetGroupEnabled(char_t *group, bool_t enabled);

//  MOB - don't use short
extern short umGetGroupPrivilege(char_t *group);
extern int umSetGroupPrivilege(char_t *group, short privileges);

extern bool_t umGetGroupProtected(char_t *group);
extern int umSetGroupProtected(char_t *group, bool_t protect);

/*
    umAccessLimit functions use a URL as a key
 */
extern int umAddAccessLimit(char_t *url, accessMeth_t am, short secure, char_t *group);
extern int umDeleteAccessLimit(char_t *url);
extern char_t *umGetFirstAccessLimit();
extern char_t *umGetNextAccessLimit(char_t *lastUser);

/*
    Returns the name of an ancestor access limit if
 */
extern char_t *umGetAccessLimit(char_t *url);
extern bool_t umAccessLimitExists(char_t *url);
extern accessMeth_t umGetAccessLimitMethod(char_t *url);
extern int umSetAccessLimitMethod(char_t *url, accessMeth_t am);
extern short umGetAccessLimitSecure(char_t *url);
extern int umSetAccessLimitSecure(char_t *url, short secure);
extern char_t *umGetAccessLimitGroup(char_t *url);
extern int umSetAccessLimitGroup(char_t *url, char_t *group);
extern accessMeth_t umGetAccessMethodForURL(char_t *url);
extern bool_t umUserCanAccessURL(char_t *user, char_t *url);

extern void formDefineUserMgmt(void);

#endif /* BIT_USER_MANAGEMENT */
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
