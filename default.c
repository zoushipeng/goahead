/*
    default.c -- Default URL handler. Includes support for ASP.
  
    This module provides default URL handling and Active Server Page support.  Validate the incoming URL request by both
    standard path rules, as well as a very tight whitelist that is automatically built based on the files under the
    websDefaultDir directory. In many cases we don't check the return code of calls to websWrite as it is easier,
    smaller and non-fatal to continue even when the requesting browser has gone away.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static char_t   *websDefaultPage;           /* Default page name */
static char_t   *websDefaultDir;            /* Default Web page directory */

/**************************** Forward Declarations ****************************/
#define MAX_URL_DEPTH           8   /* Max directory depth of websDefaultDir */

#if BIT_WHITELIST

#ifndef WIN
#include <dirent.h>
#endif

//  MOB - move to goahead.h
#define WHITELIST_BLOCKED   0x100   /* File is in list, but inaccessible */
#define WHITELIST_DIR       0x200   /* Node is a Directory */

#if BIT_PACK_SSL
typedef struct _sslList {
    struct _sslList *next;
    char            *url;
} sslList_t;

static sslList_t    *sslList;
#endif

typedef struct _fileNode {
    struct _fileNode *next;     /* Next file/dir on same level (NULL if last) */
    struct _fileNode *child;    /* Non-NULL if this is a directory with child */
    char    *name;              /* path segment with trailing '/' or filename */
    int     flags;              /* Flags pertaining to this list entry */
} fileNode_t;

static fileNode_t   *whitelist = NULL; /* Global tree of files in list */

static int websBuildWhitelistRecursive(char *_path, fileNode_t *dir, int level);
static fileNode_t* websWhitelistCheckRecursive(fileNode_t *n, char *path);
static void websDeleteWhitelistRecursive(fileNode_t *dir);
static void websMakePath(char *d, char *src, char *subdir, int wildcard);
#else
#define websBuildWhitelist()
#endif /* WEBS_WHITELIST_SUPPORT */

static void websDefaultWriteEvent(webs_t wp);

/*********************************** Code *************************************/
/*
    Process a default URL request. This will validate the URL and handle "../" and will provide support for Active
    Server Pages. As the handler is the last handler to run, it always indicates that it has handled the URL by
    returning 1. 
 */

int websDefaultHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
    websStatType    sbuf;
    char_t          *lpath, *tmp, *date;
    int             bytes, flags, nchars, rc;

    a_assert(websValid(wp));
    a_assert(url && *url);
    a_assert(path);
    a_assert(query);

    flags = websGetRequestFlags(wp);

    /*
        We do whitelist validation in addition to standard URL validation.  The whitelist should really catch anything
        invalid first.  If the whitelist check fails, rebuild the list and try again.  Also validate if we are not on a
        secure connection, but the whitelist entry has the SSL flag set, do not serve the page.
     */
#if BIT_WHITELIST
    if ((rc = websWhitelistCheck(wp->url)) < 0) {
        websBuildWhitelist();
        if ((rc = websWhitelistCheck(wp->url)) < 0) {
            websError(wp, 404, T("Cannot open URL"));
            return 1;
        }
    }
    if (!(flags & WEBS_SECURE) && (rc & WHITELIST_SSL)) {
        websError(wp, 500, T("HTTPS access required"));
        return 1;
    }
#endif /* WEBS_WHITELIST_SUPPORT */
    /*
        Validate the URL and ensure that ".."s don't give access to unwanted files
     */
    if (websValidateUrl(wp, path) < 0) {
        /* 
            preventing a cross-site scripting exploit -- you may restore the following line of code to revert to the
            original behavior...  websError(wp, 500, T("Invalid URL %s"), url);
        */
        websError(wp, 500, T("Invalid URL"));
        websBuildWhitelist();
        return 1;
    }
    lpath = websGetRequestLpath(wp);
    nchars = gstrlen(lpath) - 1;
    if (lpath[nchars] == '/' || lpath[nchars] == '\\') {
        lpath[nchars] = '\0';
    }

    /*
        If the file is a directory, redirect using the nominated default page
     */
    if (websPageIsDirectory(lpath)) {
        nchars = gstrlen(path);
        if (path[nchars-1] == '/' || path[nchars-1] == '\\') {
            path[--nchars] = '\0';
        }
        nchars += gstrlen(websDefaultPage) + 2;
        fmtAlloc(&tmp, nchars, T("%s/%s"), path, websDefaultPage);
        websRedirect(wp, tmp);
        bfreeSafe(tmp);
        return 1;
    }

    /*
        Open the document. Stat for later use.
     */
    if (websPageOpen(wp, lpath, path, O_RDONLY | O_BINARY, 0666) < 0) {
        websError(wp, 404, T("Cannot open URL"));
        websBuildWhitelist();
        return 1;
    } 

    if (websPageStat(wp, lpath, path, &sbuf) < 0) {
        websError(wp, 400, T("Cannot stat page for URL"));
        websBuildWhitelist();
        return 1;
    }

    /*
        If the page has not been modified since the user last received it and it is not dynamically generated each time
        (ASP), then optimize request by sending a 304 Use local copy response.
     */
    websStats.localHits++;
#if BIT_IF_MODIFIED
    if (flags & WEBS_IF_MODIFIED && !(flags & WEBS_ASP)) {
        if (sbuf.mtime <= wp->since) {
            websWrite(wp, T("HTTP/1.0 304 Use local copy\r\n"));
            /*
                NOTE: by license terms the following line of code must not be modified.
                MOB: remove define
             */
            websWrite(wp, T("Server: GoAhead/%s\r\n"), BIT_VERSION);
            if (flags & WEBS_KEEP_ALIVE) {
                websWrite(wp, T("Connection: keep-alive\r\n"));
            }
            websWrite(wp, T("\r\n"));
            websSetRequestFlags(wp, flags |= WEBS_HEADER_DONE);
            websDone(wp, 304);
            return 1;
        }
    }
#endif

    /*
        Output the normal HTTP response header
     */
    if ((date = websGetDateString(NULL)) != NULL) {
        websWrite(wp, T("HTTP/1.0 200 OK\r\nDate: %s\r\n"), date);
        /*
            The Server HTTP header below must not be modified unless explicitly allowed by licensing terms.
         */
        websWrite(wp, T("Server: GoAhead/%s\r\n"), BIT_VERSION);
        bfree(date);
    }
    flags |= WEBS_HEADER_DONE;

    /*
        If this is an ASP request, ensure the remote browser doesn't cache it.
        Send back both HTTP/1.0 and HTTP/1.1 cache control directives
     */
    if (flags & WEBS_ASP) {
        bytes = 0;
        websWrite(wp, T("Pragma: no-cache\r\nCache-Control: no-cache\r\n"));

    } else {
        if ((date = websGetDateString(&sbuf)) != NULL) {
            websWrite(wp, T("Last-modified: %s\r\n"), date);
            bfree(date);
        }
        bytes = sbuf.size;
    }
    if (bytes) {
        websWrite(wp, T("Content-length: %d\r\n"), bytes);
        websSetRequestBytes(wp, bytes);
    }
    websWrite(wp, T("Content-type: %s\r\n"), websGetRequestType(wp));

    if ((flags & WEBS_KEEP_ALIVE) && !(flags & WEBS_ASP)) {
        websWrite(wp, T("Connection: keep-alive\r\n"));
    }
    websWrite(wp, T("\r\n"));

    /*
        All done if the browser did a HEAD request
     */
    if (flags & WEBS_HEAD_REQUEST) {
        websDone(wp, 200);
        return 1;
    }
#if BIT_JAVASCRIPT
    /*
        Evaluate ASP requests
     */
    if (flags & WEBS_ASP) {
        if (websAspRequest(wp, lpath) < 0) {
            return 1;
        }
        websDone(wp, 200);
        return 1;
    }
#endif
    /*
        Return the data via background write
     */
    websSetRequestSocketHandler(wp, SOCKET_WRITABLE, websDefaultWriteEvent);
    return 1;
}


//  MOB - change define
#if WIN32
static int badPath(char_t* path, char_t* badPath, int badLen)
{
   int retval = 0;
   int len = gstrlen(path);
   int i = 0;

   if (len <= badLen + 1) {
      for (i = 0; i < badLen; ++i) {
         if (badPath[i] != gtolower(path[i])) {
            return 0;
         }
      }
      /* 
            If we get here, the first 'badLen' characters match.  If 'path' is 1 character larger than 'badPath' and
            that extra character is NOT a letter or a number, we have a bad path.
       */
      retval = 1;
      if (badLen + 1 == len) {
         /* e.g. path == "aux:" */
         if (gisalnum(path[len-1])) {
            /* 
                the last character is alphanumeric, so we let this path go through. 
             */
            retval = 0;
         }
      }
   }
   return retval;
}


static int isBadWindowsPath(char_t** parts, int partCount)
{
   /*
        If we're running on Windows 95/98/ME, malicious users can crash the OS by requesting an URL with any of several
        reserved DOS device names in them (AUX, NUL, etc.).  If we're running on any of those OS versions, we scan the
        URL for paths with any of these elements before trying to access them. If any of the subdirectory names match
        one of our prohibited links, we declare this to be a 'bad' path, and return 1 to indicate this. This may be a
        heavy-handed approach, but should prevent the DOS attack.  NOTE that this function is only compiled in when we
        are running on Win32, and only has an effect when running on Win95/98, or ME. On all other versions of Windows,
        we check the version info, and return 0 immediately.
     
        According to http://packetstormsecurity.nl/0003-exploits/SCX-SA-01.txt: II.  Problem Description When the
        Microsoft Windows operating system is parsing a path that is being crafted like "c:\[device]\[device]" it will
        halt, and crash the entire operating system.  Four device drivers have been found to crash the system.  The CON,
        NUL, AUX, CLOCK$ and CONFIG$ are the two device drivers which are known to crash.  Other devices as LPT[x]:,
        COM[x]: and PRN have not been found to crash the system.  Making combinations as CON\NUL, NUL\CON, AUX\NUL, ...
        seems to crash Ms Windows as well.  Calling a path such as "C:\CON\[filename]" won't result in a crash but in an
        error-message.  Creating the map "CON", "CLOCK$", "AUX" "NUL" or "CONFIG$" will also result in a simple
        error-message saying: ''creating that map isn't allowed''.
     
        returns 1 if it finds a bad path element.
    */
   OSVERSIONINFO version;
   int i;
   version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
   if (GetVersionEx(&version)) {
      if (VER_PLATFORM_WIN32_NT != version.dwPlatformId) {
         /*
          * we are currently running on 95/98/ME.
          */
         for (i = 0; i < partCount; ++i) {
            /*
             * check against the prohibited names. If any of our requested 
             * subdirectories match any of these, return '1' immediately.
             */
            if ( 
             (badPath(parts[i], T("con"), 3)) ||
             (badPath(parts[i], T("com"), 3)) ||
             (badPath(parts[i], T("nul"), 3)) ||
             (badPath(parts[i], T("aux"), 3)) ||
             (badPath(parts[i], T("clock$"), 6)) ||
             (badPath(parts[i], T("config$"), 7)) ) {
                return 1;
            }
         }
      }
   }

   /*
        either we're not on one of the bad OS versions, or the request has no problems.
    */
   return 0;
}
#endif


/*
    Validate the URL path and process ".." path segments. Return -1 if the URL is bad.
 */
int websValidateUrl(webs_t wp, char_t *path)
{
    char_t  *parts[MAX_URL_DEPTH];  /* Array of ptr's to URL parts */
    char_t  *token, *dir, *lpath; 
    int       i, len, npart;

    a_assert(websValid(wp));
    a_assert(path);

    dir = websGetRequestDir(wp);
    if (dir == NULL || *dir == '\0') {
        return -1;
    }
    /*
        Copy the string so we don't destroy the original
     */
    path = bstrdup(path);
    websDecodeUrl(path, path, gstrlen(path));
    len = npart = 0;
    parts[0] = NULL;
    token = gstrchr(path, '\\');
    while (token != NULL) {
        *token = '/';
        token = gstrchr(token, '\\');
    }
    token = gstrtok(path, T("/"));

    /*
        Look at each directory segment and process "." and ".." segments. Don't allow the browser to pop outside the
        root web
    */
    while (token != NULL) {
        if (npart >= MAX_URL_DEPTH) {
             /*
              * malformed URL -- too many parts for us to process.
              */
             bfree(path);
             return -1;
        }
        if (gstrcmp(token, T("..")) == 0) {
            if (npart > 0) {
                npart--;
            }
        } else if (gstrcmp(token, T(".")) != 0) {
            parts[npart] = token;
            len += gstrlen(token) + 1;
            npart++;
        }
        token = gstrtok(NULL, T("/"));
    }

#if WIN32
   if (isBadWindowsPath(parts, npart)) {
      bfree(path);
      return -1;
   }

#endif

    /*
        Create local path for document. Need extra space all "/" and null.
     */
    if (npart || (gstrcmp(path, T("/")) == 0) || (path[0] == '\0')) {
        lpath = balloc((gstrlen(dir) + 1 + len + 1) * sizeof(char_t));
        gstrcpy(lpath, dir);

        for (i = 0; i < npart; i++) {
            gstrcat(lpath, T("/"));
            gstrcat(lpath, parts[i]);
        }
        websSetRequestLpath(wp, lpath);
        bfree(path);
        bfree(lpath);
    } else {
        bfree(path);
        return -1;
    }
    return 0;
}


/*
    Do output back to the browser in the background. This is a socket write handler.
 */
static void websDefaultWriteEvent(webs_t wp)
{
    int     len, wrote, flags, bytes, written;
    char    *buf;

    a_assert(websValid(wp));

    flags = websGetRequestFlags(wp);
    websSetTimeMark(wp);
    wrote = bytes = 0;
    written = websGetRequestWritten(wp);

    /*
        We only do this for non-ASP documents
     */
    if ( !(flags & WEBS_ASP)) {
        bytes = websGetRequestBytes(wp);
        /*
            Note: websWriteDataNonBlock may return less than we wanted. It will return -1 on a socket error
         */
        if ((buf = balloc(PAGE_READ_BUFSIZE)) == NULL) {
            websError(wp, 200, T("Can't get memory"));
        } else {
            while ((len = websPageReadData(wp, buf, PAGE_READ_BUFSIZE)) > 0) {
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
                a_assert(written >= bytes);
                written = bytes;
            }
            bfree(buf);
        }
    }
    /*
        We're done if an error, or all bytes output
     */
    websSetRequestWritten(wp, written);
    if (wrote < 0 || written >= bytes) {
        websDone(wp, 200);
    }
}


/* 
    Initialize variables and data for the default URL handler module
 */

void websDefaultOpen()
{
#if BIT_WHITELIST
#if BIT_PACK_SSL
    sslList = NULL;
#endif
    whitelist = NULL;
    websBuildWhitelist();
#endif /* WEBS_WHITELIST_SUPPORT */
}


/* 
    Closing down. Free resources.
 */
void websDefaultClose()
{
#if BIT_WHITELIST
#if BIT_PACK_SSL
    sslList_t   *l;
    while (sslList != NULL) {
        l = sslList;
        sslList = sslList->next;
        bfreeSafe(l->url);
        bfree(l);
    }   
#endif
    websDeleteWhitelist();
#endif /* WEBS_WHITELIST_SUPPORT */
    if (websDefaultPage) {
        bfree(websDefaultPage);
        websDefaultPage = NULL;
    }
    if (websDefaultDir) {
        bfree(websDefaultDir);
        websDefaultDir = NULL;
    }
}


/*
    Get the default page for URL requests ending in "/"
 */
char_t *websGetDefaultPage()
{
    return websDefaultPage;
}


char_t *websGetDefaultDir()
{
    return websDefaultDir;
}


/*
    Set the default page for URL requests ending in "/"
 */
void websSetDefaultPage(char_t *page)
{
    a_assert(page && *page);

    if (websDefaultPage) {
        bfree(websDefaultPage);
    }
    websDefaultPage = bstrdup(page);
}


/*
    Set the default web directory
 */
void websSetDefaultDir(char_t *dir)
{
    a_assert(dir && *dir);
    if (websDefaultDir) {
        bfree(websDefaultDir);
    }
    websDefaultDir = bstrdup(dir);
}

#if BIT_WHITELIST
/******************************************************************************/
/*
    MOB - this should be a dev time utility
    Build a whitelist by recursing the root www directory. Matches are NOT case sensitive (MOB - should be on some
    platforms). Returns < 0 if not found, else returns whitelist flags 
 */
int websWhitelistCheck(char *url)
{
    fileNode_t  *n;
    if ((n = websWhitelistCheckRecursive(whitelist, url)) == NULL) {
        return -1;
    }
    return n->flags;
}


static fileNode_t* websWhitelistCheckRecursive(fileNode_t *n, char *path)
{
    char    *c, *p;

    if (n == NULL) return NULL;
    if ((n->flags & WHITELIST_BLOCKED) != 0) {
        return websWhitelistCheckRecursive(n->next, path);
    }
    if (n->name) {
        for (c = n->name, p = path; *c != '\0' && *p != '\0'; c++, p++) {
            if (tolower(*c) != tolower(*p)) {
                break;  /* not a match */
            }
        }
    }
    if (*p == '\0' && (*c == '\0' || (*c == '/' && *(c+1) == '\0'))) {
        return n;   /* We've matched to the end of the path */
    }
    if (*c == '\0' && (n->flags & WHITELIST_DIR)) {
        return websWhitelistCheckRecursive(n->child, p);
    }
    return websWhitelistCheckRecursive(n->next, path);
}

/*
    Add a DIRECTORY url to the ssl-only urls list, and rebuild the whitelist
    This API can be called multiple times, but should only be called
    AFTER websOpenServer()
    URL should contain trailing '/', eg. "/matrixssl/"
 */
#if BIT_PACK_SSL
int websRequireSSL(char *url)
{
    sslList_t   *l;
    int         sz;
    
    l = balloc(sizeof(sslList_t));
    memset(l, 0x0, sizeof(sslList_t));
    sz = strlen(url) + 1;
    if (url[sz - 2] == '/') {
        l->url = bstrdup(url);
    } else {
        sz++;
        l->url = balloc(sz);
        memcpy(l->url, url, sz - 1);
        l->url[sz - 2] = '/';
        l->url[sz - 1] = '\0';
    }
    l->next = sslList;
    sslList = l;
    websBuildWhitelist();
    return 0;
}
#endif /* BIT_PACK_SSL */

/*
    Free all allocated memory in whitelist
 */
void websDeleteWhitelist(void)
{
    websDeleteWhitelistRecursive(whitelist);
    whitelist = NULL;
}


static void websDeleteWhitelistRecursive(fileNode_t *dir)
{
    if (dir == NULL) return;
    if (dir->child) {   /* Depth first recursion to delete leaves */
        websDeleteWhitelistRecursive(dir->child);
    }
    if (dir->next) {
        websDeleteWhitelistRecursive(dir->next);
    }
    if (dir->name) {
        bfree(dir->name);
    }
    memset(dir, 0x0, sizeof(fileNode_t));
    bfree(dir);
}


/*
    Build or rebuild whitelist
 */
int websBuildWhitelist(void)
{
    if (websDefaultDir == NULL || *websDefaultDir == '\0') {
        return -1;
    }
    websDeleteWhitelistRecursive(whitelist);
    whitelist = balloc(sizeof(fileNode_t));
    memset(whitelist, 0x0, sizeof(fileNode_t));
    whitelist->name = bstrdup("/");
    whitelist->flags |= WHITELIST_DIR;
    return websBuildWhitelistRecursive(websDefaultDir, whitelist, 0);
}


/*
    WINDOWS: Build whitelist
 */
#if WIN32
static int websBuildWhitelistRecursive(char *_path, fileNode_t *dir, int level)
{
    WIN32_FIND_DATA findData;
    HANDLE  fh;
    fileNode_t  *cnode, *nnode; /* current node, next node */
#if BIT_PACK_SSL
    sslList_t   *l;
#endif
    int     rc = 0;
    int     firstTime;
    char    path[MAX_PATH];
    char    tmppath[MAX_PATH];

    firstTime = 1;
    cnode = dir;
    websMakePath(path, _path, NULL, 1);
    fh = FindFirstFile((LPCSTR)path, &findData);
    while (fh != INVALID_HANDLE_VALUE) {
        if ((strcmp(findData.cFileName, ".") == 0) ||
                (strcmp(findData.cFileName, "..") == 0) ||
                (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ||
                (findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) {
            goto nextFile;
        }
        nnode = balloc(sizeof(fileNode_t));
        memset(nnode, 0x0, sizeof(fileNode_t));
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            fmtAlloc(&(nnode->name), MAX_PATH, "%s/", findData.cFileName);
            nnode->flags |= WHITELIST_DIR;
        } else {
            nnode->name = bstrdup(findData.cFileName);
        }
        if (firstTime) {
            cnode->child = nnode;
            firstTime = 0;
        } else {
            cnode->next = nnode;
        }
        cnode = nnode;
        if (strncmp(path + strlen(websDefaultDir), "/" CGI_BIN, 
                strlen(CGI_BIN) + 1) == 0) {
            cnode->flags |= WHITELIST_CGI;
        }
#if BIT_PACK_SSL
        for (l = sslList; l != NULL; l = l->next) {
            if (strncmp(path + strlen(websDefaultDir), l->url, 
                    strlen(l->url)) == 0) {
                cnode->flags |= WHITELIST_SSL;
            }
        }
#endif
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            websMakePath(tmppath, path, findData.cFileName, 1);
            if (level < MAX_URL_DEPTH) {
                if (websBuildWhitelistRecursive(tmppath, cnode, level + 1) < 0){
                    cnode->flags |= WHITELIST_BLOCKED;
                }
            }
        }
nextFile:
        if (FindNextFile(fh, &findData) == 0) {
            if (GetLastError() != ERROR_NO_MORE_FILES) {
                rc = -1;
            }
            break;
        }
    }
    FindClose(fh);
    return rc;
}



/*
    LINUX, MACOSX, ETC: Build whitelist
 */
#else /* !WIN */
static int websBuildWhitelistRecursive(char *_path, fileNode_t *dir, int level)
{
    struct dirent   *findData, *result;
    DIR             *fh;
    fileNode_t      *cnode, *nnode; /* current node, next node */
    int     firstTime;
    char    path[PATH_MAX];
    char    tmppath[PATH_MAX];

    firstTime = 1;
    cnode = dir;
    /* 
        On some platforms such as Solaris, struct dirent includes only one byte for d_name field, meaning we would
        overflow the field when readdir_r is called.  So we check for this here.  Another potential issue is if somehow
        a filesystem is mounted or linked in a subdirectory or file that has name longer than PATH_MAX.  We ignore that
        possibility here, and leave it to the user to ensure that the wwwroot directory does not contain this
        configuration. http://womble.decadentplace.org.uk/readdir_r-advisory.html
     */ 
    if (sizeof(struct dirent) > PATH_MAX) {
        findData = balloc(sizeof(struct dirent));
    } else {
        findData = balloc(sizeof(struct dirent) + PATH_MAX);
    }
    websMakePath(path, _path, NULL, 0);
    if ((fh = opendir(path)) == NULL) {
        bfree(findData);
        return -1;  /* Likely no permission to access this directory */
    }
    while ((readdir_r(fh, findData, &result) == 0) && result) {
        if ((strcmp(findData->d_name, ".") == 0) || (strcmp(findData->d_name, "..") == 0) ||
                (findData->d_type != DT_REG && findData->d_type != DT_DIR)) {
            continue;
        }
        nnode = balloc(sizeof(fileNode_t));
        memset(nnode, 0x0, sizeof(fileNode_t));
        if (findData->d_type == DT_DIR) {
            fmtAlloc(&(nnode->name), PATH_MAX, "%s/", findData->d_name);
            nnode->flags |= WHITELIST_DIR;
        } else {
            nnode->name = bstrdup(findData->d_name);
        }
        if (firstTime) {
            cnode->child = nnode;
            firstTime = 0;
        } else {
            cnode->next = nnode;
        }
        cnode = nnode;
        if (strncmp(path + strlen(websDefaultDir), "/" CGI_BIN, 
                strlen(CGI_BIN) + 1) == 0) {
            cnode->flags |= WHITELIST_CGI;
        }
#if BIT_PACK_SSL
        sslList_t   *l;
        for (l = sslList; l != NULL; l = l->next) {
            if (strncmp(path + strlen(websDefaultDir), l->url, strlen(l->url)) == 0) {
                cnode->flags |= WHITELIST_SSL;
                break;
            }
        }
#endif
        if (findData->d_type == DT_DIR) {
            websMakePath(tmppath, path, findData->d_name, 0);
            if (level < MAX_URL_DEPTH) {
                if (websBuildWhitelistRecursive(tmppath, cnode, level + 1) < 0){
                    cnode->flags |= WHITELIST_BLOCKED;
                }
            }
        }
    }
    bfree(findData);
    closedir(fh);
    return 0;
}
#endif /* WIN, LINUX, ETC */


/*
    Copy src to d,
    Append subdir, if subdir != NULL,
    Append '/' and '*' if wildcard != 0
*/
static void websMakePath(char *d, char *src, char *subdir, int wildcard)
{
    int     max;
    char    *a;

#if WIN
    max = MAX_PATH - 4 - (subdir ? strlen(subdir) : 0);
#else
    max = PATH_MAX - 4 - (subdir ? strlen(subdir) : 0);
#endif
    for (a = src; *a != '\0' && (int)(a - src) < max; a++, d++) {
        *d = *a;
    }
    if (*(d - 1) == '*') d -= 1;
    if (subdir) {
        if (*(d - 1) != '/') {
            *d = '/'; d++;
        }
        for (a = subdir; *a != '\0'; a++) {
            *d = *a; d++;
        }
    }
    if (*(d - 1) != '/') {
        *d = '/'; d++;
    }
    if (wildcard) {
        *d = '*'; d++;
    }
    *d = '\0';
}

#endif /* WEBS_WHITELIST_SUPPORT */

/******************************************************************************/
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
