/*
    goahead.c -- Main program for GoAhead

    Usage: goahead [options] [IPaddress][:port] [documents]
        Options:
        --home directory       # Change to directory to run
        --log logFile:level    # Log to file file at verbosity level
        --verbose              # Same as --log stderr:2
        --version              # Output version information

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/********************************* Defines ************************************/

#define ALIGN(x) (((x) + 4 - 1) & ~(4 - 1))
static int finished = 0;

/********************************* Forwards ***********************************/

static void initPlatform();
static void usage();

#if WINDOWS
static void windowsClose();
static int windowsInit();
static long CALLBACK websWindProc(HWND hwnd, UINT msg, UINT wp, LPARAM lp);
#endif

#if BIT_UNIX_LIKE
static void sigHandler(int signo);
#endif

/*********************************** Code *************************************/

MAIN(goahead, int argc, char **argv, char **envp)
{
    char_t  *argp, *home, *ipAddrPort, *ip, *documents;
    int     port, sslPort, argind;

#if WINDOWS
    if (windowsInit() < 0) {
        return 0;
    }
#endif
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
    if (websOpenServer(ip, port, sslPort, documents) < 0) {
        error(E_L, E_LOG, T("Can't open GoAhead server. Exiting."));
        return -1;
    }
    websUrlHandlerDefine(T(""), 0, 0, websSecurityHandler, WEBS_HANDLER_FIRST);
    websUrlHandlerDefine(T("/forms"), 0, 0, websFormHandler, 0);
    websUrlHandlerDefine(T("/cgi-bin"), 0, 0, websCgiHandler, 0);
    websUrlHandlerDefine(T("/"), 0, 0, websDefaultHomePageHandler, 0); 
    websUrlHandlerDefine(T(""), 0, 0, websDefaultHandler, WEBS_HANDLER_LAST); 

    /*
        Service events till terminated
     */
    websServiceEvents(&finished);
    websClose();
#if WINDOWS
    windowsClose();
#endif
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


#if WINDOWS
/*
    Create a taskbar entry. Register the window class and create a window
 */
static int windowsInit()
{
    HINSTANCE   inst;
    WNDCLASS    wc;                     /* Window class */
    HMENU       hSysMenu;
    HWND        hwnd;

    inst = ggetAppInstance();
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = inst;
    wc.hIcon         = NULL;
    wc.lpfnWndProc   = (WNDPROC) websWindProc;
    wc.lpszMenuName  = wc.lpszClassName = BIT_PRODUCT;
    if (! RegisterClass(&wc)) {
        return -1;
    }
    /*
        Create a window just so we can have a taskbar to close this web server
     */
    hwnd = CreateWindow(BIT_PRODUCT, BIT_TITLE, WS_MINIMIZE | WS_POPUPWINDOW, CW_USEDEFAULT, 
        0, 0, 0, NULL, NULL, inst, NULL);
    if (hwnd == NULL) {
        return -1;
    }

    /*
        Add the about box menu item to the system menu
     */
    hSysMenu = GetSystemMenu(hwnd, FALSE);
    if (hSysMenu != NULL) {
        AppendMenu(hSysMenu, MF_SEPARATOR, 0, NULL);
#if UNUSED
        AppendMenu(hSysMenu, MF_STRING, IDM_ABOUTBOX, T("About WebServer"));
#endif
    }
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    return 0;
}


static void windowsClose()
{
    HINSTANCE   inst;

    inst = ggetAppInstance();
    UnregisterClass(BIT_PRODUCT, inst);
#if UNUSED
    if (hwndAbout) {
        DestroyWindow(hwndAbout);
    }
#endif
}


/*
    Main menu window message handler.
 */
static long CALLBACK websWindProc(HWND hwnd, UINT msg, UINT wp, LPARAM lp)
{
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            finished++;
            return 0;

        case WM_SYSCOMMAND:
#if UNUSED
            if (wp == IDM_ABOUTBOX) {
                if (!hwndAbout) {
                    createAboutBox((HINSTANCE) emfInstGet(), hwnd);
                }
                if (hwndAbout) {
                    ShowWindow(hwndAbout, SW_SHOWNORMAL);
                }
            }
#endif
            break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}


/*
    Check for Windows Messages
 */
WPARAM checkWindowsMsgLoop()
{
    MSG     msg;

    if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
        if (!GetMessage(&msg, NULL, 0, 0) || msg.message == WM_QUIT) {
            return msg.wParam;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}


/*
    Windows message handler for the About Box
 */
static long CALLBACK websAboutProc(HWND hwndDlg, uint msg, uint wp, long lp)
{
    long    lResult;

    lResult = DefWindowProc(hwndDlg, msg, wp, lp);

    switch (msg) {
        case WM_CREATE:
#if UNUSED
            hwndAbout = hwndDlg;
#endif
            break;

        case WM_DESTROY:
#if UNUSED
            hwndAbout = NULL;
#endif
            break;

        case WM_COMMAND:
#if UNUSED
            if (wp == IDOK) {
                EndDialog(hwndDlg, 0);
                PostMessage(hwndDlg, WM_CLOSE, 0, 0);
            }
#endif
            break;

#if UNUSED
        case WM_INITDIALOG:
            /*
                Set the version and build date values
             */
            hwnd = GetDlgItem(hwndDlg, IDC_VERSION);
            if (hwnd) {
                SetWindowText(hwnd, BIT_VERSION);
            }
            hwnd = GetDlgItem(hwndDlg, IDC_BUILDDATE);
            if (hwnd) {
                SetWindowText(hwnd, __DATE__);
            }
            SetWindowText(hwndDlg, T("GoAhead WebServer"));
            centerWindowOnDisplay(hwndDlg);
            hwndAbout = hwndDlg;
            lResult = FALSE;
            break;
#endif
    }
    return lResult;
}


#if UNUSED
/*
    Registers the About Box class
 */
static int registerAboutBox(HINSTANCE hInstance)
{
    WNDCLASS  wc;

    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC)websAboutProc;

    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(LTGRAY_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = IDS_ABOUTBOX;
    if (!RegisterClass(&wc)) {
        return 0;
    }
    return 1;
}
#endif


#if UNUSED
/*
     Helper routine.  Takes second parameter as Ansi string, copies it to first parameter as wide character (16-bits /
     char) string, and returns integer number of wide characters (words) in string (including the trailing wide char
     NULL).
 */
//  MOB - refactor
static int nCopyAnsiToWideChar(LPWORD lpWCStr, LPSTR lpAnsiIn)
{
    int cchAnsi = lstrlen(lpAnsiIn);
    return MultiByteToWideChar(GetACP(), MB_PRECOMPOSED, lpAnsiIn, cchAnsi, lpWCStr, cchAnsi) + 1;
}


/*
    Creates an About Box Window
    MOB - get rid of about box. Not worth all this
 */
static int createAboutBox(HINSTANCE hInstance, HWND hwnd)
{
    WORD    *p, *pdlgtemplate;
    int     nchar;
    DWORD   lStyle;
    HWND    hwndReturn;

    /* 
        Allocate some memory to play with  
     */
    pdlgtemplate = p = (PWORD) LocalAlloc(LPTR, 1000);

    /*
        Start to fill in the dlgtemplate information.  addressing by WORDs 
     */
    lStyle = WS_DLGFRAME | WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | DS_SETFONT;
    *p++ = LOWORD(lStyle);
    *p++ = HIWORD(lStyle);
    *p++ = 0;          /* LOWORD (lExtendedStyle) */
    *p++ = 0;          /* HIWORD (lExtendedStyle) */
    *p++ = 7;          /* Number Of Items   */
    *p++ = 210;        /* x */
    *p++ = 10;         /* y */
    *p++ = 200;        /* cx */
    *p++ = 100;        /* cy */
    *p++ = 0;          /* Menu */
    *p++ = 0;          /* Class */

    /* 
        Copy the title of the dialog 
     */
    nchar = nCopyAnsiToWideChar(p, BIT_TITLE);
    p += nchar;

    /*  
        Font information because of DS_SETFONT
     */
    *p++ = 11;     /* point size */
    nchar = nCopyAnsiToWideChar(p, T("Arial Bold"));
    p += nchar;
    p = ALIGN(p);

    /*
        Now start with the first item (Product Identifier)
     */
    lStyle = SS_CENTER | WS_VISIBLE | WS_CHILD | WS_TABSTOP;
    *p++ = LOWORD(lStyle);
    *p++ = HIWORD(lStyle);
    *p++ = 0;           /* LOWORD (lExtendedStyle) */
    *p++ = 0;           /* HIWORD (lExtendedStyle) */
    *p++ = 10;          /* x */
    *p++ = 10;          /* y  */
    *p++ = 180;         /* cx */
    *p++ = 15;          /* cy */
    *p++ = 1;           /* ID */

    /*
        Fill in class i.d., this time by name
     */
    nchar = nCopyAnsiToWideChar(p, TEXT("STATIC"));
    p += nchar;

    /*
        Copy the text of the first item
     */
    nchar = nCopyAnsiToWideChar(p, TEXT("GoAhead WebServer ") BIT_VERSION);
    p += nchar;
    *p++ = 0;  
    p = ALIGN(p);

/*
 *  Next, the Copyright Notice.
 */
    lStyle = SS_CENTER | WS_VISIBLE | WS_CHILD | WS_TABSTOP;
    *p++ = LOWORD(lStyle);
    *p++ = HIWORD(lStyle);
    *p++ = 0;           /* LOWORD (lExtendedStyle) */
    *p++ = 0;           /* HIWORD (lExtendedStyle) */
    *p++ = 10;          /* x */
    *p++ = 30;          /* y  */
    *p++ = 180;         /* cx */
    *p++ = 15;          /* cy */
    *p++ = 1;           /* ID */

    /*
        Fill in class i.d. by name
     */
    nchar = nCopyAnsiToWideChar(p, TEXT("STATIC"));
    p += nchar;

    /*
        Copy the text of the item
     */
    nchar = nCopyAnsiToWideChar(p, GOAHEAD_COPYRIGHT);
    p += nchar;
    *p++ = 0;  
    p = ALIGN(p);

    /*
        Add third item ("Version:")
     */
    lStyle = SS_RIGHT | WS_VISIBLE | WS_CHILD | WS_TABSTOP;
    *p++ = LOWORD(lStyle);
    *p++ = HIWORD(lStyle);
    *p++ = 0;          
    *p++ = 0;
    *p++ = 28;
    *p++ = 50;
    *p++ = 70;
    *p++ = 10;
    *p++ = 1;
    nchar = nCopyAnsiToWideChar(p, T("STATIC"));
    p += nchar;
    nchar = nCopyAnsiToWideChar(p, T("Version:"));
    p += nchar;
    *p++ = 0;
    p = ALIGN(p);

    /*
        Add fourth Item (IDC_VERSION)
     */
    lStyle = SS_LEFT | WS_VISIBLE | WS_CHILD | WS_TABSTOP;
    *p++ = LOWORD(lStyle);
    *p++ = HIWORD(lStyle);
    *p++ = 0;          
    *p++ = 0;
    *p++ = 102;
    *p++ = 50;
    *p++ = 70;
    *p++ = 10;
    *p++ = IDC_VERSION;
    nchar = nCopyAnsiToWideChar(p, T("STATIC"));
    p += nchar;
    nchar = nCopyAnsiToWideChar(p, T("version"));
    p += nchar;
    *p++ = 0;
    p = ALIGN(p);

    /*
        Add fifth item ("Build Date:")
     */
    lStyle = SS_RIGHT | WS_VISIBLE | WS_CHILD | WS_TABSTOP;
    *p++ = LOWORD(lStyle);
    *p++ = HIWORD(lStyle);
    *p++ = 0;          
    *p++ = 0;
    *p++ = 28;
    *p++ = 65;
    *p++ = 70;
    *p++ = 10;
    *p++ = 1;
    nchar = nCopyAnsiToWideChar(p, T("STATIC"));
    p += nchar;
    nchar = nCopyAnsiToWideChar(p, T("Build Date:"));
    p += nchar;
    *p++ = 0;
    p = ALIGN(p);

    /*
        Add sixth item (IDC_BUILDDATE)
     */
    lStyle = SS_LEFT | WS_VISIBLE | WS_CHILD | WS_TABSTOP;
    *p++ = LOWORD(lStyle);
    *p++ = HIWORD(lStyle);
    *p++ = 0;          
    *p++ = 0;
    *p++ = 102;
    *p++ = 65;
    *p++ = 70;
    *p++ = 10;
    *p++ = IDC_BUILDDATE;
    nchar = nCopyAnsiToWideChar(p, T("STATIC"));
    p += nchar;
    nchar = nCopyAnsiToWideChar(p, T("Build Date"));
    p += nchar;
    *p++ = 0;
    p = ALIGN(p);

    /*
        Add seventh item (IDOK)
     */
    lStyle = BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | WS_TABSTOP;
    *p++ = LOWORD(lStyle);
    *p++ = HIWORD(lStyle);
    *p++ = 0;          
    *p++ = 0;
    *p++ = 80;
    *p++ = 80;
    *p++ = 40;
    *p++ = 10;
    *p++ = IDOK;
    nchar = nCopyAnsiToWideChar(p, T("BUTTON"));
    p += nchar;
    nchar = nCopyAnsiToWideChar(p, T("OK"));
    p += nchar;
    *p++ = 0;

    hwndReturn = CreateDialogIndirect(hInstance, (LPDLGTEMPLATE) pdlgtemplate, hwnd, (DLGPROC) websAboutProc);
    LocalFree(LocalHandle(pdlgtemplate));
    return 0;
}
#endif

#endif


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
