/*
    openssl.c - OpensSSL socket layer for GoAhead

    This is the interface between GoAhead and the OpenSSL stack.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/************************************ Include *********************************/

#include    "goahead.h"

#if ME_COM_OPENSSL

#if ME_UNIX_LIKE
    /*
        Mac OS X OpenSSL stack is deprecated. Suppress those warnings.
     */
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
/*
    Default ciphers from Mozilla (https://wiki.mozilla.org/Security/Server_Side_TLS) without SSLv3 ciphers.
    TLSv1 and TLSv2 only. Recommended RSA and DH parameter size: 2048 bits.
 */
#define OPENSSL_DEFAULT_CIPHERS "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-  AES128-GCM-SHA256:ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA:ECDHE-RSA-AES128-SHA:DHE-RSA-AES256-  SHA256:DHE-RSA-AES128-SHA256:DHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA:ECDHE-RSA-DES-CBC3-SHA:EDH-RSA-DES-CBC3-SHA:AES256-GCM-   SHA384:AES128-GCM-SHA256:AES256-SHA256:AES128-SHA256:AES256-SHA:AES128-SHA:DES-CBC3-SHA:HIGH:!aNULL:!eNULL:!EXPORT:!DES:!    MD5:!PSK:!RC4:!SSLv3"

/*
    OpenSSL context (singleton)
 */
static SSL_CTX *sslctx = NULL;

typedef struct RandBuf {
    time_t      now;
    int         pid;
} RandBuf;

#define VERIFY_DEPTH 10

#ifndef ME_GOAHEAD_CURVE
    #define ME_GOAHEAD_CURVE "prime256v1"
#endif

/*
    DH parameters
 */
static DH *dhKey;

/************************************ Forwards ********************************/

static DH   *dhcallback(SSL *handle, int is_export, int keylength);
static DH   *getDhKey(cchar *path);
static int  sslSetCertFile(char *certFile);
static int  sslSetKeyFile(char *keyFile);
static int  verifyPeerCertificate(int ok, X509_STORE_CTX *ctx);

/************************************** Code **********************************/
/*
    Open the SSL module
 */
PUBLIC int sslOpen()
{
    RandBuf     randBuf;
    uchar       resume[16];
    cchar       *ciphers;

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
    if (ME_GOAHEAD_VERIFY_PEER) {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verifyPeerCertificate);
        SSL_CTX_set_verify_depth(sslctx, VERIFY_DEPTH);
    } else {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_NONE, verifyPeerCertificate);
    }

    /*
        Configure DH parameters
     */
    dhKey = getDhKey(ME_GOAHEAD_DHPARAMS);
    SSL_CTX_set_tmp_dh_callback(sslctx, dhcallback);

    /*
          Set the certificate authority list for the client
     */
    if (ME_GOAHEAD_CA && *ME_GOAHEAD_CA) {
        SSL_CTX_set_client_CA_list(sslctx, SSL_load_client_CA_file(ME_GOAHEAD_CA));
    }
    if (ME_GOAHEAD_CIPHERS && *ME_GOAHEAD_CIPHERS) {
        ciphers = ME_GOAHEAD_CIPHERS;
    } else {
        ciphers = OPENSSL_DEFAULT_CIPHERS;
    }
    if (ciphers) {
        trace(5, "Using OpenSSL ciphers: %s", ciphers);
        if (SSL_CTX_set_cipher_list(sslctx, ciphers) != 1) {
            error("Unable to set cipher list \"%s\". %s", ciphers);
            sslClose();
            return -1;
        }
    }

    /*
        Define default OpenSSL options
     */
    SSL_CTX_set_options(sslctx, SSL_OP_ALL);

    /* 
        Ensure we generate a new private key for each connection
     */
    SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_DH_USE);

    /*
        Define a session reuse context
     */
    RAND_bytes(resume, sizeof(resume));
    SSL_CTX_set_session_id_context(sslctx, resume, sizeof(resume));

    /*
        Elliptic Curve initialization
     */
#if SSL_OP_SINGLE_ECDH_USE
    #ifdef SSL_CTX_set_ecdh_auto
        SSL_CTX_set_ecdh_auto(sslctx, 1);
    #else
        {
            EC_KEY  *ecdh;
            cchar   *name;
            int      nid;

            name = ME_GOAHEAD_CURVE;
            if ((nid = OBJ_sn2nid(name)) == 0) {
                error("Unknown curve name \"%s\"", name);
                sslClose();
                return -1;
            }
            if ((ecdh = EC_KEY_new_by_curve_name(nid)) == 0) {
                error("Unable to create curve \"%s\"", name);
                sslClose();
                return -1;
            }
            SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_ECDH_USE);
            SSL_CTX_set_tmp_ecdh(sslctx, ecdh);
            EC_KEY_free(ecdh);
        }
    #endif
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
        Select the required protocols
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
        Options set via main.me mpr.ssl.*
     */
#if defined(SSL_OP_NO_TICKET)
    /*
        Ticket based session reuse is enabled by default
     */
    #if defined(ME_GOAHEAD_TLS_TICKET)
        if (ME_GOAHEAD_TLS_TICKET) {
            SSL_CTX_clear_options(sslctx, SSL_OP_NO_TICKET);
        } else {
            SSL_CTX_set_options(sslctx, SSL_OP_NO_TICKET);
        }
    #else
        SSL_CTX_clear_options(sslctx, SSL_OP_NO_TICKET);
    #endif
#endif

#if defined(SSL_OP_NO_COMPRESSION)
    /*
        Use of compression is not secure. Disabled by default.
     */
    #if defined(ME_GOAHEAD_TLS_COMPRESSION)
        if (ME_GOAHEAD_TLS_COMPRESSION) {
            SSL_CTX_clear_options(sslctx, SSL_OP_NO_COMPRESSION);
        } else {
            SSL_CTX_set_options(sslctx, SSL_OP_NO_COMPRESSION);
        }
    #else
        /*
            CRIME attack targets compression
         */
        SSL_CTX_clear_options(sslctx, SSL_OP_NO_COMPRESSION);
    #endif
#endif

#if defined(SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS)
    /*
        Disables a countermeasure against a SSL 3.0/TLS 1.0 protocol vulnerability affecting CBC ciphers.
        Defaults to true.
     */
    #if defined(ME_GOAHEAD_TLS_EMPTY_FRAGMENTS)
        if (ME_GOAHEAD_TLS_EMPTY_FRAGMENTS) {
            /* SSL_OP_ALL disables empty fragments. Only needed for ancient browsers like IE-6 on SSL-3.0/TLS-1.0 */
            SSL_CTX_clear_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
        } else {
            SSL_CTX_set_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
        }
    #else
        SSL_CTX_set_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
    #endif
#endif

#if defined(ME_GOAHEAD_TLS_CACHE)
    /*
        Set the number of sessions supported. Default in OpenSSL is 20K.
     */
    SSL_CTX_sess_set_cache_size(sslctx, ME_GOAHEAD_TLS_CACHE);
#else
    SSL_CTX_sess_set_cache_size(sslctx, 256);
#endif

    return 0;
}


/*
    Close the SSL module
 */
PUBLIC void sslClose()
{
    if (sslctx != NULL) {
        SSL_CTX_free(sslctx);
        sslctx = NULL;
    }
}


/*
    Upgrade a socket to use SSL
 */
PUBLIC int sslUpgrade(Webs *wp)
{
    WebsSocket      *sptr;
    BIO             *bio;

    assert(wp);

    sptr = socketPtr(wp->sid);
    if ((wp->ssl = SSL_new(sslctx)) == 0) {
        return -1;
    }
    /*
        Create a socket bio. We don't use the BIO except as storage for the fd.
     */
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
        wp->ssl = 0;
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
        return -1;
    }
    totalWritten = 0;
    error = 0;
    ERR_clear_error();

    do {
        rc = SSL_write(wp->ssl, buf, (int) len);
        trace(7, "OpenSSL: written %d, requested len %d", rc, len);
        if (rc <= 0) {
            error = SSL_get_error(wp->ssl, rc);
            if (error == SSL_ERROR_NONE || error == SSL_ERROR_WANT_WRITE) {
                break;
            }
            trace(7, "OpenSSL: error %d", error);
            return -1;
        }
        totalWritten += rc;
        buf = (void*) ((char*) buf + rc);
        len -= rc;
        trace(7, "OpenSSL: write: len %d, written %d, total %d", len, rc, totalWritten); 
    } while (len > 0);

    if (totalWritten == 0 && error == SSL_ERROR_WANT_WRITE) {
        socketSetError(EAGAIN);
        return -1;
    }
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


static int verifyPeerCertificate(int ok, X509_STORE_CTX *xContext)
{
    X509    *cert;
    char    subject[260], issuer[260], peer[260];
    int     error, depth;
    
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


/*
    Get the DH parameters. This tries to read the 
    Default DH parameters used if the dh.pem file is not supplied.

    Generated via
        openssl dhparam -C 2048 -out /dev/null >dhparams.h

    Though not essential, you should generate your own local DH parameters 
    and supply a dh.pem file to make it a bit harder for attackers.
 */

static DH *getDhKey(cchar *path)
{
	static unsigned char dh2048_p[] = {
		0xDA,0xDD,0x26,0xAA,0xBF,0x0B,0x2D,0xC5,0x83,0x20,0x26,0xD3,
		0x99,0x62,0x76,0xF6,0xF5,0x55,0x7F,0x84,0xB4,0x6A,0x7F,0x3E,
		0xBF,0x1C,0xF8,0xB6,0xE9,0xD3,0x40,0x0A,0xBB,0xED,0xD9,0xFF,
		0x3F,0x51,0x38,0xCC,0xFD,0x89,0x63,0x4C,0x0F,0x6C,0x9E,0x52,
		0x90,0x14,0xC4,0x55,0x34,0xE8,0xF1,0xD9,0xEB,0x43,0xD4,0xAD,
		0x27,0x67,0x4C,0x3A,0x0F,0x18,0x86,0x96,0x47,0x1D,0xA1,0x17,
		0xE3,0x30,0xEF,0x3B,0x7D,0x34,0x45,0x4C,0x11,0x86,0xFB,0x29,
		0xFC,0xB5,0x05,0x1B,0xC3,0xA8,0x22,0x38,0xC9,0xEA,0xD7,0x2D,
		0x02,0x96,0x2D,0xD9,0x77,0xF8,0x87,0x31,0x96,0xAA,0x6A,0xFF,
		0xEA,0xC1,0x78,0xBE,0x12,0xA2,0x78,0xBD,0x9A,0x78,0x7C,0xA5,
		0x4D,0x2F,0x3B,0xE8,0x6F,0xAD,0xE6,0xBE,0x21,0x3E,0x6C,0x7D,
		0xB5,0x53,0xE1,0x1E,0x83,0x81,0xBD,0x98,0x54,0x8E,0xE5,0x54,
		0xEC,0x43,0x09,0x54,0x9A,0xDC,0x7C,0xC8,0xE9,0xBC,0x20,0x50,
		0x31,0x28,0xE9,0xF5,0x99,0x60,0xE2,0x40,0x48,0x57,0x8D,0xD9,
		0x29,0xF5,0x8B,0x22,0xDE,0x93,0xE2,0x56,0x0B,0x76,0xE3,0x8B,
		0xC4,0x37,0x2F,0xD2,0xC1,0x34,0xF5,0x9B,0x12,0xD8,0x2B,0xE2,
		0x98,0xBA,0x0C,0xBB,0xDC,0x7A,0x65,0x7C,0x2D,0xC2,0x56,0x01,
		0x94,0x9F,0xB5,0xAE,0xC2,0xAA,0x6B,0x42,0x1B,0x54,0x36,0xE3,
		0x86,0xAC,0x21,0x93,0xDD,0x8B,0xD1,0x0D,0xEF,0x39,0x20,0x14,
		0x29,0xA1,0xB1,0xEE,0xE7,0xB1,0xA3,0x29,0x6C,0xD5,0xE6,0xD6,
		0x23,0x20,0xDC,0xDC,0xFA,0x0A,0x06,0x81,0xB3,0xF4,0xEB,0xCE,
		0xA6,0xD7,0x23,0x93,
    };
	static unsigned char dh2048_g[] = {
		0x02,
    };
	DH      *dh;
    BIO     *bio;

    dh = 0;
    if (path && *path) {
        if ((bio = BIO_new_file(path, "r")) != 0) {
            dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
        }
    }
    if (dh == 0) {
        if ((dh = DH_new()) == 0) {
            return 0;
        }
        dh->p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), NULL);
        dh->g = BN_bin2bn(dh2048_g, sizeof(dh2048_g), NULL);
        if ((dh->p == 0) || (dh->g == 0)) { 
            DH_free(dh); 
            return 0;
        }
    }
	return dh;
}


/*
    Set the ephemeral DH key
 */
static DH *dhcallback(SSL *handle, int isExport, int keyLength)
{
    return dhKey;
}

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
