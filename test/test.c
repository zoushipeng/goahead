/*
    test.c -- Unit test program for GoAhead

    Usage: goahead-test [options] [IPaddress][:port] [documents]
        Options:
        --home directory       # Change to directory to run
        --log logFile:level    # Log to file file at verbosity level
        --verbose              # Same as --log stderr:2
        --version              # Output version information

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"
#include    "js.h"

/********************************* Defines ************************************/

#define ALIGN(x) (((x) + 4 - 1) & ~(4 - 1))
static int finished = 0;

/********************************* Forwards ***********************************/

static void initPlatform();
static void usage();

static int aliasTest(Webs *wp, char_t *prefix, char_t *dir, int arg);
#if BIT_JAVASCRIPT
static int aspTest(int eid, Webs *wp, int argc, char_t **argv);
static int bigTest(int eid, Webs *wp, int argc, char_t **argv);
#endif
static void procTest(Webs *wp, char_t *path, char_t *query);
static void sessionTest(Webs *wp, char_t *path, char_t *query);
static void showTest(Webs *wp, char_t *path, char_t *query);
static void uploadTest(Webs *wp, char_t *path, char_t *query);

#if BIT_UNIX_LIKE
static void sigHandler(int signo);
#endif

/*********************************** Code *************************************/

MAIN(goahead, int argc, char **argv, char **envp)
{
    char_t  *argp, *home, *documents, *endpoint, addr[32], *route;
    int     argind;

    route = "route.txt";

    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;

        } else if (gmatch(argp, "--background") || gmatch(argp, "-b")) {                                   
            websSetBackground(1);

        } else if (gmatch(argp, "--debug")) {
            websSetDebug(1);

        } else if (gmatch(argp, "--home")) {
            if (argind >= argc) usage();
            home = argv[++argind];
            if (chdir(home) < 0) {
                error(T("Can't change directory to %s"), home);
                exit(-1);
            }
        } else if (gmatch(argp, "--log") || gmatch(argp, "-l")) {
            if (argind >= argc) usage();
            traceSetPath(argv[++argind]);

        } else if (gmatch(argp, "--verbose") || gmatch(argp, "-v")) {
            traceSetPath("stdout:4");

        } else if (gmatch(argp, "--route") || gmatch(argp, "-r")) {
            route = argv[++argind];

        } else if (gmatch(argp, "--version") || gmatch(argp, "-V")) {
            printf("%s: %s-%s\n", BIT_PRODUCT, BIT_VERSION, BIT_BUILD_NUMBER);
            exit(0);

        } else {
            usage();
        }
    }
    documents = BIT_DOCUMENTS;
    if (argc > argind) {
        documents = argv[argind++];
    }
    initPlatform();
    if (websOpen(documents, route) < 0) {
        error(T("Can't initialize server. Exiting."));
        return -1;
    }
    if (argind < argc) {
        while (argind < argc) {
            endpoint = argv[argind++];
            if (websListen(endpoint) < 0) {
                return -1;
            }
        }
    } else {
        if (BIT_HTTP_PORT > 0) {
            gfmtStatic(addr, sizeof(addr), "http://:%d", BIT_HTTP_PORT);
            if (websListen(addr) < 0) {
                return -1;
            }
        }
        if (BIT_HTTP_V6_PORT > 0) {
            gfmtStatic(addr, sizeof(addr), "http://[::]:%d", BIT_HTTP_V6_PORT);
            if (websListen(addr) < 0) {
                return -1;
            }
        }
#if BIT_PACK_SSL
        if (BIT_SSL_PORT > 0) {
            gfmtStatic(addr, sizeof(addr), "https://:%d", BIT_SSL_PORT);
            if (websListen(addr) < 0) {
                return -1;
            }
        }
#endif
    }
    websUrlHandlerDefine(T("/proc/"), 0, 0, websProcHandler, 0);
    websUrlHandlerDefine(T("/cgi-bin"), 0, 0, websCgiHandler, 0);
    websUrlHandlerDefine(T("/alias/"), 0, 0, aliasTest, 0);
#if BIT_JAVASCRIPT
    websUrlHandlerDefine(T("/"), 0, 0, websJsHandler, 0);
#endif
    websUrlHandlerDefine(T("/"), 0, 0, websHomePageHandler, 0); 
    websUrlHandlerDefine(T(""), 0, 0, websFileHandler, WEBS_HANDLER_LAST); 

#if BIT_JAVASCRIPT
    websJsDefine(T("aspTest"), aspTest);
    websJsDefine(T("bigTest"), bigTest);
#endif
    websProcDefine(T("test"), procTest);
    websProcDefine(T("sessionTest"), sessionTest);
    websProcDefine(T("showTest"), showTest);
    websProcDefine(T("uploadTest"), uploadTest);
#if BIT_UNIX_LIKE
    /*
        Service events till terminated
    */
    if (websGetBackground()) {
        if (daemon(0, 0) < 0) {
            error("Can't run as daemon");
            return -1;
        }
    }
#endif
    websServiceEvents(&finished);
    websClose();
    return 0;
}


static void usage() {
    //  MOB - replace
    fprintf(stderr, "\n%s Usage:\n\n"
        "  %s [options] [documents] [IPaddress][:port]...\n\n"
        "  Options:\n"
        "    --debug                # Run in debug mode\n"
        "    --home directory       # Change to directory to run\n"
        "    --log logFile:level    # Log to file file at verbosity level\n"
        "    --route routeFile      # Route configuration file\n"
        "    --verbose              # Same as --log stderr:2\n"
        "    --version              # Output version information\n\n",
        BIT_TITLE, BIT_PRODUCT);
    exit(-1);
}


void initPlatform() 
{
#if BIT_UNIX_LIKE
    signal(SIGTERM, sigHandler);
    signal(SIGKILL, sigHandler);
    #ifdef SIGPIPE
        signal(SIGPIPE, SIG_IGN);
    #endif
#elif BIT_WIN_LIKE
    _fmode=_O_BINARY;
#endif
}


#if BIT_UNIX_LIKE
static void sigHandler(int signo)
{
    finished = 1;
}
#endif


/*
    Rewrite /alias => /alias/atest.html
 */
static int aliasTest(Webs *wp, char_t *prefix, char_t *dir, int arg)
{
    if (gmatch(prefix, "/alias/")) {
        websRewriteRequest(wp, "/alias/atest.html");
    }
    return 0;
}


#if BIT_JAVASCRIPT
/*
    Parse the form variables: name, address and echo back
 */
static int aspTest(int eid, Webs *wp, int argc, char_t **argv)
{
	char_t	*name, *address;

	if (jsArgs(argc, argv, T("%s %s"), &name, &address) < 2) {
		websError(wp, 400, T("Insufficient args\n"));
		return -1;
	}
	return (int) websWrite(wp, T("Name: %s, Address %s"), name, address);
}


/*
    Generate a large response
 */
static int bigTest(int eid, Webs *wp, int argc, char_t **argv)
{
    int     i;

    websWriteHeaders(wp, 200, -1, 0);
    websWriteHeader(wp, T("\r\n"));
    websWrite(wp, T("<html>\n"));
    for (i = 0; i < 800; i++) {
        websWrite(wp, " Line: %05d %s", i, "aaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccccddddddd<br/>\r\n");
    }
    websWrite(wp, T("</html>\n"));
    websDone(wp, 200);
    return 0;
}
#endif


/*
    Implement /proc/procTest. Parse the form variables: name, address and echo back.
 */
static void procTest(Webs *wp, char_t *path, char_t *query)
{
	char_t	*name, *address;

	name = websGetVar(wp, T("name"), NULL);
	address = websGetVar(wp, T("address"), NULL);
    websWriteHeaders(wp, 200, -1, 0);
    websWriteHeader(wp, T("\r\n"));
	websWrite(wp, T("<html><body><h2>name: %s, address: %s</h2></body></html>\n"), name, address);
	websDone(wp, 200);
}


static void sessionTest(Webs *wp, char_t *path, char_t *query)
{
	char_t	*number;

    if (gcaselessmatch(wp->method, "POST")) {
        number = websGetVar(wp, T("number"), 0);
        websSetSessionVar(wp, "number", number);
    } else {
        number = websGetSessionVar(wp, "number", 0);
    }
    websWriteHeaders(wp, 200, -1, 0);
    websWriteHeader(wp, T("\r\n"));
    websWrite(wp, T("<html><body><p>Number %s</p></body></html>\n"), number);
    websDone(wp, 200);
}


static void showTest(Webs *wp, char_t *path, char_t *query)
{
    WebsKey     *s;

    websWriteHeaders(wp, 200, -1, 0);
    websWriteHeader(wp, T("\r\n"));
    websWrite(wp, T("<html><body><pre>\n"));
    for (s = symFirst(wp->vars); s != NULL; s = symNext(wp->vars, s)) {
        websWrite(wp, "%s=%s\n", s->name.value.string, s->content.value.string);
    }
    websWrite(wp, T("</pre></body></html>\n"));
    websDone(wp, 200);
}


static void uploadTest(Webs *wp, char_t *path, char_t *query)
{
    WebsKey     *s;

    websWriteHeaders(wp, 200, -1, 0);
    websWriteHeader(wp, T("Content-Type: text/plain\r\n\r\n"));
    if (gcaselessmatch(wp->method, "POST")) {
        for (s = symFirst(wp->vars); s != NULL; s = symNext(wp->vars, s)) {
            websWrite(wp, "%s=%s\r\n", s->name.value.string, s->content.value.string);
        }
    }
    websDone(wp, 200);
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
