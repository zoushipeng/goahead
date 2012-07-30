/*
  	md5.h
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/******************************************************************************/

#ifndef _h_MD5
#define _h_MD5 1

#ifndef WEBS_SSL_SUPPORT

#ifndef ulong32
typedef unsigned int	ulong32;
#endif

typedef struct {
    ulong32 lengthHi;
    ulong32 lengthLo;
    ulong32 state[4], curlen;
    unsigned char buf[64];
} psDigestContext_t;

typedef psDigestContext_t	psMd5Context_t;

extern void psMd5Init(psMd5Context_t *md);
extern void psMd5Update(psMd5Context_t *md, unsigned char *buf,
			unsigned int len);
extern int	psMd5Final(psMd5Context_t *md, unsigned char *hash);

/* Uncomment below for old API Compatibility */
/*
typedef psMdContext_t		MD5_CONTEXT;
#define MD5Init(A)			psMd5Init(A)
#define MD5Update(A, B, C)	psMd5Update(A, B, C);
#define MD5Final(A, B)		psMd5Final(B, A);
*/

#endif /* WEBS_SSL_SUPPORT */
#endif /* _h_MD5 */

/*
    @copy   default

    Copyright (c) PeerSec Networks, 2002-2010
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
