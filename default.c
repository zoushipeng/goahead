/*
    default.c -- Default URL handler. Includes support for Javascript.
  
    This module provides default URL handling and Active Server Page support.  
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static char_t   *websDefaultPage;           /* Default page name */
static char_t   *websDefaultDir;            /* Default Web page directory */

/**************************** Forward Declarations ****************************/

#define MAX_URL_DEPTH           8   /* Max directory depth of websDefaultDir */

static void websDefaultWriteEvent(Webs *wp);

/*********************************** Code *************************************/
/*
    Process a default URL request. This will validate the URL and handle "../" and will provide support for Active
    Server Pages. As the handler is the last handler to run, it always indicates that it has handled the URL by
    returning 1. 
 */
int websDefaultHandler(Webs *wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
    WebsFileInfo    info;
    char_t          *lpath, *tmp, *date;
    ssize           nchars;
    int             code;

    gassert(websValid(wp));
    gassert(url && *url);
    gassert(path);
    gassert(query);

    lpath = websGetRequestLpath(wp);

    /*
        If the file is a directory, redirect using the nominated default page
     */
    if (websPageIsDirectory(lpath)) {
        nchars = gstrlen(path);
        if (path[nchars-1] == '/' || path[nchars-1] == '\\') {
            path[--nchars] = '\0';
        }
        nchars += gstrlen(websDefaultPage) + 2;
        gfmtAlloc(&tmp, nchars, T("%s/%s"), path, websDefaultPage);
        websRedirect(wp, tmp);
        gfree(tmp);
        return 1;
    }
    /*
        Open the document. Stat for later use.
     */
    if (websPageOpen(wp, lpath, path, O_RDONLY | O_BINARY, 0666) < 0) {
        websError(wp, 404, T("Cannot open URL"));
        return 1;
    } 
    if (websPageStat(wp, lpath, path, &info) < 0) {
        websError(wp, 400, T("Cannot stat page for URL"));
        return 1;
    }
    websStats.localHits++;

    /*
        If the page has not been modified since the user last received it and it is not dynamically generated each time
        (Javascript), then optimize request by sending a 304 Use local copy response.
     */
    code = 200;
#if BIT_IF_MODIFIED
    //  MOB - should check for forms here too
    if (wp->flags & WEBS_IF_MODIFIED && !(wp->flags & WEBS_JS) && info.mtime <= wp->since) {
        code = 304;
    }
#endif
    websWriteHeaders(wp, code, (wp->flags & WEBS_JS) ? -1 : info.size, 0);
    if (!(wp->flags & WEBS_JS)) {
        if ((date = websGetDateString(&info)) != NULL) {
            websWriteHeader(wp, T("Last-modified: %s\r\n"), date);
            gfree(date);
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
        Evaluate Javascript requests
     */
    if (wp->flags & WEBS_JS) {
        if (websJsRequest(wp, lpath) < 0) {
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
static void websDefaultWriteEvent(Webs *wp)
{
    char    *buf;
    ssize   len, wrote, written, bytes;
    int     flags;

    gassert(websValid(wp));

    flags = websGetRequestFlags(wp);
    websSetTimeMark(wp);
    wrote = bytes = 0;
    written = websGetRequestWritten(wp);

    /*
        We only do this for non-Javascript documents
     */
    if (!(flags & WEBS_JS)) {
        bytes = websGetRequestBytes(wp);
        /*
            Note: websWriteDataNonBlock may return less than we wanted. It will return -1 on a socket error
         */
        if ((buf = galloc(BIT_LIMIT_BUFFER)) == NULL) {
            websError(wp, 200, T("Can't get memory"));
        } else {
            while ((len = websPageReadData(wp, buf, BIT_LIMIT_BUFFER)) > 0) {
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
                gassert(written >= bytes);
                written = bytes;
            }
            gfree(buf);
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
    websDefaultPage = gstrdup(T("index.html"));
}


/* 
    Closing down. Free resources.
 */
void websDefaultClose()
{
    gfree(websDefaultPage);
    websDefaultPage = NULL;
    gfree(websDefaultDir);
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
    gassert(page && *page);

    if (websDefaultPage) {
        gfree(websDefaultPage);
    }
    websDefaultPage = gstrdup(page);
}


/*
    Set the default web directory
 */
void websSetDefaultDir(char_t *dir)
{
    gassert(dir && *dir);
    if (websDefaultDir) {
        gfree(websDefaultDir);
    }
    websDefaultDir = gstrdup(dir);
}


int websDefaultHomePageHandler(Webs *wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, 
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
