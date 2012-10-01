/*
    gfree -- CGI processing (for the GoAhead Web server
  
    This module implements the /cgi-bin handler. CGI processing differs from
    goforms processing in that each CGI request is executed as a separate 
    process, rather than within the webserver process. For each CGI request the
    environment of the new process must be set to include all the CGI variables
    and its standard input and output must be directed to the socket.  This
    is done using temporary files.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/*********************************** Defines **********************************/

typedef struct Cgi {            /* Struct for CGI tasks which have completed */
    Webs    *wp;                /* Connection object */
    char    *stdIn;             /* File desc. for task's temp input fd */
    char    *stdOut;            /* File desc. for task's temp output fd */
    char    *cgiPath;           /* Path to executable process file */
    char    **argp;             /* Pointer to buf containing argv tokens */
    char    **envp;             /* Pointer to array of environment strings */
    int     handle;             /* Process handle of the task */
    off_t   fplacemark;         /* Seek location for CGI output file */
} Cgi;

static Cgi      **cgiList;      /* galloc chain list of wp's to be closed */
static int      cgiMax;         /* Size of galloc list */

/************************************* Code ***********************************/
/*
    Process a form request. Returns 1 always to indicate it handled the URL
 */
static bool cgiHandler(Webs *wp)
{
    Cgi         *cgip;
    WebsKey     *s;
    char        cgiBuf[BIT_LIMIT_FILENAME], *stdIn, *stdOut, cwd[BIT_LIMIT_FILENAME];
    char        *cp, *cgiName, *cgiPath, **argp, **envp, **ep, *tok, *query;
    int         n, envpsize, argpsize, pHandle, cid;

    gassert(websValid(wp));

    /*
        Extract the form name and then build the full path name.  The form name will follow the first '/' in path.
     */
    strncpy(cgiBuf, wp->path, TSZ(cgiBuf));
    if ((cgiName = strchr(&cgiBuf[1], '/')) == NULL) {
        websError(wp, 200, "Missing CGI name");
        return 1;
    }
    cgiName++;
    if ((cp = strchr(cgiName, '/')) != NULL) {
        *cp = '\0';
    }
    cgiPath = sfmt("%s/%s/%s", websGetDocuments(), BIT_CGI_BIN, cgiName);
#if !VXWORKS
    /*
        See if the file exists and is executable.  If not error out.  Don't do this step for VxWorks, since the module
        may already be part of the OS image, rather than in the file system.
    */
    {
        WebsStat sbuf;
        if (stat(cgiPath, &sbuf) != 0 || (sbuf.st_mode & S_IFREG) == 0) {
            websError(wp, 404, "CGI process file does not exist");
            gfree(cgiPath);
            return 1;
        }
#if BIT_WIN_LIKE
        if (strstr(cgiPath, ".exe") == NULL && strstr(cgiPath, ".bat") == NULL)
#else
        if (access(cgiPath, X_OK) != 0)
#endif
        {
            websError(wp, 200, "CGI process file is not executable");
            gfree(cgiPath);
            return 1;
        }
    }
#endif /* ! VXWORKS */
         
    /*
        Get the CWD for resetting after launching the child process CGI
     */
    getcwd(cwd, BIT_LIMIT_FILENAME);

    /*
        Retrieve the directory of the child process CGI
     */
    if ((cp = strrchr(cgiPath, '/')) != NULL) {
        *cp = '\0';
        chdir(cgiPath);
        *cp = '/';
    }

    /*
         Build command line arguments.  Only used if there is no non-encoded = character.  This is indicative of a ISINDEX
         query.  POST separators are & and others are +.  argp will point to a galloc'd array of pointers.  Each pointer
         will point to substring within the query string.  This array of string pointers is how the spawn or exec routines
         expect command line arguments to be passed.  Since we don't know ahead of time how many individual items there are
         in the query string, the for loop includes logic to grow the array size via grealloc.
     */
    argpsize = 10;
    argp = galloc(argpsize * sizeof(char *));
    *argp = cgiPath;
    n = 1;
    if (strchr(wp->query, '=') == NULL) {
        query = sclone(wp->query);
        websDecodeUrl(query, query, strlen(query));
        for (cp = stok(query, " ", &tok); cp != NULL; ) {
            *(argp+n) = cp;
            n++;
            if (n >= argpsize) {
                argpsize *= 2;
                argp = grealloc(argp, argpsize * sizeof(char *));
            }
            cp = stok(NULL, " ", &tok);
        }
    }
    *(argp+n) = NULL;

    /*
        Add all CGI variables to the environment strings to be passed to the spawned CGI process. This includes a few
        we don't already have in the symbol table, plus all those that are in the vars symbol table. envp will point
        to a galloc'd array of pointers. Each pointer will point to a galloc'd string containing the keyword value pair
        in the form keyword=value. Since we don't know ahead of time how many environment strings there will be the for
        loop includes logic to grow the array size via grealloc.
     */
    envpsize = 64;
    envp = galloc(envpsize * sizeof(char *));
    n = 0;
    envp[n++] = sfmt("%s=%s", "PATH_TRANSLATED", cgiPath);
    envp[n++] = sfmt("%s=%s/%s", "SCRIPT_NAME", BIT_CGI_BIN, cgiName);

    websSetEnv(wp);
    for (s = symFirst(wp->vars); s != NULL; s = symNext(wp->vars, s)) {
        if (s->content.valid && s->content.type == string &&
            strcmp(s->name.value.string, "REMOTE_HOST") != 0 &&
            strcmp(s->name.value.string, "HTTP_AUTHORIZATION") != 0) {
            envp[n++] = sfmt("%s=%s", s->name.value.string, s->content.value.string);
            if (n >= envpsize) {
                envpsize *= 2;
                envp = grealloc(envp, envpsize * sizeof(char *));
            }
        }
    }
    *(envp+n) = NULL;

    /*
        Create temporary file name(s) for the child's stdin and stdout. For POST data the stdin temp file (and name)
        should already exist.  
     */
    if (wp->cgiStdin == NULL) {
        wp->cgiStdin = websGetCgiCommName();
    } 
    stdIn = wp->cgiStdin;
    stdOut = websGetCgiCommName();
    /*
        Now launch the process.  If not successful, do the cleanup of resources.  If successful, the cleanup will be
        done after the process completes.  
     */
    if ((pHandle = websLaunchCgiProc(cgiPath, argp, envp, stdIn, stdOut)) == -1) {
        websError(wp, 200, "failed to spawn CGI task");
        for (ep = envp; *ep != NULL; ep++) {
            gfree(*ep);
        }
        gfree(cgiPath);
        gfree(argp);
        gfree(envp);
        gfree(stdOut);
        gfree(query);
    } else {
        /*
            If the spawn was successful, put this wp on a queue to be checked for completion.
         */
        cid = gallocEntry(&cgiList, &cgiMax, sizeof(Cgi));
        cgip = cgiList[cid];
        cgip->handle = pHandle;
        cgip->stdIn = stdIn;
        cgip->stdOut = stdOut;
        cgip->cgiPath = cgiPath;
        cgip->argp = argp;
        cgip->envp = envp;
        cgip->wp = wp;
        cgip->fplacemark = 0;
        websTimeoutCancel(wp);
        gfree(query);
    }
    /*
        Restore the current working directory after spawning child CGI
     */
    chdir(cwd);
    return 1;
}


int websCgiOpen()
{
    websDefineHandler("cgi", cgiHandler, 0, 0);
    return 0;
}


int websProcessCgiData(Webs *wp)
{
    ssize   nbytes;

    nbytes = bufLen(&wp->input);
    if (write(wp->cgifd, wp->input.servp, (int) nbytes) != nbytes) {
        websError(wp, WEBS_CLOSE | 500, "Can't write to CGI gateway");
        return -1;
    }
    websConsumeInput(wp, nbytes);
    return 0;
}


/*
    Any entry in the cgiList need to be checked to see if it has
 */
void websCgiGatherOutput (Cgi *cgip)
{
    Webs     *wp;
    WebsStat sbuf;
    char   cgiBuf[BIT_LIMIT_FILENAME];
    ssize    nRead;
    int      fdout;

    if ((stat(cgip->stdOut, &sbuf) == 0) && (sbuf.st_size > cgip->fplacemark)) {
        if ((fdout = open(cgip->stdOut, O_RDONLY | O_BINARY, 0444)) >= 0) {
            /*
                Check to see if any data is available in the output file and send its contents to the socket.
                Write the HTTP header on our first pass
             */
            wp = cgip->wp;
            if (cgip->fplacemark == 0) {
                websWriteHeader(wp, "HTTP/1.0 200 OK\r\n");
                websWriteHeader(wp, "Pragma: no-cache\r\nCache-Control: no-cache\r\n");
            }
            lseek(fdout, cgip->fplacemark, SEEK_SET);
            while ((nRead = read(fdout, cgiBuf, BIT_LIMIT_FILENAME)) > 0) {
                websWriteBlock(wp, cgiBuf, nRead);
                cgip->fplacemark += (off_t) nRead;
            }
            close(fdout);
        }
    }
}


/*
    Any entry in the cgiList need to be checked to see if it has completed, and if so, process its output and clean up.
 */
void websCgiCleanup()
{
    Webs    *wp;
    Cgi     *cgip;
    char  **ep;
    int     cid, nTries;

    for (cid = 0; cid < cgiMax; cid++) {
        if ((cgip = cgiList[cid]) != NULL) {
            wp = cgip->wp;
            websCgiGatherOutput (cgip);
            if (websCheckCgiProc(cgip->handle) == 0) {
                /*
                    We get here if the CGI process has terminated.  Clean up.
                 */
                nTries = 0;
                /*              
                     Make sure we didn't miss something during a task switch.  Maximum wait is 100 times 10 msecs (1
                     second).  
                 */
                while ((cgip->fplacemark == 0) && (nTries < 100)) {
                    websCgiGatherOutput(cgip);
                    /*                  
                         There are some cases when we detect app exit before the file is ready. 
                     */
                    if (cgip->fplacemark == 0) {
#if WINDOWS
                        //  MOB - refactor
                        Sleep(10);
#endif
                    }
                    nTries++;
                }
                if (cgip->fplacemark == 0) {
                    websError(wp, 200, "CGI generated no output");
                } else {
                    websDone(wp, 200);
                }
                /*
                    Remove the temporary re-direction files
                 */
                unlink(cgip->stdIn);
                unlink(cgip->stdOut);
                /*
                    Free all the memory buffers pointed to by cgip. The stdin file name (wp->cgiStdin) gets freed as
                    part of websFree().
                 */
                cgiMax = gfreeHandle(&cgiList, cid);
                for (ep = cgip->envp; ep != NULL && *ep != NULL; ep++) {
                    gfree(*ep);
                }
                gfree(cgip->cgiPath);
                gfree(cgip->argp);
                gfree(cgip->envp);
                gfree(cgip->stdOut);
                gfree(cgip);
            }
        }
    }
}


/*  
    PLATFORM IMPLEMENTATIONS FOR CGI HELPERS
        websGetCgiCommName, websLaunchCgiProc, websCheckCgiProc
*/

#if CE
/*
     Returns a pointer to an allocated qualified unique temporary file name.
     This filename must eventually be deleted with gfree().
 */
char *websGetCgiCommName()
{
    /*
         tmpnam, tempnam, tmpfile not supported for CE 2.12 or lower.  The Win32 API
         GetTempFileName is scheduled to be part of CE 3.0.
     */
    //  MOB
#if 0  
    char  *pname1, *pname2;
    pname1 = gtempnam(NULL, "cgi");
    pname2 = strdup(pname1);
    free(pname1);
    return pname2;
#endif
    return NULL;
}


/*
     Launch the CGI process and return a handle to it.  CE note: This function is not complete.  The missing piece is
     the ability to redirect stdout.
 */
int websLaunchCgiProc(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
{
    PROCESS_INFORMATION procinfo;       /*  Information about created proc   */
    DWORD               dwCreateFlags;
    char                *fulldir;
    BOOL                bReturn;
    int                 i, nLen;

    /*
        Replace directory delimiters with Windows-friendly delimiters
     */
    nLen = strlen(cgiPath);
    for (i = 0; i < nLen; i++) {
        if (cgiPath[i] == '/') {
            cgiPath[i] = '\\';
        }
    }
    fulldir = NULL;
    dwCreateFlags = CREATE_NEW_CONSOLE;

    /*
         CreateProcess returns errors sometimes, even when the process was started correctly.  The cause is not evident.
         For now: we detect an error by checking the value of procinfo.hProcess after the call.
     */
    procinfo.hThread = NULL;
    bReturn = CreateProcess(
        cgiPath,            /*  Name of executable module        */
        NULL,               /*  Command line string              */
        NULL,               /*  Process security attributes      */
        NULL,               /*  Thread security attributes       */
        0,                  /*  Handle inheritance flag          */
        dwCreateFlags,      /*  Creation flags                   */
        NULL,               /*  New environment block            */
        NULL,               /*  Current directory name           */
        NULL,               /*  STARTUPINFO                      */
        &procinfo);         /*  PROCESS_INFORMATION              */
    if (bReturn == 0) {
        return -1;
    } else {
        CloseHandle(procinfo.hThread);
    }
    return (int) procinfo.dwProcessId;
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
int websCheckCgiProc(int handle)
{
    int     nReturn;
    DWORD   exitCode;

    nReturn = GetExitCodeProcess((HANDLE)handle, &exitCode);
    /*
        We must close process handle to free up the window resource, but only when we're done with it.
     */
    if ((nReturn == 0) || (exitCode != STILL_ACTIVE)) {
        CloseHandle((HANDLE)handle);
        return 0;
    }
    return 1;
}
#endif /* CE */

#if BIT_UNIX_LIKE || QNX
/*
     Returns a pointer to an allocated qualified unique temporary file name. This filename must eventually be deleted
     with gfree(); 
 */
char *websGetCgiCommName()
{
    char  *pname1, *pname2;

    pname1 = tempnam(NULL, "cgi");
    pname2 = strdup(pname1);
    free(pname1);
    return pname2;
}


/*
    Launch the CGI process and return a handle to it.
 */
int websLaunchCgiProc(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
{
    int pid, fdin, fdout, hstdin, hstdout, rc;

    fdin = fdout = hstdin = hstdout = rc = -1;
    if ((fdin = open(stdIn, O_RDWR | O_CREAT, 0666)) < 0 ||
            (fdout = open(stdOut, O_RDWR | O_CREAT, 0666)) < 0 ||
            (hstdin = dup(0)) == -1 || (hstdout = dup(1)) == -1 ||
            dup2(fdin, 0) == -1 || dup2(fdout, 1) == -1) {
        goto DONE;
    }
    rc = pid = fork();
    if (pid == 0) {
        /*
            if pid == 0, then we are in the child process
         */
        if (execve(cgiPath, argp, envp) == -1) {
            printf("content-type: text/html\n\nExecution of cgi process failed\n");
        }
        exit (0);
    }
DONE:
    if (hstdout >= 0) {
        dup2(hstdout, 1);
        close(hstdout);
    }
    if (hstdin >= 0) {
        dup2(hstdin, 0);
        close(hstdin);
    }
    if (fdout >= 0) {
        close(fdout);
    }
    if (fdin >= 0) {
        close(fdin);
    }
    return rc;
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
int websCheckCgiProc(int handle)
{
    /*
        Check to see if the CGI child process has terminated or not yet.
     */
    if (waitpid(handle, NULL, WNOHANG) == handle) {
        return 0;
    } else {
        return 1;
    }
}

#endif /* LINUX || LYNX || MACOSX || QNX4 */


#if VXWORKS
static void vxWebsCgiEntry(void *entryAddr(int argc, char **argv), char **argv, char **envp, char *stdIn, char
        *stdOut); 
/*
     Returns a pointer to an allocated qualified unique temporary file name.
     This filename must eventually be deleted with gfree();
 */
char *websGetCgiCommName()
{
    char  *tname, buf[BIT_LIMIT_FILENAME];

    tname = sfmt("%s/%s", getcwd(buf, BIT_LIMIT_FILENAME), tmpnam(NULL));
    return tname;
}

/******************************************************************************/
/*
    Launch the CGI process and return a handle to it. Process spawning is not supported in VxWorks.  Instead, we spawn a
    "task".  A major difference is that we have to know the entry point for the taskSpawn API.  Also the module may have
    to be loaded before being executed; it may also be part of the OS image, in which case it cannot be loaded or
    unloaded.  
    The following sequence is used:
    1. If the module is already loaded, unload it from memory.
    2. Search for a query string keyword=value pair in the environment variables where the keyword is cgientry.  If
        found use its value as the the entry point name.  If there is no such pair set the entry point name to the
        default: basename_cgientry, where basename is the name of the cgi file without the extension.  Use the entry
        point name in a symbol table search for that name to use as the entry point address.  If successful go to step 5.
    3. Try to load the module into memory.  If not successful error out.
    4. If step 3 is successful repeat the entry point search from step 2. If the entry point exists, go to step 5.  If
        it does not, error out.
    5. Use taskSpawn to start a new task which uses vxWebsCgiEntry as its starting point. The five arguments to
        vxWebsCgiEntry will be the user entry point address, argp, envp, stdIn and stdOut.  vxWebsCgiEntry will convert
        argp to an argc argv pair to pass to the user entry, it will initialize the task environment with envp, it will
        open and redirect stdin and stdout to stdIn and stdOut, and then it will call the user entry.
    6.  Return the taskSpawn return value.
 */
int websLaunchCgiProc(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
{
    SYM_TYPE    ptype;
    char      *p, *basename, *pEntry, *pname, *entryAddr, **pp;
    int         priority, rc, fd;

    /*
        Determine the basename, which is without path or the extension.
     */
    if ((int)(p = strrchr(cgiPath, '/') + 1) == 1) {
        p = cgiPath;
    }
    basename = strdup(p);
    if ((p = strrchr(basename, '.')) != NULL) {
        *p = '\0';
    }
    /*
        Unload the module, if it is already loaded.  Get the current task priority.
     */
    unld(cgiPath, 0);
    taskPriorityGet(taskIdSelf(), &priority);
    rc = fd = -1;

    /*
         Set the entry point symbol name as described above.  Look for an already loaded entry point; if it exists, spawn
         the task accordingly.  
     */
    for (pp = envp, pEntry = NULL; pp != NULL && *pp != NULL; pp++) {
        if (strncmp(*pp, "cgientry=", 9) == 0) {
            pEntry = strdup(*pp + 9);
            break;
        }
    }
    if (pEntry == NULL) {
        pEntry = sfmt("%s_%s", basename, "cgientry");
    }
    entryAddr = 0;
    if (symFindByName(sysSymTbl, pEntry, &entryAddr, &ptype) == -1) {
        pname = sfmt("_%s", pEntry);
        symFindByName(sysSymTbl, pname, &entryAddr, &ptype);
        gfree(pname);
    }
    if (entryAddr != 0) {
        rc = taskSpawn(pEntry, priority, 0, 20000, (void*) vxWebsCgiEntry, (int) entryAddr, (int) argp, 
            (int) envp, (int) stdIn, (int) stdOut, 0, 0, 0, 0, 0);
        goto DONE;
    }

    /*
        Try to load the module.
     */
    if ((fd = open(cgiPath, O_RDONLY | O_BINARY, 0666)) < 0 ||
        loadModule(fd, LOAD_GLOBAL_SYMBOLS) == NULL) {
        goto DONE;
    }
    if ((symFindByName(sysSymTbl, pEntry, &entryAddr, &ptype)) == -1) {
        pname = sfmt("_%s", pEntry);
        symFindByName(sysSymTbl, pname, &entryAddr, &ptype);
        gfree(pname);
    }
    if (entryAddr != 0) {
        rc = taskSpawn(pEntry, priority, 0, 20000, (void*) vxWebsCgiEntry, (int) entryAddr, (int) argp, 
            (int) envp, (int) stdIn, (int) stdOut, 0, 0, 0, 0, 0);
    }
DONE:
    if (fd != -1) {
        close(fd);
    }
    gfree(basename);
    gfree(pEntry);
    return rc;
}


/*
    This is the CGI process wrapper.  It will open and redirect stdin and stdout to stdIn and stdOut.  It converts argv
    to an argc, argv pair to pass to the user entry. It initializes the task environment with envp strings. Then it
    will call the user entry.
 */
static void vxWebsCgiEntry(void *entryAddr(int argc, char **argv), char **argp, char **envp, char *stdIn, char *stdOut)
{
    char  **p;
    int     argc, taskId, fdin, fdout;

    /*
        Open the stdIn and stdOut files and redirect stdin and stdout to them.
     */
    taskId = taskIdSelf();
    if ((fdout = open(stdOut, O_RDWR | O_CREAT, 0666)) < 0 &&
            (fdout = creat(stdOut, O_RDWR)) < 0) {
        exit(0);
    }
    ioTaskStdSet(taskId, 1, fdout);

    if ((fdin = open(stdIn, O_RDONLY | O_CREAT, 0666)) < 0 && (fdin = creat(stdIn, O_RDWR)) < 0) {
        printf("content-type: text/html\n\n" "Can not create CGI stdin to %s\n", stdIn);
        close(fdout);
        exit (0);
    }
    ioTaskStdSet(taskId, 0, fdin);

    /*
        Count the number of entries in argv
     */
    for (argc = 0, p = argp; p != NULL && *p != NULL; p++, argc++) { }

    /*
        Create a private envirnonment and copy the envp strings to it.
     */
    if (envPrivateCreate(taskId, -1) != OK) {
        printf("content-type: text/html\n\n" "Can not create CGI environment space\n");
        close(fdin);
        close(fdout);
        exit (0);
    }
    for (p = envp; p != NULL && *p != NULL; p++) {
        putenv(*p);
    }

    /*
        Call the user entry.
     */
    (*entryAddr)(argc, argp);

    /*
        The user code should return here for cleanup.
     */
    envPrivateDestroy(taskId);
    close(fdin);
    close(fdout);
    exit(0);
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
int websCheckCgiProc(int handle)
{
    STATUS stat;

    /*
        Verify the existence of a VxWorks task
     */
    stat = taskIdVerify(handle);
    if (stat == OK) {
        return 1;
    } else {
        return 0;
    }
}
#endif /* VXWORKS */

#if WINDOWS 
/*
    Convert a table of strings into a single block of memory. The input table consists of an array of null-terminated
    strings, terminated in a null pointer.  Returns the address of a block of memory allocated using the galloc()
    function.  The returned pointer must be deleted using gfree().  Returns NULL on error.
 */
static uchar *tableToBlock(char **table)
{
    uchar   *pBlock;        /*  Allocated block */
    char    *pEntry;        /*  Pointer into block */
    size_t  sizeBlock;      /*  Size of table */
    int     index;          /*  Index into string table */

    gassert(table);

    /*  
        Calculate the size of the data block.  Allow for final null byte. 
     */
    sizeBlock = 1;                    
    for (index = 0; table[index]; index++) {
        sizeBlock += strlen(table[index]) + 1;
    }
    /*
        Allocate the data block and fill it with the strings                   
     */
    pBlock = galloc(sizeBlock);

    if (pBlock != NULL) {
        pEntry = (char*) pBlock;
        for (index = 0; table[index]; index++) {
            strcpy(pEntry, table[index]);
            pEntry += strlen(pEntry) + 1;
        }
        /*      
            Terminate the data block with an extra null string                
         */
        *pEntry = '\0';              
    }
    return pBlock;
}


/*
    Returns a pointer to an allocated qualified unique temporary file name. This filename must eventually be deleted
    with gfree().  
 */
char *websGetCgiCommName()
{
    char  *pname1, *pname2;

    pname1 = tempnam(NULL, "cgi");
    pname2 = strdup(pname1);
    free(pname1);
    return pname2;
}



/*
    Create a temporary stdout file and launch the CGI process. Returns a handle to the spawned CGI process.
 */
int websLaunchCgiProc(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
{
    STARTUPINFO         newinfo;
    SECURITY_ATTRIBUTES security;
    PROCESS_INFORMATION procinfo;       /*  Information about created proc   */
    DWORD               dwCreateFlags;
    char              *cmdLine, **pArgs;
    BOOL                bReturn;
    ssize               nLen;
    int                 i;
    uchar               *pEnvData;

    /*
        Replace directory delimiters with Windows-friendly delimiters
     */
    nLen = strlen(cgiPath);
    for (i = 0; i < nLen; i++) {
        if (cgiPath[i] == '/') {
            cgiPath[i] = '\\';
        }
    }
    /*
        Calculate length of command line
     */
    nLen = 0;
    pArgs = argp;
    while (pArgs && *pArgs && **pArgs) {
        nLen += strlen(*pArgs) + 1;
        pArgs++;
    }

    /*
        Construct command line
     */
    cmdLine = galloc(sizeof(char) * nLen);
    gassert (cmdLine);
    strcpy(cmdLine, "");

    pArgs = argp;
    while (pArgs && *pArgs && **pArgs) {
        strcat(cmdLine, *pArgs);
        strcat(cmdLine, " ");
        pArgs++;
    }

    /*
        Create the process start-up information
     */
    memset (&newinfo, 0, sizeof(newinfo));
    newinfo.cb          = sizeof(newinfo);
    newinfo.dwFlags     = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    newinfo.wShowWindow = SW_HIDE;
    newinfo.lpTitle     = NULL;

    /*
        Create file handles for the spawned processes stdin and stdout files
     */
    security.nLength = sizeof(SECURITY_ATTRIBUTES);
    security.lpSecurityDescriptor = NULL;
    security.bInheritHandle = TRUE;

    /*
        Stdin file should already exist.
     */
    newinfo.hStdInput = CreateFile(stdIn, GENERIC_READ, FILE_SHARE_READ, &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
            NULL); 
    /*
        Stdout file is created and file pointer is reset to start.
     */
    newinfo.hStdOutput = CreateFile(stdOut, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ + FILE_SHARE_WRITE, 
            &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    SetFilePointer (newinfo.hStdOutput, 0, NULL, FILE_END);
    newinfo.hStdError = newinfo.hStdOutput;

    dwCreateFlags = CREATE_NEW_CONSOLE;
    pEnvData = tableToBlock(envp);

    /*
        CreateProcess returns errors sometimes, even when the process was started correctly.  The cause is not evident.
        For now: we detect an error by checking the value of procinfo.hProcess after the call.
    */
    procinfo.hProcess = NULL;
    bReturn = CreateProcess(
        NULL,               /*  Name of executable module        */
        cmdLine,            /*  Command line string              */
        NULL,               /*  Process security attributes      */
        NULL,               /*  Thread security attributes       */
        TRUE,               /*  Handle inheritance flag          */
        dwCreateFlags,      /*  Creation flags                   */
        pEnvData,           /*  New environment block            */
        NULL,               /*  Current directory name           */
        &newinfo,           /*  STARTUPINFO                      */
        &procinfo);         /*  PROCESS_INFORMATION              */

    if (procinfo.hThread != NULL)  {
        CloseHandle(procinfo.hThread);
    }
    if (newinfo.hStdInput) {
        CloseHandle(newinfo.hStdInput);
    }
    if (newinfo.hStdOutput) {
        CloseHandle(newinfo.hStdOutput);
    }
    gfree(pEnvData);
    gfree(cmdLine);

    if (bReturn == 0) {
        return -1;
    } else {
        return (int) procinfo.hProcess;
    }
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
int websCheckCgiProc(int handle)
{
    int     nReturn;
    DWORD   exitCode;

    nReturn = GetExitCodeProcess((HANDLE)handle, &exitCode);
    /*
        We must close process handle to free up the window resource, but only
        when we're done with it.
     */
    if ((nReturn == 0) || (exitCode != STILL_ACTIVE)) {
        CloseHandle((HANDLE)handle);
        return 0;
    }
    return 1;
}
#endif /* WIN */

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
