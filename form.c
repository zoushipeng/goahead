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

static sym_fd_t formSymtab = -1;            /* Symbol table for form handlers */

/************************************* Code ***********************************/
/*
    Process a form request. Returns 1 always to indicate it handled the URL
 */
int websFormHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
    sym_t       *sp;
    char_t      formBuf[BIT_LIMIT_FILENAME];
    char_t      *cp, *formName;
    int         (*fn)(void *sock, char_t *path, char_t *args);

    gassert(websValid(wp));
    gassert(url && *url);
    gassert(path && *path == '/');

    websStats.formHits++;

    /*
        Extract the form name
     */
    gstrncpy(formBuf, path, TSZ(formBuf));
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
        fn = (int (*)(void *, char_t *, char_t *)) sp->content.value.symbol;
        gassert(fn);
        if (fn) {
            (*fn)((void*) wp, formName, query);
        }
    }
    return 1;
}


/*
    Define a form function in the "form" map space.
 */
int websFormDefine(char_t *name, void (*fn)(webs_t wp, char_t *path, char_t *query))
{
    gassert(name && *name);
    gassert(fn);

    if (fn == NULL) {
        return -1;
    }
    symEnter(formSymtab, name, valueSymbol((void*) fn), (int) NULL);
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
void websHeader(webs_t wp)
{
    gassert(websValid(wp));

    websWriteHeaders(wp, 200, -1, 0);
    websWriteHeader(wp, T("\r\n"));
    websWrite(wp, T("<html>\n"));
}


void websFooter(webs_t wp)
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
