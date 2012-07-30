/* 
  	websda.h -- GoAhead Digest Access Authentication public header
  
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_WEBSDA
#define _h_WEBSDA 1

/******************************** Description *********************************/

/* 
 *	GoAhead Digest Access Authentication header. This defines the Digest 
 *	access authentication public APIs.  Include this header for files that 
 *	use DAA functions
 */

/********************************* Includes ***********************************/

#include	"uemf.h"
#include	"webs.h"

/****************************** Definitions ***********************************/

extern char_t 	*websCalcNonce(webs_t wp);
extern char_t 	*websCalcOpaque(webs_t wp);
extern char_t 	*websCalcDigest(webs_t wp);
extern char_t 	*websCalcUrlDigest(webs_t wp);

#endif /* _h_WEBSDA */

/******************************************************************************/

/*
    @copy   default

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
