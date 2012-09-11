/*
    template.c -- JavaScript web page templates
  
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"
#include    "js.h"

#if BIT_JAVASCRIPT
/********************************** Locals ************************************/

static WebsHash websJsFunctions = -1;  /* Symbol table of functions */
static int      aspOpenCount = 0;       /* count of apps using this module */

/***************************** Forward Declarations ***************************/

static char_t   *strtokcmp(char_t *s1, char_t *s2);
static char_t   *skipWhite(char_t *s);

/************************************* Code ***********************************/
/*
    Process template JS request
 */
int websJsHandler(Webs *wp, char_t *prefix, char_t *dir, int arg)
{
    char_t          *filename;

    gassert(websValid(wp));

    if (gstrcmp(wp->ext, T(".asp")) != 0) {
        return 0;
    }
    /*
        Evaluate Javascript requests
     */
    if (wp->flags & WEBS_JS) {
        filename = websGetRequestFilename(wp);
        if (websJsRequest(wp, filename) < 0) {
            return 1;
        }
        websDone(wp, 200);
        return 1;
    }
    return 1;
}


/*
    Create script spaces and commands
 */
int websJsOpen()
{
    if (++aspOpenCount == 1) {
        /*
            Create the table for functions
         */
        websJsFunctions = symOpen(WEBS_SYM_INIT * 2);

        /*
            Create standard commands
         */
        websJsDefine(T("write"), websJsWrite);
    }
    return 0;
}

void websJsClose()
{
    if (--aspOpenCount <= 0) {
        if (websJsFunctions != -1) {
            symClose(websJsFunctions);
            websJsFunctions = -1;
        }
    }
}


int websEval(char_t *cmd, char_t **result, void* chan)
{
    int     jsid;

    jsid = PTOI(chan);
    if (jsEval(jsid, cmd, result)) {
        return 0;
    } else {
        return -1;
    }
    return -1;
}


/*
    Process requests and expand all scripting commands. We read the entire web page into memory and then process. If
    you have really big documents, it is better to make them plain HTML files rather than Javascript web pages.
 */
int websJsRequest(Webs *wp, char_t *filename)
{
    WebsFileInfo    sbuf;
    char            *rbuf;
    char_t          *token, *lang, *result, *path, *ep, *cp, *buf, *nextp;
    char_t          *last;
    ssize           len;
    int             rc, jsid;

    gassert(websValid(wp));
    gassert(filename && *filename);

    rc = -1;
    buf = NULL;
    rbuf = NULL;
    wp->flags |= WEBS_HEADER_DONE;
    wp->flags &= ~WEBS_KEEP_ALIVE;
    path = websGetRequestPath(wp);

    /*
        Create Javascript instance in case it is needed
     */
    if ((jsid = jsOpenEngine(wp->vars, websJsFunctions)) < 0) {
        websError(wp, 200, T("Can't create JavaScript engine"));
        goto done;
    }
    jsSetUserHandle(jsid, wp);

    if (websPageStat(wp, filename, path, &sbuf) < 0) {
        websError(wp, 404, T("Can't stat %s"), filename);
        goto done;
    }

    /*
        Create a buffer to hold the web page in-memory
     */
    len = sbuf.size * sizeof(char);
    if ((rbuf = galloc(len + 1)) == NULL) {
        websError(wp, 200, T("Can't get memory"));
        goto done;
    }
    rbuf[len] = '\0';

    if (websPageReadData(wp, rbuf, len) != len) {
        websError(wp, 200, T("Cant read %s"), filename);
        goto done;
    }
    websPageClose(wp);

    /*
        Convert to UNICODE if necessary.
     */
    if ((buf = gallocAscToUni(rbuf, len)) == NULL) {
        websError(wp, 200, T("Can't get memory"));
        goto done;
    }

    websWriteHeaders(wp, 200, (ssize) -1, 0);
    websWriteHeader(wp, T("Pragma: no-cache\r\nCache-Control: no-cache\r\n"));
    websWriteHeader(wp, T("\r\n"));

    /*
        Scan for the next "<%"
     */
    last = buf;
    rc = 0;
    while (rc == 0 && *last && ((nextp = gstrstr(last, T("<%"))) != NULL)) {
        websWriteBlock(wp, last, (nextp - last));
        nextp = skipWhite(nextp + 2);

        /*
            Decode the language
         */
        token = T("language");

        if ((lang = strtokcmp(nextp, token)) != NULL) {
            if ((cp = strtokcmp(lang, T("=javascript"))) != NULL) {
                /* Ignore */;
            } else {
                cp = nextp;
            }
            nextp = cp;
        }

        /*
            Find tailing bracket and then evaluate the script
         */
        if ((ep = gstrstr(nextp, T("%>"))) != NULL) {

            *ep = '\0';
            last = ep + 2;
            nextp = skipWhite(nextp);
            /*
                Handle backquoted newlines
             */
            for (cp = nextp; *cp; ) {
                if (*cp == '\\' && (cp[1] == '\r' || cp[1] == '\n')) {
                    *cp++ = ' ';
                    while (*cp == '\r' || *cp == '\n') {
                        *cp++ = ' ';
                    }
                } else {
                    cp++;
                }
            }
            if (*nextp) {
                result = NULL;
                if ((rc = websEval(nextp, &result, ITOP(jsid))) < 0) {
                    /*
                         On an error, discard all output accumulated so far and store the error in the result buffer. Be
                         careful if the user has called websError() already.
                     */
                    if (websValid(wp)) {
                        if (result) {
                            websWrite(wp, T("<h2><b>Javascript Error: %s</b></h2>\n"), result);
                            websWrite(wp, T("<pre>%s</pre>"), nextp);
                            gfree(result);
                        } else {
                            websWrite(wp, T("<h2><b>Javascript Error</b></h2>\n%s\n"), nextp);
                        }
                        websWrite(wp, T("</body></html>\n"));
                        rc = 0;
                    }
                    goto done;
                }
            }

        } else {
            websError(wp, 200, T("Unterminated script in %s: \n"), filename);
            rc = -1;
            goto done;
        }
    }
    /*
        Output any trailing HTML page text
     */
    if (last && *last && rc == 0) {
        websWriteBlock(wp, last, gstrlen(last));
    }
    rc = 0;

/*
    Common exit and cleanup
 */
done:
    if (websValid(wp)) {
        websPageClose(wp);
        if (jsid >= 0) {
            jsCloseEngine(jsid);
        }
    }
    gfree(buf);
    gfree(rbuf);
    return rc;
}


/*
    Define a Javascript function. Bind an Javascript name to a C procedure.
 */
int websJsDefine(char_t *name, 
    int (*fn)(int jsid, Webs *wp, int argc, char_t **argv))
{
    return jsSetGlobalFunctionDirect(websJsFunctions, name, 
        (int (*)(int, void*, int, char_t**)) fn);
}


/*
    Javascript write command. This implemements <% write("text"); %> command
 */
int websJsWrite(int jsid, Webs *wp, int argc, char_t **argv)
{
    int     i;

    gassert(websValid(wp));
    
    for (i = 0; i < argc; ) {
        gassert(argv);
        if (websWriteBlock(wp, argv[i], gstrlen(argv[i])) < 0) {
            return -1;
        }
        if (++i < argc) {
            if (websWriteBlock(wp, T(" "), 2) < 0) {
                return -1;
            }
        }
    }
    return 0;
}


/*
    Find s2 in s1. We skip leading white space in s1.  Return a pointer to the location in s1 after s2 ends.
 */
static char_t *strtokcmp(char_t *s1, char_t *s2)
{
    ssize     len;

    s1 = skipWhite(s1);
    len = gstrlen(s2);
    for (len = gstrlen(s2); len > 0 && (tolower(*s1) == tolower(*s2)); len--) {
        if (*s2 == '\0') {
            return s1;
        }
        s1++;
        s2++;
    }
    if (len == 0) {
        return s1;
    }
    return NULL;
}


static char_t *skipWhite(char_t *s) 
{
    gassert(s);

    if (s == NULL) {
        return s;
    }
    while (*s && gisspace(*s)) {
        s++;
    }
    return s;
}

#else
int websJsHandler(Webs *wp, char_t *prefix, char_t *dir, int arg) { return 0; }
int websJsOpen() { return 0; }
void websJsClose() {}
#endif /* BIT_JAVASCRIPT */

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
