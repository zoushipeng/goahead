/* 
  	websSSL.h -- MatrixSSL Layer Header

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/******************************************************************************/

#ifndef _h_websSSL
#define _h_websSSL 1

/******************************** Description *********************************/
/* 
 *	Header file for the PeerSec MatrixSSL layer. This defines the interface to 
 *	integrate MatrixSSL into the GoAhead Webserver.
 */

/********************************* Includes ***********************************/

#ifdef WEBS_SSL_SUPPORT

#include "webs.h"
#include "matrixSSLSocket.h"
#include "uemf.h"

/********************************** Defines ***********************************/

#define DEFAULT_CERT_FILE   "./certSrv.pem"		/* Public key certificate */
#define DEFAULT_KEY_FILE    "./privkeySrv.pem"	/* Private key file */

typedef struct {
	sslConn_t* sslConn;
	struct websRec* wp;
} websSSL_t;

/*************************** User Code Prototypes *****************************/

extern int	websSSLOpen();
extern int	websSSLIsOpen();
extern void websSSLClose();
#ifdef WEBS_WHITELIST_SUPPORT
extern int	websRequireSSL(char *url);
#endif /* WEBS_WHITELIST_SUPPORT */

/*************************** Internal Prototypes *****************************/

extern int	websSSLWrite(websSSL_t *wsp, char_t *buf, int nChars);
extern int	websSSLGets(websSSL_t *wsp, char_t **buf);
extern int	websSSLRead(websSSL_t *wsp, char_t *buf, int nChars);
extern int	websSSLEof(websSSL_t *wsp);

extern int	websSSLFree(websSSL_t *wsp);
extern int	websSSLFlush(websSSL_t *wsp);

extern int	websSSLSetKeyFile(char_t *keyFile);
extern int	websSSLSetCertFile(char_t *certFile);

#endif /* WEBS_SSL_SUPPORT */

#endif /* _h_websSSL */

/*****************************************************************************/
/*
    @copy   default

    Copyright (c) PeerSec Networks, 2002-2012. All Rights Reserved.
    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.
    Copyright (c) GoAhead Software, 2003. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis GoAhead open source license or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details.

    This software is open source; you can redistribute it and/or modify it
    under the terms of the Embedthis GoAhead Open Source License as published 
    at:

        http://embedthis.com/products/goahead/goahead-license.pdf 

    This Embedthis GoAhead Open Source license does NOT generally permit 
    incorporating this software into proprietary programs. If you are unable 
    to comply with the Embedthis Open Source license, you must acquire a 
    commercial license to use this software. Commercial licenses for this 
    software and support services are available from Embedthis Software at:

        http://embedthis.com

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
