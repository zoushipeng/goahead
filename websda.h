/* 
    websda.h -- GoAhead Digest Access Authentication public header
  
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_WEBSDA
#define _h_WEBSDA 1

/********************************* Includes ***********************************/

#include    "goahead.h"
#include    "webs.h"

/****************************** Definitions ***********************************/

extern char_t   *websCalcNonce(webs_t wp);
extern char_t   *websCalcOpaque(webs_t wp);
extern char_t   *websCalcDigest(webs_t wp);
extern char_t   *websCalcUrlDigest(webs_t wp);

#endif /* _h_WEBSDA */

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
