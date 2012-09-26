/*
    options.c -- Options and Trace handler

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Code *************************************/

static bool optionsHandler(Webs *wp)
{
    gassert(wp);

    if (smatch(wp->method, "OPTIONS")) {
        websWriteHeaders(wp, HTTP_CODE_OK, 0, 0);
        websWriteHeader(wp, "Allow: DELETE,GET,HEAD,OPTIONS,POST,PUT%s\r\n", BIT_TRACE_METHOD ? ",TRACE" : "");
        websWriteEndHeaders(wp);
        websDone(wp, HTTP_CODE_OK);
        return 1;

#if BIT_TRACE_METHOD
    } else if (smatch(wp->method, "TRACE")) {
        websWriteHeaders(wp, HTTP_CODE_OK, -1, 0);
        websWriteEndHeaders(wp);
        websWrite(wp, "%s %s %s\r\n", wp->method, wp->url, wp->protoVersion);
        websDone(wp, HTTP_CODE_OK);
        
        return 1;
#endif
    }
    websResponse(wp, HTTP_CODE_NOT_ACCEPTABLE, "Unsupported method");
    return 1;
}


int websOptionsOpen()
{
    websDefineHandler("options", optionsHandler, 0, 0);
    return 0;
}


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
