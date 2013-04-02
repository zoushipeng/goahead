/*
    est.c - Embedded Secure Transport SSL for GoAhead

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/************************************ Include *********************************/

#include    "goahead.h"

#if BIT_PACK_EST

#include    "est.h"

/************************************* Defines ********************************/

typedef struct EstConfig {
    rsa_context     rsa;                /* RSA context */
    x509_cert       cert;               /* Certificate (own) */
    x509_cert       ca;                 /* Certificate authority bundle to verify peer */
    int             *ciphers;           /* Set of acceptable ciphers */
} EstConfig;

/*
    Per socket state
 */
typedef struct EstSocket {
    havege_state    hs;                 /* Random HAVEGE state */
    ssl_context     ctx;                /* SSL state */
    ssl_session     session;            /* SSL sessions */
} EstSocket;

static EstConfig estConfig;

/*
    Regenerate using: dh_genprime
    Generated on 1/1/2013
 */
static char *dhg = "4";
static char *dhKey =
    "E4004C1F94182000103D883A448B3F80"
    "2CE4B44A83301270002C20D0321CFD00"
    "11CCEF784C26A400F43DFB901BCA7538"
    "F2C6B176001CF5A0FD16D2C48B1D0C1C"
    "F6AC8E1DA6BCC3B4E1F96B0564965300"
    "FFA1D0B601EB2800F489AA512C4B248C"
    "01F76949A60BB7F00A40B1EAB64BDD48"
    "E8A700D60B7F1200FA8E77B0A979DABF";

/************************************ Forwards ********************************/

static int estHandshake(Webs *wp);
static void estTrace(void *fp, int level, char *str);

/************************************** Code **********************************/

PUBLIC int sslOpen()
{
    trace(7, "Initializing EST SSL"); 

    /*
        Set the server certificate and key files
     */
    if (*BIT_GOAHEAD_KEY) {
        /*
            Load a decrypted PEM format private key. The last arg is the private key.
         */
        if (x509parse_keyfile(&estConfig.rsa, BIT_GOAHEAD_KEY, 0) < 0) {
            error("EST: Unable to read key file %s", BIT_GOAHEAD_KEY); 
            return -1;
        }
    }
    if (*BIT_GOAHEAD_CERTIFICATE) {
        /*
            Load a PEM format certificate file
         */
        if (x509parse_crtfile(&estConfig.cert, BIT_GOAHEAD_CERTIFICATE) < 0) {
            error("EST: Unable to read certificate %s", BIT_GOAHEAD_CERTIFICATE); 
            return -1;
        }
    }
    if (*BIT_GOAHEAD_CA) {
        if (x509parse_crtfile(&estConfig.ca, BIT_GOAHEAD_CA) != 0) {
            error("Unable to parse certificate bundle %s", *BIT_GOAHEAD_CA); 
            return -1;
        }
    }
    estConfig.ciphers = ssl_create_ciphers(BIT_GOAHEAD_CIPHERS);
    return 0;
}


PUBLIC void sslClose()
{
}


PUBLIC int sslUpgrade(Webs *wp)
{
    EstSocket   *est;
    WebsSocket  *sp;

    assert(wp);
    if ((est = malloc(sizeof(EstSocket))) == 0) {
        return -1;
    }
    memset(est, 0, sizeof(EstSocket));
    wp->ssl = est;

    ssl_free(&est->ctx);
    havege_init(&est->hs);
    ssl_init(&est->ctx);
	ssl_set_endpoint(&est->ctx, 1);
	ssl_set_authmode(&est->ctx, BIT_GOAHEAD_VERIFY_PEER ? SSL_VERIFY_OPTIONAL : SSL_VERIFY_NO_CHECK);
    ssl_set_rng(&est->ctx, havege_rand, &est->hs);
	ssl_set_dbg(&est->ctx, estTrace, NULL);
    sp = socketPtr(wp->sid);
	ssl_set_bio(&est->ctx, net_recv, &sp->sock, net_send, &sp->sock);
    ssl_set_ciphers(&est->ctx, estConfig.ciphers);
	ssl_set_session(&est->ctx, 1, 0, &est->session);
	memset(&est->session, 0, sizeof(ssl_session));

	ssl_set_ca_chain(&est->ctx, *BIT_GOAHEAD_CA ? &estConfig.ca : NULL, NULL);
    if (*BIT_GOAHEAD_CERTIFICATE && *BIT_GOAHEAD_KEY) {
        ssl_set_own_cert(&est->ctx, &estConfig.cert, &estConfig.rsa);
    }
	ssl_set_dh_param(&est->ctx, dhKey, dhg);

    if (estHandshake(wp) < 0) {
        return -1;
    }
    return 0;
}
    

PUBLIC void sslFree(Webs *wp)
{
    EstSocket   *est;

    est = wp->ssl;
    if (est) {
        ssl_free(&est->ctx);
        wfree(est);
        wp->ssl = 0;
    }
}


/*
    Initiate or continue SSL handshaking with the peer. This routine does not block.
    Return -1 on errors, 0 incomplete and awaiting I/O, 1 if successful
 */
static int estHandshake(Webs *wp)
{
    WebsSocket  *sp;
    EstSocket   *est;
    int         rc, vrc, trusted;

    est = (EstSocket*) wp->ssl;
    trusted = 1;
    rc = 0;

    sp = socketPtr(wp->sid);
    sp->flags |= SOCKET_HANDSHAKING;

    while (est->ctx.state != SSL_HANDSHAKE_OVER) {
        if ((rc = ssl_handshake(&est->ctx)) != 0) {
            if (rc == EST_ERR_NET_TRY_AGAIN) {
                return 0;
            }
            break;
        }
    }
    sp->flags &= ~SOCKET_HANDSHAKING;

    /*
        Analyze the handshake result
     */
    if (rc < 0) {
        if (rc == EST_ERR_SSL_PRIVATE_KEY_REQUIRED && !(*BIT_GOAHEAD_KEY || *BIT_GOAHEAD_CERTIFICATE)) {
            error("Missing required certificate and key");
        } else {
            error("Cannot handshake: error -0x%x", -rc);
        }
        sp->flags |= SOCKET_EOF;
        errno = EPROTO;
        return -1;
       
    } else if ((vrc = ssl_get_verify_result(&est->ctx)) != 0) {
        if (vrc & BADCERT_EXPIRED) {
            logmsg(2, "Certificate expired");

        } else if (vrc & BADCERT_REVOKED) {
            logmsg(2, "Certificate revoked");

        } else if (vrc & BADCERT_CN_MISMATCH) {
            logmsg(2, "Certificate common name mismatch");

        } else if (vrc & BADCERT_NOT_TRUSTED) {
            if (vrc & BADCERT_SELF_SIGNED) {                                                               
                logmsg(2, "Self-signed certificate");
            } else {
                logmsg(2, "Certificate not trusted");
            }
            trusted = 0;

        } else {
            if (est->ctx.client_auth && !*BIT_GOAHEAD_CERTIFICATE) {
                logmsg(2, "Server requires a client certificate");
            } else if (rc == EST_ERR_NET_CONN_RESET) {
                logmsg(2, "Peer disconnected");
            } else {
                logmsg(2, "Cannot handshake: error -0x%x", -rc);
            }
        }
        if (BIT_GOAHEAD_VERIFY_PEER) {
            /* 
               If not verifying the issuer, permit certs that are only untrusted (no other error).
               This allows self-signed certs.
             */
            if (!BIT_GOAHEAD_VERIFY_ISSUER && !trusted) {
                return 1;
            } else {
                sp->flags |= SOCKET_EOF;
                errno = EPROTO;
                return -1;
            }
        }
#if UNUSED
    } else {
        /* Being emitted when no cert supplied */
        logmsg(3, "Certificate verified");
#endif
    }
    return 1;
}


PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    WebsSocket      *sp;
    EstSocket       *est;
    int             rc;

    if (!wp->ssl) {
        assert(0);
        return -1;
    }
    est = (EstSocket*) wp->ssl;
    assert(est);
    sp = socketPtr(wp->sid);

    if (est->ctx.state != SSL_HANDSHAKE_OVER) {
        if ((rc = estHandshake(wp)) <= 0) {
            return rc;
        }
    }
    while (1) {
        rc = ssl_read(&est->ctx, buf, (int) len);
        trace(5, "EST: ssl_read %d", rc);
        if (rc < 0) {
            if (rc == EST_ERR_NET_TRY_AGAIN)  {
                continue;
            } else if (rc == EST_ERR_SSL_PEER_CLOSE_NOTIFY) {
                trace(5, "EST: connection was closed gracefully");
                sp->flags |= SOCKET_EOF;
                return -1;
            } else if (rc == EST_ERR_NET_CONN_RESET) {
                trace(5, "EST: connection reset");
                sp->flags |= SOCKET_EOF;
                return -1;
            } else {
                trace(4, "EST: read error -0x%", -rc);                                                    
                sp->flags |= SOCKET_EOF;                                                               
                return -1; 
            }
        }
        break;
    }
    socketHiddenData(sp, ssl_get_bytes_avail(&est->ctx), SOCKET_READABLE);
    return rc;
}


PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    EstSocket   *est;
    ssize       totalWritten;
    int         rc;

    if (wp->ssl == 0 || len <= 0) {
        assert(0);
        return -1;
    }
    est = (EstSocket*) wp->ssl;
    if (est->ctx.state != SSL_HANDSHAKE_OVER) {
        if ((rc = estHandshake(wp)) <= 0) {
            return rc;
        }
    }
    totalWritten = 0;
    do {
        rc = ssl_write(&est->ctx, (uchar*) buf, (int) len);
        trace(7, "EST: written %d, requested len %d", rc, len);
        if (rc <= 0) {
            if (rc == EST_ERR_NET_TRY_AGAIN) {                                                          
                break;
            }
            if (rc == EST_ERR_NET_CONN_RESET) {                                                         
                trace(4, "ssl_write peer closed");
                return -1;
            } else {
                trace(4, "ssl_write failed rc %d", rc);
                return -1;
            }
        } else {
            totalWritten += rc;
            buf = (void*) ((char*) buf + rc);
            len -= rc;
            trace(7, "EST: write: len %d, written %d, total %d", len, rc, totalWritten);
        }
    } while (len > 0);

    socketHiddenData(socketPtr(wp->sid), est->ctx.in_left, SOCKET_WRITABLE);
    return totalWritten;
}


static void estTrace(void *fp, int level, char *str)
{
    level += 3;
    if (level <= websGetLogLevel()) {
        str = sclone(str);
        str[slen(str) - 1] = '\0';
        trace(level, "%s", str);
    }
}

#else
void estDummy() {}
#endif /* BIT_PACK_EST */

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
