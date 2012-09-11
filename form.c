/*
    form.c -- Form processing (in-memory CGI) for the GoAhead Web server

    This module implements the /goform handler. It emulates CGI processing but performs this in-process and not as an
    external process. This enables a very high performance implementation with easy parsing and decoding of query
    strings and posted data.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/************************************ Locals **********************************/

static WebsHash formSymtab = -1;            /* Symbol table for form handlers */

/************************************* Code ***********************************/
/*
    Process a form request. Returns 1 always to indicate it handled the URL
 */
int websFormHandler(Webs *wp, char_t *prefix, char_t *dir, int arg)
{
    WebsKey         *sp;
    char_t          formBuf[BIT_LIMIT_FILENAME];
    char_t          *cp, *formName;
    WebsFormProc    fn;

    gassert(websValid(wp));

    /*
        Extract the form name
     */
    gstrncpy(formBuf, wp->path, TSZ(formBuf));
    if ((formName = gstrchr(&formBuf[1], '/')) == NULL) {
        websError(wp, 200, T("Missing form name"));
        return 1;
    }
    formName++;
    if ((cp = gstrchr(formName, '/')) != NULL) {
        *cp = '\0';
    }

    /*
        Lookup the C form function first and then try tcl (no javascript support yet).
     */
    sp = symLookup(formSymtab, formName);
    if (sp == NULL) {
        websError(wp, 404, T("Form %s is not defined"), formName);
    } else {
        //  MOB - should be a typedef
        fn = (WebsFormProc) sp->content.value.symbol;
        gassert(fn);
        if (fn) {
            (*fn)((void*) wp, formName, wp->query);
        }
    }
    return 1;
}


/*
    Define a form function in the "form" map space.
 */
int websFormDefine(char_t *name, void *fn)
{
    gassert(name && *name);
    gassert(fn);

    if (fn == NULL) {
        return -1;
    }
    symEnter(formSymtab, name, valueSymbol(fn), 0);
    return 0;
}


void websFormOpen()
{
    formSymtab = symOpen(WEBS_SYM_INIT);
}


void websFormClose()
{
    if (formSymtab != -1) {
        symClose(formSymtab);
        formSymtab = -1;
    }
}


/*
    Write a webs header. This is a convenience routine to write a common header for a form back to the browser.
 */
//  MOB - should be renamed websFormHeaders
void websHeader(Webs *wp)
{
    gassert(websValid(wp));

    websWriteHeaders(wp, 200, -1, 0);
    websWriteHeader(wp, T("\r\n"));
    websWrite(wp, T("<html>\n"));
}


void websFooter(Webs *wp)
{
    gassert(websValid(wp));
    websWrite(wp, T("</html>\n"));
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
