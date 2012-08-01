/*
    goahead.c -- Main program for GoAhead

Embedthis Appweb Usage:

appweb [options] [IPaddress][:port] [documents]

Options:
--home directory       # Change to directory to run
--log logFile:level    # Log to file file at verbosity level
--verbose              # Same as --log stderr:2
--version              # Output version information

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Code *************************************/

int main(int argc, char** argv)
{
    int     finished;

    if (websOpen() < 0) {
        error(E_L, E_LOG, T("Can't initialize Goahead. Exiting"));
        return -1;
    }
    if (websOpenServer(BIT_HTTP_PORT, BIT_DOCUMENTS) < 0) {
        error(E_L, E_LOG, T("Can't open GoAhead. Exiting"));
        return -1;
    }
    websUrlHandlerDefine(T(""), 0, 0, websSecurityHandler, WEBS_HANDLER_FIRST);
    websUrlHandlerDefine(T("/forms"), 0, 0, websFormHandler, 0);
    websUrlHandlerDefine(T("/cgi-bin"), 0, 0, websCgiHandler, 0);
    websUrlHandlerDefine(T("/"), 0, 0, websDefaultHomePageHandler, 0); 
    websUrlHandlerDefine(T(""), 0, 0, websDefaultHandler, WEBS_HANDLER_LAST); 
    websServiceEvents(&finished);
    websClose();
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
