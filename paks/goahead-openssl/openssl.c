/*
    openssl.c - OpensSSL socket layer for GoAhead

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/************************************ Include *********************************/

#include    "goahead.h"

#if ME_COM_OPENSSL

#if ME_UNIX_LIKE
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* Clashes with WinCrypt.h */
#undef OCSP_RESPONSE

 /*
    Indent includes to bypass MakeMe dependencies
  */
 #include    <openssl/ssl.h>
 #include    <openssl/evp.h>
 #include    <openssl/rand.h>
 #include    <openssl/err.h>
 #include    <openssl/dh.h>

/************************************* Defines ********************************/

static SSL_CTX *sslctx = NULL;

typedef struct RandBuf {
    time_t      now;
    int         pid;
} RandBuf;

#define VERIFY_DEPTH 10

#ifndef ME_GOAHEAD_CURVE
    #define ME_GOAHEAD_CURVE "prime256v1"
#endif

/************************************ Forwards ********************************/

static RSA *rsaCallback(SSL *ssl, int isExport, int keyLength);
static int sslSetCertFile(char *certFile);
static int sslSetKeyFile(char *keyFile);
static int verifyX509Certificate(int ok, X509_STORE_CTX *ctx);

/************************************** Code **********************************/

PUBLIC int sslOpen()
{
    RandBuf     randBuf;

    trace(7, "Initializing SSL"); 

    randBuf.now = time(0);
    randBuf.pid = getpid();
    RAND_seed((void*) &randBuf, sizeof(randBuf));
#if ME_UNIX_LIKE
    trace(6, "OpenSsl: Before calling RAND_load_file");
    RAND_load_file("/dev/urandom", 256);
    trace(6, "OpenSsl: After calling RAND_load_file");
#endif

    CRYPTO_malloc_init(); 
#if !ME_WIN_LIKE
    OpenSSL_add_all_algorithms();
#endif
    SSL_library_init();
    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();

    if ((sslctx = SSL_CTX_new(SSLv23_server_method())) == 0) {
        error("Unable to create SSL context"); 
        return -1;
    }

    /*
          Set the client certificate verification locations
     */
    if (*ME_GOAHEAD_CA) {
        if ((!SSL_CTX_load_verify_locations(sslctx, ME_GOAHEAD_CA, NULL)) || (!SSL_CTX_set_default_verify_paths(sslctx))) {
            error("Unable to read cert verification locations");
            sslClose();
            return -1;
        }
    }
    /*
          Set the server certificate and key files
     */
    if (*ME_GOAHEAD_KEY && sslSetKeyFile(ME_GOAHEAD_KEY) < 0) {
        sslClose();
        return -1;
    }
    if (*ME_GOAHEAD_CERTIFICATE && sslSetCertFile(ME_GOAHEAD_CERTIFICATE) < 0) {
        sslClose();
        return -1;
    }
    SSL_CTX_set_tmp_rsa_callback(sslctx, rsaCallback);

    if (ME_GOAHEAD_VERIFY_PEER) {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verifyX509Certificate);
        SSL_CTX_set_verify_depth(sslctx, VERIFY_DEPTH);
    } else {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_NONE, verifyX509Certificate);
    }
    /*
          Set the certificate authority list for the client
     */
    if (ME_GOAHEAD_CA && *ME_GOAHEAD_CA) {
        SSL_CTX_set_client_CA_list(sslctx, SSL_load_client_CA_file(ME_GOAHEAD_CA));
    }
    if (ME_GOAHEAD_CIPHERS && *ME_GOAHEAD_CIPHERS) {
        SSL_CTX_set_cipher_list(sslctx, ME_GOAHEAD_CIPHERS);
    }

    /*
        Elliptic Curve initialization
     */
#if SSL_OP_SINGLE_ECDH_USE
{
    EC_KEY  *ecdh;
    cchar   *name;
    int      nid;

    name = ME_GOAHEAD_CURVE;
    if ((nid = OBJ_sn2nid(name)) == 0) {
        error("Unknown curve name \"%s\"", name);
        SSL_CTX_free(sslctx);
        return 0;
    }
    if ((ecdh = EC_KEY_new_by_curve_name(nid)) == 0) {
        error("Unable to create curve \"%s\"", name);
        SSL_CTX_free(sslctx);
        return 0;
    }
    SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_ECDH_USE);
    SSL_CTX_set_tmp_ecdh(sslctx, ecdh);
    EC_KEY_free(ecdh);
}
#endif

    SSL_CTX_set_options(sslctx, SSL_OP_ALL);
#if defined(SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) && ME_GOAHEAD_TLS_EMPTY_FRAGMENTS
    /* SSL_OP_ALL enables this. Only needed for ancient browsers like IE-6 */
    SSL_CTX_clear_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
#endif
    SSL_CTX_set_mode(sslctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_AUTO_RETRY | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
    SSL_CTX_set_options(sslctx, SSL_OP_MSIE_SSLV2_RSA_PADDING);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(sslctx, SSL_MODE_RELEASE_BUFFERS);
#endif
#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
    SSL_CTX_set_mode(sslctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
#endif

    /*
        Options set via main.me
     */
#if defined(ME_GOAHEAD_TICKET) && defined(SSL_OP_NO_TICKET)
    if (ME_GOAHEAD_TICKET) {
        SSL_CTX_clear_options(sslctx, SSL_OP_NO_TICKET);
    } else {
        SSL_CTX_set_options(sslctx, SSL_OP_NO_TICKET);
    }
#endif
#if defined(ME_GOAHEAD_COMPRESSION) && defined(SSL_OP_NO_COMPRESSION)
    if (ME_GOAHEAD_COMPRESSION) {
        SSL_CTX_clear_options(sslctx, SSL_OP_NO_COMPRESSION);
    } else {
        SSL_CTX_set_options(sslctx, SSL_OP_NO_COMPRESSION);
    }
#endif
#if defined(ME_GOAHEAD_RENEGOTIATE)
    RAND_bytes(resume, sizeof(resume));
    SSL_CTX_set_session_id_sslctx(sslctx, resume, sizeof(resume));
    #if defined(SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION)
        if (ME_GOAHEAD_RENEGOTIATE) {
            SSL_CTX_clear_options(sslctx, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
        } else {
            SSL_CTX_set_options(sslctx, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
        }
    #endif
#endif
#if defined(ME_GOAHEAD_TLS_EMPTY_FRAGMENTS)
    #if defined(SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS)
        if (ME_GOAHEAD_TLS_EMPTY_FRAGMENTS) {
            /* SSL_OP_ALL disables empty fragments. Only needed for ancient browsers like IE-6 on SSL-3.0/TLS-1.0 */
            SSL_CTX_clear_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
        } else {
            SSL_CTX_set_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
        }
    #endif
#endif
#if defined(ME_GOAHEAD_CACHE)
    SSL_CTX_sess_set_cache_size(sslctx, ME_GOAHEAD_CACHE);
#endif

    /*
        Disable both SSLv2 and SSLv3 by default - they are insecure
     */
    SSL_CTX_set_options(sslctx, SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(sslctx, SSL_OP_NO_SSLv3);

#if defined(SSL_OP_NO_TLSv1) && ME_GOAHEAD_TLS_NO_V1
    SSL_CTX_set_options(sslctx, SSL_OP_NO_TLSv1);
#endif
#if defined(SSL_OP_NO_TLSv1_1) && ME_GOAHEAD_TLS_NO_V1_1
    SSL_CTX_set_options(sslctx, SSL_OP_NO_TLSv1_1);
#endif
#if defined(SSL_OP_NO_TLSv1_2) && ME_GOAHEAD_TLS_NO_V1_2
    SSL_CTX_set_options(sslctx, SSL_OP_NO_TLSv1_2);
#endif

    /* 
        Ensure we generate a new private key for each connection
     */
    SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_DH_USE);
    return 0;
}


PUBLIC void sslClose()
{
    if (sslctx != NULL) {
        SSL_CTX_free(sslctx);
        sslctx = NULL;
    }
}


PUBLIC int sslUpgrade(Webs *wp)
{
    WebsSocket      *sptr;
    BIO             *bio;

    assert(wp);

    sptr = socketPtr(wp->sid);
    if ((wp->ssl = SSL_new(sslctx)) == 0) {
        return -1;
    }
    if ((bio = BIO_new_socket((int) sptr->sock, BIO_NOCLOSE)) == 0) {
        return -1;
    }
    SSL_set_bio(wp->ssl, bio, bio);
    SSL_set_accept_state(wp->ssl);
    SSL_set_app_data(wp->ssl, (void*) wp);
    return 0;
}
    

PUBLIC void sslFree(Webs *wp)
{
    if (wp->ssl) {
        SSL_set_shutdown(wp->ssl, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
        SSL_free(wp->ssl);
    }
}


/*
    Return the number of bytes read. Return -1 on errors and EOF. Distinguish EOF via mprIsSocketEof.
    If non-blocking, may return zero if no data or still handshaking.
 */
PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    WebsSocket      *sp;
    char            ebuf[ME_GOAHEAD_LIMIT_STRING];
    ulong           serror;
    int             rc, error, retries, i;

    if (wp->ssl == 0 || len <= 0) {
        assert(0);
        return -1;
    }
    /*  
        Limit retries on WANT_READ. If non-blocking and no data, then this can spin forever.
     */
    sp = socketPtr(wp->sid);
    retries = 5;
    for (i = 0; i < retries; i++) {
        rc = SSL_read(wp->ssl, buf, (int) len);
        if (rc < 0) {
            error = SSL_get_error(wp->ssl, rc);
            if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_CONNECT || error == SSL_ERROR_WANT_ACCEPT) {
                continue;
            }
            serror = ERR_get_error();
            ERR_error_string_n(serror, ebuf, sizeof(ebuf) - 1);
            trace(5, "SSL_read %s", ebuf);
        }
        break;
    }
    if (rc <= 0) {
        error = SSL_get_error(wp->ssl, rc);
        if (error == SSL_ERROR_WANT_READ) {
            rc = 0;
        } else if (error == SSL_ERROR_WANT_WRITE) {
            sleep(0);
            rc = 0;
        } else if (error == SSL_ERROR_ZERO_RETURN) {
            sp->flags |= SOCKET_EOF;
            rc = -1;
        } else if (error == SSL_ERROR_SYSCALL) {
            sp->flags |= SOCKET_EOF;
            rc = -1;
        } else if (error != SSL_ERROR_ZERO_RETURN) {
            /* SSL_ERROR_SSL */
            serror = ERR_get_error();
            ERR_error_string_n(serror, ebuf, sizeof(ebuf) - 1);
            trace(4, "OpenSSL: connection with protocol error: %s", ebuf);
            rc = -1;
            sp->flags |= SOCKET_EOF;
        }
    } else if (SSL_pending(wp->ssl) > 0) {
        socketHiddenData(sp, SSL_pending(wp->ssl), SOCKET_READABLE);
    }
    return rc;
}


PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    ssize   totalWritten;
    int     error, rc;

    if (wp->ssl == 0 || len <= 0) {
        assert(0);
        return -1;
    }
    totalWritten = 0;
    ERR_clear_error();

    do {
        rc = SSL_write(wp->ssl, buf, (int) len);
        trace(7, "OpenSSL: written %d, requested len %d", rc, len);
        if (rc <= 0) {
            error = SSL_get_error(wp->ssl, rc);
            if (error == SSL_ERROR_NONE) {
                break;
            } else if (error == SSL_ERROR_WANT_WRITE) {
                socketSetError(EAGAIN);
                return -1;
            } else if (error == SSL_ERROR_WANT_READ) {
                return -1;
            } else {
                trace(7, "OpenSSL: error %d", error);
                return -1;
            }
            break;
        }
        totalWritten += rc;
        buf = (void*) ((char*) buf + rc);
        len -= rc;
        trace(7, "OpenSSL: write: len %d, written %d, total %d, error %d", len, rc, totalWritten, 
            SSL_get_error(wp->ssl, rc));
    } while (len > 0);
    return totalWritten;
}


/*
    Set certificate file for SSL context
 */
static int sslSetCertFile(char *certFile)
{
    assert(sslctx);
    assert(certFile);

    if (sslctx == NULL) {
        return -1;
    }
    if (SSL_CTX_use_certificate_file(sslctx, certFile, SSL_FILETYPE_PEM) <= 0) {
        if (SSL_CTX_use_certificate_file(sslctx, certFile, SSL_FILETYPE_ASN1) <= 0) {
            error("Unable to read certificate file: %s", certFile); 
            return -1;
        }
        return -1;
    }
    /*        
          Confirm that the certificate and the private key jive.
     */
    if (!SSL_CTX_check_private_key(sslctx)) {
        return -1;
    }
    return 0;
}


/*
      Set key file for SSL context
 */
static int sslSetKeyFile(char *keyFile)
{
    assert(sslctx);
    assert(keyFile);

    if (sslctx == NULL) {
        return -1;
    }
    if (SSL_CTX_use_PrivateKey_file(sslctx, keyFile, SSL_FILETYPE_PEM) <= 0) {
        if (SSL_CTX_use_PrivateKey_file(sslctx, keyFile, SSL_FILETYPE_ASN1) <= 0) {
            error("Unable to read private key file: %s", keyFile); 
            return -1;
        }
        return -1;
    }
    return 0;
}


static int verifyX509Certificate(int ok, X509_STORE_CTX *xContext)
{
    X509            *cert;
    char            subject[260], issuer[260], peer[260];
    int             error, depth;
    
    subject[0] = issuer[0] = '\0';

    cert = X509_STORE_CTX_get_current_cert(xContext);
    error = X509_STORE_CTX_get_error(xContext);
    depth = X509_STORE_CTX_get_error_depth(xContext);

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
    if (ok && VERIFY_DEPTH < depth) {
        if (error == 0) {
            error = X509_V_ERR_CERT_CHAIN_TOO_LONG;
        }
    }
    switch (error) {
    case X509_V_OK:
        break;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        if (ME_GOAHEAD_VERIFY_ISSUER) {
            logmsg(3, "Self-signed certificate");
            ok = 0;
        }

    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        if (ME_GOAHEAD_VERIFY_ISSUER) {
            logmsg(3, "Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
        if (ME_GOAHEAD_VERIFY_ISSUER) {
            logmsg(3, "Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_INVALID_CA:
    default:
        logmsg(3, "Certificate verification error %d", error);
        ok = 0;
        break;
    }
    if (ok) {
        trace(3, "OpenSSL: Certificate verified: subject %s", subject);
    } else {
        trace(1, "OpenSSL: Certification failed: subject %s (more trace at level 4)", subject);
        trace(4, "OpenSSL: Error: %d: %s", error, X509_verify_cert_error_string(error));
    }
    trace(4, "OpenSSL: Issuer: %s", issuer);
    trace(4, "OpenSSL: Peer: %s", peer);
    return ok;
}


static RSA *rsaCallback(SSL *ssl, int isExport, int keyLength)
{
    static RSA *rsaTemp = NULL;

    if (rsaTemp == NULL) {
        rsaTemp = RSA_generate_key(keyLength, RSA_F4, NULL, NULL);
    }
    return rsaTemp;
}


#else
void opensslDummy() {}
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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
