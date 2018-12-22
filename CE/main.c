/*
 * main.c -- Main program for the GoAhead WebServer
 *
 * Copyright (c) GoAhead Software Inc., 1995-2000. All Rights Reserved.
 *
 * See the file "license.txt" for usage and redistribution license requirements
 *
 * $Id: main.c,v 1.3 2002/01/24 21:57:47 bporter Exp $
 */

/******************************** Description *********************************/

/*
 *	Main program for the GoAhead WebServer. This is a demonstration
 *	program to initialize and configure the web server.
 */

/********************************* Includes ***********************************/

#include	<windows.h>
#include	<winsock.h>

#include	"../wsIntrn.h"

#ifdef WEBS_SSL_SUPPORT
#include	"../websSSL.h"
#endif

#ifdef USER_MANAGEMENT_SUPPORT
#include	"../um.h"
void		formDefineUserMgmt(void);
#endif

/*********************************** Locals ***********************************/
/*
 *	Change configuration here
 */
static HWND			hwnd;							/* Window handle */
static int			sockServiceTime = 1000;			/* milliseconds */
static char_t		*title = T("GoAhead WebServer");/* Window title */
static char_t		*name = T("gowebs");			/* Window name */
static char_t		*rootWeb = T("web");			/* Root web directory */
static char_t		*password = T("");				/* Security password */
static int			port = 80;						/* Server port */
static int			retries = 5;					/* Server port retries */
static int			finished;						/* Finished flag */

/****************************** Forward Declarations **************************/

static int 	initWebs();
static long	CALLBACK websWindProc(HWND hwnd, unsigned int msg, 
				unsigned int wp, long lp);
static int	aspTest(int eid, webs_t wp, int argc, char_t **argv);
static void formTest(webs_t wp, char_t *path, char_t *query);
static int	windowsInit(HINSTANCE hinstance);
static int  websHomePageHandler(webs_t wp, char_t *urlPrefix, char_t *webDir,
			int arg, char_t *url, char_t *path, char_t *query);
extern void defaultErrorHandler(int etype, char_t *msg);
extern void defaultTraceHandler(int level, char_t *buf);
static void printMemStats(int handle, char_t *fmt, ...);
static void memLeaks();

/*********************************** Code *************************************/
/*
 *	WinMain -- entry point from Windows
 */

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprevinstance,
					LPTSTR args, int cmd_show)
{
/*
 *	Initialize the memory allocator. Allow use of malloc and start 
 *	with a 60K heap.  For each page request approx 8KB is allocated.
 *	60KB allows for several concurrent page requests.  If more space
 *	is required, malloc will be used for the overflow.
 */
	bopen(NULL, (60 * 1024), B_USE_MALLOC);

/* 
 *	Store the instance handle (used in socket.c)
 */
	if (windowsInit(hinstance) < 0) {
		return FALSE;
	}

/*
 *	Initialize the web server
 */
	if (initWebs() < 0) {
		return FALSE;
	}

#ifdef WEBS_SSL_SUPPORT
	websSSLOpen();
#endif

/*
 *	Basic event loop. SocketReady returns true when a socket is ready for
 *	service. SocketSelect will block until an event occurs. SocketProcess
 *	will actually do the servicing.
 */
	while (!finished) {
		if (socketReady(-1) || socketSelect(-1, sockServiceTime)) {
			socketProcess(-1);
		}
		emfSchedProcess();
		websCgiCleanup();
	}

#ifdef WEBS_SSL_SUPPORT
	websSSLClose();
#endif

/*
 *	Close the User Management database
 */
#ifdef USER_MANAGEMENT_SUPPORT
	umClose();
#endif

/*
 *	Close the socket module, report memory leaks and close the memory allocator
 */
	websCloseServer();
	socketClose();
#ifdef B_STATS
	memLeaks();
#endif
	bclose();
	return 0;
}

/******************************************************************************/
/*
 *	Initialize the web server.
 */

static int initWebs()
{
	struct hostent *hp;
	struct in_addr	intaddr;
	char_t			wbuf[128];
	char			host[64];
	char			*cp;

/*
 *	Initialize the socket subsystem
 */
	socketOpen();

/*
 *	Initialize the User Management database
 */
#ifdef USER_MANAGEMENT_SUPPORT
	umOpen();
	umRestore(T("umconfig.txt"));
#endif

/*
 *	Define the local Ip address, host name, default home page and the 
 *	root web directory.
 */
	if (gethostname(host, sizeof(host)) < 0) {
		error(E_L, E_LOG, T("Can't get hostname"));
		return -1;
	}
	if ((hp = gethostbyname(host)) == NULL) {
		error(E_L, E_LOG, T("Can't get host address"));
		return -1;
	}
	memcpy((void*) &intaddr, (void*) hp->h_addr_list[0],
		(size_t) hp->h_length);

/*
 *	Set /web as the root web. Modify this to suit your needs
 */
	websSetDefaultDir(T("/web"));

/*
 *	Set the IP address and host name.
 */
	cp = inet_ntoa(intaddr);
	ascToUni(wbuf, cp, min(strlen(cp) + 1, sizeof(wbuf)));
	websSetIpaddr(wbuf);
	ascToUni(wbuf, hp->h_name, min(strlen(hp->h_name) + 1, sizeof(wbuf)));
	websSetHost(wbuf);

/*
 *	Configure the web server options before opening the web server
 */
	websSetDefaultPage(T("default.asp"));
	websSetPassword(password);

/* 
 *	Open the web server on the given port. If that port is taken, try
 *	the next sequential port for up to "retries" attempts.
 */
	websOpenServer(port, retries);

/*
 * 	First create the URL handlers. Note: handlers are called in sorted order
 *	with the longest path handler examined first. Here we define the security 
 *	handler, forms handler and the default web page handler.
 */
	websUrlHandlerDefine(T(""), NULL, 0, websSecurityHandler, 
						WEBS_HANDLER_FIRST);
	websUrlHandlerDefine(T("/goform"), NULL, 0, websFormHandler, 0);
	websUrlHandlerDefine(T("/cgi-bin"), NULL, 0, websCgiHandler, 0);
	websUrlHandlerDefine(T(""), NULL, 0, websDefaultHandler, 
						WEBS_HANDLER_LAST); 

/*
 *	Now define two test procedures. Replace these with your application
 *	relevant ASP script procedures and form functions.
 */
	websAspDefine(T("aspTest"), aspTest);
	websFormDefine(T("formTest"), formTest);

/*
 *	Create the Form handlers for the User Management pages
 */
#ifdef USER_MANAGEMENT_SUPPORT
	formDefineUserMgmt();
#endif

/*
 *	Create a handler for the default home page
 */
	websUrlHandlerDefine(T("/"), NULL, 0, websHomePageHandler, 0); 
	return 0;
}

/******************************************************************************/
/*
 *	Create a taskbar entry. Register the window class and create a window
 */

static int windowsInit(HINSTANCE hinstance)
{
/*
 *	Restore this code if you want to call Create Window
 */
#if 0
	WNDCLASS  		wc;						/* Window class */

	emfInstSet((int) hinstance);

	wc.style		 = 0;
	wc.hbrBackground = NULL;
	wc.hCursor		 = NULL;
	wc.cbClsExtra	 = 0;
	wc.cbWndExtra	 = 0;
	wc.hInstance	 = hinstance;
	wc.hIcon		 = NULL;
	wc.lpfnWndProc	 = (WNDPROC) websWindProc;
	wc.lpszMenuName	 = NULL;
	wc.lpszClassName = name;
	if (! RegisterClass(&wc)) {
		return -1;
	}

/*
 *	Create a window just so we can have a taskbar to close this web server
 */
	hwnd = CreateWindow(name, title, WS_VISIBLE,
		CW_USEDEFAULT, 0, 0, 0, NULL, NULL, hinstance, NULL);
	if (hwnd == NULL) {
		return -1;
	}
	ShowWindow(hwnd, SW_MINIMIZE);
	UpdateWindow(hwnd);

#endif
	return 0;
}

/*
 *	Restore websWindProc if you want to use windowsInit and call CreateWindow
 */
#if 0
/******************************************************************************/
/*
 *	Windows message handler. Just process the window destroy event.
 */

static long CALLBACK websWindProc(HWND hwnd, unsigned int msg, 
	unsigned int wp, long lp)
{
	switch (msg) {
		case WM_DESTROY:
			PostQuitMessage(0);
			finished++;
			return 0;
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}
#endif /* #if 0 */

/******************************************************************************/
/*
 *	Test Javascript binding for ASP. This will be invoked when "aspTest" is
 *	embedded in an ASP page. See web/asp.asp for usage. Set browser to 
 *	"localhost/asp.asp" to test.
 */

static int aspTest(int eid, webs_t wp, int argc, char_t **argv)
{
	char_t	*name, *address;

	if (ejArgs(argc, argv, T("%s %s"), &name, &address) < 2) {
		websError(wp, 400, T("Insufficient args\n"));
		return -1;
	}
	return websWrite(wp, T("Name: %s, Address %s"), name, address);
}

/******************************************************************************/
/*
 *	Test form for posted data (in-memory CGI). This will be called when the
 *	form in web/forms.asp is invoked. Set browser to "localhost/forms.asp" to test.
 */

static void formTest(webs_t wp, char_t *path, char_t *query)
{
	char_t	*name, *address;

	name = websGetVar(wp, T("name"), T("Joe Smith")); 
	address = websGetVar(wp, T("address"), T("1212 Milky Way Ave.")); 

	websHeader(wp);
	websWrite(wp, T("<body><h2>Name: %s, Address: %s</h2>\n"), name, address);
	websFooter(wp);
	websDone(wp, 200);
}

/******************************************************************************/
/*
 *	Home page handler
 */

static int websHomePageHandler(webs_t wp, char_t *urlPrefix, char_t *webDir,
							int arg, char_t *url, char_t *path, char_t *query)
{
/*
 *	If the empty or "/" URL is invoked, redirect default URLs to the home page
 */
	if (*url == '\0' || gstrcmp(url, T("/")) == 0) {
		websRedirect(wp, T("home.asp"));
		return 1;
	}
	return 0;
}

/******************************************************************************/
/*
 *	Default error handler.  The developer should insert code to handle
 *	error messages in the desired manner.
 */

void defaultErrorHandler(int etype, char_t *msg)
{
#if 0
	write(1, msg, gstrlen(msg));
#endif
}

/******************************************************************************/
/*
 *	Trace log. Customize this function to log trace output
 */

void defaultTraceHandler(int level, char_t *buf)
{
/*
 *	The following code would write all trace regardless of level
 *	to stdout.
 */
#if 0
	if (buf) {
		write(1, buf, gstrlen(buf));
	}
#endif
}

/******************************************************************************/
/*
 *	Returns a pointer to an allocated qualified unique temporary file name.
 *	This filename must eventually be deleted with bfree().
 */

char_t *websGetCgiCommName()
{
/* 
 *	tmpnam, tempnam, tmpfile not supported for CE 2.12 or lower.  The Win32 API
 *	GetTempFileName is scheduled to be part of CE 3.0.
 */
#if 0	
	char_t	*pname1, *pname2;

	pname1 = gtmpnam(NULL, T("cgi"));
	pname2 = bstrdup(B_L, pname1);
	free(pname1);
	return pname2;
#endif
	return NULL;
}

/******************************************************************************/
/*
 *	Launch the CGI process and return a handle to it.
 *	CE note: This function is not complete.  The missing piece is the ability
 *	to redirect stdout.
 */

int websLaunchCgiProc(char_t *cgiPath, char_t **argp, char_t **envp, 
					  char_t *stdIn, char_t *stdOut)
{
    PROCESS_INFORMATION	procinfo;		/*  Information about created proc   */
    DWORD				dwCreateFlags;
    char				*fulldir;
	BOOL				bReturn;
	int					i, nLen;

/*
 *	Replace directory delimiters with Windows-friendly delimiters
 */
	nLen = gstrlen(cgiPath);
	for (i = 0; i < nLen; i++) {
		if (cgiPath[i] == '/') {
			cgiPath[i] = '\\';
		}
	}

    fulldir = NULL;
	dwCreateFlags = CREATE_NEW_CONSOLE;

/*
 *	CreateProcess returns errors sometimes, even when the process was    
 *	started correctly.  The cause is not evident.  For now: we detect    
 *	an error by checking the value of procinfo.hProcess after the call.  
 */
    procinfo.hThread = NULL;
    bReturn = CreateProcess(
        cgiPath,			/*  Name of executable module        */
        NULL,				/*  Command line string              */
        NULL,				/*  Process security attributes      */
        NULL,				/*  Thread security attributes       */
        0,					/*  Handle inheritance flag          */
        dwCreateFlags,		/*  Creation flags                   */
        NULL,				/*  New environment block            */
        NULL,				/*  Current directory name           */
        NULL,				/*  STARTUPINFO                      */
        &procinfo);			/*  PROCESS_INFORMATION              */

	if (bReturn == 0) {
		DWORD dw;
		dw = GetLastError();
		return -1;
	} else {
		CloseHandle(procinfo.hThread);
	}
    return (int) procinfo.dwProcessId;
}

/******************************************************************************/
/*
 *	Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */

int websCheckCgiProc(int handle)
{
#if 0
	HANDLE	hCgi;

	hCgi = OpenProcess(0, FALSE, (DWORD)handle);
	if (hCgi != NULL) {
		CloseHandle(hCgi);
		return 1;
	}
	return 0;
#endif

	int		nReturn;
	DWORD	exitCode;

	nReturn = GetExitCodeProcess((HANDLE)handle, &exitCode);
/*
 *	We must close process handle to free up the window resource, but only
 *  when we're done with it.
 */
	if ((nReturn == 0) || (exitCode != STILL_ACTIVE)) {
		CloseHandle((HANDLE)handle);
		return 0;
	}

	return 1;
}

/******************************************************************************/
#ifdef B_STATS
static void memLeaks() 
{
	int		fd;

	if ((fd = gopen(T("leak.txt"), O_CREAT | O_TRUNC | O_WRONLY)) >= 0) {
		bstats(fd, printMemStats);
		close(fd);
	}
}

/******************************************************************************/
/*
 *	Print memory usage / leaks  
 */

static void printMemStats(int handle, char_t *fmt, ...)
{
	va_list		args;
	char_t		buf[256];

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	write(handle, buf, strlen(buf));
}
#endif	/* B_STATS */

/******************************************************************************/
