/*
    handler.c -- URL handler support

    This module implements a URL handler interface and API to permit the addition of user definable URL processors.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static WebsHandler  *websUrlHandler;            /* URL handler list */
static int          websUrlHandlerMax;          /* Number of entries */
static int          urlHandlerOpenCount = 0;    /* count of apps */

/**************************** Forward Declarations ****************************/

static int      websUrlHandlerSort(const void *p1, const void *p2);
static int      websPublishHandler(Webs *wp, char_t *urlPrefix, char_t *webDir, 
                    int sid, char_t *url, char_t *path, char_t *query);

/*********************************** Code *************************************/

int websUrlHandlerOpen()
{
    if (++urlHandlerOpenCount == 1) {
        websUrlHandler = NULL;
        websUrlHandlerMax = 0;
        websAspOpen();
    }
    return 0;
}


void websUrlHandlerClose()
{
    WebsHandler     *sp;

    if (--urlHandlerOpenCount <= 0) {
        websAspClose();
        for (sp = websUrlHandler; sp < &websUrlHandler[websUrlHandlerMax];
            sp++) {
            gfree(sp->urlPrefix);
            if (sp->webDir) {
                gfree(sp->webDir);
            }
        }
        gfree(websUrlHandler);
        websUrlHandlerMax = 0;
    }
}


/*
    Define a new URL handler. urlPrefix is the URL prefix to match. webDir is an optional root directory path for a web
    directory. arg is an optional arg to pass to the URL handler. flags defines the matching order. Valid flags include
    WEBS_HANDLER_LAST, WEBS_HANDLER_FIRST. If multiple users specify last or first, their order is defined
    alphabetically by the urlPrefix.
 */
int websUrlHandlerDefine(char_t *urlPrefix, char_t *webDir, int arg, 
        int (*handler)(Webs *wp, char_t *urlPrefix, char_t *webdir, int arg, 
        char_t *url, char_t *path, char_t *query), int flags)
{
    WebsHandler *sp;
    int         len;

    gassert(urlPrefix);
    gassert(handler);

    /*
        Grow the URL handler array to create a new slot
     */
    len = (websUrlHandlerMax + 1) * sizeof(WebsHandler);
    if ((websUrlHandler = grealloc(websUrlHandler, len)) == NULL) {
        return -1;
    }
    sp = &websUrlHandler[websUrlHandlerMax++];
    memset(sp, 0, sizeof(WebsHandler));

    sp->urlPrefix = gstrdup(urlPrefix);
    sp->len = gstrlen(sp->urlPrefix);
    if (webDir) {
        sp->webDir = gstrdup(webDir);
    } else {
        sp->webDir = gstrdup(T(""));
    }
    sp->handler = handler;
    sp->arg = arg;
    sp->flags = flags;
    /*
        Sort in decreasing URL length order observing the flags for first and last
     */
    qsort(websUrlHandler, websUrlHandlerMax, sizeof(WebsHandler), websUrlHandlerSort);
    return 0;
}


/*
    Delete an existing URL handler. We don't reclaim the space of the old handler, just NULL the entry. Return -1 if
    handler is not found.  
 */
int websUrlHandlerDelete(int (*handler)(Webs *wp, char_t *urlPrefix, 
    char_t *webDir, int arg, char_t *url, char_t *path, char_t *query))
{
    WebsHandler *sp;
    int         i;

    for (i = 0; i < websUrlHandlerMax; i++) {
        sp = &websUrlHandler[i];
        if (sp->handler == handler) {
            sp->handler = NULL;
            return 0;
        }
    }
    return -1;
}


/*
    Sort in decreasing URL length order observing the flags for first and last
 */
static int websUrlHandlerSort(const void *p1, const void *p2)
{
    WebsHandler     *s1, *s2;
    int             rc;

    gassert(p1);
    gassert(p2);

    s1 = (WebsHandler*) p1;
    s2 = (WebsHandler*) p2;

    if ((s1->flags & WEBS_HANDLER_FIRST) || (s2->flags & WEBS_HANDLER_LAST)) {
        return -1;
    }
    if ((s2->flags & WEBS_HANDLER_FIRST) || (s1->flags & WEBS_HANDLER_LAST)) {
        return 1;
    }
    if ((rc = gstrcmp(s1->urlPrefix, s2->urlPrefix)) == 0) {
        if (s1->len < s2->len) {
            return 1;
        } else if (s1->len > s2->len) {
            return -1;
        }
    }
    return -rc; 
}


/*
    Publish a new web directory (Use the default URL handler)
 */
int websPublish(char_t *urlPrefix, char_t *path)
{
    return websUrlHandlerDefine(urlPrefix, path, 0, websPublishHandler, 0);
}


/*
    Return the directory for a given prefix. Ignore empty prefixes
 */
char_t *websGetPublishDir(char_t *path, char_t **urlPrefix)
{
    WebsHandler  *sp;
    int          i;

    for (i = 0; i < websUrlHandlerMax; i++) {
        sp = &websUrlHandler[i];
        if (sp->urlPrefix[0] == '\0') {
            continue;
        }
        if (sp->handler && gstrncmp(sp->urlPrefix, path, sp->len) == 0) {
            if (urlPrefix) {
                *urlPrefix = sp->urlPrefix;
            }
            return sp->webDir;
        }
    }
    return NULL;
}


/*
    Publish URL handler. We just patch the web page Directory and let the default handler do the rest.
 */
static int websPublishHandler(Webs *wp, char_t *urlPrefix, char_t *webDir, int sid, char_t *url, char_t *path, 
        char_t *query)
{
    ssize   len;

    gassert(websValid(wp));
    gassert(path);

    /*
        Trim the urlPrefix off the path and set the webdirectory. Add one to step over the trailing '/'
     */
    len = gstrlen(urlPrefix) + 1;
    websSetRequestPath(wp, webDir, &path[len]);
    return 0;
}


/*
    See if any valid handlers are defined for this request. If so, call them and continue calling valid handlers until
    one accepts the request.  Return true if a handler was invoked, else return FALSE.
 */
int websUrlHandlerRequest(Webs *wp)
{
    WebsHandler   *sp;
    int           i, first;

    gassert(websValid(wp));

    /*
        Delete the socket handler as we don't want to start reading any data on the connection as it may be for the next
        pipelined HTTP/1.1 request if using Keep Alive.
     */
    socketDeleteHandler(wp->sid);
    wp->state = WEBS_PROCESSING;
    websStats.handlerHits++;

    if ((wp->path[0] != '/') || strchr(wp->path, '\\')) {
        websError(wp, 400, T("Bad request"));
        return 0;
    }
#if BIT_AUTH
    if (!websVerifyRoute(wp)) {
        gassert(wp->code);
        websStats.access++;
        return 1;
    }
#endif
    /*
        We loop over each handler in order till one accepts the request.  The security handler will handle the request
        if access is NOT allowed.  
     */
    first = 1;
    for (i = 0; i < websUrlHandlerMax; i++) {
        sp = &websUrlHandler[i];
        if (sp->handler && gstrncmp(sp->urlPrefix, wp->path, sp->len) == 0) {
            if (first) {
                websSetEnv(wp);
                first = 0;
            }
            if ((*sp->handler)(wp, sp->urlPrefix, sp->webDir, sp->arg, wp->url, wp->path, wp->query)) {
                return 1;
            }
            if (!websValid(wp)) {
                trace(0, T("handler %s called websDone, but didn't return 1\n"), sp->urlPrefix);
                return 1;
            }
        }
    }
    /*
        If no handler processed the request, then return an error. Note: It is the handlers responsibility to call websDone
     */
    if (i >= websUrlHandlerMax) {
        websError(wp, 200, T("No handler for this URL"));
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
