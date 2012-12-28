/*
    matrixssl.c - SSL socket layer for MatrixSSL

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/************************************ Includes ********************************/

#include    "bit.h"

#if BIT_PACK_MATRIXSSL && !BIT_PACK_OPENSSL
/*
    Matrixssl defines int32, uint32, int64 and uint64, but does not provide HAS_XXX to disable.
    So must include matrixsslApi.h first and then workaround. Ugh!
*/
#if WIN32
    #include   <winsock2.h>
    #include   <windows.h>
#endif
    /* Indent to not create a dependency on this file if not enabled */
    #include    "matrixsslApi.h"

#define     HAS_INT32 1
#define     HAS_UINT32 1
#define     HAS_INT64 1
#define     HAS_UINT64 1

#include    "goahead.h"

/************************************ Defines *********************************/

/*
    MatrixSSL per-socket state
 */
typedef struct Ms {
    int     fd;
    ssl_t   *handle;
    char    *outbuf;        /* Pending output data */
    ssize   outlen;         /* Length of outbuf */
    ssize   written;        /* Number of unencoded bytes written */
    int     more;           /* MatrixSSL stack has buffered data */
} Ms;

static sslKeys_t *sslKeys = NULL;

/************************************ Code ************************************/

PUBLIC int sslOpen()
{
    char    *password;

    if (matrixSslOpen() < 0) {
        return -1;
    }
    if (matrixSslNewKeys(&sslKeys) < 0) {
        error("Failed to allocate keys in sslOpen\n");
        return -1;
    }
    password = 0;
    if (matrixSslLoadRsaKeys(sslKeys, BIT_GOAHEAD_CERTIFICATE, BIT_GOAHEAD_KEY, password,  NULL) < 0) {
        error(0, "Failed to read certificate %s or key file\n", BIT_GOAHEAD_CERTIFICATE);
        return -1;
    }
    return 0;
}


PUBLIC void sslClose()
{
    if (sslKeys) {
        matrixSslDeleteKeys(sslKeys);
        sslKeys = 0;
    }
    matrixSslClose();
}


PUBLIC void sslFree(Webs *wp)
{
    Ms          *ms;
    WebsSocket    *sp;
    uchar       *buf;
    int         len;
    
    if ((sp = socketPtr(wp->sid)) == 0) {
        return;
    }
    if (!sp->flags & SOCKET_EOF && wp->ms) {
        ms = wp->ms;
        /*
            Flush data. Append a closure alert to any buffered output data, and try to send it.
            Don't bother retrying or blocking, we're just closing anyway.
        */
        matrixSslEncodeClosureAlert(ms->handle);
        if ((len = matrixSslGetOutdata(ms->handle, &buf)) > 0) {
            sslWrite(wp, buf, len);
        }
        if (ms->handle) {
            matrixSslDeleteSession(ms->handle);
        }
    }
    wp->ms = 0;
}


PUBLIC int sslUpgrade(Webs *wp)
{
    Ms      *ms;
    ssl_t   *handle;
    
    if (matrixSslNewServerSession(&handle, sslKeys, NULL) < 0) {
        return -1;
    }
    if ((ms = walloc(sizeof(Ms))) == 0) {
        return -1;
    }
    memset(ms, 0x0, sizeof(Ms));
    ms->handle = handle;
    wp->ms = ms;
    return 0;
}


static ssize blockingWrite(Webs *wp, void *buf, ssize len)
{
    ssize   written, bytes;
    int     prior;

    prior = socketSetBlock(wp->sid, 1);
    for (written = 0; len > 0; ) {
        if ((bytes = socketWrite(wp->sid, buf, len)) < 0) {
            socketSetBlock(wp->sid, prior);
            return bytes;
        }
        buf += bytes;
        len -= bytes;
        written += bytes;
    }
    socketSetBlock(wp->sid, prior);
    return written;
}


static ssize processIncoming(Webs *wp, char *buf, ssize size, ssize nbytes, int *readMore)
{
    Ms      *ms;
    uchar   *data, *obuf;
    ssize   toWrite, written, copied, sofar;
    uint32  dlen;
    int     rc;

    ms = (Ms*) wp->ms;
    *readMore = 0;
    sofar = 0;

    /*
        Process the received data. If there is application data, it is returned in data/dlen
     */
    rc = matrixSslReceivedData(ms->handle, (int) nbytes, &data, &dlen);

    while (1) {
        switch (rc) {
        case PS_SUCCESS:
            return sofar;

        case MATRIXSSL_REQUEST_SEND:
            toWrite = matrixSslGetOutdata(ms->handle, &obuf);
            if ((written = blockingWrite(wp, obuf, toWrite)) < 0) {
                error("MatrixSSL: Error in process");
                return -1;
            }
            matrixSslSentData(ms->handle, (int) written);
            *readMore = 1;
            return 0;

        case MATRIXSSL_REQUEST_RECV:
            /* Partial read. More read data required */
            *readMore = 1;
            ms->more = 1;
            return 0;

        case MATRIXSSL_HANDSHAKE_COMPLETE:
            *readMore = 0;
            return 0;

        case MATRIXSSL_RECEIVED_ALERT:
            assure(dlen == 2);
            if (data[0] == SSL_ALERT_LEVEL_FATAL) {
                return -1;
            } else if (data[1] == SSL_ALERT_CLOSE_NOTIFY) {
                //  ignore - graceful close
                return 0;
            } else {
                //  ignore
            }
            rc = matrixSslProcessedData(ms->handle, &data, &dlen);
            break;

        case MATRIXSSL_APP_DATA:
            copied = min((ssize) dlen, size);
            memcpy(buf, data, copied);
            buf += copied;
            size -= copied;
            data += copied;
            dlen = dlen - (int) copied;
            sofar += copied;
            ms->more = ((ssize) dlen > size) ? 1 : 0;
            if (!ms->more) {
                /* The MatrixSSL buffer has been consumed, see if we can get more data */
                rc = matrixSslProcessedData(ms->handle, &data, &dlen);
                break;
            }
            return sofar;

        default:
            return -1;
        }
    }
}


/*
    Return number of bytes read. Return -1 on errors and EOF.
 */
static ssize innerRead(Webs *wp, char *buf, ssize size)
{
    Ms          *ms;
    uchar       *mbuf;
    ssize       nbytes;
    int         msize, readMore;

    ms = (Ms*) wp->ms;
    do {
        if ((msize = matrixSslGetReadbuf(ms->handle, &mbuf)) < 0) {
            return -1;
        }
        readMore = 0;
        if ((nbytes = socketRead(wp->sid, mbuf, msize)) > 0) {
            if ((nbytes = processIncoming(wp, buf, size, nbytes, &readMore)) > 0) {
                return nbytes;
            }
        }
    } while (readMore);
    return 0;
}


/*
    Return number of bytes read. Return -1 on errors and EOF.
 */
PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    Ms      *ms;
    ssize   bytes;

    if (len <= 0) {
        return -1;
    }
    bytes = innerRead(wp, buf, len);
    ms = (Ms*) wp->ms;
    if (ms->more) {
        wp->flags |= SOCKET_PENDING;
        socketReservice(wp->sid);
    }
    return bytes;
}


/*
    Non-blocking write data. Return the number of bytes written or -1 on errors.
    Returns zero if part of the data was written.

    Encode caller's data buffer into an SSL record and write to socket. The encoded data will always be 
    bigger than the incoming data because of the record header (5 bytes) and MAC (16 bytes MD5 / 20 bytes SHA1)
    This would be fine if we were using blocking sockets, but non-blocking presents an interesting problem.  Example:

        A 100 byte input record is encoded to a 125 byte SSL record
        We can send 124 bytes without blocking, leaving one buffered byte
        We can't return 124 to the caller because it's more than they requested
        We can't return 100 to the caller because they would assume all data
        has been written, and we wouldn't get re-called to send the last byte

    We handle the above case by returning 0 to the caller if the entire encoded record could not be sent. Returning 
    0 will prompt us to select this socket for write events, and we'll be called again when the socket is writable.  
    We'll use this mechanism to flush the remaining encoded data, ignoring the bytes sent in, as they have already 
    been encoded.  When it is completely flushed, we return the originally requested length, and resume normal 
    processing.
 */
PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    Ms      *ms;
    uchar   *obuf;
    ssize   encoded, nbytes, written;

    ms = (Ms*) wp->ms;
    while (len > 0 || ms->outlen > 0) {
        if ((encoded = matrixSslGetOutdata(ms->handle, &obuf)) <= 0) {
            if (ms->outlen <= 0) {
                ms->outbuf = (char*) buf;
                ms->outlen = len;
                ms->written = 0;
                len = 0;
            }
            nbytes = min(ms->outlen, SSL_MAX_PLAINTEXT_LEN);
            if ((encoded = matrixSslEncodeToOutdata(ms->handle, (uchar*) buf, (int) nbytes)) < 0) {
                return encoded;
            }
            ms->outbuf += nbytes;
            ms->outlen -= nbytes;
            ms->written += nbytes;
        }
        if ((written = socketWrite(wp->sid, obuf, encoded)) < 0) {
            return written;
        } else if (written == 0) {
            break;
        }
        matrixSslSentData(ms->handle, (int) written);
    }
    /*
        Only signify all the data has been written if MatrixSSL has absorbed all the data
     */
    return ms->outlen == 0 ? ms->written : 0;
}

#endif /* BIT_PACK_MATRIXSSL */

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2013. All Rights Reserved.

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
