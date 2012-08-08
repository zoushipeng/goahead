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
static int verifyCallback(int ok, X509_STORE_CTX *ctx);

static int sslVerifyDepth = 0;
static int sslVerifyError = X509_V_OK;


typedef struct RandBuf {
    time_t      now;
    int         pid;
} RandBuf;

/************************************** Code **********************************/

int sslOpen()
{
    RandBuf             randBuf;
    char_t              *caFile, *caPath;
	const SSL_METHOD	*meth;

	trace(7, T("Initializing SSL\n")); 

    randBuf.now = time(0);
    randBuf.pid = getpid();
    RAND_seed((void*) &randBuf, sizeof(randBuf));
#if BIT_UNIX_LIKE
    trace(6, T("OpenSsl: Before calling RAND_load_file"));
    RAND_load_file("/dev/urandom", 256);
    trace(6, T("OpenSsl: After calling RAND_load_file"));
#endif

    CRYPTO_malloc_init(); 
#if !BIT_WIN_LIKE
    OpenSSL_add_all_algorithms();
#endif
	SSL_library_init();
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	meth = SSLv23_server_method();
	if ((sslctx = SSL_CTX_new(meth)) == 0) {
		error(E_L, E_USER, T("Unable to create SSL context")); 
		return -1;
	}
	SSL_CTX_set_quiet_shutdown(sslctx, 1);
	SSL_CTX_set_options(sslctx, 0);
	SSL_CTX_sess_set_cache_size(sslctx, 128);

    /*
      	Set the certificate verification locations
     */
    caFile = *BIT_CA_FILE ? BIT_CA_FILE : 0;
    caPath = *BIT_CA_PATH ? BIT_CA_PATH : 0;
    if (caFile || caPath) {
        if ((!SSL_CTX_load_verify_locations(sslctx, caFile, caPath)) || (!SSL_CTX_set_default_verify_paths(sslctx))) {
            error(E_L, E_LOG, T("Unable to set cert verification locations"));
            websSSLClose();
            return -1;
        }
    }
    /*
      	Set the certificate and key files for the SSL context
     */
    if (*BIT_KEY && websSSLSetKeyFile(BIT_KEY) < 0) {
		websSSLClose();
		return -1;
    }
    if (*BIT_CERTIFICATE && websSSLSetCertFile(BIT_CERTIFICATE) < 0) {
		websSSLClose();
		return -1;
    }
#if UNUSED
	if (setCerts(sslctx, BIT_CERTIFICATE, BIT_KEY) != 0) {
		websSSLClose();
		return -1;
	}
#endif
	SSL_CTX_set_tmp_rsa_callback(sslctx, rsaCallback);
	SSL_CTX_set_verify(sslctx, SSL_VERIFY_NONE, verifyCallback);

    /*
      	Set the certificate authority list for the client
     */
    if (BIT_CA_FILE && *BIT_CA_FILE) {
        SSL_CTX_set_client_CA_list(sslctx, SSL_load_client_CA_file(BIT_CA_FILE));
    }
    return 0;
}


void sslClose()
{
	if (sslctx != NULL) {
		SSL_CTX_free(sslctx);
		sslctx = NULL;
	}
}


int sslAccept(webs_t wp) 
{
	socket_t	*sptr;
	SSL			*ssl;
	BIO			*bio, *bioSSL, *bioSock;
    long        ret;
	int			sock;

	a_assert (wp);
	a_assert(websValid(wp));

	sptr = socketPtr(wp->sid);
	a_assert(sptr);
	sock = sptr->sock;

    /*
      	Create a new BIO and SSL session for this web request
     */
	bio = BIO_new(BIO_f_buffer());
	a_assert(bio);

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
    websReadEvent(wp);
    return (int) ret;
}
    

ssize sslRead(webs_t wp, char *buf, ssize len)
{
    return (ssize) BIO_read(wp->bio, buf, (int) len);
}


void sslFlush(webs_t wp)
{
    a_assert(wp);

    if (!wp || !wp->bio) {
        return;
    }
    (void) BIO_flush(wp->bio);
}


ssize sslWrite(webs_t wp, char *buf, ssize len)
{
    a_assert(wp);
    a_assert(buf);

    if (!wp || !wp->bio) {
        return -1;
    }
    return (ssize) BIO_write(wp->bio, buf, (int) len);
}


#if UNUSED
/*
  	Set the SSL certificate and key for the SSL context
 */
static int setCerts(SSL_CTX *ctx, char *cert, char *key)
{
	a_assert (ctx);
	a_assert (cert);

    if (key == NULL) {
        key = cert;
    }
	if (cert && SSL_CTX_use_certificate_chain_file(ctx, cert) <= 0) {
        if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_ASN1) <= 0) {
            error(E_L, E_LOG, T("Unable to set certificate file: %s"), cert); 
            return -1;
        }
    }
    if (key && SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0) {
        if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0) {
            error(E_L, E_LOG, T("Unable to set private key file: %s"), key); 
            return -1;
        }
        /*		
            Now we know that a key and cert have been set against the SSL context 
         */
		if (!SSL_CTX_check_private_key(ctx)) {
			error(E_L, E_LOG, T("Check of private key file <%s> FAILED"), key); 
			return -1;
		}
	}
	return 0;
}
#endif


/*
    Set certificate file for SSL context
 */
int websSSLSetCertFile(char_t *certFile)
{
	a_assert (sslctx);
	a_assert (certFile);

	if (sslctx == NULL) {
		return -1;
	}
	if (SSL_CTX_use_certificate_file(sslctx, certFile, SSL_FILETYPE_PEM) <= 0) {
        if (SSL_CTX_use_certificate_file(sslctx, certFile, SSL_FILETYPE_ASN1) <= 0) {
            error(E_L, E_LOG, T("Unable to set certificate file: %s"), certFile); 
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
int websSSLSetKeyFile(char_t *keyFile)
{
	a_assert (sslctx);
	a_assert (keyFile);

	if (sslctx == NULL) {
		return -1;
	}
	if (SSL_CTX_use_PrivateKey_file(sslctx, keyFile, SSL_FILETYPE_PEM) <= 0) {
        if (SSL_CTX_use_PrivateKey_file(sslctx, keyFile, SSL_FILETYPE_PEM) <= 0) {
            error(E_L, E_LOG, T("Unable to set private key file: %s"), keyFile); 
            return -1;
        }
		return -1;
	}
#if UNUSED
    /*
        Confirm that the certificate and the private key jive.
     */
	if (!SSL_CTX_check_private_key(sslctx)) {
		return -1;
	}
#endif
	return 0;
}



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


static RSA *rsaCallback(SSL *ssl, int isExport, int keyLength)
{
	static RSA *rsaTemp = NULL;

	if (rsaTemp == NULL) {
		rsaTemp = RSA_generate_key(keyLength, RSA_F4, NULL, NULL);
	}
	return rsaTemp;
}


void sslFree(webs_t wp)
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
