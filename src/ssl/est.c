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
    rsa_context     rsa;
    x509_cert       cert;
    int             *ciphers;
    //  MOB - where should this be set from?
    int             verifyPeer;
    char            *dhKey;
} EstConfig;

/*
    Per socket state
 */
typedef struct EstSocket {
    // MprSocket    *sock;
    havege_state    hs;
    ssl_context     ssl;
    ssl_session     session;
} EstSocket;

static EstConfig estConfig;

//  MOB - GENERATE
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

static void estTrace(void *fp, int level, char *str);
static int      setSession(ssl_context *ssl);
static int      getSession(ssl_context *ssl);

/************************************** Code **********************************/

PUBLIC int sslOpen()
{
    trace(7, "Initializing EST SSL\n"); 

#if 0
    //  MOB - use estCreateCiphers when ready
    estConfig.ciphers = createCiphers(BIT_GOAHEAD_CIPHERS);
#endif
    /*
          Set the server certificate and key files
     */
    if (*BIT_GOAHEAD_KEY && x509parse_keyfile(&estConfig.rsa, BIT_GOAHEAD_KEY, 0) < 0) {
        error("EST: Unable to read key file %s", BIT_GOAHEAD_KEY); 
        return -1;
    }
    if (*BIT_GOAHEAD_CERTIFICATE && x509parse_crtfile(&estConfig.cert, BIT_GOAHEAD_CERTIFICATE) < 0) {
        error("EST: Unable to read certificate %s", BIT_GOAHEAD_CERTIFICATE); 
        return -1;
    }

#if UNUSED
#if VERIFY_CLIENT
    if (verifyPeer) {
        SSL_CTX_set_verify(context, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verifyX509Certificate);
#if FUTURE && KEEP
        SSL_CTX_set_verify_depth(context, VERIFY_DEPTH);
#endif
    } else {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_NONE, verifyX509Certificate);
    }
#else
    SSL_CTX_set_verify(sslctx, SSL_VERIFY_NONE, verifyX509Certificate);
#endif
#endif

#if UNUSED
    /*
        Set the client certificate verification locations
     */
    caFile = *BIT_GOAHEAD_CA_FILE ? BIT_GOAHEAD_CA_FILE : 0;
    caPath = *BIT_GOAHEAD_CA_PATH ? BIT_GOAHEAD_CA_PATH : 0;
    if (caFile || caPath) {
        if ((!SSL_CTX_load_verify_locations(sslctx, caFile, caPath)) || (!SSL_CTX_set_default_verify_paths(sslctx))) {
            error("Unable to read cert verification locations");
            return -1;
        }
    }
    /*
        Set the certificate authority list for the client
     */
    if (BIT_GOAHEAD_CA_FILE && *BIT_GOAHEAD_CA_FILE) {
        SSL_CTX_set_client_CA_list(sslctx, SSL_load_client_CA_file(BIT_GOAHEAD_CA_FILE));
    }
#endif
    return 0;
}


PUBLIC void sslClose()
{
    //  MOB - what to do
}


PUBLIC int sslUpgrade(Webs *wp)
{
    EstSocket   *esp;
    WebsSocket  *sp;

    assure(wp);
    if ((esp = malloc(sizeof(EstSocket))) == 0) {
        return -1;
    }
    wp->est = esp;

    ssl_free(&esp->ssl);
    //  MOB - convert to proper entropy source API
    //  MOB - can't put this in cfg yet as it is not thread safe
    havege_init(&esp->hs);
    ssl_init(&esp->ssl);
	ssl_set_endpoint(&esp->ssl, 1);
	ssl_set_authmode(&esp->ssl, (estConfig.verifyPeer) ? SSL_VERIFY_REQUIRED : SSL_VERIFY_NO_CHECK);
    ssl_set_rng(&esp->ssl, havege_rand, &esp->hs);
	ssl_set_dbg(&esp->ssl, estTrace, NULL);

    sp = socketPtr(wp->sid);
	ssl_set_bio(&esp->ssl, net_recv, &sp->sock, net_send, &sp->sock);

    //  MOB - better if the API took a handle (esp)
	ssl_set_scb(&esp->ssl, getSession, setSession);

    ssl_set_ciphers(&esp->ssl, estConfig.ciphers);

    //  MOB
	ssl_set_session(&esp->ssl, 1, 0, &esp->session);
	memset(&esp->session, 0, sizeof(ssl_session));

	ssl_set_ca_chain(&esp->ssl, estConfig.cert.next, NULL);
	ssl_set_own_cert(&esp->ssl, &estConfig.cert, &estConfig.rsa);
	ssl_set_dh_param(&esp->ssl, dhKey, dhg);
    return 0;
}
    

PUBLIC void sslFree(Webs *wp)
{
    EstSocket   *esp;

    esp = wp->est;
    ssl_free(&esp->ssl);
}


PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    WebsSocket      *sp;
    EstSocket       *esp;
    int             rc;

    if (wp->est) {
        assure(0);
        return -1;
    }
    esp = (EstSocket*) wp->est;
    assure(esp);
    sp = socketPtr(wp->sid);

    while (esp->ssl.state != SSL_HANDSHAKE_OVER && (rc = ssl_handshake(&esp->ssl)) != 0) {
        if (rc != EST_ERR_NET_TRY_AGAIN) {
            trace(2, "EST: readEst: Cannot handshake: %d", rc);
            sp->flags |= SOCKET_EOF;
            return -1;
        }
    }
    while (1) {
        rc = ssl_read(&esp->ssl, buf, (int) len);
        trace(5, "EST: ssl_read %d", rc);
        if (rc < 0) {
            if (rc == EST_ERR_NET_TRY_AGAIN)  {
                continue;
            } else if (rc == EST_ERR_SSL_PEER_CLOSE_NOTIFY) {
                trace(5, "EST: connection was closed gracefully\n");
                sp->flags |= SOCKET_EOF;
                return -1;
            } else if (rc == EST_ERR_NET_CONN_RESET) {
                trace(5, "EST: connection reset");
                sp->flags |= SOCKET_EOF;
                return -1;
            }
        }
        break;
    }
    if (esp->ssl.in_left > 0) {
        sp->flags |= SOCKET_PENDING;
        socketReservice(wp->sid);
    }
    return rc;
}


PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    EstSocket   *esp;
    ssize       totalWritten;
    int         rc;

    if (wp->est == 0 || len <= 0) {
        assure(0);
        return -1;
    }
    esp = (EstSocket*) wp->est;
    if (len <= 0) {
        assure(0);
        return -1;
    }
    totalWritten = 0;

    do {
        rc = ssl_write(&esp->ssl, (uchar*) buf, (int) len);
        trace(7, "EST: written %d, requested len %d", rc, len);
        if (rc <= 0) {
            if (rc == EST_ERR_NET_TRY_AGAIN) {                                                          
                continue;
            }
            if (rc == EST_ERR_NET_CONN_RESET) {                                                         
                printf(" failed\n  ! peer closed the connection\n\n");                                         
                trace(0, "ssl_write peer closed");
                return -1;
            } else if (rc != EST_ERR_NET_TRY_AGAIN) {                                                          
                trace(0, "ssl_write failed rc %d", rc);
                return -1;
            }
        } else {
            totalWritten += rc;
            buf = (void*) ((char*) buf + rc);
            len -= rc;
            trace(7, "EST: write: len %d, written %d, total %d", len, rc, totalWritten);
        }
    } while (len > 0);
    return totalWritten;
}


#if UNUSED
static int verifyX509Certificate(int ok, X509_STORE_CTX *xContext)
{
    X509            *cert;
    char            subject[260], issuer[260], peer[260];
    int             error;
    
    subject[0] = issuer[0] = '\0';

    cert = X509_STORE_CTX_get_current_cert(xContext);
    error = X509_STORE_CTX_get_error(xContext);

    ok = 1;
    if (X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject) - 1) < 0) {
        ok = 0;
    }
    if (X509_NAME_oneline(X509_get_issuer_name(xContext->current_cert), issuer, sizeof(issuer) - 1) < 0) {
        ok = 0;
    }
    if (X509_NAME_get_text_by_NID(X509_get_subject_name(xContext->current_cert), NID_commonName, peer, 
            sizeof(peer) - 1) < 0) {
        ok = 0;
    }
#if FUTURE && KEEP
    if (ok && ssl->verifyDepth < depth) {
        if (error == 0) {
            error = X509_V_ERR_CERT_CHAIN_TOO_LONG;
        }
    }
#endif
    switch (error) {
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
        /* Normal self signed certificate */
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
#if FUTURE && KEEP
        if (ssl->verifyIssuer) {
            /* Issuer can't be verified */
            ok = 0;
        }
#endif
        break;

    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
    case X509_V_ERR_INVALID_CA:
    default:
        ok = 0;
        break;
    }
    if (ok) {
        trace(3, "OpenSSL: Certificate verified: subject %s", subject);
        trace(4, "OpenSSL: Issuer: %s", issuer);
        trace(4, "OpenSSL: Peer: %s", peer);
    } else {
        trace(1, "OpenSSL: Certification failed: subject %s (more trace at level 4)", subject);
        trace(4, "OpenSSL: Issuer: %s", issuer);
        trace(4, "OpenSSL: Peer: %s", peer);
        trace(4, "OpenSSL: Error: %d: %s", error, X509_verify_cert_error_string(error));
    }
    return ok;
}
#endif

/*
    These session callbacks use a simple chained list to store and retrieve the session information.
 */
static ssl_session *slist = NULL;
static ssl_session *cur, *prv;

static int getSession(ssl_context *ssl)
{
    time_t  t;
   
    t = time(NULL);

    if (ssl->resume == 0) {
        return 1;
    }
    cur = slist;
    prv = NULL;

    while (cur) {
        prv = cur;
        cur = cur->next;
        if (ssl->timeout != 0 && t - prv->start > ssl->timeout) {
            continue;
        }
        if (ssl->session->cipher != prv->cipher || ssl->session->length != prv->length) {
            continue;
        }
        if (memcmp(ssl->session->id, prv->id, prv->length) != 0) {
            continue;
        }
        memcpy(ssl->session->master, prv->master, 48);
        return 0;
    }
    return 1;
}

static int setSession(ssl_context *ssl)
{
    time_t  t;
   
    t = time(NULL);

    cur = slist;
    prv = NULL;
    while (cur) {
        if (ssl->timeout != 0 && t - cur->start > ssl->timeout) {
            /* expired, reuse this slot */
            break;
        }
        if (memcmp(ssl->session->id, cur->id, cur->length) == 0) {
            /* client reconnected */
            break;  
        }
        prv = cur;
        cur = cur->next;
    }
    if (cur == NULL) {
        cur = (ssl_session*) malloc(sizeof(ssl_session));
        if (cur == NULL) {
            return 1;
        }
        if (prv == NULL) {
            slist = cur;
        } else {
            prv->next = cur;
        }
    }
    memcpy(cur, ssl->session, sizeof(ssl_session));
    return 0;
}


static void estTrace(void *fp, int level, char *str)
{
    level += 3;
    if (level <= websGetTraceLevel()) {
        str = sclone(str);
        str[slen(str) - 1] = '\0';
        trace(level, "%s", str);
    }
}

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
