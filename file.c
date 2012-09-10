/*
    file.c -- File handler
  
    This module serves static file documents
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static char_t   *websIndex;                 /* Default page name */
static char_t   *websDocuments;            /* Default Web page directory */

/**************************** Forward Declarations ****************************/

static void writeEvent(Webs *wp);

/*********************************** Code *************************************/
/*
    Process a URL request for static file documents. 
 */
int websFileHandler(Webs *wp, char_t *prefix, char_t *dir, int arg)
{
    WebsFileInfo    info;
    char_t          *tmp, *date;
    ssize           nchars;
    int             code;

    gassert(websValid(wp));

    //  MOB - need some route protection for this. What about abilities "delete"

#if !BIT_ROM
    if (wp->flags & WEBS_DELETE_REQUEST) {
        if (unlink(wp->filename) < 0) {
            websError(wp, 404, "Can't delete the URI");
        } else {
            /* No content */
            websResponse(wp, 204, 0, 0);
        }
    } else if (wp->flags & WEBS_PUT_REQUEST) {
        /* Code is already set for us by processContent() */
        websResponse(wp, wp->code, 0, 0);

    } else 
#endif /* !BIT_ROM */
    {
        /*
            If the file is a directory, redirect using the nominated default page
         */
        if (websPageIsDirectory(wp->filename)) {
            nchars = gstrlen(wp->path);
            if (wp->path[nchars - 1] == '/' || wp->path[nchars - 1] == '\\') {
                wp->path[--nchars] = '\0';
            }
            nchars += gstrlen(websIndex) + 2;
            gfmtAlloc(&tmp, nchars, T("%s/%s"), wp->path, websIndex);
            websRedirect(wp, tmp);
            gfree(tmp);
            return 1;
        }
        /*
            Open the document. Stat for later use.
         */
        if (websPageOpen(wp, wp->filename, wp->path, O_RDONLY | O_BINARY, 0666) < 0) {
            websError(wp, 404, T("Cannot open URL"));
            return 1;
        } 
        //  MOB - confusion with filename and path
        //  MOB - should we be using just lower stat?
        if (websPageStat(wp, wp->filename, wp->path, &info) < 0) {
            websError(wp, 400, T("Cannot stat page for URL"));
            return 1;
        }
#if UNUSED
        websStats.localHits++;
#endif

        code = 200;
#if BIT_IF_MODIFIED
        //  MOB - should check for forms here too
        if (wp->flags & WEBS_IF_MODIFIED && !(wp->flags & WEBS_JS) && info.mtime <= wp->since) {
            code = 304;
        }
#endif
        /* WARNING: windows needs cast of -1 */
        websWriteHeaders(wp, code, info.size, 0);
        if ((date = websGetDateString(&info)) != NULL) {
            websWriteHeader(wp, T("Last-modified: %s\r\n"), date);
            gfree(date);
        }
        websWriteHeader(wp, T("\r\n"));

        /*
            All done if the browser did a HEAD request
         */
        if (wp->flags & WEBS_HEAD_REQUEST) {
            websDone(wp, 200);
            return 1;
        }
        writeEvent(wp);
    }
    return 1;
}


/*
    Do output back to the browser in the background. This is a socket write handler.
 */
static void writeEvent(Webs *wp)
{
    socket_t    *sp;
    char        *buf;
    ssize       len, wrote, written, bytes;
    int         flags;

    gassert(websValid(wp));

    flags = websGetRequestFlags(wp);
    websSetTimeMark(wp);
    wrote = bytes = 0;
    written = websGetRequestWritten(wp);

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
    /*
        We're done if an error, or all bytes output
     */
    websSetRequestWritten(wp, written);
    if (wrote < 0 || written >= bytes) {
        websDone(wp, 200);
    } else {
        //  MOB - wrap in websRegisterInterest(wp, SOCKET_WRITABLE, writeEvent);
        sp = socketPtr(wp->sid);
        socketRegisterInterest(sp, sp->handlerMask | SOCKET_WRITABLE);
        wp->writable = writeEvent;
    }
}


/* 
    Initialize variables and data for the default URL handler module
 */

void websFileOpen()
{
    //  MOB - should not be unicode
    websIndex = gstrdup(T("index.html"));
}


void websFileClose()
{
    gfree(websIndex);
    websIndex = NULL;
    gfree(websDocuments);
    websDocuments = NULL;
}


/*
    Get the default page for URL requests ending in "/"
 */
char_t *websGetIndex()
{
    return websIndex;
}


char_t *websGetDocuments()
{
    return websDocuments;
}


/*
    Set the default page for URL requests ending in "/"
 */
void websSetIndex(char_t *page)
{
    gassert(page && *page);

    if (websIndex) {
        gfree(websIndex);
    }
    websIndex = gstrdup(page);
}


/*
    Set the default web directory
 */
void websSetDocuments(char_t *dir)
{
    gassert(dir && *dir);
    if (websDocuments) {
        gfree(websDocuments);
    }
    websDocuments = gstrdup(dir);
}


int websHomePageHandler(Webs *wp, char_t *prefix, char_t *dir, int arg)
{
    //  MOB gmatch
    //  MOB - could groutines apply T() to args)
    if (*wp->url == '\0' || gstrcmp(wp->url, T("/")) == 0) {
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
