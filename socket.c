/*
    socket.c -- Sockets layer

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "goahead.h"

/************************************ Locals **********************************/

socket_t    **socketList;           /* List of open sockets */
int         socketMax;              /* Maximum size of socket */
int         socketHighestFd = -1;   /* Highest socket fd opened */
int         socketOpenCount = 0;    /* Number of task using sockets */

/***************************** Forward Declarations ***************************/

static void socketAccept(socket_t *sp);
static int  socketDoEvent(socket_t *sp);
static ssize socketDoOutput(socket_t *sp, char *buf, ssize toWrite, int *errCode);
static ssize tryAlternateSendTo(int sock, char *buf, ssize toWrite, int i, struct sockaddr *server);
static int  tryAlternateConnect(int sock, struct sockaddr *sockaddr);

/*********************************** Code *************************************/

int socketOpen()
{
#if BIT_WIN_LIKE
    WSADATA     wsaData;
#endif

    if (++socketOpenCount > 1) {
        return 0;
    }

#if BIT_WIN_LIKE
    if (WSAStartup(MAKEWORD(1,1), &wsaData) != 0) {
        return -1;
    }
    if (wsaData.wVersion != MAKEWORD(1,1)) {
        WSACleanup();
        return -1;
    }
#endif

    socketList = NULL;
    socketMax = 0;
    socketHighestFd = -1;
    return 0;
}


void socketClose()
{
    int     i;

    if (--socketOpenCount <= 0) {
        for (i = socketMax; i >= 0; i--) {
            if (socketList && socketList[i]) {
                socketCloseConnection(i);
            }
        }
        socketOpenCount = 0;
    }
}


/*
    Open a client or server socket. Host is NULL if we want server capability.
 */
int socketOpenConnection(char *host, int port, socketAccept_t accept, int flags)
{
    //  MOB - refactor
#if !NO_GETHOSTBYNAME && !VXWORKS
    struct hostent      *hostent;                   /* Host database entry */
#endif
    socket_t            *sp;
    struct sockaddr_in  sockaddr;
    int                 sid, bcast, dgram, rc;

    if (port > SOCKET_PORT_MAX) {
        return -1;
    }
    if ((sid = socketAlloc(host, port, accept, flags)) < 0) {
        return -1;
    }
    sp = socketList[sid];
    a_assert(sp);

    memset((char *) &sockaddr, '\0', sizeof(struct sockaddr_in));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons((short) (port & 0xFFFF));

    if (host == NULL) {
        sockaddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        sockaddr.sin_addr.s_addr = inet_addr(host);
        if (sockaddr.sin_addr.s_addr == INADDR_NONE) {
            /*
                If the OS does not support gethostbyname functionality, the macro: NO_GETHOSTBYNAME should be defined to
                skip the use of gethostbyname. Unfortunatly there is no easy way to recover, the following code simply
                uses the basicGetHost IP for the sockaddr.  
             */
#if NO_GETHOSTBYNAME
            if (strcmp(host, basicGetHost()) == 0) {
                sockaddr.sin_addr.s_addr = inet_addr("localhost");
            }
            if (sockaddr.sin_addr.s_addr == INADDR_NONE) {
                socketFree(sid);
                return -1;
            }
#elif VXWORKS
            //  MOB - check what appweb does for this?
            sockaddr.sin_addr.s_addr = (ulong) hostGetByName(host);
            if (sockaddr.sin_addr.s_addr == NULL) {
                errno = ENXIO;
                socketFree(sid);
                return -1;
            }
#else
            hostent = gethostbyname(host);
            if (hostent != NULL) {
                memcpy((char *) &sockaddr.sin_addr, (char *) hostent->h_addr_list[0], (size_t) hostent->h_length);
            } else {
                char    *asciiAddress;
                char_t  *address;
                address = "localhost";
                asciiAddress = ballocUniToAsc(address, gstrlen(address));
                sockaddr.sin_addr.s_addr = inet_addr(asciiAddress);
                bfree(asciiAddress);
                if (sockaddr.sin_addr.s_addr == INADDR_NONE) {
                    errno = ENXIO;
                    socketFree(sid);
                    return -1;
                }
            }
#endif
        }
    }
    bcast = sp->flags & SOCKET_BROADCAST;
    if (bcast) {
        sp->flags |= SOCKET_DATAGRAM;
    }
    dgram = sp->flags & SOCKET_DATAGRAM;

    /*
        Create the socket. Support for datagram sockets. Set the close on exec flag so children don't inherit the socket.
     */
    sp->sock = socket(AF_INET, dgram ? SOCK_DGRAM: SOCK_STREAM, 0);
    if (sp->sock < 0) {
        socketFree(sid);
        return -1;
    }
#if BIT_HAS_FCNTL
    fcntl(sp->sock, F_SETFD, FD_CLOEXEC);
#endif
    socketHighestFd = max(socketHighestFd, sp->sock);

    /*
        If broadcast, we need to turn on broadcast capability.
     */
    if (bcast) {
        int broadcastFlag = 1;
        if (setsockopt(sp->sock, SOL_SOCKET, SO_BROADCAST,
                (char *) &broadcastFlag, sizeof(broadcastFlag)) < 0) {
            socketFree(sid);
            return -1;
        }
    }

    /*
        Host is set if we are the client
     */
    if (host) {
        /*
            Connect to the remote server in blocking mode, then go into non-blocking mode if desired.
         */
        if (!dgram) {
            if (! (sp->flags & SOCKET_BLOCK)) {
                /*
                    sockGen.c is only used for Windows products when blocking connects are expected.  This applies to
                    webserver connectws.  Therefore the asynchronous connect code here is not compiled.
                 */
#if BIT_WIN_LIKE
                int flag;
                sp->flags |= SOCKET_ASYNC;
                /*
                    Set to non-blocking for an async connect
                 */
                flag = 1;
                if (ioctlsocket(sp->sock, FIONBIO, &flag) == SOCKET_ERROR) {
                    socketFree(sid);
                    return -1;
                }
#else
                socketSetBlock(sid, 1);
#endif

            }
            if ((rc = connect(sp->sock, (struct sockaddr *) &sockaddr, sizeof(sockaddr))) < 0 && 
                (rc = tryAlternateConnect(sp->sock, (struct sockaddr *) &sockaddr)) < 0) {
#if BIT_WIN_LIKE
                if (socketGetError() != EWOULDBLOCK) {
                    socketFree(sid);
                    return -1;
                }
#else
                socketFree(sid);
                return -1;

#endif

            }
        }
    } else {
        /*
            Bind to the socket endpoint and the call listen() to start listening
         */
        rc = 1;
        setsockopt(sp->sock, SOL_SOCKET, SO_REUSEADDR, (char *)&rc, sizeof(rc));
        if (bind(sp->sock, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0) {
            socketFree(sid);
            return -1;
        }
        if (! dgram) {
            if (listen(sp->sock, SOMAXCONN) < 0) {
                socketFree(sid);
                return -1;
            }
            sp->flags |= SOCKET_LISTENING;
        }
        sp->handlerMask |= SOCKET_READABLE;
    }

    /*
        Set the blocking mode
     */
    if (flags & SOCKET_BLOCK) {
        socketSetBlock(sid, 1);
    } else {
        socketSetBlock(sid, 0);
    }
    return sid;
}


/*
    If the connection failed, swap the first two bytes in the sockaddr structure.  This is a kludge due to a change in
    VxWorks between versions 5.3 and 5.4, but we want the product to run on either.
 */
static int tryAlternateConnect(int sock, struct sockaddr *sockaddr)
{
#if VXWORKS
    char *ptr;

    ptr = (char *)sockaddr;
    *ptr = *(ptr+1);
    *(ptr+1) = 0;
    return connect(sock, sockaddr, sizeof(struct sockaddr));
#else
    return -1;
#endif /* VXWORKS */
}


void socketCloseConnection(int sid)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    socketFree(sid);
}


/*
    Accept a connection. Called as a callback on incoming connection.
 */
static void socketAccept(socket_t *sp)
{
    struct sockaddr_in  addr;
    socket_t            *nsp;
    size_t              len;
    char                *ip;
    int                 newSock, nid;

    a_assert(sp);

    /*
        Accept the connection and prevent inheriting by children (F_SETFD)
     */
    len = sizeof(struct sockaddr_in);
    if ((newSock = accept(sp->sock, (struct sockaddr*) &addr, (socklen_t*) &len)) < 0) {
        return;
    }
#if BIT_HAS_FCNTL
    fcntl(newSock, F_SETFD, FD_CLOEXEC);
#endif
    socketHighestFd = max(socketHighestFd, newSock);

    /*
        Create a socket structure and insert into the socket list
     */
    nid = socketAlloc(sp->host, sp->port, sp->accept, sp->flags);
    nsp = socketList[nid];
    a_assert(nsp);
    nsp->sock = newSock;
    nsp->flags &= ~SOCKET_LISTENING;

    if (nsp == NULL) {
        return;
    }
    /*
        Set the blocking mode before calling the accept callback.
     */

    socketSetBlock(nid, (nsp->flags & SOCKET_BLOCK) ? 1: 0);
    /*
        Call the user accept callback. The user must call socketCreateHandler to register for further events of interest.
     */
    if (sp->accept != NULL) {
        ip = inet_ntoa(addr.sin_addr);
        if ((sp->accept)(nid, ip, ntohs(addr.sin_port), sp->sid) < 0) {
            socketFree(nid);
        }
#if VXWORKS
        free(ip);
#endif
    }
}


/*
    Get more from the socket and return in buf. Returns 0 for EOF, -1 for errors and otherwise the number of bytes read.  
 */
ssize socketGetInput(int sid, char *buf, ssize toRead, int *errCode)
{
    struct sockaddr_in  server;
    socket_t            *sp;
    ssize               len, bytesRead;

    a_assert(buf);
    a_assert(errCode);

    *errCode = 0;

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }

    /*
        If we have previously seen an EOF condition, then just return
     */
    if (sp->flags & SOCKET_EOF) {
        return 0;
    }
#if BIT_WIN_LIKE
    if (!(sp->flags & SOCKET_BLOCK) && !socketWaitForEvent(sp,  FD_CONNECT, errCode)) {
        return -1;
    }
#endif

    /*
        Read the data
     */
    if (sp->flags & SOCKET_DATAGRAM) {
        len = sizeof(server);
        bytesRead = recvfrom(sp->sock, buf, toRead, 0, (struct sockaddr *) &server, (socklen_t *) &len);
    } else {
        bytesRead = recv(sp->sock, buf, toRead, 0);
    }
    if (bytesRead < 0) {
        *errCode = socketGetError();
        if (*errCode == ECONNRESET) {
            sp->flags |= SOCKET_CONNRESET;
            return 0;
        }
        return -1;
    }
    return bytesRead;
}


void socketRegisterInterest(socket_t *sp, int handlerMask)
{
    a_assert(sp);
    sp->handlerMask = handlerMask;
}


/*
    Wait until an event occurs on a socket. Return 1 on success, 0 on failure. or -1 on exception
 */
int socketWaitForEvent(socket_t *sp, int handlerMask, int *errCode)
{
    int mask;

    a_assert(sp);

    mask = sp->handlerMask;
    sp->handlerMask |= handlerMask;
    while (socketSelect(sp->sid, 1000)) {
        if (sp->currentEvents & (handlerMask | SOCKET_EXCEPTION)) {
            break;
        }
    }
    sp->handlerMask = mask;
    if (sp->currentEvents & SOCKET_EXCEPTION) {
        return -1;
    } else if (sp->currentEvents & handlerMask) {
        return 1;
    }
    if (errCode) {
        *errCode = errno = EWOULDBLOCK;
    }
    return 0;
}


/*
    Return 1 if there is a socket with an event ready to process,
 */
int socketReady(int sid)
{
    socket_t    *sp;
    int         all;

    all = 0;
    if (sid < 0) {
        sid = 0;
        all = 1;
    }
    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            if (! all) {
                break;
            } else {
                continue;
            }
        } 
        if (sp->flags & SOCKET_CONNRESET) {
            socketCloseConnection(sid);
            return 0;
        }
        if (sp->currentEvents & sp->handlerMask) {
            return 1;
        }
        /*
            If there is input data, also call select to test for new events
         */
        if (sp->handlerMask & SOCKET_READABLE && socketInputBuffered(sid) > 0) {
            socketSelect(sid, 0);
            return 1;
        }
        if (! all) {
            break;
        }
    }
    return 0;
}


/*
    Wait for a handle to become readable or writable and return a number of noticed events. Timeout is in milliseconds.
 */
#if BLD_WIN_LIKE

int socketSelect(int sid, int timeout)
{
    struct timeval  tv;
    socket_t        *sp;
    fd_set          readFds, writeFds, exceptFds;
    int             nEvents;
    int             all, socketHighestFd;   /* Highest socket fd opened */

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);
    socketHighestFd = -1;

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    /*
        Set the select event masks for events to watch
     */
    all = nEvents = 0;

    if (sid < 0) {
        all++;
        sid = 0;
    }

    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            continue;
        }
        a_assert(sp);
        /*
                Set the appropriate bit in the ready masks for the sp->sock.
         */
        if (sp->handlerMask & SOCKET_READABLE) {
            FD_SET(sp->sock, &readFds);
            nEvents++;
            if (socketInputBuffered(sid) > 0) {
                tv.tv_sec = 0;
                tv.tv_usec = 0;
            }
        }
        if (sp->handlerMask & SOCKET_WRITABLE) {
            FD_SET(sp->sock, &writeFds);
            nEvents++;
        }
        if (sp->handlerMask & SOCKET_EXCEPTION) {
            FD_SET(sp->sock, &exceptFds);
            nEvents++;
        }
        if (! all) {
            break;
        }
    }

    /*
        Windows select() fails if no descriptors are set, instead of just sleeping like other, nice select() calls. So,
        if WIN, sleep.  
     */
    if (nEvents == 0) {
        Sleep(timeout);
        return 0;
    }

    /*
        Wait for the event or a timeout.
     */
    nEvents = select(socketHighestFd+1, &readFds, &writeFds, &exceptFds, &tv);

    if (all) {
        sid = 0;
    }
    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            continue;
        }

        if (FD_ISSET(sp->sock, &readFds) || socketInputBuffered(sid) > 0) {
                sp->currentEvents |= SOCKET_READABLE;
        }
        if (FD_ISSET(sp->sock, &writeFds)) {
                sp->currentEvents |= SOCKET_WRITABLE;
        }
        if (FD_ISSET(sp->sock, &exceptFds)) {
                sp->currentEvents |= SOCKET_EXCEPTION;
        }
        if (! all) {
            break;
        }
    }

    return nEvents;
}

#else /* !BIT_WIN_LIKE */

int socketSelect(int sid, int timeout)
{
    socket_t        *sp;
    struct timeval  tv;
    fd_mask         *readFds, *writeFds, *exceptFds;
    int             all, len, nwords, index, bit, nEvents;

    /*
        Allocate and zero the select masks
     */
    nwords = (socketHighestFd + NFDBITS) / NFDBITS;
    len = nwords * sizeof(fd_mask);

    readFds = balloc(len);
    memset(readFds, 0, len);
    writeFds = balloc(len);
    memset(writeFds, 0, len);
    exceptFds = balloc(len);
    memset(exceptFds, 0, len);

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    /*
        Set the select event masks for events to watch
     */
    all = nEvents = 0;

    if (sid < 0) {
        all++;
        sid = 0;
    }

    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            if (all == 0) {
                break;
            } else {
                continue;
            }
        }
        a_assert(sp);

        /*
                Initialize the ready masks and compute the mask offsets.
         */
        index = sp->sock / (NBBY * sizeof(fd_mask));
        bit = 1 << (sp->sock % (NBBY * sizeof(fd_mask)));
        
        /*
            Set the appropriate bit in the ready masks for the sp->sock.
         */
        if (sp->handlerMask & SOCKET_READABLE) {
            readFds[index] |= bit;
            nEvents++;
            if (socketInputBuffered(sid) > 0) {
                tv.tv_sec = 0;
                tv.tv_usec = 0;
            }
        }
        if (sp->handlerMask & SOCKET_WRITABLE) {
            writeFds[index] |= bit;
            nEvents++;
        }
        if (sp->handlerMask & SOCKET_EXCEPTION) {
            exceptFds[index] |= bit;
            nEvents++;
        }
        if (! all) {
            break;
        }
    }

    /*
        Wait for the event or a timeout. Reset nEvents to be the number of actual events now.
     */
    nEvents = select(socketHighestFd + 1, (fd_set *) readFds, (fd_set *) writeFds, (fd_set *) exceptFds, &tv);

    if (nEvents > 0) {
        if (all) {
            sid = 0;
        }
        for (; sid < socketMax; sid++) {
            if ((sp = socketList[sid]) == NULL) {
                if (all == 0) {
                    break;
                } else {
                    continue;
                }
            }
            index = sp->sock / (NBBY * sizeof(fd_mask));
            bit = 1 << (sp->sock % (NBBY * sizeof(fd_mask)));

            if (readFds[index] & bit || socketInputBuffered(sid) > 0) {
                sp->currentEvents |= SOCKET_READABLE;
            }
            if (writeFds[index] & bit) {
                sp->currentEvents |= SOCKET_WRITABLE;
            }
            if (exceptFds[index] & bit) {
                sp->currentEvents |= SOCKET_EXCEPTION;
            }
            if (! all) {
                break;
            }
        }
    }
    bfree(readFds);
    bfree(writeFds);
    bfree(exceptFds);
    return nEvents;
}
#endif /* WIN || CE */


void socketProcess(int sid)
{
    socket_t    *sp;
    int         all;

    all = 0;
    if (sid < 0) {
        all = 1;
        sid = 0;
    }
    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            if (! all) {
                break;
            } else {
                continue;
            }
        }
        if (socketReady(sid)) {
            socketDoEvent(sp);
        }
        if (! all) {
            break;
        }
    }
}


static int socketDoEvent(socket_t *sp)
{
    ringq_t     *rq;
    int         sid;

    a_assert(sp);

    sid = sp->sid;
    if (sp->currentEvents & SOCKET_READABLE) {
        if (sp->flags & SOCKET_LISTENING) { 
            socketAccept(sp);
            sp->currentEvents = 0;
            return 1;
        } 

    } else {
        /*
             If there is still read data in the buffers, trigger the read handler
             NOTE: this may busy spin if the read handler doesn't read the data
         */
        if (sp->handlerMask & SOCKET_READABLE && socketInputBuffered(sid) > 0) {
            sp->currentEvents |= SOCKET_READABLE;
        }
    }
    /*
        If now writable and flushing in the background, continue flushing
     */
    if (sp->currentEvents & SOCKET_WRITABLE) {
        if (sp->flags & SOCKET_FLUSHING) {
            rq = &sp->outBuf;
            if (ringqLen(rq) > 0) {
                socketFlush(sp->sid);
            } else {
                sp->flags &= ~SOCKET_FLUSHING;
            }
        }
    }

    /*
        Now invoke the users socket handler. NOTE: the handler may delete the
        socket, so we must be very careful after calling the handler.
     */
    if (sp->handler && (sp->handlerMask & sp->currentEvents)) {
        (sp->handler)(sid, sp->handlerMask & sp->currentEvents, sp->handler_data);
        /*
            Make sure socket pointer is still valid, then reset the currentEvents.
         */ 
        if (socketList && sid < socketMax && socketList[sid] == sp) {
            sp->currentEvents = 0;
        }
    }
    return 1;
}


/*
    Set the socket blocking mode
 */
int socketSetBlock(int sid, int on)
{
    socket_t        *sp;
    ulong           flag;
    int             iflag;
    int             oldBlock;

    flag = iflag = !on;

    if ((sp = socketPtr(sid)) == NULL) {
        a_assert(0);
        return 0;
    }
    oldBlock = (sp->flags & SOCKET_BLOCK);
    sp->flags &= ~(SOCKET_BLOCK);
    if (on) {
        sp->flags |= SOCKET_BLOCK;
    }
    /*
        Put the socket into block / non-blocking mode
     */
    if (sp->flags & SOCKET_BLOCK) {
#if BIT_WIN_LIKE
        ioctlsocket(sp->sock, FIONBIO, &flag);
#elif ECOS
        int off;
        off = 0;
        ioctl(sp->sock, FIONBIO, &off);
#elif VXWORKS
        ioctl(sp->sock, FIONBIO, (int)&iflag);
#else
        fcntl(sp->sock, F_SETFL, fcntl(sp->sock, F_GETFL) & ~O_NONBLOCK);
#endif

    } else {
#if BIT_WIN_LIKE
        ioctlsocket(sp->sock, FIONBIO, &flag);
#elif ECOS
        int on;
        on = 1;
        ioctl(sp->sock, FIONBIO, &on);
#elif VXWORKS
        ioctl(sp->sock, FIONBIO, (int)&iflag);
#else
        fcntl(sp->sock, F_SETFL, fcntl(sp->sock, F_GETFL) | O_NONBLOCK);
#endif
    }
    /* Prevent SIGPIPE when writing to closed socket on OS X */
#if MACOSX
    iflag = 1;
    setsockopt(sp->sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&iflag, sizeof(iflag));
#endif
    return oldBlock;
}


/*
    Return true if a readable socket has buffered data. - not public
 */
int socketDontBlock()
{
    socket_t    *sp;
    int         i;

    for (i = 0; i < socketMax; i++) {
        if ((sp = socketList[i]) == NULL || (sp->handlerMask & SOCKET_READABLE) == 0) {
            continue;
        }
        if (socketInputBuffered(i) > 0) {
            return 1;
        }
    }
    return 0;
}


/*
    Return true if a particular socket buffered data. - not public
 */
ssize socketSockBuffered(int sock)
{
    socket_t    *sp;
    int         i;

    for (i = 0; i < socketMax; i++) {
        if ((sp = socketList[i]) == NULL || sp->sock != sock) {
            continue;
        }
        return socketInputBuffered(i);
    }
    return 0;
}


/*
    Write to a socket. Absorb as much data as the socket can buffer. Block if the socket is in blocking mode. Returns -1
    on error, otherwise the number of bytes written.
 */
ssize socketWrite(int sid, char *buf, ssize bufsize)
{
    socket_t    *sp;
    ringq_t     *rq;
    ssize       len, bytesWritten, room;

    a_assert(buf);
    a_assert(bufsize >= 0);

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    /*
        Loop adding as much data to the output ringq as we can absorb. Initiate a flush when the ringq is too full and
        continue. Block in socketFlush if the socket is in blocking mode.
     */
    rq = &sp->outBuf;
    for (bytesWritten = 0; bufsize > 0; ) {
        if ((room = ringqPutBlkMax(rq)) == 0) {
            if (socketFlush(sid) < 0) {
                return -1;
            }
            if ((room = ringqPutBlkMax(rq)) == 0) {
                if (sp->flags & SOCKET_BLOCK) {
#if BIT_WIN_LIKE
                    int     errCode;
                    if (! socketWaitForEvent(sp,  FD_WRITE | SOCKET_WRITABLE,
                        &errCode)) {
                        return -1;
                    }
#endif
                    continue;
                }
                break;
            }
            continue;
        }
        len = min(room, bufsize);
        ringqPutBlk(rq, (uchar *) buf, len);
        bytesWritten += len;
        bufsize -= len;
        buf += len;
    }
    return bytesWritten;
}


/*
    Write a string to a socket
 */
ssize socketWriteString(int sid, char_t *buf)
{
 #if UNICODE
    char    *byteBuf;
    ssize   r, len;
 
    len = gstrlen(buf);
    byteBuf = ballocUniToAsc(buf, len);
    r = socketWrite(sid, byteBuf, len);
    bfree(byteBuf);
    return r;
 #else
    return socketWrite(sid, buf, strlen(buf));
 #endif /* UNICODE */
}


/*
    Read from a socket. Return the number of bytes read if successful. This may be less than the requested "bufsize" and
    may be zero. Return -1 for errors. Return 0 for EOF. Otherwise return the number of bytes read.  If this routine
    returns zero it indicates an EOF condition.  which can be verified with socketEof()
 
    Note: this ignores the line buffer, so a previous socketGets which read a partial line may cause a subsequent
    socketRead to miss some data. This routine may block if the socket is in blocking mode.
 */
ssize socketRead(int sid, char *buf, ssize bufsize)
{
    socket_t    *sp;
    ringq_t     *rq;
    ssize       len, room, bytesRead;
    int         errCode;

    a_assert(buf);
    a_assert(bufsize > 0);

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    if (sp->flags & SOCKET_EOF) {
        return 0;
    }
    rq = &sp->inBuf;
    for (bytesRead = 0; bufsize > 0; ) {
        len = min(ringqLen(rq), bufsize);
        if (len <= 0) {
            /*
                If blocking mode and already have data, exit now or it may block forever.
             */
            if ((sp->flags & SOCKET_BLOCK) &&
                (bytesRead > 0)) {
                break;
            }
            /*
                This flush is critical for readers of datagram packets. If the buffer is not big enough to read the
                whole datagram in one hit, the recvfrom call will fail. 
             */
            ringqFlush(rq);
            room = ringqPutBlkMax(rq);
            len = socketGetInput(sid, (char *) rq->endp, room, &errCode);
            if (len < 0) {
                if (errCode == EWOULDBLOCK) {
                    if ((sp->flags & SOCKET_BLOCK) &&
                        (bytesRead ==  0)) {
                        continue;
                    }
                    if (bytesRead >= 0) {
                        return bytesRead;
                    }
                }
                return -1;

            } else if (len == 0) {
                /*
                    If bytesRead is 0, this is EOF since socketRead should never be called unless there is data yet to
                    be read. Set the flag. Then pass back the number of bytes read.
                 */
                if (bytesRead == 0) {
                    sp->flags |= SOCKET_EOF;
                }
                return bytesRead;
            }
            ringqPutBlkAdj(rq, len);
            len = min(len, bufsize);
        }
        memcpy(&buf[bytesRead], rq->servp, len);
        ringqGetBlkAdj(rq, len);
        bufsize -= len;
        bytesRead += len;
    }
    return bytesRead;
}


/*
    Get a string from a socket. This returns data in *buf in a malloced string after trimming the '\n'. If there is zero
    bytes returned, *buf will be set to NULL. If doing non-blocking I/O, it returns -1 for error, EOF or when no
    complete line yet read. If doing blocking I/O, it will block until an entire line is read. If a partial line is read
    socketInputBuffered or socketEof can be used to distinguish between EOF and partial line still buffered. This
    routine eats and ignores carriage returns.
 */
ssize socketGets(int sid, char_t **buf)
{
    socket_t    *sp;
    ringq_t     *lq;
    char        c;
    ssize       rc, len;

    a_assert(buf);
    *buf = NULL;
    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    lq = &sp->lineBuf;

    while (1) {
        if ((rc = socketRead(sid, &c, 1)) < 0) {
            return rc;
        }
        if (rc == 0) {
            /*
                If there is a partial line and we are at EOF, pretend we saw a '\n'
             */
            if (ringqLen(lq) > 0 && (sp->flags & SOCKET_EOF)) {
                c = '\n';
            } else {
                return -1;
            }
        }
        /* 
            Validate length of request.  Ignore long strings without newlines to safeguard against long URL attacks.
         */
        if (ringqLen(lq) > E_MAX_REQUEST) {
            c = '\n';
        }
        /*
            If a newline is seen, return the data excluding the new line to the caller. If carriage return is seen, just
            eat it.  
         */
        if (c == '\n') {
            len = ringqLen(lq);
            if (len > 0) {
                *buf = ballocAscToUni((char *)lq->servp, len);
            } else {
                *buf = NULL;
            }
            ringqFlush(lq);
            return len;

        } else if (c == '\r') {
            continue;
        }
        ringqPutcA(lq, c);
    }
    return 0;
}


/*
    Flush the socket. Block if the socket is in blocking mode.  This will return -1 on errors and 0 if successful.
 */
int socketFlush(int sid)
{
    socket_t    *sp;
    ringq_t     *rq;
    ssize       len, bytesWritten;
    int         errCode;

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    rq = &sp->outBuf;

    /*
        Set the background flushing flag which socketEventProc will check to continue the flush.
     */
    if (! (sp->flags & SOCKET_BLOCK)) {
        sp->flags |= SOCKET_FLUSHING;
    }

    /*
        Break from loop if not blocking after initiating output. If we are blocking we wait for a write event.
     */
    while (ringqLen(rq) > 0) {
        len = ringqGetBlkMax(&sp->outBuf);
        bytesWritten = socketDoOutput(sp, (char*) rq->servp, len, &errCode);
        if (bytesWritten < 0) {
            if (errCode == EINTR) {
                continue;
            } else if (errCode == EWOULDBLOCK || errCode == EAGAIN) {
#if BIT_WIN_LIKE
                if (sp->flags & SOCKET_BLOCK) {
                    int     errCode;
                    if (! socketWaitForEvent(sp,  FD_WRITE | SOCKET_WRITABLE,
                        &errCode)) {
                        return -1;
                    }
                    continue;
                } 
#endif
                /*
                    Ensure we get a FD_WRITE message when the socket can absorb more data (non-blocking only.) Store the
                    user's mask if we haven't done it already.
                 */
                if (sp->saveMask < 0 ) {
                    sp->saveMask = sp->handlerMask;
                    socketRegisterInterest(sp, 
                    sp->handlerMask | SOCKET_WRITABLE);
                }
                return 0;
            }
            return -1;
        }
        ringqGetBlkAdj(rq, bytesWritten);
    }
    /*
        If the buffer is empty, reset the ringq pointers to point to the start of the buffer. This is essential to
        ensure that datagrams get written in one single I/O operation.  
     */
    if (ringqLen(rq) == 0) {
        ringqFlush(rq);
    }
    /*
        Restore the users mask if it was saved by the non-blocking code above. Note: saveMask = -1 if empty.
        socketRegisterInterest will set handlerMask 
     */
    if (sp->saveMask >= 0) {
        socketRegisterInterest(sp, sp->saveMask);
        sp->saveMask = -1;
    }
    sp->flags &= ~SOCKET_FLUSHING;
    return 0;
}


/*
    Return the count of input characters buffered. We look at both the line
    buffer and the input (raw) buffer. Return -1 on error or EOF.
 */
ssize socketInputBuffered(int sid)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    if (socketEof(sid)) {
        return -1;
    }
    return ringqLen(&sp->inBuf);
}


/*
    Return true if EOF
 */
int socketEof(int sid)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    return sp->flags & SOCKET_EOF;
}


/*
    Return the number of bytes the socket can absorb without blocking
 */
ssize socketCanWrite(int sid)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    return sp->outBuf.buflen - ringqLen(&sp->outBuf) - 1;
}


/*
    Add one to allow the user to write exactly SOCKET_BUFSIZ
 */
void socketSetBufferSize(int sid, int in, int line, int out)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    if (in >= 0) {
        ringqClose(&sp->inBuf);
        in++;
        ringqOpen(&sp->inBuf, in, in);
    }
    if (line >= 0) {
        ringqClose(&sp->lineBuf);
        line++;
        ringqOpen(&sp->lineBuf, line, line);
    }
    if (out >= 0) {
        ringqClose(&sp->outBuf);
        out++;
        ringqOpen(&sp->outBuf, out, out);
    }
}


/*
    Create a user handler for this socket. The handler called whenever there
    is an event of interest as defined by handlerMask (SOCKET_READABLE, ...)
 */
void socketCreateHandler(int sid, int handlerMask, socketHandler_t handler, void* data)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    sp->handler = handler;
    sp->handler_data = data;
    socketRegisterInterest(sp, handlerMask);
}


void socketDeleteHandler(int sid)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    sp->handler = NULL;
    socketRegisterInterest(sp, 0);
}


/*
    Socket output procedure. Return -1 on errors otherwise return the number of bytes written.
 */
static ssize socketDoOutput(socket_t *sp, char *buf, ssize toWrite, int *errCode)
{
    struct sockaddr_in  server;
    ssize               bytes;

    a_assert(sp);
    a_assert(buf);
    a_assert(toWrite > 0);
    a_assert(errCode);

    *errCode = 0;

#if BIT_WIN_LIKE
    if ((sp->flags & SOCKET_ASYNC)
            && ! socketWaitForEvent(sp,  FD_CONNECT, errCode)) {
        return -1;
    }
#endif

    /*
        Write the data
     */
    if (sp->flags & SOCKET_BROADCAST) {
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_BROADCAST;
        server.sin_port = htons((short)(sp->port & 0xFFFF));
        if ((bytes = sendto(sp->sock, buf, toWrite, 0, (struct sockaddr *) &server, sizeof(server))) < 0) {
            bytes = tryAlternateSendTo(sp->sock, buf, toWrite, 0, (struct sockaddr *) &server);
        }
    } else if (sp->flags & SOCKET_DATAGRAM) {
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(sp->host);
        server.sin_port = htons((short)(sp->port & 0xFFFF));
        bytes = sendto(sp->sock, buf, toWrite, 0, (struct sockaddr *) &server, sizeof(server));
    } else {
        bytes = send(sp->sock, buf, toWrite, 0);
    }
    if (bytes < 0) {
        *errCode = socketGetError();
#if BIT_WIN_LIKE
        sp->currentEvents &= ~FD_WRITE;
#endif
        return -1;
    } else if (bytes == 0 && bytes != toWrite) {
        *errCode = EWOULDBLOCK;
#if BIT_WIN_LIKE
        sp->currentEvents &= ~FD_WRITE;
#endif
        return -1;
    }

    /*
        Ensure we get to write some more data real soon if the socket can absorb more data
     */
    return bytes;
}


/*
    If the sendto failed, swap the first two bytes in the sockaddr structure.  This is a kludge due to a change in
    VxWorks between versions 5.3 and 5.4, but we want the product to run on either.
 */
static ssize tryAlternateSendTo(int sock, char *buf, ssize toWrite, int i, struct sockaddr *server)
{
#if VXWORKS
    char *ptr;

    ptr = (char *)server;
    *ptr = *(ptr+1);
    *(ptr+1) = 0;
    return sendto(sock, buf, toWrite, i, server, sizeof(struct sockaddr));
#else
    return -1;
#endif /* VXWORKS */
}


/*
    Allocate a new socket structure
 */
int socketAlloc(char *host, int port, socketAccept_t accept, int flags)
{
    socket_t    *sp;
    int         sid;

    if ((sid = hAllocEntry((void***) &socketList, &socketMax, sizeof(socket_t))) < 0) {
        return -1;
    }
    sp = socketList[sid];
    sp->sid = sid;
    sp->accept = accept;
    sp->port = port;
    sp->fileHandle = -1;
    sp->saveMask = -1;

    if (host) {
        strncpy(sp->host, host, sizeof(sp->host));
    }

    /*
        Preserve only specified flags from the callers open
     */
    a_assert((flags & ~(SOCKET_BROADCAST|SOCKET_DATAGRAM|SOCKET_BLOCK| SOCKET_LISTENING)) == 0);
    sp->flags = flags & (SOCKET_BROADCAST | SOCKET_DATAGRAM | SOCKET_BLOCK | SOCKET_LISTENING | SOCKET_MYOWNBUFFERS);

    if (!(flags & SOCKET_MYOWNBUFFERS)) { 
        /*
            Add one to allow the user to write exactly SOCKET_BUFSIZ
         */
        ringqOpen(&sp->inBuf, SOCKET_BUFSIZ, SOCKET_BUFSIZ);
        ringqOpen(&sp->outBuf, SOCKET_BUFSIZ + 1, SOCKET_BUFSIZ + 1);
    } else {
        memset(&sp->inBuf, 0x0, sizeof(ringq_t));
        memset(&sp->outBuf, 0x0, sizeof(ringq_t));
    }
    ringqOpen(&sp->lineBuf, SOCKET_BUFSIZ, -1);
    return sid;
}


/*
    Free a socket structure
 */
void socketFree(int sid)
{
    socket_t    *sp;
    char_t      buf[256];
    int         i;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    /*
        To close a socket, remove any registered interests, set it to non-blocking so that the recv which follows won't
        block, do a shutdown on it so peers on the other end will receive a FIN, then read any data not yet retrieved
        from the receive buffer, and finally close it.  If these steps are not all performed RESETs may be sent to the
        other end causing problems.
     */
    socketRegisterInterest(sp, 0);
    if (sp->sock >= 0) {
        socketSetBlock(sid, 0);
        if (shutdown(sp->sock, 1) >= 0) {
            recv(sp->sock, buf, sizeof(buf), 0);
        }
#if BIT_WIN_LIKE
        closesocket(sp->sock);
#else
        close(sp->sock);
#endif
    }
    if (!(sp->flags & SOCKET_MYOWNBUFFERS)) { 
        ringqClose(&sp->inBuf);
        ringqClose(&sp->outBuf);
    }
    ringqClose(&sp->lineBuf);
    bfree(sp);
    socketMax = hFree((void***) &socketList, sid);

    /*
        Calculate the new highest socket number
     */
    socketHighestFd = -1;
    for (i = 0; i < socketMax; i++) {
        if ((sp = socketList[i]) == NULL) {
            continue;
        } 
        socketHighestFd = max(socketHighestFd, sp->sock);
    }
}


/*
    Validate a socket handle
 */
socket_t *socketPtr(int sid)
{
    if (sid < 0 || sid >= socketMax || socketList[sid] == NULL) {
        a_assert(NULL);
        errno = EBADF;
        return NULL;
    }
    a_assert(socketList[sid]);
    return socketList[sid];
}


/*
    Get the operating system error code
 */
int socketGetError()
{
#if BIT_WIN_LIKE
    switch (WSAGetLastError()) {
    case WSAEWOULDBLOCK:
        return EWOULDBLOCK;
    case WSAECONNRESET:
        return ECONNRESET;
    case WSAENETDOWN:
        return ENETDOWN;
    case WSAEPROCLIM:
        return EAGAIN;
    case WSAEINTR:
        return EINTR;
    default:
        return EINVAL;
    }
#else
    return errno;
#endif
}


/*
    Return the underlying socket handle
 */
int socketGetHandle(int sid)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    return sp->sock;
}


/*
    Get blocking mode
 */
int socketGetBlock(int sid)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        a_assert(0);
        return 0;
    }
    return (sp->flags & SOCKET_BLOCK);
}


int socketGetMode(int sid)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        a_assert(0);
        return 0;
    }
    return sp->flags;
}


void socketSetMode(int sid, int mode)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        a_assert(0);
        return;
    }
    sp->flags = mode;
}


int socketGetPort(int sid)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    return sp->port;
}


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
