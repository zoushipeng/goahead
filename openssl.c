/*
    openssl.c - SSL socket layer for OpenSSL

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/************************************ Include *********************************/

#include    "goahead.h"

#if BIT_PACK_OPENSSL

/************************************* Defines ********************************/


#if UNUSED
#if BIT_PACK_OPENSSL
    #define SSLC_add_all_algorithms()	SSLeay_add_all_algorithms()
#else
    extern void SSLC_add_all_algorithms(void);
#endif
#endif

extern RSA *RSA_new(void);
static SSL_CTX *sslctx = NULL;

#if UNUSED
static int setCerts(SSL_CTX *ctx, char *certFile, char *keyFile);
#endif

static RSA *rsaCallback(SSL *ssl, int isExport, int keyLength);
static int verifyX509Certificate(int ok, X509_STORE_CTX *ctx);

#if UNUSED
static int sslVerifyDepth = 0;
static int sslVerifyError = X509_V_OK;
#endif

typedef struct RandBuf {
    time_t      now;
    int         pid;
} RandBuf;

/************************************** Code **********************************/

int sslOpen()
{
    RandBuf             randBuf;
    char_t              *caFile, *caPath;

	trace(7, T("Initializing SSL\n")); 

    randBuf.now = time(0);
    randBuf.pid = getpid();
    RAND_seed((void*) &randBuf, sizeof(randBuf));
#if BIT_UNIX_LIKE
    trace(6, T("OpenSsl: Before calling RAND_load_file\n"));
    RAND_load_file("/dev/urandom", 256);
    trace(6, T("OpenSsl: After calling RAND_load_file\n"));
#endif

    CRYPTO_malloc_init(); 
#if !BIT_WIN_LIKE
    OpenSSL_add_all_algorithms();
#endif
	SSL_library_init();
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	if ((sslctx = SSL_CTX_new(SSLv23_server_method())) == 0) {
		error(T("Unable to create SSL context")); 
		return -1;
	}

    /*
      	Set the certificate verification locations
     */
    caFile = *BIT_CA_FILE ? BIT_CA_FILE : 0;
    caPath = *BIT_CA_PATH ? BIT_CA_PATH : 0;
    if (caFile || caPath) {
        if ((!SSL_CTX_load_verify_locations(sslctx, caFile, caPath)) || (!SSL_CTX_set_default_verify_paths(sslctx))) {
            error(T("Unable to set cert verification locations"));
            sslClose();
            return -1;
        }
    }
    /*
      	Set the certificate and key files for the SSL context
     */
    if (*BIT_KEY && sslSetKeyFile(BIT_KEY) < 0) {
		sslClose();
		return -1;
    }
    if (*BIT_CERTIFICATE && sslSetCertFile(BIT_CERTIFICATE) < 0) {
		sslClose();
		return -1;
    }
#if UNUSED
	if (setCerts(sslctx, BIT_CERTIFICATE, BIT_KEY) != 0) {
		sslClose();
		return -1;
	}
#endif
	SSL_CTX_set_tmp_rsa_callback(sslctx, rsaCallback);

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
    /*
      	Set the certificate authority list for the client
     */
    if (BIT_CA_FILE && *BIT_CA_FILE) {
        SSL_CTX_set_client_CA_list(sslctx, SSL_load_client_CA_file(BIT_CA_FILE));
    }

#if UNUSED
	SSL_CTX_set_quiet_shutdown(sslctx, 1);
#endif
	SSL_CTX_set_options(sslctx, SSL_OP_ALL);
	SSL_CTX_sess_set_cache_size(sslctx, 128);
#ifdef SSL_OP_NO_TICKET
    SSL_CTX_set_options(sslctx, SSL_OP_NO_TICKET);
#endif
#ifdef SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION                                                       
    SSL_CTX_set_options(sslctx, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
#endif                                                                                                     
    SSL_CTX_set_mode(sslctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_options(sslctx, SSL_OP_NO_SSLv2);

#if FUTURE && KEEP
    /* 
        Ensure we generate a new private key for each connection
     */
    SSL_CTX_set_options(context, SSL_OP_SINGLE_DH_USE);
#endif
    return 0;
}


void sslClose()
{
	if (sslctx != NULL) {
		SSL_CTX_free(sslctx);
		sslctx = NULL;
	}
}


int sslUpgrade(Webs *wp)
{
    socket_t        *sptr;

    gassert(wp);

	sptr = socketPtr(wp->sid);
    if ((wp->ssl = SSL_new(sslctx)) == 0) {
        return -1;
    }
    if ((wp->bio = BIO_new_socket(sptr->sock, BIO_NOCLOSE)) == 0) {
        return -1;
    }
    SSL_set_bio(wp->ssl, wp->bio, wp->bio);
    SSL_set_accept_state(wp->ssl);
    SSL_set_app_data(wp->ssl, (void*) wp);
    return 0;
}


#if UNUSED
int sslAccept(Webs *wp) 
{
	socket_t	*sptr;
	SSL			*ssl;
	BIO			*bio, *bioSSL, *bioSock;
    long        ret;
	int			sock;

	gassert (wp);
	gassert(websValid(wp));

	sptr = socketPtr(wp->sid);
	gassert(sptr);
	sock = sptr->sock;

    /*
      	Create a new BIO and SSL session for this web request
     */
	bio = BIO_new(BIO_f_buffer());
	gassert(bio);

	if (!BIO_set_write_buffer_size(bio, 128)) {
		return -1;
	}
	if ((ssl = (SSL*) SSL_new(sslctx)) == 0) {
		return -1;
	}
	SSL_set_session(ssl, NULL);
	if ((bioSSL = BIO_new(BIO_f_ssl())) == 0) {
        return -1;
    }
	if ((bioSock = BIO_new_socket(sock, BIO_NOCLOSE)) == 0) {
        return -1;
    }
	SSL_set_bio(ssl, bioSock, bioSock);
	SSL_set_accept_state(ssl);
	ret = BIO_set_ssl(bioSSL, ssl, BIO_CLOSE);
	BIO_push(bio, bioSSL);
	wp->bio = bio;
	wp->ssl = ssl;
    return (int) ret;
}
#endif
    

ssize sslRead(Webs *wp, char *buf, ssize len)
{
    socket_t        *sp;
#if UNUSED
    X509_NAME       *xSubject;
    X509            *cert;
    char            subject[260], issuer[260], peer[260];
#endif
    char            ebuf[BIT_LIMIT_STRING];
    ulong           serror;
    int             rc, error, retries, i;

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
#if UNUSED
    if (rc > 0 && !(sp->flags & SOCKET_TRACED)) {
        cert = SSL_get_peer_certificate(wp->ssl);
        if (cert == 0) {
            trace(4, "OpenSSL: client supplied no certificate");
        } else {
            xSubject = X509_get_subject_name(cert);
            X509_NAME_oneline(xSubject, subject, sizeof(subject) -1);
            X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer) -1);
            X509_NAME_get_text_by_NID(xSubject, NID_commonName, peer, sizeof(peer) - 1);
            trace(4, "OpenSSL Subject %s", subject);
            trace(4, "OpenSSL Issuer: %s", issuer);
            trace(4, "OpenSSL Peer: %s", peer);
            X509_free(cert);
        }
        sp->flags |= SOCKET_TRACED;
    }
#endif
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
        sp->flags |= SOCKET_PENDING;
        socketReservice(wp->sid);
    }
    return rc;
}


void sslFlush(Webs *wp)
{
#if UNUSED
    gassert(wp);

    if (!wp || !wp->bio) {
        return;
    }
    (void) BIO_flush(wp->bio);
#endif
}


ssize sslWrite(Webs *wp, char *buf, ssize len)
{
    ssize   totalWritten;
    int     rc;

    if (wp->bio == 0 || wp->ssl == 0 || len <= 0) {
        gassert(0);
        return -1;
    }
    totalWritten = 0;
    ERR_clear_error();

    do {
        rc = SSL_write(wp->ssl, buf, (int) len);
        trace(7, "OpenSSL: written %d, requested len %d", rc, len);
        if (rc <= 0) {
            rc = SSL_get_error(wp->ssl, rc);
            if (rc == SSL_ERROR_WANT_WRITE) {
                sleep(0);
                continue;
            } else if (rc == SSL_ERROR_WANT_READ) {
                //  AUTO-RETRY should stop this
                gassert(0);
                return -1;
            } else {
                return -1;
            }
        }
        totalWritten += rc;
        buf = (void*) ((char*) buf + rc);
        len -= rc;
        trace(7, "OpenSSL: write: len %d, written %d, total %d, error %d", len, rc, totalWritten, 
            SSL_get_error(wp->ssl, rc));
    } while (len > 0);
    return totalWritten;
}


#if UNUSED
/*
  	Set the SSL certificate and key for the SSL context
 */
static int setCerts(SSL_CTX *ctx, char *cert, char *key)
{
	gassert (ctx);
	gassert (cert);

    if (key == NULL) {
        key = cert;
    }
	if (cert && SSL_CTX_use_certificate_chain_file(ctx, cert) <= 0) {
        if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_ASN1) <= 0) {
            error(T("Unable to set certificate file: %s"), cert); 
            return -1;
        }
    }
    if (key && SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0) {
        if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0) {
            error(T("Unable to set private key file: %s"), key); 
            return -1;
        }
        /*		
            Now we know that a key and cert have been set against the SSL context 
         */
		if (!SSL_CTX_check_private_key(ctx)) {
			error(T("Check of private key file <%s> FAILED"), key); 
			return -1;
		}
	}
	return 0;
}
#endif


/*
    Set certificate file for SSL context
 */
int sslSetCertFile(char_t *certFile)
{
	gassert (sslctx);
	gassert (certFile);

	if (sslctx == NULL) {
		return -1;
	}
	if (SSL_CTX_use_certificate_file(sslctx, certFile, SSL_FILETYPE_PEM) <= 0) {
        if (SSL_CTX_use_certificate_file(sslctx, certFile, SSL_FILETYPE_ASN1) <= 0) {
            error(T("Unable to set certificate file: %s"), certFile); 
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
int sslSetKeyFile(char_t *keyFile)
{
	gassert (sslctx);
	gassert (keyFile);

	if (sslctx == NULL) {
		return -1;
	}
	if (SSL_CTX_use_PrivateKey_file(sslctx, keyFile, SSL_FILETYPE_PEM) <= 0) {
        if (SSL_CTX_use_PrivateKey_file(sslctx, keyFile, SSL_FILETYPE_PEM) <= 0) {
            error(T("Unable to set private key file: %s"), keyFile); 
            return -1;
        }
		return -1;
	}
#if UNUSED
    //  MOB - re-enable
    /*
        Confirm that the certificate and the private key jive.
     */
	if (!SSL_CTX_check_private_key(sslctx)) {
		return -1;
	}
#endif
	return 0;
}



#if UNUSED
//  MOB - compare with MPR
static int verifyCallback(int ok, X509_STORE_CTX *ctx)
{
	char	buf[256];
	X509	*errCert;
	int		err;
	int		depth;

	errCert =	X509_STORE_CTX_get_current_cert(ctx);
	err =		X509_STORE_CTX_get_error(ctx);
	depth =		X509_STORE_CTX_get_error_depth(ctx);

	X509_NAME_oneline(X509_get_subject_name(errCert), buf, 256);

	if (!ok) {
		if (sslVerifyDepth >= depth)	{
			ok = 1;
			sslVerifyError = X509_V_OK;
		} else {
			ok=0;
			sslVerifyError = X509_V_ERR_CERT_CHAIN_TOO_LONG;
		}
	}

	switch (err)	{
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
		X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), buf, 256);
		break;

	case X509_V_ERR_CERT_NOT_YET_VALID:
	case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
	case X509_V_ERR_CERT_HAS_EXPIRED:
	case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
		break;
	}
	return ok;
}
#endif


static int verifyX509Certificate(int ok, X509_STORE_CTX *xContext)
{
    X509            *cert;
    Webs            *wp;
    SSL             *handle;
    char            subject[260], issuer[260], peer[260];
    int             error, depth;
    
    subject[0] = issuer[0] = '\0';

    handle = (SSL*) X509_STORE_CTX_get_app_data(xContext);
    wp = (Webs*) SSL_get_app_data(handle);

    cert = X509_STORE_CTX_get_current_cert(xContext);
    depth = X509_STORE_CTX_get_error_depth(xContext);
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


static RSA *rsaCallback(SSL *ssl, int isExport, int keyLength)
{
	static RSA *rsaTemp = NULL;

	if (rsaTemp == NULL) {
		rsaTemp = RSA_generate_key(keyLength, RSA_F4, NULL, NULL);
	}
	return rsaTemp;
}


void sslFree(Webs *wp)
{
    /* 
        Re-use sessions
     */
	if (wp->ssl != NULL) {
		SSL_set_shutdown(wp->ssl, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
	}
	if (wp->bio != NULL) {
		BIO_free_all(wp->bio);
	}
}

#endif

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
