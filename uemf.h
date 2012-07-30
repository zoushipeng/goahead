/*
    uemf.h -- GoAhead Micro Embedded Management Framework Header
    MOB - rename  core.h 
  
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_UEMF
#define _h_UEMF 1

/************************************ Defaults ********************************/

#define DIGEST_ACCESS_SUPPORT  1

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

#elif BIT_TUNE == BIT_TUNE_BALANCED
    #define BUF_MAX             4096
    #define FNAMESIZE           PATHSIZE
    #define SYM_MAX             (512)
    #define VALUE_MAX_STRING    (4096 - 48)
    #define E_MAX_ERROR         4096
    #define E_MAX_REQUEST       4096

#else /* BIT_TUNE == BIT_TUNE_SPEED */
    #define BUF_MAX             4096
    #define FNAMESIZE           PATHSIZE
    #define SYM_MAX             (512)
    #define VALUE_MAX_STRING    (4096 - 48)
    #define E_MAX_ERROR         4096
    #define E_MAX_REQUEST       4096
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

//  MOB - others

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
    Operating system defines. Use compiler standard defintions to sleuth. 
    Works for all except VxWorks which does not define any special symbol.
    NOTE: Support for SCOV Unix, LynxOS and UnixWare is deprecated. 
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
    #define QNX 0
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

#if NW
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
#endif /* NW */

#if SCOV5 
    #include    <sys/types.h>
    #include    <stdio.h>
    #include    "sys/socket.h"
    #include    "sys/select.h"
    #include    "netinet/in.h"
    #include    "arpa/inet.h"
    #include    "netdb.h"
#endif /* SCOV5 */

#if UNIX
    #include    <stdio.h>
#endif /* UNIX */

#if LINUX
    #include    <sys/types.h>
    #include    <sys/stat.h>
    #include    <sys/param.h>
    #include    <limits.h>
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
#endif /* LINUX */

#if LYNX
    #include    <limits.h>
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
#endif /* VXWORKS */

#if sparc
# define __NO_PACK
#endif /* sparc */

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
#endif /* SOLARIS */

#if QNX4
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
#endif /* QNX4 */

#if ECOS
    #include    <limits.h>
    #include    <cyg/infra/cyg_type.h>
    #include    <cyg/kernel/kapi.h>
    #include    <time.h>
    #include    <network.h>
    #include    <errno.h>
#endif /* ECOS */

/********************************** Includes **********************************/

#include    <ctype.h>
#include    <stdarg.h>
#include    <string.h>

#if UNUSED
#ifndef WEBS
#include    "messages.h"
#endif /* ! WEBS */
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
    #define HAS_OFF64 1
#else
    #define HAS_OFF64 0
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
        /**
            Unsigned integer 64 bit data type.
         */
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
typedef int64 MprOff;

/*
    Socklen_t
 */
#if DOXYGEN
    typedef int MprSocklen;
#elif VXWORKS
    typedef int MprSocklen;
#else
    typedef socklen_t MprSocklen;
#endif

/**
    Date and Time Service
    @stability Evolving
    @see MprTime mprCompareTime mprCreateTimeService mprDecodeLocalTime mprDecodeUniversalTime mprFormatLocalTime 
        mprFormatTm mprGetDate mprGetElapsedTime mprGetRemainingTime mprGetTicks mprGetTimeZoneOffset mprMakeTime 
        mprMakeUniversalTime mprParseTime 
    @defgroup MprTime MprTime
 */
typedef int64 MprTime;

#ifndef BITSPERBYTE
    #define BITSPERBYTE     (8 * sizeof(char))
#endif

#ifndef BITS
    #define BITS(type)      (BITSPERBYTE * (int) sizeof(type))
#endif

#if BIT_FLOAT
    #ifndef MAXFLOAT
        #if BIT_WIN_LIKE
            #define MAXFLOAT        DBL_MAX
        #else
            #define MAXFLOAT        FLT_MAX
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
    #define MPR_EXPORT  __declspec(dllexport)
#else
    #define INT64(x)    (x##LL)
    #define UINT64(x)   (x##ULL)
    #define MPR_EXPORT 
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

#if MACSOX
    #define LD_LIBRARY_PATH "DYLD_LIBRARY_PATH"
#else
    #define LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#endif

#if VXWORKS
/*
    Old VxWorks can't do array[]
 */
#define MPR_FLEX 0
#else
#define MPR_FLEX
#endif

/*********************************** Fixups ***********************************/

//  MOB - sort this out
#ifdef UW
    #define     __NO_PACK       1
#endif /* UW */

#if SCOV5 || VXWORKS || LINUX || LYNX || MACOSX
    #ifndef O_BINARY
        #define O_BINARY 0
    #endif /* O_BINARY */
    #define SOCKET_ERROR -1
#endif /* SCOV5 || VXWORKS || LINUX || LYNX || MACOSX */

#if WIN || CE
    #define     __NO_FCNTL      1
    #undef R_OK
    #define R_OK    4
    #undef W_OK
    #define W_OK    2
    #undef X_OK
    #define X_OK    1
    #undef F_OK
    #define F_OK    0
    typedef int socklen_t;
#endif /* WIN || CE */

//MOB - why?
#if LINUX && !defined(_STRUCT_TIMEVAL)
struct timeval
{
    time_t  tv_sec;     /* Seconds.  */
    time_t  tv_usec;    /* Microseconds.  */
};
#define _STRUCT_TIMEVAL 1
#endif /* LINUX && !_STRUCT_TIMEVAL */

#ifdef ECOS
    #define     O_RDONLY        1
    #define     O_BINARY        2
    #define     __NO_PACK       1
    #define     __NO_EJ_FILE    1
    #define     __NO_CGI_BIN    1
    #define     __NO_FCNTL      1
    /* #define LIBKERN_INLINE to avoid kernel inline functions */
    #define     LIBKERN_INLINE
#endif /* ECOS */

#ifdef QNX4
    typedef long        fd_mask;
    #define NFDBITS (sizeof (fd_mask) * NBBY)   /* bits per mask */
#endif /* QNX4 */

#ifdef MACOSX
    typedef int32_t         fd_mask;
#endif

#ifdef NW
    #define fd_mask         fd_set
    #define INADDR_NONE     -1l
    #define Sleep           delay
    #define __NO_FCNTL      1
    #undef R_OK
    #define R_OK    4
    #undef W_OK
    #define W_OK    2
    #undef X_OK
    #define X_OK    1
    #undef F_OK
    #define F_OK    0
#endif /* NW */


#ifndef CHAR_T_DEFINED
#define CHAR_T_DEFINED 1
#ifdef UNICODE
/*
    To convert strings to UNICODE. We have a level of indirection so things like T(__FILE__) will expand properly.
 */
#define T(x)                __TXT(x)
#define __TXT(s)            L ## s
typedef unsigned short      char_t;
typedef unsigned short      uchar_t;

/*
    Text size of buffer macro. A buffer bytes will hold (size / char size) characters. 
 */
#define TSZ(x)              (sizeof(x) / sizeof(char_t))

/*
    How many ASCII bytes are required to represent this UNICODE string?
 */
#define TASTRL(x)           ((wcslen(x) + 1) * sizeof(char_t))

#else
#define T(s)                s
typedef char                char_t;
#define TSZ(x)              (sizeof(x))
#define TASTRL(x)           (strlen(x) + 1)
#ifdef WIN
typedef unsigned char       uchar_t;
#endif /* WIN */

#endif /* UNICODE */
#endif /* ! CHAR_T_DEFINED */

/*
    "Boolean" constants
    MOB - remove these
 */
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/*
    GoAhead Copyright.
    MOB - this must be included in some source somewhere
 */
#define GOAHEAD_COPYRIGHT T("Copyright (c) Embedthis Software Inc., 1993-2012. All Rights Reserved.")

/*
    Type for unicode systems
 */
#ifdef UNICODE
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
#define gopen       _wopen
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

#ifdef CE
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
#endif  /* if CE */

#define gisalnum    iswalnum
#define gisalpha    iswalpha
#define gatoi(s)    wcstol(s, NULL, 10)

#define gctime      _wctime
#define ggetenv     _wgetenv
#define gexecvp     _wexecvp
#else /* ! UNICODE */

#ifdef VXWORKS
#define gchdir      vxchdir
#define gmkdir      vxmkdir
#define grmdir      vxrmdir
#elif (defined (LYNX) || defined (LINUX) || defined (MACOSX) || defined (SOLARIS))
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
#define gopen       open
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

typedef struct stat gstat_t;

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
#ifdef VXWORKS
#define fcntl(a, b, c)
#endif /* VXWORKS */
#endif /* ! UNICODE */

#ifdef WIN32
#define getcwd  _getcwd
#define tempnam _tempnam
#define open    _open
#define close   _close
#define read    _read
#define write   _write
#define chdir   _chdir
#define lseek   _lseek
#define unlink  _unlink
//#define strtok(x, y) strtok_s(x, y, NULL)
#define localtime localtime_s
//#define strcat(x, y) strcat_s(x, elementsof(x), y)
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

#if (defined (ASSERT) || defined (ASSERT_CE))
    #define a_assert(C)     if (C) ; else error(E_L, E_ASSERT, T("%s"), T(#C))
#else
    #define a_assert(C)     if (1) ; else
#endif /* ASSERT || ASSERT_CE */

#define elementsof(X) sizeof(X) / sizeof(X[0])

/******************************************************************************/
/*                                 VALUE                                      */
/******************************************************************************/
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

#ifndef __NO_PACK
#pragma pack(2)
#endif /* _NO_PACK */

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
    unsigned int    allocated   : 8;        /* String was balloced */
} value_t;

#ifndef __NO_PACK
#pragma pack()
#endif /* __NO_PACK */

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
    unsigned char   *buf;               /* Holding buffer for data */
    unsigned char   *servp;             /* Pointer to start of data */
    unsigned char   *endp;              /* Pointer to end of data */
    unsigned char   *endbuf;            /* Pointer to end of buffer */
    int             buflen;             /* Length of ring queue */
    int             maxsize;            /* Maximum size */
    int             increment;          /* Growth increment */
} ringq_t;

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

/******************************************************************************/

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

/******************************************************************************/
/*
    Socket flags 
 */

#if ((defined (WIN) || defined (CE)) && defined (WEBS) && !defined(WIN32))

#ifndef EWOULDBLOCK
#define EWOULDBLOCK             WSAEWOULDBLOCK
#endif
#ifndef ENETDOWN
#define ENETDOWN                WSAENETDOWN
#endif
#ifndef ECONNRESET
#define ECONNRESET              WSAECONNRESET
#endif
#endif /* (WIN || CE) && WEBS) */

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

#define WEBS_MAX_REQUEST        2048        /* Request safeguard max */

#ifdef LITTLEFOOT
#define SOCKET_BUFSIZ           510     /* Underlying buffer size */
#else
#define SOCKET_BUFSIZ           1024    /* Underlying buffer size */
#endif /* LITTLEFOOT */

typedef void    (*socketHandler_t)(int sid, int mask, void* data);
typedef int     (*socketAccept_t)(int sid, char *ipaddr, int port, 
                    int listenSid);
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
} socket_t;

/********************************* Prototypes *********************************/
/*
    Balloc module
 */

extern void      bclose();
extern int       bopen(void *buf, int bufsize, int flags);

/*
    Define NO_BALLOC to turn off our balloc module altogether #define NO_BALLOC 1
 */

#ifdef NO_BALLOC
#define balloc(B_ARGS, num) malloc(num)
#define bfree(B_ARGS, p) free(p)
#define bfreeSafe(B_ARGS, p) \
    if (p) { free(p); } else
#define brealloc(B_ARGS, p, num) realloc(p, num)
extern char_t *bstrdupNoBalloc(char_t *s);
extern char *bstrdupANoBalloc(char *s);
#define bstrdup(B_ARGS, s) bstrdupNoBalloc(s)
#define bstrdupA(B_ARGS, s) bstrdupANoBalloc(s)
#define gstrdup(B_ARGS, s) bstrdupNoBalloc(s)

#else /* BALLOC */

//  MOB - remove
#define balloc(B_ARGS, num) balloc(num)
#define bfree(B_ARGS, p) bfree(p)
#define bfreeSafe(B_ARGS, p) bfreeSafe(p)
#define brealloc(B_ARGS, p, size) brealloc(p, size)
#define bstrdup(B_ARGS, p) bstrdup(p)

#ifdef UNICODE
#define bstrdupA(B_ARGS, p) bstrdupA(p)
#else /* UNICODE */
#define bstrdupA bstrdup
#endif /* UNICODE */

#define gstrdup bstrdup
extern void     *balloc(B_ARGS_DEC, int size);
extern void     bfree(B_ARGS_DEC, void *mp);
extern void     bfreeSafe(B_ARGS_DEC, void *mp);
extern void     *brealloc(B_ARGS_DEC, void *buf, int newsize);
extern char_t   *bstrdup(B_ARGS_DEC, char_t *s);

#ifdef UNICODE
extern char *bstrdupA(B_ARGS_DEC, char *s);
#else /* UNICODE */
#define bstrdupA bstrdup
#endif /* UNICODE */
#endif /* BALLOC */

extern void bstats(int handle, void (*writefn)(int handle, char_t *fmt, ...));

/*
    Flags. The integrity value is used as an arbitrary value to fill the flags.
 */
#define B_USE_MALLOC        0x1             /* Okay to use malloc if required */
#define B_USER_BUF          0x2             /* User supplied buffer for mem */

#ifndef LINUX
extern char_t   *basename(char_t *name);
#endif /* !LINUX */

typedef void    (emfSchedProc)(void *data, int id);
extern int      emfSchedCallback(int delay, emfSchedProc *proc, void *arg);
extern void     emfUnschedCallback(int id);
extern void     emfReschedCallback(int id, int delay);
extern void     emfSchedProcess();
extern int      emfInstGet();
extern void     emfInstSet(int inst);
extern void     error(E_ARGS_DEC, int flags, char_t *fmt, ...);
extern void     (*errorSetHandler(void (*function)(int etype, char_t *msg))) (int etype, char_t *msg);

extern int      hAlloc(void ***map);
extern int      hAllocEntry(void ***list, int *max, int size);

extern int      hFree(void ***map, int handle);

extern int      ringqOpen(ringq_t *rq, int increment, int maxsize);
extern void     ringqClose(ringq_t *rq);
extern int      ringqLen(ringq_t *rq);

extern int      ringqPutc(ringq_t *rq, char_t c);
extern int      ringqInsertc(ringq_t *rq, char_t c);
extern int      ringqPutStr(ringq_t *rq, char_t *str);
extern int      ringqGetc(ringq_t *rq);

extern int      fmtValloc(char_t **s, int n, char_t *fmt, va_list arg);
extern int      fmtAlloc(char_t **s, int n, char_t *fmt, ...);
extern int      fmtStatic(char_t *s, int n, char_t *fmt, ...);

#ifdef UNICODE
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

extern int      ringqPutBlk(ringq_t *rq, unsigned char *buf, int len);
extern int      ringqPutBlkMax(ringq_t *rq);
extern void     ringqPutBlkAdj(ringq_t *rq, int size);
extern int      ringqGetBlk(ringq_t *rq, unsigned char *buf, int len);
extern int      ringqGetBlkMax(ringq_t *rq);
extern void     ringqGetBlkAdj(ringq_t *rq, int size);
extern void     ringqFlush(ringq_t *rq);
extern void     ringqAddNull(ringq_t *rq);

extern int      scriptSetVar(int engine, char_t *var, char_t *value);
extern int      scriptEval(int engine, char_t *cmd, char_t **rslt, void* chan);

extern void     socketClose();
extern void     socketCloseConnection(int sid);
extern void     socketCreateHandler(int sid, int mask, socketHandler_t handler, void* arg);
extern void     socketDeleteHandler(int sid);
extern int      socketEof(int sid);
extern int      socketCanWrite(int sid);
extern void     socketSetBufferSize(int sid, int in, int line, int out);
extern int      socketFlush(int sid);
extern int      socketGets(int sid, char_t **buf);
extern int      socketGetPort(int sid);
extern int      socketInputBuffered(int sid);
extern int      socketOpen();
extern int      socketOpenConnection(char *host, int port, socketAccept_t accept, int flags);
extern void     socketProcess(int hid);
extern int      socketRead(int sid, char *buf, int len);
extern int      socketReady(int hid);
extern int      socketWrite(int sid, char *buf, int len);
extern int      socketWriteString(int sid, char_t *buf);
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
extern int      socketGetInput(int sid, char *buf, int toRead, int *errCode);

extern char_t   *strlower(char_t *string);
extern char_t   *strupper(char_t *string);

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

extern void     trace(int lev, char_t *fmt, ...);
extern void     traceRaw(char_t *buf);
extern void     (*traceSetHandler(void (*function)(int level, char_t *buf))) (int level, char_t *buf);
 
extern value_t  valueInteger(long value);
extern value_t  valueString(char_t *value, int flags);
extern value_t  valueErrmsg(char_t *value);
extern void     valueFree(value_t *v);
extern int      vxchdir(char *dirname);

extern unsigned int hextoi(char_t *hexstring);
extern unsigned int gstrtoi(char_t *s);
extern time_t    timeMsec();

extern char_t   *ascToUni(char_t *ubuf, char *str, int nBytes);
extern char     *uniToAsc(char *buf, char_t *ustr, int nBytes);
extern char_t   *ballocAscToUni(char  *cp, int alen);
extern char     *ballocUniToAsc(char_t *unip, int ulen);

extern char_t   *basicGetHost();
extern char_t   *basicGetAddress();
extern char_t   *basicGetProduct();
extern void     basicSetHost(char_t *host);
extern void     basicSetAddress(char_t *addr);

extern int      harnessOpen(char_t **argv);
extern void     harnessClose(int status);
extern void     harnessTesting(char_t *msg, ...);
extern void     harnessPassed();
extern void     harnessFailed(int line);
extern int      harnessLevel();

#endif /* _h_UEMF */

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
