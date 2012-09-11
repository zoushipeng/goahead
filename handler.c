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

/**************************** Forward Declarations ****************************/

static int      websUrlHandlerSort(const void *p1, const void *p2);
static int      websPublishHandler(Webs *wp, char_t *prefix, char_t *dir, int sid);

/*********************************** Code *************************************/

int websUrlHandlerOpen()
{
    websUrlHandler = NULL;
    websUrlHandlerMax = 0;
    websJsOpen();
    return 0;
}


void websUrlHandlerClose()
{
    WebsHandler     *sp;

    for (sp = websUrlHandler; sp < &websUrlHandler[websUrlHandlerMax];
        sp++) {
        gfree(sp->prefix);
        if (sp->dir) {
            gfree(sp->dir);
        }
    }
    gfree(websUrlHandler);
    websUrlHandlerMax = 0;
}


/*
    Define a new URL handler. prefix is the URL prefix to match. dir is an optional root directory path for a web
    directory. arg is an optional arg to pass to the URL handler. flags defines the matching order. Valid flags include
    WEBS_HANDLER_LAST, WEBS_HANDLER_FIRST. If multiple users specify last or first, their order is defined
    alphabetically by the prefix.
 */
int websUrlHandlerDefine(char_t *prefix, char_t *dir, int arg, WebsHandlerProc handler, int flags)
{
    WebsHandler *sp;
    int         len;

    gassert(prefix);
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

    sp->prefix = gstrdup(prefix);
    sp->len = gstrlen(sp->prefix);
    if (dir) {
        sp->dir = gstrdup(dir);
    } else {
        sp->dir = gstrdup(T(""));
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
int websUrlHandlerDelete(WebsHandlerProc handler)
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
    if ((rc = gstrcmp(s1->prefix, s2->prefix)) == 0) {
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
int websPublish(char_t *prefix, char_t *dir)
{
    return websUrlHandlerDefine(prefix, dir, 0, websPublishHandler, 0);
}


/*
    Return the directory for a given prefix. Ignore empty prefixes
 */
char_t *websGetPublishDir(char_t *path, char_t **prefix)
{
    WebsHandler  *sp;
    int          i;

    for (i = 0; i < websUrlHandlerMax; i++) {
        sp = &websUrlHandler[i];
        if (sp->prefix[0] == '\0') {
            continue;
        }
        if (sp->handler && gstrncmp(sp->prefix, path, sp->len) == 0) {
            if (prefix) {
                *prefix = sp->prefix;
            }
            return sp->dir;
        }
    }
    return NULL;
}


/*
    Publish URL handler. We just patch the web page Directory and let the default handler do the rest.
 */
static int websPublishHandler(Webs *wp, char_t *prefix, char_t *dir, int sid)
{
    ssize   len;

    gassert(websValid(wp));

    /*
        Trim the prefix off the path and set the webdirectory. Add one to step over the trailing '/'
     */
    len = gstrlen(prefix) + 1;
    websSetRequestPath(wp, dir, &wp->path[len]);
    return 0;
}


/*
    See if any valid handlers are defined for this request. If so, call them and continue calling valid handlers until
    one accepts the request.
 */
void websHandleRequest(Webs *wp)
{
    WebsHandler   *sp;
    int           i;

    gassert(websValid(wp));

    /*
        Delete the socket handler as we don't want to start reading any data on the connection as it may be for the next
        pipelined HTTP/1.1 request if using Keep Alive.
     */
    socketDeleteHandler(wp->sid);

    if ((wp->path[0] != '/') || strchr(wp->path, '\\')) {
        websError(wp, 400, T("Bad request"));
        return;
    }
#if BIT_AUTH
    if (!websVerifyRoute(wp)) {
        gassert(wp->code);
        return;
    }
#endif
    websSetEnv(wp);

    /*
        We loop over each handler in order till one accepts the request.  The security handler will handle the request
        if access is NOT allowed.  
     */
    for (i = 0; i < websUrlHandlerMax; i++) {
        sp = &websUrlHandler[i];
        if (sp->handler && gstrncmp(sp->prefix, wp->path, sp->len) == 0) {
            if ((*sp->handler)(wp, sp->prefix, sp->dir, sp->arg)) {
                return;
            }
            if (!websValid(wp)) {
                trace(0, T("handler %s called websDone, but didn't return 1\n"), sp->prefix);
                return;
            }
        }
    }
    /*
        If no handler processed the request, then return an error. Note: It is the handlers responsibility to call websDone
     */
    if (i >= websUrlHandlerMax) {
        websError(wp, 200, T("No handler for this URL"));
    }
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
