/*
    file.c -- File handler
  
    This module serves static file documents
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static char   *websIndex;                 /* Default page name */
static char   *websDocuments;            /* Default Web page directory */

/**************************** Forward Declarations ****************************/

static void writeEvent(Webs *wp);

/*********************************** Code *************************************/
/*
    Serve static files
 */
static bool fileHandler(Webs *wp)
{
    WebsFileInfo    info;
    char          *tmp, *date;
    ssize           nchars;
    int             code;

    gassert(websValid(wp));

    //  MOB - need some route protection for this. What about abilities "delete"

#if !BIT_ROM
    if (smatch(wp->method, "DELETE")) {
        if (unlink(wp->filename) < 0) {
            websError(wp, 404, "Can't delete the URI");
        } else {
            /* No content */
            websResponse(wp, 204, 0);
        }
    } else if (smatch(wp->method, "PUT")) {
        /* Code is already set for us by processContent() */
        websResponse(wp, wp->code, 0);

    } else 
#endif /* !BIT_ROM */
    {
        /*
            If the file is a directory, redirect using the nominated default page
         */
        if (websPageIsDirectory(wp->filename)) {
            nchars = strlen(wp->path);
            if (wp->path[nchars - 1] == '/' || wp->path[nchars - 1] == '\\') {
                wp->path[--nchars] = '\0';
            }
            tmp = sfmt("%s/%s", wp->path, websIndex);
            websRedirect(wp, tmp);
            gfree(tmp);
            return 1;
        }
        if (websPageOpen(wp, wp->filename, wp->path, O_RDONLY | O_BINARY, 0666) < 0) {
#if BIT_DEBUG
            /* Yes Veronica, the HTTP spec does misspell Referrer */
            char    *ref;
            if ((ref = websGetVar(wp, "HTTP_REFERER", 0)) != 0) {
                trace(1, "From %s\n", ref);
            }
#endif
            websError(wp, 404, "Cannot open: %s", wp->filename);
            return 1;
        }
        //  MOB - confusion with filename and path
        //  MOB - should we be using just lower stat?
        if (websPageStat(wp, wp->filename, wp->path, &info) < 0) {
            websError(wp, 400, "Cannot stat page for URL");
            return 1;
        }
        code = 200;
        if (info.mtime <= wp->since) {
            code = 304;
        }
        websWriteHeaders(wp, code, info.size, 0);
        if ((date = websGetDateString(&info)) != NULL) {
            websWriteHeader(wp, "Last-modified: %s\r\n", date);
            gfree(date);
        }
        websWriteEndHeaders(wp);

        /*
            All done if the browser did a HEAD request
         */
        if (smatch(wp->method, "HEAD")) {
            websDone(wp, 200);
            return 1;
        }
        writeEvent(wp);
    }
    return 1;
}


int websProcessPutData(Webs *wp)
{
    ssize   nbytes;

    gassert(wp->putfd >= 0);
    nbytes = ringqLen(&wp->input);
    if (write(wp->putfd, wp->input.servp, (int) nbytes) != nbytes) {
        websError(wp, WEBS_CLOSE | 500, "Can't write to file");
        return -1;
    }
    websConsumeInput(wp, nbytes);
    return 0;
}


/*
    Do output back to the browser in the background. This is a socket write handler.
 */
static void writeEvent(Webs *wp)
{
    WebsSocket    *sp;
    char        *buf;
    ssize       len, wrote;

    gassert(websValid(wp));
    websSetTimeMark(wp);

    /*
        Note: websWriteRaw may return less than we wanted. It will return -1 on a socket error.
     */
    if ((buf = galloc(BIT_LIMIT_BUFFER)) == NULL) {
        websError(wp, 200, "Can't get memory");
        return;
    }
    wrote = 0;
    while ((len = websPageReadData(wp, buf, BIT_LIMIT_BUFFER)) > 0) {
        if ((wrote = websWriteRaw(wp, buf, len)) < 0) {
            break;
        }
        wp->written += wrote;
        if (wrote != len) {
            websPageSeek(wp, - (len - wrote));
            break;
        }
    }
    gfree(buf);
    if (wrote <= 0 || wp->written >= wp->numbytes) {
        websDone(wp, 200);
    } else {
        //  MOB - wrap in websRegisterInterest(wp, SOCKET_WRITABLE, writeEvent);
        sp = socketPtr(wp->sid);
        socketRegisterInterest(sp, sp->handlerMask | SOCKET_WRITABLE);
        wp->writable = writeEvent;
    }
}


static void fileClose()
{
    gfree(websIndex);
    websIndex = NULL;
    gfree(websDocuments);
    websDocuments = NULL;
}


void websFileOpen()
{
    websIndex = strdup("index.html");
    websDefineHandler("file", fileHandler, fileClose, 0);
}


/*
    Get the default page for URL requests ending in "/"
 */
char *websGetIndex()
{
    return websIndex;
}


char *websGetDocuments()
{
    return websDocuments;
}


/*
    Set the default page for URL requests ending in "/"
 */
void websSetIndex(char *page)
{
    gassert(page && *page);

    if (websIndex) {
        gfree(websIndex);
    }
    websIndex = strdup(page);
}


/*
    Set the default web directory
 */
void websSetDocuments(char *dir)
{
    gassert(dir && *dir);
    if (websDocuments) {
        gfree(websDocuments);
    }
    websDocuments = strdup(dir);
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
