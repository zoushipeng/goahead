/*
    default.c -- Default URL handler. Includes support for ASP.
  
    This module provides default URL handling and Active Server Page support.  
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static char_t   *websDefaultPage;           /* Default page name */
static char_t   *websDefaultDir;            /* Default Web page directory */

/**************************** Forward Declarations ****************************/

#define MAX_URL_DEPTH           8   /* Max directory depth of websDefaultDir */

static void websDefaultWriteEvent(webs_t wp);

/*********************************** Code *************************************/
/*
    Process a default URL request. This will validate the URL and handle "../" and will provide support for Active
    Server Pages. As the handler is the last handler to run, it always indicates that it has handled the URL by
    returning 1. 
 */
int websDefaultHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
    websStatType    sbuf;
    char_t          *lpath, *tmp, *date;
    ssize           nchars;
    int             code;

    a_assert(websValid(wp));
    a_assert(url && *url);
    a_assert(path);
    a_assert(query);

    lpath = websGetRequestLpath(wp);
#if UNUSED
    nchars = gstrlen(lpath) - 1;
    if (lpath[nchars] == '/' || lpath[nchars] == '\\') {
        lpath[nchars] = '\0';
    }
#endif

    /*
        If the file is a directory, redirect using the nominated default page
     */
    if (websPageIsDirectory(lpath)) {
        nchars = gstrlen(path);
        if (path[nchars-1] == '/' || path[nchars-1] == '\\') {
            path[--nchars] = '\0';
        }
        nchars += gstrlen(websDefaultPage) + 2;
        fmtAlloc(&tmp, nchars, T("%s/%s"), path, websDefaultPage);
        websRedirect(wp, tmp);
        bfree(tmp);
        return 1;
    }
    /*
        Open the document. Stat for later use.
     */
    if (websPageOpen(wp, lpath, path, O_RDONLY | O_BINARY, 0666) < 0) {
        websError(wp, 404, T("Cannot open URL"));
        return 1;
    } 
    if (websPageStat(wp, lpath, path, &sbuf) < 0) {
        websError(wp, 400, T("Cannot stat page for URL"));
        return 1;
    }

    /*
        If the page has not been modified since the user last received it and it is not dynamically generated each time
        (ASP), then optimize request by sending a 304 Use local copy response.
     */
    websStats.localHits++;
    code = 200;
#if BIT_IF_MODIFIED
    if (wp->flags & WEBS_IF_MODIFIED && !(wp->flags & WEBS_ASP) && sbuf.mtime <= wp->since) {
        code = 304;
    }
#endif
    websWriteHeaders(wp, code, (wp->flags & WEBS_ASP) ? -1 : sbuf.size, 0);
    if (!(wp->flags & WEBS_ASP)) {
        if ((date = websGetDateString(&sbuf)) != NULL) {
            websWriteHeader(wp, T("Last-modified: %s\r\n"), date);
            bfree(date);
        }
    }
    websWriteHeader(wp, T("\r\n"));

    /*
        All done if the browser did a HEAD request
     */
    if (wp->flags & WEBS_HEAD_REQUEST) {
        websDone(wp, 200);
        return 1;
    }
#if BIT_JAVASCRIPT
    /*
        Evaluate ASP requests
     */
    if (wp->flags & WEBS_ASP) {
        if (websAspRequest(wp, lpath) < 0) {
            return 1;
        }
        websDone(wp, 200);
        return 1;
    }
#endif
    /*
        Return the data via background write
     */
    websSetRequestSocketHandler(wp, SOCKET_WRITABLE, websDefaultWriteEvent);
    return 1;
}


/*
    Do output back to the browser in the background. This is a socket write handler.
 */
static void websDefaultWriteEvent(webs_t wp)
{
    char    *buf;
    ssize   len, wrote, written, bytes;
    int     flags;

    a_assert(websValid(wp));

    flags = websGetRequestFlags(wp);
    websSetTimeMark(wp);
    wrote = bytes = 0;
    written = websGetRequestWritten(wp);

    /*
        We only do this for non-ASP documents
     */
    if (!(flags & WEBS_ASP)) {
        bytes = websGetRequestBytes(wp);
        /*
            Note: websWriteDataNonBlock may return less than we wanted. It will return -1 on a socket error
         */
        if ((buf = balloc(PAGE_READ_BUFSIZE)) == NULL) {
            websError(wp, 200, T("Can't get memory"));
        } else {
            while ((len = websPageReadData(wp, buf, PAGE_READ_BUFSIZE)) > 0) {
                if ((wrote = websWriteDataNonBlock(wp, buf, len)) < 0) {
                    break;
                }
                written += wrote;
                if (wrote != len) {
                    websPageSeek(wp, - (len - wrote));
                    break;
                }
            }
            /*
                Safety: If we are at EOF, we must be done
             */
            if (len == 0) {
                a_assert(written >= bytes);
                written = bytes;
            }
            bfree(buf);
        }
    }
    /*
        We're done if an error, or all bytes output
     */
    websSetRequestWritten(wp, written);
    if (wrote < 0 || written >= bytes) {
        websDone(wp, 200);
    }
}


/* 
    Initialize variables and data for the default URL handler module
 */

void websDefaultOpen()
{
    websDefaultPage = bstrdup(T("index.html"));
}


/* 
    Closing down. Free resources.
 */
void websDefaultClose()
{
    bfree(websDefaultPage);
    websDefaultPage = NULL;
    bfree(websDefaultDir);
    websDefaultDir = NULL;
}


/*
    Get the default page for URL requests ending in "/"
 */
char_t *websGetDefaultPage()
{
    return websDefaultPage;
}


char_t *websGetDefaultDir()
{
    return websDefaultDir;
}


/*
    Set the default page for URL requests ending in "/"
 */
void websSetDefaultPage(char_t *page)
{
    a_assert(page && *page);

    if (websDefaultPage) {
        bfree(websDefaultPage);
    }
    websDefaultPage = bstrdup(page);
}


/*
    Set the default web directory
 */
void websSetDefaultDir(char_t *dir)
{
    a_assert(dir && *dir);
    if (websDefaultDir) {
        bfree(websDefaultDir);
    }
    websDefaultDir = bstrdup(dir);
}


int websDefaultHomePageHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, 
        char_t *path, char_t *query)
{
    //  MOB gmatch
    //  MOB - could groutines apply T() to args)
    if (*url == '\0' || gstrcmp(url, T("/")) == 0) {
        websRedirect(wp, "index.html");
        return 1;
    }
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
