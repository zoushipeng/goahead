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

static int ipv6(char_t *ip);
static void socketAccept(socket_t *sp);
static int  socketDoEvent(socket_t *sp);
static ssize socketDoOutput(socket_t *sp, char *buf, ssize toWrite, int *errCode);

/*********************************** Code *************************************/

int socketOpen()
{
    if (++socketOpenCount > 1) {
        return 0;
    }

#if BIT_WIN_LIKE
{
    WSADATA     wsaData;
    if (WSAStartup(MAKEWORD(1,1), &wsaData) != 0) {
        return -1;
    }
    if (wsaData.wVersion != MAKEWORD(1,1)) {
        WSACleanup();
        return -1;
    }
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


int socketListen(char *ip, int port, socketAccept_t accept, int flags)
{
    socket_t                *sp;
    struct sockaddr_storage addr;
    GSockLenArg            addrlen;
    int                     family, protocol, sid, rc;

    if (port > SOCKET_PORT_MAX) {
        return -1;
    }
    if ((sid = socketAlloc(ip, port, accept, flags)) < 0) {
        return -1;
    }
    sp = socketList[sid];
    gassert(sp);

    /*
        Bind to the socket endpoint and the call listen() to start listening
     */
    if (socketInfo(ip, port, &family, &protocol, &addr, &addrlen) < 0) {
        return -1;
    }
    if ((sp->sock = socket(family, SOCK_STREAM, protocol)) < 0) {
        socketFree(sid);
        return -1;
    }
    socketHighestFd = max(socketHighestFd, sp->sock);

#if BIT_HAS_FCNTL
    fcntl(sp->sock, F_SETFD, FD_CLOEXEC);
#endif
    rc = 1;
    setsockopt(sp->sock, SOL_SOCKET, SO_REUSEADDR, (char*) &rc, sizeof(rc));

    if (bind(sp->sock, (struct sockaddr*) &addr, addrlen) < 0) {
        error(T("Can't bind to address %s:%d, errno %d"), ip ? ip : "*", port, errno);
        socketFree(sid);
        return -1;
    }
    if (listen(sp->sock, SOMAXCONN) < 0) {
        socketFree(sid);
        return -1;
    }
    sp->flags |= SOCKET_LISTENING;
    sp->handlerMask |= SOCKET_READABLE;
    socketSetBlock(sid, (flags & SOCKET_BLOCK));
    return sid;
}


#if UNUSED && KEEP
int socketConnect(char *ip, int port, int flags)
{
    socket_t                *sp;
    struct sockaddr_storage addr;
    socklen_t               addrlen;
    int                     family, protocol, sid, rc;

    if (port > SOCKET_PORT_MAX) {
        return -1;
    }
    if ((sid = socketAlloc(ip, port, NULL, flags)) < 0) {
        return -1;
    }
    sp = socketList[sid];
    gassert(sp);

    if (socketInfo(ip, port, &family, &protocol, &addr, &addrlen) < 0) {
        return -1;       
    }
    if ((sp->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        socketFree(sid);
        return -1;
    }
    socketHighestFd = max(socketHighestFd, sp->sock);

#if BIT_HAS_FCNTL
    fcntl(sp->sock, F_SETFD, FD_CLOEXEC);
#endif

    /*
        Connect to the remote server in blocking mode, then go into non-blocking mode if desired.
     */
    if (! (sp->flags & SOCKET_BLOCK)) {
#if BIT_WIN_LIKE
        /*
            Set to non-blocking for an async connect
         */
        int flag = 1;
        if (ioctlsocket(sp->sock, FIONBIO, &flag) == SOCKET_ERROR) {
            socketFree(sid);
            return -1;
        }
        sp->flags |= SOCKET_ASYNC;
#else
        socketSetBlock(sid, 1);
#endif
    }
    if ((rc = connect(sp->sock, (struct sockaddr*) &addr, sizeof(addr))) < 0 && 
        (rc = tryAlternateConnect(sp->sock, (struct sockaddr*) &addr)) < 0) {
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
    socketSetBlock(sid, (flags & SOCKET_BLOCK));
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

    ptr = (char*) sockaddr;
    *ptr = *(ptr+1);
    *(ptr+1) = 0;
    return connect(sock, sockaddr, sizeof(struct sockaddr));
#else
    return -1;
#endif /* VXWORKS */
}
#endif


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

    gassert(sp);

    /*
        Accept the connection and prevent inheriting by children (F_SETFD)
     */
    len = sizeof(struct sockaddr_in);
    if ((newSock = accept(sp->sock, (struct sockaddr*) &addr, (GSockLenArg*) &len)) < 0) {
        return;
    }
#if BIT_HAS_FCNTL
    fcntl(newSock, F_SETFD, FD_CLOEXEC);
#endif
    socketHighestFd = max(socketHighestFd, newSock);

    /*
        Create a socket structure and insert into the socket list
     */
    nid = socketAlloc(sp->ip, sp->port, sp->accept, sp->flags);
    if ((nsp = socketList[nid]) == 0) {
        return;
    }
    gassert(nsp);
    nsp->sock = newSock;
    nsp->flags &= ~SOCKET_LISTENING;
    socketSetBlock(nid, (nsp->flags & SOCKET_BLOCK));

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
    socket_t            *sp;
    ssize               bytesRead;

    gassert(buf);
    gassert(errCode);

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
    if ((bytesRead = recv(sp->sock, buf, toRead, 0)) < 0) {
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
    gassert(sp);
    sp->handlerMask = handlerMask;
}


/*
    Wait until an event occurs on a socket. Return 1 on success, 0 on failure. or -1 on exception
 */
int socketWaitForEvent(socket_t *sp, int handlerMask, int *errCode)
{
    int mask;

    gassert(sp);

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
#if BIT_WIN_LIKE

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
        gassert(sp);
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
        if WINDOWS, sleep.  
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

    readFds = galloc(len);
    memset(readFds, 0, len);
    writeFds = galloc(len);
    memset(writeFds, 0, len);
    exceptFds = galloc(len);
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
        gassert(sp);

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
    gfree(readFds);
    gfree(writeFds);
    gfree(exceptFds);
    return nEvents;
}
#endif /* WINDOWS || CE */


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

    gassert(sp);

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
    int             oldBlock;

    if ((sp = socketPtr(sid)) == NULL) {
        gassert(0);
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
        ulong flag = !on;
        ioctlsocket(sp->sock, FIONBIO, &flag);
#elif ECOS
        int off;
        off = 0;
        ioctl(sp->sock, FIONBIO, &off);
#elif VXWORKS
        int iflag = !on;
        ioctl(sp->sock, FIONBIO, (int) &iflag);
#else
        fcntl(sp->sock, F_SETFL, fcntl(sp->sock, F_GETFL) & ~O_NONBLOCK);
#endif

    } else {
#if BIT_WIN_LIKE
        ulong flag = !on;
        ioctlsocket(sp->sock, FIONBIO, &flag);
#elif ECOS
        int on = 1;
        ioctl(sp->sock, FIONBIO, &on);
#elif VXWORKS
        int iflag = !on;
        ioctl(sp->sock, FIONBIO, (int) &iflag);
#else
        fcntl(sp->sock, F_SETFL, fcntl(sp->sock, F_GETFL) | O_NONBLOCK);
#endif
    }
#if MACOSX
    /* Prevent SIGPIPE when writing to closed socket on OS X */
    int iflag = 1;
    setsockopt(sp->sock, SOL_SOCKET, SO_NOSIGPIPE, (void*) &iflag, sizeof(iflag));
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

    gassert(buf);
    gassert(bufsize >= 0);

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
    byteBuf = gallocUniToAsc(buf, len);
    r = socketWrite(sid, byteBuf, len);
    gfree(byteBuf);
    return r;
 #else
    return socketWrite(sid, buf, strlen(buf));
 #endif /* UNICODE */
}


/*
    Read from a socket. Return the number of bytes read if successful. This may be less than the requested "bufsize" and
    may be zero. Return -1 for errors or EOF. Distinguish between error and EOF via socketEof().
 
    Note: this ignores the line buffer, so a previous socketGets which read a partial line may cause a subsequent
    socketRead to miss some data. This routine may block if the socket is in blocking mode.
 */
ssize socketRead(int sid, char *buf, ssize bufsize)
{
    socket_t    *sp;
    ringq_t     *rq;
    ssize       len, room, bytesRead;
    int         errCode;

    gassert(buf);
    gassert(bufsize > 0);

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    if (sp->flags & SOCKET_EOF) {
        return -1;
    }
    rq = &sp->inBuf;
    for (bytesRead = 0; bufsize > 0; ) {
        len = min(ringqLen(rq), bufsize);
        if (len <= 0) {
            /*
                If blocking mode and already have data, exit now or it may block forever.
             */
            if ((sp->flags & SOCKET_BLOCK) && (bytesRead > 0)) {
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
                if (errCode == EWOULDBLOCK || errCode == EAGAIN) {
                    if ((sp->flags & SOCKET_BLOCK) && (bytesRead == 0)) {
                        continue;
                    }
                    if (bytesRead > 0) {
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
                    bytesRead = -1;
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

    gassert(buf);
    *buf = NULL;
    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    lq = &sp->lineBuf;

    while (1) {
        if ((rc = socketRead(sid, &c, 1)) < 0) {
            if (sp->flags & SOCKET_EOF && ringqLen(lq) > 0) {
                c = '\n';
            } else {
                return rc;
            }
        }
        if (rc == 0) {
            return -1;
        }
        /* 
            Validate length of request.  Ignore long strings without newlines to safeguard against long URL attacks.
         */
        if (ringqLen(lq) > BIT_LIMIT_HEADER) {
            c = '\n';
        }
        /*
            If a newline is seen, return the data excluding the new line to the caller. If carriage return is seen, just
            eat it.  
         */
        if (c == '\n') {
            len = ringqLen(lq);
            if (len > 0) {
                *buf = gallocAscToUni((char *)lq->servp, len);
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
    ssize   bytes;

    gassert(sp);
    gassert(buf);
    gassert(toWrite > 0);
    gassert(errCode);

    *errCode = 0;

#if BIT_WIN_LIKE
    if ((sp->flags & SOCKET_ASYNC)
            && ! socketWaitForEvent(sp,  FD_CONNECT, errCode)) {
        return -1;
    }
#endif
    if ((bytes = send(sp->sock, buf, toWrite, 0)) < 0) {
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
    Allocate a new socket structure
 */
int socketAlloc(char *ip, int port, socketAccept_t accept, int flags)
{
    socket_t    *sp;
    int         sid;

    if ((sid = gallocEntry((void***) &socketList, &socketMax, sizeof(socket_t))) < 0) {
        return -1;
    }
    sp = socketList[sid];
    sp->sid = sid;
    sp->accept = accept;
    sp->port = port;
    sp->fileHandle = -1;
    sp->saveMask = -1;
    if (ip) {
        sp->ip = gstrdup(ip);
    }

    /*
        Preserve only specified flags from the callers open
     */
    sp->flags = flags & (SOCKET_BLOCK | SOCKET_LISTENING | SOCKET_OWN_BUFFERS);

    if (!(flags & SOCKET_OWN_BUFFERS)) { 
        ringqOpen(&sp->inBuf, BIT_LIMIT_SOCKET_BUFFER + 1, BIT_LIMIT_SOCKET_BUFFER + 1);
        ringqOpen(&sp->outBuf, BIT_LIMIT_SOCKET_BUFFER + 1, BIT_LIMIT_SOCKET_BUFFER + 1);
    } else {
        memset(&sp->inBuf, 0x0, sizeof(ringq_t));
        memset(&sp->outBuf, 0x0, sizeof(ringq_t));
    }
    ringqOpen(&sp->lineBuf, BIT_LIMIT_SOCKET_BUFFER, -1);
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
    if (!(sp->flags & SOCKET_OWN_BUFFERS)) { 
        ringqClose(&sp->inBuf);
        ringqClose(&sp->outBuf);
    }
    ringqClose(&sp->lineBuf);
    gfree(sp->ip);
    gfree(sp);
    socketMax = gfreeHandle((void***) &socketList, sid);

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
        gassert(NULL);
        errno = EBADF;
        return NULL;
    }
    gassert(socketList[sid]);
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
        gassert(0);
        return 0;
    }
    return (sp->flags & SOCKET_BLOCK);
}


int socketGetMode(int sid)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        gassert(0);
        return 0;
    }
    return sp->flags;
}


void socketSetMode(int sid, int mode)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        gassert(0);
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


#if BIT_UNIX_LIKE || WINDOWS
/*  
    Get a socket address from a host/port combination. If a host provides both IPv4 and IPv6 addresses, 
    prefer the IPv4 address. This routine uses getaddrinfo.
    Caller must free addr.
 */
int socketInfo(char_t *ip, int port, int *family, int *protocol, struct sockaddr_storage *addr, GSockLenArg *addrlen)
{
    struct addrinfo     hints, *res, *r;
    char                portBuf[16];
    int                 v6;

    gassert(addr);
    memset((char*) &hints, '\0', sizeof(hints));

    /*
        Note that IPv6 does not support broadcast, there is no 255.255.255.255 equivalent.
        Multicast can be used over a specific link, but the user must provide that address plus %scope_id.
     */
    if (ip == 0 || ip[0] == '\0') {
        ip = 0;
        hints.ai_flags |= AI_PASSIVE;           /* Bind to 0.0.0.0 and :: */
    }
    v6 = ipv6(ip);
    hints.ai_socktype = SOCK_STREAM;
    if (ip) {
        hints.ai_family = v6 ? AF_INET6 : AF_INET;
    } else {
        hints.ai_family = AF_UNSPEC;
    }
    gstritoa(port, portBuf, sizeof(portBuf));

    /*  
        Try to sleuth the address to avoid duplicate address lookups. Then try IPv4 first then IPv6.
     */
    res = 0;
    if (getaddrinfo(ip, portBuf, &hints, &res) != 0) {
        return -1;
    }
    /*
        Prefer IPv4 if IPv6 not requested
     */
    for (r = res; r; r = r->ai_next) {
        if (v6) {
            if (r->ai_family == AF_INET6) {
                break;
            }
        } else {
            if (r->ai_family == AF_INET) {
                break;
            }
        }
    }
    if (r == NULL) {
        r = res;
    }
    memset(addr, 0, sizeof(*addr));
    memcpy((char*) addr, (char*) r->ai_addr, (int) r->ai_addrlen);

    *addrlen = (int) r->ai_addrlen;
    *family = r->ai_family;
    *protocol = r->ai_protocol;
    freeaddrinfo(res);
    return 0;
}
#else

int socketInfo(char_t *ip, int port, int *family, int *protocol, struct sockaddr_storage *addr, GSockLenArg *addrlen)
{
    struct sockaddr_in  sa;

    memset((char*) &sa, '\0', sizeof(struct sockaddr_in));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((short) (port & 0xFFFF));

    if (strcmp(ip, "") != 0) {
        sa.sin_addr.s_addr = inet_addr((char*) ip);
    } else {
        sa.sin_addr.s_addr = INADDR_ANY;
    }
    if (sa.sin_addr.s_addr == INADDR_NONE) {
#if VXWORKS
        /*
            VxWorks only supports one interface and this code only supports IPv4
         */
        sa.sin_addr.s_addr = (ulong) hostGetByName((char*) ip);
        if (sa.sin_addr.s_addr < 0) {
            gassert(0);
            return 0;
        }
#else
        struct hostent *hostent;
        hostent = gethostbyname2(ip, AF_INET);
        if (hostent == 0) {
            hostent = gethostbyname2(ip, AF_INET6);
            if (hostent == 0) {
                return -1;
            }
        }
        memcpy((char*) &sa.sin_addr, (char*) hostent->h_addr_list[0], (ssize) hostent->h_length);
#endif
    }
    memcpy((char*) addr, (char*) &sa, sizeof(sa));
    *addrlen = sizeof(struct sockaddr_in);
    *family = sa.sin_family;
    *protocol = 0;
    return 0;
}
#endif


#if UNUSED && KEEP
/*  
    Return a numerical IP address and port for the given socket info
 */
int socketAddress(struct sockaddr *addr, int addrlen, char *ip, int ipLen, int *port)
{
#if (BIT_UNIX_LIKE || WINDOWS)
    char    service[NI_MAXSERV];

#ifdef IN6_IS_ADDR_V4MAPPED
    if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*) addr;
        if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
            struct sockaddr_in addr4;
            memset(&addr4, 0, sizeof(addr4));
            addr4.sin_family = AF_INET;
            addr4.sin_port = addr6->sin6_port;
            memcpy(&addr4.sin_addr.s_addr, addr6->sin6_addr.s6_addr + 12, sizeof(addr4.sin_addr.s_addr));
            memcpy(addr, &addr4, sizeof(addr4));
            addrlen = sizeof(addr4);
        }
    }
#endif
    if (getnameinfo(addr, addrlen, ip, ipLen, service, sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV | NI_NOFQDN)) {
        return -1;
    }
    *port = atoi(service);

#else
    struct sockaddr_in  *sa;

#if HAVE_NTOA_R
    sa = (struct sockaddr_in*) addr;
    inet_ntoa_r(sa->sin_addr, ip, ipLen);
#else
    uchar   *cp;
    sa = (struct sockaddr_in*) addr;
    cp = (uchar*) &sa->sin_addr;
    gfmtStatic(ip, ipLen, "%d.%d.%d.%d", cp[0], cp[1], cp[2], cp[3]);
#endif
    *port = ntohs(sa->sin_port);
#endif
    return 0;
}
#endif


/*
    Looks like an IPv6 address if it has 2 or more colons
 */
static int ipv6(char_t *ip)
{
    char_t  *cp;
    int     colons;

    if (ip == 0 || *ip == 0) {
        /*
            Listening on just a bare port means IPv4 only.
         */
        return 0;
    }
    colons = 0;
    for (cp = (char*) ip; ((*cp != '\0') && (colons < 2)) ; cp++) {
        if (*cp == ':') {
            colons++;
        }
    }
    return colons >= 2;
}


/*  
    Parse ipAddrPort and return the IP address and port components. Handles ipv4 and ipv6 addresses. 
    If the IP portion is absent, *pip is set to null. If the port portion is absent, port is set to the defaultPort.
    If a ":*" port specifier is used, *pport is set to -1;
    When an ipAddrPort
    contains an ipv6 port it should be written as

        aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh:iiii
    or
        [aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh:iiii]:port

    If supplied an IPv6 address, the backets are stripped in the returned IP address.
    This routine skips any "protocol://" prefix.

    Caller must free *pip
 */
int socketParseAddress(char_t *ipAddrPort, char_t **pip, int *pport, int defaultPort)
{
    char_t    *ip, *cp;

    ip = 0;
    if (defaultPort < 0) {
        defaultPort = 80;
    }
    if ((cp = strstr(ipAddrPort, "://")) != 0) {
        ipAddrPort = &cp[3];
    }
    if (ipv6(ipAddrPort)) {
        /*  
            IPv6. If port is present, it will follow a closing bracket ']'
         */
        if ((cp = strchr(ipAddrPort, ']')) != 0) {
            cp++;
            if ((*cp) && (*cp == ':')) {
                *pport = (*++cp == '*') ? -1 : atoi(cp);

                /* Set ipAddr to ipv6 address without brackets */
                ip = gstrdup(ipAddrPort + 1);
                cp = strchr(ip, ']');
                *cp = '\0';

            } else {
                /* Handles [a:b:c:d:e:f:g:h:i] case (no port)- should not occur */
                ip = gstrdup(ipAddrPort + 1);
                if ((cp = strchr(ip, ']')) != 0) {
                    *cp = '\0';
                }
                if (*ip == '\0') {
                    ip = 0;
                }
                /* No port present, use callers default */
                *pport = defaultPort;
            }
        } else {
            /* Handles a:b:c:d:e:f:g:h:i case (no port) */
            ip = gstrdup(ipAddrPort);

            /* No port present, use callers default */
            *pport = defaultPort;
        }

    } else {
        /*  
            ipv4 
         */
        ip = gstrdup(ipAddrPort);
        if ((cp = strchr(ip, ':')) != 0) {
            *cp++ = '\0';
            if (*cp == '*') {
                *pport = -1;
            } else {
                *pport = atoi(cp);
            }
            if (*ip == '*') {
                ip = 0;
            }
            
        } else if (strchr(ip, '.')) {
            *pport = defaultPort;
            
        } else {
            if (isdigit((uchar) *ip)) {
                *pport = atoi(ip);
                ip = 0;
            } else {
                /* No port present, use callers default */
                *pport = defaultPort;
            }
        }
    }
    if (pip) {
        *pip = ip;
    }
    return 0;
}


bool socketIsV6(int sid)
{
    socket_t    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return 0;
    }
    return sp->ip && ipv6(sp->ip);
}


bool socketAddressIsV6(char_t *ip)
{
    return ip && ipv6(ip);
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
