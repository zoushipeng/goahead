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

/********************************** Forwards **********************************/

#define ALIGN(x) (((x) + 4 - 1) & ~(4 - 1))
static void initPlatform();
static void sigHandler(int signo);
static void usage();

static int aliasTest(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query);
#if BIT_JAVASCRIPT
static int aspTest(int eid, webs_t wp, int argc, char_t **argv);
static int bigTest(int eid, webs_t wp, int argc, char_t **argv);
#endif
static void formTest(webs_t wp, char_t *path, char_t *query);

static int finished = 0;

/*********************************** Code *************************************/

MAIN(goahead, int argc, char **argv, char **envp)
{
    char_t  *argp, *home, *ipAddrPort, *ip, *documents;
    int     port, sslPort, argind;

    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;
        } else if (gmatch(argp, "--debug")) {
            websDebug = 1;
        } else if (gmatch(argp, "--home")) {
            if (argind >= argc) usage();
            home = argv[++argind];
            if (chdir(home) < 0) {
                error(E_L, E_LOG, T("Can't change directory to %s"), home);
                exit(-1);
            }
#if BIT_DEBUG_LOG
        } else if (gmatch(argp, "--log") || gmatch(argp, "-l")) {
            if (argind >= argc) usage();
            traceSetPath(argv[++argind]);

        } else if (gmatch(argp, "--verbose") || gmatch(argp, "-v")) {
            traceSetPath("stdout:4");
#endif

        } else if (gmatch(argp, "--version") || gmatch(argp, "-V")) {
            //  MOB - replace
            printf("%s: %s-%s\n", BIT_PRODUCT, BIT_VERSION, BIT_BUILD_NUMBER);
            exit(0);
        } else {
            usage();
        }
    }
    ip = NULL;
    port = BIT_HTTP_PORT;
    sslPort = BIT_SSL_PORT;
    documents = BIT_DOCUMENTS;
    if (argc > argind) {
        if (argc > (argind + 2)) usage();
        ipAddrPort = bstrdup(argv[argind++]);
        socketParseAddress(ipAddrPort, &ip, &port, 80);
        if (argc > argind) {
            documents = argv[argind++];
        }
    }
    initPlatform();
    if (websOpen() < 0) {
        error(E_L, E_LOG, T("Can't initialize Goahead server. Exiting."));
        return -1;
    }
#if BIT_PACK_SSL
    websSSLSetKeyFile("server.key");
    websSSLSetCertFile("server.crt");
#endif
    websSetPassword("pass1");
#if BIT_USER_MANAGEMENT
    if (umOpen(T("umconfig.db")) < 0) {
        return -1;
    }
#endif
    
    if (websOpenServer(ip, port, sslPort, documents) < 0) {
        error(E_L, E_LOG, T("Can't open GoAhead server. Exiting."));
        return -1;
    }
    websUrlHandlerDefine(T(""), 0, 0, websSecurityHandler, WEBS_HANDLER_FIRST);
    websUrlHandlerDefine(T("/form"), 0, 0, websFormHandler, 0);
    websUrlHandlerDefine(T("/cgi-bin"), 0, 0, websCgiHandler, 0);
    websUrlHandlerDefine(T("/alias/"), 0, 0, aliasTest, 0);
    websUrlHandlerDefine(T("/"), 0, 0, websDefaultHomePageHandler, 0); 
    websUrlHandlerDefine(T(""), 0, 0, websDefaultHandler, WEBS_HANDLER_LAST); 

#if BIT_USER_MANAGEMENT_GUI
    formDefineUserMgmt();
#endif

#if BIT_JAVASCRIPT
    websAspDefine(T("aspTest"), aspTest);
    websAspDefine(T("bigTest"), bigTest);
#endif
    websFormDefine(T("test"), formTest);

    websServiceEvents(&finished);

#if BIT_USER_MANAGEMENT
    umClose();
#endif
    websClose();
    return 0;
}


static void usage() {
    //  MOB - replace
    fprintf(stderr, "\n%s Usage:\n\n"
        "  %s [options] [IPaddress][:port] [documents]\n\n"
        "  Options:\n"
        "    --debug                # Run in debug mode\n"
        "    --home directory       # Change to directory to run\n"
        "    --log logFile:level    # Log to file file at verbosity level\n"
        "    --verbose              # Same as --log stderr:2\n"
        "    --version              # Output version information\n\n",
        BIT_TITLE, BIT_PRODUCT);
    exit(-1);
}


void initPlatform() 
{
#if BIT_UNIX_LIKE || VXWORKS
    signal(SIGTERM, sigHandler);
    signal(SIGKILL, sigHandler);
    #ifdef SIGPIPE
        signal(SIGPIPE, SIG_IGN);
    #endif
#elif BIT_WIN_LIKE
    _fmode=_O_BINARY;
#endif
}


static void sigHandler(int signo)
{
    finished = 1;
}


/*
    Rewrite /alias => /alias/atest.html
 */
static int aliasTest(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
    if (gmatch(urlPrefix, "/alias/")) {
        websRewriteRequest(wp, "/alias/atest.html");
    }
    return 0;
}


#if BIT_JAVASCRIPT
/*
    Parse the form variables: name, address and echo back
 */
static int aspTest(int eid, webs_t wp, int argc, char_t **argv)
{
	char_t	*name, *address;

	if (ejArgs(argc, argv, T("%s %s"), &name, &address) < 2) {
		websError(wp, 400, T("Insufficient args\n"));
		return -1;
	}
	return (int) websWrite(wp, T("Name: %s, Address %s"), name, address);
}


/*
    Generate a large response
 */
static int bigTest(int eid, webs_t wp, int argc, char_t **argv)
{
    int     i;

	websHeader(wp);
    for (i = 0; i < 800; i++) {
        websWrite(wp, " Line: %05d %s", i, "aaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccccddddddd<br/>\r\n");
    }
	websFooter(wp);
    websDone(wp, 200);
    return 0;
}
#endif


/*
    Implement /form/formTest. Parse the form variables: name, address and echo back.
 */
static void formTest(webs_t wp, char_t *path, char_t *query)
{
	char_t	*name, *address;

	name = websGetVar(wp, T("name"), NULL);
	address = websGetVar(wp, T("address"), NULL);
	websHeader(wp);
	websWrite(wp, T("<body><h2>name: %s, address: %s</h2>\n"), name, address);
	websFooter(wp);
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
