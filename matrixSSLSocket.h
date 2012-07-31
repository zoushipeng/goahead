/*
    matrixSSLSocket.h - SSL socket layer header for MatrixSSL
  
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/******************************************************************************/

#if BIT_PACK_SSL

#ifndef _h_SSLSOCKET
#define _h_SSLSOCKET

#ifdef __cplusplus
extern "C" {
#endif

#include "matrixssl/matrixsslApi.h"

/*
    Blocking or non-blocking socket model.  Disable for blocking
*/
#define USE_NONBLOCKING_SSL_SOCKETS

#if MACOSX
#define LINUX 1
#define OSX 1
#endif

/*
    OS specific macros
*/
#if WIN32 || WINCE
    #include    <windows.h>
    #include    <winsock.h>
    #define     fcntl(A, B, C)
    #define     MSG_NOSIGNAL    0
    #define     MSG_DONTWAIT    0
    #define     WOULD_BLOCK WSAEWOULDBLOCK
    #define     getSocketError()  WSAGetLastError()
#elif LINUX
    #include    <sys/socket.h>
    #include    <netinet/in.h>
    #include    <netinet/tcp.h>
    #include    <arpa/inet.h>
#ifdef OSX
    #include    <sys/socket.h>
    #define     MSG_NOSIGNAL    0
#endif /* OSX */
    #include    <fcntl.h>
    #include    <unistd.h>
    #include    <string.h>
    #include    <errno.h>
    #define     SOCKET_ERROR -1
    #define     getSocketError() errno 
    #define     WOULD_BLOCK EAGAIN
    #define     closesocket close
    #define     MAKEWORD(A, B)
    #define     WSAStartup(A, B)
    #define     WSACleanup()
    #define     INVALID_SOCKET -1
    typedef int WSADATA;
    typedef int SOCKET;
#elif VXWORKS
    #include    <types.h>
    #include    <socket.h>
    #include    <netinet/in.h>
    #include    <netinet/tcp.h>
    #include    <fcntl.h>
    #include    <errno.h>
    #define     fcntl(A, B, C) ioctl(A, B, C)
    #define     SOCKET_ERROR -1
    #define     getSocketError() errno 
    #define     WOULD_BLOCK EAGAIN
    #define     closesocket close
    #define     MAKEWORD(A, B)
    #define     WSAStartup(A, B)
    #define     WSACleanup()
    #define     INVALID_SOCKET -1
    #define     MSG_NOSIGNAL    0
    typedef int WSADATA;
    typedef int SOCKET;
#endif /* OS macros */

#if MACOSX
#undef LINUX
#undef OSX
#endif

#define sslAssert(C) if (C) ; else {fprintf(stderr, "%s:%d sslAssert(%s)\n",\
                        __FILE__, __LINE__, #C); }
#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif /* min */

/*
    sslRead and sslWrite status values
*/
#define SSLSOCKET_EOF           0x1
#define SSLSOCKET_CLOSE_NOTIFY  0x2

/*
    Connection structure
*/
typedef struct {
    ssl_t               *ssl;
    char                *pt;        /* app data start */
    char                *currPt;    /* app data current location */
    int                 ptBytes;    /* plaintext bytes remaining */
    SOCKET              fd;
    int                 ptReqBytes;
    int                 sendBlocked;
} sslConn_t;

/*
    Secure Socket apis
*/
extern int  sslAccept(sslConn_t **cp, SOCKET fd, sslKeys_t *keys, int32 resume,
                int (*certValidator)(ssl_t *, psX509Cert_t *, int32));
extern void sslFreeConnection(sslConn_t **cp);

extern int  sslRead(sslConn_t *cp, char *buf, int len);
extern int  sslWrite(sslConn_t *cp, char *buf, int len);
extern void sslWriteClosureAlert(sslConn_t *cp);

#ifdef __cplusplus
}
#endif

#endif /* _h_SSLSOCKET */

#endif /* WEBS_SSL_SUPPORT */

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
