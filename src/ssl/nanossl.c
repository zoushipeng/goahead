/*
    nanossl.c - Mocana NanoSSL

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "bit.h"

#if BIT_PACK_NANOSSL
 #include "goahead.h"

 #include "common/moptions.h"
 #include "common/mdefs.h"
 #include "common/mtypes.h"
 #include "common/merrors.h"
 #include "common/mrtos.h"
 #include "common/mtcp.h"
 #include "common/mocana.h"
 #include "common/random.h"
 #include "common/vlong.h"
 #include "crypto/hw_accel.h"
 #include "crypto/crypto.h"
 #include "crypto/pubcrypto.h"
 #include "crypto/ca_mgmt.h"
 #include "ssl/ssl.h"
 #include "asn1/oiddefs.h"

/************************************* Defines ********************************/

static certDescriptor  cert;
static certDescriptor  ca;

/*
    Per socket state
 */
typedef struct Nano {
    int             fd;
    sbyte4          handle;
    int             connected;
} Nano;

#if BIT_DEBUG
    #define SSL_HELLO_TIMEOUT   15000
    #define SSL_RECV_TIMEOUT    300000
#else
    #define SSL_HELLO_TIMEOUT   15000000
    #define SSL_RECV_TIMEOUT    30000000
#endif

#define KEY_SIZE                1024
#define MAX_CIPHERS             16

/***************************** Forward Declarations ***************************/

static void     nanoLog(sbyte4 module, sbyte4 severity, sbyte *msg);
static void     DEBUG_PRINT(void *where, void *msg);
static void     DEBUG_PRINTNL(void *where, void *msg);

/************************************* Code ***********************************/
/*
    Create the Openssl module. This is called only once
 */
PUBLIC int sslOpen()
{
    sslSettings     *settings;
    char            *certificate, *key, *cacert;
    int             rc;

    if (MOCANA_initMocana() < 0) {
        error("NanoSSL initialization failed");
        return -1;
    }
    MOCANA_initLog(nanoLog);

    if (SSL_init(SOMAXCONN, 0) < 0) {
        error("SSL_init failed");
        return -1;
    }

    certificate = *BIT_GOAHEAD_CERTIFICATE ? BIT_GOAHEAD_CERTIFICATE : 0;
    key = *BIT_GOAHEAD_KEY ? BIT_GOAHEAD_KEY : 0;
    cacert = *BIT_GOAHEAD_CA ? BIT_GOAHEAD_CA: 0;

    if (certificate) {
        certDescriptor tmp;
        if ((rc = MOCANA_readFile((sbyte*) certificate, &tmp.pCertificate, &tmp.certLength)) < 0) {
            error("NanoSSL: Unable to read certificate %s", certificate); 
            CA_MGMT_freeCertificate(&tmp);
            return -1;
        }
        assert(__ENABLE_MOCANA_PEM_CONVERSION__);
        if ((rc = CA_MGMT_decodeCertificate(tmp.pCertificate, tmp.certLength, &cert.pCertificate, 
                &cert.certLength)) < 0) {
            error("NanoSSL: Unable to decode PEM certificate %s", certificate); 
            CA_MGMT_freeCertificate(&tmp);
            return -1;
        }
        MOCANA_freeReadFile(&tmp.pCertificate);
    }
    if (key) {
        certDescriptor tmp;
        if ((rc = MOCANA_readFile((sbyte*) key, &tmp.pKeyBlob, &tmp.keyBlobLength)) < 0) {
            error("NanoSSL: Unable to read key file %s", key); 
            CA_MGMT_freeCertificate(&cert);
        }
        if ((rc = CA_MGMT_convertKeyPEM(tmp.pKeyBlob, tmp.keyBlobLength, 
                &cert.pKeyBlob, &cert.keyBlobLength)) < 0) {
            error("NanoSSL: Unable to decode PEM key file %s", key); 
            CA_MGMT_freeCertificate(&tmp);
            return -1;
        }
        MOCANA_freeReadFile(&tmp.pKeyBlob);    
    }
    if (cacert) {
        certDescriptor tmp;
        if ((rc = MOCANA_readFile((sbyte*) cacert, &tmp.pCertificate, &tmp.certLength)) < 0) {
            error("NanoSSL: Unable to read CA certificate file %s", cacert); 
            CA_MGMT_freeCertificate(&tmp);
            return -1;
        }
        if ((rc = CA_MGMT_decodeCertificate(tmp.pCertificate, tmp.certLength, &ca.pCertificate, 
                &ca.certLength)) < 0) {
            error("NanoSSL: Unable to decode PEM certificate %s", cacert); 
            CA_MGMT_freeCertificate(&tmp);
            return -1;
        }
        MOCANA_freeReadFile(&tmp.pCertificate);
    }

    if (SSL_initServerCert(&cert, FALSE, 0)) {
        error("SSL_initServerCert failed");
        return -1;
    }
    settings = SSL_sslSettings();
    settings->sslTimeOutHello = SSL_HELLO_TIMEOUT;
    settings->sslTimeOutReceive = SSL_RECV_TIMEOUT;
    return 0;
}


PUBLIC void sslClose() 
{
    SSL_releaseTables();
    MOCANA_freeMocana();
    CA_MGMT_freeCertificate(&cert);
}


PUBLIC void sslFree(Webs *wp)
{
    Nano        *np;
    
    if (wp->ssl) {
        np = wp->ssl;
        if (np->handle) {
            SSL_closeConnection(np->handle);
            np->handle = 0;
        }
        wfree(np);
        wp->ssl = 0;
    }
}


/*
    Upgrade a standard socket to use TLS
 */
PUBLIC int sslUpgrade(Webs *wp)
{
    Nano        *np;

    assert(wp);

    if ((np = walloc(sizeof(Nano))) == 0) {
        return -1;
    }
    memset(np, 0, sizeof(Nano));
    wp->ssl = np;
    if ((np->handle = SSL_acceptConnection(socketGetHandle(wp->sid))) < 0) {
        return -1;
    }
    return 0;
}


/*
    Initiate or continue SSL handshaking with the peer. This routine does not block.
    Return -1 on errors, 0 incomplete and awaiting I/O, 1 if successful
*/
static int nanoHandshake(Webs *wp)
{
    Nano        *np;
    ubyte4      flags;
    int         rc;

    np = (Nano*) wp->ssl;
    wp->flags |= SOCKET_HANDSHAKING;
    SSL_getSessionFlags(np->handle, &flags);
    if (BIT_GOAHEAD_VERIFY_PEER) {
        flags |= SSL_FLAG_REQUIRE_MUTUAL_AUTH;
    } else {
        flags |= SSL_FLAG_NO_MUTUAL_AUTH_REQUEST;
    }
    SSL_setSessionFlags(np->handle, flags);
    rc = 0;

    while (!np->connected) {
        if ((rc = SSL_negotiateConnection(np->handle)) < 0) {
            break;
        }
        np->connected = 1;
        break;
    }
    wp->flags &= ~SOCKET_HANDSHAKING;

    /*
        Analyze the handshake result
    */
    if (rc < 0) {
        if (rc == ERR_SSL_UNKNOWN_CERTIFICATE_AUTHORITY) {
            logmsg(3, "Unknown certificate authority");

        /* Common name mismatch, cert revoked */
        } else if (rc == ERR_SSL_PROTOCOL_PROCESS_CERTIFICATE) {
            logmsg(3, "Bad certificate");
        } else if (rc == ERR_SSL_NO_SELF_SIGNED_CERTIFICATES) {
            logmsg(3, "Self-signed certificate");
        } else if (rc == ERR_SSL_CERT_VALIDATION_FAILED) {
            logmsg(3, "Certificate does not validate");
        }
        DISPLAY_ERROR(0, rc); 
        logmsg(4, "NanoSSL: Cannot handshake: error %d", rc);
        errno = EPROTO;
        return -1;
    }
    return 1;
}


/*
    Return the number of bytes read. Return -1 on errors and EOF.
 */
PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    Nano        *np;
    WebsSocket  *sp;
    sbyte4      nbytes, count;
    int         rc;

    np = (Nano*) wp->ssl;
    assert(np);

    if (!np->connected && (rc = nanoHandshake(wp)) <= 0) {
        return rc;
    }
    while (1) {
        /*
            This will do the actual blocking I/O
         */
        rc = SSL_recv(np->handle, buf, (sbyte4) len, &nbytes, 0);
        logmsg(5, "NanoSSL: ssl_read %d", rc);
        if (rc < 0) {
            if (rc != ERR_TCP_READ_ERROR) {
                sp = socketPtr(wp->sid);
                sp->flags |= SOCKET_EOF;
            }
            return -1;
        }
        break;
    }
    SSL_recvPending(np->handle, &count);
    if (count > 0) {
        socketReservice(wp->wid);
    }
    return nbytes;
}


/*
    Write data. Return the number of bytes written or -1 on errors.
 */
PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    Nano        *np;
    WebsSocket  *sp;
    ssize       totalWritten;
    int         rc, count, sent;

    np = (Nano*) wp->ssl;
    if (len <= 0) {
        assert(0);
        return -1;
    }
    if (!np->connected && (rc = nanoHandshake(wp)) <= 0) {
        return rc;
    }
    totalWritten = 0;
    do {
        rc = sent = SSL_send(np->handle, (sbyte*) buf, (int) len);
        logmsg(7, "NanoSSL: written %d, requested len %d", sent, len);
        if (rc <= 0) {
            logmsg(0, "NanoSSL: SSL_send failed sent %d", rc);
            sp = socketPtr(wp->sid);
            sp->flags |= SOCKET_EOF;
            return -1;
        }
        totalWritten += sent;
        buf = (void*) ((char*) buf + sent);
        len -= sent;
        logmsg(7, "NanoSSL: write: len %d, written %d, total %d", len, sent, totalWritten);
    } while (len > 0);

    SSL_sendPending(np->handle, &count);
    if (count > 0) {
        socketReservice(wp->sid);
    }
    return totalWritten;
}


static void DEBUG_PRINT(void *where, void *msg)
{
    logmsg(2, "%s", msg);
}

static void DEBUG_PRINTNL(void *where, void *msg)
{
    logmsg(2, "%s", msg);
}

static void nanoLog(sbyte4 module, sbyte4 severity, sbyte *msg)
{
    logmsg(3, "%s", (cchar*) msg);
}

#else
void nanosslDummy() {}
#endif /* BIT_PACK_NANOSSL */

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2013. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
