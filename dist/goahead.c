/*
 * Embedthis GoAhead Library Source
 */

#include "goahead.h"

#if ME_COM_GOAHEAD



/********* Start of file ../../../src/action.c ************/


/*
    action.c -- Action handler

    This module implements the /action handler. It is a simple binding of URIs to C functions.
    This enables a very high performance implementation with easy parsing and decoding of query
    strings and posted data.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/



/************************************ Locals **********************************/

static WebsHash actionTable = -1;            /* Symbol table for actions */

/************************************* Code ***********************************/
/*
    Process an action request. Returns 1 always to indicate it handled the URL
 */
static bool actionHandler(Webs *wp)
{
    WebsKey     *sp;
    char        actionBuf[ME_GOAHEAD_LIMIT_URI + 1];
    char        *cp, *actionName;
    WebsAction  fn;

    assert(websValid(wp));
    assert(actionTable >= 0);

    /*
        Extract the action name
     */
    scopy(actionBuf, sizeof(actionBuf), wp->path);
    if ((actionName = strchr(&actionBuf[1], '/')) == NULL) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Missing action name");
        return 1;
    }
    actionName++;
    if ((cp = strchr(actionName, '/')) != NULL) {
        *cp = '\0';
    }
    /*
        Lookup the C action function first and then try tcl (no javascript support yet).
     */
    sp = hashLookup(actionTable, actionName);
    if (sp == NULL) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Action %s is not defined", actionName);
    } else {
        fn = (WebsAction) sp->content.value.symbol;
        assert(fn);
        if (fn) {
#if ME_GOAHEAD_LEGACY
            (*((WebsProc) fn))((void*) wp, actionName, wp->query);
#else
            (*fn)((void*) wp);
#endif
        }
    }
    return 1;
}


/*
    Define a function in the "action" map space
 */
PUBLIC int websDefineAction(char *name, void *fn)
{
    assert(name && *name);
    assert(fn);

    if (fn == NULL) {
        return -1;
    }
    hashEnter(actionTable, name, valueSymbol(fn), 0);
    return 0;
}


static void closeAction()
{
    if (actionTable != -1) {
        hashFree(actionTable);
        actionTable = -1;
    }
}


PUBLIC void websActionOpen()
{
    actionTable = hashCreate(WEBS_HASH_INIT);
    websDefineHandler("action", 0, actionHandler, closeAction, 0);
}


#if ME_GOAHEAD_LEGACY
/*
    Don't use these routes. Use websWriteHeaders, websEndHeaders instead.

    Write a webs header. This is a convenience routine to write a common header for an action back to the browser.
 */
PUBLIC void websHeader(Webs *wp)
{
    assert(websValid(wp));

    websWriteHeaders(wp, -1, 0);
    websWriteEndHeaders(wp);
    websWrite(wp, "<html>\n");
}


PUBLIC void websFooter(Webs *wp)
{
    assert(websValid(wp));
    websWrite(wp, "</html>\n");
}
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/alloc.c ************/


/*
    alloc.c -- Optional WebServer memory allocator

    This file implements a fast block allocation scheme suitable for operating systems whoes malloc suffers from
    fragmentation.  This allocator maintains block class queues for rapid allocation and minimal fragmentation. This
    allocator does not coalesce blocks. The storage space may be populated statically or via the traditional malloc
    mechanisms. Large blocks greater than the maximum class size may be allocated from the O/S or run-time system via
    malloc. To permit the use of malloc, call wopen with flags set to WEBS_USE_MALLOC (this is the default).  It is
    recommended that wopen be called first thing in the application.  If it is not, it will be called with default
    values on the first call to walloc(). Note that this code is not designed for multi-threading purposes and it
    depends on newly declared variables being initialized to zero.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/********************************* Includes ***********************************/



#if ME_GOAHEAD_REPLACE_MALLOC
/********************************* Defines ************************************/
/*
    ROUNDUP4(size) returns the next higher integer value of size that is divisible by 4, or the value of size if size is
    divisible by 4. ROUNDUP4() is used in aligning memory allocations on 4-byte boundaries.
    Note:  ROUNDUP4() is only required on some operating systems.
 */
#define ROUNDUP4(size) ((size) % 4) ? (size) + (4 - ((size) % 4)) : (size)

/*
    qhead blocks are created as the original memory allocation is freed up. See wfree.
 */
static WebsAlloc    *qhead[WEBS_MAX_CLASS];             /* Per class block q head */
static char         *freeBuf;                           /* Pointer to free memory */
static char         *freeNext;                          /* Pointer to next free mem */
static int          freeSize;                           /* Size of free memory */
static int          freeLeft;                           /* Size of free left for use */
static int          controlFlags = WEBS_USE_MALLOC;     /* Default to auto-malloc */
static int          wopenCount = 0;                     /* Num tasks using walloc */

/*************************** Forward Declarations *****************************/

static int wallocGetSize(ssize size, int *q);

/********************************** Code **************************************/
/*
    Initialize the walloc module. wopenAlloc should be called the very first thing after the application starts and 
    wcloseAlloc should be called the last thing before exiting. If wopenAlloc is not called, it will be called on the first
    allocation with default values. "buf" points to memory to use of size "bufsize". If buf is NULL, memory is allocated
    using malloc. flags may be set to WEBS_USE_MALLOC if using malloc is okay. This routine will allocate *  an initial
    buffer of size bufsize for use by the application.
 */
PUBLIC int wopenAlloc(void *buf, int bufsize, int flags)
{
    controlFlags = flags;

    /*
        If wopen already called by a shared process, just increment the count and return
     */
    if (++wopenCount > 1) {
        return 0;
    }
    if (buf == NULL) {
        if (bufsize == 0) {
            bufsize = WEBS_DEFAULT_MEM;
        }
        bufsize = ROUNDUP4(bufsize);
        if ((buf = malloc(bufsize)) == NULL) {
            /* 
                Resetting wopenCount so client code can decide to call wopenAlloc() again with a smaller memory request.
            */
             --wopenCount;
            return -1;
        }
    } else {
        controlFlags |= WEBS_USER_BUF;
    }
    freeSize = freeLeft = bufsize;
    freeBuf = freeNext = buf;
    memset(qhead, 0, sizeof(qhead));
    return 0;
}


PUBLIC void wcloseAlloc()
{
    if (--wopenCount <= 0 && !(controlFlags & WEBS_USER_BUF)) {
        free(freeBuf);
        wopenCount = 0;
    }
}


/*
    Allocate a block of the requested size. First check the block queues for a suitable one.
 */
PUBLIC void *walloc(ssize size)
{
    WebsAlloc   *bp;
    int     q, memSize;

    /*
        Call wopen with default values if the application has not yet done so
     */
    if (freeBuf == NULL) {
        if (wopenAlloc(NULL, WEBS_DEFAULT_MEM, 0) < 0) {
            return NULL;
        }
    }
    if (size < 0) {
        return NULL;
    }
    memSize = wallocGetSize(size, &q);

    if (q >= WEBS_MAX_CLASS) {
        /*
            Size if bigger than the maximum class. Malloc if use has been okayed
         */
        if (controlFlags & WEBS_USE_MALLOC) {
            memSize = ROUNDUP4(memSize);
            bp = (WebsAlloc*) malloc(memSize);
            if (bp == NULL) {
                printf("B: malloc failed\n");
                return NULL;
            }
        } else {
            printf("B: malloc failed\n");
            return NULL;
        }
        /*
            the u.size is the actual size allocated for data
         */
        bp->u.size = memSize - sizeof(WebsAlloc);
        bp->flags = WEBS_MALLOCED;

    } else if ((bp = qhead[q]) != NULL) {
        /*
            Take first block off the relevant q if non-empty
         */
        qhead[q] = bp->u.next;
        bp->u.size = memSize - sizeof(WebsAlloc);
        bp->flags = 0;

    } else {
        if (freeLeft > memSize) {
            /*
                The q was empty, and the free list has spare memory so create a new block out of the primary free block
             */
            bp = (WebsAlloc*) freeNext;
            freeNext += memSize;
            freeLeft -= memSize;
            bp->u.size = memSize - sizeof(WebsAlloc);
            bp->flags = 0;

        } else if (controlFlags & WEBS_USE_MALLOC) {
            /*
                Nothing left on the primary free list, so malloc a new block
             */
            memSize = ROUNDUP4(memSize);
            if ((bp = (WebsAlloc*) malloc(memSize)) == NULL) {
                printf("B: malloc failed\n");
                return NULL;
            }
            bp->u.size = memSize - sizeof(WebsAlloc);
            bp->flags = WEBS_MALLOCED;

        } else {
            printf("B: malloc failed\n");
            return NULL;
        }
    }
    bp->flags |= WEBS_INTEGRITY;
    return (void*) ((char*) bp + sizeof(WebsAlloc));
}


/*
    Free a block back to the relevant free q. We don't free back to the O/S or run time system unless the block is
    greater than the maximum class size. We also do not coalesce blocks.  
 */
PUBLIC void wfree(void *mp)
{
    WebsAlloc   *bp;
    int     q;

    if (mp == 0) {
        return;
    }
    bp = (WebsAlloc*) ((char*) mp - sizeof(WebsAlloc));
    assert((bp->flags & WEBS_INTEGRITY_MASK) == WEBS_INTEGRITY);
    if ((bp->flags & WEBS_INTEGRITY_MASK) != WEBS_INTEGRITY) {
        return;
    }
    wallocGetSize(bp->u.size, &q);
    if (bp->flags & WEBS_MALLOCED) {
        free(bp);
        return;
    }
    /*
        Simply link onto the head of the relevant q
     */
    bp->u.next = qhead[q];
    qhead[q] = bp;
    bp->flags = WEBS_FILL_WORD;
}


/*
    Reallocate a block. Allow NULL pointers and just do a malloc. Note: if the realloc fails, we return NULL and the
    previous buffer is preserved.
 */
PUBLIC void *wrealloc(void *mp, ssize newsize)
{
    WebsAlloc   *bp;
    void    *newbuf;

    if (mp == NULL) {
        return walloc(newsize);
    }
    bp = (WebsAlloc*) ((char*) mp - sizeof(WebsAlloc));
    assert((bp->flags & WEBS_INTEGRITY_MASK) == WEBS_INTEGRITY);

    /*
        If the allocated memory already has enough room just return the previously allocated address.
     */
    if (bp->u.size >= newsize) {
        return mp;
    }
    if ((newbuf = walloc(newsize)) != NULL) {
        memcpy(newbuf, mp, bp->u.size);
        wfree(mp);
    }
    return newbuf;
}


/*
    Find the size of the block to be walloc'ed.  It takes in a size, finds the smallest binary block it fits into, adds
    an overhead amount and returns.  q is the binary size used to keep track of block sizes in use.  Called from both
    walloc and wfree.
 */
static int wallocGetSize(ssize size, int *q)
{
    int mask;

    mask = (size == 0) ? 0 : (size-1) >> WEBS_SHIFT;
    for (*q = 0; mask; mask >>= 1) {
        *q = *q + 1;
    }
    return ((1 << (WEBS_SHIFT + *q)) + sizeof(WebsAlloc));
}


#else /* !ME_GOAHEAD_REPLACE_MALLOC */

/*
    Stubs
 */
PUBLIC int wopenAlloc(void *buf, int bufsize, int flags) { return 0; }
PUBLIC void wcloseAlloc() { }

#endif /* ME_GOAHEAD_REPLACE_MALLOC */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/auth.c ************/


/*
    auth.c -- Authorization Management

    This modules supports a user/role/ability based authorization scheme.

    In this scheme Users have passwords and can have multiple roles. A role is associated with the ability to do
    things like "admin" or "user" or "support". A role may have abilities (which are typically verbs) like "add",
    "shutdown". 

    When the web server starts up, it loads a route and authentication configuration file that specifies the Users, 
    Roles and Routes.  Routes specify the required abilities to access URLs by specifying the URL prefix. Once logged
    in, the user's abilities are tested against the route abilities. When the web server receivess a request, the set of
    Routes is consulted to select the best route. If the routes requires abilities, the user must be logged in and
    authenticated. 

    Three authentication backend protocols are supported:
        HTTP basic authentication which uses browser dialogs and clear text passwords (insecure unless over TLS)
        HTTP digest authentication which uses browser dialogs
        Web form authentication which uses a web page form to login (insecure unless over TLS)

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************* Includes ***********************************/



#if ME_GOAHEAD_AUTH

#if ME_COMPILER_HAS_PAM
    #include <security/pam_appl.h>
#endif

/*********************************** Locals ***********************************/

static WebsHash users = -1;
static WebsHash roles = -1;
static char *secret;
static int autoLogin = ME_GOAHEAD_AUTO_LOGIN;
static WebsVerify verifyPassword = websVerifyPasswordFromFile;

#if ME_COMPILER_HAS_PAM
typedef struct {
    char    *name;
    char    *password;
} UserInfo;

#if MACOSX
    typedef int Gid;
#else
    typedef gid_t Gid;
#endif
#endif

/********************************** Forwards **********************************/

static void computeAbilities(WebsHash abilities, char *role, int depth);
static void computeUserAbilities(WebsUser *user);
static WebsUser *createUser(char *username, char *password, char *roles);
static void freeRole(WebsRole *rp);
static void freeUser(WebsUser *up);
static void logoutServiceProc(Webs *wp);
static void loginServiceProc(Webs *wp);

#if ME_GOAHEAD_JAVASCRIPT && FUTURE
static int jsCan(int jsid, Webs *wp, int argc, char **argv);
#endif
#if ME_DIGEST
static char *calcDigest(Webs *wp, char *username, char *password);
static char *createDigestNonce(Webs *wp);
static int parseDigestNonce(char *nonce, char **secret, char **realm, WebsTime *when);
#endif

#if ME_COMPILER_HAS_PAM
static int pamChat(int msgCount, const struct pam_message **msg, struct pam_response **resp, void *data);
#endif

/************************************ Code ************************************/

PUBLIC bool websAuthenticate(Webs *wp)
{
    WebsRoute   *route;
    char        *username;
    int         cached;

    assert(wp);
    assert(wp->route);
    route = wp->route;

    if (!route || !route->authType || autoLogin) {
        /* Authentication not required */
        return 1;
    }
    cached = 0;
    if (wp->cookie && websGetSession(wp, 0) != 0) {
        /*
            Retrieve authentication state from the session storage. Faster than re-authenticating.
         */
        if ((username = (char*) websGetSessionVar(wp, WEBS_SESSION_USERNAME, 0)) != 0) {
            cached = 1;
            wp->username = sclone(username);
        }
    }
    if (!cached) {
        if (wp->authType && !smatch(wp->authType, route->authType)) {
            websError(wp, HTTP_CODE_UNAUTHORIZED, "Access denied. Wrong authentication protocol type.");
            return 0;
        }
        if (wp->authDetails && route->parseAuth) {
            if (!(route->parseAuth)(wp)) {
                return 0;
            }
        }
        if (!wp->username || !*wp->username) {
            if (route->askLogin) {
                (route->askLogin)(wp);
            }
            websRedirectByStatus(wp, HTTP_CODE_UNAUTHORIZED);
            return 0;
        }
        if (!(route->verify)(wp)) {
            if (route->askLogin) {
                (route->askLogin)(wp);
            }
            websRedirectByStatus(wp, HTTP_CODE_UNAUTHORIZED);
            return 0;
        }
        /*
            Store authentication state and user in session storage                                         
         */                                                                                                
        if (websGetSession(wp, 1) != 0) {                                                    
            websSetSessionVar(wp, WEBS_SESSION_USERNAME, wp->username);                                
        }
    }
    return 1;
}


PUBLIC int websOpenAuth(int minimal)
{
    char    sbuf[64];
    
    assert(minimal == 0 || minimal == 1);

    if ((users = hashCreate(-1)) < 0) {
        return -1;
    }
    if ((roles = hashCreate(-1)) < 0) {
        return -1;
    }
    if (!minimal) {
        fmt(sbuf, sizeof(sbuf), "%x:%x", rand(), time(0));
        secret = websMD5(sbuf);
#if ME_GOAHEAD_JAVASCRIPT && FUTURE
        websJsDefine("can", jsCan);
#endif
        websDefineAction("login", loginServiceProc);
        websDefineAction("logout", logoutServiceProc);
    }
    if (smatch(ME_GOAHEAD_AUTH_STORE, "file")) {
        verifyPassword = websVerifyPasswordFromFile;
#if ME_COMPILER_HAS_PAM
    } else if (smatch(ME_GOAHEAD_AUTH_STORE, "pam")) {
        verifyPassword = websVerifyPasswordFromPam;
#endif
    }
    return 0;
}


PUBLIC void websCloseAuth() 
{
    WebsKey     *key, *next;

    wfree(secret);
    if (users >= 0) {
        for (key = hashFirst(users); key; key = next) {
            next = hashNext(users, key);
            freeUser(key->content.value.symbol);
        }
        hashFree(users);
        users = -1;
    }
    if (roles >= 0) {
        for (key = hashFirst(roles); key; key = next) {
            next = hashNext(roles, key);
            freeRole(key->content.value.symbol);
        }
        hashFree(roles);
        roles = -1;
    }
}


#if KEEP
PUBLIC int websWriteAuthFile(char *path)
{
    FILE        *fp;
    WebsKey     *kp, *ap;
    WebsRole    *role;
    WebsUser    *user;
    char        *tempFile;

    assert(path && *path);

    tempFile = websTempFile(NULL, NULL);
    if ((fp = fopen(tempFile, "w" FILE_TEXT)) == 0) {
        error("Cannot open %s", tempFile);
        return -1;
    }
    fprintf(fp, "#\n#   %s - Authorization data\n#\n\n", basename(path));

    if (roles >= 0) {
        for (kp = hashFirst(roles); kp; kp = hashNext(roles, kp)) {
            role = kp->content.value.symbol;
            fprintf(fp, "role name=%s abilities=", kp->name.value.string);
            for (ap = hashFirst(role->abilities); ap; ap = hashNext(role->abilities, ap)) {
                fprintf(fp, "%s,", ap->name.value.string);
            }
            fputc('\n', fp);
        }
        fputc('\n', fp);
    }
    if (users >= 0) {
        for (kp = hashFirst(users); kp; kp = hashNext(users, kp)) {
            user = kp->content.value.symbol;
            fprintf(fp, "user name=%s password=%s roles=%s", user->name, user->password, user->roles);
            fputc('\n', fp);
        }
    }
    fclose(fp);
    unlink(path);
    if (rename(tempFile, path) < 0) {
        error("Cannot create new %s", path);
        return -1;
    }
    return 0;
}
#endif


static WebsUser *createUser(char *username, char *password, char *roles)
{
    WebsUser    *user;

    assert(username);

    if ((user = walloc(sizeof(WebsUser))) == 0) {
        return 0;
    }
    user->name = sclone(username);
    user->roles = sclone(roles);
    user->password = sclone(password);
    user->abilities = -1;
    return user;
}


WebsUser *websAddUser(char *username, char *password, char *roles)
{
    WebsUser    *user;

    if (!username) {
        error("User is missing name");
        return 0;
    }
    if (websLookupUser(username)) {
        error("User %s already exists", username);
        /* Already exists */
        return 0;
    }
    if ((user = createUser(username, password, roles)) == 0) {
        return 0;
    }
    if (hashEnter(users, username, valueSymbol(user), 0) == 0) {
        return 0;
    }
    return user;
}


PUBLIC int websRemoveUser(char *username) 
{
    WebsKey     *key;
    
    assert(username && *username);
    if ((key = hashLookup(users, username)) != 0) {
        freeUser(key->content.value.symbol);
    }
    return hashDelete(users, username);
}


static void freeUser(WebsUser *up)
{
    assert(up);
    hashFree(up->abilities);
    wfree(up->name);
    wfree(up->password);
    wfree(up->roles);
    wfree(up);
}


PUBLIC int websSetUserRoles(char *username, char *roles)
{
    WebsUser    *user;

    assert(username &&*username);
    if ((user = websLookupUser(username)) == 0) {
        return -1;
    }
    wfree(user->roles);
    user->roles = sclone(roles);
    computeUserAbilities(user);
    return 0;
}


WebsUser *websLookupUser(char *username)
{
    WebsKey     *key;

    assert(username && *username);
    if ((key = hashLookup(users, username)) == 0) {
        return 0;
    }
    return (WebsUser*) key->content.value.symbol;
}


static void computeAbilities(WebsHash abilities, char *role, int depth)
{
    WebsRole    *rp;
    WebsKey     *key;

    assert(abilities >= 0);
    assert(role && *role);
    assert(depth >= 0);

    if (depth > 20) {
        error("Recursive ability definition for %s", role);
        return;
    }
    if (roles >= 0) {
        if ((key = hashLookup(roles, role)) != 0) {
            rp = (WebsRole*) key->content.value.symbol;
            for (key = hashFirst(rp->abilities); key; key = hashNext(rp->abilities, key)) {
                computeAbilities(abilities, key->name.value.string, ++depth);
            }
        } else {
            hashEnter(abilities, role, valueInteger(0), 0);
        }
    }
}


static void computeUserAbilities(WebsUser *user)
{
    char    *ability, *roles, *tok;

    assert(user);
    if ((user->abilities = hashCreate(-1)) == 0) {
        return;
    }
    roles = sclone(user->roles);
    for (ability = stok(roles, " \t,", &tok); ability; ability = stok(NULL, " \t,", &tok)) {
        computeAbilities(user->abilities, ability, 0);
    }
#if ME_DEBUG
    {
        WebsKey *key;
        trace(5 | WEBS_RAW_MSG, "User \"%s\" has abilities: ", user->name);
        for (key = hashFirst(user->abilities); key; key = hashNext(user->abilities, key)) {
            trace(5 | WEBS_RAW_MSG, "%s ", key->name.value.string);
            ability = key->name.value.string;
        }
        trace(5, "");
    }
#endif
    wfree(roles);
}


PUBLIC void websComputeAllUserAbilities()
{
    WebsUser    *user;
    WebsKey     *sym;

    if (users) {
        for (sym = hashFirst(users); sym; sym = hashNext(users, sym)) {
            user = (WebsUser*) sym->content.value.symbol;
            computeUserAbilities(user);
        }
    }
}


WebsRole *websAddRole(char *name, WebsHash abilities)
{
    WebsRole    *rp;

    if (!name) {
        error("Role is missing name");
        return 0;
    }
    if (hashLookup(roles, name)) {
        error("Role %s already exists", name);
        /* Already exists */
        return 0;
    }
    if ((rp = walloc(sizeof(WebsRole))) == 0) {
        return 0;
    }
    rp->abilities = abilities;
    if (hashEnter(roles, name, valueSymbol(rp), 0) == 0) {
        return 0;
    }
    return rp;
}


static void freeRole(WebsRole *rp)
{
    assert(rp);
    hashFree(rp->abilities);
    wfree(rp);
}


/*
    Does not recompute abilities for users that use this role
 */
PUBLIC int websRemoveRole(char *name) 
{
    WebsRole    *rp;
    WebsKey     *sym;

    assert(name && *name);
    if (roles) {
        if ((sym = hashLookup(roles, name)) == 0) {
            return -1;
        }
        rp = sym->content.value.symbol;
        hashFree(rp->abilities);
        wfree(rp);
        return hashDelete(roles, name);
    }
    return -1;
}


PUBLIC WebsHash websGetUsers()
{
    return users;
}


PUBLIC WebsHash websGetRoles()
{
    return roles;
}


PUBLIC bool websLoginUser(Webs *wp, char *username, char *password)
{
    assert(wp);
    assert(wp->route);
    assert(username);
    assert(password);

    if (!wp->route || !wp->route->verify) {
        return 0;
    }
    wfree(wp->username);
    wp->username = sclone(username);
    wfree(wp->password);
    wp->password = sclone(password);

    if (!(wp->route->verify)(wp)) {
        trace(2, "Password does not match");
        return 0;
    }
    trace(2, "Login successful for %s", username);
    websSetSessionVar(wp, WEBS_SESSION_USERNAME, wp->username);                                
    return 1;
}


/*
    Internal login service routine for Form-based auth
 */
static void loginServiceProc(Webs *wp)
{
    WebsRoute   *route;

    assert(wp);
    route = wp->route;
    assert(route);
    
    if (websLoginUser(wp, websGetVar(wp, "username", ""), websGetVar(wp, "password", ""))) {
        /* If the application defines a referrer session var, redirect to that */
        char *referrer;
        if ((referrer = websGetSessionVar(wp, "referrer", 0)) != 0) {
            websRedirect(wp, referrer);
        } else {
            websRedirectByStatus(wp, HTTP_CODE_OK);
        }
        websSetSessionVar(wp, "loginStatus", "ok");
    } else {
        if (route->askLogin) {
            (route->askLogin)(wp);
        }
        websSetSessionVar(wp, "loginStatus", "failed");
        websRedirectByStatus(wp, HTTP_CODE_UNAUTHORIZED);
    }
}


static void logoutServiceProc(Webs *wp)
{
    assert(wp);
    websRemoveSessionVar(wp, WEBS_SESSION_USERNAME);
    if (smatch(wp->authType, "basic") || smatch(wp->authType, "digest")) {
        websError(wp, HTTP_CODE_UNAUTHORIZED, "Logged out.");
        return;
    }
    websRedirectByStatus(wp, HTTP_CODE_OK);
}


static void basicLogin(Webs *wp)
{
    assert(wp);
    assert(wp->route);
    wfree(wp->authResponse);
    wp->authResponse = sfmt("Basic realm=\"%s\"", ME_GOAHEAD_REALM);
}


void websSetPasswordStoreVerify(WebsVerify verify)
{
    verifyPassword = verify;
}


WebsVerify websGetPasswordStoreVerify()
{
    return verifyPassword;
}


PUBLIC bool websVerifyPasswordFromFile(Webs *wp)
{
    char    passbuf[ME_GOAHEAD_LIMIT_PASSWORD * 3 + 3];
    bool    success;

    assert(wp);
    if (!wp->user && (wp->user = websLookupUser(wp->username)) == 0) {
        trace(5, "verifyUser: Unknown user \"%s\"", wp->username);
        return 0;
    }
    /*
        Verify the password. If using Digest auth, we compare the digest of the password.
        Otherwise we encode the plain-text password and compare that
     */
    if (!wp->encoded) {
        fmt(passbuf, sizeof(passbuf), "%s:%s:%s", wp->username, ME_GOAHEAD_REALM, wp->password);
        wfree(wp->password);
        wp->password = websMD5(passbuf);
        wp->encoded = 1;
    }
    if (wp->digest) {
        success = smatch(wp->password, wp->digest);
    } else {
        success = smatch(wp->password, wp->user->password);
    }
    if (success) {
        trace(5, "User \"%s\" authenticated", wp->username);
    } else {
        trace(5, "Password for user \"%s\" failed to authenticate", wp->username);
    }
    return success;
}


#if ME_COMPILER_HAS_PAM
/*
    Copy this routine if creating your own custom password store back end
 */
PUBLIC bool websVerifyPasswordFromPam(Webs *wp)
{
    WebsBuf             abilities;
    pam_handle_t        *pamh;
    UserInfo            info;
    struct pam_conv     conv = { pamChat, &info };
    struct group        *gp;
    int                 res, i;
   
    assert(wp);
    assert(wp->username && wp->username);
    assert(wp->password);
    assert(!wp->encoded);

    info.name = (char*) wp->username;
    info.password = (char*) wp->password;
    pamh = NULL;
    if ((res = pam_start("login", info.name, &conv, &pamh)) != PAM_SUCCESS) {
        return 0;
    }
    if ((res = pam_authenticate(pamh, PAM_DISALLOW_NULL_AUTHTOK)) != PAM_SUCCESS) {
        pam_end(pamh, PAM_SUCCESS);
        trace(5, "httpPamVerifyUser failed to verify %s", wp->username);
        return 0;
    }
    pam_end(pamh, PAM_SUCCESS);
    trace(5, "httpPamVerifyUser verified %s", wp->username);

    if (!wp->user) {
        wp->user = websLookupUser(wp->username);
    }
    if (!wp->user) {
        Gid     groups[32];
        int     ngroups;
        /* 
            Create a temporary user with a abilities set to the groups 
         */
        ngroups = sizeof(groups) / sizeof(Gid);
        if ((i = getgrouplist(wp->username, 99999, groups, &ngroups)) >= 0) {
            bufCreate(&abilities, 128, -1);
            for (i = 0; i < ngroups; i++) {
                if ((gp = getgrgid(groups[i])) != 0) {
                    bufPutStr(&abilities, gp->gr_name);
                    bufPutc(&abilities, ' ');
                }
            }
            bufAddNull(&abilities);
            trace(5, "Create temp user \"%s\" with abilities: %s", wp->username, abilities.servp);
            if ((wp->user = websAddUser(wp->username, 0, abilities.servp)) == 0) {
                return 0;
            }
            computeUserAbilities(wp->user);
        }
    }
    return 1;
}


/*  
    Callback invoked by the pam_authenticate function
 */
static int pamChat(int msgCount, const struct pam_message **msg, struct pam_response **resp, void *data) 
{
    UserInfo                *info;
    struct pam_response     *reply;
    int                     i;
    
    i = 0;
    reply = 0;
    info = (UserInfo*) data;

    if (resp == 0 || msg == 0 || info == 0) {
        return PAM_CONV_ERR;
    }
    if ((reply = calloc(msgCount, sizeof(struct pam_response))) == 0) {
        return PAM_CONV_ERR;
    }
    for (i = 0; i < msgCount; i++) {
        reply[i].resp_retcode = 0;
        reply[i].resp = 0;
        
        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_ON:
            reply[i].resp = sclone(info->name);
            break;

        case PAM_PROMPT_ECHO_OFF:
            /* Retrieve the user password and pass onto pam */
            reply[i].resp = sclone(info->password);
            break;

        default:
            free(reply);
            return PAM_CONV_ERR;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;
}
#endif /* ME_COMPILER_HAS_PAM */


#if ME_GOAHEAD_JAVASCRIPT && FUTURE
static int jsCan(int jsid, Webs *wp, int argc, char **argv)
{
    assert(jsid >= 0);
    assert(wp);

    if (websCanString(wp, argv[0])) {
        //  FUTURE
        return 0;
    }
    return 1;
} 
#endif


static bool parseBasicDetails(Webs *wp)
{
    char    *cp, *userAuth;

    assert(wp);
    /*
        Split userAuth into userid and password
     */
    cp = 0;
    if ((userAuth = websDecode64(wp->authDetails)) != 0) {
        if ((cp = strchr(userAuth, ':')) != NULL) {
            *cp++ = '\0';
        }
    }
    if (cp) {
        wp->username = sclone(userAuth);
        wp->password = sclone(cp);
        wp->encoded = 0;
    } else {
        wp->username = sclone("");
        wp->password = sclone("");
    }
    wfree(userAuth);
    return 1;
}


#if ME_DIGEST
static void digestLogin(Webs *wp)
{
    char  *nonce, *opaque;

    assert(wp);
    assert(wp->route);

    nonce = createDigestNonce(wp);
    /* Opaque is unused. Set to anything */
    opaque = "5ccc069c403ebaf9f0171e9517f40e41";
    wp->authResponse = sfmt(
        "Digest realm=\"%s\", domain=\"%s\", qop=\"%s\", nonce=\"%s\", opaque=\"%s\", algorithm=\"%s\", stale=\"%s\"",
        ME_GOAHEAD_REALM, websGetServerUrl(), "auth", nonce, opaque, "MD5", "FALSE");
    wfree(nonce);
}


static bool parseDigestDetails(Webs *wp)
{
    WebsTime    when;
    char        *value, *tok, *key, *dp, *sp, *secret, *realm;
    int         seenComma;

    assert(wp);
    key = sclone(wp->authDetails);

    while (*key) {
        while (*key && isspace((uchar) *key)) {
            key++;
        }
        tok = key;
        while (*tok && !isspace((uchar) *tok) && *tok != ',' && *tok != '=') {
            tok++;
        }
        if (*tok) {
            *tok++ = '\0';
        }

        while (isspace((uchar) *tok)) {
            tok++;
        }
        seenComma = 0;
        if (*tok == '\"') {
            value = ++tok;
            while (*tok != '\"' && *tok != '\0') {
                tok++;
            }
        } else {
            value = tok;
            while (*tok != ',' && *tok != '\0') {
                tok++;
            }
            seenComma++;
        }
        if (*tok) {
            *tok++ = '\0';
        }

        /*
            Handle back-quoting
         */
        if (strchr(value, '\\')) {
            for (dp = sp = value; *sp; sp++) {
                if (*sp == '\\') {
                    sp++;
                }
                *dp++ = *sp++;
            }
            *dp = '\0';
        }

        /*
            user, response, oqaque, uri, realm, nonce, nc, cnonce, qop
         */
        switch (tolower((uchar) *key)) {
        case 'a':
            if (scaselesscmp(key, "algorithm") == 0) {
                break;
            } else if (scaselesscmp(key, "auth-param") == 0) {
                break;
            }
            break;

        case 'c':
            if (scaselesscmp(key, "cnonce") == 0) {
                wp->cnonce = sclone(value);
            }
            break;

        case 'd':
            if (scaselesscmp(key, "domain") == 0) {
                break;
            }
            break;

        case 'n':
            if (scaselesscmp(key, "nc") == 0) {
                wp->nc = sclone(value);
            } else if (scaselesscmp(key, "nonce") == 0) {
                wp->nonce = sclone(value);
            }
            break;

        case 'o':
            if (scaselesscmp(key, "opaque") == 0) {
                wp->opaque = sclone(value);
            }
            break;

        case 'q':
            if (scaselesscmp(key, "qop") == 0) {
                wp->qop = sclone(value);
            }
            break;

        case 'r':
            if (scaselesscmp(key, "realm") == 0) {
                wp->realm = sclone(value);
            } else if (scaselesscmp(key, "response") == 0) {
                /* Store the response digest in the password field. This is MD5(user:realm:password) */
                wp->password = sclone(value);
                wp->encoded = 1;
            }
            break;

        case 's':
            if (scaselesscmp(key, "stale") == 0) {
                break;
            }
        
        case 'u':
            if (scaselesscmp(key, "uri") == 0) {
                wp->digestUri = sclone(value);
            } else if (scaselesscmp(key, "username") == 0 || scaselesscmp(key, "user") == 0) {
                wp->username = sclone(value);
            }
            break;

        default:
            /*  Just ignore keywords we don't understand */
            ;
        }
        key = tok;
        if (!seenComma) {
            while (*key && *key != ',') {
                key++;
            }
            if (*key) {
                key++;
            }
        }
    }
    if (wp->username == 0 || wp->realm == 0 || wp->nonce == 0 || wp->route == 0 || wp->password == 0) {
        return 0;
    }
    if (wp->qop && (wp->cnonce == 0 || wp->nc == 0)) {
        return 0;
    }
    if (wp->qop == 0) {
        wp->qop = sclone("");
    }
    /*
        Validate the nonce value - prevents replay attacks
     */
    when = 0; secret = 0; realm = 0;
    parseDigestNonce(wp->nonce, &secret, &realm, &when);
    if (!smatch(secret, secret)) {
        trace(2, "Access denied: Nonce mismatch");
        return 0;
    } else if (!smatch(realm, ME_GOAHEAD_REALM)) {
        trace(2, "Access denied: Realm mismatch");
        return 0;
    } else if (!smatch(wp->qop, "auth")) {
        trace(2, "Access denied: Bad qop");
        return 0;
    } else if ((when + (5 * 60)) < time(0)) {
        trace(2, "Access denied: Nonce is stale");
        return 0;
    }
    if (!wp->user) {
        if ((wp->user = websLookupUser(wp->username)) == 0) {
            trace(2, "Access denied: user is unknown");
            return 0;
        }
    }
    wp->digest = calcDigest(wp, 0, wp->user->password);
    return 1;
}


/*
    Create a nonce value for digest authentication (RFC 2617)
 */
static char *createDigestNonce(Webs *wp)
{
    static int64 next = 0;
    char         nonce[256];

    assert(wp);
    assert(wp->route);

    fmt(nonce, sizeof(nonce), "%s:%s:%x:%x", secret, ME_GOAHEAD_REALM, time(0), next++);
    return websEncode64(nonce);
}


static int parseDigestNonce(char *nonce, char **secret, char **realm, WebsTime *when)
{
    char    *tok, *decoded, *whenStr;                                                                      
                                                                                                           
    assert(nonce && *nonce);
    assert(secret);
    assert(realm);
    assert(when);

    if ((decoded = websDecode64(nonce)) == 0) {                                                             
        return -1;
    }                                                                                                      
    *secret = stok(decoded, ":", &tok);
    *realm = stok(NULL, ":", &tok);
    whenStr = stok(NULL, ":", &tok);
    *when = hextoi(whenStr);
    return 0;
}


/*
   Get a Digest value using the MD5 algorithm -- See RFC 2617 to understand this code.
*/
static char *calcDigest(Webs *wp, char *username, char *password)
{
    char  a1Buf[256], a2Buf[256], digestBuf[256];
    char  *ha1, *ha2, *method, *result;

    assert(wp);
    assert(username && *username);
    assert(password);

    /*
        Compute HA1. If username == 0, then the password is already expected to be in the HA1 format 
        (MD5(username:realm:password).
     */
    if (username == 0) {
        ha1 = sclone(password);
    } else {
        fmt(a1Buf, sizeof(a1Buf), "%s:%s:%s", username, wp->realm, password);
        ha1 = websMD5(a1Buf);
    }

    /*
        HA2
     */ 
    method = wp->method;
    fmt(a2Buf, sizeof(a2Buf), "%s:%s", method, wp->digestUri);
    ha2 = websMD5(a2Buf);

    /*
        H(HA1:nonce:HA2)
     */
    if (scmp(wp->qop, "auth") == 0) {
        fmt(digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, wp->nonce, wp->nc, wp->cnonce, wp->qop, ha2);

    } else if (scmp(wp->qop, "auth-int") == 0) {
        fmt(digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, wp->nonce, wp->nc, wp->cnonce, wp->qop, ha2);

    } else {
        fmt(digestBuf, sizeof(digestBuf), "%s:%s:%s", ha1, wp->nonce, ha2);
    }
    result = websMD5(digestBuf);
    wfree(ha1);
    wfree(ha2);
    return result;
}
#endif /* ME_DIGEST */


PUBLIC int websSetRouteAuth(WebsRoute *route, char *auth)
{
    WebsParseAuth parseAuth;
    WebsAskLogin  askLogin;

    assert(route);
    assert(auth && *auth);

    askLogin = 0;
    parseAuth = 0;
    if (smatch(auth, "basic")) {
        askLogin = basicLogin;
        parseAuth = parseBasicDetails;
#if ME_DIGEST
    } else if (smatch(auth, "digest")) {
        askLogin = digestLogin;
        parseAuth = parseDigestDetails;
#endif
    } else {
        auth = 0;
    }
    route->authType = sclone(auth);
    route->askLogin = askLogin;
    route->parseAuth = parseAuth;
    return 0;
}
#endif /* ME_GOAHEAD_AUTH */


/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/cgi.c ************/


/*
    cgi.c -- CGI processing
  
    This module implements the /cgi-bin handler. CGI processing differs from
    goforms processing in that each CGI request is executed as a separate 
    process, rather than within the webserver process. For each CGI request the
    environment of the new process must be set to include all the CGI variables
    and its standard input and output must be directed to the socket.  This
    is done using temporary files.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/



/*********************************** Defines **********************************/
#if ME_GOAHEAD_CGI && !ME_ROM

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

static Cgi      **cgiList;      /* walloc chain list of wp's to be closed */
static int      cgiMax;         /* Size of walloc list */

/************************************ Forwards ********************************/

static int checkCgi(int handle);
static int launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut);

/************************************* Code ***********************************/
/*
    Process a form request. Returns 1 always to indicate it handled the URL
 */
static bool cgiHandler(Webs *wp)
{
    Cgi         *cgip;
    WebsKey     *s;
    char        cgiPrefix[ME_GOAHEAD_LIMIT_FILENAME], *stdIn, *stdOut, cwd[ME_GOAHEAD_LIMIT_FILENAME];
    char        *cp, *cgiName, *cgiPath, **argp, **envp, **ep, *tok, *query, *dir, *extraPath;
    int         n, envpsize, argpsize, pHandle, cid;

    assert(websValid(wp));
    
    websSetEnv(wp);

    /*
        Extract the form name and then build the full path name.  The form name will follow the first '/' in path.
     */
    scopy(cgiPrefix, sizeof(cgiPrefix), wp->path);
    if ((cgiName = strchr(&cgiPrefix[1], '/')) == NULL) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Missing CGI name");
        return 1;
    }
    *cgiName++ = '\0';

    getcwd(cwd, ME_GOAHEAD_LIMIT_FILENAME);
    dir = wp->route->dir ? wp->route->dir : cwd;
    chdir(dir);
    
    extraPath = 0;
    if ((cp = strchr(cgiName, '/')) != NULL) {
        extraPath = sclone(cp);
        *cp = '\0';
        websSetVar(wp, "PATH_INFO", extraPath);
        websSetVarFmt(wp, "PATH_TRANSLATED", "%s%s%s", dir, cgiPrefix, extraPath);
        wfree(extraPath);
    } else {
        websSetVar(wp, "PATH_INFO", "");
        websSetVar(wp, "PATH_TRANSLATED", "");        
    }
    cgiPath = sfmt("%s%s/%s", dir, cgiPrefix, cgiName);
    websSetVarFmt(wp, "SCRIPT_NAME", "%s/%s", cgiPrefix, cgiName);
    websSetVar(wp, "SCRIPT_FILENAME", cgiPath);
    
/*
    See if the file exists and is executable.  If not error out.  Don't do this step for VxWorks, since the module
    may already be part of the OS image, rather than in the file system.
*/
#if !VXWORKS
    {
        WebsStat sbuf;
        if (stat(cgiPath, &sbuf) != 0 || (sbuf.st_mode & S_IFREG) == 0) {
            error("Cannot find CGI program: ", cgiPath);
            websError(wp, HTTP_CODE_NOT_FOUND | WEBS_NOLOG, "CGI program file does not exist");
            wfree(cgiPath);
            return 1;
        }
#if ME_WIN_LIKE
        if (strstr(cgiPath, ".exe") == NULL && strstr(cgiPath, ".bat") == NULL)
#else
        if (access(cgiPath, X_OK) != 0)
#endif
        {
            websError(wp, HTTP_CODE_NOT_FOUND, "CGI process file is not executable");
            wfree(cgiPath);
            return 1;
        }
    }
#endif /* ! VXWORKS */
    /*
        Build command line arguments.  Only used if there is no non-encoded = character.  This is indicative of a ISINDEX
        query.  POST separators are & and others are +.  argp will point to a walloc'd array of pointers.  Each pointer
        will point to substring within the query string.  This array of string pointers is how the spawn or exec routines
        expect command line arguments to be passed.  Since we don't know ahead of time how many individual items there are
        in the query string, the for loop includes logic to grow the array size via wrealloc.
     */
    argpsize = 10;
    argp = walloc(argpsize * sizeof(char *));
    *argp = cgiPath;
    n = 1;
    query = 0;

    if (strchr(wp->query, '=') == NULL) {
        query = sclone(wp->query);
        websDecodeUrl(query, query, strlen(query));
        for (cp = stok(query, " ", &tok); cp != NULL; ) {
            *(argp+n) = cp;
            trace(5, "ARG[%d] %s", n, argp[n-1]);
            n++;
            if (n >= argpsize) {
                argpsize *= 2;
                argp = wrealloc(argp, argpsize * sizeof(char *));
            }
            cp = stok(NULL, " ", &tok);
        }
    }
    *(argp+n) = NULL;

    /*
        Add all CGI variables to the environment strings to be passed to the spawned CGI process. This includes a few
        we don't already have in the symbol table, plus all those that are in the vars symbol table. envp will point
        to a walloc'd array of pointers. Each pointer will point to a walloc'd string containing the keyword value pair
        in the form keyword=value. Since we don't know ahead of time how many environment strings there will be the for
        loop includes logic to grow the array size via wrealloc.
     */
    envpsize = 64;
    envp = walloc(envpsize * sizeof(char*));
    for (n = 0, s = hashFirst(wp->vars); s != NULL; s = hashNext(wp->vars, s)) {
        if (s->content.valid && s->content.type == string &&
            strcmp(s->name.value.string, "REMOTE_HOST") != 0 &&
            strcmp(s->name.value.string, "HTTP_AUTHORIZATION") != 0) {
            envp[n++] = sfmt("%s=%s", s->name.value.string, s->content.value.string);
            trace(5, "Env[%d] %s", n, envp[n-1]);
            if (n >= envpsize) {
                envpsize *= 2;
                envp = wrealloc(envp, envpsize * sizeof(char *));
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
    if ((pHandle = launchCgi(cgiPath, argp, envp, stdIn, stdOut)) == -1) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "failed to spawn CGI task");
        for (ep = envp; *ep != NULL; ep++) {
            wfree(*ep);
        }
        wfree(cgiPath);
        wfree(argp);
        wfree(envp);
        wfree(stdOut);
        wfree(query);

    } else {
        /*
            If the spawn was successful, put this wp on a queue to be checked for completion.
         */
        cid = wallocObject(&cgiList, &cgiMax, sizeof(Cgi));
        cgip = cgiList[cid];
        cgip->handle = pHandle;
        cgip->stdIn = stdIn;
        cgip->stdOut = stdOut;
        cgip->cgiPath = cgiPath;
        cgip->argp = argp;
        cgip->envp = envp;
        cgip->wp = wp;
        cgip->fplacemark = 0;
        wfree(query);
    }
    /*
        Restore the current working directory after spawning child CGI
     */
    chdir(cwd);
    return 1;
}


PUBLIC int websCgiOpen()
{
    websDefineHandler("cgi", 0, cgiHandler, 0, 0);
    return 0;
}


PUBLIC int websProcessCgiData(Webs *wp)
{
    ssize   nbytes;

    nbytes = bufLen(&wp->input);
    trace(5, "cgi: write %d bytes to CGI program", nbytes);
    if (write(wp->cgifd, wp->input.servp, (int) nbytes) != nbytes) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR| WEBS_CLOSE, "Cannot write to CGI gateway");
        return -1;
    }
    websConsumeInput(wp, nbytes);
    trace(5, "cgi: write %d bytes to CGI program", nbytes);
    return 0;
}


static void writeCgiHeaders(Webs *wp, int status, ssize contentLength, char *location, char *contentType)
{
    trace(5, "cgi: Start response headers");
    websSetStatus(wp, status);
    websWriteHeaders(wp, contentLength, location);
    websWriteHeader(wp, "Pragma", "no-cache");
    websWriteHeader(wp, "Cache-Control", "no-cache");
    if (contentType) {
        websWriteHeader(wp, "Content-Type", contentType);
    }
}


static ssize parseCgiHeaders(Webs *wp, char *buf)
{
    char    *end, *cp, *key, *value, *location, *contentType;
    ssize   len, contentLength;
    int     status, doneHeaders;

    status = HTTP_CODE_OK;
    contentLength = -1;
    contentType = 0;
    location = 0;
    doneHeaders = 0;

    /*
        Look for end of headers
     */
    if ((end = strstr(buf, "\r\n\r\n")) == NULL) {
        if ((end = strstr(buf, "\n\n")) == NULL) {
            return 0;
        }
        len = 2;
    } else {
        len = 4;
    }
    end[len - 1] = '\0';
    end += len;
    cp = buf;
    if (!strchr(cp, ':')) {
        /* No headers found */
        return 0;
    }
    if (strncmp(cp, "HTTP/1.", 7) == 0) {
        ssplit(cp, "\r\n", &cp);
    }
    for (; cp && *cp && (*cp != '\r' && *cp != '\n') && cp < end; ) {
        key = slower(ssplit(cp, ":", &value));
        if (strcmp(key, "location") == 0) {
            location = value;
        } else if (strcmp(key, "status") == 0) {
            status = atoi(value);
        } else if (strcmp(key, "content-type") == 0) {
            contentType = value;
        } else if (strcmp(key, "content-length") == 0) {
            contentLength = atoi(value);
        } else {
            /*
                Now pass all other headers back to the client
             */
            if (!doneHeaders) {
                writeCgiHeaders(wp, status, contentLength, location, contentType);
                doneHeaders = 1;
            }
            if (key && value && !strspn(key, "%<>/\\")) {
                websWriteHeader(wp, key, "%s", value);
            } else {
                trace(5, "cgi: bad response http header: \"%s\": \"%s\"", key, value);
            }
        }
        stok(value, "\r\n", &cp);
    }
    if (!doneHeaders) {
        writeCgiHeaders(wp, status, contentLength, location, contentType);
     }
    websWriteEndHeaders(wp);
    return end - buf;
}


PUBLIC void websCgiGatherOutput(Cgi *cgip)
{
    Webs        *wp;
    WebsStat    sbuf;
    char        buf[ME_GOAHEAD_LIMIT_HEADERS + 2];
    ssize       nbytes, skip;
    int         fdout;

    /*
        OPT - currently polling and doing a stat each poll. Also doing open/close each chunk.
        If the CGI process writes partial headers, this repeatedly reads the data until complete 
        headers are written or more than ME_GOAHEAD_LIMIT_HEADERS of data is received.
     */
    if ((stat(cgip->stdOut, &sbuf) == 0) && (sbuf.st_size > cgip->fplacemark)) {
        if ((fdout = open(cgip->stdOut, O_RDONLY | O_BINARY, 0444)) >= 0) {
            /*
                Check to see if any data is available in the output file and send its contents to the socket.
                Write the HTTP header on our first pass. The header must fit into ME_GOAHEAD_LIMIT_BUFFER.
             */
            wp = cgip->wp;
            lseek(fdout, cgip->fplacemark, SEEK_SET);
            while ((nbytes = read(fdout, buf, sizeof(buf))) > 0) {
                if (!(wp->flags & WEBS_HEADERS_CREATED)) {
                    if ((skip = parseCgiHeaders(wp, buf)) == 0) {
                        if (cgip->handle && sbuf.st_size < ME_GOAHEAD_LIMIT_HEADERS) {
                            trace(5, "cgi: waiting for http headers");
                            break;
                        } else {
                            trace(5, "cgi: missing http headers - create default headers");
                            writeCgiHeaders(wp, HTTP_CODE_OK, -1, 0, 0);
                        }
                    }
                }
                trace(5, "cgi: write %d bytes to client", nbytes - skip);
                websWriteBlock(wp, &buf[skip], nbytes - skip);
                cgip->fplacemark += (off_t) nbytes;
            }
            close(fdout);
        } else {
            trace(5, "cgi: open failed");
        }
    }
}


/*
    Any entry in the cgiList need to be checked to see if it has completed, and if so, process its output and clean up.
    Return time till next poll.
 */
WebsTime websCgiPoll()
{
    Webs    *wp;
    Cgi     *cgip;
    char    **ep;
    int     cid;

    for (cid = 0; cid < cgiMax; cid++) {
        if ((cgip = cgiList[cid]) != NULL) {
            wp = cgip->wp;
            websCgiGatherOutput(cgip);
            if (checkCgi(cgip->handle) == 0) {
                /*
                    We get here if the CGI process has terminated. Clean up.
                 */
                cgip->handle = 0;
                websCgiGatherOutput(cgip);

#if WINDOWS
                /*                  
                     Windows can have delayed notification through the file system after process exit.
                 */
                {
                    int nTries;
                    for (nTries = 0; (cgip->fplacemark == 0) && (nTries < 100); nTries++) {
                        websCgiGatherOutput(cgip);
                        if (cgip->fplacemark == 0) {
                            Sleep(10);
                        }
                    }
                }
#endif
                if (cgip->fplacemark == 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "CGI generated no output");
                } else {
                    trace(5, "cgi: Request complete - calling websDone");
                    websDone(wp);
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
                cgiMax = wfreeHandle(&cgiList, cid);
                for (ep = cgip->envp; ep != NULL && *ep != NULL; ep++) {
                    wfree(*ep);
                }
                wfree(cgip->cgiPath);
                wfree(cgip->argp);
                wfree(cgip->envp);
                wfree(cgip->stdOut);
                wfree(cgip);
                websPump(wp);
                websFree(wp);
                /* wp no longer valid */
            }
        }
    }
    return cgiMax ? 10 : MAXINT;
}


/*
    Returns a pointer to an allocated qualified unique temporary file name. This filename must eventually be deleted with wfree().  
 */
PUBLIC char *websGetCgiCommName()
{
    return sclone(websTempFile(NULL, "cgi"));
}


#if WINCE
/*
     Launch the CGI process and return a handle to it.  CE note: This function is not complete.  The missing piece is
     the ability to redirect stdout.
 */
static int launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
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
static int checkCgi(int handle)
{
    int     nReturn;
    DWORD   exitCode;

    nReturn = GetExitCodeProcess((HANDLE)handle, &exitCode);
    /*
        We must close process handle to free up the window resource, but only when we're done with it.
     */
    if ((nReturn == 0) || (exitCode != STILL_ACTIVE)) {
        CloseHandle((HANDLE) handle);
        return 0;
    }
    return 1;
}
#endif /* WINCE */


#if ME_UNIX_LIKE || QNX
/*
    Launch the CGI process and return a handle to it.
 */
static int launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
{
    int pid, fdin, fdout, hstdin, hstdout;

    trace(5, "cgi: run %s", cgiPath);
    pid = fdin = fdout = hstdin = hstdout = -1;
    if ((fdin = open(stdIn, O_RDWR | O_CREAT | O_BINARY, 0666)) < 0 ||
            (fdout = open(stdOut, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666)) < 0 ||
            (hstdin = dup(0)) == -1 || (hstdout = dup(1)) == -1 ||
            dup2(fdin, 0) == -1 || dup2(fdout, 1) == -1) {
        goto done;
    }

    pid = vfork();
    if (pid == 0) {
        /*
            if pid == 0, then we are in the child process
         */
        if (execve(cgiPath, argp, envp) == -1) {
            printf("content-type: text/html\n\nExecution of cgi process failed\n");
        }
        _exit(0);
    }
done:
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
    return pid;
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
static int checkCgi(int handle)
{
    int     pid;
    
    /*
        Check to see if the CGI child process has terminated or not yet.
     */
    if ((pid = waitpid(handle, NULL, WNOHANG)) == handle) {
        trace(5, "cgi: waited for pid %d", pid);
        return 0;
    } else {
        return 1;
    }
}

#endif /* LINUX || LYNX || MACOSX || QNX4 */


#if VXWORKS
static void vxWebsCgiEntry(void *entryAddr(int argc, char **argv), char **argv, char **envp, char *stdIn, char *stdOut); 
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
static int launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
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
    basename = sclone(p);
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
            pEntry = sclone(*pp + 9);
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
        wfree(pname);
    }
    if (entryAddr != 0) {
        rc = taskSpawn(pEntry, priority, 0, 20000, (void*) vxWebsCgiEntry, (int) entryAddr, (int) argp, 
            (int) envp, (int) stdIn, (int) stdOut, 0, 0, 0, 0, 0);
        goto done;
    }

    /*
        Try to load the module.
     */
    if ((fd = open(cgiPath, O_RDONLY | O_BINARY, 0666)) < 0 ||
        loadModule(fd, LOAD_GLOBAL_SYMBOLS) == NULL) {
        goto done;
    }
    if ((symFindByName(sysSymTbl, pEntry, &entryAddr, &ptype)) == -1) {
        pname = sfmt("_%s", pEntry);
        symFindByName(sysSymTbl, pname, &entryAddr, &ptype);
        wfree(pname);
    }
    if (entryAddr != 0) {
        rc = taskSpawn(pEntry, priority, 0, 20000, (void*) vxWebsCgiEntry, (int) entryAddr, (int) argp, 
            (int) envp, (int) stdIn, (int) stdOut, 0, 0, 0, 0, 0);
    }
done:
    if (fd != -1) {
        close(fd);
    }
    wfree(basename);
    wfree(pEntry);
    return rc;
}


/*
    This is the CGI process wrapper.  It will open and redirect stdin and stdout to stdIn and stdOut.  It converts argv
    to an argc, argv pair to pass to the user entry. It initializes the task environment with envp strings. Then it
    will call the user entry.
 */
static void vxWebsCgiEntry(void *entryAddr(int argc, char **argv), char **argp, char **envp, char *stdIn, char *stdOut)
{
    char    **p;
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
static int checkCgi(int handle)
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
    strings, terminated in a null pointer.  Returns the address of a block of memory allocated using the walloc()
    function.  The returned pointer must be deleted using wfree().  Returns NULL on error.
 */
static uchar *tableToBlock(char **table)
{
    uchar   *pBlock;        /*  Allocated block */
    char    *pEntry;        /*  Pointer into block */
    size_t  sizeBlock;      /*  Size of table */
    int     index;          /*  Index into string table */

    assert(table);

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
    pBlock = walloc(sizeBlock);

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
    Create a temporary stdout file and launch the CGI process. Returns a handle to the spawned CGI process.
 */
static int launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
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
    cmdLine = walloc(sizeof(char) * nLen);
    assert(cmdLine);
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
    wfree(pEnvData);
    wfree(cmdLine);

    if (bReturn == 0) {
        return -1;
    } else {
        return (int) procinfo.hProcess;
    }
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
static int checkCgi(int handle)
{
    DWORD   exitCode;
    int     nReturn;

    nReturn = GetExitCodeProcess((HANDLE) handle, &exitCode);
    /*
        We must close process handle to free up the window resource, but only
        when we're done with it.
     */
    if ((nReturn == 0) || (exitCode != STILL_ACTIVE)) {
        CloseHandle((HANDLE) handle);
        return 0;
    }
    return 1;
}
#endif /* WIN */
#endif /* ME_GOAHEAD_CGI && !ME_ROM */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/crypt.c ************/


/*
    crypt.c - Base-64 encoding and decoding and MD5 support.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/*********************************** Locals ***********************************/
/*
    Constants for transform routine
 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static uchar PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
   F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/*
   ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/*
     FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
     Rotation is separate from addition to prevent recomputation.
 */
 
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (uint)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (uint)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (uint)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (uint)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

typedef struct {
    uint state[4];
    uint count[2];
    uchar buffer[64];
} MD5CONTEXT;

/******************************* Base 64 Data *********************************/

#define CRYPT_HASH_SIZE   16

/*
    Encoding map lookup
 */
static char encodeMap[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/',
};


/*
    Decode map
 */
static signed char decodeMap[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, 
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

/*************************** Forward Declarations *****************************/

static void decode(uint *output, uchar *input, uint len);
static void encode(uchar *output, uint *input, uint len);
static void finalizeMD5(uchar digest[16], MD5CONTEXT *context);
static void initMD5(MD5CONTEXT *context);
static void transform(uint state[4], uchar block[64]);
static void update(MD5CONTEXT *context, uchar *input, uint inputLen);

/*********************************** Code *************************************/
/*
    Decode a null terminated string and returns a null terminated string.
    Stops decoding at the end of string or '='
 */
PUBLIC char *websDecode64(char *s)
{
    return websDecode64Block(s, NULL, WEBS_DECODE_TOKEQ);
}


/*
    Decode a null terminated string and return a block with length.
    Stops decoding at the end of the block or '=' if WEBS_DECODE_TOKEQ is specified.
 */
PUBLIC char *websDecode64Block(char *s, ssize *len, int flags)
{
    uint    bitBuf;
    char  *buffer, *bp;
    char  *end;
    ssize   size;
    int     c, i, j, shift;

    size = strlen(s);
    if ((buffer = walloc(size + 1)) == 0) {
        return NULL;
    }
    bp = buffer;
    *bp = '\0';
    end = &s[size];
    while (s < end && (*s != '=' || !(flags & WEBS_DECODE_TOKEQ))) {
        bitBuf = 0;
        shift = 18;
        for (i = 0; i < 4 && (s < end && (*s != '=' || !(flags & WEBS_DECODE_TOKEQ))); i++, s++) {
            c = decodeMap[*s & 0xff];
            if (c == -1) {
                return NULL;
            } 
            bitBuf = bitBuf | (c << shift);
            shift -= 6;
        }
        --i;
        assert((bp + i) < &buffer[size]);
        for (j = 0; j < i; j++) {
            *bp++ = (char) ((bitBuf >> (8 * (2 - j))) & 0xff);
        }
        *bp = '\0';
    }
    if (len) {
        *len = bp - buffer;
    }
    return buffer;
}


PUBLIC char *websMD5(char *s)
{
    return websMD5Block(s, strlen(s), NULL);
}


/*
    Return the MD5 hash of a block. Returns allocated string. A prefix for the result can be supplied.
 */
PUBLIC char *websMD5Block(char *buf, ssize length, char *prefix)
{
    MD5CONTEXT      context;
    uchar           hash[CRYPT_HASH_SIZE];
    cchar           *hex = "0123456789abcdef";
    char            *r, *str;
    char            result[(CRYPT_HASH_SIZE * 2) + 1];
    ssize           len;
    int             i;

    if (length < 0) {
        length = strlen(buf);
    }
    initMD5(&context);
    update(&context, (uchar*) buf, (uint) length);
    finalizeMD5(hash, &context);

    for (i = 0, r = result; i < 16; i++) {
        *r++ = hex[hash[i] >> 4];
        *r++ = hex[hash[i] & 0xF];
    }
    *r = '\0';
    len = (prefix) ? strlen(prefix) : 0;
    str = walloc(sizeof(result) + len);
    if (str) {
        if (prefix) {
            strcpy(str, prefix);
        }
        strcpy(str + len, result);
    }
    return str;
}


/*
    MD5 initialization. Begins an MD5 operation, writing a new context.
 */ 
static void initMD5(MD5CONTEXT *context)
{
    context->count[0] = context->count[1] = 0;
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}


/*
    MD5 block update operation. Continues an MD5 message-digest operation, processing another message block, 
    and updating the context.
 */
static void update(MD5CONTEXT *context, uchar *input, uint inputLen)
{
    uint    i, index, partLen;

    index = (uint) ((context->count[0] >> 3) & 0x3F);

    if ((context->count[0] += ((uint)inputLen << 3)) < ((uint)inputLen << 3)){
        context->count[1]++;
    }
    context->count[1] += ((uint)inputLen >> 29);
    partLen = 64 - index;

    if (inputLen >= partLen) {
        memcpy((uchar*) &context->buffer[index], (uchar*) input, partLen);
        transform(context->state, context->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64) {
            transform(context->state, &input[i]);
        }
        index = 0;
    } else {
        i = 0;
    }
    memcpy((uchar*) &context->buffer[index], (uchar*) &input[i], inputLen-i);
}


/*
    MD5 finalization. Ends an MD5 message-digest operation, writing the message digest and zeroizing the context.
 */ 
static void finalizeMD5(uchar digest[16], MD5CONTEXT *context)
{
    uchar   bits[8];
    uint    index, padLen;

    /* Save number of bits */
    encode(bits, context->count, 8);

    /* Pad out to 56 mod 64. */
    index = (uint)((context->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    update(context, PADDING, padLen);

    /* Append length (before padding) */
    update(context, bits, 8);
    /* Store state in digest */
    encode(digest, context->state, 16);

    /* Zero sensitive information. */
    memset((uchar*)context, 0, sizeof (*context));
}


/*
    MD5 basic transformation. Transforms state based on block.
 */
static void transform(uint state[4], uchar block[64])
{
    uint a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    decode(x, block, 64);

    /* Round 1 */
    FF(a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
    FF(d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
    FF(c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
    FF(b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
    FF(a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
    FF(d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
    FF(c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
    FF(b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
    FF(a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
    FF(d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
    FF(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF(b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG(a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
    GG(d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
    GG(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG(b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
    GG(a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
    GG(d, a, b, c, x[10], S22,  0x2441453); /* 22 */
    GG(c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    GG(b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
    GG(a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
    GG(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG(c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
    GG(b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
    GG(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG(d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
    GG(c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH(a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
    HH(d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
    HH(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH(a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
    HH(d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
    HH(c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
    HH(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH(d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
    HH(c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
    HH(b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
    HH(a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
    HH(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    HH(b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II(a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
    II(d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
    II(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II(b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
    II(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II(d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
    II(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II(b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
    II(a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II(c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
    II(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II(a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
    II(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II(c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
    II(b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    /* Zero sensitive information. */
    memset((uchar*) x, 0, sizeof(x));
}


/*
    Encodes input(uint) into output(uchar). Assumes len is a multiple of 4.
 */
static void encode(uchar *output, uint *input, uint len)
{
    uint i, j;

    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j] = (uchar) (input[i] & 0xff);
        output[j+1] = (uchar) ((input[i] >> 8) & 0xff);
        output[j+2] = (uchar) ((input[i] >> 16) & 0xff);
        output[j+3] = (uchar) ((input[i] >> 24) & 0xff);
    }
}


/*
    Decodes input(uchar) into output(uint). Assumes len is a multiple of 4.
 */
static void decode(uint *output, uchar *input, uint len)
{
    uint    i, j;

    for (i = 0, j = 0; j < len; i++, j += 4)
        output[i] = ((uint) input[j]) | (((uint) input[j+1]) << 8) | (((uint) input[j+2]) << 16) | 
            (((uint) input[j+3]) << 24);
}


/*
    Encode a null terminated string.
    Returns a null terminated block
 */
PUBLIC char *websEncode64(char *s)
{
    return websEncode64Block(s, slen(s));
}


/*
    Encode a block of a given length
    Returns a null terminated block
 */
PUBLIC char *websEncode64Block(char *s, ssize len)
{
    uint    shiftbuf;
    char    *buffer, *bp;
    cchar   *end;
    ssize   size;
    int     i, j, shift;

    size = len * 2;
    if ((buffer = walloc(size + 1)) == 0) {
        return NULL;
    }
    bp = buffer;
    *bp = '\0';
    end = &s[len];
    while (s < end) {
        shiftbuf = 0;
        for (j = 2; j >= 0 && *s; j--, s++) {
            shiftbuf |= ((*s & 0xff) << (j * 8));
        }
        shift = 18;
        for (i = ++j; i < 4 && bp < &buffer[size] ; i++) {
            *bp++ = encodeMap[(shiftbuf >> shift) & 0x3f];
            shift -= 6;
        }
        while (j-- > 0) {
            *bp++ = '=';
        }
        *bp = '\0';
    }
    return buffer;
}

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/est/estLib.c ************/


/*
 * Embethis EST Library Source
 */

#include "est.h"

#if ME_COM_EST



/********* Start of file src/aes.c ************/


/*
    aes.c -- FIPS-197 compliant AES implementation

    The AES block cipher was designed by Vincent Rijmen and Joan Daemen.

    http://csrc.nist.gov/encryption/aes/rijndael/Rijndael.pdf
    http://csrc.nist.gov/publications/fips/fips197/fips-197.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_AES

/*
    32-bit integer manipulation macros (little endian)
 */
#ifndef GET_ULONG_LE
#define GET_ULONG_LE(n,b,i)                     \
{                                               \
    (n) = ( (ulong) (b)[(i)    ]       )        \
        | ( (ulong) (b)[(i) + 1] <<  8 )        \
        | ( (ulong) (b)[(i) + 2] << 16 )        \
        | ( (ulong) (b)[(i) + 3] << 24 );       \
}
#endif

#ifndef PUT_ULONG_LE
#define PUT_ULONG_LE(n,b,i)                     \
{                                               \
    (b)[(i)    ] = (uchar) ( (n)       );       \
    (b)[(i) + 1] = (uchar) ( (n) >>  8 );       \
    (b)[(i) + 2] = (uchar) ( (n) >> 16 );       \
    (b)[(i) + 3] = (uchar) ( (n) >> 24 );       \
}
#endif

#define FSb AESFSb

#if ME_EST_ROM_TABLES
/*
   Forward S-box
 */
static const uchar FSb[256] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5,
    0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0,
    0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC,
    0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A,
    0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0,
    0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B,
    0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85,
    0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5,
    0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17,
    0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88,
    0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C,
    0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9,
    0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6,
    0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E,
    0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94,
    0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68,
    0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
};


/*
   Forward tables
 */
#define FT \
    V(A5,63,63,C6), V(84,7C,7C,F8), V(99,77,77,EE), V(8D,7B,7B,F6), \
    V(0D,F2,F2,FF), V(BD,6B,6B,D6), V(B1,6F,6F,DE), V(54,C5,C5,91), \
    V(50,30,30,60), V(03,01,01,02), V(A9,67,67,CE), V(7D,2B,2B,56), \
    V(19,FE,FE,E7), V(62,D7,D7,B5), V(E6,AB,AB,4D), V(9A,76,76,EC), \
    V(45,CA,CA,8F), V(9D,82,82,1F), V(40,C9,C9,89), V(87,7D,7D,FA), \
    V(15,FA,FA,EF), V(EB,59,59,B2), V(C9,47,47,8E), V(0B,F0,F0,FB), \
    V(EC,AD,AD,41), V(67,D4,D4,B3), V(FD,A2,A2,5F), V(EA,AF,AF,45), \
    V(BF,9C,9C,23), V(F7,A4,A4,53), V(96,72,72,E4), V(5B,C0,C0,9B), \
    V(C2,B7,B7,75), V(1C,FD,FD,E1), V(AE,93,93,3D), V(6A,26,26,4C), \
    V(5A,36,36,6C), V(41,3F,3F,7E), V(02,F7,F7,F5), V(4F,CC,CC,83), \
    V(5C,34,34,68), V(F4,A5,A5,51), V(34,E5,E5,D1), V(08,F1,F1,F9), \
    V(93,71,71,E2), V(73,D8,D8,AB), V(53,31,31,62), V(3F,15,15,2A), \
    V(0C,04,04,08), V(52,C7,C7,95), V(65,23,23,46), V(5E,C3,C3,9D), \
    V(28,18,18,30), V(A1,96,96,37), V(0F,05,05,0A), V(B5,9A,9A,2F), \
    V(09,07,07,0E), V(36,12,12,24), V(9B,80,80,1B), V(3D,E2,E2,DF), \
    V(26,EB,EB,CD), V(69,27,27,4E), V(CD,B2,B2,7F), V(9F,75,75,EA), \
    V(1B,09,09,12), V(9E,83,83,1D), V(74,2C,2C,58), V(2E,1A,1A,34), \
    V(2D,1B,1B,36), V(B2,6E,6E,DC), V(EE,5A,5A,B4), V(FB,A0,A0,5B), \
    V(F6,52,52,A4), V(4D,3B,3B,76), V(61,D6,D6,B7), V(CE,B3,B3,7D), \
    V(7B,29,29,52), V(3E,E3,E3,DD), V(71,2F,2F,5E), V(97,84,84,13), \
    V(F5,53,53,A6), V(68,D1,D1,B9), V(00,00,00,00), V(2C,ED,ED,C1), \
    V(60,20,20,40), V(1F,FC,FC,E3), V(C8,B1,B1,79), V(ED,5B,5B,B6), \
    V(BE,6A,6A,D4), V(46,CB,CB,8D), V(D9,BE,BE,67), V(4B,39,39,72), \
    V(DE,4A,4A,94), V(D4,4C,4C,98), V(E8,58,58,B0), V(4A,CF,CF,85), \
    V(6B,D0,D0,BB), V(2A,EF,EF,C5), V(E5,AA,AA,4F), V(16,FB,FB,ED), \
    V(C5,43,43,86), V(D7,4D,4D,9A), V(55,33,33,66), V(94,85,85,11), \
    V(CF,45,45,8A), V(10,F9,F9,E9), V(06,02,02,04), V(81,7F,7F,FE), \
    V(F0,50,50,A0), V(44,3C,3C,78), V(BA,9F,9F,25), V(E3,A8,A8,4B), \
    V(F3,51,51,A2), V(FE,A3,A3,5D), V(C0,40,40,80), V(8A,8F,8F,05), \
    V(AD,92,92,3F), V(BC,9D,9D,21), V(48,38,38,70), V(04,F5,F5,F1), \
    V(DF,BC,BC,63), V(C1,B6,B6,77), V(75,DA,DA,AF), V(63,21,21,42), \
    V(30,10,10,20), V(1A,FF,FF,E5), V(0E,F3,F3,FD), V(6D,D2,D2,BF), \
    V(4C,CD,CD,81), V(14,0C,0C,18), V(35,13,13,26), V(2F,EC,EC,C3), \
    V(E1,5F,5F,BE), V(A2,97,97,35), V(CC,44,44,88), V(39,17,17,2E), \
    V(57,C4,C4,93), V(F2,A7,A7,55), V(82,7E,7E,FC), V(47,3D,3D,7A), \
    V(AC,64,64,C8), V(E7,5D,5D,BA), V(2B,19,19,32), V(95,73,73,E6), \
    V(A0,60,60,C0), V(98,81,81,19), V(D1,4F,4F,9E), V(7F,DC,DC,A3), \
    V(66,22,22,44), V(7E,2A,2A,54), V(AB,90,90,3B), V(83,88,88,0B), \
    V(CA,46,46,8C), V(29,EE,EE,C7), V(D3,B8,B8,6B), V(3C,14,14,28), \
    V(79,DE,DE,A7), V(E2,5E,5E,BC), V(1D,0B,0B,16), V(76,DB,DB,AD), \
    V(3B,E0,E0,DB), V(56,32,32,64), V(4E,3A,3A,74), V(1E,0A,0A,14), \
    V(DB,49,49,92), V(0A,06,06,0C), V(6C,24,24,48), V(E4,5C,5C,B8), \
    V(5D,C2,C2,9F), V(6E,D3,D3,BD), V(EF,AC,AC,43), V(A6,62,62,C4), \
    V(A8,91,91,39), V(A4,95,95,31), V(37,E4,E4,D3), V(8B,79,79,F2), \
    V(32,E7,E7,D5), V(43,C8,C8,8B), V(59,37,37,6E), V(B7,6D,6D,DA), \
    V(8C,8D,8D,01), V(64,D5,D5,B1), V(D2,4E,4E,9C), V(E0,A9,A9,49), \
    V(B4,6C,6C,D8), V(FA,56,56,AC), V(07,F4,F4,F3), V(25,EA,EA,CF), \
    V(AF,65,65,CA), V(8E,7A,7A,F4), V(E9,AE,AE,47), V(18,08,08,10), \
    V(D5,BA,BA,6F), V(88,78,78,F0), V(6F,25,25,4A), V(72,2E,2E,5C), \
    V(24,1C,1C,38), V(F1,A6,A6,57), V(C7,B4,B4,73), V(51,C6,C6,97), \
    V(23,E8,E8,CB), V(7C,DD,DD,A1), V(9C,74,74,E8), V(21,1F,1F,3E), \
    V(DD,4B,4B,96), V(DC,BD,BD,61), V(86,8B,8B,0D), V(85,8A,8A,0F), \
    V(90,70,70,E0), V(42,3E,3E,7C), V(C4,B5,B5,71), V(AA,66,66,CC), \
    V(D8,48,48,90), V(05,03,03,06), V(01,F6,F6,F7), V(12,0E,0E,1C), \
    V(A3,61,61,C2), V(5F,35,35,6A), V(F9,57,57,AE), V(D0,B9,B9,69), \
    V(91,86,86,17), V(58,C1,C1,99), V(27,1D,1D,3A), V(B9,9E,9E,27), \
    V(38,E1,E1,D9), V(13,F8,F8,EB), V(B3,98,98,2B), V(33,11,11,22), \
    V(BB,69,69,D2), V(70,D9,D9,A9), V(89,8E,8E,07), V(A7,94,94,33), \
    V(B6,9B,9B,2D), V(22,1E,1E,3C), V(92,87,87,15), V(20,E9,E9,C9), \
    V(49,CE,CE,87), V(FF,55,55,AA), V(78,28,28,50), V(7A,DF,DF,A5), \
    V(8F,8C,8C,03), V(F8,A1,A1,59), V(80,89,89,09), V(17,0D,0D,1A), \
    V(DA,BF,BF,65), V(31,E6,E6,D7), V(C6,42,42,84), V(B8,68,68,D0), \
    V(C3,41,41,82), V(B0,99,99,29), V(77,2D,2D,5A), V(11,0F,0F,1E), \
    V(CB,B0,B0,7B), V(FC,54,54,A8), V(D6,BB,BB,6D), V(3A,16,16,2C)

#define V(a,b,c,d) 0x##a##b##c##d
static const ulong FT0[256] = { FT };

#undef V
#define V(a,b,c,d) 0x##b##c##d##a
static const ulong FT1[256] = { FT };

#undef V
#define V(a,b,c,d) 0x##c##d##a##b
static const ulong FT2[256] = { FT };

#undef V
#define V(a,b,c,d) 0x##d##a##b##c
static const ulong FT3[256] = { FT };

#undef V
#undef FT

/*
   Reverse S-box
 */
static const uchar RSb[256] = {
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38,
    0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB,
    0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87,
    0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB,
    0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D,
    0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
    0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2,
    0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25,
    0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92,
    0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA,
    0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
    0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A,
    0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
    0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02,
    0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B,
    0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA,
    0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
    0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85,
    0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E,
    0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89,
    0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B,
    0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20,
    0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
    0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31,
    0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F,
    0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D,
    0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
    0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0,
    0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26,
    0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D
};

/*
   Reverse tables
 */
#define RT \
    V(50,A7,F4,51), V(53,65,41,7E), V(C3,A4,17,1A), V(96,5E,27,3A), \
    V(CB,6B,AB,3B), V(F1,45,9D,1F), V(AB,58,FA,AC), V(93,03,E3,4B), \
    V(55,FA,30,20), V(F6,6D,76,AD), V(91,76,CC,88), V(25,4C,02,F5), \
    V(FC,D7,E5,4F), V(D7,CB,2A,C5), V(80,44,35,26), V(8F,A3,62,B5), \
    V(49,5A,B1,DE), V(67,1B,BA,25), V(98,0E,EA,45), V(E1,C0,FE,5D), \
    V(02,75,2F,C3), V(12,F0,4C,81), V(A3,97,46,8D), V(C6,F9,D3,6B), \
    V(E7,5F,8F,03), V(95,9C,92,15), V(EB,7A,6D,BF), V(DA,59,52,95), \
    V(2D,83,BE,D4), V(D3,21,74,58), V(29,69,E0,49), V(44,C8,C9,8E), \
    V(6A,89,C2,75), V(78,79,8E,F4), V(6B,3E,58,99), V(DD,71,B9,27), \
    V(B6,4F,E1,BE), V(17,AD,88,F0), V(66,AC,20,C9), V(B4,3A,CE,7D), \
    V(18,4A,DF,63), V(82,31,1A,E5), V(60,33,51,97), V(45,7F,53,62), \
    V(E0,77,64,B1), V(84,AE,6B,BB), V(1C,A0,81,FE), V(94,2B,08,F9), \
    V(58,68,48,70), V(19,FD,45,8F), V(87,6C,DE,94), V(B7,F8,7B,52), \
    V(23,D3,73,AB), V(E2,02,4B,72), V(57,8F,1F,E3), V(2A,AB,55,66), \
    V(07,28,EB,B2), V(03,C2,B5,2F), V(9A,7B,C5,86), V(A5,08,37,D3), \
    V(F2,87,28,30), V(B2,A5,BF,23), V(BA,6A,03,02), V(5C,82,16,ED), \
    V(2B,1C,CF,8A), V(92,B4,79,A7), V(F0,F2,07,F3), V(A1,E2,69,4E), \
    V(CD,F4,DA,65), V(D5,BE,05,06), V(1F,62,34,D1), V(8A,FE,A6,C4), \
    V(9D,53,2E,34), V(A0,55,F3,A2), V(32,E1,8A,05), V(75,EB,F6,A4), \
    V(39,EC,83,0B), V(AA,EF,60,40), V(06,9F,71,5E), V(51,10,6E,BD), \
    V(F9,8A,21,3E), V(3D,06,DD,96), V(AE,05,3E,DD), V(46,BD,E6,4D), \
    V(B5,8D,54,91), V(05,5D,C4,71), V(6F,D4,06,04), V(FF,15,50,60), \
    V(24,FB,98,19), V(97,E9,BD,D6), V(CC,43,40,89), V(77,9E,D9,67), \
    V(BD,42,E8,B0), V(88,8B,89,07), V(38,5B,19,E7), V(DB,EE,C8,79), \
    V(47,0A,7C,A1), V(E9,0F,42,7C), V(C9,1E,84,F8), V(00,00,00,00), \
    V(83,86,80,09), V(48,ED,2B,32), V(AC,70,11,1E), V(4E,72,5A,6C), \
    V(FB,FF,0E,FD), V(56,38,85,0F), V(1E,D5,AE,3D), V(27,39,2D,36), \
    V(64,D9,0F,0A), V(21,A6,5C,68), V(D1,54,5B,9B), V(3A,2E,36,24), \
    V(B1,67,0A,0C), V(0F,E7,57,93), V(D2,96,EE,B4), V(9E,91,9B,1B), \
    V(4F,C5,C0,80), V(A2,20,DC,61), V(69,4B,77,5A), V(16,1A,12,1C), \
    V(0A,BA,93,E2), V(E5,2A,A0,C0), V(43,E0,22,3C), V(1D,17,1B,12), \
    V(0B,0D,09,0E), V(AD,C7,8B,F2), V(B9,A8,B6,2D), V(C8,A9,1E,14), \
    V(85,19,F1,57), V(4C,07,75,AF), V(BB,DD,99,EE), V(FD,60,7F,A3), \
    V(9F,26,01,F7), V(BC,F5,72,5C), V(C5,3B,66,44), V(34,7E,FB,5B), \
    V(76,29,43,8B), V(DC,C6,23,CB), V(68,FC,ED,B6), V(63,F1,E4,B8), \
    V(CA,DC,31,D7), V(10,85,63,42), V(40,22,97,13), V(20,11,C6,84), \
    V(7D,24,4A,85), V(F8,3D,BB,D2), V(11,32,F9,AE), V(6D,A1,29,C7), \
    V(4B,2F,9E,1D), V(F3,30,B2,DC), V(EC,52,86,0D), V(D0,E3,C1,77), \
    V(6C,16,B3,2B), V(99,B9,70,A9), V(FA,48,94,11), V(22,64,E9,47), \
    V(C4,8C,FC,A8), V(1A,3F,F0,A0), V(D8,2C,7D,56), V(EF,90,33,22), \
    V(C7,4E,49,87), V(C1,D1,38,D9), V(FE,A2,CA,8C), V(36,0B,D4,98), \
    V(CF,81,F5,A6), V(28,DE,7A,A5), V(26,8E,B7,DA), V(A4,BF,AD,3F), \
    V(E4,9D,3A,2C), V(0D,92,78,50), V(9B,CC,5F,6A), V(62,46,7E,54), \
    V(C2,13,8D,F6), V(E8,B8,D8,90), V(5E,F7,39,2E), V(F5,AF,C3,82), \
    V(BE,80,5D,9F), V(7C,93,D0,69), V(A9,2D,D5,6F), V(B3,12,25,CF), \
    V(3B,99,AC,C8), V(A7,7D,18,10), V(6E,63,9C,E8), V(7B,BB,3B,DB), \
    V(09,78,26,CD), V(F4,18,59,6E), V(01,B7,9A,EC), V(A8,9A,4F,83), \
    V(65,6E,95,E6), V(7E,E6,FF,AA), V(08,CF,BC,21), V(E6,E8,15,EF), \
    V(D9,9B,E7,BA), V(CE,36,6F,4A), V(D4,09,9F,EA), V(D6,7C,B0,29), \
    V(AF,B2,A4,31), V(31,23,3F,2A), V(30,94,A5,C6), V(C0,66,A2,35), \
    V(37,BC,4E,74), V(A6,CA,82,FC), V(B0,D0,90,E0), V(15,D8,A7,33), \
    V(4A,98,04,F1), V(F7,DA,EC,41), V(0E,50,CD,7F), V(2F,F6,91,17), \
    V(8D,D6,4D,76), V(4D,B0,EF,43), V(54,4D,AA,CC), V(DF,04,96,E4), \
    V(E3,B5,D1,9E), V(1B,88,6A,4C), V(B8,1F,2C,C1), V(7F,51,65,46), \
    V(04,EA,5E,9D), V(5D,35,8C,01), V(73,74,87,FA), V(2E,41,0B,FB), \
    V(5A,1D,67,B3), V(52,D2,DB,92), V(33,56,10,E9), V(13,47,D6,6D), \
    V(8C,61,D7,9A), V(7A,0C,A1,37), V(8E,14,F8,59), V(89,3C,13,EB), \
    V(EE,27,A9,CE), V(35,C9,61,B7), V(ED,E5,1C,E1), V(3C,B1,47,7A), \
    V(59,DF,D2,9C), V(3F,73,F2,55), V(79,CE,14,18), V(BF,37,C7,73), \
    V(EA,CD,F7,53), V(5B,AA,FD,5F), V(14,6F,3D,DF), V(86,DB,44,78), \
    V(81,F3,AF,CA), V(3E,C4,68,B9), V(2C,34,24,38), V(5F,40,A3,C2), \
    V(72,C3,1D,16), V(0C,25,E2,BC), V(8B,49,3C,28), V(41,95,0D,FF), \
    V(71,01,A8,39), V(DE,B3,0C,08), V(9C,E4,B4,D8), V(90,C1,56,64), \
    V(61,84,CB,7B), V(70,B6,32,D5), V(74,5C,6C,48), V(42,57,B8,D0)

#define V(a,b,c,d) 0x##a##b##c##d
static const ulong RT0[256] = { RT };

#undef V
#define V(a,b,c,d) 0x##b##c##d##a
static const ulong RT1[256] = { RT };

#undef V
#define V(a,b,c,d) 0x##c##d##a##b
static const ulong RT2[256] = { RT };

#undef V
#define V(a,b,c,d) 0x##d##a##b##c
static const ulong RT3[256] = { RT };

#undef V
#undef RT

/*
   Round constants
 */
static const ulong RCON[10] = {
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x0000001B, 0x00000036
};

#else

/*
   Forward S-box & tables
 */
static uchar FSb[256];
static ulong FT0[256];
static ulong FT1[256];
static ulong FT2[256];
static ulong FT3[256];

/*
   Reverse S-box & tables
 */
static uchar RSb[256];
static ulong RT0[256];
static ulong RT1[256];
static ulong RT2[256];
static ulong RT3[256];

/*
   Round constants
 */
static ulong RCON[10];

/*
   Tables generation code
 */
#define ROTL8(x) ((x << 8) & 0xFFFFFFFF) | (x >> 24)
#define XTIME(x) ((x << 1) ^ ((x & 0x80) ? 0x1B : 0x00))
#define MUL(x,y) ((x && y) ? pow[(log[x]+log[y]) % 255] : 0)

static int aes_init_done = 0;

static void aes_gen_tables(void)
{
    int i, x, y, z;
    int pow[256];
    int log[256];

    /*
       Compute pow and log tables over GF(2^8)
     */
    for (i = 0, x = 1; i < 256; i++) {
        pow[i] = x;
        log[x] = i;
        x = (x ^ XTIME(x)) & 0xFF;
    }
    /*
        Calculate the round constants
     */
    for (i = 0, x = 1; i < 10; i++) {
        RCON[i] = (ulong)x;
        x = XTIME(x) & 0xFF;
    }
    /*
        Generate the forward and reverse S-boxes
     */
    FSb[0x00] = 0x63;
    RSb[0x63] = 0x00;

    for (i = 1; i < 256; i++) {
        x = pow[255 - log[i]];
        y = x;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y ^ 0x63;
        FSb[i] = (uchar)x;
        RSb[x] = (uchar)i;
    }

    /*
       generate the forward and reverse tables
     */
    for (i = 0; i < 256; i++) {
        x = FSb[i];
        y = XTIME(x) & 0xFF;
        z = (y ^ x) & 0xFF;

        FT0[i] = ((ulong)y) ^ ((ulong)x << 8) ^ ((ulong)x << 16) ^ ((ulong)z << 24);
        FT1[i] = ROTL8(FT0[i]);
        FT2[i] = ROTL8(FT1[i]);
        FT3[i] = ROTL8(FT2[i]);

        x = RSb[i];
        RT0[i] = ((ulong)MUL(0x0E, x)) ^ ((ulong)MUL(0x09, x) << 8) ^ ((ulong)MUL(0x0D, x) << 16) ^
            ((ulong)MUL(0x0B, x) << 24);

        RT1[i] = ROTL8(RT0[i]);
        RT2[i] = ROTL8(RT1[i]);
        RT3[i] = ROTL8(RT2[i]);
    }
}

#endif

/*
   AES key schedule (encryption)
 */
void aes_setkey_enc(aes_context * ctx, uchar *key, int keysize)
{
    int i;
    ulong *RK;

#if !ME_EST_ROM_TABLES
    if (aes_init_done == 0) {
        aes_gen_tables();
        aes_init_done = 1;
    }
#endif

    switch (keysize) {
    case 128:
        ctx->nr = 10;
        break;
    case 192:
        ctx->nr = 12;
        break;
    case 256:
        ctx->nr = 14;
        break;
    default:
        return;
    }

#if defined(PADLOCK_ALIGN16)
    ctx->rk = RK = PADLOCK_ALIGN16(ctx->buf);
#else
    ctx->rk = RK = ctx->buf;
#endif

    for (i = 0; i < (keysize >> 5); i++) {
        GET_ULONG_LE(RK[i], key, i << 2);
    }
    switch (ctx->nr) {
    case 10:
        for (i = 0; i < 10; i++, RK += 4) {
            RK[4] = RK[0] ^ RCON[i] ^
                ((ulong)FSb[(RK[3] >> 8) & 0xFF]) ^
                ((ulong)FSb[(RK[3] >> 16) & 0xFF] << 8) ^
                ((ulong)FSb[(RK[3] >> 24) & 0xFF] << 16) ^
                ((ulong)FSb[(RK[3]) & 0xFF] << 24);

            RK[5] = RK[1] ^ RK[4];
            RK[6] = RK[2] ^ RK[5];
            RK[7] = RK[3] ^ RK[6];
        }
        break;

    case 12:
        for (i = 0; i < 8; i++, RK += 6) {
            RK[6] = RK[0] ^ RCON[i] ^
                ((ulong)FSb[(RK[5] >> 8) & 0xFF]) ^
                ((ulong)FSb[(RK[5] >> 16) & 0xFF] << 8) ^
                ((ulong)FSb[(RK[5] >> 24) & 0xFF] << 16) ^
                ((ulong)FSb[(RK[5]) & 0xFF] << 24);

            RK[7] = RK[1] ^ RK[6];
            RK[8] = RK[2] ^ RK[7];
            RK[9] = RK[3] ^ RK[8];
            RK[10] = RK[4] ^ RK[9];
            RK[11] = RK[5] ^ RK[10];
        }
        break;

    case 14:
        for (i = 0; i < 7; i++, RK += 8) {
            RK[8] = RK[0] ^ RCON[i] ^
                ((ulong)FSb[(RK[7] >> 8) & 0xFF]) ^
                ((ulong)FSb[(RK[7] >> 16) & 0xFF] << 8) ^
                ((ulong)FSb[(RK[7] >> 24) & 0xFF] << 16) ^
                ((ulong)FSb[(RK[7]) & 0xFF] << 24);

            RK[9] = RK[1] ^ RK[8];
            RK[10] = RK[2] ^ RK[9];
            RK[11] = RK[3] ^ RK[10];

            RK[12] = RK[4] ^
                ((ulong)FSb[(RK[11]) & 0xFF]) ^
                ((ulong)FSb[(RK[11] >> 8) & 0xFF] << 8) ^
                ((ulong)FSb[(RK[11] >> 16) & 0xFF] << 16) ^
                ((ulong)FSb[(RK[11] >> 24) & 0xFF] << 24);

            RK[13] = RK[5] ^ RK[12];
            RK[14] = RK[6] ^ RK[13];
            RK[15] = RK[7] ^ RK[14];
        }
        break;

    default:
        break;
    }
}


/*
   AES key schedule (decryption)
 */
void aes_setkey_dec(aes_context * ctx, uchar *key, int keysize)
{
    int         i, j;
    aes_context cty;
    ulong       *RK, *SK;

    switch (keysize) {
    case 128:
        ctx->nr = 10;
        break;
    case 192:
        ctx->nr = 12;
        break;
    case 256:
        ctx->nr = 14;
        break;
    default:
        return;
    }

#if defined(PADLOCK_ALIGN16)
    ctx->rk = RK = PADLOCK_ALIGN16(ctx->buf);
#else
    ctx->rk = RK = ctx->buf;
#endif
    aes_setkey_enc(&cty, key, keysize);
    SK = cty.rk + cty.nr * 4;

    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;

    for (i = ctx->nr - 1, SK -= 8; i > 0; i--, SK -= 8) {
        for (j = 0; j < 4; j++, SK++) {
            *RK++ = RT0[FSb[(*SK) & 0xFF]] ^
                RT1[FSb[(*SK >> 8) & 0xFF]] ^
                RT2[FSb[(*SK >> 16) & 0xFF]] ^
                RT3[FSb[(*SK >> 24) & 0xFF]];
        }
    }

    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;

    memset(&cty, 0, sizeof(aes_context));
}


#define AES_FROUND(X0,X1,X2,X3,Y0,Y1,Y2,Y3)     \
{                                               \
    X0 = *RK++ ^ FT0[ ( Y0       ) & 0xFF ] ^   \
                 FT1[ ( Y1 >>  8 ) & 0xFF ] ^   \
                 FT2[ ( Y2 >> 16 ) & 0xFF ] ^   \
                 FT3[ ( Y3 >> 24 ) & 0xFF ];    \
                                                \
    X1 = *RK++ ^ FT0[ ( Y1       ) & 0xFF ] ^   \
                 FT1[ ( Y2 >>  8 ) & 0xFF ] ^   \
                 FT2[ ( Y3 >> 16 ) & 0xFF ] ^   \
                 FT3[ ( Y0 >> 24 ) & 0xFF ];    \
                                                \
    X2 = *RK++ ^ FT0[ ( Y2       ) & 0xFF ] ^   \
                 FT1[ ( Y3 >>  8 ) & 0xFF ] ^   \
                 FT2[ ( Y0 >> 16 ) & 0xFF ] ^   \
                 FT3[ ( Y1 >> 24 ) & 0xFF ];    \
                                                \
    X3 = *RK++ ^ FT0[ ( Y3       ) & 0xFF ] ^   \
                 FT1[ ( Y0 >>  8 ) & 0xFF ] ^   \
                 FT2[ ( Y1 >> 16 ) & 0xFF ] ^   \
                 FT3[ ( Y2 >> 24 ) & 0xFF ];    \
}

#define AES_RROUND(X0,X1,X2,X3,Y0,Y1,Y2,Y3)     \
{                                               \
    X0 = *RK++ ^ RT0[ ( Y0       ) & 0xFF ] ^   \
                 RT1[ ( Y3 >>  8 ) & 0xFF ] ^   \
                 RT2[ ( Y2 >> 16 ) & 0xFF ] ^   \
                 RT3[ ( Y1 >> 24 ) & 0xFF ];    \
                                                \
    X1 = *RK++ ^ RT0[ ( Y1       ) & 0xFF ] ^   \
                 RT1[ ( Y0 >>  8 ) & 0xFF ] ^   \
                 RT2[ ( Y3 >> 16 ) & 0xFF ] ^   \
                 RT3[ ( Y2 >> 24 ) & 0xFF ];    \
                                                \
    X2 = *RK++ ^ RT0[ ( Y2       ) & 0xFF ] ^   \
                 RT1[ ( Y1 >>  8 ) & 0xFF ] ^   \
                 RT2[ ( Y0 >> 16 ) & 0xFF ] ^   \
                 RT3[ ( Y3 >> 24 ) & 0xFF ];    \
                                                \
    X3 = *RK++ ^ RT0[ ( Y3       ) & 0xFF ] ^   \
                 RT1[ ( Y2 >>  8 ) & 0xFF ] ^   \
                 RT2[ ( Y1 >> 16 ) & 0xFF ] ^   \
                 RT3[ ( Y0 >> 24 ) & 0xFF ];    \
}


/*
   AES-ECB block encryption/decryption
 */
void aes_crypt_ecb(aes_context * ctx, int mode, uchar input[16], uchar output[16])
{
    int     i;
    ulong   *RK, X0, X1, X2, X3, Y0, Y1, Y2, Y3;

#if ME_EST_PADLOCK && defined(EST_HAVE_X86)
    if (padlock_supports(PADLOCK_ACE)) {
        if (padlock_xcryptecb(ctx, mode, input, output) == 0) {
            return;
        }
    }
#endif
    RK = ctx->rk;
    GET_ULONG_LE(X0, input, 0);
    X0 ^= *RK++;
    GET_ULONG_LE(X1, input, 4);
    X1 ^= *RK++;
    GET_ULONG_LE(X2, input, 8);
    X2 ^= *RK++;
    GET_ULONG_LE(X3, input, 12);
    X3 ^= *RK++;

    if (mode == AES_DECRYPT) {
        for (i = (ctx->nr >> 1) - 1; i > 0; i--) {
            AES_RROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
            AES_RROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3);
        }
        AES_RROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
        X0 = *RK++ ^
            ((ulong)RSb[(Y0) & 0xFF]) ^
            ((ulong)RSb[(Y3 >> 8) & 0xFF] << 8) ^
            ((ulong)RSb[(Y2 >> 16) & 0xFF] << 16) ^
            ((ulong)RSb[(Y1 >> 24) & 0xFF] << 24);

        X1 = *RK++ ^
            ((ulong)RSb[(Y1) & 0xFF]) ^
            ((ulong)RSb[(Y0 >> 8) & 0xFF] << 8) ^
            ((ulong)RSb[(Y3 >> 16) & 0xFF] << 16) ^
            ((ulong)RSb[(Y2 >> 24) & 0xFF] << 24);

        X2 = *RK++ ^
            ((ulong)RSb[(Y2) & 0xFF]) ^
            ((ulong)RSb[(Y1 >> 8) & 0xFF] << 8) ^
            ((ulong)RSb[(Y0 >> 16) & 0xFF] << 16) ^
            ((ulong)RSb[(Y3 >> 24) & 0xFF] << 24);

        X3 = *RK++ ^
            ((ulong)RSb[(Y3) & 0xFF]) ^
            ((ulong)RSb[(Y2 >> 8) & 0xFF] << 8) ^
            ((ulong)RSb[(Y1 >> 16) & 0xFF] << 16) ^
            ((ulong)RSb[(Y0 >> 24) & 0xFF] << 24);
    } else {
        /* AES_ENCRYPT */
        for (i = (ctx->nr >> 1) - 1; i > 0; i--) {
            AES_FROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
            AES_FROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3);
        }
        AES_FROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
        X0 = *RK++ ^
            ((ulong)FSb[(Y0) & 0xFF]) ^
            ((ulong)FSb[(Y1 >> 8) & 0xFF] << 8) ^
            ((ulong)FSb[(Y2 >> 16) & 0xFF] << 16) ^
            ((ulong)FSb[(Y3 >> 24) & 0xFF] << 24);

        X1 = *RK++ ^
            ((ulong)FSb[(Y1) & 0xFF]) ^
            ((ulong)FSb[(Y2 >> 8) & 0xFF] << 8) ^
            ((ulong)FSb[(Y3 >> 16) & 0xFF] << 16) ^
            ((ulong)FSb[(Y0 >> 24) & 0xFF] << 24);

        X2 = *RK++ ^
            ((ulong)FSb[(Y2) & 0xFF]) ^
            ((ulong)FSb[(Y3 >> 8) & 0xFF] << 8) ^
            ((ulong)FSb[(Y0 >> 16) & 0xFF] << 16) ^
            ((ulong)FSb[(Y1 >> 24) & 0xFF] << 24);

        X3 = *RK++ ^
            ((ulong)FSb[(Y3) & 0xFF]) ^
            ((ulong)FSb[(Y0 >> 8) & 0xFF] << 8) ^
            ((ulong)FSb[(Y1 >> 16) & 0xFF] << 16) ^
            ((ulong)FSb[(Y2 >> 24) & 0xFF] << 24);
    }
    PUT_ULONG_LE(X0, output, 0);
    PUT_ULONG_LE(X1, output, 4);
    PUT_ULONG_LE(X2, output, 8);
    PUT_ULONG_LE(X3, output, 12);
}


/*
   AES-CBC buffer encryption/decryption
 */
void aes_crypt_cbc(aes_context *ctx, int mode, int length, uchar iv[16], uchar *input, uchar *output)
{
    int     i;
    uchar   temp[16];

#if ME_EST_PADLOCK && defined(EST_HAVE_X86)
    if (padlock_supports(PADLOCK_ACE)) {
        if (padlock_xcryptcbc(ctx, mode, length, iv, input, output) == 0)
            return;
    }
#endif
    if (mode == AES_DECRYPT) {
        while (length > 0) {
            memcpy(temp, input, 16);
            aes_crypt_ecb(ctx, mode, input, output);

            for (i = 0; i < 16; i++) {
                output[i] = (uchar)(output[i] ^ iv[i]);
            }
            memcpy(iv, temp, 16);
            input += 16;
            output += 16;
            length -= 16;
        }
    } else {
        while (length > 0) {
            for (i = 0; i < 16; i++) {
                output[i] = (uchar)(input[i] ^ iv[i]);
            }
            aes_crypt_ecb(ctx, mode, output, output);
            memcpy(iv, output, 16);
            input += 16;
            output += 16;
            length -= 16;
        }
    }
}


/*
   AES-CFB128 buffer encryption/decryption
 */
void aes_crypt_cfb128(aes_context *ctx, int mode, int length, int *iv_off, uchar iv[16], uchar *input, uchar *output)
{
    int c, n;

    n = *iv_off;
    if (mode == AES_DECRYPT) {
        while (length--) {
            if (n == 0) {
                aes_crypt_ecb(ctx, AES_ENCRYPT, iv, iv);
            }
            c = *input++;
            *output++ = (uchar)(c ^ iv[n]);
            iv[n] = (uchar)c;
            n = (n + 1) & 0x0F;
        }
    } else {
        while (length--) {
            if (n == 0) {
                aes_crypt_ecb(ctx, AES_ENCRYPT, iv, iv);
            }
            iv[n] = *output++ = (uchar)(iv[n] ^ *input++);
            n = (n + 1) & 0x0F;
        }
    }
    *iv_off = n;
}


#undef FSb
#undef SWAP
#undef P
#undef R
#undef S0
#undef S1
#undef S2
#undef S3

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/arc4.c ************/


/*
    arc4.c -- An implementation of the ARCFOUR algorithm

    The ARCFOUR algorithm was publicly disclosed on 94/09.

    http://groups.google.com/group/sci.crypt/msg/10a300c9d21afca0

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_RC4
/*
   ARC4 key schedule
 */
void arc4_setup(arc4_context *ctx, uchar *key, int keylen)
{
    int     i, j, k, a;
    uchar   *m;

    ctx->x = 0;
    ctx->y = 0;
    m = ctx->m;

    for (i = 0; i < 256; i++) {
        m[i] = (uchar) i;
    }
    j = k = 0;
    for (i = 0; i < 256; i++, k++) {
        if (k >= keylen) {
            k = 0;
        }
        a = m[i];
        j = (j + a + key[k]) & 0xFF;
        m[i] = m[j];
        m[j] = (uchar)a;
    }
}


/*
   ARC4 cipher function
 */
void arc4_crypt(arc4_context *ctx, uchar *buf, int buflen)
{
    int     i, x, y, a, b;
    uchar   *m;

    x = ctx->x;
    y = ctx->y;
    m = ctx->m;

    for (i = 0; i < buflen; i++) {
        x = (x + 1) & 0xFF;
        a = m[x];
        y = (y + a) & 0xFF;
        b = m[y];

        m[x] = (uchar)b;
        m[y] = (uchar)a;

        buf[i] = (uchar) (buf[i] ^ m[(uchar)(a + b)]);
    }
    ctx->x = x;
    ctx->y = y;
}

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/base64.c ************/


/*
    base64.c -- RFC 1521 base64 encoding/decoding

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_BASE64
static const uchar base64_enc_map[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '+', '/'
};

static const uchar base64_dec_map[128] = {
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 62, 127, 127, 127, 63, 52, 53,
    54, 55, 56, 57, 58, 59, 60, 61, 127, 127,
    127, 64, 127, 127, 127, 0, 1, 2, 3, 4,
    5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 127, 127, 127, 127, 127, 127, 26, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
    39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 127, 127, 127, 127, 127
};

/*
 * Encode a buffer into base64 format
 */
int base64_encode(uchar *dst, int *dlen, uchar *src, int slen)
{
    int i, n;
    int C1, C2, C3;
    uchar *p;

    if (slen == 0) {
        return 0;
    }
    n = (slen << 3) / 6;

    switch ((slen << 3) - (n * 6)) {
    case 2:
        n += 3;
        break;
    case 4:
        n += 2;
        break;
    default:
        break;
    }

    if (*dlen < n + 1) {
        *dlen = n + 1;
        return EST_ERR_BASE64_BUFFER_TOO_SMALL;
    }

    n = (slen / 3) * 3;

    for (i = 0, p = dst; i < n; i += 3) {
        C1 = *src++;
        C2 = *src++;
        C3 = *src++;
        *p++ = base64_enc_map[(C1 >> 2) & 0x3F];
        *p++ = base64_enc_map[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];
        *p++ = base64_enc_map[(((C2 & 15) << 2) + (C3 >> 6)) & 0x3F];
        *p++ = base64_enc_map[C3 & 0x3F];
    }

    if (i < slen) {
        C1 = *src++;
        C2 = ((i + 1) < slen) ? *src++ : 0;

        *p++ = base64_enc_map[(C1 >> 2) & 0x3F];
        *p++ = base64_enc_map[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];

        if ((i + 1) < slen) {
            *p++ = base64_enc_map[((C2 & 15) << 2) & 0x3F];
        } else {
            *p++ = '=';
        }
        *p++ = '=';
    }
    *dlen = p - dst;
    *p = 0;
    return 0;
}

/*
    Decode a base64-formatted buffer
 */
int base64_decode(uchar *dst, int *dlen, uchar *src, int slen)
{
    int i, j, n;
    ulong x;
    uchar *p;

    for (i = j = n = 0; i < slen; i++) {
        if ((slen - i) >= 2 && src[i] == '\r' && src[i + 1] == '\n') {
            continue;
        }
        if (src[i] == '\n') {
            continue;
        }
        if (src[i] == '=' && ++j > 2) {
            return EST_ERR_BASE64_INVALID_CHARACTER;
        }
        if (src[i] > 127 || base64_dec_map[src[i]] == 127) {
            return EST_ERR_BASE64_INVALID_CHARACTER;
        }
        if (base64_dec_map[src[i]] < 64 && j != 0) {
            return EST_ERR_BASE64_INVALID_CHARACTER;
        }
        n++;
    }

    if (n == 0) {
        return 0;
    }
    n = ((n * 6) + 7) >> 3;

    if (*dlen < n) {
        *dlen = n;
        return EST_ERR_BASE64_BUFFER_TOO_SMALL;
    }
    for (j = 3, n = x = 0, p = dst; i > 0; i--, src++) {
        if (*src == '\r' || *src == '\n')
            continue;

        j -= (base64_dec_map[*src] == 64);
        x = (x << 6) | (base64_dec_map[*src] & 0x3F);

        if (++n == 4) {
            n = 0;
            if (j > 0) {
                *p++ = (uchar)(x >> 16);
            }
            if (j > 1) {
                *p++ = (uchar)(x >> 8);
            }
            if (j > 2) {
                *p++ = (uchar)(x);
            }
        }
    }
    *dlen = p - dst;
    return 0;
}

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/bignum.c ************/


/*
    bignum.c -- Multi-precision integer library

    This MPI implementation is based on:
  
    http://www.cacr.math.uwaterloo.ca/hac/about/chap14.pdf
    http://www.stillhq.com/extracted/gnupg-api/mpi/
    http://math.libtomcrypt.com/files/tommath.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if ME_EST_BIGNUM

#define ciL    ((int) sizeof(t_int))    /* chars in limb  */
#define biL    (ciL << 3)               /* bits  in limb  */
#define biH    (ciL << 2)               /* half limb size */

/*
   Convert between bits/chars and number of limbs
 */
#define BITS_TO_LIMBS(i)  (((i) + biL - 1) / biL)
#define CHARS_TO_LIMBS(i) (((i) + ciL - 1) / ciL)

/*
    Initialize one or more mpi
 */
void mpi_init(mpi *X, ...)
{
    va_list args;

    va_start(args, X);

    while (X != NULL) {
        X->s = 1;
        X->n = 0;
        X->p = NULL;
        X = va_arg(args, mpi *);
    }
    va_end(args);
}


/*
    Unallocate one or more mpi
 */
void mpi_free(mpi *X, ...)
{
    va_list args;

    va_start(args, X);

    while (X != NULL) {
        if (X->p != NULL) {
            memset(X->p, 0, X->n * ciL);
            free(X->p);
        }
        X->s = 1;
        X->n = 0;
        X->p = NULL;
        X = va_arg(args, mpi *);
    }
    va_end(args);
}


/*
    Enlarge to the specified number of limbs
 */
int mpi_grow(mpi *X, int nblimbs)
{
    t_int *p;

    if (X->n < nblimbs) {
        if ((p = (t_int *) malloc(nblimbs * ciL)) == NULL) {
            return 1;
        }
        memset(p, 0, nblimbs * ciL);
        if (X->p != NULL) {
            memcpy(p, X->p, X->n * ciL);
            memset(X->p, 0, X->n * ciL);
            free(X->p);
        }
        X->n = nblimbs;
        X->p = p;
    }
    return 0;
}


/*
    Copy the contents of Y into X
 */
int mpi_copy(mpi *X, mpi *Y)
{
    int ret, i;

    if (X == Y) {
        return 0;
    }
    for (i = Y->n - 1; i > 0; i--) {
        if (Y->p[i] != 0) {
            break;
        }
    }
    i++;
    X->s = Y->s;
    MPI_CHK(mpi_grow(X, i));
    memset(X->p, 0, X->n * ciL);
    memcpy(X->p, Y->p, i * ciL);

cleanup:
    return ret;
}


/*
    Swap the contents of X and Y
 */
void mpi_swap(mpi *X, mpi *Y)
{
    mpi     T;

    memcpy(&T, X, sizeof(mpi));
    memcpy(X, Y, sizeof(mpi));
    memcpy(Y, &T, sizeof(mpi));
}


/*
    Set value from integer
 */
int mpi_lset(mpi *X, int z)
{
    int ret;

    MPI_CHK(mpi_grow(X, 1));
    memset(X->p, 0, X->n * ciL);

    X->p[0] = (z < 0) ? -z : z;
    X->s = (z < 0) ? -1 : 1;

cleanup:

    return ret;
}


/*
    Return the number of least significant bits
 */
int mpi_lsb(mpi *X)
{
    int i, j, count = 0;

    for (i = 0; i < X->n; i++)
        for (j = 0; j < (int)biL; j++, count++)
            if (((X->p[i] >> j) & 1) != 0)
                return count;

    return 0;
}

/*
 * Return the number of most significant bits
 */
int mpi_msb(mpi *X)
{
    int i, j;

    for (i = X->n - 1; i > 0; i--)
        if (X->p[i] != 0)
            break;

    for (j = biL - 1; j >= 0; j--)
        if (((X->p[i] >> j) & 1) != 0)
            break;

    return (i * biL) + j + 1;
}


/*
    Return the total size in bytes
 */
int mpi_size(mpi *X)
{
    return (mpi_msb(X) + 7) >> 3;
}


/*
    Convert an ASCII character to digit value
 */
static int mpi_get_digit(t_int * d, int radix, char c)
{
    *d = 255;

    if (c >= 0x30 && c <= 0x39)
        *d = c - 0x30;
    if (c >= 0x41 && c <= 0x46)
        *d = c - 0x37;
    if (c >= 0x61 && c <= 0x66)
        *d = c - 0x57;

    if (*d >= (t_int) radix)
        return EST_ERR_MPI_INVALID_CHARACTER;

    return 0;
}


/*
    Import from an ASCII string
 */
int mpi_read_string(mpi *X, int radix, char *s)
{
    int ret, i, j, n;
    t_int d;
    mpi T;

    if (radix < 2 || radix > 16)
        return EST_ERR_MPI_BAD_INPUT_DATA;

    mpi_init(&T, NULL);

    if (radix == 16) {
        n = BITS_TO_LIMBS(strlen(s) << 2);

        MPI_CHK(mpi_grow(X, n));
        MPI_CHK(mpi_lset(X, 0));

        for (i = strlen(s) - 1, j = 0; i >= 0; i--, j++) {
            if (i == 0 && s[i] == '-') {
                X->s = -1;
                break;
            }

            MPI_CHK(mpi_get_digit(&d, radix, s[i]));
            X->p[j / (2 * ciL)] |= d << ((j % (2 * ciL)) << 2);
        }
    } else {
        MPI_CHK(mpi_lset(X, 0));

        for (i = 0; i < (int)strlen(s); i++) {
            if (i == 0 && s[i] == '-') {
                X->s = -1;
                continue;
            }

            MPI_CHK(mpi_get_digit(&d, radix, s[i]));
            MPI_CHK(mpi_mul_int(&T, X, radix));
            MPI_CHK(mpi_add_int(X, &T, d));
        }
    }

cleanup:
    mpi_free(&T, NULL);
    return ret;
}


/*
   Helper to write the digits high-order first
 */
static int mpi_write_hlp(mpi *X, int radix, char **p)
{
    int ret;
    t_int r;

    if (radix < 2 || radix > 16)
        return EST_ERR_MPI_BAD_INPUT_DATA;

    MPI_CHK(mpi_mod_int(&r, X, radix));
    MPI_CHK(mpi_div_int(X, NULL, X, radix));

    if (mpi_cmp_int(X, 0) != 0)
        MPI_CHK(mpi_write_hlp(X, radix, p));

    if (r < 10)
        *(*p)++ = (char)(r + 0x30);
    else
        *(*p)++ = (char)(r + 0x37);

cleanup:

    return ret;
}


/*
    Export into an ASCII string
 */
int mpi_write_string(mpi *X, int radix, char *s, int *slen)
{
    int ret = 0, n;
    char *p;
    mpi T;

    if (radix < 2 || radix > 16)
        return EST_ERR_MPI_BAD_INPUT_DATA;

    n = mpi_msb(X);
    if (radix >= 4)
        n >>= 1;
    if (radix >= 16)
        n >>= 1;
    n += 3;

    if (*slen < n) {
        *slen = n;
        return EST_ERR_MPI_BUFFER_TOO_SMALL;
    }

    p = s;
    mpi_init(&T, NULL);

    if (X->s == -1) {
        *p++ = '-';
    }
    if (radix == 16) {
        int c, i, j, k;

        for (i = X->n - 1, k = 0; i >= 0; i--) {
            for (j = ciL - 1; j >= 0; j--) {
                c = (X->p[i] >> (j << 3)) & 0xFF;

                if (c == 0 && k == 0 && (i + j) != 0) {
                    continue;
                }
                p += sprintf(p, "%02X", c);
                k = 1;
            }
        }
    } else {
        MPI_CHK(mpi_copy(&T, X));
        MPI_CHK(mpi_write_hlp(&T, radix, &p));
    }

    *p++ = '\0';
    *slen = p - s;

cleanup:
    mpi_free(&T, NULL);
    return ret;
}


/*
    Read X from an opened file
 */
int mpi_read_file(mpi *X, int radix, FILE * fin)
{
    t_int d;
    int slen;
    char *p;
    char s[1024];

    memset(s, 0, sizeof(s));
    if (fgets(s, sizeof(s) - 1, fin) == NULL) {
        return EST_ERR_MPI_FILE_IO_ERROR;
    }
    slen = strlen(s);
    if (s[slen - 1] == '\n') {
        slen--;
        s[slen] = '\0';
    }
    if (s[slen - 1] == '\r') {
        slen--;
        s[slen] = '\0';
    }

    p = s + slen;
    while (--p >= s) {
        if (mpi_get_digit(&d, radix, *p) != 0) {
            break;
        }
    }
    return mpi_read_string(X, radix, p + 1);
}


/*
    Write X into an opened file (or stdout if fout == NULL)
 */
int mpi_write_file(char *p, mpi *X, int radix, FILE * fout)
{
    int n, ret;
    size_t slen;
    size_t plen;
    char s[1024];

    n = sizeof(s);
    memset(s, 0, n);
    n -= 2;

    MPI_CHK(mpi_write_string(X, radix, s, (int *)&n));

    if (p == NULL) {
        p = "";
    }
    plen = strlen(p);
    slen = strlen(s);
    s[slen++] = '\r';
    s[slen++] = '\n';

    if (fout != NULL) {
        if (fwrite(p, 1, plen, fout) != plen || fwrite(s, 1, slen, fout) != slen) {
            return EST_ERR_MPI_FILE_IO_ERROR;
        }
    } else {
        printf("%s%s", p, s);
    }

cleanup:
    return ret;
}


/*
    Import X from unsigned binary data, big endian
 */
int mpi_read_binary(mpi *X, uchar *buf, int buflen)
{
    int ret, i, j, n;

    for (n = 0; n < buflen; n++) {
        if (buf[n] != 0) {
            break;
        }
    }
    MPI_CHK(mpi_grow(X, CHARS_TO_LIMBS(buflen - n)));
    MPI_CHK(mpi_lset(X, 0));

    for (i = buflen - 1, j = 0; i >= n; i--, j++) {
        X->p[j / ciL] |= ((t_int) buf[i]) << ((j % ciL) << 3);
    }

cleanup:
    return ret;
}


/*
    Export X into unsigned binary data, big endian
 */
int mpi_write_binary(mpi *X, uchar *buf, int buflen)
{
    int i, j, n;

    n = mpi_size(X);

    if (buflen < n) {
        return EST_ERR_MPI_BUFFER_TOO_SMALL;
    }
    memset(buf, 0, buflen);

    for (i = buflen - 1, j = 0; n > 0; i--, j++, n--) {
        buf[i] = (uchar)(X->p[j / ciL] >> ((j % ciL) << 3));
    }
    return 0;
}


/*
    Left-shift: X <<= count
 */
int mpi_shift_l(mpi *X, int count)
{
    int ret, i, v0, t1;
    t_int r0 = 0, r1;

    v0 = count / (biL);
    t1 = count & (biL - 1);

    i = mpi_msb(X) + count;

    if (X->n * (int)biL < i)
        MPI_CHK(mpi_grow(X, BITS_TO_LIMBS(i)));

    ret = 0;

    /*
        shift by count / limb_size
     */
    if (v0 > 0) {
        for (i = X->n - 1; i >= v0; i--)
            X->p[i] = X->p[i - v0];

        for (; i >= 0; i--)
            X->p[i] = 0;
    }

    /*
        shift by count % limb_size
     */
    if (t1 > 0) {
        for (i = v0; i < X->n; i++) {
            r1 = X->p[i] >> (biL - t1);
            X->p[i] <<= t1;
            X->p[i] |= r0;
            r0 = r1;
        }
    }
cleanup:
    return ret;
}


/*
    Right-shift: X >>= count
 */
int mpi_shift_r(mpi *X, int count)
{
    int i, v0, v1;
    t_int r0 = 0, r1;

    v0 = count / biL;
    v1 = count & (biL - 1);

    /*
        shift by count / limb_size
     */
    if (v0 > 0) {
        for (i = 0; i < X->n - v0; i++)
            X->p[i] = X->p[i + v0];

        for (; i < X->n; i++)
            X->p[i] = 0;
    }

    /*
        shift by count % limb_size
     */
    if (v1 > 0) {
        for (i = X->n - 1; i >= 0; i--) {
            r1 = X->p[i] << (biL - v1);
            X->p[i] >>= v1;
            X->p[i] |= r0;
            r0 = r1;
        }
    }
    return 0;
}


/*
    Compare unsigned values
 */
int mpi_cmp_abs(mpi *X, mpi *Y)
{
    int i, j;

    for (i = X->n - 1; i >= 0; i--)
        if (X->p[i] != 0)
            break;

    for (j = Y->n - 1; j >= 0; j--)
        if (Y->p[j] != 0)
            break;

    if (i < 0 && j < 0)
        return 0;

    if (i > j)
        return 1;
    if (j > i)
        return -1;

    for (; i >= 0; i--) {
        if (X->p[i] > Y->p[i])
            return 1;
        if (X->p[i] < Y->p[i])
            return -1;
    }
    return 0;
}


/*
    Compare signed values
 */
int mpi_cmp_mpi(mpi *X, mpi *Y)
{
    int i, j;

    for (i = X->n - 1; i >= 0; i--)
        if (X->p[i] != 0)
            break;

    for (j = Y->n - 1; j >= 0; j--)
        if (Y->p[j] != 0)
            break;

    if (i < 0 && j < 0)
        return 0;

    if (i > j)
        return X->s;
    if (j > i)
        return -X->s;

    if (X->s > 0 && Y->s < 0)
        return 1;
    if (Y->s > 0 && X->s < 0)
        return -1;

    for (; i >= 0; i--) {
        if (X->p[i] > Y->p[i])
            return X->s;
        if (X->p[i] < Y->p[i])
            return -X->s;
    }

    return 0;
}


/*
    Compare signed values
 */
int mpi_cmp_int(mpi *X, int z)
{
    mpi Y;
    t_int p[1];

    *p = (z < 0) ? -z : z;
    Y.s = (z < 0) ? -1 : 1;
    Y.n = 1;
    Y.p = p;

    return mpi_cmp_mpi(X, &Y);
}


/*
    Unsigned addition: X = |A| + |B|  (HAC 14.7)
 */
int mpi_add_abs(mpi *X, mpi *A, mpi *B)
{
    int ret, i, j;
    t_int *o, *p, c;

    if (X == B) {
        mpi *T = A;
        A = X;
        B = T;
    }

    if (X != A)
        MPI_CHK(mpi_copy(X, A));

    for (j = B->n - 1; j >= 0; j--)
        if (B->p[j] != 0)
            break;

    MPI_CHK(mpi_grow(X, j + 1));

    o = B->p;
    p = X->p;
    c = 0;

    for (i = 0; i <= j; i++, o++, p++) {
        *p += c;
        c = (*p < c);
        *p += *o;
        c += (*p < *o);
    }
    while (c != 0) {
        if (i >= X->n) {
            MPI_CHK(mpi_grow(X, i + 1));
            p = X->p + i;
        }

        *p += c;
        c = (*p < c);
        i++;
    }
cleanup:
    return ret;
}


/*
    Helper for mpi substraction
 */
static void mpi_sub_hlp(int n, t_int * s, t_int * d)
{
    int     i;
    t_int   c, z;

    for (i = c = 0; i < n; i++, s++, d++) {
        z = (*d < c);
        *d -= c;
        c = (*d < *s) + z;
        *d -= *s;
    }

    while (c != 0) {
        z = (*d < c);
        *d -= c;
        c = z;
        i++;
        d++;
    }
}


/*
    Unsigned substraction: X = |A| - |B|  (HAC 14.9)
 */
int mpi_sub_abs(mpi *X, mpi *A, mpi *B)
{
    mpi TB;
    int ret, n;

    if (mpi_cmp_abs(A, B) < 0)
        return EST_ERR_MPI_NEGATIVE_VALUE;

    mpi_init(&TB, NULL);

    if (X == B) {
        MPI_CHK(mpi_copy(&TB, B));
        B = &TB;
    }
    if (X != A) {
        MPI_CHK(mpi_copy(X, A));
    }
    ret = 0;

    for (n = B->n - 1; n >= 0; n--) {
        if (B->p[n] != 0) {
            break;
        }
    }
    mpi_sub_hlp(n + 1, B->p, X->p);

cleanup:

    mpi_free(&TB, NULL);
    return ret;
}


/*
    Signed addition: X = A + B
 */
int mpi_add_mpi(mpi *X, mpi *A, mpi *B)
{
    int ret, s = A->s;

    if (A->s * B->s < 0) {
        if (mpi_cmp_abs(A, B) >= 0) {
            MPI_CHK(mpi_sub_abs(X, A, B));
            X->s = s;
        } else {
            MPI_CHK(mpi_sub_abs(X, B, A));
            X->s = -s;
        }
    } else {
        MPI_CHK(mpi_add_abs(X, A, B));
        X->s = s;
    }
cleanup:
    return ret;
}


/*
    Signed substraction: X = A - B
 */
int mpi_sub_mpi(mpi *X, mpi *A, mpi *B)
{
    int ret, s = A->s;

    if (A->s * B->s > 0) {
        if (mpi_cmp_abs(A, B) >= 0) {
            MPI_CHK(mpi_sub_abs(X, A, B));
            X->s = s;
        } else {
            MPI_CHK(mpi_sub_abs(X, B, A));
            X->s = -s;
        }
    } else {
        MPI_CHK(mpi_add_abs(X, A, B));
        X->s = s;
    }
cleanup:
    return ret;
}


/*
    Signed addition: X = A + b
 */
int mpi_add_int(mpi *X, mpi *A, int b)
{
    mpi     _B;
    t_int   p[1];

    p[0] = (b < 0) ? -b : b;
    _B.s = (b < 0) ? -1 : 1;
    _B.n = 1;
    _B.p = p;
    return mpi_add_mpi(X, A, &_B);
}


/*
    Signed substraction: X = A - b
 */
int mpi_sub_int(mpi *X, mpi *A, int b)
{
    mpi     _B;
    t_int   p[1];

    p[0] = (b < 0) ? -b : b;
    _B.s = (b < 0) ? -1 : 1;
    _B.n = 1;
    _B.p = p;
    return mpi_sub_mpi(X, A, &_B);
}


/*
    Helper for mpi multiplication
 */
static void mpi_mul_hlp(int i, t_int * s, t_int * d, t_int b)
{
    t_int c = 0, t = 0;

#if defined(MULADDC_HUIT)
    for (; i >= 8; i -= 8) {
    MULADDC_INIT MULADDC_HUIT MULADDC_STOP} for (; i > 0; i--) {
    MULADDC_INIT MULADDC_CORE MULADDC_STOP}
#else
    for (; i >= 16; i -= 16) {
        MULADDC_INIT
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE 
        MULADDC_STOP
    }
    for (; i >= 8; i -= 8) {
        MULADDC_INIT
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE 
        MULADDC_STOP
    }
    for (; i > 0; i--) {
        MULADDC_INIT 
        MULADDC_CORE 
        MULADDC_STOP
    }
#endif
    t++;

    do {
        *d += c;
        c = (*d < c);
        d++;
    } while (c != 0);
}


/*
   Baseline multiplication: X = A * B  (HAC 14.12)
 */
int mpi_mul_mpi(mpi *X, mpi *A, mpi *B)
{
    int     ret, i, j;
    mpi     TA, TB;

    mpi_init(&TA, &TB, NULL);

    if (X == A) {
        MPI_CHK(mpi_copy(&TA, A));
        A = &TA;
    }
    if (X == B) {
        MPI_CHK(mpi_copy(&TB, B));
        B = &TB;
    }
    for (i = A->n - 1; i >= 0; i--)
        if (A->p[i] != 0)
            break;

    for (j = B->n - 1; j >= 0; j--)
        if (B->p[j] != 0)
            break;

    MPI_CHK(mpi_grow(X, i + j + 2));
    MPI_CHK(mpi_lset(X, 0));

    for (i++; j >= 0; j--)
        mpi_mul_hlp(i, A->p, X->p + j, B->p[j]);

    X->s = A->s * B->s;

cleanup:
    mpi_free(&TB, &TA, NULL);
    return ret;
}


/*
    Baseline multiplication: X = A * b
 */
int mpi_mul_int(mpi *X, mpi *A, t_int b)
{
    mpi _B;
    t_int p[1];

    _B.s = 1;
    _B.n = 1;
    _B.p = p;
    p[0] = b;
    return mpi_mul_mpi(X, A, &_B);
}


/*
    Division by mpi: A = Q * B + R  (HAC 14.20)
 */
int mpi_div_mpi(mpi *Q, mpi *R, mpi *A, mpi *B)
{
    int ret, i, n, t, k;
    mpi X, Y, Z, T1, T2;

    if (mpi_cmp_int(B, 0) == 0)
        return EST_ERR_MPI_DIVISION_BY_ZERO;

    mpi_init(&X, &Y, &Z, &T1, &T2, NULL);

    if (mpi_cmp_abs(A, B) < 0) {
        if (Q != NULL)
            MPI_CHK(mpi_lset(Q, 0));
        if (R != NULL)
            MPI_CHK(mpi_copy(R, A));
        return 0;
    }
    MPI_CHK(mpi_copy(&X, A));
    MPI_CHK(mpi_copy(&Y, B));
    X.s = Y.s = 1;

    MPI_CHK(mpi_grow(&Z, A->n + 2));
    MPI_CHK(mpi_lset(&Z, 0));
    MPI_CHK(mpi_grow(&T1, 2));
    MPI_CHK(mpi_grow(&T2, 3));

    k = mpi_msb(&Y) % biL;
    if (k < (int)biL - 1) {
        k = biL - 1 - k;
        MPI_CHK(mpi_shift_l(&X, k));
        MPI_CHK(mpi_shift_l(&Y, k));
    } else
        k = 0;

    n = X.n - 1;
    t = Y.n - 1;
    mpi_shift_l(&Y, biL * (n - t));

    while (mpi_cmp_mpi(&X, &Y) >= 0) {
        Z.p[n - t]++;
        mpi_sub_mpi(&X, &X, &Y);
    }
    mpi_shift_r(&Y, biL * (n - t));

    for (i = n; i > t; i--) {
        if (X.p[i] >= Y.p[t])
            Z.p[i - t - 1] = ~0;
        else {
#if ME_USE_LONG_LONG
            t_dbl r;

            r = (t_dbl) X.p[i] << biL;
            r |= (t_dbl) X.p[i - 1];
            r /= Y.p[t];
            if (r > ((t_dbl) 1 << biL) - 1)
                r = ((t_dbl) 1 << biL) - 1;

            Z.p[i - t - 1] = (t_int) r;
#else
            /*
             * __udiv_qrnnd_c, from gmp/longlong.h
             */
            t_int q0, q1, r0, r1;
            t_int d0, d1, d, m;

            d = Y.p[t];
            d0 = (d << biH) >> biH;
            d1 = (d >> biH);

            q1 = X.p[i] / d1;
            r1 = X.p[i] - d1 * q1;
            r1 <<= biH;
            r1 |= (X.p[i - 1] >> biH);

            m = q1 * d0;
            if (r1 < m) {
                q1--, r1 += d;
                while (r1 >= d && r1 < m)
                    q1--, r1 += d;
            }
            r1 -= m;

            q0 = r1 / d1;
            r0 = r1 - d1 * q0;
            r0 <<= biH;
            r0 |= (X.p[i - 1] << biH) >> biH;

            m = q0 * d0;
            if (r0 < m) {
                q0--, r0 += d;
                while (r0 >= d && r0 < m)
                    q0--, r0 += d;
            }
            r0 -= m;

            Z.p[i - t - 1] = (q1 << biH) | q0;
#endif
        }

        Z.p[i - t - 1]++;
        do {
            Z.p[i - t - 1]--;

            MPI_CHK(mpi_lset(&T1, 0));
            T1.p[0] = (t < 1) ? 0 : Y.p[t - 1];
            T1.p[1] = Y.p[t];
            MPI_CHK(mpi_mul_int(&T1, &T1, Z.p[i - t - 1]));

            MPI_CHK(mpi_lset(&T2, 0));
            T2.p[0] = (i < 2) ? 0 : X.p[i - 2];
            T2.p[1] = (i < 1) ? 0 : X.p[i - 1];
            T2.p[2] = X.p[i];
        } while (mpi_cmp_mpi(&T1, &T2) > 0);

        MPI_CHK(mpi_mul_int(&T1, &Y, Z.p[i - t - 1]));
        MPI_CHK(mpi_shift_l(&T1, biL * (i - t - 1)));
        MPI_CHK(mpi_sub_mpi(&X, &X, &T1));

        if (mpi_cmp_int(&X, 0) < 0) {
            MPI_CHK(mpi_copy(&T1, &Y));
            MPI_CHK(mpi_shift_l(&T1, biL * (i - t - 1)));
            MPI_CHK(mpi_add_mpi(&X, &X, &T1));
            Z.p[i - t - 1]--;
        }
    }

    if (Q != NULL) {
        mpi_copy(Q, &Z);
        Q->s = A->s * B->s;
    }

    if (R != NULL) {
        mpi_shift_r(&X, k);
        mpi_copy(R, &X);

        R->s = A->s;
        if (mpi_cmp_int(R, 0) == 0)
            R->s = 1;
    }

cleanup:
    mpi_free(&X, &Y, &Z, &T1, &T2, NULL);
    return ret;
}


/*
   Division by int: A = Q * b + R
  
   Returns 0 if successful
           1 if memory allocation failed
           EST_ERR_MPI_DIVISION_BY_ZERO if b == 0
 */
int mpi_div_int(mpi *Q, mpi *R, mpi *A, int b)
{
    mpi _B;
    t_int p[1];

    p[0] = (b < 0) ? -b : b;
    _B.s = (b < 0) ? -1 : 1;
    _B.n = 1;
    _B.p = p;

    return mpi_div_mpi(Q, R, A, &_B);
}


/*
    Modulo: R = A mod B
 */
int mpi_mod_mpi(mpi *R, mpi *A, mpi *B)
{
    int ret;

    MPI_CHK(mpi_div_mpi(NULL, R, A, B));

    while (mpi_cmp_int(R, 0) < 0)
        MPI_CHK(mpi_add_mpi(R, R, B));

    while (mpi_cmp_mpi(R, B) >= 0)
        MPI_CHK(mpi_sub_mpi(R, R, B));

cleanup:
    return ret;
}


/*
   Modulo: r = A mod b
 */
int mpi_mod_int(t_int * r, mpi *A, int b)
{
    int i;
    t_int x, y, z;

    if (b == 0)
        return EST_ERR_MPI_DIVISION_BY_ZERO;

    if (b < 0)
        b = -b;

    /*
       handle trivial cases
     */
    if (b == 1) {
        *r = 0;
        return 0;
    }

    if (b == 2) {
        *r = A->p[0] & 1;
        return 0;
    }

    /*
     * general case
     */
    for (i = A->n - 1, y = 0; i >= 0; i--) {
        x = A->p[i];
        y = (y << biH) | (x >> biH);
        z = y / b;
        y -= z * b;

        x <<= biH;
        y = (y << biH) | (x >> biH);
        z = y / b;
        y -= z * b;
    }
    *r = y;
    return 0;
}


/*
    Fast Montgomery initialization (thanks to Tom St Denis)
 */
static void mpi_montg_init(t_int * mm, mpi *N)
{
    t_int x, m0 = N->p[0];

    x = m0;
    x += ((m0 + 2) & 4) << 1;
    x *= (2 - (m0 * x));

    if (biL >= 16)
        x *= (2 - (m0 * x));
    if (biL >= 32)
        x *= (2 - (m0 * x));
    if (biL >= 64)
        x *= (2 - (m0 * x));

    *mm = ~x + 1;
}


/*
    Montgomery multiplication: A = A * B * R^-1 mod N  (HAC 14.36)
 */
static void mpi_montmul(mpi *A, mpi *B, mpi *N, t_int mm, mpi *T)
{
    int i, n, m;
    t_int u0, u1, *d;

    memset(T->p, 0, T->n * ciL);

    d = T->p;
    n = N->n;
    m = (B->n < n) ? B->n : n;

    for (i = 0; i < n; i++) {
        /*
            T = (T + u0*B + u1*N) / 2^biL
         */
        u0 = A->p[i];
        u1 = (d[0] + u0 * B->p[0]) * mm;

        mpi_mul_hlp(m, B->p, d, u0);
        mpi_mul_hlp(n, N->p, d, u1);

        *d++ = u0;
        d[n + 1] = 0;
    }

    memcpy(A->p, d, (n + 1) * ciL);

    if (mpi_cmp_abs(A, N) >= 0)
        mpi_sub_hlp(n, N->p, A->p);
    else
        /* prevent timing attacks */
        mpi_sub_hlp(n, A->p, T->p);
}


/*
    Montgomery reduction: A = A * R^-1 mod N
 */
static void mpi_montred(mpi *A, mpi *N, t_int mm, mpi *T)
{
    t_int z = 1;
    mpi U;

    U.n = U.s = z;
    U.p = &z;

    mpi_montmul(A, &U, N, mm, T);
}


/*
    Sliding-window exponentiation: X = A^E mod N  (HAC 14.85)
 */
int mpi_exp_mod(mpi *X, mpi *A, mpi *E, mpi *N, mpi *_RR)
{
    int ret, i, j, wsize, wbits;
    int bufsize, nblimbs, nbits;
    t_int ei, mm, state;
    mpi RR, T, W[64];

    if (mpi_cmp_int(N, 0) < 0 || (N->p[0] & 1) == 0)
        return EST_ERR_MPI_BAD_INPUT_DATA;

    /*
       Init temps and window size
     */
    mpi_montg_init(&mm, N);
    mpi_init(&RR, &T, NULL);
    memset(W, 0, sizeof(W));

    i = mpi_msb(E);

    wsize = (i > 671) ? 6 : (i > 239) ? 5 : (i > 79) ? 4 : (i > 23) ? 3 : 1;

    j = N->n + 1;
    MPI_CHK(mpi_grow(X, j));
    MPI_CHK(mpi_grow(&W[1], j));
    MPI_CHK(mpi_grow(&T, j * 2));

    /*
        If 1st call, pre-compute R^2 mod N
     */
    if (_RR == NULL || _RR->p == NULL) {
        MPI_CHK(mpi_lset(&RR, 1));
        MPI_CHK(mpi_shift_l(&RR, N->n * 2 * biL));
        MPI_CHK(mpi_mod_mpi(&RR, &RR, N));

        if (_RR != NULL)
            memcpy(_RR, &RR, sizeof(mpi));
    } else
        memcpy(&RR, _RR, sizeof(mpi));

    /*
        W[1] = A * R^2 * R^-1 mod N = A * R mod N
     */
    if (mpi_cmp_mpi(A, N) >= 0)
        mpi_mod_mpi(&W[1], A, N);
    else
        mpi_copy(&W[1], A);

    mpi_montmul(&W[1], &RR, N, mm, &T);

    /*
        X = R^2 * R^-1 mod N = R mod N
     */
    MPI_CHK(mpi_copy(X, &RR));
    mpi_montred(X, N, mm, &T);

    if (wsize > 1) {
        /*
         * W[1 << (wsize - 1)] = W[1] ^ (wsize - 1)
         */
        j = 1 << (wsize - 1);

        MPI_CHK(mpi_grow(&W[j], N->n + 1));
        MPI_CHK(mpi_copy(&W[j], &W[1]));

        for (i = 0; i < wsize - 1; i++)
            mpi_montmul(&W[j], &W[j], N, mm, &T);

        /*
         * W[i] = W[i - 1] * W[1]
         */
        for (i = j + 1; i < (1 << wsize); i++) {
            MPI_CHK(mpi_grow(&W[i], N->n + 1));
            MPI_CHK(mpi_copy(&W[i], &W[i - 1]));

            mpi_montmul(&W[i], &W[1], N, mm, &T);
        }
    }

    nblimbs = E->n;
    bufsize = 0;
    nbits = 0;
    wbits = 0;
    state = 0;

    while (1) {
        if (bufsize == 0) {
            if (nblimbs-- == 0)
                break;

            bufsize = sizeof(t_int) << 3;
        }

        bufsize--;

        ei = (E->p[nblimbs] >> bufsize) & 1;

        /*
            skip leading 0s
         */
        if (ei == 0 && state == 0)
            continue;

        if (ei == 0 && state == 1) {
            /*
             * out of window, square X
             */
            mpi_montmul(X, X, N, mm, &T);
            continue;
        }

        /*
            add ei to current window
         */
        state = 2;

        nbits++;
        wbits |= (ei << (wsize - nbits));

        if (nbits == wsize) {
            /*
                X = X^wsize R^-1 mod N
             */
            for (i = 0; i < wsize; i++)
                mpi_montmul(X, X, N, mm, &T);

            /*
                X = X * W[wbits] R^-1 mod N
             */
            mpi_montmul(X, &W[wbits], N, mm, &T);

            state--;
            nbits = 0;
            wbits = 0;
        }
    }

    /*
        process the remaining bits
     */
    for (i = 0; i < nbits; i++) {
        mpi_montmul(X, X, N, mm, &T);

        wbits <<= 1;

        if ((wbits & (1 << wsize)) != 0)
            mpi_montmul(X, &W[1], N, mm, &T);
    }

    /*
        X = A^E * R * R^-1 mod N = A^E mod N
     */
    mpi_montred(X, N, mm, &T);

cleanup:

    for (i = (1 << (wsize - 1)); i < (1 << wsize); i++)
        mpi_free(&W[i], NULL);

    if (_RR != NULL)
        mpi_free(&W[1], &T, NULL);
    else
        mpi_free(&W[1], &T, &RR, NULL);

    return ret;
}


/*
    Greatest common divisor: G = gcd(A, B)  (HAC 14.54)
 */
int mpi_gcd(mpi *G, mpi *A, mpi *B)
{
    int ret, lz, lzt;
    mpi TG, TA, TB;

    mpi_init(&TG, &TA, &TB, NULL);

    MPI_CHK(mpi_copy(&TA, A));
    MPI_CHK(mpi_copy(&TB, B));

    lz = mpi_lsb(&TA);
    lzt = mpi_lsb(&TB);

    if (lzt < lz)
        lz = lzt;

    MPI_CHK(mpi_shift_r(&TA, lz));
    MPI_CHK(mpi_shift_r(&TB, lz));

    TA.s = TB.s = 1;

    while (mpi_cmp_int(&TA, 0) != 0) {
        MPI_CHK(mpi_shift_r(&TA, mpi_lsb(&TA)));
        MPI_CHK(mpi_shift_r(&TB, mpi_lsb(&TB)));

        if (mpi_cmp_mpi(&TA, &TB) >= 0) {
            MPI_CHK(mpi_sub_abs(&TA, &TA, &TB));
            MPI_CHK(mpi_shift_r(&TA, 1));
        } else {
            MPI_CHK(mpi_sub_abs(&TB, &TB, &TA));
            MPI_CHK(mpi_shift_r(&TB, 1));
        }
    }

    MPI_CHK(mpi_shift_l(&TB, lz));
    MPI_CHK(mpi_copy(G, &TB));

cleanup:
    mpi_free(&TB, &TA, &TG, NULL);
    return ret;
}


#if ME_EST_GEN_PRIME
/*
    Modular inverse: X = A^-1 mod N  (HAC 14.61 / 14.64)
 */
int mpi_inv_mod(mpi *X, mpi *A, mpi *N)
{
    int ret;
    mpi G, TA, TU, U1, U2, TB, TV, V1, V2;

    if (mpi_cmp_int(N, 0) <= 0)
        return EST_ERR_MPI_BAD_INPUT_DATA;

    mpi_init(&TA, &TU, &U1, &U2, &G, &TB, &TV, &V1, &V2, NULL);

    MPI_CHK(mpi_gcd(&G, A, N));

    if (mpi_cmp_int(&G, 1) != 0) {
        ret = EST_ERR_MPI_NOT_ACCEPTABLE;
        goto cleanup;
    }

    MPI_CHK(mpi_mod_mpi(&TA, A, N));
    MPI_CHK(mpi_copy(&TU, &TA));
    MPI_CHK(mpi_copy(&TB, N));
    MPI_CHK(mpi_copy(&TV, N));

    MPI_CHK(mpi_lset(&U1, 1));
    MPI_CHK(mpi_lset(&U2, 0));
    MPI_CHK(mpi_lset(&V1, 0));
    MPI_CHK(mpi_lset(&V2, 1));

    do {
        while ((TU.p[0] & 1) == 0) {
            MPI_CHK(mpi_shift_r(&TU, 1));

            if ((U1.p[0] & 1) != 0 || (U2.p[0] & 1) != 0) {
                MPI_CHK(mpi_add_mpi(&U1, &U1, &TB));
                MPI_CHK(mpi_sub_mpi(&U2, &U2, &TA));
            }

            MPI_CHK(mpi_shift_r(&U1, 1));
            MPI_CHK(mpi_shift_r(&U2, 1));
        }

        while ((TV.p[0] & 1) == 0) {
            MPI_CHK(mpi_shift_r(&TV, 1));

            if ((V1.p[0] & 1) != 0 || (V2.p[0] & 1) != 0) {
                MPI_CHK(mpi_add_mpi(&V1, &V1, &TB));
                MPI_CHK(mpi_sub_mpi(&V2, &V2, &TA));
            }

            MPI_CHK(mpi_shift_r(&V1, 1));
            MPI_CHK(mpi_shift_r(&V2, 1));
        }

        if (mpi_cmp_mpi(&TU, &TV) >= 0) {
            MPI_CHK(mpi_sub_mpi(&TU, &TU, &TV));
            MPI_CHK(mpi_sub_mpi(&U1, &U1, &V1));
            MPI_CHK(mpi_sub_mpi(&U2, &U2, &V2));
        } else {
            MPI_CHK(mpi_sub_mpi(&TV, &TV, &TU));
            MPI_CHK(mpi_sub_mpi(&V1, &V1, &U1));
            MPI_CHK(mpi_sub_mpi(&V2, &V2, &U2));
        }
    } while (mpi_cmp_int(&TU, 0) != 0);

    while (mpi_cmp_int(&V1, 0) < 0)
        MPI_CHK(mpi_add_mpi(&V1, &V1, N));

    while (mpi_cmp_mpi(&V1, N) >= 0)
        MPI_CHK(mpi_sub_mpi(&V1, &V1, N));

    MPI_CHK(mpi_copy(X, &V1));

cleanup:
    mpi_free(&V2, &V1, &TV, &TB, &G, &U2, &U1, &TU, &TA, NULL);
    return ret;
}


static const int small_prime[] = {
    3, 5, 7, 11, 13, 17, 19, 23,
    29, 31, 37, 41, 43, 47, 53, 59,
    61, 67, 71, 73, 79, 83, 89, 97,
    101, 103, 107, 109, 113, 127, 131, 137,
    139, 149, 151, 157, 163, 167, 173, 179,
    181, 191, 193, 197, 199, 211, 223, 227,
    229, 233, 239, 241, 251, 257, 263, 269,
    271, 277, 281, 283, 293, 307, 311, 313,
    317, 331, 337, 347, 349, 353, 359, 367,
    373, 379, 383, 389, 397, 401, 409, 419,
    421, 431, 433, 439, 443, 449, 457, 461,
    463, 467, 479, 487, 491, 499, 503, 509,
    521, 523, 541, 547, 557, 563, 569, 571,
    577, 587, 593, 599, 601, 607, 613, 617,
    619, 631, 641, 643, 647, 653, 659, 661,
    673, 677, 683, 691, 701, 709, 719, 727,
    733, 739, 743, 751, 757, 761, 769, 773,
    787, 797, 809, 811, 821, 823, 827, 829,
    839, 853, 857, 859, 863, 877, 881, 883,
    887, 907, 911, 919, 929, 937, 941, 947,
    953, 967, 971, 977, 983, 991, 997, -103
};


/*
    Miller-Rabin primality test  (HAC 4.24)
 */
int mpi_is_prime(mpi *X, int (*f_rng) (void *), void *p_rng)
{
    int ret, i, j, n, s, xs;
    mpi W, R, T, A, RR;
    uchar *p;

    if (mpi_cmp_int(X, 0) == 0)
        return 0;

    mpi_init(&W, &R, &T, &A, &RR, NULL);

    xs = X->s;
    X->s = 1;

    /*
        test trivial factors first
     */
    if ((X->p[0] & 1) == 0)
        return EST_ERR_MPI_NOT_ACCEPTABLE;

    for (i = 0; small_prime[i] > 0; i++) {
        t_int r;

        if (mpi_cmp_int(X, small_prime[i]) <= 0) {
            return 0;
        }
        MPI_CHK(mpi_mod_int(&r, X, small_prime[i]));
        if (r == 0) {
            return EST_ERR_MPI_NOT_ACCEPTABLE;
        }
    }

    /*
       W = |X| - 1
       R = W >> lsb(W)
     */
    s = mpi_lsb(&W);
    MPI_CHK(mpi_sub_int(&W, X, 1));
    MPI_CHK(mpi_copy(&R, &W));
    MPI_CHK(mpi_shift_r(&R, s));

    i = mpi_msb(X);
    /*
        HAC, table 4.4
     */
    n = ((i >= 1300) ? 2 : (i >= 850) ? 3 :
         (i >= 650) ? 4 : (i >= 350) ? 8 :
         (i >= 250) ? 12 : (i >= 150) ? 18 : 27);

    for (i = 0; i < n; i++) {
        /*
            pick a random A, 1 < A < |X| - 1
         */
        MPI_CHK(mpi_grow(&A, X->n));

        p = (uchar *)A.p;
        for (j = 0; j < A.n * ciL; j++) {
            *p++ = (uchar)f_rng(p_rng);
        }
        j = mpi_msb(&A) - mpi_msb(&W);
        MPI_CHK(mpi_shift_r(&A, j + 1));
        A.p[0] |= 3;

        /*
            A = A^R mod |X|
         */
        MPI_CHK(mpi_exp_mod(&A, &A, &R, X, &RR));

        if (mpi_cmp_mpi(&A, &W) == 0 || mpi_cmp_int(&A, 1) == 0) {
            continue;
        }
        j = 1;
        while (j < s && mpi_cmp_mpi(&A, &W) != 0) {
            /*
                A = A * A mod |X|
             */
            MPI_CHK(mpi_mul_mpi(&T, &A, &A));
            MPI_CHK(mpi_mod_mpi(&A, &T, X));

            if (mpi_cmp_int(&A, 1) == 0) {
                break;
            }
            j++;
        }

        /*
            not prime if A != |X| - 1 or A == 1
         */
        if (mpi_cmp_mpi(&A, &W) != 0 || mpi_cmp_int(&A, 1) == 0) {
            ret = EST_ERR_MPI_NOT_ACCEPTABLE;
            break;
        }
    }
cleanup:
    X->s = xs;
    mpi_free(&RR, &A, &T, &R, &W, NULL);
    return ret;
}


/*
    Prime number generation
 */
int mpi_gen_prime(mpi *X, int nbits, int dh_flag, int (*f_rng) (void *), void *p_rng)
{
    int ret, k, n;
    uchar *p;
    mpi Y;

    if (nbits < 3) {
        return EST_ERR_MPI_BAD_INPUT_DATA;
    }
    mpi_init(&Y, NULL);

    n = BITS_TO_LIMBS(nbits);

    MPI_CHK(mpi_grow(X, n));
    MPI_CHK(mpi_lset(X, 0));

    p = (uchar *)X->p;
    for (k = 0; k < X->n * ciL; k++)
        *p++ = (uchar)f_rng(p_rng);

    k = mpi_msb(X);
    if (k < nbits) {
        MPI_CHK(mpi_shift_l(X, nbits - k));
    }
    if (k > nbits) {
        MPI_CHK(mpi_shift_r(X, k - nbits));
    }
    X->p[0] |= 3;

    if (dh_flag == 0) {
        while ((ret = mpi_is_prime(X, f_rng, p_rng)) != 0) {
            if (ret != EST_ERR_MPI_NOT_ACCEPTABLE)
                goto cleanup;

            MPI_CHK(mpi_add_int(X, X, 2));
        }
    } else {
        MPI_CHK(mpi_sub_int(&Y, X, 1));
        MPI_CHK(mpi_shift_r(&Y, 1));
        while (1) {
            if ((ret = mpi_is_prime(X, f_rng, p_rng)) == 0) {
                if ((ret = mpi_is_prime(&Y, f_rng, p_rng)) == 0) {
                    break;
                }
                if (ret != EST_ERR_MPI_NOT_ACCEPTABLE) {
                    goto cleanup;
                }
            }
            if (ret != EST_ERR_MPI_NOT_ACCEPTABLE) {
                goto cleanup;
            }
            MPI_CHK(mpi_add_int(&Y, X, 1));
            MPI_CHK(mpi_add_int(X, X, 2));
            MPI_CHK(mpi_shift_r(&Y, 1));
        }
    }

cleanup:
    mpi_free(&Y, NULL);
    return ret;
}

#endif /* ME_EST_GEN_PRIME */
#endif /* ME_EST_BIGNUM */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/camellia.c ************/


/*
    camellia.c -- Header for the Multithreaded Portable Runtime (MPR).

    The Camellia block cipher was designed by NTT and Mitsubishi Electric Corporation.
    http://info.isl.ntt.co.jp/crypt/eng/camellia/dl/01espec.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_CAMELLIA

/*
    32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ] << 24 )    \
            | ( (ulong) (b)[(i) + 1] << 16 )    \
            | ( (ulong) (b)[(i) + 2] <<  8 )    \
            | ( (ulong) (b)[(i) + 3]       );   \
    }
#endif

#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 2] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 3] = (uchar) ( (n)       );   \
    }
#endif

static const uchar SIGMA_CHARS[6][8] = {
    {0xa0, 0x9e, 0x66, 0x7f, 0x3b, 0xcc, 0x90, 0x8b},
    {0xb6, 0x7a, 0xe8, 0x58, 0x4c, 0xaa, 0x73, 0xb2},
    {0xc6, 0xef, 0x37, 0x2f, 0xe9, 0x4f, 0x82, 0xbe},
    {0x54, 0xff, 0x53, 0xa5, 0xf1, 0xd3, 0x6f, 0x1c},
    {0x10, 0xe5, 0x27, 0xfa, 0xde, 0x68, 0x2d, 0x1d},
    {0xb0, 0x56, 0x88, 0xc2, 0xb3, 0xe6, 0xc1, 0xfd}
};

#define FSb CAMELLIAFSb

#ifdef EST_CAMELLIA_SMALL_MEMORY
static const uchar FSb[256] = {
    112, 130, 44, 236, 179, 39, 192, 229, 228, 133, 87, 53, 234, 12, 174,
    65,
    35, 239, 107, 147, 69, 25, 165, 33, 237, 14, 79, 78, 29, 101, 146, 189,
    134, 184, 175, 143, 124, 235, 31, 206, 62, 48, 220, 95, 94, 197, 11, 26,
    166, 225, 57, 202, 213, 71, 93, 61, 217, 1, 90, 214, 81, 86, 108, 77,
    139, 13, 154, 102, 251, 204, 176, 45, 116, 18, 43, 32, 240, 177, 132,
    153,
    223, 76, 203, 194, 52, 126, 118, 5, 109, 183, 169, 49, 209, 23, 4, 215,
    20, 88, 58, 97, 222, 27, 17, 28, 50, 15, 156, 22, 83, 24, 242, 34,
    254, 68, 207, 178, 195, 181, 122, 145, 36, 8, 232, 168, 96, 252, 105,
    80,
    170, 208, 160, 125, 161, 137, 98, 151, 84, 91, 30, 149, 224, 255, 100,
    210,
    16, 196, 0, 72, 163, 247, 117, 219, 138, 3, 230, 218, 9, 63, 221, 148,
    135, 92, 131, 2, 205, 74, 144, 51, 115, 103, 246, 243, 157, 127, 191,
    226,
    82, 155, 216, 38, 200, 55, 198, 59, 129, 150, 111, 75, 19, 190, 99, 46,
    233, 121, 167, 140, 159, 110, 188, 142, 41, 245, 249, 182, 47, 253, 180,
    89,
    120, 152, 6, 106, 231, 70, 113, 186, 212, 37, 171, 66, 136, 162, 141,
    250,
    114, 7, 185, 85, 248, 238, 172, 10, 54, 73, 42, 104, 60, 56, 241, 164,
    64, 40, 211, 123, 187, 201, 67, 193, 21, 227, 173, 244, 119, 199, 128,
    158
};

#define SBOX1(n) FSb[(n)]
#define SBOX2(n) (uchar)((FSb[(n)] >> 7 ^ FSb[(n)] << 1) & 0xff)
#define SBOX3(n) (uchar)((FSb[(n)] >> 1 ^ FSb[(n)] << 7) & 0xff)
#define SBOX4(n) FSb[((n) << 1 ^ (n) >> 7) &0xff]

#else

static const uchar FSb[256] = {
    112, 130, 44, 236, 179, 39, 192, 229, 228, 133, 87, 53, 234, 12, 174,
    65,
    35, 239, 107, 147, 69, 25, 165, 33, 237, 14, 79, 78, 29, 101, 146, 189,
    134, 184, 175, 143, 124, 235, 31, 206, 62, 48, 220, 95, 94, 197, 11, 26,
    166, 225, 57, 202, 213, 71, 93, 61, 217, 1, 90, 214, 81, 86, 108, 77,
    139, 13, 154, 102, 251, 204, 176, 45, 116, 18, 43, 32, 240, 177, 132,
    153,
    223, 76, 203, 194, 52, 126, 118, 5, 109, 183, 169, 49, 209, 23, 4, 215,
    20, 88, 58, 97, 222, 27, 17, 28, 50, 15, 156, 22, 83, 24, 242, 34,
    254, 68, 207, 178, 195, 181, 122, 145, 36, 8, 232, 168, 96, 252, 105,
    80,
    170, 208, 160, 125, 161, 137, 98, 151, 84, 91, 30, 149, 224, 255, 100,
    210,
    16, 196, 0, 72, 163, 247, 117, 219, 138, 3, 230, 218, 9, 63, 221, 148,
    135, 92, 131, 2, 205, 74, 144, 51, 115, 103, 246, 243, 157, 127, 191,
    226,
    82, 155, 216, 38, 200, 55, 198, 59, 129, 150, 111, 75, 19, 190, 99, 46,
    233, 121, 167, 140, 159, 110, 188, 142, 41, 245, 249, 182, 47, 253, 180,
    89,
    120, 152, 6, 106, 231, 70, 113, 186, 212, 37, 171, 66, 136, 162, 141,
    250,
    114, 7, 185, 85, 248, 238, 172, 10, 54, 73, 42, 104, 60, 56, 241, 164,
    64, 40, 211, 123, 187, 201, 67, 193, 21, 227, 173, 244, 119, 199, 128,
    158
};

static const uchar FSb2[256] = {
    224, 5, 88, 217, 103, 78, 129, 203, 201, 11, 174, 106, 213, 24, 93, 130,
    70, 223, 214, 39, 138, 50, 75, 66, 219, 28, 158, 156, 58, 202, 37, 123,
    13, 113, 95, 31, 248, 215, 62, 157, 124, 96, 185, 190, 188, 139, 22, 52,
    77, 195, 114, 149, 171, 142, 186, 122, 179, 2, 180, 173, 162, 172, 216,
    154,
    23, 26, 53, 204, 247, 153, 97, 90, 232, 36, 86, 64, 225, 99, 9, 51,
    191, 152, 151, 133, 104, 252, 236, 10, 218, 111, 83, 98, 163, 46, 8,
    175,
    40, 176, 116, 194, 189, 54, 34, 56, 100, 30, 57, 44, 166, 48, 229, 68,
    253, 136, 159, 101, 135, 107, 244, 35, 72, 16, 209, 81, 192, 249, 210,
    160,
    85, 161, 65, 250, 67, 19, 196, 47, 168, 182, 60, 43, 193, 255, 200, 165,
    32, 137, 0, 144, 71, 239, 234, 183, 21, 6, 205, 181, 18, 126, 187, 41,
    15, 184, 7, 4, 155, 148, 33, 102, 230, 206, 237, 231, 59, 254, 127, 197,
    164, 55, 177, 76, 145, 110, 141, 118, 3, 45, 222, 150, 38, 125, 198, 92,
    211, 242, 79, 25, 63, 220, 121, 29, 82, 235, 243, 109, 94, 251, 105,
    178,
    240, 49, 12, 212, 207, 140, 226, 117, 169, 74, 87, 132, 17, 69, 27, 245,
    228, 14, 115, 170, 241, 221, 89, 20, 108, 146, 84, 208, 120, 112, 227,
    73,
    128, 80, 167, 246, 119, 147, 134, 131, 42, 199, 91, 233, 238, 143, 1, 61
};

static const uchar FSb3[256] = {
    56, 65, 22, 118, 217, 147, 96, 242, 114, 194, 171, 154, 117, 6, 87, 160,
    145, 247, 181, 201, 162, 140, 210, 144, 246, 7, 167, 39, 142, 178, 73,
    222,
    67, 92, 215, 199, 62, 245, 143, 103, 31, 24, 110, 175, 47, 226, 133, 13,
    83, 240, 156, 101, 234, 163, 174, 158, 236, 128, 45, 107, 168, 43, 54,
    166,
    197, 134, 77, 51, 253, 102, 88, 150, 58, 9, 149, 16, 120, 216, 66, 204,
    239, 38, 229, 97, 26, 63, 59, 130, 182, 219, 212, 152, 232, 139, 2, 235,
    10, 44, 29, 176, 111, 141, 136, 14, 25, 135, 78, 11, 169, 12, 121, 17,
    127, 34, 231, 89, 225, 218, 61, 200, 18, 4, 116, 84, 48, 126, 180, 40,
    85, 104, 80, 190, 208, 196, 49, 203, 42, 173, 15, 202, 112, 255, 50,
    105,
    8, 98, 0, 36, 209, 251, 186, 237, 69, 129, 115, 109, 132, 159, 238, 74,
    195, 46, 193, 1, 230, 37, 72, 153, 185, 179, 123, 249, 206, 191, 223,
    113,
    41, 205, 108, 19, 100, 155, 99, 157, 192, 75, 183, 165, 137, 95, 177,
    23,
    244, 188, 211, 70, 207, 55, 94, 71, 148, 250, 252, 91, 151, 254, 90,
    172,
    60, 76, 3, 53, 243, 35, 184, 93, 106, 146, 213, 33, 68, 81, 198, 125,
    57, 131, 220, 170, 124, 119, 86, 5, 27, 164, 21, 52, 30, 28, 248, 82,
    32, 20, 233, 189, 221, 228, 161, 224, 138, 241, 214, 122, 187, 227, 64,
    79
};

static const uchar FSb4[256] = {
    112, 44, 179, 192, 228, 87, 234, 174, 35, 107, 69, 165, 237, 79, 29,
    146,
    134, 175, 124, 31, 62, 220, 94, 11, 166, 57, 213, 93, 217, 90, 81, 108,
    139, 154, 251, 176, 116, 43, 240, 132, 223, 203, 52, 118, 109, 169, 209,
    4,
    20, 58, 222, 17, 50, 156, 83, 242, 254, 207, 195, 122, 36, 232, 96, 105,
    170, 160, 161, 98, 84, 30, 224, 100, 16, 0, 163, 117, 138, 230, 9, 221,
    135, 131, 205, 144, 115, 246, 157, 191, 82, 216, 200, 198, 129, 111, 19,
    99,
    233, 167, 159, 188, 41, 249, 47, 180, 120, 6, 231, 113, 212, 171, 136,
    141,
    114, 185, 248, 172, 54, 42, 60, 241, 64, 211, 187, 67, 21, 173, 119,
    128,
    130, 236, 39, 229, 133, 53, 12, 65, 239, 147, 25, 33, 14, 78, 101, 189,
    184, 143, 235, 206, 48, 95, 197, 26, 225, 202, 71, 61, 1, 214, 86, 77,
    13, 102, 204, 45, 18, 32, 177, 153, 76, 194, 126, 5, 183, 49, 23, 215,
    88, 97, 27, 28, 15, 22, 24, 34, 68, 178, 181, 145, 8, 168, 252, 80,
    208, 125, 137, 151, 91, 149, 255, 210, 196, 72, 247, 219, 3, 218, 63,
    148,
    92, 2, 74, 51, 103, 243, 127, 226, 155, 38, 55, 59, 150, 75, 190, 46,
    121, 140, 110, 142, 245, 182, 253, 89, 152, 106, 70, 186, 37, 66, 162,
    250,
    7, 85, 238, 10, 73, 104, 56, 164, 40, 123, 201, 193, 227, 244, 199, 158
};

#define SBOX1(n) FSb[(n)]
#define SBOX2(n) FSb2[(n)]
#define SBOX3(n) FSb3[(n)]
#define SBOX4(n) FSb4[(n)]
#endif

static const uchar shifts[2][4][4] = {
    {
     {1, 1, 1, 1},      /* KL */
     {0, 0, 0, 0},      /* KR */
     {1, 1, 1, 1},      /* KA */
     {0, 0, 0, 0}       /* KB */
     },
    {
     {1, 0, 1, 1},      /* KL */
     {1, 1, 0, 1},      /* KR */
     {1, 1, 1, 0},      /* KA */
     {1, 1, 0, 1}       /* KB */
     }
};

static cchar indexes[2][4][20] = {
    {
     {
      0, 1, 2, 3, 8, 9, 10, 11, 38, 39,
      36, 37, 23, 20, 21, 22, 27, -1, -1, 26},  /* KL -> RK */
     {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},  /* KR -> RK */
     {
      4, 5, 6, 7, 12, 13, 14, 15, 16, 17,
      18, 19, -1, 24, 25, -1, 31, 28, 29, 30},  /* KA -> RK */
     {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}   /* KB -> RK */
     },
    {
     {
      0, 1, 2, 3, 61, 62, 63, 60, -1, -1,
      -1, -1, 27, 24, 25, 26, 35, 32, 33, 34},  /* KL -> RK */
     {
      -1, -1, -1, -1, 8, 9, 10, 11, 16, 17,
      18, 19, -1, -1, -1, -1, 39, 36, 37, 38},  /* KR -> RK */
     {
      -1, -1, -1, -1, 12, 13, 14, 15, 58, 59,
      56, 57, 31, 28, 29, 30, -1, -1, -1, -1},  /* KA -> RK */
     {
      4, 5, 6, 7, 65, 66, 67, 64, 20, 21,
      22, 23, -1, -1, -1, -1, 43, 40, 41, 42}   /* KB -> RK */
     }
};

static cchar transposes[2][20] = {
    {
     21, 22, 23, 20,
     -1, -1, -1, -1,
     18, 19, 16, 17,
     11, 8, 9, 10,
     15, 12, 13, 14},
    {
     25, 26, 27, 24,
     29, 30, 31, 28,
     18, 19, 16, 17,
     -1, -1, -1, -1,
     -1, -1, -1, -1}
};

/* Shift macro for 128 bit strings with rotation smaller than 32 bits (!) */
#define ROTL(DEST, SRC, SHIFT)                                          \
    {                                                                   \
        (DEST)[0] = (SRC)[0] << (SHIFT) ^ (SRC)[1] >> (32 - (SHIFT));   \
        (DEST)[1] = (SRC)[1] << (SHIFT) ^ (SRC)[2] >> (32 - (SHIFT));   \
        (DEST)[2] = (SRC)[2] << (SHIFT) ^ (SRC)[3] >> (32 - (SHIFT));   \
        (DEST)[3] = (SRC)[3] << (SHIFT) ^ (SRC)[0] >> (32 - (SHIFT));   \
    }

#define FL(XL, XR, KL, KR)                                              \
    {                                                                   \
        (XR) = ((((XL) & (KL)) << 1) | (((XL) & (KL)) >> 31)) ^ (XR);   \
        (XL) = ((XR) | (KR)) ^ (XL);                                    \
    }

#define FLInv(YL, YR, KL, KR)                                           \
    {                                                                   \
        (YL) = ((YR) | (KR)) ^ (YL);                                    \
        (YR) = ((((YL) & (KL)) << 1) | (((YL) & (KL)) >> 31)) ^ (YR);   \
    }

#define SHIFT_AND_PLACE(INDEX, OFFSET)                              \
    {                                                               \
        TK[0] = KC[(OFFSET) * 4 + 0];                               \
        TK[1] = KC[(OFFSET) * 4 + 1];                               \
        TK[2] = KC[(OFFSET) * 4 + 2];                               \
        TK[3] = KC[(OFFSET) * 4 + 3];                               \
                                                                    \
        for ( i = 1; i <= 4; i++ )                                  \
            if (shifts[(INDEX)][(OFFSET)][i -1])                    \
                ROTL(TK + i * 4, TK, (15 * i) % 32);                \
                                                                    \
        for ( i = 0; i < 20; i++ )                                  \
            if (indexes[(INDEX)][(OFFSET)][i] != -1) {              \
                RK[(int)indexes[(INDEX)][(OFFSET)][i]] = TK[ i ];   \
            }                                                       \
    }

void camellia_feistel(ulong x[2], ulong k[2],
              ulong z[2])
{
    ulong I0, I1;
    I0 = x[0] ^ k[0];
    I1 = x[1] ^ k[1];

    I0 = (SBOX1((I0 >> 24) & 0xFF) << 24) |
        (SBOX2((I0 >> 16) & 0xFF) << 16) |
        (SBOX3((I0 >> 8) & 0xFF) << 8) | (SBOX4((I0) & 0xFF));
    I1 = (SBOX2((I1 >> 24) & 0xFF) << 24) |
        (SBOX3((I1 >> 16) & 0xFF) << 16) |
        (SBOX4((I1 >> 8) & 0xFF) << 8) | (SBOX1((I1) & 0xFF));

    I0 ^= (I1 << 8) | (I1 >> 24);
    I1 ^= (I0 << 16) | (I0 >> 16);
    I0 ^= (I1 >> 8) | (I1 << 24);
    I1 ^= (I0 >> 8) | (I0 << 24);

    z[0] ^= I1;
    z[1] ^= I0;
}

/*
 * Camellia key schedule (encryption)
 */
void camellia_setkey_enc(camellia_context *ctx, uchar *key, int keysize)
{
    int i, idx;
    ulong *RK;
    uchar t[64];

    RK = ctx->rk;

    memset(t, 0, 64);
    memset(RK, 0, sizeof(ctx->rk));

    switch (keysize) {
    case 128:
        ctx->nr = 3;
        idx = 0;
        break;
    case 192:
    case 256:
        ctx->nr = 4;
        idx = 1;
        break;
    default:
        return;
    }

    for (i = 0; i < keysize / 8; ++i)
        t[i] = key[i];

    if (keysize == 192) {
        for (i = 0; i < 8; i++)
            t[24 + i] = ~t[16 + i];
    }

    /*
       Prepare SIGMA values
     */
    ulong SIGMA[6][2];
    for (i = 0; i < 6; i++) {
        GET_ULONG_BE(SIGMA[i][0], SIGMA_CHARS[i], 0);
        GET_ULONG_BE(SIGMA[i][1], SIGMA_CHARS[i], 4);
    }

    /*
       Key storage in KC
       Order: KL, KR, KA, KB
     */
    ulong KC[16];
    memset(KC, 0, sizeof(KC));

    /* Store KL, KR */
    for (i = 0; i < 8; i++)
        GET_ULONG_BE(KC[i], t, i * 4);

    /* Generate KA */
    for (i = 0; i < 4; ++i)
        KC[8 + i] = KC[i] ^ KC[4 + i];

    camellia_feistel(KC + 8, SIGMA[0], KC + 10);
    camellia_feistel(KC + 10, SIGMA[1], KC + 8);

    for (i = 0; i < 4; ++i)
        KC[8 + i] ^= KC[i];

    camellia_feistel(KC + 8, SIGMA[2], KC + 10);
    camellia_feistel(KC + 10, SIGMA[3], KC + 8);

    if (keysize > 128) {
        /* Generate KB */
        for (i = 0; i < 4; ++i)
            KC[12 + i] = KC[4 + i] ^ KC[8 + i];

        camellia_feistel(KC + 12, SIGMA[4], KC + 14);
        camellia_feistel(KC + 14, SIGMA[5], KC + 12);
    }

    /*
       Generating subkeys
     */
    ulong TK[20];

    /* Manipulating KL */
    SHIFT_AND_PLACE(idx, 0);

    /* Manipulating KR */
    if (keysize > 128) {
        SHIFT_AND_PLACE(idx, 1);
    }

    /* Manipulating KA */
    SHIFT_AND_PLACE(idx, 2);

    /* Manipulating KB */
    if (keysize > 128) {
        SHIFT_AND_PLACE(idx, 3);
    }

    /* Do transpositions */
    for (i = 0; i < 20; i++) {
        if (transposes[idx][i] != -1) {
            RK[32 + 12 * idx + i] = RK[(int)transposes[idx][i]];
        }
    }
}


/*
   Camellia key schedule (decryption)
 */
void camellia_setkey_dec(camellia_context *ctx, uchar *key, int keysize)
{
    int i, idx;
    camellia_context cty;
    ulong *RK;
    ulong *SK;

    switch (keysize) {
    case 128:
        ctx->nr = 3;
        idx = 0;
        break;
    case 192:
    case 256:
        ctx->nr = 4;
        idx = 1;
        break;
    default:
        return;
    }

    RK = ctx->rk;
    camellia_setkey_enc(&cty, key, keysize);
    SK = cty.rk + 24 * 2 + 8 * idx * 2;

    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;

    for (i = 22 + 8 * idx, SK -= 6; i > 0; i--, SK -= 4) {
        *RK++ = *SK++;
        *RK++ = *SK++;
    }
    SK -= 2;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    memset(&cty, 0, sizeof(camellia_context));
}


/*
   Camellia-ECB block encryption/decryption
 */
void camellia_crypt_ecb(camellia_context *ctx, int mode, uchar input[16], uchar output[16])
{
    int NR;
    ulong *RK, X[4];

    NR = ctx->nr;
    RK = ctx->rk;

    GET_ULONG_BE(X[0], input, 0);
    GET_ULONG_BE(X[1], input, 4);
    GET_ULONG_BE(X[2], input, 8);
    GET_ULONG_BE(X[3], input, 12);

    X[0] ^= *RK++;
    X[1] ^= *RK++;
    X[2] ^= *RK++;
    X[3] ^= *RK++;

    while (NR) {
        --NR;
        camellia_feistel(X, RK, X + 2);
        RK += 2;
        camellia_feistel(X + 2, RK, X);
        RK += 2;
        camellia_feistel(X, RK, X + 2);
        RK += 2;
        camellia_feistel(X + 2, RK, X);
        RK += 2;
        camellia_feistel(X, RK, X + 2);
        RK += 2;
        camellia_feistel(X + 2, RK, X);
        RK += 2;

        if (NR) {
            FL(X[0], X[1], RK[0], RK[1]);
            RK += 2;
            FLInv(X[2], X[3], RK[0], RK[1]);
            RK += 2;
        }
    }
    X[2] ^= *RK++;
    X[3] ^= *RK++;
    X[0] ^= *RK++;
    X[1] ^= *RK++;

    PUT_ULONG_BE(X[2], output, 0);
    PUT_ULONG_BE(X[3], output, 4);
    PUT_ULONG_BE(X[0], output, 8);
    PUT_ULONG_BE(X[1], output, 12);
}


/*
   Camellia-CBC buffer encryption/decryption
 */
void camellia_crypt_cbc(camellia_context *ctx, int mode, int length, uchar iv[16], uchar *input, uchar *output)
{
    int i;
    uchar temp[16];

    if (mode == CAMELLIA_DECRYPT) {
        while (length > 0) {
            memcpy(temp, input, 16);
            camellia_crypt_ecb(ctx, mode, input, output);

            for (i = 0; i < 16; i++) {
                output[i] = (uchar)(output[i] ^ iv[i]);
            }
            memcpy(iv, temp, 16);
            input += 16;
            output += 16;
            length -= 16;
        }
    } else {
        while (length > 0) {
            for (i = 0; i < 16; i++) {
                output[i] = (uchar)(input[i] ^ iv[i]);
            }
            camellia_crypt_ecb(ctx, mode, output, output);
            memcpy(iv, output, 16);
            input += 16;
            output += 16;
            length -= 16;
        }
    }
}


/*
   Camellia-CFB128 buffer encryption/decryption
 */
void camellia_crypt_cfb128(camellia_context *ctx, int mode, int length, int *iv_off, uchar iv[16], uchar *input, 
        uchar *output)
{
    int c, n = *iv_off;

    if (mode == CAMELLIA_DECRYPT) {
        while (length--) {
            if (n == 0) {
                camellia_crypt_ecb(ctx, CAMELLIA_ENCRYPT, iv, iv);
            }
            c = *input++;
            *output++ = (uchar)(c ^ iv[n]);
            iv[n] = (uchar)c;
            n = (n + 1) & 0x0F;
        }
    } else {
        while (length--) {
            if (n == 0) {
                camellia_crypt_ecb(ctx, CAMELLIA_ENCRYPT, iv, iv);
            }
            iv[n] = *output++ = (uchar)(iv[n] ^ *input++);
            n = (n + 1) & 0x0F;
        }
    }

    *iv_off = n;
}


#undef FSb
#undef SWAP
#undef P
#undef R
#undef S0
#undef S1
#undef S2
#undef S3

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/certs.c ************/


/*
    certs.c -- X.509 test certificates

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_TEST_CERTS

char test_ca_crt[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDpTCCAo2gAwIBAgIBADANBgkqhkiG9w0BAQUFADBFMQswCQYDVQQGEwJGUjEO\r\n"
    "MAwGA1UEBxMFUGFyaXMxDjAMBgNVBAoTBVh5U1NMMRYwFAYDVQQDEw1YeVNTTCBU\r\n"
    "ZXN0IENBMB4XDTA3MDcwNzA1MDAxOFoXDTE3MDcwNzA1MDAxOFowRTELMAkGA1UE\r\n"
    "BhMCRlIxDjAMBgNVBAcTBVBhcmlzMQ4wDAYDVQQKEwVYeVNTTDEWMBQGA1UEAxMN\r\n"
    "WHlTU0wgVGVzdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAM+k\r\n"
    "gt70fIPiYqmvXr+9uPmWoN405eoSzxdiRLLCHqL4V/Ts0E/H+JNfHS4DHlAgxrJu\r\n"
    "+ZIadvSJuHkI6e1iMkAh5SU1DqaF3jrrFdJCooM6a077M4CRkE1tdAeZDf+BYp0q\r\n"
    "BeMU9Y2+j7ibsQaPAizunbXLf4QdteExCwhYRJ8OVXSEaSNt339gJzTD6kOhES3b\r\n"
    "lEN3qbx6lqFaJ5MLHTix5uNVc2rvbOizV5oLhJNqm52AKOp11tv6WTiI8loagvAc\r\n"
    "jlhEZRNb9el5SL6Jai/uFcqXKzfXNKW3FYPQHFGobmiMfGt1lUSBJ3F2mrqEC7gC\r\n"
    "wHy/FDvAI64/k5LZAFkCAwEAAaOBnzCBnDAMBgNVHRMEBTADAQH/MB0GA1UdDgQW\r\n"
    "BBS87h+Y6Porg+SkfV7DdXKTMdkyZzBtBgNVHSMEZjBkgBS87h+Y6Porg+SkfV7D\r\n"
    "dXKTMdkyZ6FJpEcwRTELMAkGA1UEBhMCRlIxDjAMBgNVBAcTBVBhcmlzMQ4wDAYD\r\n"
    "VQQKEwVYeVNTTDEWMBQGA1UEAxMNWHlTU0wgVGVzdCBDQYIBADANBgkqhkiG9w0B\r\n"
    "AQUFAAOCAQEAIHdoh0NCg6KAAhwDSmfEgSbKUI8/Zr/d56uw42HO0sj/uKPQzUco\r\n"
    "3Mx2BYE1m1itg7q5OhrkB7J4ZB78EtNZM84nV+y6od3YpR0Z9VUxCx7948MozYRy\r\n"
    "TKF5x/lKHRx1PJKfEO4clKdWTFAtWtGhewXrHJQ8C+ENh2Up2wTVh3Z+pEzuZNv3\r\n"
    "u/JYu1H+vkt3l1WCy/9mxUnu+anW1DzxPWnjy4lx6Mi0BD2qfKBWLjVS+7v6ALcj\r\n"
    "S2oRWWr4LUvXT7z9BBAvw2eJQD+a4uAya6EURG7AsAvr5MnWn/r0wLWmBJ6fB1Yp\r\n"
    "F1kOmamOFvstLMf74rLX+LGKeJ/nwuI5FQ==\r\n" "-----END CERTIFICATE-----\r\n";

char test_ca_key[] =
    "-----BEGIN RSA PRIVATE KEY-----\r\n"
    "Proc-Type: 4,ENCRYPTED\r\n"
    "DEK-Info: DES-EDE3-CBC,7BDC280BA4C2F45C\r\n"
    "\r\n"
    "KI6rmwY0ChjF/0/Q7d1vE2jojZQTuF8j0wJUOf02PUUD4en8/FCLj69Pf3/lvOlu\r\n"
    "F8hU4IuBHxN2+feuXp6xTmGd/VyKdWh4+NGtKr2V1gXfk2wqHk3/P/YuF7QhOQjZ\r\n"
    "BlNF7n5o76Nufr3iwBbtIABCGQfNRUwGzrvmFXLIrSqns8ppv83qaD5jHVQQj5MM\r\n"
    "cGIoidwmNORdVmBCVZ17dQ+lMPfqjhN27GBhzXbp9a2EP3w+IqrXOvcl4DqSY+DU\r\n"
    "PIhS6KuQZ4iek4dI93wmw2D8Q67omcKYOX/BjCkZ/v6oq481qOej6MicIY/L9LxH\r\n"
    "r91jqIYLJC+1w20YavB3kIkSe5zys30RzhPTOtG60j8kgiQ4Fh+L/nKBP5noOXGE\r\n"
    "uzDBa051HCEYfufOVkXr6wBCFo9boqM1p/GI0Ze5gCsYY+Vzyu96gu+OGv55Jtyo\r\n"
    "fY2yEwVKhfqNy+xJ+8dwf8dcc5vQLatXebDynuSPI2jbaos6MJT6n0LoY2TpAz47\r\n"
    "vNE7UtEEdgZPPVwE4xO+dVa0kCqJxuG8b+ZZTHZidlQ6MBiebL4vXbeIFieQRzUM\r\n"
    "06IQ7YqsiBVLYxicMlApdFpJ4QM2fFKlZRo+fHg8EN9HEbRpgxIf4IwAAFjOgsiO\r\n"
    "ll7OmyzSF12NUIrRsIo02E4Y8X6xSFeYnOIiqZqdxG4xz/DZKoX6Js8WdYbtzrDv\r\n"
    "7gYYEZy1cuR9PJzMDKXoLz/mjBqDsh11Vgm1nAHbhBqFaWSuueH/LV2rgRijUBKh\r\n"
    "yMTnGXrz56DeJ85bYLjVEOM3xhvjsz6Fq5xLqXRTAzyQn7QuqRg2szLtCnN8olCh\r\n"
    "rdS1Gd7EV5g24WnF9gTzYJ4lwO9oxOsPKKCD1Z77B+lbZLoxTu7+rakTtqLikWMv\r\n"
    "7OILfR4iaIu1nKxhhXpwnze3u+1OWcuk0UnBjSnF519tlrgV3dmA0P2LHd6h6xru\r\n"
    "vIgUFHbigCV0i55peEPxtcy9JGLGJ3WvHA3NGSsqkkB/ymdfEaMmEH7403UKqQqq\r\n"
    "+K9Z1cYmeZlfoXClmdSjsxkYeN5raB0kOOSV/MEOySG+lztyLUGB4n46AZG2PgXN\r\n"
    "A9g9tv2gqc51Vl+55qwTg5429DigjiByRkcmBU3A1aF8yzD1QerwobPmqeNZ0mWv\r\n"
    "SllAvgVs2uy2Wf9/5gEWm+HjnJwS9VPNTocG/r+4BnDK8XG0Jy4oI+jSPxj/Y8Jt\r\n"
    "0ejFPM5A7HGngiaPFYHxcwPwo4Aa4HZnk+keFrA+vF9eDd1IOj195a9TESL+IGt1\r\n"
    "nuNmmuYjyJqM9Uvi1Mutv0UQ6Fl6yv4XxaUtMZKl4LtrAaMdW1T0PEgUG0tjSIxX\r\n"
    "hBd1W+Ob4nmK29aa4iuXaOxeAA3QK7KCZo18CJFgnp1w97qohwlKf+4FuNXHL64Q\r\n"
    "FgmpycV9nfP8G9aUYKAxs1+xutNv/B6opHmfLNL6d0cwi8dvGsrDdGlcyi8Dk/PF\r\n"
    "GKSqlQTF5V7l4UOanefB51tuziEBY+LWcXP4XgqNGPQknPF90NnbH1EglQG2paqb\r\n"
    "5bLyT8G6kfCSY4uHxs9lPuvfOjk9ptjy2FwfyBb3Sl4K+IFEE8XTNuNErh83AKh2\r\n"
    "-----END RSA PRIVATE KEY-----\r\n";

char test_ca_pwd[] = "test";

char test_srv_crt[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDPjCCAiagAwIBAgIBAjANBgkqhkiG9w0BAQUFADBFMQswCQYDVQQGEwJGUjEO\r\n"
    "MAwGA1UEBxMFUGFyaXMxDjAMBgNVBAoTBVh5U1NMMRYwFAYDVQQDEw1YeVNTTCBU\r\n"
    "ZXN0IENBMB4XDTA3MDcwNzA1MDEyOVoXDTA4MDcwNjA1MDEyOVowMTELMAkGA1UE\r\n"
    "BhMCRlIxDjAMBgNVBAoTBVh5U1NMMRIwEAYDVQQDEwlsb2NhbGhvc3QwggEiMA0G\r\n"
    "CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC40PDcGTgmHkt6noXDfkjVuymjiNYB\r\n"
    "gjtiL7uA1Ke3tXStacEecQek/OJxYqYr7ffcWalS29LL6HbKpi0xLZKBbD9ACkDh\r\n"
    "1Z/SvHlyQPILJdYb9DMw+kzZds5myXUjzn7Aem1YjoxMZUAMyc34i2900X2pL0v2\r\n"
    "SfCeJ9Ym4MOnZxYl217+dX9ZbkgIgrT6uY2IYK4boDwxbTcyT8i/NPsVsiMwtWPM\r\n"
    "rnQMr+XbgS98sUzcZE70Pe1TlV9Iy8j/8d2OiFo+qTyMu/6UpM2s3gdkQkMzx+Sm\r\n"
    "4QitRUjzmEXeUePRUjEgHIv7vz069xuVBzrks36w5BXiVAhLke/OTKVPAgMBAAGj\r\n"
    "TTBLMAkGA1UdEwQCMAAwHQYDVR0OBBYEFNkOyCTx64SDdPySGWl/tzD7/WMSMB8G\r\n"
    "A1UdIwQYMBaAFLzuH5jo+iuD5KR9XsN1cpMx2TJnMA0GCSqGSIb3DQEBBQUAA4IB\r\n"
    "AQBelJv5t+suaqy5Lo5bjNeHjNZfgg8EigDQ7NqaosvlQZAsh2N34Gg5YdkGyVdg\r\n"
    "s32I/K5aaywyUbG9qVXQxCM2T95qBqyK56h9yJoZKWQD9H//+zB8kCK/16WvRfv3\r\n"
    "VA7eSR19qOFWlHe+1qGh2YhxeDUfyi+fm4D36dGxqC2A34tZjo0QPHKtIeqM0kJy\r\n"
    "zzL65TlbJQKkyTuRHofFv0jW9ZFG2wkGysVgCY5fjuLI1do/sWUaXd2987iNFa+K\r\n"
    "FrHsTi6urSfZuGlZNxDXDHEE7Q2snAvvev+KR7DD9X4DJGcPX9gA4CGJj+9ZzyAA\r\n"
    "ZTGpOzk1hIH44RFs2lJMZRlE\r\n" "-----END CERTIFICATE-----\r\n";

char test_srv_key[] =
    "-----BEGIN RSA PRIVATE KEY-----\r\n"
    "MIIEowIBAAKCAQEAuNDw3Bk4Jh5Lep6Fw35I1bspo4jWAYI7Yi+7gNSnt7V0rWnB\r\n"
    "HnEHpPzicWKmK+333FmpUtvSy+h2yqYtMS2SgWw/QApA4dWf0rx5ckDyCyXWG/Qz\r\n"
    "MPpM2XbOZsl1I85+wHptWI6MTGVADMnN+ItvdNF9qS9L9knwnifWJuDDp2cWJdte\r\n"
    "/nV/WW5ICIK0+rmNiGCuG6A8MW03Mk/IvzT7FbIjMLVjzK50DK/l24EvfLFM3GRO\r\n"
    "9D3tU5VfSMvI//HdjohaPqk8jLv+lKTNrN4HZEJDM8fkpuEIrUVI85hF3lHj0VIx\r\n"
    "IByL+789OvcblQc65LN+sOQV4lQIS5HvzkylTwIDAQABAoIBABeah8h0aBlmMRmd\r\n"
    "+VN4Y3D4kF7UcRCMQ21Mz1Oq1Si/QgGLyiBLK0DFE16LzNE7eTZpNRjh/lAQhmtn\r\n"
    "QcpQGa/x1TomlRbCo8DUVWZkKQWHdYroa0lMDliPtdimzhEepE2M1T5EJmLzY3S+\r\n"
    "qVGe7UMsJjJfWgJAezyXteANQK+2YSt+CjPIqIHch1KexUnvdN9++1oEx6AbuZ8T\r\n"
    "4avhFYZQP15tZNGsk2LfQlYS/NfbowkCsd0/TVubJBmDGUML/E5MbxjxLzlaNB2M\r\n"
    "V59cBNgsgA35CODAUF4xOyoSfZGqG1Rb9qQrv1E6Jz56dG8SsKF3HqnDjxiPOVBN\r\n"
    "FBnVJ+ECgYEA29MhAsKMm4XqBUKp6pIMFTgm/s1E5vxig70vqIiL+guvBhhQ7zs1\r\n"
    "8UMTNXZoMELNoB/ev9fN0Cjc1Vr46b/x/yDw7wMb96i+vzENOzu4RHWi3OWpCPbp\r\n"
    "qBKEi3hzN8M+BulPX8CDQx3aLRrfxw51J5EuA0NeybngbItgxTi0u6kCgYEA1zr0\r\n"
    "6P5YdOhYHtSWDlkeD49MApcVuzaHnsHZVAhUqu3Rwiy9LRaJLZfr7fQDb9DYJbZp\r\n"
    "sxTRLG6LSAcsR7mw+m+GvNqGt/9pSqbtW+L/VwVWSyF+YYklxZUD3UAAyrDVcDEC\r\n"
    "a5S+jad4Csi/lVHt5ulWIckWL1fJvadn5ubKNDcCgYA+71xVGPP+lsFgTiytfrC8\r\n"
    "5n2rl4MxinJ9+w0I+EbzCKNMYGvTgiU4dJasSMEdiBKs1FMGo7dF8F0BLHF1IsIa\r\n"
    "5Ah2tXItXn9154o9OiTQXMmK6qmRaneM6fhOoeaCwYAhpGxYIpqx/Xr4TOhiag46\r\n"
    "jMMaphAeOvw4t1K2RDziOQKBgQCyPCCU0gxuw/o1jda2CxbZy9EmU/erEX09+0n+\r\n"
    "TOfQpSEPq/z9WaxAFY9LfsdZ0ZktoeHma1bNdL3i6A3DWAM3YSQzQMRPmzOWnqXx\r\n"
    "cgoCBmlvzkzaeLjO5phMoLQHJmmafvuCG6uxov3F8Hi3LyHUF2c8k0nL6ucmJ3vj\r\n"
    "uzu4AQKBgBSASMAJS63M9UJB1Eazy2v2NWw04CmzNxUfWrHuKpd/C2ik4QKu0sRO\r\n"
    "r9KnkDgxxEhjDm7lXhlW12PU42yORst5I3Eaa1Cfi4KPFn/ozt+iNBYrzd8Tyvnb\r\n"
    "qkdECl0+G2Fo/ER4NRCv7a24WNEsOMGzGRqw5cnSJrjbZLYMaIyK\r\n"
    "-----END RSA PRIVATE KEY-----\r\n";

char test_cli_crt[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDPTCCAiWgAwIBAgIBATANBgkqhkiG9w0BAQUFADBFMQswCQYDVQQGEwJGUjEO\r\n"
    "MAwGA1UEBxMFUGFyaXMxDjAMBgNVBAoTBVh5U1NMMRYwFAYDVQQDEw1YeVNTTCBU\r\n"
    "ZXN0IENBMB4XDTA3MDcwNzA1MDEyMFoXDTA4MDcwNjA1MDEyMFowMDELMAkGA1UE\r\n"
    "BhMCRlIxDjAMBgNVBAoTBVh5U1NMMREwDwYDVQQDEwhKb2UgVXNlcjCCASIwDQYJ\r\n"
    "KoZIhvcNAQEBBQADggEPADCCAQoCggEBAKZkg8ANl6kGmLGqKc6KUHb9IsZwb2+K\r\n"
    "jBw83Qb0KuvPVnu3MzEcXFvOZ83g0PL/z8ob5PKr8HP6bVYzhsD65imcCDEEVPk2\r\n"
    "9r0XGTggGjB601Fd8aTShUWE4NLrKw6YNXTXgTNdvhHNxXwqmdNVLkmZjj3ZwYUc\r\n"
    "cEE8eE5jHs8cMDXJLMCwgKIM7Sax22OhSHQHKwifVO4/Fdw5G+Suys8PhMX2jDXM\r\n"
    "ICFwq8ld+bZGoNUtgp48FWhAMfJyTEaHh9LC46KkqGSDRIzx7/4cPB6QqrpzJN0o\r\n"
    "Kr8kH7vdRDTFDmO23D4C5l0Bw/2aC76DhEJpB2bGA4iIszJs+F/PIL8CAwEAAaNN\r\n"
    "MEswCQYDVR0TBAIwADAdBgNVHQ4EFgQUiWX1IvjRdYGt0zz5Sq16x01k0o4wHwYD\r\n"
    "VR0jBBgwFoAUvO4fmOj6K4PkpH1ew3VykzHZMmcwDQYJKoZIhvcNAQEFBQADggEB\r\n"
    "AGdqD7VThJmC+oeeMUHk2TQX2wZNU+GsC+RLjtlenckny95KnljGvMtCznyLkS5D\r\n"
    "fAjLKfR1No8pk5gRdscqgyIuQx5WnHNv4QBZmMsmvDICxzRQaxuPFHbS4aLXldeL\r\n"
    "yOWm5Z4qkMHpCKvA86blYsEkksGDV47fF9ZkOQ8nkh7Z4eY4/5TwqTY72ww5g4NL\r\n"
    "6DZtWpcpGbX99NRaNVzcq9D+ElxkgHnH4YWafOKBclSgqrutbRLi2uZx/QpvuF+i\r\n"
    "sUbe+HFPMWwU5lBv/oOhQkz0VD+HusYtXWS2lG88cT40aNly2CkYUugdTR/b9Uea\r\n"
    "p/i862sL/lO40qlQ0xV5N7U=\r\n" "-----END CERTIFICATE-----\r\n";

char test_cli_key[] =
    "-----BEGIN RSA PRIVATE KEY-----\r\n"
    "MIIEowIBAAKCAQEApmSDwA2XqQaYsaopzopQdv0ixnBvb4qMHDzdBvQq689We7cz\r\n"
    "MRxcW85nzeDQ8v/Pyhvk8qvwc/ptVjOGwPrmKZwIMQRU+Tb2vRcZOCAaMHrTUV3x\r\n"
    "pNKFRYTg0usrDpg1dNeBM12+Ec3FfCqZ01UuSZmOPdnBhRxwQTx4TmMezxwwNcks\r\n"
    "wLCAogztJrHbY6FIdAcrCJ9U7j8V3Dkb5K7Kzw+ExfaMNcwgIXCryV35tkag1S2C\r\n"
    "njwVaEAx8nJMRoeH0sLjoqSoZINEjPHv/hw8HpCqunMk3SgqvyQfu91ENMUOY7bc\r\n"
    "PgLmXQHD/ZoLvoOEQmkHZsYDiIizMmz4X88gvwIDAQABAoIBAE0nBkAjDVN+j4ax\r\n"
    "1DjEwZKqxVkmAUXDBDyDrCjxRoWY2gz7YW1ALUMUbeV0fO5v1zVrwbkUKKZeVBxI\r\n"
    "QA9zRw28H8A6tfvolHgRIcx4dixMh3ePC+DVDJ6zglvKV2ipAwBufKYIrX0r4Io2\r\n"
    "ZqUrNg9CeEYNlkHWceaN12rhYwO82pgHxnB1p5pI42pY7lzyLgSddf5n+M5UBOJI\r\n"
    "gsNCkvbGdv7WQPVFTRDiRgEnCJ3rI8oPSK6MOUWJw3rh2hbkx+ex8NPvEKbzEXiU\r\n"
    "p5j1AlbHIWP5sYBbA1YviFtryAV4fyfLcWPfoqa33Oozofjlwoj0Aixz+6rerLjZ\r\n"
    "cpTSrAECgYEA2oCffUo6HH3Lq9oeWhFCoOyG3YjZmFrJaJwjHnvroX9/pXHqYKog\r\n"
    "TehcjUJBtFZw0klcetYbZCFqT8v9nf0uPlgaiVGCtXf1MSbFXDUFKkYBiFwzdWMT\r\n"
    "Ysmvhff82jMWZ8ecsXTyDRl858SR5WPZ52qEsCc5X2un7QENm6FtVT8CgYEAwvKS\r\n"
    "zQNzuoJETqZX7AalmK3JM8Fdam+Qm5LNMcbvhbKwI8HKMS1VMuqaR0XdAX/iMXAx\r\n"
    "P1VhSsmoSDbsMpxBEZIptpCen/GcqctITxANTakrBHxqb2aQ5EEu7SzgfHZWse3/\r\n"
    "vQEyfcFTBlPIdcZUDzk4/w7WmyivpYtCWoAh1IECgYEA0UYZ+1UJfVpapRj+swMP\r\n"
    "DrQbo7i7t7lUaFYLKNpFX2OPLTWC5txqnlOruTu5VHDqE+5hneDNUUTT3uOg4B2q\r\n"
    "mdmmaNjh2M6wz0e0BVFexhNQynqMaqTe32IOM8DFs3L0xacgg7JfVn6P7CeQGOVe\r\n"
    "wc96kICw6ZxhtJSqpOGipt8CgYBI/0Pw+IXxJK4nNSpe+u4vCYP5mUI9hKEFYCbt\r\n"
    "qKwvyAUknn/zgiIQ+r/iSErFMPmlwXjvWi0gL/qPb+Fp4hCLX8u2zNhY08Px4Gin\r\n"
    "Ej+pANtWxq+kHyfKEI5dyRwV/snfvlqwjy404JsSF3VMhIMdYDPzbb72Qnni5w5l\r\n"
    "jO0eAQKBgBqt9jJMd1JdpemC2dm0BuuDIz2h3/MH+CMjfaDLenVpKykn17B6N92h\r\n"
    "klMesqK3RQzDGwauDw431LQw0R69onn9fCM3wJw2yEC6wC9sF8I8hsNZbt64yZhZ\r\n"
    "4Bi2YRTiHhpEuBqKlhHLDFHneo3SMYh8PU/PDQQcyWGHHUi9z1RE\r\n"
    "-----END RSA PRIVATE KEY-----\r\n";

char xyssl_ca_crt[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIEADCCAuigAwIBAgIBADANBgkqhkiG9w0BAQUFADBjMQswCQYDVQQGEwJVUzEh\r\n"
    "MB8GA1UEChMYVGhlIEdvIERhZGR5IEdyb3VwLCBJbmMuMTEwLwYDVQQLEyhHbyBE\r\n"
    "YWRkeSBDbGFzcyAyIENlcnRpZmljYXRpb24gQXV0aG9yaXR5MB4XDTA0MDYyOTE3\r\n"
    "MDYyMFoXDTM0MDYyOTE3MDYyMFowYzELMAkGA1UEBhMCVVMxITAfBgNVBAoTGFRo\r\n"
    "ZSBHbyBEYWRkeSBHcm91cCwgSW5jLjExMC8GA1UECxMoR28gRGFkZHkgQ2xhc3Mg\r\n"
    "MiBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eTCCASAwDQYJKoZIhvcNAQEBBQADggEN\r\n"
    "ADCCAQgCggEBAN6d1+pXGEmhW+vXX0iG6r7d/+TvZxz0ZWizV3GgXne77ZtJ6XCA\r\n"
    "PVYYYwhv2vLM0D9/AlQiVBDYsoHUwHU9S3/Hd8M+eKsaA7Ugay9qK7HFiH7Eux6w\r\n"
    "wdhFJ2+qN1j3hybX2C32qRe3H3I2TqYXP2WYktsqbl2i/ojgC95/5Y0V4evLOtXi\r\n"
    "EqITLdiOr18SPaAIBQi2XKVlOARFmR6jYGB0xUGlcmIbYsUfb18aQr4CUWWoriMY\r\n"
    "avx4A6lNf4DD+qta/KFApMoZFv6yyO9ecw3ud72a9nmYvLEHZ6IVDd2gWMZEewo+\r\n"
    "YihfukEHU1jPEX44dMX4/7VpkI+EdOqXG68CAQOjgcAwgb0wHQYDVR0OBBYEFNLE\r\n"
    "sNKR1EwRcbNhyz2h/t2oatTjMIGNBgNVHSMEgYUwgYKAFNLEsNKR1EwRcbNhyz2h\r\n"
    "/t2oatTjoWekZTBjMQswCQYDVQQGEwJVUzEhMB8GA1UEChMYVGhlIEdvIERhZGR5\r\n"
    "IEdyb3VwLCBJbmMuMTEwLwYDVQQLEyhHbyBEYWRkeSBDbGFzcyAyIENlcnRpZmlj\r\n"
    "YXRpb24gQXV0aG9yaXR5ggEAMAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcNAQEFBQAD\r\n"
    "ggEBADJL87LKPpH8EsahB4yOd6AzBhRckB4Y9wimPQoZ+YeAEW5p5JYXMP80kWNy\r\n"
    "OO7MHAGjHZQopDH2esRU1/blMVgDoszOYtuURXO1v0XJJLXVggKtI3lpjbi2Tc7P\r\n"
    "TMozI+gciKqdi0FuFskg5YmezTvacPd+mSYgFFQlq25zheabIZ0KbIIOqPjCDPoQ\r\n"
    "HmyW74cNxA9hi63ugyuV+I6ShHI56yDqg+2DzZduCLzrTia2cyvk0/ZM/iZx4mER\r\n"
    "dEr/VxqHD3VILs9RaRegAhJhldXRQLIQTO7ErBBDpqWeCtWVYpoNz4iCxTIM5Cuf\r\n"
    "ReYNnyicsbkqWletNw+vHX/bvZ8=\r\n" "-----END CERTIFICATE-----\r\n";

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/debug.c ************/


/*
    debug.c -- Debugging routines

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if defined _MSC_VER && !defined  snprintf
    #define snprintf  _snprintf
#endif

#if defined _MSC_VER && !defined vsnprintf
    #define vsnprintf _vsnprintf
#endif

/*
    Safely format into a buffer. This works around inconsistencies in snprintf which is dangerous.
 */
int snfmt(char *buf, ssize bufsize, cchar *fmt, ...)
{
    va_list     ap;
    int         n;

    if (bufsize <= 0) {
        return 0;
    }
    va_start(ap, fmt);
#if _WIN32
    /* Windows does not guarantee a null will be appended */
    if ((n = vsnprintf(buf, bufsize - 1, fmt, ap)) < 0) {
        n = 0;
    }
    buf[n] = '\0';
#else
    /* Posix will return the number of characters that would fix in an unlimited buffer -- Ugh, dangerous! */
    n = vsnprintf(buf, bufsize, fmt, ap);
    n = min(n, bufsize);
#endif
    va_end(ap);
    return n;
}

#if ME_EST_LOGGING

char *debug_fmt(cchar *format, ...)
{
    va_list argp;
    static char str[512];
    int maxlen = sizeof(str) - 1;

    va_start(argp, format);
    vsnprintf(str, maxlen, format, argp);
    va_end(argp);
    str[maxlen] = '\0';
    return str;
}


void debug_print_msg(ssl_context *ssl, int level, char *text)
{
    char str[512];
    int maxlen = sizeof(str) - 1;

    if (ssl->f_dbg == NULL) {
        return;
    }
    snprintf(str, maxlen, "%s\n", text);
    str[maxlen] = '\0';
    ssl->f_dbg(ssl->p_dbg, level, str);
}


void debug_print_ret(ssl_context *ssl, int level, char *text, int ret)
{
    char str[512];
    int maxlen = sizeof(str) - 1;

    if (ssl->f_dbg == NULL) {
        return;
    }
    snprintf(str, maxlen, "%s() returned %d (0x%x)\n", text, ret, ret);
    str[maxlen] = '\0';
    ssl->f_dbg(ssl->p_dbg, level, str);
}


void debug_print_buf(ssl_context *ssl, int level, char *text, uchar *buf, int len)
{
    char str[512];
    int i, maxlen = sizeof(str) - 1;

    if (ssl->f_dbg == NULL || len < 0) {
        return;
    }
    snfmt(str, maxlen, "dumping '%s' (%d bytes)\n", text, len);
    str[maxlen] = '\0';
    ssl->f_dbg(ssl->p_dbg, level, str);

    for (i = 0; i < len; i++) {
        if (i >= 4096) {
            break;
        }
        if (i % 16 == 0) {
            if (i > 0) {
                ssl->f_dbg(ssl->p_dbg, level, "\n");
            }
            snfmt(str, maxlen, "%04x: ", i);
            str[maxlen] = '\0';
            ssl->f_dbg(ssl->p_dbg, level, str);
        }
        snfmt(str, maxlen, " %02x", (uint)buf[i]);
        str[maxlen] = '\0';
        ssl->f_dbg(ssl->p_dbg, level, str);
    }
    if (len > 0) {
        ssl->f_dbg(ssl->p_dbg, level, "\n");
    }
}


void debug_print_mpi(ssl_context *ssl, int level, char *text, mpi * X)
{
    char str[512];
    int i, j, k, n, maxlen = sizeof(str) - 1;

    if (ssl->f_dbg == NULL || X == NULL) {
        return;
    }
    for (n = X->n - 1; n >= 0; n--) {
        if (X->p[n] != 0) {
            break;
        }
    }
    snfmt(str, maxlen, "value of '%s' (%u bits) is:\n", text, (int) ((n + 1) * sizeof(t_int)) << 3);

    str[maxlen] = '\0';
    ssl->f_dbg(ssl->p_dbg, level, str);

    for (i = n, j = 0; i >= 0; i--, j++) {
        if (j % (16 / sizeof(t_int)) == 0) {
            if (j > 0) {
                ssl->f_dbg(ssl->p_dbg, level, "\n");
            }
            snfmt(str, maxlen, " ");
            str[maxlen] = '\0';
            ssl->f_dbg(ssl->p_dbg, level, str);
        }
        for (k = sizeof(t_int) - 1; k >= 0; k--) {
            snfmt(str, maxlen, " %02x", (uint) (X->p[i] >> (k << 3)) & 0xFF);
            str[maxlen] = '\0';
            ssl->f_dbg(ssl->p_dbg, level, str);
        }
    }
    ssl->f_dbg(ssl->p_dbg, level, "\n");
}


void debug_print_crt(ssl_context *ssl, int level, char *text, x509_cert * crt)
{
    char cbuf[5120], str[512], *p;
    int i, maxlen;

    if (ssl->f_dbg == NULL || crt == NULL) {
        return;
    }
    maxlen = sizeof(str) - 1;

    i = 0;
    while (crt != NULL && crt->next != NULL) {
        p = x509parse_cert_info("", cbuf, sizeof(cbuf), crt);
        snfmt(str, maxlen, "%s #%d:\n%s", text, ++i, p);
        str[maxlen] = '\0';
        ssl->f_dbg(ssl->p_dbg, level, str);
        debug_print_mpi(ssl, level, "crt->rsa.N", &crt->rsa.N);
        debug_print_mpi(ssl, level, "crt->rsa.E", &crt->rsa.E);
        crt = crt->next;
    }
}

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/des.c ************/


/*
    des.c -- FIPS-46-3 compliant Triple-DES implementation

    DES, on which TDES is based, was originally designed by Horst Feistel
    at IBM in 1974, and was adopted as a standard by NIST (formerly NBS).

    http://csrc.nist.gov/publications/fips/fips46-3/fips46-3.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_DES

/*
    32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ] << 24 )    \
            | ( (ulong) (b)[(i) + 1] << 16 )    \
            | ( (ulong) (b)[(i) + 2] <<  8 )    \
            | ( (ulong) (b)[(i) + 3]       );   \
    }
#endif

#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 2] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 3] = (uchar) ( (n)       );   \
    }
#endif

/*
    Expanded DES S-boxes
 */
static const ulong SB1[64] = {
    0x01010400, 0x00000000, 0x00010000, 0x01010404,
    0x01010004, 0x00010404, 0x00000004, 0x00010000,
    0x00000400, 0x01010400, 0x01010404, 0x00000400,
    0x01000404, 0x01010004, 0x01000000, 0x00000004,
    0x00000404, 0x01000400, 0x01000400, 0x00010400,
    0x00010400, 0x01010000, 0x01010000, 0x01000404,
    0x00010004, 0x01000004, 0x01000004, 0x00010004,
    0x00000000, 0x00000404, 0x00010404, 0x01000000,
    0x00010000, 0x01010404, 0x00000004, 0x01010000,
    0x01010400, 0x01000000, 0x01000000, 0x00000400,
    0x01010004, 0x00010000, 0x00010400, 0x01000004,
    0x00000400, 0x00000004, 0x01000404, 0x00010404,
    0x01010404, 0x00010004, 0x01010000, 0x01000404,
    0x01000004, 0x00000404, 0x00010404, 0x01010400,
    0x00000404, 0x01000400, 0x01000400, 0x00000000,
    0x00010004, 0x00010400, 0x00000000, 0x01010004
};

static const ulong SB2[64] = {
    0x80108020, 0x80008000, 0x00008000, 0x00108020,
    0x00100000, 0x00000020, 0x80100020, 0x80008020,
    0x80000020, 0x80108020, 0x80108000, 0x80000000,
    0x80008000, 0x00100000, 0x00000020, 0x80100020,
    0x00108000, 0x00100020, 0x80008020, 0x00000000,
    0x80000000, 0x00008000, 0x00108020, 0x80100000,
    0x00100020, 0x80000020, 0x00000000, 0x00108000,
    0x00008020, 0x80108000, 0x80100000, 0x00008020,
    0x00000000, 0x00108020, 0x80100020, 0x00100000,
    0x80008020, 0x80100000, 0x80108000, 0x00008000,
    0x80100000, 0x80008000, 0x00000020, 0x80108020,
    0x00108020, 0x00000020, 0x00008000, 0x80000000,
    0x00008020, 0x80108000, 0x00100000, 0x80000020,
    0x00100020, 0x80008020, 0x80000020, 0x00100020,
    0x00108000, 0x00000000, 0x80008000, 0x00008020,
    0x80000000, 0x80100020, 0x80108020, 0x00108000
};

static const ulong SB3[64] = {
    0x00000208, 0x08020200, 0x00000000, 0x08020008,
    0x08000200, 0x00000000, 0x00020208, 0x08000200,
    0x00020008, 0x08000008, 0x08000008, 0x00020000,
    0x08020208, 0x00020008, 0x08020000, 0x00000208,
    0x08000000, 0x00000008, 0x08020200, 0x00000200,
    0x00020200, 0x08020000, 0x08020008, 0x00020208,
    0x08000208, 0x00020200, 0x00020000, 0x08000208,
    0x00000008, 0x08020208, 0x00000200, 0x08000000,
    0x08020200, 0x08000000, 0x00020008, 0x00000208,
    0x00020000, 0x08020200, 0x08000200, 0x00000000,
    0x00000200, 0x00020008, 0x08020208, 0x08000200,
    0x08000008, 0x00000200, 0x00000000, 0x08020008,
    0x08000208, 0x00020000, 0x08000000, 0x08020208,
    0x00000008, 0x00020208, 0x00020200, 0x08000008,
    0x08020000, 0x08000208, 0x00000208, 0x08020000,
    0x00020208, 0x00000008, 0x08020008, 0x00020200
};

static const ulong SB4[64] = {
    0x00802001, 0x00002081, 0x00002081, 0x00000080,
    0x00802080, 0x00800081, 0x00800001, 0x00002001,
    0x00000000, 0x00802000, 0x00802000, 0x00802081,
    0x00000081, 0x00000000, 0x00800080, 0x00800001,
    0x00000001, 0x00002000, 0x00800000, 0x00802001,
    0x00000080, 0x00800000, 0x00002001, 0x00002080,
    0x00800081, 0x00000001, 0x00002080, 0x00800080,
    0x00002000, 0x00802080, 0x00802081, 0x00000081,
    0x00800080, 0x00800001, 0x00802000, 0x00802081,
    0x00000081, 0x00000000, 0x00000000, 0x00802000,
    0x00002080, 0x00800080, 0x00800081, 0x00000001,
    0x00802001, 0x00002081, 0x00002081, 0x00000080,
    0x00802081, 0x00000081, 0x00000001, 0x00002000,
    0x00800001, 0x00002001, 0x00802080, 0x00800081,
    0x00002001, 0x00002080, 0x00800000, 0x00802001,
    0x00000080, 0x00800000, 0x00002000, 0x00802080
};

static const ulong SB5[64] = {
    0x00000100, 0x02080100, 0x02080000, 0x42000100,
    0x00080000, 0x00000100, 0x40000000, 0x02080000,
    0x40080100, 0x00080000, 0x02000100, 0x40080100,
    0x42000100, 0x42080000, 0x00080100, 0x40000000,
    0x02000000, 0x40080000, 0x40080000, 0x00000000,
    0x40000100, 0x42080100, 0x42080100, 0x02000100,
    0x42080000, 0x40000100, 0x00000000, 0x42000000,
    0x02080100, 0x02000000, 0x42000000, 0x00080100,
    0x00080000, 0x42000100, 0x00000100, 0x02000000,
    0x40000000, 0x02080000, 0x42000100, 0x40080100,
    0x02000100, 0x40000000, 0x42080000, 0x02080100,
    0x40080100, 0x00000100, 0x02000000, 0x42080000,
    0x42080100, 0x00080100, 0x42000000, 0x42080100,
    0x02080000, 0x00000000, 0x40080000, 0x42000000,
    0x00080100, 0x02000100, 0x40000100, 0x00080000,
    0x00000000, 0x40080000, 0x02080100, 0x40000100
};

static const ulong SB6[64] = {
    0x20000010, 0x20400000, 0x00004000, 0x20404010,
    0x20400000, 0x00000010, 0x20404010, 0x00400000,
    0x20004000, 0x00404010, 0x00400000, 0x20000010,
    0x00400010, 0x20004000, 0x20000000, 0x00004010,
    0x00000000, 0x00400010, 0x20004010, 0x00004000,
    0x00404000, 0x20004010, 0x00000010, 0x20400010,
    0x20400010, 0x00000000, 0x00404010, 0x20404000,
    0x00004010, 0x00404000, 0x20404000, 0x20000000,
    0x20004000, 0x00000010, 0x20400010, 0x00404000,
    0x20404010, 0x00400000, 0x00004010, 0x20000010,
    0x00400000, 0x20004000, 0x20000000, 0x00004010,
    0x20000010, 0x20404010, 0x00404000, 0x20400000,
    0x00404010, 0x20404000, 0x00000000, 0x20400010,
    0x00000010, 0x00004000, 0x20400000, 0x00404010,
    0x00004000, 0x00400010, 0x20004010, 0x00000000,
    0x20404000, 0x20000000, 0x00400010, 0x20004010
};

static const ulong SB7[64] = {
    0x00200000, 0x04200002, 0x04000802, 0x00000000,
    0x00000800, 0x04000802, 0x00200802, 0x04200800,
    0x04200802, 0x00200000, 0x00000000, 0x04000002,
    0x00000002, 0x04000000, 0x04200002, 0x00000802,
    0x04000800, 0x00200802, 0x00200002, 0x04000800,
    0x04000002, 0x04200000, 0x04200800, 0x00200002,
    0x04200000, 0x00000800, 0x00000802, 0x04200802,
    0x00200800, 0x00000002, 0x04000000, 0x00200800,
    0x04000000, 0x00200800, 0x00200000, 0x04000802,
    0x04000802, 0x04200002, 0x04200002, 0x00000002,
    0x00200002, 0x04000000, 0x04000800, 0x00200000,
    0x04200800, 0x00000802, 0x00200802, 0x04200800,
    0x00000802, 0x04000002, 0x04200802, 0x04200000,
    0x00200800, 0x00000000, 0x00000002, 0x04200802,
    0x00000000, 0x00200802, 0x04200000, 0x00000800,
    0x04000002, 0x04000800, 0x00000800, 0x00200002
};

static const ulong SB8[64] = {
    0x10001040, 0x00001000, 0x00040000, 0x10041040,
    0x10000000, 0x10001040, 0x00000040, 0x10000000,
    0x00040040, 0x10040000, 0x10041040, 0x00041000,
    0x10041000, 0x00041040, 0x00001000, 0x00000040,
    0x10040000, 0x10000040, 0x10001000, 0x00001040,
    0x00041000, 0x00040040, 0x10040040, 0x10041000,
    0x00001040, 0x00000000, 0x00000000, 0x10040040,
    0x10000040, 0x10001000, 0x00041040, 0x00040000,
    0x00041040, 0x00040000, 0x10041000, 0x00001000,
    0x00000040, 0x10040040, 0x00001000, 0x00041040,
    0x10001000, 0x00000040, 0x10000040, 0x10040000,
    0x10040040, 0x10000000, 0x00040000, 0x10001040,
    0x00000000, 0x10041040, 0x00040040, 0x10000040,
    0x10040000, 0x10001000, 0x10001040, 0x00000000,
    0x10041040, 0x00041000, 0x00041000, 0x00001040,
    0x00001040, 0x00040040, 0x10000000, 0x10041000
};

/*
 * PC1: left and right halves bit-swap
 */
static const ulong LHs[16] = {
    0x00000000, 0x00000001, 0x00000100, 0x00000101,
    0x00010000, 0x00010001, 0x00010100, 0x00010101,
    0x01000000, 0x01000001, 0x01000100, 0x01000101,
    0x01010000, 0x01010001, 0x01010100, 0x01010101
};

static const ulong RHs[16] = {
    0x00000000, 0x01000000, 0x00010000, 0x01010000,
    0x00000100, 0x01000100, 0x00010100, 0x01010100,
    0x00000001, 0x01000001, 0x00010001, 0x01010001,
    0x00000101, 0x01000101, 0x00010101, 0x01010101,
};

/*
 * Initial Permutation macro
 */
#define DES_IP(X,Y)                                                 \
    {                                                               \
        T = ((X >>  4) ^ Y) & 0x0F0F0F0F; Y ^= T; X ^= (T <<  4);   \
        T = ((X >> 16) ^ Y) & 0x0000FFFF; Y ^= T; X ^= (T << 16);   \
        T = ((Y >>  2) ^ X) & 0x33333333; X ^= T; Y ^= (T <<  2);   \
        T = ((Y >>  8) ^ X) & 0x00FF00FF; X ^= T; Y ^= (T <<  8);   \
        Y = ((Y << 1) | (Y >> 31)) & 0xFFFFFFFF;                    \
        T = (X ^ Y) & 0xAAAAAAAA; Y ^= T; X ^= T;                   \
        X = ((X << 1) | (X >> 31)) & 0xFFFFFFFF;                    \
    }

/*
 * Final Permutation macro
 */
#define DES_FP(X,Y)                                                 \
    {                                                               \
        X = ((X << 31) | (X >> 1)) & 0xFFFFFFFF;                    \
        T = (X ^ Y) & 0xAAAAAAAA; X ^= T; Y ^= T;                   \
        Y = ((Y << 31) | (Y >> 1)) & 0xFFFFFFFF;                    \
        T = ((Y >>  8) ^ X) & 0x00FF00FF; X ^= T; Y ^= (T <<  8);   \
        T = ((Y >>  2) ^ X) & 0x33333333; X ^= T; Y ^= (T <<  2);   \
        T = ((X >> 16) ^ Y) & 0x0000FFFF; Y ^= T; X ^= (T << 16);   \
        T = ((X >>  4) ^ Y) & 0x0F0F0F0F; Y ^= T; X ^= (T <<  4);   \
    }

/*
 * DES round macro
 */
#define DES_ROUND(X,Y)                          \
    {                                           \
        T = *SK++ ^ X;                          \
        Y ^= SB8[ (T      ) & 0x3F ] ^          \
            SB6[ (T >>   8) & 0x3F ] ^          \
            SB4[ (T >> 16) & 0x3F ] ^           \
            SB2[ (T >> 24) & 0x3F ];            \
                                                \
        T = *SK++ ^ ((X << 28) | (X >> 4));     \
        Y ^= SB7[ (T      ) & 0x3F ] ^          \
            SB5[ (T >>   8) & 0x3F ] ^          \
            SB3[ (T >> 16) & 0x3F ] ^           \
            SB1[ (T >> 24) & 0x3F ];            \
    }

#define SWAP(a,b) { ulong t = a; a = b; b = t; t = 0; }

static void des_setkey(ulong SK[32], uchar key[8])
{
    int i;
    ulong X, Y, T;

    GET_ULONG_BE(X, key, 0);
    GET_ULONG_BE(Y, key, 4);

    /*
     * Permuted Choice 1
     */
    T = ((Y >> 4) ^ X) & 0x0F0F0F0F;
    X ^= T;
    Y ^= (T << 4);
    T = ((Y) ^ X) & 0x10101010;
    X ^= T;
    Y ^= (T);

    X = (LHs[(X) & 0xF] << 3) | (LHs[(X >> 8) & 0xF] << 2)
        | (LHs[(X >> 16) & 0xF] << 1) | (LHs[(X >> 24) & 0xF])
        | (LHs[(X >> 5) & 0xF] << 7) | (LHs[(X >> 13) & 0xF] << 6)
        | (LHs[(X >> 21) & 0xF] << 5) | (LHs[(X >> 29) & 0xF] << 4);

    Y = (RHs[(Y >> 1) & 0xF] << 3) | (RHs[(Y >> 9) & 0xF] << 2)
        | (RHs[(Y >> 17) & 0xF] << 1) | (RHs[(Y >> 25) & 0xF])
        | (RHs[(Y >> 4) & 0xF] << 7) | (RHs[(Y >> 12) & 0xF] << 6)
        | (RHs[(Y >> 20) & 0xF] << 5) | (RHs[(Y >> 28) & 0xF] << 4);

    X &= 0x0FFFFFFF;
    Y &= 0x0FFFFFFF;

    /*
       calculate subkeys
     */
    for (i = 0; i < 16; i++) {
        if (i < 2 || i == 8 || i == 15) {
            X = ((X << 1) | (X >> 27)) & 0x0FFFFFFF;
            Y = ((Y << 1) | (Y >> 27)) & 0x0FFFFFFF;
        } else {
            X = ((X << 2) | (X >> 26)) & 0x0FFFFFFF;
            Y = ((Y << 2) | (Y >> 26)) & 0x0FFFFFFF;
        }
        *SK++ = ((X << 4) & 0x24000000) | ((X << 28) & 0x10000000)
            | ((X << 14) & 0x08000000) | ((X << 18) & 0x02080000)
            | ((X << 6) & 0x01000000) | ((X << 9) & 0x00200000)
            | ((X >> 1) & 0x00100000) | ((X << 10) & 0x00040000)
            | ((X << 2) & 0x00020000) | ((X >> 10) & 0x00010000)
            | ((Y >> 13) & 0x00002000) | ((Y >> 4) & 0x00001000)
            | ((Y << 6) & 0x00000800) | ((Y >> 1) & 0x00000400)
            | ((Y >> 14) & 0x00000200) | ((Y) & 0x00000100)
            | ((Y >> 5) & 0x00000020) | ((Y >> 10) & 0x00000010)
            | ((Y >> 3) & 0x00000008) | ((Y >> 18) & 0x00000004)
            | ((Y >> 26) & 0x00000002) | ((Y >> 24) & 0x00000001);

        *SK++ = ((X << 15) & 0x20000000) | ((X << 17) & 0x10000000)
            | ((X << 10) & 0x08000000) | ((X << 22) & 0x04000000)
            | ((X >> 2) & 0x02000000) | ((X << 1) & 0x01000000)
            | ((X << 16) & 0x00200000) | ((X << 11) & 0x00100000)
            | ((X << 3) & 0x00080000) | ((X >> 6) & 0x00040000)
            | ((X << 15) & 0x00020000) | ((X >> 4) & 0x00010000)
            | ((Y >> 2) & 0x00002000) | ((Y << 8) & 0x00001000)
            | ((Y >> 14) & 0x00000808) | ((Y >> 9) & 0x00000400)
            | ((Y) & 0x00000200) | ((Y << 7) & 0x00000100)
            | ((Y >> 7) & 0x00000020) | ((Y >> 3) & 0x00000011)
            | ((Y << 2) & 0x00000004) | ((Y >> 21) & 0x00000002);
    }
}


/*
    DES key schedule (56-bit, encryption)
 */
void des_setkey_enc(des_context *ctx, uchar key[8])
{
    des_setkey(ctx->sk, key);
}


/*
    DES key schedule (56-bit, decryption)
 */
void des_setkey_dec(des_context *ctx, uchar key[8])
{
    int i;

    des_setkey(ctx->sk, key);

    for (i = 0; i < 16; i += 2) {
        SWAP(ctx->sk[i], ctx->sk[30 - i]);
        SWAP(ctx->sk[i + 1], ctx->sk[31 - i]);
    }
}

static void des3_set2key(ulong esk[96], ulong dsk[96], uchar key[16])
{
    int i;

    des_setkey(esk, key);
    des_setkey(dsk + 32, key + 8);

    for (i = 0; i < 32; i += 2) {
        dsk[i] = esk[30 - i];
        dsk[i + 1] = esk[31 - i];

        esk[i + 32] = dsk[62 - i];
        esk[i + 33] = dsk[63 - i];

        esk[i + 64] = esk[i];
        esk[i + 65] = esk[i + 1];

        dsk[i + 64] = dsk[i];
        dsk[i + 65] = dsk[i + 1];
    }
}


/*
    Triple-DES key schedule (112-bit, encryption)
 */
void des3_set2key_enc(des3_context *ctx, uchar key[16])
{
    ulong   sk[96];

    des3_set2key(ctx->sk, sk, key);
    memset(sk, 0, sizeof(sk));
}


/*
    Triple-DES key schedule (112-bit, decryption)
 */
void des3_set2key_dec(des3_context *ctx, uchar key[16])
{
    ulong sk[96];

    des3_set2key(sk, ctx->sk, key);
    memset(sk, 0, sizeof(sk));
}


static void des3_set3key(ulong esk[96], ulong dsk[96], uchar key[24])
{
    int i;

    des_setkey(esk, key);
    des_setkey(dsk + 32, key + 8);
    des_setkey(esk + 64, key + 16);

    for (i = 0; i < 32; i += 2) {
        dsk[i] = esk[94 - i];
        dsk[i + 1] = esk[95 - i];
        esk[i + 32] = dsk[62 - i];
        esk[i + 33] = dsk[63 - i];
        dsk[i + 64] = esk[30 - i];
        dsk[i + 65] = esk[31 - i];
    }
}


/*
    Triple-DES key schedule (168-bit, encryption)
 */
void des3_set3key_enc(des3_context *ctx, uchar key[24])
{
    ulong   sk[96];

    des3_set3key(ctx->sk, sk, key);
    memset(sk, 0, sizeof(sk));
}


/*
    Triple-DES key schedule (168-bit, decryption)
 */
void des3_set3key_dec(des3_context *ctx, uchar key[24])
{
    ulong sk[96];

    des3_set3key(sk, ctx->sk, key);
    memset(sk, 0, sizeof(sk));
}


/*
    DES-ECB block encryption/decryption
 */
void des_crypt_ecb(des_context *ctx, uchar input[8], uchar output[8])
{
    ulong   X, Y, T, *SK;
    int     i;

    SK = ctx->sk;
    GET_ULONG_BE(X, input, 0);
    GET_ULONG_BE(Y, input, 4);
    DES_IP(X, Y);

    for (i = 0; i < 8; i++) {
        DES_ROUND(Y, X);
        DES_ROUND(X, Y);
    }
    DES_FP(Y, X);
    PUT_ULONG_BE(Y, output, 0);
    PUT_ULONG_BE(X, output, 4);
}


/*
    DES-CBC buffer encryption/decryption
 */
void des_crypt_cbc(des_context *ctx, int mode, int length, uchar iv[8], uchar *input, uchar *output)
{
    int i;
    uchar temp[8];

    if (mode == DES_ENCRYPT) {
        while (length > 0) {
            for (i = 0; i < 8; i++) {
                output[i] = (uchar)(input[i] ^ iv[i]);
            }
            des_crypt_ecb(ctx, output, output);
            memcpy(iv, output, 8);
            input += 8;
            output += 8;
            length -= 8;
        }
    } else {
        /* DES_DECRYPT */
        while (length > 0) {
            memcpy(temp, input, 8);
            des_crypt_ecb(ctx, input, output);
            for (i = 0; i < 8; i++) {
                output[i] = (uchar)(output[i] ^ iv[i]);
            }
            memcpy(iv, temp, 8);
            input += 8;
            output += 8;
            length -= 8;
        }
    }
}

/*
    3DES-ECB block encryption/decryption
 */
void des3_crypt_ecb(des3_context *ctx, uchar input[8], uchar output[8])
{
    int i;
    ulong X, Y, T, *SK;

    SK = ctx->sk;
    GET_ULONG_BE(X, input, 0);
    GET_ULONG_BE(Y, input, 4);
    DES_IP(X, Y);

    for (i = 0; i < 8; i++) {
        DES_ROUND(Y, X);
        DES_ROUND(X, Y);
    }
    for (i = 0; i < 8; i++) {
        DES_ROUND(X, Y);
        DES_ROUND(Y, X);
    }
    for (i = 0; i < 8; i++) {
        DES_ROUND(Y, X);
        DES_ROUND(X, Y);
    }
    DES_FP(Y, X);
    PUT_ULONG_BE(Y, output, 0);
    PUT_ULONG_BE(X, output, 4);
}


/*
    3DES-CBC buffer encryption/decryption
 */
void des3_crypt_cbc(des3_context *ctx, int mode, int length, uchar iv[8], uchar *input, uchar *output)
{
    int i;
    uchar temp[8];

    if (mode == DES_ENCRYPT) {
        while (length > 0) {
            for (i = 0; i < 8; i++) {
                output[i] = (uchar)(input[i] ^ iv[i]);
            }
            des3_crypt_ecb(ctx, output, output);
            memcpy(iv, output, 8);
            input += 8;
            output += 8;
            length -= 8;
        }
    } else {
        /* DES_DECRYPT */
        while (length > 0) {
            memcpy(temp, input, 8);
            des3_crypt_ecb(ctx, input, output);
            for (i = 0; i < 8; i++) {
                output[i] = (uchar)(output[i] ^ iv[i]);
            }
            memcpy(iv, temp, 8);
            input += 8;
            output += 8;
            length -= 8;
        }
    }
}


#undef SWAP
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/dhm.c ************/


/*
    dhm.c -- Diffie-Hellman-Merkle key exchange

    http://www.cacr.math.uwaterloo.ca/hac/ (chapter 12)
 
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_DHM
/*
   helper to validate the mpi size and import it
 */
static int dhm_read_bignum(mpi * X, uchar **p, uchar *end)
{
    int ret, n;

    if (end - *p < 2) {
        return EST_ERR_DHM_BAD_INPUT_DATA;
    }
    n = ((*p)[0] << 8) | (*p)[1];
    (*p) += 2;

    if ((int)(end - *p) < n) {
        return EST_ERR_DHM_BAD_INPUT_DATA;
    }
    if ((ret = mpi_read_binary(X, *p, n)) != 0) {
        return EST_ERR_DHM_READ_PARAMS_FAILED | ret;
    }
    (*p) += n;
    return 0;
}


/*
    Parse the ServerKeyExchange parameters
 */
int dhm_read_params(dhm_context *ctx, uchar **p, uchar *end)
{
    int ret, n;

    memset(ctx, 0, sizeof(dhm_context));

    if ((ret = dhm_read_bignum(&ctx->P, p, end)) != 0 ||
        (ret = dhm_read_bignum(&ctx->G, p, end)) != 0 ||
        (ret = dhm_read_bignum(&ctx->GY, p, end)) != 0)
        return ret;

    ctx->len = mpi_size(&ctx->P);

    if (end - *p < 2)
        return EST_ERR_DHM_BAD_INPUT_DATA;

    n = ((*p)[0] << 8) | (*p)[1];
    (*p) += 2;

    if (end != *p + n)
        return EST_ERR_DHM_BAD_INPUT_DATA;
    return 0;
}


/*
   Setup and write the ServerKeyExchange parameters
 */
int dhm_make_params(dhm_context *ctx, int x_size, uchar *output, int *olen, int (*f_rng) (void *), void *p_rng)
{
    int i, ret, n, n1, n2, n3;
    uchar *p;

    /*
        Generate X and calculate GX = G^X mod P
     */
    n = x_size / sizeof(t_int);
    MPI_CHK(mpi_grow(&ctx->X, n));
    MPI_CHK(mpi_lset(&ctx->X, 0));

    n = x_size >> 3;
    p = (uchar *)ctx->X.p;
    for (i = 0; i < n; i++)
        *p++ = (uchar)f_rng(p_rng);

    while (mpi_cmp_mpi(&ctx->X, &ctx->P) >= 0)
        mpi_shift_r(&ctx->X, 1);

    MPI_CHK(mpi_exp_mod(&ctx->GX, &ctx->G, &ctx->X, &ctx->P, &ctx->RP));

    /*
        Export P, G, GX
     */
#define DHM_MPI_EXPORT(X,n) \
    MPI_CHK(mpi_write_binary( X, p + 2, n ) ); \
    *p++ = (uchar)( n >> 8 ); \
    *p++ = (uchar)( n      ); p += n;

    n1 = mpi_size(&ctx->P);
    n2 = mpi_size(&ctx->G);
    n3 = mpi_size(&ctx->GX);

    p = output;
    DHM_MPI_EXPORT(&ctx->P, n1);
    DHM_MPI_EXPORT(&ctx->G, n2);
    DHM_MPI_EXPORT(&ctx->GX, n3);

    *olen = p - output;
    ctx->len = n1;

cleanup:
    if (ret != 0)
        return ret | EST_ERR_DHM_MAKE_PARAMS_FAILED;
    return 0;
}


/*
   Import the peer's public value G^Y
 */
int dhm_read_public(dhm_context *ctx, uchar *input, int ilen)
{
    int ret;

    if (ctx == NULL || ilen < 1 || ilen > ctx->len) {
        return EST_ERR_DHM_BAD_INPUT_DATA;
    }
    if ((ret = mpi_read_binary(&ctx->GY, input, ilen)) != 0) {
        return EST_ERR_DHM_READ_PUBLIC_FAILED | ret;
    }
    return 0;
}


/*
   Create own private value X and export G^X
 */
int dhm_make_public(dhm_context *ctx, int x_size, uchar *output, int olen, int (*f_rng) (void *), void *p_rng)
{
    int ret, i, n;
    uchar *p;

    if (ctx == NULL || olen < 1 || olen > ctx->len) {
        return EST_ERR_DHM_BAD_INPUT_DATA;
    }

    /*
        generate X and calculate GX = G^X mod P
     */
    n = x_size / sizeof(t_int);
    MPI_CHK(mpi_grow(&ctx->X, n));
    MPI_CHK(mpi_lset(&ctx->X, 0));

    n = x_size >> 3;
    p = (uchar *)ctx->X.p;
    for (i = 0; i < n; i++) {
        *p++ = (uchar)f_rng(p_rng);
    }
    while (mpi_cmp_mpi(&ctx->X, &ctx->P) >= 0) {
        mpi_shift_r(&ctx->X, 1);
    }
    MPI_CHK(mpi_exp_mod(&ctx->GX, &ctx->G, &ctx->X, &ctx->P, &ctx->RP));
    MPI_CHK(mpi_write_binary(&ctx->GX, output, olen));

cleanup:
    if (ret != 0) {
        return EST_ERR_DHM_MAKE_PUBLIC_FAILED | ret;
    }
    return 0;
}


/*
    Derive and export the shared secret (G^Y)^X mod P
 */
int dhm_calc_secret(dhm_context *ctx, uchar *output, int *olen)
{
    int ret;

    if (ctx == NULL || *olen < ctx->len) {
        return EST_ERR_DHM_BAD_INPUT_DATA;
    }
    MPI_CHK(mpi_exp_mod(&ctx->K, &ctx->GY, &ctx->X, &ctx->P, &ctx->RP));
    *olen = mpi_size(&ctx->K);
    MPI_CHK(mpi_write_binary(&ctx->K, output, *olen));

cleanup:
    if (ret != 0) {
        return EST_ERR_DHM_CALC_SECRET_FAILED | ret;
    }
    return 0;
}


/*
    Free the components of a DHM key
 */
void dhm_free(dhm_context * ctx)
{
    mpi_free(&ctx->RP, &ctx->K, &ctx->GY, &ctx->GX, &ctx->X, &ctx->G, &ctx->P, NULL);
}

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/havege.c ************/


/*
    havege.c -- Hardware Volatile Entropy Gathering and Expansion

    The HAVEGE RNG was designed by Andre Seznec in 2002.
    http://www.irisa.fr/caps/projects/hipsor/publi.php

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_HAVEGE

/* Undefine for windows */
#undef IN

/*
   On average, one iteration accesses two 8-word blocks in the havege WALK table, and generates 16 words in the RES array.
  
   The data read in the WALK table is updated and permuted after each use. The result of the hardware clock counter read is
   used for this update.
  
   25 conditional tests are present. The conditional tests are grouped in two nested groups of 12 conditional tests and 1
   test that controls the permutation; on average, there should be 6 tests executed and 3 of them should be mispredicted.
 */

#define SWAP(X,Y) { int *T = X; X = Y; Y = T; }

#define TST1_ENTER if( PTEST & 1 ) { PTEST ^= 3; PTEST >>= 1;
#define TST2_ENTER if( PTEST & 1 ) { PTEST ^= 3; PTEST >>= 1;

#define TST1_LEAVE U1++; }
#define TST2_LEAVE U2++; }

#define ONE_ITERATION                                   \
                                                        \
    PTEST = PT1 >> 20;                                  \
                                                        \
    TST1_ENTER  TST1_ENTER  TST1_ENTER  TST1_ENTER      \
    TST1_ENTER  TST1_ENTER  TST1_ENTER  TST1_ENTER      \
    TST1_ENTER  TST1_ENTER  TST1_ENTER  TST1_ENTER      \
                                                        \
    TST1_LEAVE  TST1_LEAVE  TST1_LEAVE  TST1_LEAVE      \
    TST1_LEAVE  TST1_LEAVE  TST1_LEAVE  TST1_LEAVE      \
    TST1_LEAVE  TST1_LEAVE  TST1_LEAVE  TST1_LEAVE      \
                                                        \
    PTX = (PT1 >> 18) & 7;                              \
    PT1 &= 0x1FFF;                                      \
    PT2 &= 0x1FFF;                                      \
    CLK = (int) hardclock();                            \
                                                        \
    i = 0;                                              \
    A = &WALK[PT1    ]; RES[i++] ^= *A;                 \
    B = &WALK[PT2    ]; RES[i++] ^= *B;                 \
    C = &WALK[PT1 ^ 1]; RES[i++] ^= *C;                 \
    D = &WALK[PT2 ^ 4]; RES[i++] ^= *D;                 \
                                                        \
    IN = (*A >> (1)) ^ (*A << (31)) ^ CLK;              \
    *A = (*B >> (2)) ^ (*B << (30)) ^ CLK;              \
    *B = IN ^ U1;                                       \
    *C = (*C >> (3)) ^ (*C << (29)) ^ CLK;              \
    *D = (*D >> (4)) ^ (*D << (28)) ^ CLK;              \
                                                        \
    A = &WALK[PT1 ^ 2]; RES[i++] ^= *A;                 \
    B = &WALK[PT2 ^ 2]; RES[i++] ^= *B;                 \
    C = &WALK[PT1 ^ 3]; RES[i++] ^= *C;                 \
    D = &WALK[PT2 ^ 6]; RES[i++] ^= *D;                 \
                                                        \
    if( PTEST & 1 ) SWAP( A, C );                       \
                                                        \
    IN = (*A >> (5)) ^ (*A << (27)) ^ CLK;              \
    *A = (*B >> (6)) ^ (*B << (26)) ^ CLK;              \
    *B = IN; CLK = (int) hardclock();                   \
    *C = (*C >> (7)) ^ (*C << (25)) ^ CLK;              \
    *D = (*D >> (8)) ^ (*D << (24)) ^ CLK;              \
                                                        \
    A = &WALK[PT1 ^ 4];                                 \
    B = &WALK[PT2 ^ 1];                                 \
                                                        \
    PTEST = PT2 >> 1;                                   \
                                                        \
    PT2 = (RES[(i - 8) ^ PTY] ^ WALK[PT2 ^ PTY ^ 7]);   \
    PT2 = ((PT2 & 0x1FFF) & (~8)) ^ ((PT1 ^ 8) & 0x8);  \
    PTY = (PT2 >> 10) & 7;                              \
                                                        \
    TST2_ENTER  TST2_ENTER  TST2_ENTER  TST2_ENTER      \
    TST2_ENTER  TST2_ENTER  TST2_ENTER  TST2_ENTER      \
    TST2_ENTER  TST2_ENTER  TST2_ENTER  TST2_ENTER      \
                                                        \
    TST2_LEAVE  TST2_LEAVE  TST2_LEAVE  TST2_LEAVE      \
    TST2_LEAVE  TST2_LEAVE  TST2_LEAVE  TST2_LEAVE      \
    TST2_LEAVE  TST2_LEAVE  TST2_LEAVE  TST2_LEAVE      \
                                                        \
    C = &WALK[PT1 ^ 5];                                 \
    D = &WALK[PT2 ^ 5];                                 \
                                                        \
    RES[i++] ^= *A;                                     \
    RES[i++] ^= *B;                                     \
    RES[i++] ^= *C;                                     \
    RES[i++] ^= *D;                                     \
                                                        \
    IN = (*A >> ( 9)) ^ (*A << (23)) ^ CLK;             \
    *A = (*B >> (10)) ^ (*B << (22)) ^ CLK;             \
    *B = IN ^ U2;                                       \
    *C = (*C >> (11)) ^ (*C << (21)) ^ CLK;             \
    *D = (*D >> (12)) ^ (*D << (20)) ^ CLK;             \
                                                        \
    A = &WALK[PT1 ^ 6]; RES[i++] ^= *A;                 \
    B = &WALK[PT2 ^ 3]; RES[i++] ^= *B;                 \
    C = &WALK[PT1 ^ 7]; RES[i++] ^= *C;                 \
    D = &WALK[PT2 ^ 7]; RES[i++] ^= *D;                 \
                                                        \
    IN = (*A >> (13)) ^ (*A << (19)) ^ CLK;             \
    *A = (*B >> (14)) ^ (*B << (18)) ^ CLK;             \
    *B = IN;                                            \
    *C = (*C >> (15)) ^ (*C << (17)) ^ CLK;             \
    *D = (*D >> (16)) ^ (*D << (16)) ^ CLK;             \
                                                        \
    PT1 = ( RES[(i - 8) ^ PTX] ^                        \
            WALK[PT1 ^ PTX ^ 7] ) & (~1);               \
    PT1 ^= (PT2 ^ 0x10) & 0x10;                         \
                                                        \
    for (n++, i = 0; i < 16; i++)                       \
        hs->pool[n % COLLECT_SIZE] ^= RES[i];

/*
    Entropy gathering function
 */
static void havege_fill(havege_state * hs)
{
    int i, n = 0;
    int U1, U2, *A, *B, *C, *D;
    int PT1, PT2, *WALK, RES[16];
    int PTX, PTY, CLK, PTEST, IN;

    WALK = hs->WALK;
    PT1 = hs->PT1;
    PT2 = hs->PT2;
    PTX = U1 = 0;
    PTY = U2 = 0;

    memset(RES, 0, sizeof(RES));

    while (n < COLLECT_SIZE * 4) {
        ONE_ITERATION 
        ONE_ITERATION 
        ONE_ITERATION 
        ONE_ITERATION
    } 
    hs->PT1 = PT1;
    hs->PT2 = PT2;
    hs->offset[0] = 0;
    hs->offset[1] = COLLECT_SIZE / 2;
}


/*
    HAVEGE initialization
 */
void havege_init(havege_state * hs)
{
    memset(hs, 0, sizeof(havege_state));
    havege_fill(hs);
}


/*
    HAVEGE rand function
 */
int havege_rand(void *p_rng)
{
    havege_state *hs = (havege_state *) p_rng;
    int     ret;

    if (hs->offset[1] >= COLLECT_SIZE) {
        havege_fill(hs);
    }
    ret = hs->pool[hs->offset[0]++];
    ret ^= hs->pool[hs->offset[1]++];
    return ret;
}

#undef SWAP

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/md2.c ************/


/*
    md2.c -- RFC 1115/1319 compliant MD2 implementation

    The MD2 algorithm was designed by Ron Rivest in 1989.

    http://www.ietf.org/rfc/rfc1115.txt
    http://www.ietf.org/rfc/rfc1319.txt

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_MD2

static const uchar PI_SUBST[256] = {
    0x29, 0x2E, 0x43, 0xC9, 0xA2, 0xD8, 0x7C, 0x01, 0x3D, 0x36,
    0x54, 0xA1, 0xEC, 0xF0, 0x06, 0x13, 0x62, 0xA7, 0x05, 0xF3,
    0xC0, 0xC7, 0x73, 0x8C, 0x98, 0x93, 0x2B, 0xD9, 0xBC, 0x4C,
    0x82, 0xCA, 0x1E, 0x9B, 0x57, 0x3C, 0xFD, 0xD4, 0xE0, 0x16,
    0x67, 0x42, 0x6F, 0x18, 0x8A, 0x17, 0xE5, 0x12, 0xBE, 0x4E,
    0xC4, 0xD6, 0xDA, 0x9E, 0xDE, 0x49, 0xA0, 0xFB, 0xF5, 0x8E,
    0xBB, 0x2F, 0xEE, 0x7A, 0xA9, 0x68, 0x79, 0x91, 0x15, 0xB2,
    0x07, 0x3F, 0x94, 0xC2, 0x10, 0x89, 0x0B, 0x22, 0x5F, 0x21,
    0x80, 0x7F, 0x5D, 0x9A, 0x5A, 0x90, 0x32, 0x27, 0x35, 0x3E,
    0xCC, 0xE7, 0xBF, 0xF7, 0x97, 0x03, 0xFF, 0x19, 0x30, 0xB3,
    0x48, 0xA5, 0xB5, 0xD1, 0xD7, 0x5E, 0x92, 0x2A, 0xAC, 0x56,
    0xAA, 0xC6, 0x4F, 0xB8, 0x38, 0xD2, 0x96, 0xA4, 0x7D, 0xB6,
    0x76, 0xFC, 0x6B, 0xE2, 0x9C, 0x74, 0x04, 0xF1, 0x45, 0x9D,
    0x70, 0x59, 0x64, 0x71, 0x87, 0x20, 0x86, 0x5B, 0xCF, 0x65,
    0xE6, 0x2D, 0xA8, 0x02, 0x1B, 0x60, 0x25, 0xAD, 0xAE, 0xB0,
    0xB9, 0xF6, 0x1C, 0x46, 0x61, 0x69, 0x34, 0x40, 0x7E, 0x0F,
    0x55, 0x47, 0xA3, 0x23, 0xDD, 0x51, 0xAF, 0x3A, 0xC3, 0x5C,
    0xF9, 0xCE, 0xBA, 0xC5, 0xEA, 0x26, 0x2C, 0x53, 0x0D, 0x6E,
    0x85, 0x28, 0x84, 0x09, 0xD3, 0xDF, 0xCD, 0xF4, 0x41, 0x81,
    0x4D, 0x52, 0x6A, 0xDC, 0x37, 0xC8, 0x6C, 0xC1, 0xAB, 0xFA,
    0x24, 0xE1, 0x7B, 0x08, 0x0C, 0xBD, 0xB1, 0x4A, 0x78, 0x88,
    0x95, 0x8B, 0xE3, 0x63, 0xE8, 0x6D, 0xE9, 0xCB, 0xD5, 0xFE,
    0x3B, 0x00, 0x1D, 0x39, 0xF2, 0xEF, 0xB7, 0x0E, 0x66, 0x58,
    0xD0, 0xE4, 0xA6, 0x77, 0x72, 0xF8, 0xEB, 0x75, 0x4B, 0x0A,
    0x31, 0x44, 0x50, 0xB4, 0x8F, 0xED, 0x1F, 0x1A, 0xDB, 0x99,
    0x8D, 0x33, 0x9F, 0x11, 0x83, 0x14
};


/*
    MD2 context setup
 */
void md2_starts(md2_context *ctx)
{
    memset(ctx, 0, sizeof(md2_context));
}


static void md2_process(md2_context *ctx)
{
    int     i, j;
    uchar   t = 0;

    for (i = 0; i < 16; i++) {
        ctx->state[i + 16] = ctx->buffer[i];
        ctx->state[i + 32] = (uchar)(ctx->buffer[i] ^ ctx->state[i]);
    }

    for (i = 0; i < 18; i++) {
        for (j = 0; j < 48; j++) {
            ctx->state[j] = (uchar) (ctx->state[j] ^ PI_SUBST[t]);
            t = ctx->state[j];
        }
        t = (uchar)(t + i);
    }
    t = ctx->cksum[15];

    for (i = 0; i < 16; i++) {
        ctx->cksum[i] = (uchar) (ctx->cksum[i] ^ PI_SUBST[ctx->buffer[i] ^ t]);
        t = ctx->cksum[i];
    }
}


/*
    MD2 process buffer
 */
void md2_update(md2_context *ctx, uchar *input, int ilen)
{
    int fill;

    while (ilen > 0) {
        if (ctx->left + ilen > 16) {
            fill = 16 - ctx->left;
        } else {
            fill = ilen;
        }
        memcpy(ctx->buffer + ctx->left, input, fill);
        ctx->left += fill;
        input += fill;
        ilen -= fill;
        if (ctx->left == 16) {
            ctx->left = 0;
            md2_process(ctx);
        }
    }
}


/*
    MD2 final digest
 */
void md2_finish(md2_context *ctx, uchar output[16])
{
    uchar   x;
    int     i;

    x = (uchar)(16 - ctx->left);
    for (i = ctx->left; i < 16; i++) {
        ctx->buffer[i] = x;
    }
    md2_process(ctx);
    memcpy(ctx->buffer, ctx->cksum, 16);
    md2_process(ctx);
    memcpy(output, ctx->state, 16);
}


/*
    output = MD2( input buffer )
 */
void md2(uchar *input, int ilen, uchar output[16])
{
    md2_context ctx;

    md2_starts(&ctx);
    md2_update(&ctx, input, ilen);
    md2_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md2_context));
}


/*
    output = MD2( file contents )
 */
int md2_file(char *path, uchar output[16])
{
    FILE        *f;
    size_t      n;
    md2_context ctx;
    uchar       buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    md2_starts(&ctx);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        md2_update(&ctx, buf, (int)n);
    }
    md2_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md2_context));

    if (ferror(f) != 0) {
        fclose(f);
        return 2;
    }
    fclose(f);
    return 0;
}


/*
    MD2 HMAC context setup
 */
void md2_hmac_starts(md2_context *ctx, uchar *key, int keylen)
{
    uchar   sum[16];
    int     i;

    if (keylen > 64) {
        md2(key, keylen, sum);
        keylen = 16;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 64);
    memset(ctx->opad, 0x5C, 64);

    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    md2_starts(ctx);
    md2_update(ctx, ctx->ipad, 64);
    memset(sum, 0, sizeof(sum));
}


/*
    MD2 HMAC process buffer
 */
void md2_hmac_update(md2_context *ctx, uchar *input, int ilen)
{
    md2_update(ctx, input, ilen);
}


/*
    MD2 HMAC final digest
 */
void md2_hmac_finish(md2_context *ctx, uchar output[16])
{
    uchar tmpbuf[16];

    md2_finish(ctx, tmpbuf);
    md2_starts(ctx);
    md2_update(ctx, ctx->opad, 64);
    md2_update(ctx, tmpbuf, 16);
    md2_finish(ctx, output);
    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-MD2( hmac key, input buffer )
 */
void md2_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[16])
{
    md2_context ctx;

    md2_hmac_starts(&ctx, key, keylen);
    md2_hmac_update(&ctx, input, ilen);
    md2_hmac_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md2_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/md4.c ************/


/*
    md4.c -- RFC 1186/1320 compliant MD4 implementation

    The MD4 algorithm was designed by Ron Rivest in 1990.
    http://www.ietf.org/rfc/rfc1186.txt
    http://www.ietf.org/rfc/rfc1320.txt

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_MD4

/*
   32-bit integer manipulation macros (little endian)
 */
#ifndef GET_ULONG_LE
#define GET_ULONG_LE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ]       )    \
            | ( (ulong) (b)[(i) + 1] <<  8 )    \
            | ( (ulong) (b)[(i) + 2] << 16 )    \
            | ( (ulong) (b)[(i) + 3] << 24 );   \
    }
#endif

#ifndef PUT_ULONG_LE
#define PUT_ULONG_LE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n)       );   \
        (b)[(i) + 1] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 2] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 3] = (uchar) ( (n) >> 24 );   \
    }
#endif


/*
   MD4 context setup
 */
void md4_starts(md4_context * ctx)
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
}


static void md4_process(md4_context * ctx, uchar data[64])
{
    ulong X[16], A, B, C, D;

    GET_ULONG_LE(X[0], data, 0);
    GET_ULONG_LE(X[1], data, 4);
    GET_ULONG_LE(X[2], data, 8);
    GET_ULONG_LE(X[3], data, 12);
    GET_ULONG_LE(X[4], data, 16);
    GET_ULONG_LE(X[5], data, 20);
    GET_ULONG_LE(X[6], data, 24);
    GET_ULONG_LE(X[7], data, 28);
    GET_ULONG_LE(X[8], data, 32);
    GET_ULONG_LE(X[9], data, 36);
    GET_ULONG_LE(X[10], data, 40);
    GET_ULONG_LE(X[11], data, 44);
    GET_ULONG_LE(X[12], data, 48);
    GET_ULONG_LE(X[13], data, 52);
    GET_ULONG_LE(X[14], data, 56);
    GET_ULONG_LE(X[15], data, 60);

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];

#define F(x, y, z) ((x & y) | ((~x) & z))
#define P(a,b,c,d,x,s) { a += F(b,c,d) + x; a = S(a,s); }

    P(A, B, C, D, X[0], 3);
    P(D, A, B, C, X[1], 7);
    P(C, D, A, B, X[2], 11);
    P(B, C, D, A, X[3], 19);
    P(A, B, C, D, X[4], 3);
    P(D, A, B, C, X[5], 7);
    P(C, D, A, B, X[6], 11);
    P(B, C, D, A, X[7], 19);
    P(A, B, C, D, X[8], 3);
    P(D, A, B, C, X[9], 7);
    P(C, D, A, B, X[10], 11);
    P(B, C, D, A, X[11], 19);
    P(A, B, C, D, X[12], 3);
    P(D, A, B, C, X[13], 7);
    P(C, D, A, B, X[14], 11);
    P(B, C, D, A, X[15], 19);

#undef P
#undef F

#define F(x,y,z) ((x & y) | (x & z) | (y & z))
#define P(a,b,c,d,x,s) { a += F(b,c,d) + x + 0x5A827999; a = S(a,s); }

    P(A, B, C, D, X[0], 3);
    P(D, A, B, C, X[4], 5);
    P(C, D, A, B, X[8], 9);
    P(B, C, D, A, X[12], 13);
    P(A, B, C, D, X[1], 3);
    P(D, A, B, C, X[5], 5);
    P(C, D, A, B, X[9], 9);
    P(B, C, D, A, X[13], 13);
    P(A, B, C, D, X[2], 3);
    P(D, A, B, C, X[6], 5);
    P(C, D, A, B, X[10], 9);
    P(B, C, D, A, X[14], 13);
    P(A, B, C, D, X[3], 3);
    P(D, A, B, C, X[7], 5);
    P(C, D, A, B, X[11], 9);
    P(B, C, D, A, X[15], 13);

#undef P
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define P(a,b,c,d,x,s) { a += F(b,c,d) + x + 0x6ED9EBA1; a = S(a,s); }

    P(A, B, C, D, X[0], 3);
    P(D, A, B, C, X[8], 9);
    P(C, D, A, B, X[4], 11);
    P(B, C, D, A, X[12], 15);
    P(A, B, C, D, X[2], 3);
    P(D, A, B, C, X[10], 9);
    P(C, D, A, B, X[6], 11);
    P(B, C, D, A, X[14], 15);
    P(A, B, C, D, X[1], 3);
    P(D, A, B, C, X[9], 9);
    P(C, D, A, B, X[5], 11);
    P(B, C, D, A, X[13], 15);
    P(A, B, C, D, X[3], 3);
    P(D, A, B, C, X[11], 9);
    P(C, D, A, B, X[7], 11);
    P(B, C, D, A, X[15], 15);

#undef F
#undef P

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
}


/*
   MD4 process buffer
 */
void md4_update(md4_context * ctx, uchar *input, int ilen)
{
    int     fill;
    ulong   left;

    if (ilen <= 0)
        return;

    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if (ctx->total[0] < (ulong) ilen) {
        ctx->total[1]++;
    }
    if (left && ilen >= fill) {
        memcpy((void *)(ctx->buffer + left), (void *)input, fill);
        md4_process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }
    while (ilen >= 64) {
        md4_process(ctx, input);
        input += 64;
        ilen -= 64;
    }
    if (ilen > 0) {
        memcpy((void *)(ctx->buffer + left), (void *)input, ilen);
    }
}


static const uchar md4_padding[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
    MD4 final digest
 */
void md4_finish(md4_context * ctx, uchar output[16])
{
    ulong last, padn;
    ulong high, low;
    uchar msglen[8];

    high = (ctx->total[0] >> 29) | (ctx->total[1] << 3);
    low = (ctx->total[0] << 3);

    PUT_ULONG_LE(low, msglen, 0);
    PUT_ULONG_LE(high, msglen, 4);

    last = ctx->total[0] & 0x3F;
    padn = (last < 56) ? (56 - last) : (120 - last);

    md4_update(ctx, (uchar *)md4_padding, padn);
    md4_update(ctx, msglen, 8);

    PUT_ULONG_LE(ctx->state[0], output, 0);
    PUT_ULONG_LE(ctx->state[1], output, 4);
    PUT_ULONG_LE(ctx->state[2], output, 8);
    PUT_ULONG_LE(ctx->state[3], output, 12);
}


/*
    output = MD4( input buffer )
 */
void md4(uchar *input, int ilen, uchar output[16])
{
    md4_context ctx;

    md4_starts(&ctx);
    md4_update(&ctx, input, ilen);
    md4_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md4_context));
}


/*
    output = MD4( file contents )
 */
int md4_file(char *path, uchar output[16])
{
    FILE *f;
    size_t n;
    md4_context ctx;
    uchar buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return (1);
    }
    md4_starts(&ctx);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        md4_update(&ctx, buf, (int)n);
    }
    md4_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md4_context));

    if (ferror(f) != 0) {
        fclose(f);
        return (2);
    }
    fclose(f);
    return 0;
}


/*
    MD4 HMAC context setup
 */
void md4_hmac_starts(md4_context * ctx, uchar *key, int keylen)
{
    int     i;
    uchar   sum[16];

    if (keylen > 64) {
        md4(key, keylen, sum);
        keylen = 16;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 64);
    memset(ctx->opad, 0x5C, 64);

    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    md4_starts(ctx);
    md4_update(ctx, ctx->ipad, 64);
    memset(sum, 0, sizeof(sum));
}


/*
    MD4 HMAC process buffer
 */
void md4_hmac_update(md4_context * ctx, uchar *input, int ilen)
{
    md4_update(ctx, input, ilen);
}


/*
    MD4 HMAC final digest
 */
void md4_hmac_finish(md4_context * ctx, uchar output[16])
{
    uchar tmpbuf[16];

    md4_finish(ctx, tmpbuf);
    md4_starts(ctx);
    md4_update(ctx, ctx->opad, 64);
    md4_update(ctx, tmpbuf, 16);
    md4_finish(ctx, output);
    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-MD4( hmac key, input buffer )
 */
void md4_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[16])
{
    md4_context ctx;

    md4_hmac_starts(&ctx, key, keylen);
    md4_hmac_update(&ctx, input, ilen);
    md4_hmac_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md4_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/md5.c ************/


/*
    md5.c -- RFC 1321 compliant MD5 implementation

    The MD5 algorithm was designed by Ron Rivest in 1991.
    http://www.ietf.org/rfc/rfc1321.txt

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_MD5

/*
    32-bit integer manipulation macros (little endian)
 */
#ifndef GET_ULONG_LE
#define GET_ULONG_LE(n,b,i)                     \
    {                                           \
        (n) = ((ulong) (b)[(i)    ]      )      \
            | ((ulong) (b)[(i) + 1] <<  8)      \
            | ((ulong) (b)[(i) + 2] << 16)      \
            | ((ulong) (b)[(i) + 3] << 24);     \
    }
#endif

#ifndef PUT_ULONG_LE
#define PUT_ULONG_LE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ((n)      );     \
        (b)[(i) + 1] = (uchar) ((n) >>  8);     \
        (b)[(i) + 2] = (uchar) ((n) >> 16);     \
        (b)[(i) + 3] = (uchar) ((n) >> 24);     \
    }
#endif

/*
    MD5 context setup
 */
void md5_starts(md5_context * ctx)
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
}

static void md5_process(md5_context * ctx, uchar data[64])
{
    ulong X[16], A, B, C, D;

    GET_ULONG_LE(X[0], data, 0);
    GET_ULONG_LE(X[1], data, 4);
    GET_ULONG_LE(X[2], data, 8);
    GET_ULONG_LE(X[3], data, 12);
    GET_ULONG_LE(X[4], data, 16);
    GET_ULONG_LE(X[5], data, 20);
    GET_ULONG_LE(X[6], data, 24);
    GET_ULONG_LE(X[7], data, 28);
    GET_ULONG_LE(X[8], data, 32);
    GET_ULONG_LE(X[9], data, 36);
    GET_ULONG_LE(X[10], data, 40);
    GET_ULONG_LE(X[11], data, 44);
    GET_ULONG_LE(X[12], data, 48);
    GET_ULONG_LE(X[13], data, 52);
    GET_ULONG_LE(X[14], data, 56);
    GET_ULONG_LE(X[15], data, 60);

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define P(a,b,c,d,k,s,t)                            \
    {                                               \
        a += F(b,c,d) + X[k] + t; a = S(a,s) + b;   \
    }

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];

#define F(x,y,z) (z ^ (x & (y ^ z)))

    P(A, B, C, D, 0, 7, 0xD76AA478);
    P(D, A, B, C, 1, 12, 0xE8C7B756);
    P(C, D, A, B, 2, 17, 0x242070DB);
    P(B, C, D, A, 3, 22, 0xC1BDCEEE);
    P(A, B, C, D, 4, 7, 0xF57C0FAF);
    P(D, A, B, C, 5, 12, 0x4787C62A);
    P(C, D, A, B, 6, 17, 0xA8304613);
    P(B, C, D, A, 7, 22, 0xFD469501);
    P(A, B, C, D, 8, 7, 0x698098D8);
    P(D, A, B, C, 9, 12, 0x8B44F7AF);
    P(C, D, A, B, 10, 17, 0xFFFF5BB1);
    P(B, C, D, A, 11, 22, 0x895CD7BE);
    P(A, B, C, D, 12, 7, 0x6B901122);
    P(D, A, B, C, 13, 12, 0xFD987193);
    P(C, D, A, B, 14, 17, 0xA679438E);
    P(B, C, D, A, 15, 22, 0x49B40821);

#undef F

#define F(x,y,z) (y ^ (z & (x ^ y)))

    P(A, B, C, D, 1, 5, 0xF61E2562);
    P(D, A, B, C, 6, 9, 0xC040B340);
    P(C, D, A, B, 11, 14, 0x265E5A51);
    P(B, C, D, A, 0, 20, 0xE9B6C7AA);
    P(A, B, C, D, 5, 5, 0xD62F105D);
    P(D, A, B, C, 10, 9, 0x02441453);
    P(C, D, A, B, 15, 14, 0xD8A1E681);
    P(B, C, D, A, 4, 20, 0xE7D3FBC8);
    P(A, B, C, D, 9, 5, 0x21E1CDE6);
    P(D, A, B, C, 14, 9, 0xC33707D6);
    P(C, D, A, B, 3, 14, 0xF4D50D87);
    P(B, C, D, A, 8, 20, 0x455A14ED);
    P(A, B, C, D, 13, 5, 0xA9E3E905);
    P(D, A, B, C, 2, 9, 0xFCEFA3F8);
    P(C, D, A, B, 7, 14, 0x676F02D9);
    P(B, C, D, A, 12, 20, 0x8D2A4C8A);

#undef F

#define F(x,y,z) (x ^ y ^ z)

    P(A, B, C, D, 5, 4, 0xFFFA3942);
    P(D, A, B, C, 8, 11, 0x8771F681);
    P(C, D, A, B, 11, 16, 0x6D9D6122);
    P(B, C, D, A, 14, 23, 0xFDE5380C);
    P(A, B, C, D, 1, 4, 0xA4BEEA44);
    P(D, A, B, C, 4, 11, 0x4BDECFA9);
    P(C, D, A, B, 7, 16, 0xF6BB4B60);
    P(B, C, D, A, 10, 23, 0xBEBFBC70);
    P(A, B, C, D, 13, 4, 0x289B7EC6);
    P(D, A, B, C, 0, 11, 0xEAA127FA);
    P(C, D, A, B, 3, 16, 0xD4EF3085);
    P(B, C, D, A, 6, 23, 0x04881D05);
    P(A, B, C, D, 9, 4, 0xD9D4D039);
    P(D, A, B, C, 12, 11, 0xE6DB99E5);
    P(C, D, A, B, 15, 16, 0x1FA27CF8);
    P(B, C, D, A, 2, 23, 0xC4AC5665);

#undef F

#define F(x,y,z) (y ^ (x | ~z))

    P(A, B, C, D, 0, 6, 0xF4292244);
    P(D, A, B, C, 7, 10, 0x432AFF97);
    P(C, D, A, B, 14, 15, 0xAB9423A7);
    P(B, C, D, A, 5, 21, 0xFC93A039);
    P(A, B, C, D, 12, 6, 0x655B59C3);
    P(D, A, B, C, 3, 10, 0x8F0CCC92);
    P(C, D, A, B, 10, 15, 0xFFEFF47D);
    P(B, C, D, A, 1, 21, 0x85845DD1);
    P(A, B, C, D, 8, 6, 0x6FA87E4F);
    P(D, A, B, C, 15, 10, 0xFE2CE6E0);
    P(C, D, A, B, 6, 15, 0xA3014314);
    P(B, C, D, A, 13, 21, 0x4E0811A1);
    P(A, B, C, D, 4, 6, 0xF7537E82);
    P(D, A, B, C, 11, 10, 0xBD3AF235);
    P(C, D, A, B, 2, 15, 0x2AD7D2BB);
    P(B, C, D, A, 9, 21, 0xEB86D391);

#undef F

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
}


/*
   MD5 process buffer
 */
void md5_update(md5_context *ctx, uchar *input, int ilen)
{
    ulong   left;
    int     fill;

    if (ilen <= 0) {
        return;
    }
    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if (ctx->total[0] < (ulong)ilen) {
        ctx->total[1]++;
    }
    if (left && ilen >= fill) {
        memcpy((void *)(ctx->buffer + left), (void *)input, fill);
        md5_process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }
    while (ilen >= 64) {
        md5_process(ctx, input);
        input += 64;
        ilen -= 64;
    }
    if (ilen > 0) {
        memcpy((void *)(ctx->buffer + left), (void *)input, ilen);
    }
}

static const uchar md5_padding[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
    MD5 final digest
 */
void md5_finish(md5_context *ctx, uchar output[16])
{
    ulong last, padn, high, low;
    uchar msglen[8];

    high = (ctx->total[0] >> 29) | (ctx->total[1] << 3);
    low = (ctx->total[0] << 3);

    PUT_ULONG_LE(low, msglen, 0);
    PUT_ULONG_LE(high, msglen, 4);

    last = ctx->total[0] & 0x3F;
    padn = (last < 56) ? (56 - last) : (120 - last);

    md5_update(ctx, (uchar *)md5_padding, padn);
    md5_update(ctx, msglen, 8);

    PUT_ULONG_LE(ctx->state[0], output, 0);
    PUT_ULONG_LE(ctx->state[1], output, 4);
    PUT_ULONG_LE(ctx->state[2], output, 8);
    PUT_ULONG_LE(ctx->state[3], output, 12);
}

/*
    output = MD5(input buffer)
 */
void md5(uchar *input, int ilen, uchar output[16])
{
    md5_context ctx;

    md5_starts(&ctx);
    md5_update(&ctx, input, ilen);
    md5_finish(&ctx, output);

    memset(&ctx, 0, sizeof(md5_context));
}


/*
    output = MD5(file contents)
 */
int md5_file(char *path, uchar output[16])
{
    FILE        *f;
    size_t      n;
    md5_context ctx;
    uchar       buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    md5_starts(&ctx);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        md5_update(&ctx, buf, (int)n);

    md5_finish(&ctx, output);

    memset(&ctx, 0, sizeof(md5_context));

    if (ferror(f) != 0) {
        fclose(f);
        return 2;
    }
    fclose(f);
    return 0;
}


/*
   MD5 HMAC context setup
 */
void md5_hmac_starts(md5_context *ctx, uchar *key, int keylen)
{
    uchar   sum[16];
    int     i;

    if (keylen > 64) {
        md5(key, keylen, sum);
        keylen = 16;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 64);
    memset(ctx->opad, 0x5C, 64);

    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    md5_starts(ctx);
    md5_update(ctx, ctx->ipad, 64);
    memset(sum, 0, sizeof(sum));
}


/*
   MD5 HMAC process buffer
 */
void md5_hmac_update(md5_context *ctx, uchar *input, int ilen)
{
    md5_update(ctx, input, ilen);
}


/*
    MD5 HMAC final digest
 */
void md5_hmac_finish(md5_context *ctx, uchar output[16])
{
    uchar tmpbuf[16];

    md5_finish(ctx, tmpbuf);
    md5_starts(ctx);
    md5_update(ctx, ctx->opad, 64);
    md5_update(ctx, tmpbuf, 16);
    md5_finish(ctx, output);

    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-MD5(hmac key, input buffer)
 */
void md5_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[16])
{
    md5_context ctx;

    md5_hmac_starts(&ctx, key, keylen);
    md5_hmac_update(&ctx, input, ilen);
    md5_hmac_finish(&ctx, output);

    memset(&ctx, 0, sizeof(md5_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/net.c ************/


/*
    net.c -- Network routines

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_NET

#if WINDOWS || WINCE
    #undef read
    #undef write
    #undef close
    #define read(fd,buf,len)        recv(fd,buf,len,0)
    #define write(fd,buf,len)       send(fd,buf,len,0)
    #define close(fd)               closesocket(fd)
    static int wsa_init_done = 0;
#endif

/*
    htons() is not always available
 */
#if defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && __BYTE_ORDER == __BIG_ENDIAN
    #define SSL_HTONS(n) (n)
#else
    #define SSL_HTONS(n) (((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8))
#endif
#define net_htons(n) SSL_HTONS(n)

/*
   Initiate a TCP connection with host:port
 */
int net_connect(int *fd, char *host, int port)
{
    struct sockaddr_in server_addr;
    struct hostent *server_host;

#if WINDOWS || WINCE
    WSADATA wsaData;

    if (!wsa_init_done) {
        if (WSAStartup(MAKEWORD(2, 0), &wsaData) == SOCKET_ERROR) {
            return EST_ERR_NET_SOCKET_FAILED;
        }
        wsa_init_done = 1;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    if ((server_host = gethostbyname(host)) == NULL) {
        return EST_ERR_NET_UNKNOWN_HOST;
    }
    if ((*fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
        return EST_ERR_NET_SOCKET_FAILED;
    }
    memcpy((void *)&server_addr.sin_addr, (void *)server_host->h_addr, server_host->h_length);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = net_htons(port);

    if (connect(*fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        closesocket(*fd);
        return EST_ERR_NET_CONNECT_FAILED;
    }
    return 0;
}


/*
   Create a listening socket on bind_ip:port
 */
int net_bind(int *fd, char *bind_ip, int port)
{
    int n, c[4];
    struct sockaddr_in server_addr;

#if defined(WIN32) || defined(_WIN32_WCE)
    WSADATA wsaData;

    if (wsa_init_done == 0) {
        if (WSAStartup(MAKEWORD(2, 0), &wsaData) == SOCKET_ERROR) {
            return EST_ERR_NET_SOCKET_FAILED;
        }
        wsa_init_done = 1;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    if ((*fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
        return EST_ERR_NET_SOCKET_FAILED;
    }
    n = 1;
    setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, (char*) &n, sizeof(n));

    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = net_htons(port);

    if (bind_ip != NULL) {
        memset(c, 0, sizeof(c));
        sscanf(bind_ip, "%d.%d.%d.%d", &c[0], &c[1], &c[2], &c[3]);

        for (n = 0; n < 4; n++) {
            if (c[n] < 0 || c[n] > 255) {
                break;
            }
        }
        if (n == 4) {
            server_addr.sin_addr.s_addr = ((ulong)c[0] << 24) | ((ulong)c[1] << 16) | ((ulong)c[2] << 8) | ((ulong)c[3]);
        }
    }
    if (bind(*fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        closesocket(*fd);
        return EST_ERR_NET_BIND_FAILED;
    }
    if (listen(*fd, 10) != 0) {
        closesocket(*fd);
        return EST_ERR_NET_LISTEN_FAILED;
    }
    return 0;
}


/*
    Check if the current operation is blocking
 */
static int net_is_blocking(void)
{
#if WINDOWS || WINCE
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    switch (errno) {
#if defined EAGAIN
    case EAGAIN:
#endif
#if defined EWOULDBLOCK && EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
        return 1;
    }
    return 0;
#endif
}


/*
    Accept a connection from a remote client
 */
int net_accept(int bind_fd, int *client_fd, void *client_ip)
{
    struct sockaddr_in client_addr;

#if defined(__socklen_t_defined) || 1
    socklen_t n = (socklen_t) sizeof(client_addr);
#else
    int n = (int)sizeof(client_addr);
#endif

    *client_fd = accept(bind_fd, (struct sockaddr *) &client_addr, &n);

    if (*client_fd < 0) {
        if (net_is_blocking() != 0) {
            return EST_ERR_NET_TRY_AGAIN;
        }
        return EST_ERR_NET_ACCEPT_FAILED;
    }
    if (client_ip != NULL) {
        memcpy(client_ip, &client_addr.sin_addr.s_addr, sizeof(client_addr.sin_addr.s_addr));
    }
    return 0;
}


/*
    Set the socket blocking or non-blocking
 */
int net_set_block(int fd)
{
#if ME_WIN_LIKE
    long n = 0;
    return ioctlsocket(fd, FIONBIO, &n);
#else
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
#endif
}


int net_set_nonblock(int fd)
{
#if ME_WIN_LIKE
    long n = 1;
    return ioctlsocket(fd, FIONBIO, &n);
#else
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#endif
}


/*
    Portable usleep helper
 */
void net_usleep(ulong usec)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = usec;
    select(0, NULL, NULL, NULL, &tv);
}


/*
    Read at most 'len' characters
 */
int net_recv(void *ctx, uchar *buf, int len)
{
    int ret = read(*((int *)ctx), buf, len);

    if (len > 0 && ret == 0) {
        return EST_ERR_NET_CONN_RESET;
    }
    if (ret < 0) {
        if (net_is_blocking() != 0) {
            return EST_ERR_NET_TRY_AGAIN;
        }
#if defined(WIN32) || defined(_WIN32_WCE)
        if (WSAGetLastError() == WSAECONNRESET) {
            return EST_ERR_NET_CONN_RESET;
        }
#else
        if (errno == EPIPE || errno == ECONNRESET) {
            return EST_ERR_NET_CONN_RESET;
        }
        if (errno == EINTR) {
            return EST_ERR_NET_TRY_AGAIN;
        }
#endif
        return EST_ERR_NET_RECV_FAILED;
    }
    return ret;
}


/*
    Write at most 'len' characters
 */
int net_send(void *ctx, uchar *buf, int len)
{
    int ret = send(*((int*)ctx), buf, len, 0);

    if (ret < 0) {
        if (net_is_blocking() != 0) {
            return EST_ERR_NET_TRY_AGAIN;
        }
#if defined(WIN32) || defined(_WIN32_WCE)
        if (WSAGetLastError() == WSAECONNRESET) {
            return EST_ERR_NET_CONN_RESET;
        }
#else
        if (errno == EPIPE || errno == ECONNRESET) {
            return EST_ERR_NET_CONN_RESET;
        }
        if (errno == EINTR) {
            return EST_ERR_NET_TRY_AGAIN;
        }
#endif
        return EST_ERR_NET_SEND_FAILED;
    }
    return ret;
}


/*
    Gracefully close the connection
 */
void net_close(int fd)
{
    shutdown(fd, 2);
    closesocket(fd);
}

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/padlock.c ************/


/*
    padlock.c -- Header VIA padlock suport

    This implementation is based on the VIA PadLock Programming Guide:
    http://www.via.com.tw/en/downloads/whitepapers/initiatives/padlock/programming_guide.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_PADLOCK

// #if ME_CPU_ARCH == ME_CPU_X86
#if defined(EST_HAVE_X86)

/*
    PadLock detection routine
 */
int padlock_supports(int feature)
{
    static int flags = -1;
    int ebx, edx;

    if (flags == -1) {
asm("movl  %%ebx, %0           \n" "movl  $0xC0000000, %%eax  \n" "cpuid                     \n" "cmpl  $0xC0000001, %%eax  \n" "movl  $0, %%edx           \n" "jb    unsupported         \n" "movl  $0xC0000001, %%eax  \n" "cpuid                     \n" "unsupported:              \n" "movl  %%edx, %1           \n" "movl  %2, %%ebx           \n":"=m"(ebx),
            "=m"
            (edx)
:           "m"(ebx)
:           "eax", "ecx", "edx");

        flags = edx;
    }

    return flags & feature;
}

/*
    PadLock AES-ECB block en(de)cryption
 */
int padlock_xcryptecb(aes_context * ctx, int mode, uchar input[16], uchar output[16])
{
    ulong   *rk, *blk, *ctrl, buf[256];
    int     ebx;

    rk = ctx->rk;
    blk = PADLOCK_ALIGN16(buf);
    memcpy(blk, input, 16);

    ctrl = blk + 4;
    *ctrl = 0x80 | ctx->nr | ((ctx->nr + (mode ^ 1) - 10) << 9);

asm("pushfl; popfl         \n" "movl    %%ebx, %0     \n" "movl    $1, %%ecx     \n" "movl    %2, %%edx     \n" "movl    %3, %%ebx     \n" "movl    %4, %%esi     \n" "movl    %4, %%edi     \n" ".byte  0xf3,0x0f,0xa7,0xc8\n" "movl    %1, %%ebx     \n":"=m"(ebx)
:       "m"(ebx), "m"(ctrl), "m"(rk), "m"(blk)
:       "ecx", "edx", "esi", "edi");

    memcpy(output, blk, 16);
    return 0;
}


/*
    PadLock AES-CBC buffer en(de)cryption
 */
int padlock_xcryptcbc(aes_context * ctx, int mode, int length, uchar iv[16], uchar *input, uchar *output)
{
    ulong   *rk, *iw, *ctrl, buf[256];
    int     ebx, count;

    if (((long)input & 15) != 0 || ((long)output & 15) != 0) {
        return 1;
    }
    rk = ctx->rk;
    iw = PADLOCK_ALIGN16(buf);
    memcpy(iw, iv, 16);

    ctrl = iw + 4;
    *ctrl = 0x80 | ctx->nr | ((ctx->nr + (mode ^ 1) - 10) << 9);

    count = (length + 15) >> 4;

asm("pushfl; popfl         \n" "movl    %%ebx, %0     \n" "movl    %2, %%ecx     \n" "movl    %3, %%edx     \n" "movl    %4, %%ebx     \n" "movl    %5, %%esi     \n" "movl    %6, %%edi     \n" "movl    %7, %%eax     \n" ".byte  0xf3,0x0f,0xa7,0xd0\n" "movl    %1, %%ebx     \n":"=m"(ebx)
:       "m"(ebx), "m"(count), "m"(ctrl),
        "m"(rk), "m"(input), "m"(output), "m"(iw)
:       "eax", "ecx", "edx", "esi", "edi");

    memcpy(iv, iw, 16);
    return 0;
}
#endif

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/rsa.c ************/


/*
    rsa.c -- The RSA public-key cryptosystem

    RSA was designed by Ron Rivest, Adi Shamir and Len Adleman.

    http://theory.lcs.mit.edu/~rivest/rsapaper.pdf
    http://www.cacr.math.uwaterloo.ca/hac/about/chap8.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_RSA

/*
    Initialize an RSA context
 */
void rsa_init(rsa_context *ctx, int padding, int hash_id, int (*f_rng) (void *), void *p_rng)
{
    memset(ctx, 0, sizeof(rsa_context));
    ctx->padding = padding;
    ctx->hash_id = hash_id;
    ctx->f_rng = f_rng;
    ctx->p_rng = p_rng;
}


#if ME_EST_GEN_PRIME
/*
    Generate an RSA keypair
 */
int rsa_gen_key(rsa_context *ctx, int nbits, int exponent)
{
    mpi     P1, Q1, H, G;
    int     ret;

    if (ctx->f_rng == NULL || nbits < 128 || exponent < 3) {
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
    mpi_init(&P1, &Q1, &H, &G, NULL);

    /*
        find primes P and Q with Q < P so that: GCD( E, (P-1)*(Q-1) ) == 1
     */
    MPI_CHK(mpi_lset(&ctx->E, exponent));

    do {
        MPI_CHK(mpi_gen_prime(&ctx->P, (nbits + 1) >> 1, 0, ctx->f_rng, ctx->p_rng));

        MPI_CHK(mpi_gen_prime(&ctx->Q, (nbits + 1) >> 1, 0, ctx->f_rng, ctx->p_rng));

        if (mpi_cmp_mpi(&ctx->P, &ctx->Q) < 0) {
            mpi_swap(&ctx->P, &ctx->Q);
        }
        if (mpi_cmp_mpi(&ctx->P, &ctx->Q) == 0) {
            continue;
        }
        MPI_CHK(mpi_mul_mpi(&ctx->N, &ctx->P, &ctx->Q));
        if (mpi_msb(&ctx->N) != nbits) {
            continue;
        }
        MPI_CHK(mpi_sub_int(&P1, &ctx->P, 1));
        MPI_CHK(mpi_sub_int(&Q1, &ctx->Q, 1));
        MPI_CHK(mpi_mul_mpi(&H, &P1, &Q1));
        MPI_CHK(mpi_gcd(&G, &ctx->E, &H));

    } while (mpi_cmp_int(&G, 1) != 0);

    /*
       D  = E^-1 mod ((P-1)*(Q-1))
       DP = D mod (P - 1)
       DQ = D mod (Q - 1)
       QP = Q^-1 mod P
     */
    MPI_CHK(mpi_inv_mod(&ctx->D, &ctx->E, &H));
    MPI_CHK(mpi_mod_mpi(&ctx->DP, &ctx->D, &P1));
    MPI_CHK(mpi_mod_mpi(&ctx->DQ, &ctx->D, &Q1));
    MPI_CHK(mpi_inv_mod(&ctx->QP, &ctx->Q, &ctx->P));

    ctx->len = (mpi_msb(&ctx->N) + 7) >> 3;

cleanup:
    mpi_free(&G, &H, &Q1, &P1, NULL);
    if (ret != 0) {
        rsa_free(ctx);
        return EST_ERR_RSA_KEY_GEN_FAILED | ret;
    }
    return 0;
}
#endif

/*
    Check a public RSA key
 */
int rsa_check_pubkey(rsa_context *ctx)
{
    if ((ctx->N.p[0] & 1) == 0 || (ctx->E.p[0] & 1) == 0) {
        return EST_ERR_RSA_KEY_CHECK_FAILED;
    }
    if (mpi_msb(&ctx->N) < 128 || mpi_msb(&ctx->N) > 4096) {
        return EST_ERR_RSA_KEY_CHECK_FAILED;
    }
    if (mpi_msb(&ctx->E) < 2 || mpi_msb(&ctx->E) > 64) {
        return EST_ERR_RSA_KEY_CHECK_FAILED;
    }
    return 0;
}


/*
    Check a private RSA key
 */
int rsa_check_privkey(rsa_context *ctx)
{
    int ret;
    mpi PQ, DE, P1, Q1, H, I, G;

    if ((ret = rsa_check_pubkey(ctx)) != 0) {
        return ret;
    }
    mpi_init(&PQ, &DE, &P1, &Q1, &H, &I, &G, NULL);

    MPI_CHK(mpi_mul_mpi(&PQ, &ctx->P, &ctx->Q));
    MPI_CHK(mpi_mul_mpi(&DE, &ctx->D, &ctx->E));
    MPI_CHK(mpi_sub_int(&P1, &ctx->P, 1));
    MPI_CHK(mpi_sub_int(&Q1, &ctx->Q, 1));
    MPI_CHK(mpi_mul_mpi(&H, &P1, &Q1));
    MPI_CHK(mpi_mod_mpi(&I, &DE, &H));
    MPI_CHK(mpi_gcd(&G, &ctx->E, &H));

    if (mpi_cmp_mpi(&PQ, &ctx->N) == 0 && mpi_cmp_int(&I, 1) == 0 && mpi_cmp_int(&G, 1) == 0) {
        mpi_free(&G, &I, &H, &Q1, &P1, &DE, &PQ, NULL);
        return 0;
    }
cleanup:
    mpi_free(&G, &I, &H, &Q1, &P1, &DE, &PQ, NULL);
    return EST_ERR_RSA_KEY_CHECK_FAILED | ret;
}


/*
    Do an RSA public key operation
 */
int rsa_public(rsa_context *ctx, uchar *input, uchar *output)
{
    int ret, olen;
    mpi T;

    mpi_init(&T, NULL);

    MPI_CHK(mpi_read_binary(&T, input, ctx->len));

    if (mpi_cmp_mpi(&T, &ctx->N) >= 0) {
        mpi_free(&T, NULL);
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
    olen = ctx->len;
    MPI_CHK(mpi_exp_mod(&T, &T, &ctx->E, &ctx->N, &ctx->RN));
    MPI_CHK(mpi_write_binary(&T, output, olen));

cleanup:
    mpi_free(&T, NULL);
    if (ret != 0) {
        return EST_ERR_RSA_PUBLIC_FAILED | ret;
    }
    return 0;
}


/*
    Do an RSA private key operation
 */
int rsa_private(rsa_context *ctx, uchar *input, uchar *output)
{
    int ret, olen;
    mpi T, T1, T2;

    mpi_init(&T, &T1, &T2, NULL);

    MPI_CHK(mpi_read_binary(&T, input, ctx->len));
    if (mpi_cmp_mpi(&T, &ctx->N) >= 0) {
        mpi_free(&T, NULL);
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
#if 0
    MPI_CHK(mpi_exp_mod(&T, &T, &ctx->D, &ctx->N, &ctx->RN));
#else
    /*
       faster decryption using the CRT
      
       T1 = input ^ dP mod P
       T2 = input ^ dQ mod Q
     */
    MPI_CHK(mpi_exp_mod(&T1, &T, &ctx->DP, &ctx->P, &ctx->RP));
    MPI_CHK(mpi_exp_mod(&T2, &T, &ctx->DQ, &ctx->Q, &ctx->RQ));

    /*
       T = (T1 - T2) * (Q^-1 mod P) mod P
     */
    MPI_CHK(mpi_sub_mpi(&T, &T1, &T2));
    MPI_CHK(mpi_mul_mpi(&T1, &T, &ctx->QP));
    MPI_CHK(mpi_mod_mpi(&T, &T1, &ctx->P));

    /*
       output = T2 + T * Q
     */
    MPI_CHK(mpi_mul_mpi(&T1, &T, &ctx->Q));
    MPI_CHK(mpi_add_mpi(&T, &T2, &T1));
#endif

    olen = ctx->len;
    MPI_CHK(mpi_write_binary(&T, output, olen));

cleanup:
    mpi_free(&T, &T1, &T2, NULL);
    if (ret != 0)
        return EST_ERR_RSA_PRIVATE_FAILED | ret;

    return 0;
}


/*
    Add the message padding, then do an RSA operation
 */
int rsa_pkcs1_encrypt(rsa_context *ctx, int mode, int ilen, uchar *input, uchar *output)
{
    int nb_pad, olen;
    uchar *p = output;

    olen = ctx->len;

    switch (ctx->padding) {
    case RSA_PKCS_V15:

        if (ilen < 0 || olen < ilen + 11) {
            return EST_ERR_RSA_BAD_INPUT_DATA;
        }
        nb_pad = olen - 3 - ilen;
        *p++ = 0;
        *p++ = RSA_CRYPT;

        while (nb_pad-- > 0) {
            do {
                *p = (uchar)rand();
            } while (*p == 0);
            p++;
        }
        *p++ = 0;
        memcpy(p, input, ilen);
        break;

    default:
        return EST_ERR_RSA_INVALID_PADDING;
    }
    return (mode == RSA_PUBLIC) ? rsa_public(ctx, output, output) : rsa_private(ctx, output, output);
}


/*
    Do an RSA operation, then remove the message padding
 */
int rsa_pkcs1_decrypt(rsa_context *ctx, int mode, int *olen, uchar *input, uchar *output, int output_max_len)
{
    int ret, ilen;
    uchar *p;
    uchar buf[512];

    ilen = ctx->len;

    if (ilen < 16 || ilen > (int)sizeof(buf)) {
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
    ret = (mode == RSA_PUBLIC) ? rsa_public(ctx, input, buf) : rsa_private(ctx, input, buf);

    if (ret != 0) {
        return ret;
    }
    p = buf;

    switch (ctx->padding) {
    case RSA_PKCS_V15:

        if (*p++ != 0 || *p++ != RSA_CRYPT) {
            return EST_ERR_RSA_INVALID_PADDING;
        }
        while (*p != 0) {
            if (p >= buf + ilen - 1) {
                return EST_ERR_RSA_INVALID_PADDING;
            }
            p++;
        }
        p++;
        break;

    default:
        return EST_ERR_RSA_INVALID_PADDING;
    }
    if (ilen - (int)(p - buf) > output_max_len) {
        return EST_ERR_RSA_OUTPUT_TO_LARGE;
    }
    *olen = ilen - (int)(p - buf);
    memcpy(output, p, *olen);
    return 0;
}


/*
    Do an RSA operation to sign the message digest
 */
int rsa_pkcs1_sign(rsa_context *ctx, int mode, int hash_id, int hashlen, uchar *hash, uchar *sig)
{
    int nb_pad, olen;
    uchar *p = sig;

    olen = ctx->len;

    switch (ctx->padding) {
    case RSA_PKCS_V15:

        switch (hash_id) {
        case RSA_RAW:
            nb_pad = olen - 3 - hashlen;
            break;

        case RSA_MD2:
        case RSA_MD4:
        case RSA_MD5:
            nb_pad = olen - 3 - 34;
            break;

        case RSA_SHA1:
            nb_pad = olen - 3 - 35;
            break;

        default:
            return EST_ERR_RSA_BAD_INPUT_DATA;
        }
        if (nb_pad < 8) {
            return EST_ERR_RSA_BAD_INPUT_DATA;
        }
        *p++ = 0;
        *p++ = RSA_SIGN;
        memset(p, 0xFF, nb_pad);
        p += nb_pad;
        *p++ = 0;
        break;

    default:
        return EST_ERR_RSA_INVALID_PADDING;
    }

    switch (hash_id) {
    case RSA_RAW:
        memcpy(p, hash, hashlen);
        break;

    case RSA_MD2:
        memcpy(p, EST_ASN1_HASH_MDX, 18);
        memcpy(p + 18, hash, 16);
        p[13] = 2;
        break;

    case RSA_MD4:
        memcpy(p, EST_ASN1_HASH_MDX, 18);
        memcpy(p + 18, hash, 16);
        p[13] = 4;
        break;

    case RSA_MD5:
        memcpy(p, EST_ASN1_HASH_MDX, 18);
        memcpy(p + 18, hash, 16);
        p[13] = 5;
        break;

    case RSA_SHA1:
        memcpy(p, EST_ASN1_HASH_SHA1, 15);
        memcpy(p + 15, hash, 20);
        break;

    default:
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
    return (mode == RSA_PUBLIC) ? rsa_public(ctx, sig, sig) : rsa_private(ctx, sig, sig);
}


/*
    Do an RSA operation and check the message digest
 */
int rsa_pkcs1_verify(rsa_context *ctx, int mode, int hash_id, int hashlen, uchar *hash, uchar *sig)
{
    uchar   *p, c, buf[512];
    int     ret, len, siglen;

    siglen = ctx->len;

    if (siglen < 16 || siglen > (int)sizeof(buf)) {
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
    ret = (mode == RSA_PUBLIC) ? rsa_public(ctx, sig, buf) : rsa_private(ctx, sig, buf);

    if (ret != 0) {
        return ret;
    }
    p = buf;

    switch (ctx->padding) {
    case RSA_PKCS_V15:
        if (*p++ != 0 || *p++ != RSA_SIGN) {
            return EST_ERR_RSA_INVALID_PADDING;
        }
        while (*p != 0) {
            if (p >= buf + siglen - 1 || *p != 0xFF)
                return EST_ERR_RSA_INVALID_PADDING;
            p++;
        }
        p++;
        break;

    default:
        return EST_ERR_RSA_INVALID_PADDING;
    }
    len = siglen - (int)(p - buf);

    if (len == 34) {
        c = p[13];
        p[13] = 0;

        if (memcmp(p, EST_ASN1_HASH_MDX, 18) != 0) {
            return EST_ERR_RSA_VERIFY_FAILED;
        }
        if ((c == 2 && hash_id == RSA_MD2) || (c == 4 && hash_id == RSA_MD4) || (c == 5 && hash_id == RSA_MD5)) {
            if (memcmp(p + 18, hash, 16) == 0) {
                return 0;
            } else {
                return EST_ERR_RSA_VERIFY_FAILED;
            }
        }
    }
    if (len == 35 && hash_id == RSA_SHA1) {
        if (memcmp(p, EST_ASN1_HASH_SHA1, 15) == 0 && memcmp(p + 15, hash, 20) == 0) {
            return 0;
        } else {
            return EST_ERR_RSA_VERIFY_FAILED;
        }
    }
    if (len == hashlen && hash_id == RSA_RAW) {
        if (memcmp(p, hash, hashlen) == 0) {
            return 0;
        } else {
            return EST_ERR_RSA_VERIFY_FAILED;
        }
    }
    return EST_ERR_RSA_INVALID_PADDING;
}


/*
    Free the components of an RSA key
 */
void rsa_free(rsa_context *ctx)
{
    mpi_free(&ctx->RQ, &ctx->RP, &ctx->RN, &ctx->QP, &ctx->DQ, &ctx->DP, &ctx->Q, &ctx->P, &ctx->D, &ctx->E, &ctx->N, NULL);
}


#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/sha1.c ************/


/*
    sha1.c -- FIPS-180-1 compliant SHA-1 implementation

    The SHA-1 standard was published by NIST in 1993.
    http://www.itl.nist.gov/fipspubs/fip180-1.htm

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_SHA1

/*
    32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ] << 24 )    \
            | ( (ulong) (b)[(i) + 1] << 16 )    \
            | ( (ulong) (b)[(i) + 2] <<  8 )    \
            | ( (ulong) (b)[(i) + 3]       );   \
    }
#endif

#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 2] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 3] = (uchar) ( (n)       );   \
    }
#endif


/*
    SHA-1 context setup
 */
void sha1_starts(sha1_context *ctx)
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
}


static void sha1_process(sha1_context *ctx, uchar data[64])
{
    ulong temp, W[16], A, B, C, D, E;

    GET_ULONG_BE(W[0], data, 0);
    GET_ULONG_BE(W[1], data, 4);
    GET_ULONG_BE(W[2], data, 8);
    GET_ULONG_BE(W[3], data, 12);
    GET_ULONG_BE(W[4], data, 16);
    GET_ULONG_BE(W[5], data, 20);
    GET_ULONG_BE(W[6], data, 24);
    GET_ULONG_BE(W[7], data, 28);
    GET_ULONG_BE(W[8], data, 32);
    GET_ULONG_BE(W[9], data, 36);
    GET_ULONG_BE(W[10], data, 40);
    GET_ULONG_BE(W[11], data, 44);
    GET_ULONG_BE(W[12], data, 48);
    GET_ULONG_BE(W[13], data, 52);
    GET_ULONG_BE(W[14], data, 56);
    GET_ULONG_BE(W[15], data, 60);

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define R(t)                                            \
    (                                                   \
        temp = W[(t -  3) & 0x0F] ^ W[(t - 8) & 0x0F] ^ \
        W[(t - 14) & 0x0F] ^ W[t & 0x0F],               \
        ( W[t & 0x0F] = S(temp,1) )                     \
        )

#define P(a,b,c,d,e,x)                                  \
    {                                                   \
        e += S(a,5) + F(b,c,d) + K + x; b = S(b,30);    \
    }

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];
    E = ctx->state[4];

#define F(x,y,z) (z ^ (x & (y ^ z)))
#define K 0x5A827999

    P(A, B, C, D, E, W[0]);
    P(E, A, B, C, D, W[1]);
    P(D, E, A, B, C, W[2]);
    P(C, D, E, A, B, W[3]);
    P(B, C, D, E, A, W[4]);
    P(A, B, C, D, E, W[5]);
    P(E, A, B, C, D, W[6]);
    P(D, E, A, B, C, W[7]);
    P(C, D, E, A, B, W[8]);
    P(B, C, D, E, A, W[9]);
    P(A, B, C, D, E, W[10]);
    P(E, A, B, C, D, W[11]);
    P(D, E, A, B, C, W[12]);
    P(C, D, E, A, B, W[13]);
    P(B, C, D, E, A, W[14]);
    P(A, B, C, D, E, W[15]);
    P(E, A, B, C, D, R(16));
    P(D, E, A, B, C, R(17));
    P(C, D, E, A, B, R(18));
    P(B, C, D, E, A, R(19));

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0x6ED9EBA1

    P(A, B, C, D, E, R(20));
    P(E, A, B, C, D, R(21));
    P(D, E, A, B, C, R(22));
    P(C, D, E, A, B, R(23));
    P(B, C, D, E, A, R(24));
    P(A, B, C, D, E, R(25));
    P(E, A, B, C, D, R(26));
    P(D, E, A, B, C, R(27));
    P(C, D, E, A, B, R(28));
    P(B, C, D, E, A, R(29));
    P(A, B, C, D, E, R(30));
    P(E, A, B, C, D, R(31));
    P(D, E, A, B, C, R(32));
    P(C, D, E, A, B, R(33));
    P(B, C, D, E, A, R(34));
    P(A, B, C, D, E, R(35));
    P(E, A, B, C, D, R(36));
    P(D, E, A, B, C, R(37));
    P(C, D, E, A, B, R(38));
    P(B, C, D, E, A, R(39));

#undef K
#undef F

#define F(x,y,z) ((x & y) | (z & (x | y)))
#define K 0x8F1BBCDC

    P(A, B, C, D, E, R(40));
    P(E, A, B, C, D, R(41));
    P(D, E, A, B, C, R(42));
    P(C, D, E, A, B, R(43));
    P(B, C, D, E, A, R(44));
    P(A, B, C, D, E, R(45));
    P(E, A, B, C, D, R(46));
    P(D, E, A, B, C, R(47));
    P(C, D, E, A, B, R(48));
    P(B, C, D, E, A, R(49));
    P(A, B, C, D, E, R(50));
    P(E, A, B, C, D, R(51));
    P(D, E, A, B, C, R(52));
    P(C, D, E, A, B, R(53));
    P(B, C, D, E, A, R(54));
    P(A, B, C, D, E, R(55));
    P(E, A, B, C, D, R(56));
    P(D, E, A, B, C, R(57));
    P(C, D, E, A, B, R(58));
    P(B, C, D, E, A, R(59));

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0xCA62C1D6

    P(A, B, C, D, E, R(60));
    P(E, A, B, C, D, R(61));
    P(D, E, A, B, C, R(62));
    P(C, D, E, A, B, R(63));
    P(B, C, D, E, A, R(64));
    P(A, B, C, D, E, R(65));
    P(E, A, B, C, D, R(66));
    P(D, E, A, B, C, R(67));
    P(C, D, E, A, B, R(68));
    P(B, C, D, E, A, R(69));
    P(A, B, C, D, E, R(70));
    P(E, A, B, C, D, R(71));
    P(D, E, A, B, C, R(72));
    P(C, D, E, A, B, R(73));
    P(B, C, D, E, A, R(74));
    P(A, B, C, D, E, R(75));
    P(E, A, B, C, D, R(76));
    P(D, E, A, B, C, R(77));
    P(C, D, E, A, B, R(78));
    P(B, C, D, E, A, R(79));

#undef K
#undef F

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
    ctx->state[4] += E;
}


/*
    SHA-1 process buffer
 */
void sha1_update(sha1_context *ctx, uchar *input, int ilen)
{
    int fill;
    ulong left;

    if (ilen <= 0) {
        return;
    }
    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if (ctx->total[0] < (ulong) ilen) {
        ctx->total[1]++;
    }
    if (left && ilen >= fill) {
        memcpy((void *)(ctx->buffer + left), (void *)input, fill);
        sha1_process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }
    while (ilen >= 64) {
        sha1_process(ctx, input);
        input += 64;
        ilen -= 64;
    }
    if (ilen > 0) {
        memcpy((void *)(ctx->buffer + left), (void *)input, ilen);
    }
}


static const uchar sha1_padding[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
    SHA-1 final digest
 */
void sha1_finish(sha1_context *ctx, uchar output[20])
{
    ulong last, padn, high, low;
    uchar msglen[8];

    high = (ctx->total[0] >> 29) | (ctx->total[1] << 3);
    low = (ctx->total[0] << 3);

    PUT_ULONG_BE(high, msglen, 0);
    PUT_ULONG_BE(low, msglen, 4);

    last = ctx->total[0] & 0x3F;
    padn = (last < 56) ? (56 - last) : (120 - last);

    sha1_update(ctx, (uchar *)sha1_padding, padn);
    sha1_update(ctx, msglen, 8);

    PUT_ULONG_BE(ctx->state[0], output, 0);
    PUT_ULONG_BE(ctx->state[1], output, 4);
    PUT_ULONG_BE(ctx->state[2], output, 8);
    PUT_ULONG_BE(ctx->state[3], output, 12);
    PUT_ULONG_BE(ctx->state[4], output, 16);
}


/*
    output = SHA-1( input buffer )
 */
void sha1(uchar *input, int ilen, uchar output[20])
{
    sha1_context ctx;

    sha1_starts(&ctx);
    sha1_update(&ctx, input, ilen);
    sha1_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha1_context));
}


/*
    output = SHA-1( file contents )
 */
int sha1_file(char *path, uchar output[20])
{
    FILE *f;
    size_t n;
    sha1_context ctx;
    uchar buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    sha1_starts(&ctx);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha1_update(&ctx, buf, (int)n);
    }
    sha1_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha1_context));

    if (ferror(f) != 0) {
        fclose(f);
        return 2;
    }
    fclose(f);
    return 0;
}


/*
    SHA-1 HMAC context setup
 */
void sha1_hmac_starts(sha1_context *ctx, uchar *key, int keylen)
{
    int i;
    uchar sum[20];

    if (keylen > 64) {
        sha1(key, keylen, sum);
        keylen = 20;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 64);
    memset(ctx->opad, 0x5C, 64);

    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    sha1_starts(ctx);
    sha1_update(ctx, ctx->ipad, 64);
    memset(sum, 0, sizeof(sum));
}


/*
    SHA-1 HMAC process buffer
 */
void sha1_hmac_update(sha1_context *ctx, uchar *input, int ilen)
{
    sha1_update(ctx, input, ilen);
}


/*
    SHA-1 HMAC final digest
 */
void sha1_hmac_finish(sha1_context *ctx, uchar output[20])
{
    uchar tmpbuf[20];

    sha1_finish(ctx, tmpbuf);
    sha1_starts(ctx);
    sha1_update(ctx, ctx->opad, 64);
    sha1_update(ctx, tmpbuf, 20);
    sha1_finish(ctx, output);
    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-SHA-1( hmac key, input buffer )
 */
void sha1_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[20])
{
    sha1_context ctx;

    sha1_hmac_starts(&ctx, key, keylen);
    sha1_hmac_update(&ctx, input, ilen);
    sha1_hmac_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha1_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/sha2.c ************/


/*
    sha2.c -- FIPS-180-2 compliant SHA-256 implementation

    The SHA-256 Secure Hash Standard was published by NIST in 2002.
    http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_SHA2

/*
    32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ] << 24 )    \
            | ( (ulong) (b)[(i) + 1] << 16 )    \
            | ( (ulong) (b)[(i) + 2] <<  8 )    \
            | ( (ulong) (b)[(i) + 3]       );   \
    }
#endif

#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 2] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 3] = (uchar) ( (n)       );   \
    }
#endif

/*
    SHA-256 context setup
 */
void sha2_starts(sha2_context *ctx, int is224)
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    if (is224 == 0) {
        /* SHA-256 */
        ctx->state[0] = 0x6A09E667;
        ctx->state[1] = 0xBB67AE85;
        ctx->state[2] = 0x3C6EF372;
        ctx->state[3] = 0xA54FF53A;
        ctx->state[4] = 0x510E527F;
        ctx->state[5] = 0x9B05688C;
        ctx->state[6] = 0x1F83D9AB;
        ctx->state[7] = 0x5BE0CD19;
    } else {
        /* SHA-224 */
        ctx->state[0] = 0xC1059ED8;
        ctx->state[1] = 0x367CD507;
        ctx->state[2] = 0x3070DD17;
        ctx->state[3] = 0xF70E5939;
        ctx->state[4] = 0xFFC00B31;
        ctx->state[5] = 0x68581511;
        ctx->state[6] = 0x64F98FA7;
        ctx->state[7] = 0xBEFA4FA4;
    }

    ctx->is224 = is224;
}


static void sha2_process(sha2_context *ctx, uchar data[64])
{
    ulong temp1, temp2, W[64];
    ulong A, B, C, D, E, F, G, H;

    GET_ULONG_BE(W[0], data, 0);
    GET_ULONG_BE(W[1], data, 4);
    GET_ULONG_BE(W[2], data, 8);
    GET_ULONG_BE(W[3], data, 12);
    GET_ULONG_BE(W[4], data, 16);
    GET_ULONG_BE(W[5], data, 20);
    GET_ULONG_BE(W[6], data, 24);
    GET_ULONG_BE(W[7], data, 28);
    GET_ULONG_BE(W[8], data, 32);
    GET_ULONG_BE(W[9], data, 36);
    GET_ULONG_BE(W[10], data, 40);
    GET_ULONG_BE(W[11], data, 44);
    GET_ULONG_BE(W[12], data, 48);
    GET_ULONG_BE(W[13], data, 52);
    GET_ULONG_BE(W[14], data, 56);
    GET_ULONG_BE(W[15], data, 60);

#define  SHR(x,n) ((x & 0xFFFFFFFF) >> n)
#define ROTR(x,n) (SHR(x,n) | (x << (32 - n)))

#define S0(x) (ROTR(x, 7) ^ ROTR(x,18) ^  SHR(x, 3))
#define S1(x) (ROTR(x,17) ^ ROTR(x,19) ^  SHR(x,10))

#define S2(x) (ROTR(x, 2) ^ ROTR(x,13) ^ ROTR(x,22))
#define S3(x) (ROTR(x, 6) ^ ROTR(x,11) ^ ROTR(x,25))

#define F0(x,y,z) ((x & y) | (z & (x | y)))
#define F1(x,y,z) (z ^ (x & (y ^ z)))

#define R(t)                                    \
    (                                           \
        W[t] = S1(W[t -  2]) + W[t -  7] +      \
        S0(W[t - 15]) + W[t - 16]               \
        )

#define P(a,b,c,d,e,f,g,h,x,K)                  \
    {                                           \
        temp1 = h + S3(e) + F1(e,f,g) + K + x;  \
        temp2 = S2(a) + F0(a,b,c);              \
        d += temp1; h = temp1 + temp2;          \
    }

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];
    E = ctx->state[4];
    F = ctx->state[5];
    G = ctx->state[6];
    H = ctx->state[7];

    P(A, B, C, D, E, F, G, H, W[0], 0x428A2F98);
    P(H, A, B, C, D, E, F, G, W[1], 0x71374491);
    P(G, H, A, B, C, D, E, F, W[2], 0xB5C0FBCF);
    P(F, G, H, A, B, C, D, E, W[3], 0xE9B5DBA5);
    P(E, F, G, H, A, B, C, D, W[4], 0x3956C25B);
    P(D, E, F, G, H, A, B, C, W[5], 0x59F111F1);
    P(C, D, E, F, G, H, A, B, W[6], 0x923F82A4);
    P(B, C, D, E, F, G, H, A, W[7], 0xAB1C5ED5);
    P(A, B, C, D, E, F, G, H, W[8], 0xD807AA98);
    P(H, A, B, C, D, E, F, G, W[9], 0x12835B01);
    P(G, H, A, B, C, D, E, F, W[10], 0x243185BE);
    P(F, G, H, A, B, C, D, E, W[11], 0x550C7DC3);
    P(E, F, G, H, A, B, C, D, W[12], 0x72BE5D74);
    P(D, E, F, G, H, A, B, C, W[13], 0x80DEB1FE);
    P(C, D, E, F, G, H, A, B, W[14], 0x9BDC06A7);
    P(B, C, D, E, F, G, H, A, W[15], 0xC19BF174);
    P(A, B, C, D, E, F, G, H, R(16), 0xE49B69C1);
    P(H, A, B, C, D, E, F, G, R(17), 0xEFBE4786);
    P(G, H, A, B, C, D, E, F, R(18), 0x0FC19DC6);
    P(F, G, H, A, B, C, D, E, R(19), 0x240CA1CC);
    P(E, F, G, H, A, B, C, D, R(20), 0x2DE92C6F);
    P(D, E, F, G, H, A, B, C, R(21), 0x4A7484AA);
    P(C, D, E, F, G, H, A, B, R(22), 0x5CB0A9DC);
    P(B, C, D, E, F, G, H, A, R(23), 0x76F988DA);
    P(A, B, C, D, E, F, G, H, R(24), 0x983E5152);
    P(H, A, B, C, D, E, F, G, R(25), 0xA831C66D);
    P(G, H, A, B, C, D, E, F, R(26), 0xB00327C8);
    P(F, G, H, A, B, C, D, E, R(27), 0xBF597FC7);
    P(E, F, G, H, A, B, C, D, R(28), 0xC6E00BF3);
    P(D, E, F, G, H, A, B, C, R(29), 0xD5A79147);
    P(C, D, E, F, G, H, A, B, R(30), 0x06CA6351);
    P(B, C, D, E, F, G, H, A, R(31), 0x14292967);
    P(A, B, C, D, E, F, G, H, R(32), 0x27B70A85);
    P(H, A, B, C, D, E, F, G, R(33), 0x2E1B2138);
    P(G, H, A, B, C, D, E, F, R(34), 0x4D2C6DFC);
    P(F, G, H, A, B, C, D, E, R(35), 0x53380D13);
    P(E, F, G, H, A, B, C, D, R(36), 0x650A7354);
    P(D, E, F, G, H, A, B, C, R(37), 0x766A0ABB);
    P(C, D, E, F, G, H, A, B, R(38), 0x81C2C92E);
    P(B, C, D, E, F, G, H, A, R(39), 0x92722C85);
    P(A, B, C, D, E, F, G, H, R(40), 0xA2BFE8A1);
    P(H, A, B, C, D, E, F, G, R(41), 0xA81A664B);
    P(G, H, A, B, C, D, E, F, R(42), 0xC24B8B70);
    P(F, G, H, A, B, C, D, E, R(43), 0xC76C51A3);
    P(E, F, G, H, A, B, C, D, R(44), 0xD192E819);
    P(D, E, F, G, H, A, B, C, R(45), 0xD6990624);
    P(C, D, E, F, G, H, A, B, R(46), 0xF40E3585);
    P(B, C, D, E, F, G, H, A, R(47), 0x106AA070);
    P(A, B, C, D, E, F, G, H, R(48), 0x19A4C116);
    P(H, A, B, C, D, E, F, G, R(49), 0x1E376C08);
    P(G, H, A, B, C, D, E, F, R(50), 0x2748774C);
    P(F, G, H, A, B, C, D, E, R(51), 0x34B0BCB5);
    P(E, F, G, H, A, B, C, D, R(52), 0x391C0CB3);
    P(D, E, F, G, H, A, B, C, R(53), 0x4ED8AA4A);
    P(C, D, E, F, G, H, A, B, R(54), 0x5B9CCA4F);
    P(B, C, D, E, F, G, H, A, R(55), 0x682E6FF3);
    P(A, B, C, D, E, F, G, H, R(56), 0x748F82EE);
    P(H, A, B, C, D, E, F, G, R(57), 0x78A5636F);
    P(G, H, A, B, C, D, E, F, R(58), 0x84C87814);
    P(F, G, H, A, B, C, D, E, R(59), 0x8CC70208);
    P(E, F, G, H, A, B, C, D, R(60), 0x90BEFFFA);
    P(D, E, F, G, H, A, B, C, R(61), 0xA4506CEB);
    P(C, D, E, F, G, H, A, B, R(62), 0xBEF9A3F7);
    P(B, C, D, E, F, G, H, A, R(63), 0xC67178F2);

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
    ctx->state[4] += E;
    ctx->state[5] += F;
    ctx->state[6] += G;
    ctx->state[7] += H;
}


/*
    SHA-256 process buffer
 */
void sha2_update(sha2_context *ctx, uchar *input, int ilen)
{
    int fill;
    ulong left;

    if (ilen <= 0) {
        return;
    }
    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if (ctx->total[0] < (ulong) ilen) {
        ctx->total[1]++;
    }
    if (left && ilen >= fill) {
        memcpy((void *)(ctx->buffer + left), (void *)input, fill);
        sha2_process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }
    while (ilen >= 64) {
        sha2_process(ctx, input);
        input += 64;
        ilen -= 64;
    }
    if (ilen > 0) {
        memcpy((void *)(ctx->buffer + left), (void *)input, ilen);
    }
}


static const uchar sha2_padding[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
    SHA-256 final digest
 */
void sha2_finish(sha2_context *ctx, uchar output[32])
{
    ulong last, padn;
    ulong high, low;
    uchar msglen[8];

    high = (ctx->total[0] >> 29) | (ctx->total[1] << 3);
    low = (ctx->total[0] << 3);

    PUT_ULONG_BE(high, msglen, 0);
    PUT_ULONG_BE(low, msglen, 4);

    last = ctx->total[0] & 0x3F;
    padn = (last < 56) ? (56 - last) : (120 - last);

    sha2_update(ctx, (uchar *)sha2_padding, padn);
    sha2_update(ctx, msglen, 8);

    PUT_ULONG_BE(ctx->state[0], output, 0);
    PUT_ULONG_BE(ctx->state[1], output, 4);
    PUT_ULONG_BE(ctx->state[2], output, 8);
    PUT_ULONG_BE(ctx->state[3], output, 12);
    PUT_ULONG_BE(ctx->state[4], output, 16);
    PUT_ULONG_BE(ctx->state[5], output, 20);
    PUT_ULONG_BE(ctx->state[6], output, 24);

    if (ctx->is224 == 0)
        PUT_ULONG_BE(ctx->state[7], output, 28);
}


/*
    output = SHA-256( input buffer )
 */
void sha2(uchar *input, int ilen, uchar output[32], int is224)
{
    sha2_context ctx;

    sha2_starts(&ctx, is224);
    sha2_update(&ctx, input, ilen);
    sha2_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha2_context));
}


/*
    output = SHA-256( file contents )
 */
int sha2_file(char *path, uchar output[32], int is224)
{
    FILE *f;
    size_t n;
    sha2_context ctx;
    uchar buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    sha2_starts(&ctx, is224);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha2_update(&ctx, buf, (int)n);
    }
    sha2_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha2_context));
    if (ferror(f) != 0) {
        fclose(f);
        return 2;
    }
    fclose(f);
    return 0;
}

/*
    SHA-256 HMAC context setup
 */
void sha2_hmac_starts(sha2_context *ctx, uchar *key, int keylen, int is224)
{
    int i;
    uchar sum[32];

    if (keylen > 64) {
        sha2(key, keylen, sum, is224);
        keylen = (is224) ? 28 : 32;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 64);
    memset(ctx->opad, 0x5C, 64);
    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    sha2_starts(ctx, is224);
    sha2_update(ctx, ctx->ipad, 64);
    memset(sum, 0, sizeof(sum));
}


/*
    SHA-256 HMAC process buffer
 */
void sha2_hmac_update(sha2_context *ctx, uchar *input, int ilen)
{
    sha2_update(ctx, input, ilen);
}


/*
    SHA-256 HMAC final digest
 */
void sha2_hmac_finish(sha2_context *ctx, uchar output[32])
{
    int is224, hlen;
    uchar tmpbuf[32];

    is224 = ctx->is224;
    hlen = (is224 == 0) ? 32 : 28;

    sha2_finish(ctx, tmpbuf);
    sha2_starts(ctx, is224);
    sha2_update(ctx, ctx->opad, 64);
    sha2_update(ctx, tmpbuf, hlen);
    sha2_finish(ctx, output);
    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-SHA-256( hmac key, input buffer )
 */
void sha2_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[32], int is224)
{
    sha2_context ctx;

    sha2_hmac_starts(&ctx, key, keylen, is224);
    sha2_hmac_update(&ctx, input, ilen);
    sha2_hmac_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha2_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/sha4.c ************/


/*
    sha4.c -- FIPS-180-2 compliant SHA-384/512 implementation

    The SHA-512 Secure Hash Standard was published by NIST in 2002.
    http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_SHA4

/*
    64-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT64_BE
#define GET_UINT64_BE(n,b,i)                    \
    {                                           \
        (n) = ( (uint64) (b)[(i)    ] << 56 )   \
            | ( (uint64) (b)[(i) + 1] << 48 )   \
            | ( (uint64) (b)[(i) + 2] << 40 )   \
            | ( (uint64) (b)[(i) + 3] << 32 )   \
            | ( (uint64) (b)[(i) + 4] << 24 )   \
            | ( (uint64) (b)[(i) + 5] << 16 )   \
            | ( (uint64) (b)[(i) + 6] <<  8 )   \
            | ( (uint64) (b)[(i) + 7]       );  \
    }
#endif

#ifndef PUT_UINT64_BE
#define PUT_UINT64_BE(n,b,i)                    \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 56 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 48 );   \
        (b)[(i) + 2] = (uchar) ( (n) >> 40 );   \
        (b)[(i) + 3] = (uchar) ( (n) >> 32 );   \
        (b)[(i) + 4] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 5] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 6] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 7] = (uchar) ( (n)       );   \
    }
#endif

/*
    Round constants
 */
static const uint64 K[80] = {
    UL64(0x428A2F98D728AE22), UL64(0x7137449123EF65CD),
    UL64(0xB5C0FBCFEC4D3B2F), UL64(0xE9B5DBA58189DBBC),
    UL64(0x3956C25BF348B538), UL64(0x59F111F1B605D019),
    UL64(0x923F82A4AF194F9B), UL64(0xAB1C5ED5DA6D8118),
    UL64(0xD807AA98A3030242), UL64(0x12835B0145706FBE),
    UL64(0x243185BE4EE4B28C), UL64(0x550C7DC3D5FFB4E2),
    UL64(0x72BE5D74F27B896F), UL64(0x80DEB1FE3B1696B1),
    UL64(0x9BDC06A725C71235), UL64(0xC19BF174CF692694),
    UL64(0xE49B69C19EF14AD2), UL64(0xEFBE4786384F25E3),
    UL64(0x0FC19DC68B8CD5B5), UL64(0x240CA1CC77AC9C65),
    UL64(0x2DE92C6F592B0275), UL64(0x4A7484AA6EA6E483),
    UL64(0x5CB0A9DCBD41FBD4), UL64(0x76F988DA831153B5),
    UL64(0x983E5152EE66DFAB), UL64(0xA831C66D2DB43210),
    UL64(0xB00327C898FB213F), UL64(0xBF597FC7BEEF0EE4),
    UL64(0xC6E00BF33DA88FC2), UL64(0xD5A79147930AA725),
    UL64(0x06CA6351E003826F), UL64(0x142929670A0E6E70),
    UL64(0x27B70A8546D22FFC), UL64(0x2E1B21385C26C926),
    UL64(0x4D2C6DFC5AC42AED), UL64(0x53380D139D95B3DF),
    UL64(0x650A73548BAF63DE), UL64(0x766A0ABB3C77B2A8),
    UL64(0x81C2C92E47EDAEE6), UL64(0x92722C851482353B),
    UL64(0xA2BFE8A14CF10364), UL64(0xA81A664BBC423001),
    UL64(0xC24B8B70D0F89791), UL64(0xC76C51A30654BE30),
    UL64(0xD192E819D6EF5218), UL64(0xD69906245565A910),
    UL64(0xF40E35855771202A), UL64(0x106AA07032BBD1B8),
    UL64(0x19A4C116B8D2D0C8), UL64(0x1E376C085141AB53),
    UL64(0x2748774CDF8EEB99), UL64(0x34B0BCB5E19B48A8),
    UL64(0x391C0CB3C5C95A63), UL64(0x4ED8AA4AE3418ACB),
    UL64(0x5B9CCA4F7763E373), UL64(0x682E6FF3D6B2B8A3),
    UL64(0x748F82EE5DEFB2FC), UL64(0x78A5636F43172F60),
    UL64(0x84C87814A1F0AB72), UL64(0x8CC702081A6439EC),
    UL64(0x90BEFFFA23631E28), UL64(0xA4506CEBDE82BDE9),
    UL64(0xBEF9A3F7B2C67915), UL64(0xC67178F2E372532B),
    UL64(0xCA273ECEEA26619C), UL64(0xD186B8C721C0C207),
    UL64(0xEADA7DD6CDE0EB1E), UL64(0xF57D4F7FEE6ED178),
    UL64(0x06F067AA72176FBA), UL64(0x0A637DC5A2C898A6),
    UL64(0x113F9804BEF90DAE), UL64(0x1B710B35131C471B),
    UL64(0x28DB77F523047D84), UL64(0x32CAAB7B40C72493),
    UL64(0x3C9EBE0A15C9BEBC), UL64(0x431D67C49C100D4C),
    UL64(0x4CC5D4BECB3E42B6), UL64(0x597F299CFC657E2A),
    UL64(0x5FCB6FAB3AD6FAEC), UL64(0x6C44198C4A475817)
};


/*
    SHA-512 context setup
 */
void sha4_starts(sha4_context *ctx, int is384)
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    if (is384 == 0) {
        /* SHA-512 */
        ctx->state[0] = UL64(0x6A09E667F3BCC908);
        ctx->state[1] = UL64(0xBB67AE8584CAA73B);
        ctx->state[2] = UL64(0x3C6EF372FE94F82B);
        ctx->state[3] = UL64(0xA54FF53A5F1D36F1);
        ctx->state[4] = UL64(0x510E527FADE682D1);
        ctx->state[5] = UL64(0x9B05688C2B3E6C1F);
        ctx->state[6] = UL64(0x1F83D9ABFB41BD6B);
        ctx->state[7] = UL64(0x5BE0CD19137E2179);
    } else {
        /* SHA-384 */
        ctx->state[0] = UL64(0xCBBB9D5DC1059ED8);
        ctx->state[1] = UL64(0x629A292A367CD507);
        ctx->state[2] = UL64(0x9159015A3070DD17);
        ctx->state[3] = UL64(0x152FECD8F70E5939);
        ctx->state[4] = UL64(0x67332667FFC00B31);
        ctx->state[5] = UL64(0x8EB44A8768581511);
        ctx->state[6] = UL64(0xDB0C2E0D64F98FA7);
        ctx->state[7] = UL64(0x47B5481DBEFA4FA4);
    }

    ctx->is384 = is384;
}


static void sha4_process(sha4_context *ctx, uchar data[128])
{
    int i;
    uint64 temp1, temp2, W[80];
    uint64 A, B, C, D, E, F, G, H;

#define  SHR(x,n) (x >> n)
#define ROTR(x,n) (SHR(x,n) | (x << (64 - n)))

#define S0(x) (ROTR(x, 1) ^ ROTR(x, 8) ^  SHR(x, 7))
#define S1(x) (ROTR(x,19) ^ ROTR(x,61) ^  SHR(x, 6))

#define S2(x) (ROTR(x,28) ^ ROTR(x,34) ^ ROTR(x,39))
#define S3(x) (ROTR(x,14) ^ ROTR(x,18) ^ ROTR(x,41))

#define F0(x,y,z) ((x & y) | (z & (x | y)))
#define F1(x,y,z) (z ^ (x & (y ^ z)))

#define P(a,b,c,d,e,f,g,h,x,K)                  \
    {                                           \
        temp1 = h + S3(e) + F1(e,f,g) + K + x;  \
        temp2 = S2(a) + F0(a,b,c);              \
        d += temp1; h = temp1 + temp2;          \
    }

    for (i = 0; i < 16; i++) {
        GET_UINT64_BE(W[i], data, i << 3);
    }

    for (; i < 80; i++) {
        W[i] = S1(W[i - 2]) + W[i - 7] + S0(W[i - 15]) + W[i - 16];
    }

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];
    E = ctx->state[4];
    F = ctx->state[5];
    G = ctx->state[6];
    H = ctx->state[7];
    i = 0;

    do {
        P(A, B, C, D, E, F, G, H, W[i], K[i]);
        i++;
        P(H, A, B, C, D, E, F, G, W[i], K[i]);
        i++;
        P(G, H, A, B, C, D, E, F, W[i], K[i]);
        i++;
        P(F, G, H, A, B, C, D, E, W[i], K[i]);
        i++;
        P(E, F, G, H, A, B, C, D, W[i], K[i]);
        i++;
        P(D, E, F, G, H, A, B, C, W[i], K[i]);
        i++;
        P(C, D, E, F, G, H, A, B, W[i], K[i]);
        i++;
        P(B, C, D, E, F, G, H, A, W[i], K[i]);
        i++;
    } while (i < 80);

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
    ctx->state[4] += E;
    ctx->state[5] += F;
    ctx->state[6] += G;
    ctx->state[7] += H;
}


/*
    SHA-512 process buffer
 */
void sha4_update(sha4_context *ctx, uchar *input, int ilen)
{
    int fill;
    uint64 left;

    if (ilen <= 0)
        return;

    left = ctx->total[0] & 0x7F;
    fill = (int)(128 - left);

    ctx->total[0] += ilen;

    if (ctx->total[0] < (uint64)ilen)
        ctx->total[1]++;

    if (left && ilen >= fill) {
        memcpy((void *)(ctx->buffer + left), (void *)input, fill);
        sha4_process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }
    while (ilen >= 128) {
        sha4_process(ctx, input);
        input += 128;
        ilen -= 128;
    }
    if (ilen > 0) {
        memcpy((void *)(ctx->buffer + left), (void *)input, ilen);
    }
}


static const uchar sha4_padding[128] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
    SHA-512 final digest
 */
void sha4_finish(sha4_context *ctx, uchar output[64])
{
    int last, padn;
    uint64 high, low;
    uchar msglen[16];

    high = (ctx->total[0] >> 61) | (ctx->total[1] << 3);
    low = (ctx->total[0] << 3);

    PUT_UINT64_BE(high, msglen, 0);
    PUT_UINT64_BE(low, msglen, 8);

    last = (int)(ctx->total[0] & 0x7F);
    padn = (last < 112) ? (112 - last) : (240 - last);

    sha4_update(ctx, (uchar *)sha4_padding, padn);
    sha4_update(ctx, msglen, 16);

    PUT_UINT64_BE(ctx->state[0], output, 0);
    PUT_UINT64_BE(ctx->state[1], output, 8);
    PUT_UINT64_BE(ctx->state[2], output, 16);
    PUT_UINT64_BE(ctx->state[3], output, 24);
    PUT_UINT64_BE(ctx->state[4], output, 32);
    PUT_UINT64_BE(ctx->state[5], output, 40);

    if (ctx->is384 == 0) {
        PUT_UINT64_BE(ctx->state[6], output, 48);
        PUT_UINT64_BE(ctx->state[7], output, 56);
    }
}


/*
    output = SHA-512( input buffer )
 */
void sha4(uchar *input, int ilen, uchar output[64], int is384)
{
    sha4_context ctx;

    sha4_starts(&ctx, is384);
    sha4_update(&ctx, input, ilen);
    sha4_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha4_context));
}


/*
    output = SHA-512( file contents )
 */
int sha4_file(char *path, uchar output[64], int is384)
{
    FILE *f;
    size_t n;
    sha4_context ctx;
    uchar buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    sha4_starts(&ctx, is384);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha4_update(&ctx, buf, (int)n);
    }
    sha4_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha4_context));

    if (ferror(f) != 0) {
        fclose(f);
        return 2;
    }
    fclose(f);
    return 0;
}


/*
    SHA-512 HMAC context setup
 */
void sha4_hmac_starts(sha4_context *ctx, uchar *key, int keylen, int is384)
{
    int i;
    uchar sum[64];

    if (keylen > 128) {
        sha4(key, keylen, sum, is384);
        keylen = (is384) ? 48 : 64;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 128);
    memset(ctx->opad, 0x5C, 128);
    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    sha4_starts(ctx, is384);
    sha4_update(ctx, ctx->ipad, 128);
    memset(sum, 0, sizeof(sum));
}


/*
    SHA-512 HMAC process buffer
 */
void sha4_hmac_update(sha4_context *ctx, uchar *input, int ilen)
{
    sha4_update(ctx, input, ilen);
}


/*
    SHA-512 HMAC final digest
 */
void sha4_hmac_finish(sha4_context *ctx, uchar output[64])
{
    int is384, hlen;
    uchar tmpbuf[64];

    is384 = ctx->is384;
    hlen = (is384 == 0) ? 64 : 48;
    sha4_finish(ctx, tmpbuf);
    sha4_starts(ctx, is384);
    sha4_update(ctx, ctx->opad, 128);
    sha4_update(ctx, tmpbuf, hlen);
    sha4_finish(ctx, output);
    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-SHA-512( hmac key, input buffer )
 */
void sha4_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[64], int is384)
{
    sha4_context ctx;

    sha4_hmac_starts(&ctx, key, keylen, is384);
    sha4_hmac_update(&ctx, input, ilen);
    sha4_hmac_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha4_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/ssl_cli.c ************/


/*
    ssl_cli.c -- SSLv3/TLSv1 client-side functions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if ME_EST_CLIENT

static int ssl_write_client_hello(ssl_context * ssl)
{
    int ret, i, n;
    uchar *buf;
    uchar *p;
    time_t t;

    SSL_DEBUG_MSG(2, ("=> write client hello"));

    ssl->major_ver = SSL_MAJOR_VERSION_3;
    ssl->minor_ver = SSL_MINOR_VERSION_0;

    ssl->max_major_ver = SSL_MAJOR_VERSION_3;
    ssl->max_minor_ver = SSL_MINOR_VERSION_1;

    /*
           0  .   0       handshake type
           1  .   3       handshake length
           4  .   5       highest version supported
           6  .   9       current UNIX time
          10  .  37       random bytes
     */
    buf = ssl->out_msg;
    p = buf + 4;

    *p++ = (uchar)ssl->max_major_ver;
    *p++ = (uchar)ssl->max_minor_ver;

    SSL_DEBUG_MSG(3, ("client hello, max version: [%d:%d]", buf[4], buf[5]));

    t = time(NULL);
    *p++ = (uchar)(t >> 24);
    *p++ = (uchar)(t >> 16);
    *p++ = (uchar)(t >> 8);
    *p++ = (uchar)(t);

    SSL_DEBUG_MSG(3, ("client hello, current time: %lu", t));

    for (i = 28; i > 0; i--) {
        *p++ = (uchar)ssl->f_rng(ssl->p_rng);
    }
    memcpy(ssl->randbytes, buf + 6, 32);

    SSL_DEBUG_BUF(3, "client hello, random bytes", buf + 6, 32);

    /*
          38  .  38       session id length
          39  . 39+n  session id
         40+n . 41+n  cipherlist length
         42+n . ..        cipherlist
         ..       . ..    compression alg. (0)
         ..       . ..    extensions (unused)
     */
    n = ssl->session->length;

    if (n < 16 || n > 32 || ssl->resume == 0 || t - ssl->session->start > ssl->timeout) {
        n = 0;
    }
    *p++ = (uchar)n;

    for (i = 0; i < n; i++) {
        *p++ = ssl->session->id[i];
    }
    SSL_DEBUG_MSG(3, ("client hello, session id len.: %d", n));
    SSL_DEBUG_BUF(3, "client hello, session id", buf + 39, n);

    for (n = 0; ssl->ciphers[n] != 0; n++) {}

    *p++ = (uchar)(n >> 7);
    *p++ = (uchar)(n << 1);

    SSL_DEBUG_MSG(3, ("client hello, got %d ciphers", n));

    for (i = 0; i < n; i++) {
        SSL_DEBUG_MSG(3, ("client hello, add cipher: %2d", ssl->ciphers[i]));
        *p++ = (uchar)(ssl->ciphers[i] >> 8);
        *p++ = (uchar)(ssl->ciphers[i]);
    }

    SSL_DEBUG_MSG(3, ("client hello, compress len.: %d", 1));
    SSL_DEBUG_MSG(3, ("client hello, compress alg.: %d", 0));

    *p++ = 1;
    *p++ = SSL_COMPRESS_NULL;

    if (ssl->hostname != NULL) {
        SSL_DEBUG_MSG(3, ("client hello, server name extension: %s", ssl->hostname));

        *p++ = (uchar)(((ssl->hostname_len + 9) >> 8) & 0xFF);
        *p++ = (uchar)(((ssl->hostname_len + 9)) & 0xFF);

        *p++ = (uchar)((TLS_EXT_SERVERNAME >> 8) & 0xFF);
        *p++ = (uchar)((TLS_EXT_SERVERNAME) & 0xFF);

        *p++ = (uchar)(((ssl->hostname_len + 5) >> 8) & 0xFF);
        *p++ = (uchar)(((ssl->hostname_len + 5)) & 0xFF);

        *p++ = (uchar)(((ssl->hostname_len + 3) >> 8) & 0xFF);
        *p++ = (uchar)(((ssl->hostname_len + 3)) & 0xFF);

        *p++ = (uchar)((TLS_EXT_SERVERNAME_HOSTNAME) & 0xFF);
        *p++ = (uchar)((ssl->hostname_len >> 8) & 0xFF);
        *p++ = (uchar)((ssl->hostname_len) & 0xFF);

        memcpy(p, ssl->hostname, ssl->hostname_len);
        p += ssl->hostname_len;
    }
    ssl->out_msglen = p - buf;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_CLIENT_HELLO;
    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write client hello"));
    return 0;
}


static int ssl_parse_server_hello(ssl_context * ssl)
{
    time_t t;
    int ret, i, n;
    int ext_len;
    uchar *buf;

    SSL_DEBUG_MSG(2, ("=> parse server hello"));

    /*
           0  .   0       handshake type
           1  .   3       handshake length
           4  .   5       protocol version
           6  .   9       UNIX time()
          10  .  37       random bytes
     */
    buf = ssl->in_msg;

    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad server hello message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }
    SSL_DEBUG_MSG(3, ("server hello, chosen version: [%d:%d]", buf[4], buf[5]));

    if (ssl->in_hslen < 42 ||
        buf[0] != SSL_HS_SERVER_HELLO || buf[4] != SSL_MAJOR_VERSION_3) {
        SSL_DEBUG_MSG(1, ("bad server hello message"));
        return EST_ERR_SSL_BAD_HS_SERVER_HELLO;
    }
    if (buf[5] != SSL_MINOR_VERSION_0 && buf[5] != SSL_MINOR_VERSION_1) {
        SSL_DEBUG_MSG(1, ("bad server hello message"));
        return EST_ERR_SSL_BAD_HS_SERVER_HELLO;
    }
    ssl->minor_ver = buf[5];

    t = ((time_t) buf[6] << 24) | ((time_t) buf[7] << 16) | ((time_t) buf[8] << 8) | ((time_t) buf[9]);
    memcpy(ssl->randbytes + 32, buf + 6, 32);

    n = buf[38];
    SSL_DEBUG_MSG(3, ("server hello, current time: %lu", t));
    SSL_DEBUG_BUF(3, "server hello, random bytes", buf + 6, 32);

    /*
          38  .  38       session id length
          39  . 38+n  session id
         39+n . 40+n  chosen cipher
         41+n . 41+n  chosen compression alg.
         42+n . 43+n  extensions length
         44+n . 44+n+m extensions
     */
    if (n < 0 || n > 32 || ssl->in_hslen > 42 + n) {
        ext_len = ((buf[42 + n] << 8) | (buf[43 + n])) + 2;
    } else {
        ext_len = 0;
    }
    if (n < 0 || n > 32 || ssl->in_hslen != 42 + n + ext_len) {
        SSL_DEBUG_MSG(1, ("bad server hello message"));
        return EST_ERR_SSL_BAD_HS_SERVER_HELLO;
    }
    i = (buf[39 + n] << 8) | buf[40 + n];
    SSL_DEBUG_MSG(3, ("server hello, session id len.: %d", n));
    SSL_DEBUG_BUF(3, "server hello, session id", buf + 39, n);

    /*
       Check if the session can be resumed
     */
    if (ssl->resume == 0 || n == 0 || ssl->session->cipher != i || ssl->session->length != n ||
            memcmp(ssl->session->id, buf + 39, n) != 0) {
        ssl->state++;
        ssl->resume = 0;
        ssl->session->start = time(NULL);
        ssl->session->cipher = i;
        ssl->session->length = n;
        memcpy(ssl->session->id, buf + 39, n);
    } else {
        ssl->state = SSL_SERVER_CHANGE_CIPHER_SPEC;
        ssl_derive_keys(ssl);
    }
    SSL_DEBUG_MSG(3, ("%s session has been resumed", ssl->resume ? "a" : "no"));
    SSL_DEBUG_MSG(3, ("server hello, chosen cipher: %d", i));
    SSL_DEBUG_MSG(3, ("server hello, compress alg.: %d", buf[41 + n]));

    i = 0;
    while (1) {
        if (ssl->ciphers[i] == 0) {
            SSL_DEBUG_MSG(1, ("bad server hello message"));
            return EST_ERR_SSL_BAD_HS_SERVER_HELLO;
        }
        if (ssl->ciphers[i++] == ssl->session->cipher)
            break;
    }
    if (buf[41 + n] != SSL_COMPRESS_NULL) {
        SSL_DEBUG_MSG(1, ("bad server hello message"));
        return EST_ERR_SSL_BAD_HS_SERVER_HELLO;
    }
    SSL_DEBUG_MSG(2, ("<= parse server hello"));
    return 0;
}


static int ssl_parse_server_key_exchange(ssl_context * ssl)
{
    int ret, n;
    uchar *p, *end;
    uchar hash[36];
    md5_context md5;
    sha1_context sha1;

    SSL_DEBUG_MSG(2, ("=> parse server key exchange"));

    if (ssl->session->cipher != TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA &&
        ssl->session->cipher != TLS_DHE_RSA_WITH_AES_256_CBC_SHA &&
        ssl->session->cipher != TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
        SSL_DEBUG_MSG(2, ("<= skip parse server key exchange"));
        ssl->state++;
        return 0;
    }
#if !ME_EST_DHM
    SSL_DEBUG_MSG(1, ("support for dhm in not available"));
    return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#else
    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad server key exchange message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }
    if (ssl->in_msg[0] != SSL_HS_SERVER_KEY_EXCHANGE) {
        SSL_DEBUG_MSG(1, ("bad server key exchange message"));
        return EST_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE;
    }

    /*
       Ephemeral DH parameters:
      
       struct {
               opaque dh_p<1..2^16-1>;
               opaque dh_g<1..2^16-1>;
               opaque dh_Ys<1..2^16-1>;
       } ServerDHParams;
     */
    p = ssl->in_msg + 4;
    end = ssl->in_msg + ssl->in_hslen;

    if ((ret = dhm_read_params(&ssl->dhm_ctx, &p, end)) != 0) {
        SSL_DEBUG_MSG(1, ("bad server key exchange message"));
        return EST_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE;
    }
    if ((int)(end - p) != ssl->peer_cert->rsa.len) {
        SSL_DEBUG_MSG(1, ("bad server key exchange message"));
        return EST_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE;
    }
    if (ssl->dhm_ctx.len < 64 || ssl->dhm_ctx.len > 256) {
        SSL_DEBUG_MSG(1, ("bad server key exchange message"));
        return EST_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE;
    }
    SSL_DEBUG_MPI(3, "DHM: P ", &ssl->dhm_ctx.P);
    SSL_DEBUG_MPI(3, "DHM: G ", &ssl->dhm_ctx.G);
    SSL_DEBUG_MPI(3, "DHM: GY", &ssl->dhm_ctx.GY);

    /*
       digitally-signed struct {
               opaque md5_hash[16];
               opaque sha_hash[20];
       };
      
       md5_hash MD5(ClientHello.random + ServerHello.random + ServerParams);
       sha_hash SHA(ClientHello.random + ServerHello.random + ServerParams);
     */
    n = ssl->in_hslen - (end - p) - 6;
    md5_starts(&md5);
    md5_update(&md5, ssl->randbytes, 64);
    md5_update(&md5, ssl->in_msg + 4, n);
    md5_finish(&md5, hash);

    sha1_starts(&sha1);
    sha1_update(&sha1, ssl->randbytes, 64);
    sha1_update(&sha1, ssl->in_msg + 4, n);
    sha1_finish(&sha1, hash + 16);

    SSL_DEBUG_BUF(3, "parameters hash", hash, 36);

    if ((ret = rsa_pkcs1_verify(&ssl->peer_cert->rsa, RSA_PUBLIC, RSA_RAW, 36, hash, p)) != 0) {
        SSL_DEBUG_RET(1, "rsa_pkcs1_verify", ret);
        return ret;
    }
    ssl->state++;
    SSL_DEBUG_MSG(2, ("<= parse server key exchange"));
    return 0;
#endif
}


static int ssl_parse_certificate_request(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> parse certificate request"));

    /*
           0  .   0       handshake type
           1  .   3       handshake length
           4  .   5       SSL version
           6  .   6       cert type count
           7  .. n-1  cert types
           n  .. n+1  length of all DNs
          n+2 .. n+3  length of DN 1
          n+4 .. ...  Distinguished Name #1
          ... .. ...  length of DN 2, etc.
     */
    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad certificate request message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }
    ssl->client_auth = 0;
    ssl->state++;

    if (ssl->in_msg[0] == SSL_HS_CERTIFICATE_REQUEST) {
        ssl->client_auth++;
    }
    SSL_DEBUG_MSG(3, ("got %s certificate request", ssl->client_auth ? "a" : "no"));
    SSL_DEBUG_MSG(2, ("<= parse certificate request"));
    return 0;
}


static int ssl_parse_server_hello_done(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> parse server hello done"));

    if (ssl->client_auth != 0) {
        if ((ret = ssl_read_record(ssl)) != 0) {
            SSL_DEBUG_RET(3, "ssl_read_record", ret);
            return ret;
        }
        if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
            SSL_DEBUG_MSG(1, ("bad server hello done message"));
            return EST_ERR_SSL_UNEXPECTED_MESSAGE;
        }
    }
    if (ssl->in_hslen != 4 || ssl->in_msg[0] != SSL_HS_SERVER_HELLO_DONE) {
        SSL_DEBUG_MSG(1, ("bad server hello done message"));
        return EST_ERR_SSL_BAD_HS_SERVER_HELLO_DONE;
    }
    ssl->state++;
    SSL_DEBUG_MSG(2, ("<= parse server hello done"));
    return 0;
}


static int ssl_write_client_key_exchange(ssl_context * ssl)
{
    int ret, i, n;

    SSL_DEBUG_MSG(2, ("=> write client key exchange"));

    if (ssl->session->cipher == TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA ||
        ssl->session->cipher == TLS_DHE_RSA_WITH_AES_256_CBC_SHA ||
        ssl->session->cipher == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
#if !ME_EST_DHM
        SSL_DEBUG_MSG(1, ("support for dhm in not available"));
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#else
        /*
           DHM key exchange -- send G^X mod P
         */
        n = ssl->dhm_ctx.len;
        ssl->out_msg[4] = (uchar)(n >> 8);
        ssl->out_msg[5] = (uchar)(n);
        i = 6;

        ret = dhm_make_public(&ssl->dhm_ctx, 256, &ssl->out_msg[i], n, ssl->f_rng, ssl->p_rng);
        if (ret != 0) {
            SSL_DEBUG_RET(1, "dhm_make_public", ret);
            return ret;
        }
        SSL_DEBUG_MPI(3, "DHM: X ", &ssl->dhm_ctx.X);
        SSL_DEBUG_MPI(3, "DHM: GX", &ssl->dhm_ctx.GX);
        ssl->pmslen = ssl->dhm_ctx.len;

        if ((ret = dhm_calc_secret(&ssl->dhm_ctx, ssl->premaster, &ssl->pmslen)) != 0) {
            SSL_DEBUG_RET(1, "dhm_calc_secret", ret);
            return ret;
        }
        SSL_DEBUG_MPI(3, "DHM: K ", &ssl->dhm_ctx.K);
#endif
    } else {
        /*
           RSA key exchange -- send rsa_public(pkcs1 v1.5(premaster))
         */
        ssl->premaster[0] = (uchar)ssl->max_major_ver;
        ssl->premaster[1] = (uchar)ssl->max_minor_ver;
        ssl->pmslen = 48;

        for (i = 2; i < ssl->pmslen; i++) {
            ssl->premaster[i] = (uchar)ssl->f_rng(ssl->p_rng);
        }
        i = 4;
        n = ssl->peer_cert->rsa.len;

        if (ssl->minor_ver != SSL_MINOR_VERSION_0) {
            i += 2;
            ssl->out_msg[4] = (uchar)(n >> 8);
            ssl->out_msg[5] = (uchar)(n);
        }
        ret = rsa_pkcs1_encrypt(&ssl->peer_cert->rsa, RSA_PUBLIC, ssl->pmslen, ssl->premaster, ssl->out_msg + i);
        if (ret != 0) {
            SSL_DEBUG_RET(1, "rsa_pkcs1_encrypt", ret);
            return ret;
        }
    }
    ssl_derive_keys(ssl);
    ssl->out_msglen = i + n;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_CLIENT_KEY_EXCHANGE;
    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write client key exchange"));
    return 0;
}


static int ssl_write_certificate_verify(ssl_context * ssl)
{
    int ret, n;
    uchar hash[36];

    SSL_DEBUG_MSG(2, ("=> write certificate verify"));

    if (ssl->client_auth == 0) {
        SSL_DEBUG_MSG(2, ("<= skip write certificate verify"));
        ssl->state++;
        return 0;
    }
    if (ssl->rsa_key == NULL) {
        SSL_DEBUG_MSG(1, ("got no private key"));
        return EST_ERR_SSL_PRIVATE_KEY_REQUIRED;
    }
    /*
        Make an RSA signature of the handshake digests
     */
    ssl_calc_verify(ssl, hash);

    n = ssl->rsa_key->len;
    ssl->out_msg[4] = (uchar)(n >> 8);
    ssl->out_msg[5] = (uchar)(n);

    if ((ret = rsa_pkcs1_sign(ssl->rsa_key, RSA_PRIVATE, RSA_RAW, 36, hash, ssl->out_msg + 6)) != 0) {
        SSL_DEBUG_RET(1, "rsa_pkcs1_sign", ret);
        return ret;
    }
    ssl->out_msglen = 6 + n;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_CERTIFICATE_VERIFY;
    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write certificate verify"));
    return 0;
}


/*
    SSL handshake -- client side
 */
int ssl_handshake_client(ssl_context * ssl)
{
    int ret = 0;

    SSL_DEBUG_MSG(2, ("=> handshake client"));

    while (ssl->state != SSL_HANDSHAKE_OVER) {
        SSL_DEBUG_MSG(2, ("client state: %d", ssl->state));

        if ((ret = ssl_flush_output(ssl)) != 0) {
            break;
        }
        switch (ssl->state) {
        case SSL_HELLO_REQUEST:
            ssl->state = SSL_CLIENT_HELLO;
            break;

            /*
                ==>       ClientHello
             */
        case SSL_CLIENT_HELLO:
            ret = ssl_write_client_hello(ssl);
            break;

            /*
                <==       ServerHello
                          Certificate
                        ( ServerKeyExchange      )
                        ( CertificateRequest )
                          ServerHelloDone
             */
        case SSL_SERVER_HELLO:
            ret = ssl_parse_server_hello(ssl);
            break;

        case SSL_SERVER_CERTIFICATE:
            ret = ssl_parse_certificate(ssl);
            break;

        case SSL_SERVER_KEY_EXCHANGE:
            ret = ssl_parse_server_key_exchange(ssl);
            break;

        case SSL_CERTIFICATE_REQUEST:
            ret = ssl_parse_certificate_request(ssl);
            break;

        case SSL_SERVER_HELLO_DONE:
            ret = ssl_parse_server_hello_done(ssl);
            break;

            /*
                ==> ( Certificate/Alert )
                      ClientKeyExchange
                    ( CertificateVerify )
                      ChangeCipherSpec
                      Finished
             */
        case SSL_CLIENT_CERTIFICATE:
            ret = ssl_write_certificate(ssl);
            break;

        case SSL_CLIENT_KEY_EXCHANGE:
            ret = ssl_write_client_key_exchange(ssl);
            break;

        case SSL_CERTIFICATE_VERIFY:
            ret = ssl_write_certificate_verify(ssl);
            break;

        case SSL_CLIENT_CHANGE_CIPHER_SPEC:
            ret = ssl_write_change_cipher_spec(ssl);
            break;

        case SSL_CLIENT_FINISHED:
            ret = ssl_write_finished(ssl);
            break;

            /*
                <== ChangeCipherSpec
                    Finished
             */
        case SSL_SERVER_CHANGE_CIPHER_SPEC:
            ret = ssl_parse_change_cipher_spec(ssl);
            break;

        case SSL_SERVER_FINISHED:
            ret = ssl_parse_finished(ssl);
            break;

        case SSL_FLUSH_BUFFERS:
            SSL_DEBUG_MSG(2, ("handshake: done"));
            ssl->state = SSL_HANDSHAKE_OVER;
            break;

        default:
            SSL_DEBUG_MSG(1, ("invalid state %d", ssl->state));
            return EST_ERR_SSL_BAD_INPUT_DATA;
        }
        if (ret != 0) {
            break;
        }
    }
    SSL_DEBUG_MSG(2, ("<= handshake client"));
    return ret;
}

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/ssl_srv.c ************/


/*
    ssl_srv.c -- SSLv3/TLSv1 server-side functions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_SERVER

static int ssl_parse_client_hello(ssl_context * ssl)
{
    int ret, i, j, n;
    int ciph_len, sess_len;
    int chal_len, comp_len;
    uchar *buf, *p;

    SSL_DEBUG_MSG(2, ("=> parse client hello"));

    if ((ret = ssl_fetch_input(ssl, 5)) != 0) {
        SSL_DEBUG_RET(3, "ssl_fetch_input", ret);
        return ret;
    }
    buf = ssl->in_hdr;

    if ((buf[0] & 0x80) != 0) {
        SSL_DEBUG_BUF(4, "record header", buf, 5);

        SSL_DEBUG_MSG(3, ("client hello v2, message type: %d", buf[2]));
        SSL_DEBUG_MSG(3, ("client hello v2, message len.: %d", ((buf[0] & 0x7F) << 8) | buf[1]));
        SSL_DEBUG_MSG(3, ("client hello v2, max. version: [%d:%d]", buf[3], buf[4]));

        /*
         * SSLv2 Client Hello
         *
         * Record layer:
         *     0  .   1   message length
         *
         * SSL layer:
         *     2  .   2   message type
         *     3  .   4   protocol version
         */
        if (buf[2] != SSL_HS_CLIENT_HELLO || buf[3] != SSL_MAJOR_VERSION_3) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        n = ((buf[0] << 8) | buf[1]) & 0x7FFF;

        if (n < 17 || n > 512) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        ssl->max_major_ver = buf[3];
        ssl->max_minor_ver = buf[4];

        ssl->major_ver = SSL_MAJOR_VERSION_3;
        ssl->minor_ver = (buf[4] <= SSL_MINOR_VERSION_1) ? buf[4] : SSL_MINOR_VERSION_1;

        if ((ret = ssl_fetch_input(ssl, 2 + n)) != 0) {
            SSL_DEBUG_RET(3, "ssl_fetch_input", ret);
            return ret;
        }
        md5_update(&ssl->fin_md5, buf + 2, n);
        sha1_update(&ssl->fin_sha1, buf + 2, n);

        buf = ssl->in_msg;
        n = ssl->in_left - 5;

        /*
         *    0  .   1   cipherlist length
         *    2  .   3   session id length
         *    4  .   5   challenge length
         *    6  .  ..   cipherlist
         *   ..  .  ..   session id
         *   ..  .  ..   challenge
         */
        SSL_DEBUG_BUF(4, "record contents", buf, n);

        ciph_len = (buf[0] << 8) | buf[1];
        sess_len = (buf[2] << 8) | buf[3];
        chal_len = (buf[4] << 8) | buf[5];

        SSL_DEBUG_MSG(3, ("ciph_len: %d, sess_len: %d, chal_len: %d", ciph_len, sess_len, chal_len));

        /*
            Make sure each parameter length is valid
         */
        if (ciph_len < 3 || (ciph_len % 3) != 0) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        if (sess_len < 0 || sess_len > 32) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        if (chal_len < 8 || chal_len > 32) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        if (n != 6 + ciph_len + sess_len + chal_len) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        SSL_DEBUG_BUF(3, "client hello, cipherlist", buf + 6, ciph_len);
        SSL_DEBUG_BUF(3, "client hello, session id", buf + 6 + ciph_len, sess_len);
        SSL_DEBUG_BUF(3, "client hello, challenge", buf + 6 + ciph_len + sess_len, chal_len);

        p = buf + 6 + ciph_len;
        ssl->session->length = sess_len;
        memset(ssl->session->id, 0, sizeof(ssl->session->id));
        memcpy(ssl->session->id, p, ssl->session->length);

        p += sess_len;
        memset(ssl->randbytes, 0, 64);
        memcpy(ssl->randbytes + 32 - chal_len, p, chal_len);

        for (i = 0; ssl->ciphers[i] != 0; i++) {
            for (j = 0, p = buf + 6; j < ciph_len; j += 3, p += 3) {
                if (p[0] == 0 &&
                    p[1] == 0 && p[2] == ssl->ciphers[i])
                    goto have_cipher;
            }
        }
    } else {
        SSL_DEBUG_BUF(4, "record header", buf, 5);
        SSL_DEBUG_MSG(3, ("client hello v3, message type: %d", buf[0]));
        SSL_DEBUG_MSG(3, ("client hello v3, message len.: %d", (buf[3] << 8) | buf[4]));
        SSL_DEBUG_MSG(3, ("client hello v3, protocol ver: [%d:%d]", buf[1], buf[2]));

        /*
         * SSLv3 Client Hello
         *
         * Record layer:
         *     0  .   0   message type
         *     1  .   2   protocol version
         *     3  .   4   message length
         */
        if (buf[0] != SSL_MSG_HANDSHAKE || buf[1] != SSL_MAJOR_VERSION_3) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        n = (buf[3] << 8) | buf[4];
        if (n < 45 || n > 512) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        if ((ret = ssl_fetch_input(ssl, 5 + n)) != 0) {
            SSL_DEBUG_RET(3, "ssl_fetch_input", ret);
            return ret;
        }
        buf = ssl->in_msg;
        n = ssl->in_left - 5;

        md5_update(&ssl->fin_md5, buf, n);
        sha1_update(&ssl->fin_sha1, buf, n);

        /*
         * SSL layer:
         *     0  .   0   handshake type
         *     1  .   3   handshake length
         *     4  .   5   protocol version
         *     6  .   9   UNIX time()
         *    10  .  37   random bytes
         *    38  .  38   session id length
         *    39  . 38+x  session id
         *   39+x . 40+x  cipherlist length
         *   41+x .  ..   cipherlist
         *    ..  .  ..   compression alg.
         *    ..  .  ..   extensions
         */
        SSL_DEBUG_BUF(4, "record contents", buf, n);
        SSL_DEBUG_MSG(3, ("client hello v3, handshake type: %d", buf[0]));
        SSL_DEBUG_MSG(3, ("client hello v3, handshake len.: %d", (buf[1] << 16) | (buf[2] << 8) | buf[3]));
        SSL_DEBUG_MSG(3, ("client hello v3, max. version: [%d:%d]", buf[4], buf[5]));

        /*
         * Check the handshake type and protocol version
         */
        if (buf[0] != SSL_HS_CLIENT_HELLO || buf[4] != SSL_MAJOR_VERSION_3) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        ssl->major_ver = SSL_MAJOR_VERSION_3;
        ssl->minor_ver = (buf[5] <= SSL_MINOR_VERSION_1) ? buf[5] : SSL_MINOR_VERSION_1;
        ssl->max_major_ver = buf[4];
        ssl->max_minor_ver = buf[5];

        memcpy(ssl->randbytes, buf + 6, 32);

        /*
            Check the handshake message length
         */
        if (buf[1] != 0 || n != 4 + ((buf[2] << 8) | buf[3])) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        /*
            Check the session length
         */
        sess_len = buf[38];

        if (sess_len < 0 || sess_len > 32) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        ssl->session->length = sess_len;
        memset(ssl->session->id, 0, sizeof(ssl->session->id));
        memcpy(ssl->session->id, buf + 39, ssl->session->length);

        /*
            Check the cipherlist length
         */
        ciph_len = (buf[39 + sess_len] << 8) | (buf[40 + sess_len]);

        if (ciph_len < 2 || ciph_len > 256 || (ciph_len % 2) != 0) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        /*
            Check the compression algorithms length
         */
        comp_len = buf[41 + sess_len + ciph_len];

        if (comp_len < 1 || comp_len > 16) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        SSL_DEBUG_BUF(3, "client hello, random bytes", buf + 6, 32);
        SSL_DEBUG_BUF(3, "client hello, session id", buf + 38, sess_len);
        SSL_DEBUG_BUF(3, "client hello, cipherlist", buf + 41 + sess_len, ciph_len);
        SSL_DEBUG_BUF(3, "client hello, compression", buf + 42 + sess_len + ciph_len, comp_len);

        /*
         * Search for a matching cipher
         */
        for (i = 0; ssl->ciphers[i] != 0; i++) {
            for (j = 0, p = buf + 41 + sess_len; j < ciph_len;
                 j += 2, p += 2) {
                if (p[0] == 0 && p[1] == ssl->ciphers[i])
                    goto have_cipher;
            }
        }
    }
    SSL_DEBUG_MSG(1, ("got no ciphers in common"));
    return EST_ERR_SSL_NO_CIPHER_CHOSEN;

have_cipher:
    ssl->session->cipher = ssl->ciphers[i];
    ssl->in_left = 0;
    ssl->state++;
    SSL_DEBUG_MSG(2, ("<= parse client hello"));
    return 0;
}


static int ssl_write_server_hello(ssl_context * ssl)
{
    time_t t;
    int ret, i, n;
    uchar *buf, *p;

    SSL_DEBUG_MSG(2, ("=> write server hello"));

    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   5   protocol version
     *     6  .   9   UNIX time()
     *    10  .  37   random bytes
     */
    buf = ssl->out_msg;
    p = buf + 4;

    *p++ = (uchar)ssl->major_ver;
    *p++ = (uchar)ssl->minor_ver;

    SSL_DEBUG_MSG(3, ("server hello, chosen version: [%d:%d]", buf[4], buf[5]));

    t = time(NULL);
    *p++ = (uchar)(t >> 24);
    *p++ = (uchar)(t >> 16);
    *p++ = (uchar)(t >> 8);
    *p++ = (uchar)(t);

    SSL_DEBUG_MSG(3, ("server hello, current time: %lu", t));

    for (i = 28; i > 0; i--) {
        *p++ = (uchar)ssl->f_rng(ssl->p_rng);
    }
    memcpy(ssl->randbytes + 32, buf + 6, 32);

    SSL_DEBUG_BUF(3, "server hello, random bytes", buf + 6, 32);

    /*
     *    38  .  38   session id length
     *    39  . 38+n  session id
     *   39+n . 40+n  chosen cipher
     *   41+n . 41+n  chosen compression alg.
     */
    ssl->session->length = n = 32;
    *p++ = (uchar)ssl->session->length;

    if (ssl->s_get == NULL || ssl->s_get(ssl) != 0) {
        /*
         * Not found, create a new session id
         */
        ssl->resume = 0;
        ssl->state++;

        for (i = 0; i < n; i++) {
            ssl->session->id[i] = (uchar)ssl->f_rng(ssl->p_rng);
        }
    } else {
        /*
         * Found a matching session, resume it
         */
        ssl->resume = 1;
        ssl->state = SSL_SERVER_CHANGE_CIPHER_SPEC;
        ssl_derive_keys(ssl);
    }

    memcpy(p, ssl->session->id, ssl->session->length);
    p += ssl->session->length;

    SSL_DEBUG_MSG(3, ("server hello, session id len.: %d", n));
    SSL_DEBUG_BUF(3, "server hello, session id", buf + 39, n);
    SSL_DEBUG_MSG(3, ("%s session has been resumed", ssl->resume ? "a" : "no"));

    *p++ = (uchar)(ssl->session->cipher >> 8);
    *p++ = (uchar)(ssl->session->cipher);
    *p++ = SSL_COMPRESS_NULL;

    SSL_DEBUG_MSG(3, ("server hello, chosen cipher: %d", ssl->session->cipher));
    SSL_DEBUG_MSG(3, ("server hello, compress alg.: %d", 0));

    ssl->out_msglen = p - buf;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_SERVER_HELLO;

    ret = ssl_write_record(ssl);

    SSL_DEBUG_MSG(2, ("<= write server hello"));
    return ret;
}


static int ssl_write_certificate_request(ssl_context * ssl)
{
    int ret, n;
    uchar *buf, *p;
    x509_cert *crt;

    SSL_DEBUG_MSG(2, ("=> write certificate request"));
    ssl->state++;

    if (ssl->authmode == SSL_VERIFY_NO_CHECK) {
        SSL_DEBUG_MSG(2, ("<= skip write certificate request"));
        return 0;
    }
    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   4   cert type count
     *     5  .. n-1  cert types
     *     n  .. n+1  length of all DNs
     *    n+2 .. n+3  length of DN 1
     *    n+4 .. ...  Distinguished Name #1
     *    ... .. ...  length of DN 2, etc.
     */
    buf = ssl->out_msg;
    p = buf + 4;

    /*
     * At the moment, only RSA certificates are supported
     */
    *p++ = 1;
    *p++ = 1;

    p += 2;
    crt = ssl->ca_chain;

    while (crt != NULL && crt->next != NULL) {
        if (p - buf > 4096) {
            break;
        }
        n = crt->subject_raw.len;
        *p++ = (uchar)(n >> 8);
        *p++ = (uchar)(n);
        memcpy(p, crt->subject_raw.p, n);

        SSL_DEBUG_BUF(3, "requested DN", p, n);
        p += n;
        crt = crt->next;
    }
    ssl->out_msglen = n = p - buf;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_CERTIFICATE_REQUEST;
    ssl->out_msg[6] = (uchar)((n - 8) >> 8);
    ssl->out_msg[7] = (uchar)((n - 8));

    ret = ssl_write_record(ssl);
    SSL_DEBUG_MSG(2, ("<= write certificate request"));
    return ret;
}

static int ssl_write_server_key_exchange(ssl_context * ssl)
{
    int ret, n;
    uchar hash[36];
    md5_context md5;
    sha1_context sha1;

    SSL_DEBUG_MSG(2, ("=> write server key exchange"));

    if (ssl->session->cipher != TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA &&
        ssl->session->cipher != TLS_DHE_RSA_WITH_AES_256_CBC_SHA &&
        ssl->session->cipher != TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
        SSL_DEBUG_MSG(2, ("<= skip write server key exchange"));
        ssl->state++;
        return 0;
    }
#if !ME_EST_DHM
    SSL_DEBUG_MSG(1, ("support for dhm is not available"));
    return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#else
    /*
     * Ephemeral DH parameters:
     *
     * struct {
     *     opaque dh_p<1..2^16-1>;
     *     opaque dh_g<1..2^16-1>;
     *     opaque dh_Ys<1..2^16-1>;
     * } ServerDHParams;
     */
    if ((ret = dhm_make_params(&ssl->dhm_ctx, 256, ssl->out_msg + 4, &n, ssl->f_rng, ssl->p_rng)) != 0) {
        SSL_DEBUG_RET(1, "dhm_make_params", ret);
        return ret;
    }

    SSL_DEBUG_MPI(3, "DHM: X ", &ssl->dhm_ctx.X);
    SSL_DEBUG_MPI(3, "DHM: P ", &ssl->dhm_ctx.P);
    SSL_DEBUG_MPI(3, "DHM: G ", &ssl->dhm_ctx.G);
    SSL_DEBUG_MPI(3, "DHM: GX", &ssl->dhm_ctx.GX);

    /*
     * digitally-signed struct {
     *     opaque md5_hash[16];
     *     opaque sha_hash[20];
     * };
     *
     * md5_hash
     *     MD5(ClientHello.random + ServerHello.random
     *                            + ServerParams);
     * sha_hash
     *     SHA(ClientHello.random + ServerHello.random
     *                            + ServerParams);
     */
    md5_starts(&md5);
    md5_update(&md5, ssl->randbytes, 64);
    md5_update(&md5, ssl->out_msg + 4, n);
    md5_finish(&md5, hash);

    sha1_starts(&sha1);
    sha1_update(&sha1, ssl->randbytes, 64);
    sha1_update(&sha1, ssl->out_msg + 4, n);
    sha1_finish(&sha1, hash + 16);

    SSL_DEBUG_BUF(3, "parameters hash", hash, 36);

    ssl->out_msg[4 + n] = (uchar)(ssl->rsa_key->len >> 8);
    ssl->out_msg[5 + n] = (uchar)(ssl->rsa_key->len);

    ret = rsa_pkcs1_sign(ssl->rsa_key, RSA_PRIVATE, RSA_RAW, 36, hash, ssl->out_msg + 6 + n);
    if (ret != 0) {
        SSL_DEBUG_RET(1, "rsa_pkcs1_sign", ret);
        return ret;
    }

    SSL_DEBUG_BUF(3, "my RSA sig", ssl->out_msg + 6 + n, ssl->rsa_key->len);

    ssl->out_msglen = 6 + n + ssl->rsa_key->len;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_SERVER_KEY_EXCHANGE;

    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write server key exchange"));
    return 0;
#endif
}

static int ssl_write_server_hello_done(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> write server hello done"));

    ssl->out_msglen = 4;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_SERVER_HELLO_DONE;

    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write server hello done"));
    return 0;
}

static int ssl_parse_client_key_exchange(ssl_context * ssl)
{
    int ret, i, n;

    SSL_DEBUG_MSG(2, ("=> parse client key exchange"));

    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad client key exchange message"));
        return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE;
    }
    if (ssl->in_msg[0] != SSL_HS_CLIENT_KEY_EXCHANGE) {
        SSL_DEBUG_MSG(1, ("bad client key exchange message"));
        return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE;
    }
    if (ssl->session->cipher == TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA ||
        ssl->session->cipher == TLS_DHE_RSA_WITH_AES_256_CBC_SHA ||
        ssl->session->cipher == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
#if !ME_EST_DHM
        SSL_DEBUG_MSG(1, ("support for dhm is not available"));
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#else
        /*
           Receive G^Y mod P, premaster = (G^Y)^X mod P
         */
        n = (ssl->in_msg[4] << 8) | ssl->in_msg[5];

        if (n < 1 || n > ssl->dhm_ctx.len || n + 6 != ssl->in_hslen) {
            SSL_DEBUG_MSG(1, ("bad client key exchange message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE;
        }
        if ((ret = dhm_read_public(&ssl->dhm_ctx, ssl->in_msg + 6, n)) != 0) {
            SSL_DEBUG_RET(1, "dhm_read_public", ret);
            return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE | ret;
        }
        SSL_DEBUG_MPI(3, "DHM: GY", &ssl->dhm_ctx.GY);
        ssl->pmslen = ssl->dhm_ctx.len;

        if ((ret = dhm_calc_secret(&ssl->dhm_ctx, ssl->premaster, &ssl->pmslen)) != 0) {
            SSL_DEBUG_RET(1, "dhm_calc_secret", ret);
            return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE | ret;
        }

        SSL_DEBUG_MPI(3, "DHM: K ", &ssl->dhm_ctx.K);
#endif
    } else {
        /*
           Decrypt the premaster using own private RSA key
         */
        i = 4;
        n = ssl->rsa_key->len;
        ssl->pmslen = 48;

        if (ssl->minor_ver != SSL_MINOR_VERSION_0) {
            i += 2;
            if (ssl->in_msg[4] != ((n >> 8) & 0xFF) ||
                ssl->in_msg[5] != ((n) & 0xFF)) {
                SSL_DEBUG_MSG(1, ("bad client key exchange message"));
                return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE;
            }
        }
        if (ssl->in_hslen != i + n) {
            SSL_DEBUG_MSG(1, ("bad client key exchange message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE;
        }
        ret = rsa_pkcs1_decrypt(ssl->rsa_key, RSA_PRIVATE, &ssl->pmslen, ssl->in_msg + i, ssl->premaster,
                sizeof(ssl->premaster));

        if (ret != 0 || ssl->pmslen != 48 || ssl->premaster[0] != ssl->max_major_ver ||
                ssl->premaster[1] != ssl->max_minor_ver) {
            SSL_DEBUG_MSG(1, ("bad client key exchange message"));

            /*
               Protection against Bleichenbacher's attack: invalid PKCS#1 v1.5 padding must not cause
               the connection to end immediately; instead, send a bad_record_mac later in the handshake.
             */
            ssl->pmslen = 48;
            for (i = 0; i < ssl->pmslen; i++) {
                ssl->premaster[i] = (uchar)ssl->f_rng(ssl->p_rng);
            }
        }
    }
    ssl_derive_keys(ssl);

    if (ssl->s_set != NULL) {
        ssl->s_set(ssl);
    }
    ssl->state++;
    SSL_DEBUG_MSG(2, ("<= parse client key exchange"));
    return 0;
}


static int ssl_parse_certificate_verify(ssl_context * ssl)
{
    int n1, n2, ret;
    uchar hash[36];

    SSL_DEBUG_MSG(2, ("=> parse certificate verify"));

    if (ssl->peer_cert == NULL) {
        SSL_DEBUG_MSG(2, ("<= skip parse certificate verify"));
        ssl->state++;
        return 0;
    }
    ssl_calc_verify(ssl, hash);

    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    ssl->state++;

    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad certificate verify message"));
        return EST_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY;
    }
    if (ssl->in_msg[0] != SSL_HS_CERTIFICATE_VERIFY) {
        SSL_DEBUG_MSG(1, ("bad certificate verify message"));
        return EST_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY;
    }
    n1 = ssl->peer_cert->rsa.len;
    n2 = (ssl->in_msg[4] << 8) | ssl->in_msg[5];

    if (n1 + 6 != ssl->in_hslen || n1 != n2) {
        SSL_DEBUG_MSG(1, ("bad certificate verify message"));
        return EST_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY;
    }
    ret = rsa_pkcs1_verify(&ssl->peer_cert->rsa, RSA_PUBLIC, RSA_RAW, 36, hash, ssl->in_msg + 6);
    if (ret != 0) {
        SSL_DEBUG_RET(1, "rsa_pkcs1_verify", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= parse certificate verify"));
    return 0;
}


/*
    SSL handshake -- server side
 */
int ssl_handshake_server(ssl_context * ssl)
{
    int ret = 0;

    SSL_DEBUG_MSG(2, ("=> handshake server"));

    while (ssl->state != SSL_HANDSHAKE_OVER) {
        SSL_DEBUG_MSG(2, ("server state: %d", ssl->state));

        if ((ret = ssl_flush_output(ssl)) != 0) {
            break;
        }
        switch (ssl->state) {
        case SSL_HELLO_REQUEST:
            ssl->state = SSL_CLIENT_HELLO;
            break;

        /*
            <==   ClientHello
         */
        case SSL_CLIENT_HELLO:
            ret = ssl_parse_client_hello(ssl);
            break;

        /*
            ==>   ServerHello
                  Certificate
                ( ServerKeyExchange  )
                ( CertificateRequest )
                  ServerHelloDone
         */
        case SSL_SERVER_HELLO:
            ret = ssl_write_server_hello(ssl);
            break;

        case SSL_SERVER_CERTIFICATE:
            ret = ssl_write_certificate(ssl);
            break;

        case SSL_SERVER_KEY_EXCHANGE:
            ret = ssl_write_server_key_exchange(ssl);
            break;

        case SSL_CERTIFICATE_REQUEST:
            ret = ssl_write_certificate_request(ssl);
            break;

        case SSL_SERVER_HELLO_DONE:
            ret = ssl_write_server_hello_done(ssl);
            break;

        /*
            <== ( Certificate/Alert  )
                  ClientKeyExchange
                ( CertificateVerify  )
                  ChangeCipherSpec
                  Finished
         */
        case SSL_CLIENT_CERTIFICATE:
            ret = ssl_parse_certificate(ssl);
            break;

        case SSL_CLIENT_KEY_EXCHANGE:
            ret = ssl_parse_client_key_exchange(ssl);
            break;

        case SSL_CERTIFICATE_VERIFY:
            ret = ssl_parse_certificate_verify(ssl);
            break;

        case SSL_CLIENT_CHANGE_CIPHER_SPEC:
            ret = ssl_parse_change_cipher_spec(ssl);
            break;

        case SSL_CLIENT_FINISHED:
            ret = ssl_parse_finished(ssl);
            break;

        /*
            ==>   ChangeCipherSpec
                  Finished
         */
        case SSL_SERVER_CHANGE_CIPHER_SPEC:
            ret = ssl_write_change_cipher_spec(ssl);
            break;

        case SSL_SERVER_FINISHED:
            ret = ssl_write_finished(ssl);
            break;

        case SSL_FLUSH_BUFFERS:
            SSL_DEBUG_MSG(2, ("handshake: done"));
            ssl->state = SSL_HANDSHAKE_OVER;
            break;

        default:
            SSL_DEBUG_MSG(1, ("invalid state %d", ssl->state));
            return EST_ERR_SSL_BAD_INPUT_DATA;
        }
        if (ret != 0) {
            break;
        }
    }
    SSL_DEBUG_MSG(2, ("<= handshake server"));
    return ret;
}

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/ssl_tls.c ************/


/*
    ssl_tls.c -- SSLv3/TLSv1 shared functions

    The SSL 3.0 specification was drafted by Netscape in 1996, and became an IETF standard in 1999.
    http://wp.netscape.com/eng/ssl3/
    http://www.ietf.org/rfc/rfc2246.txt
    http://www.ietf.org/rfc/rfc4346.txt

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_SSL

int ssl_default_ciphers[] = {
#if ME_EST_DHM
#if ME_EST_AES
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
#endif
#if ME_EST_CAMELLIA
    TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
#endif
#if ME_EST_DES
    TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
#endif
#endif
#if ME_EST_AES
    TLS_RSA_WITH_AES_128_CBC_SHA,
    TLS_RSA_WITH_AES_256_CBC_SHA,
#endif
#if ME_EST_CAMELLIA
    TLS_RSA_WITH_CAMELLIA_128_CBC_SHA,
    TLS_RSA_WITH_CAMELLIA_256_CBC_SHA,
#endif
#if ME_EST_DES
    TLS_RSA_WITH_3DES_EDE_CBC_SHA,
#endif
#if ME_EST_RC4
    TLS_RSA_WITH_RC4_128_SHA,
    TLS_RSA_WITH_RC4_128_MD5,
#endif
    0
};


/* 
    Supported ciphers. Ordered in preference order
    See: http://www.iana.org/assignments/tls-parameters/tls-parameters.xml
 */
static EstCipher cipherList[] = {
#if ME_EST_RSA && ME_EST_AES
    { "TLS_RSA_WITH_AES_128_CBC_SHA",           TLS_RSA_WITH_AES_128_CBC_SHA            /* 0x2F */ },
    { "TLS_RSA_WITH_AES_256_CBC_SHA",           TLS_RSA_WITH_AES_256_CBC_SHA            /* 0x35 */ },
#endif
#if ME_EST_RSA && ME_EST_RC4
    { "TLS_RSA_WITH_RC4_128_SHA",               TLS_RSA_WITH_RC4_128_SHA                /* 0x05 */ },
#endif
#if ME_EST_RSA && ME_EST_DES
    { "TLS_RSA_WITH_3DES_EDE_CBC_SHA",          TLS_RSA_WITH_3DES_EDE_CBC_SHA           /* 0x0A */ },
#endif
#if ME_EST_RSA && ME_EST_RC4
    { "TLS_RSA_WITH_RC4_128_MD5",               TLS_RSA_WITH_RC4_128_MD5                /* 0x04 */ },
#endif
#if ME_EST_RSA && ME_EST_AES && ME_EST_DHM
    { "TLS_DHE_RSA_WITH_AES_256_CBC_SHA",       TLS_DHE_RSA_WITH_AES_256_CBC_SHA        /* 0x39 */ },
#endif
#if ME_EST_RSA && ME_EST_DES && ME_EST_DHM
    { "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA",      TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA       /* 0x16 */ },
#endif
#if ME_EST_RSA && ME_EST_CAMELLIA
    { "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA",      TLS_RSA_WITH_CAMELLIA_128_CBC_SHA       /* 0x41 */ },
    { "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA",      TLS_RSA_WITH_CAMELLIA_256_CBC_SHA       /* 0x88 */ },
    #if ME_EST_DHM
    { "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA",  TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA   /* 0x84 */ },
    #endif
#endif
    { 0, 0 },
};


char *ssl_get_cipher(ssl_context *ssl)
{
    EstCipher   *cp;

    for (cp = cipherList; cp->name; cp++) {
        if (cp->code == ssl->session->cipher) {
            return cp->name;
        }
    }
    return 0;
}


static char *stok(char *str, char *delim, char **last)
{
    char    *start, *end;
    ssize   i;

    start = (str || *last == 0) ? str : *last;

    if (start == 0) {
        if (last) {
            *last = 0;
        }
        return 0;
    }
    i = strspn(start, delim);
    start += i;
    if (*start == '\0') {
        if (last) {
            *last = 0;
        }
        return 0;
    }
    end = strpbrk(start, delim);
    if (end) {
        *end++ = '\0';
        i = strspn(end, delim);
        end += i;
    }
    if (last) {
        *last = end;
    }
    return start;
}


int *ssl_create_ciphers(cchar *cipherSuite)
{
    EstCipher   *cp;
    char        *suite, *cipher, *next;
    int         nciphers, i, *ciphers;

    nciphers = sizeof(cipherList) / sizeof(EstCipher);
    ciphers = malloc((nciphers + 1) * sizeof(int));

    if (!cipherSuite || cipherSuite[0] == '\0') {
        memcpy(ciphers, ssl_default_ciphers, nciphers * sizeof(int));
        ciphers[nciphers] = 0;
        return ciphers;
    }
    suite = strdup(cipherSuite);
    i = 0;
    while ((cipher = stok(suite, ":, \t", &next)) != 0) {
        for (cp = cipherList; cp->name; cp++) {
            if (strcmp(cp->name, cipher) == 0) {
                break;
            }
        }
        if (cp) {
            ciphers[i++] = cp->code;
        } else {
            // SSL_DEBUG_MSG(0, ("Requested cipher %s is not supported", cipher));
        }
        suite = 0;
    }
    if (i == 0) {
        for (i = 0; i < nciphers; i++) {
            ciphers[i] = ssl_default_ciphers[i];
        }
        ciphers[i] = 0;
    }
    return ciphers;
}


/*
   Key material generation
 */
static int tls1_prf(uchar *secret, int slen, char *label, uchar *random, int rlen, uchar *dstbuf, int dlen)
{
    int nb, hs;
    int i, j, k;
    uchar *S1, *S2;
    uchar tmp[128];
    uchar h_i[20];

    if (sizeof(tmp) < 20 + strlen(label) + rlen) {
        return EST_ERR_SSL_BAD_INPUT_DATA;
    }
    hs = (slen + 1) / 2;
    S1 = secret;
    S2 = secret + slen - hs;

    nb = strlen(label);
    memcpy(tmp + 20, label, nb);
    memcpy(tmp + 20 + nb, random, rlen);
    nb += rlen;

    /*
       First compute P_md5(secret,label+random)[0..dlen]
     */
    md5_hmac(S1, hs, tmp + 20, nb, 4 + tmp);

    for (i = 0; i < dlen; i += 16) {
        md5_hmac(S1, hs, 4 + tmp, 16 + nb, h_i);
        md5_hmac(S1, hs, 4 + tmp, 16, 4 + tmp);
        k = (i + 16 > dlen) ? dlen % 16 : 16;
        for (j = 0; j < k; j++) {
            dstbuf[i + j] = h_i[j];
        }
    }

    /*
        XOR out with P_sha1(secret,label+random)[0..dlen]
     */
    sha1_hmac(S2, hs, tmp + 20, nb, tmp);

    for (i = 0; i < dlen; i += 20) {
        sha1_hmac(S2, hs, tmp, 20 + nb, h_i);
        sha1_hmac(S2, hs, tmp, 20, tmp);
        k = (i + 20 > dlen) ? dlen % 20 : 20;
        for (j = 0; j < k; j++) {
            dstbuf[i + j] = (uchar)(dstbuf[i + j] ^ h_i[j]);
        }
    }
    memset(tmp, 0, sizeof(tmp));
    memset(h_i, 0, sizeof(h_i));
    return 0;
}


int ssl_derive_keys(ssl_context * ssl)
{
    int i;
    md5_context md5;
    sha1_context sha1;
    uchar tmp[64];
    uchar padding[16];
    uchar sha1sum[20];
    uchar keyblk[256];
    uchar *key1;
    uchar *key2;

    SSL_DEBUG_MSG(2, ("=> derive keys"));

    /*
       SSLv3:
         master =
           MD5(premaster + SHA1( 'A'   + premaster + randbytes)) +
           MD5(premaster + SHA1( 'BB'  + premaster + randbytes)) +
           MD5(premaster + SHA1( 'CCC' + premaster + randbytes))
      
       TLSv1:
         master = PRF( premaster, "master secret", randbytes )[0..47]
     */
    if (ssl->resume == 0) {
        int len = ssl->pmslen;

        SSL_DEBUG_BUF(3, "premaster secret", ssl->premaster, len);

        if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
            for (i = 0; i < 3; i++) {
                memset(padding, 'A' + i, 1 + i);

                sha1_starts(&sha1);
                sha1_update(&sha1, padding, 1 + i);
                sha1_update(&sha1, ssl->premaster, len);
                sha1_update(&sha1, ssl->randbytes, 64);
                sha1_finish(&sha1, sha1sum);

                md5_starts(&md5);
                md5_update(&md5, ssl->premaster, len);
                md5_update(&md5, sha1sum, 20);
                md5_finish(&md5, ssl->session->master + i * 16);
            }
        } else {
            tls1_prf(ssl->premaster, len, "master secret", ssl->randbytes, 64, ssl->session->master, 48);
        }
        memset(ssl->premaster, 0, sizeof(ssl->premaster));
    } else {
        SSL_DEBUG_MSG(3, ("no premaster (session resumed)"));
    }

    /*
       Swap the client and server random values.
     */
    memcpy(tmp, ssl->randbytes, 64);
    memcpy(ssl->randbytes, tmp + 32, 32);
    memcpy(ssl->randbytes + 32, tmp, 32);
    memset(tmp, 0, sizeof(tmp));

    /*
        SSLv3:
          key block =
            MD5( master + SHA1( 'A'    + master + randbytes ) ) +
            MD5( master + SHA1( 'BB'   + master + randbytes ) ) +
            MD5( master + SHA1( 'CCC'  + master + randbytes ) ) +
            MD5( master + SHA1( 'DDDD' + master + randbytes ) ) +
            ...
        TLSv1:
          key block = PRF( master, "key expansion", randbytes )
     */
    if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
        for (i = 0; i < 16; i++) {
            memset(padding, 'A' + i, 1 + i);

            sha1_starts(&sha1);
            sha1_update(&sha1, padding, 1 + i);
            sha1_update(&sha1, ssl->session->master, 48);
            sha1_update(&sha1, ssl->randbytes, 64);
            sha1_finish(&sha1, sha1sum);

            md5_starts(&md5);
            md5_update(&md5, ssl->session->master, 48);
            md5_update(&md5, sha1sum, 20);
            md5_finish(&md5, keyblk + i * 16);
        }
        memset(&md5, 0, sizeof(md5));
        memset(&sha1, 0, sizeof(sha1));
        memset(padding, 0, sizeof(padding));
        memset(sha1sum, 0, sizeof(sha1sum));
    } else {
        tls1_prf(ssl->session->master, 48, "key expansion", ssl->randbytes, 64, keyblk, 256);
    }
    SSL_DEBUG_MSG(3, ("cipher = %s", ssl_get_cipher(ssl)));
    SSL_DEBUG_BUF(3, "master secret", ssl->session->master, 48);
    SSL_DEBUG_BUF(4, "random bytes", ssl->randbytes, 64);
    SSL_DEBUG_BUF(4, "key block", keyblk, 256);

    memset(ssl->randbytes, 0, sizeof(ssl->randbytes));

    /*
     * Determine the appropriate key, IV and MAC length.
     */
    switch (ssl->session->cipher) {
#if ME_EST_RC4
    case TLS_RSA_WITH_RC4_128_MD5:
        ssl->keylen = 16;
        ssl->minlen = 16;
        ssl->ivlen = 0;
        ssl->maclen = 16;
        break;

    case TLS_RSA_WITH_RC4_128_SHA:
        ssl->keylen = 16;
        ssl->minlen = 20;
        ssl->ivlen = 0;
        ssl->maclen = 20;
        break;
#endif

#if ME_EST_DES
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        ssl->keylen = 24;
        ssl->minlen = 24;
        ssl->ivlen = 8;
        ssl->maclen = 20;
        break;
#endif

#if ME_EST_AES
    case TLS_RSA_WITH_AES_128_CBC_SHA:
        ssl->keylen = 16;
        ssl->minlen = 32;
        ssl->ivlen = 16;
        ssl->maclen = 20;
        break;

    case TLS_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        ssl->keylen = 32;
        ssl->minlen = 32;
        ssl->ivlen = 16;
        ssl->maclen = 20;
        break;
#endif

#if ME_EST_CAMELLIA
    case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA:
        ssl->keylen = 16;
        ssl->minlen = 32;
        ssl->ivlen = 16;
        ssl->maclen = 20;
        break;

    case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA:
        ssl->keylen = 32;
        ssl->minlen = 32;
        ssl->ivlen = 16;
        ssl->maclen = 20;
        break;
#endif

    default:
        SSL_DEBUG_MSG(1, ("cipher %s is not available", ssl_get_cipher(ssl)));
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
    }

    SSL_DEBUG_MSG(3, ("keylen: %d, minlen: %d, ivlen: %d, maclen: %d", ssl->keylen, ssl->minlen, ssl->ivlen, ssl->maclen));

    /*
        Finally setup the cipher contexts, IVs and MAC secrets.
     */
    if (ssl->endpoint == SSL_IS_CLIENT) {
        key1 = keyblk + ssl->maclen * 2;
        key2 = keyblk + ssl->maclen * 2 + ssl->keylen;

        memcpy(ssl->mac_enc, keyblk, ssl->maclen);
        memcpy(ssl->mac_dec, keyblk + ssl->maclen, ssl->maclen);

        memcpy(ssl->iv_enc, key2 + ssl->keylen, ssl->ivlen);
        memcpy(ssl->iv_dec, key2 + ssl->keylen + ssl->ivlen, ssl->ivlen);

    } else {
        key1 = keyblk + ssl->maclen * 2 + ssl->keylen;
        key2 = keyblk + ssl->maclen * 2;

        memcpy(ssl->mac_dec, keyblk, ssl->maclen);
        memcpy(ssl->mac_enc, keyblk + ssl->maclen, ssl->maclen);

        memcpy(ssl->iv_dec, key1 + ssl->keylen, ssl->ivlen);
        memcpy(ssl->iv_enc, key1 + ssl->keylen + ssl->ivlen, ssl->ivlen);
    }

    switch (ssl->session->cipher) {
#if ME_EST_RC4
    case TLS_RSA_WITH_RC4_128_MD5:
    case TLS_RSA_WITH_RC4_128_SHA:
        arc4_setup((arc4_context *) ssl->ctx_enc, key1, ssl->keylen);
        arc4_setup((arc4_context *) ssl->ctx_dec, key2, ssl->keylen);
        break;
#endif

#if ME_EST_DES
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        des3_set3key_enc((des3_context *) ssl->ctx_enc, key1);
        des3_set3key_dec((des3_context *) ssl->ctx_dec, key2);
        break;
#endif

#if ME_EST_AES
    case TLS_RSA_WITH_AES_128_CBC_SHA:
        aes_setkey_enc((aes_context *) ssl->ctx_enc, key1, 128);
        aes_setkey_dec((aes_context *) ssl->ctx_dec, key2, 128);
        break;

    case TLS_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        aes_setkey_enc((aes_context *) ssl->ctx_enc, key1, 256);
        aes_setkey_dec((aes_context *) ssl->ctx_dec, key2, 256);
        break;
#endif

#if ME_EST_CAMELLIA
    case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA:
        camellia_setkey_enc((camellia_context *) ssl->ctx_enc, key1, 128);
        camellia_setkey_dec((camellia_context *) ssl->ctx_dec, key2, 128);
        break;

    case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA:
        camellia_setkey_enc((camellia_context *) ssl->ctx_enc, key1, 256);
        camellia_setkey_dec((camellia_context *) ssl->ctx_dec, key2, 256);
        break;
#endif

    default:
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
    }
    memset(keyblk, 0, sizeof(keyblk));
    SSL_DEBUG_MSG(2, ("<= derive keys"));
    return 0;
}

void ssl_calc_verify(ssl_context * ssl, uchar hash[36])
{
    md5_context md5;
    sha1_context sha1;
    uchar pad_1[48];
    uchar pad_2[48];

    SSL_DEBUG_MSG(2, ("=> calc verify"));

    memcpy(&md5, &ssl->fin_md5, sizeof(md5_context));
    memcpy(&sha1, &ssl->fin_sha1, sizeof(sha1_context));

    if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
        memset(pad_1, 0x36, 48);
        memset(pad_2, 0x5C, 48);

        md5_update(&md5, ssl->session->master, 48);
        md5_update(&md5, pad_1, 48);
        md5_finish(&md5, hash);

        md5_starts(&md5);
        md5_update(&md5, ssl->session->master, 48);
        md5_update(&md5, pad_2, 48);
        md5_update(&md5, hash, 16);
        md5_finish(&md5, hash);

        sha1_update(&sha1, ssl->session->master, 48);
        sha1_update(&sha1, pad_1, 40);
        sha1_finish(&sha1, hash + 16);

        sha1_starts(&sha1);
        sha1_update(&sha1, ssl->session->master, 48);
        sha1_update(&sha1, pad_2, 40);
        sha1_update(&sha1, hash + 16, 20);
        sha1_finish(&sha1, hash + 16);

    } else {
        /* TLSv1 */
        md5_finish(&md5, hash);
        sha1_finish(&sha1, hash + 16);
    }
    SSL_DEBUG_BUF(3, "calculated verify result", hash, 36);
    SSL_DEBUG_MSG(2, ("<= calc verify"));
    return;
}


/*
    SSLv3.0 MAC functions
 */
static void ssl_mac_md5(uchar *secret, uchar *buf, int len, uchar *ctr, int type)
{
    uchar header[11];
    uchar padding[48];
    md5_context md5;

    memcpy(header, ctr, 8);
    header[8] = (uchar)type;
    header[9] = (uchar)(len >> 8);
    header[10] = (uchar)(len);

    memset(padding, 0x36, 48);
    md5_starts(&md5);
    md5_update(&md5, secret, 16);
    md5_update(&md5, padding, 48);
    md5_update(&md5, header, 11);
    md5_update(&md5, buf, len);
    md5_finish(&md5, buf + len);

    memset(padding, 0x5C, 48);
    md5_starts(&md5);
    md5_update(&md5, secret, 16);
    md5_update(&md5, padding, 48);
    md5_update(&md5, buf + len, 16);
    md5_finish(&md5, buf + len);
}


static void ssl_mac_sha1(uchar *secret, uchar *buf, int len, uchar *ctr, int type)
{
    uchar header[11];
    uchar padding[40];
    sha1_context sha1;

    memcpy(header, ctr, 8);
    header[8] = (uchar)type;
    header[9] = (uchar)(len >> 8);
    header[10] = (uchar)(len);

    memset(padding, 0x36, 40);
    sha1_starts(&sha1);
    sha1_update(&sha1, secret, 20);
    sha1_update(&sha1, padding, 40);
    sha1_update(&sha1, header, 11);
    sha1_update(&sha1, buf, len);
    sha1_finish(&sha1, buf + len);

    memset(padding, 0x5C, 40);
    sha1_starts(&sha1);
    sha1_update(&sha1, secret, 20);
    sha1_update(&sha1, padding, 40);
    sha1_update(&sha1, buf + len, 20);
    sha1_finish(&sha1, buf + len);
}

/*
    Encryption/decryption functions
 */
static int ssl_encrypt_buf(ssl_context * ssl)
{
    int i, padlen;

    SSL_DEBUG_MSG(2, ("=> encrypt buf"));

    /*
        Add MAC then encrypt
     */
    if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
        if (ssl->maclen == 16) {
            ssl_mac_md5(ssl->mac_enc, ssl->out_msg, ssl->out_msglen, ssl->out_ctr, ssl->out_msgtype);
        }
        if (ssl->maclen == 20) {
            ssl_mac_sha1(ssl->mac_enc, ssl->out_msg, ssl->out_msglen, ssl->out_ctr, ssl->out_msgtype);
        }

    } else {
        if (ssl->maclen == 16) {
            md5_hmac(ssl->mac_enc, 16, ssl->out_ctr, ssl->out_msglen + 13, ssl->out_msg + ssl->out_msglen);
        }
        if (ssl->maclen == 20) {
            sha1_hmac(ssl->mac_enc, 20, ssl->out_ctr, ssl->out_msglen + 13, ssl->out_msg + ssl->out_msglen);
        }
    }
    SSL_DEBUG_BUF(4, "computed mac", ssl->out_msg + ssl->out_msglen, ssl->maclen);

    ssl->out_msglen += ssl->maclen;

    for (i = 7; i >= 0; i--) {
        if (++ssl->out_ctr[i] != 0) {
            break;
        }
    }
    if (ssl->ivlen == 0) {
#if ME_EST_RC4
        padlen = 0;
        SSL_DEBUG_MSG(3, ("before encrypt: msglen = %d, " "including %d bytes of padding", ssl->out_msglen, 0));
        SSL_DEBUG_BUF(4, "before encrypt: output payload", ssl->out_msg, ssl->out_msglen);
        arc4_crypt((arc4_context *) ssl->ctx_enc, ssl->out_msg, ssl->out_msglen);
#else
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#endif
    } else {
        padlen = ssl->ivlen - (ssl->out_msglen + 1) % ssl->ivlen;
        if (padlen == ssl->ivlen) {
            padlen = 0;
        }
        for (i = 0; i <= padlen; i++) {
            ssl->out_msg[ssl->out_msglen + i] = (uchar)padlen;
        }
        ssl->out_msglen += padlen + 1;

        SSL_DEBUG_MSG(3, ("before encrypt: msglen = %d, " "including %d bytes of padding", ssl->out_msglen, padlen + 1));
        SSL_DEBUG_BUF(4, "before encrypt: output payload", ssl->out_msg, ssl->out_msglen);

        switch (ssl->ivlen) {
        case 8:
#if ME_EST_DES
            des3_crypt_cbc((des3_context *) ssl->ctx_enc, DES_ENCRYPT, ssl->out_msglen,
                ssl->iv_enc, ssl->out_msg, ssl->out_msg);
            break;
#endif

        case 16:
#if ME_EST_AES
            if (ssl->session->cipher == TLS_RSA_WITH_AES_128_CBC_SHA ||
                ssl->session->cipher == TLS_RSA_WITH_AES_256_CBC_SHA ||
                ssl->session->cipher == TLS_DHE_RSA_WITH_AES_256_CBC_SHA) {
                    aes_crypt_cbc((aes_context *) ssl->ctx_enc, AES_ENCRYPT, ssl->out_msglen, ssl->iv_enc, ssl->out_msg,
                          ssl->out_msg);
                break;
            }
#endif

#if ME_EST_CAMELLIA
            if (ssl->session->cipher == TLS_RSA_WITH_CAMELLIA_128_CBC_SHA ||
                ssl->session->cipher == TLS_RSA_WITH_CAMELLIA_256_CBC_SHA ||
                ssl->session->cipher == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
                    camellia_crypt_cbc((camellia_context *) ssl->ctx_enc, CAMELLIA_ENCRYPT,
                    ssl->out_msglen, ssl->iv_enc, ssl->out_msg, ssl->out_msg);
                break;
            }
#endif
        default:
            return EST_ERR_SSL_FEATURE_UNAVAILABLE;
        }
    }
    SSL_DEBUG_MSG(2, ("<= encrypt buf"));
    return 0;
}


static int ssl_decrypt_buf(ssl_context * ssl)
{
    int i, padlen;
    uchar tmp[20];

    SSL_DEBUG_MSG(2, ("=> decrypt buf"));

    if (ssl->in_msglen < ssl->minlen) {
        SSL_DEBUG_MSG(1, ("in_msglen (%d) < minlen (%d)", ssl->in_msglen, ssl->minlen));
        return EST_ERR_SSL_INVALID_MAC;
    }
    if (ssl->ivlen == 0) {
#if ME_EST_RC4
        padlen = 0;
        arc4_crypt((arc4_context *) ssl->ctx_dec, ssl->in_msg, ssl->in_msglen);
#else
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#endif
    } else {
        /*
            Decrypt and check the padding
         */
        if (ssl->in_msglen % ssl->ivlen != 0) {
            SSL_DEBUG_MSG(1, ("msglen (%d) %% ivlen (%d) != 0", ssl->in_msglen, ssl->ivlen));
            return EST_ERR_SSL_INVALID_MAC;
        }
        switch (ssl->ivlen) {
#if ME_EST_DES
        case 8:
            des3_crypt_cbc((des3_context *) ssl->ctx_dec, DES_DECRYPT, ssl->in_msglen,
                ssl->iv_dec, ssl->in_msg, ssl->in_msg);
            break;
#endif

        case 16:
#if ME_EST_AES
            if (ssl->session->cipher == TLS_RSA_WITH_AES_128_CBC_SHA ||
                ssl->session->cipher == TLS_RSA_WITH_AES_256_CBC_SHA ||
                ssl->session->cipher == TLS_DHE_RSA_WITH_AES_256_CBC_SHA) {
                aes_crypt_cbc((aes_context *) ssl->ctx_dec, AES_DECRYPT, ssl->in_msglen, ssl->iv_dec, ssl->in_msg,
                    ssl->in_msg);
                break;
            }
#endif

#if ME_EST_CAMELLIA
            if (ssl->session->cipher == TLS_RSA_WITH_CAMELLIA_128_CBC_SHA ||
                ssl->session->cipher == TLS_RSA_WITH_CAMELLIA_256_CBC_SHA ||
                ssl->session->cipher == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
                camellia_crypt_cbc((camellia_context *) ssl->ctx_dec, CAMELLIA_DECRYPT,
                    ssl->in_msglen, ssl->iv_dec, ssl->in_msg, ssl->in_msg);
                break;
            }
#endif
        default:
            return EST_ERR_SSL_FEATURE_UNAVAILABLE;
        }
        padlen = 1 + ssl->in_msg[ssl->in_msglen - 1];

        if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
            if (padlen > ssl->ivlen) {
                SSL_DEBUG_MSG(1, ("bad padding length: is %d, " "should be no more than %d", padlen, ssl->ivlen));
                padlen = 0;
            }
        } else {
            /*
                TLSv1: always check the padding
             */
            for (i = 1; i <= padlen; i++) {
                if (ssl->in_msg[ssl->in_msglen - i] != padlen - 1) {
                    SSL_DEBUG_MSG(1, ("bad padding byte: should be %02x, but is %02x",
                       padlen - 1, ssl-> in_msg[ssl->in_msglen - i]));
                    padlen = 0;
                }
            }
        }
    }
    SSL_DEBUG_BUF(4, "raw buffer after decryption", ssl->in_msg, ssl->in_msglen);

    /*
        Always compute the MAC (RFC4346, CBCTIME)
     */
    ssl->in_msglen -= (ssl->maclen + padlen);

    ssl->in_hdr[3] = (uchar)(ssl->in_msglen >> 8);
    ssl->in_hdr[4] = (uchar)(ssl->in_msglen);

    memcpy(tmp, ssl->in_msg + ssl->in_msglen, 20);

    if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
        if (ssl->maclen == 16) {
            ssl_mac_md5(ssl->mac_dec, ssl->in_msg, ssl->in_msglen, ssl->in_ctr, ssl->in_msgtype);
        } else {
            ssl_mac_sha1(ssl->mac_dec, ssl->in_msg, ssl->in_msglen, ssl->in_ctr, ssl->in_msgtype);
        }
    } else {
        if (ssl->maclen == 16) {
            md5_hmac(ssl->mac_dec, 16, ssl->in_ctr, ssl->in_msglen + 13, ssl->in_msg + ssl->in_msglen);
        } else {
            sha1_hmac(ssl->mac_dec, 20, ssl->in_ctr, ssl->in_msglen + 13, ssl->in_msg + ssl->in_msglen);
        }
    }
    SSL_DEBUG_BUF(4, "message  mac", tmp, ssl->maclen);
    SSL_DEBUG_BUF(4, "computed mac", ssl->in_msg + ssl->in_msglen, ssl->maclen);

    if (memcmp(tmp, ssl->in_msg + ssl->in_msglen, ssl->maclen) != 0) {
        SSL_DEBUG_MSG(1, ("message mac does not match"));
        return EST_ERR_SSL_INVALID_MAC;
    }

    /*
        Finally check the padding length; bad padding will produce the same error as an invalid MAC.
     */
    if (ssl->ivlen != 0 && padlen == 0) {
        return EST_ERR_SSL_INVALID_MAC;
    }

    if (ssl->in_msglen == 0) {
        ssl->nb_zero++;
        /*
            Three or more empty messages may be a DoS attack (excessive CPU consumption).
         */
        if (ssl->nb_zero > 3) {
            SSL_DEBUG_MSG(1, ("received four consecutive empty " "messages, possible DoS attack"));
            return EST_ERR_SSL_INVALID_MAC;
        }
    } else
        ssl->nb_zero = 0;

    for (i = 7; i >= 0; i--) {
        if (++ssl->in_ctr[i] != 0) {
            break;
        }
    }
    SSL_DEBUG_MSG(2, ("<= decrypt buf"));
    return 0;
}


/*
    Fill the input message buffer
 */
int ssl_fetch_input(ssl_context * ssl, int nb_want)
{
    int ret, len;

    SSL_DEBUG_MSG(2, ("=> fetch input"));

    while (ssl->in_left < nb_want) {
        len = nb_want - ssl->in_left;
        ret = ssl->f_recv(ssl->p_recv, ssl->in_hdr + ssl->in_left, len);

        SSL_DEBUG_MSG(4, ("in_left: %d, nb_want: %d", ssl->in_left, nb_want));
        SSL_DEBUG_RET(4, "ssl->f_recv", ret);

        if (ret < 0) {
            return ret;
        }
        ssl->in_left += ret;
    }
    SSL_DEBUG_MSG(4, ("<= fetch input"));
    return 0;
}


/*
    Flush any data not yet written
 */
int ssl_flush_output(ssl_context * ssl)
{
    int ret;
    uchar *buf;

    SSL_DEBUG_MSG(2, ("=> flush output"));

    while (ssl->out_left > 0) {
        SSL_DEBUG_MSG(2, ("message length: %d, out_left: %d", 5 + ssl->out_msglen, ssl->out_left));

        buf = ssl->out_hdr + 5 + ssl->out_msglen - ssl->out_left;
        ret = ssl->f_send(ssl->p_send, buf, ssl->out_left);
        SSL_DEBUG_RET(2, "ssl->f_send", ret);

        if (ret <= 0) {
            return ret;
        }
        ssl->out_left -= ret;
    }
    SSL_DEBUG_MSG(2, ("<= flush output"));
    return 0;
}


/*
    Record layer functions
 */
int ssl_write_record(ssl_context * ssl)
{
    int ret, len = ssl->out_msglen;

    SSL_DEBUG_MSG(2, ("=> write record"));

    ssl->out_hdr[0] = (uchar)ssl->out_msgtype;
    ssl->out_hdr[1] = (uchar)ssl->major_ver;
    ssl->out_hdr[2] = (uchar)ssl->minor_ver;
    ssl->out_hdr[3] = (uchar)(len >> 8);
    ssl->out_hdr[4] = (uchar)(len);

    if (ssl->out_msgtype == SSL_MSG_HANDSHAKE) {
        ssl->out_msg[1] = (uchar)((len - 4) >> 16);
        ssl->out_msg[2] = (uchar)((len - 4) >> 8);
        ssl->out_msg[3] = (uchar)((len - 4));
        md5_update(&ssl->fin_md5, ssl->out_msg, len);
        sha1_update(&ssl->fin_sha1, ssl->out_msg, len);
    }
    if (ssl->do_crypt != 0) {
        if ((ret = ssl_encrypt_buf(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_encrypt_buf", ret);
            return ret;
        }
        len = ssl->out_msglen;
        ssl->out_hdr[3] = (uchar)(len >> 8);
        ssl->out_hdr[4] = (uchar)(len);
    }
    ssl->out_left = 5 + ssl->out_msglen;

    SSL_DEBUG_MSG(3, ("output record: msgtype = %d, version = [%d:%d], msglen = %d",
        ssl->out_hdr[0], ssl->out_hdr[1], ssl->out_hdr[2], (ssl->out_hdr[3] << 8) | ssl->out_hdr[4]));
    SSL_DEBUG_BUF(4, "output record sent to network", ssl->out_hdr, 5 + ssl->out_msglen);

    if ((ret = ssl_flush_output(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_flush_output", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write record"));
    return 0;
}


int ssl_read_record(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> read record"));

    if (ssl->in_hslen != 0 && ssl->in_hslen < ssl->in_msglen) {
        /*
            Get next Handshake message in the current record
         */
        ssl->in_msglen -= ssl->in_hslen;

        memcpy(ssl->in_msg, ssl->in_msg + ssl->in_hslen, ssl->in_msglen);

        ssl->in_hslen = 4;
        ssl->in_hslen += (ssl->in_msg[2] << 8) | ssl->in_msg[3];

        SSL_DEBUG_MSG(3, ("handshake message: msglen = %d, type = %d, hslen = %d",
                  ssl->in_msglen, ssl->in_msg[0], ssl->in_hslen));

        if (ssl->in_msglen < 4 || ssl->in_msg[1] != 0) {
            SSL_DEBUG_MSG(1, ("bad handshake length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        if (ssl->in_msglen < ssl->in_hslen) {
            SSL_DEBUG_MSG(1, ("bad handshake length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        md5_update(&ssl->fin_md5, ssl->in_msg, ssl->in_hslen);
        sha1_update(&ssl->fin_sha1, ssl->in_msg, ssl->in_hslen);
        return 0;
    }

    ssl->in_hslen = 0;

    /*
        Read the record header and validate it
     */
    if ((ret = ssl_fetch_input(ssl, 5)) != 0) {
        SSL_DEBUG_RET(3, "ssl_fetch_input", ret);
        return ret;
    }
    ssl->in_msgtype = ssl->in_hdr[0];
    ssl->in_msglen = (ssl->in_hdr[3] << 8) | ssl->in_hdr[4];

    SSL_DEBUG_MSG(3, ("input record: msgtype = %d, version = [%d:%d], msglen = %d",
          ssl->in_hdr[0], ssl->in_hdr[1], ssl->in_hdr[2], (ssl->in_hdr[3] << 8) | ssl->in_hdr[4]));

    if (ssl->in_hdr[1] != ssl->major_ver) {
        SSL_DEBUG_MSG(1, ("major version mismatch"));
        return EST_ERR_SSL_INVALID_RECORD;
    }
    if (ssl->in_hdr[2] != SSL_MINOR_VERSION_0 && ssl->in_hdr[2] != SSL_MINOR_VERSION_1) {
        SSL_DEBUG_MSG(1, ("minor version mismatch"));
        return EST_ERR_SSL_INVALID_RECORD;
    }
    /*
        Make sure the message length is acceptable
     */
    if (ssl->do_crypt == 0) {
        if (ssl->in_msglen < 1 || ssl->in_msglen > SSL_MAX_CONTENT_LEN) {
            SSL_DEBUG_MSG(1, ("bad message length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
    } else {
        if (ssl->in_msglen < ssl->minlen) {
            SSL_DEBUG_MSG(1, ("bad message length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        if (ssl->minor_ver == SSL_MINOR_VERSION_0 && ssl->in_msglen > ssl->minlen + SSL_MAX_CONTENT_LEN) {
            SSL_DEBUG_MSG(1, ("bad message length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        /*
            TLS encrypted messages can have up to 256 bytes of padding
         */
        if (ssl->minor_ver == SSL_MINOR_VERSION_1 &&
            ssl->in_msglen > ssl->minlen + SSL_MAX_CONTENT_LEN + 256) {
            SSL_DEBUG_MSG(1, ("bad message length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
    }
    /*
        Read and optionally decrypt the message contents
     */
    if ((ret = ssl_fetch_input(ssl, 5 + ssl->in_msglen)) != 0) {
        SSL_DEBUG_RET(3, "ssl_fetch_input", ret);
        return ret;
    }
    SSL_DEBUG_BUF(4, "input record from network", ssl->in_hdr, 5 + ssl->in_msglen);

    if (ssl->do_crypt != 0) {
        if ((ret = ssl_decrypt_buf(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_decrypt_buf", ret);
            return ret;
        }
        SSL_DEBUG_BUF(4, "input payload after decrypt", ssl->in_msg, ssl->in_msglen);

        if (ssl->in_msglen > SSL_MAX_CONTENT_LEN) {
            SSL_DEBUG_MSG(1, ("bad message length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
    }

    if (ssl->in_msgtype == SSL_MSG_HANDSHAKE) {
        ssl->in_hslen = 4;
        ssl->in_hslen += (ssl->in_msg[2] << 8) | ssl->in_msg[3];

        SSL_DEBUG_MSG(3, ("handshake message: msglen = %d, type = %d, hslen = %d",
            ssl->in_msglen, ssl->in_msg[0], ssl->in_hslen));

        /*
            Additional checks to validate the handshake header
         */
        if (ssl->in_msglen < 4 || ssl->in_msg[1] != 0) {
            SSL_DEBUG_MSG(1, ("bad handshake length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        if (ssl->in_msglen < ssl->in_hslen) {
            SSL_DEBUG_MSG(1, ("bad handshake length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        md5_update(&ssl->fin_md5, ssl->in_msg, ssl->in_hslen);
        sha1_update(&ssl->fin_sha1, ssl->in_msg, ssl->in_hslen);
    }

    if (ssl->in_msgtype == SSL_MSG_ALERT) {
        SSL_DEBUG_MSG(2, ("got an alert message, type: [%d:%d]", ssl->in_msg[0], ssl->in_msg[1]));
        /*
            Ignore non-fatal alerts, except close_notify
         */
        if (ssl->in_msg[0] == SSL_ALERT_FATAL) {
            SSL_DEBUG_MSG(1, ("is a fatal alert message"));
            return EST_ERR_SSL_FATAL_ALERT_MESSAGE | ssl->in_msg[1];
        }
        if (ssl->in_msg[0] == SSL_ALERT_WARNING && ssl->in_msg[1] == SSL_ALERT_CLOSE_NOTIFY) {
            SSL_DEBUG_MSG(2, ("is a close notify message"));
            return EST_ERR_SSL_PEER_CLOSE_NOTIFY;
        }
    }
    ssl->in_left = 0;
    SSL_DEBUG_MSG(2, ("<= read record"));
    return 0;
}


/*
    Handshake functions
 */
int ssl_write_certificate(ssl_context * ssl)
{
    int ret, i, n;
    x509_cert *crt;

    SSL_DEBUG_MSG(2, ("=> write certificate"));

    if (ssl->endpoint == SSL_IS_CLIENT) {
        if (ssl->client_auth == 0) {
            SSL_DEBUG_MSG(2, ("<= skip write certificate"));
            ssl->state++;
            return 0;
        }
        /*
           If using SSLv3 and got no cert, send an Alert message (otherwise an empty Certificate message will be sent).
         */
        if (ssl->own_cert == NULL && ssl->minor_ver == SSL_MINOR_VERSION_0) {
            ssl->out_msglen = 2;
            ssl->out_msgtype = SSL_MSG_ALERT;
            ssl->out_msg[0] = SSL_ALERT_WARNING;
            ssl->out_msg[1] = SSL_ALERT_NO_CERTIFICATE;
            SSL_DEBUG_MSG(2, ("got no certificate to send"));
            goto write_msg;
        }
    } else {
        /* SSL_IS_SERVER */
        if (ssl->own_cert == NULL) {
            SSL_DEBUG_MSG(1, ("got no certificate to send"));
            return EST_ERR_SSL_CERTIFICATE_REQUIRED;
        }
    }

    SSL_DEBUG_CRT(3, "own certificate", ssl->own_cert);

    /*
           0  .  0    handshake type
           1  .  3    handshake length
           4  .  6    length of all certs
           7  .  9    length of cert. 1
          10  . n-1   peer certificate
           n  . n+2   length of cert. 2
          n+3 . ...   upper level cert, etc.
     */
    i = 7;
    crt = ssl->own_cert;

    while (crt != NULL && crt->next != NULL) {
        n = crt->raw.len;
        if (i + 3 + n > SSL_MAX_CONTENT_LEN) {
            SSL_DEBUG_MSG(1, ("certificate too large, %d > %d", i + 3 + n, SSL_MAX_CONTENT_LEN));
            return EST_ERR_SSL_CERTIFICATE_TOO_LARGE;
        }
        ssl->out_msg[i] = (uchar)(n >> 16);
        ssl->out_msg[i + 1] = (uchar)(n >> 8);
        ssl->out_msg[i + 2] = (uchar)(n);
        i += 3;
        memcpy(ssl->out_msg + i, crt->raw.p, n);
        i += n;
        crt = crt->next;
    }
    ssl->out_msg[4] = (uchar)((i - 7) >> 16);
    ssl->out_msg[5] = (uchar)((i - 7) >> 8);
    ssl->out_msg[6] = (uchar)((i - 7));

    ssl->out_msglen = i;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_CERTIFICATE;

write_msg:
    ssl->state++;
    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write certificate"));
    return 0;
}


int ssl_parse_certificate(ssl_context * ssl)
{
    int ret, i, n;

    SSL_DEBUG_MSG(2, ("=> parse certificate"));

    if (ssl->endpoint == SSL_IS_SERVER && ssl->authmode == SSL_VERIFY_NO_CHECK) {
        SSL_DEBUG_MSG(2, ("<= skip parse certificate"));
        ssl->state++;
        return 0;
    }
    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    ssl->state++;

    /*
       Check if the client sent an empty certificate
     */
    if (ssl->endpoint == SSL_IS_SERVER && ssl->minor_ver == SSL_MINOR_VERSION_0) {
        if (ssl->in_msglen == 2 &&
            ssl->in_msgtype == SSL_MSG_ALERT &&
            ssl->in_msg[0] == SSL_ALERT_WARNING &&
            ssl->in_msg[1] == SSL_ALERT_NO_CERTIFICATE) {
            SSL_DEBUG_MSG(1, ("SSLv3 client has no certificate"));

            if (ssl->authmode == SSL_VERIFY_OPTIONAL) {
                return 0;
            } else {
                return EST_ERR_SSL_NO_CLIENT_CERTIFICATE;
            }
        }
    }
    if (ssl->endpoint == SSL_IS_SERVER && ssl->minor_ver != SSL_MINOR_VERSION_0) {
        if (ssl->in_hslen == 7 &&
            ssl->in_msgtype == SSL_MSG_HANDSHAKE &&
            ssl->in_msg[0] == SSL_HS_CERTIFICATE &&
            memcmp(ssl->in_msg + 4, "\0\0\0", 3) == 0) {
            SSL_DEBUG_MSG(1, ("TLSv1 client has no certificate"));

            if (ssl->authmode == SSL_VERIFY_REQUIRED) {
                return EST_ERR_SSL_NO_CLIENT_CERTIFICATE;
            } else {
                return 0;
            }
        }
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad certificate message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }
    if (ssl->in_msg[0] != SSL_HS_CERTIFICATE || ssl->in_hslen < 10) {
        SSL_DEBUG_MSG(1, ("bad certificate message"));
        return EST_ERR_SSL_BAD_HS_CERTIFICATE;
    }

    /*
        Same message structure as in ssl_write_certificate()
     */
    n = (ssl->in_msg[5] << 8) | ssl->in_msg[6];

    if (ssl->in_msg[4] != 0 || ssl->in_hslen != 7 + n) {
        SSL_DEBUG_MSG(1, ("bad certificate message"));
        return EST_ERR_SSL_BAD_HS_CERTIFICATE;
    }
    if ((ssl->peer_cert = (x509_cert *) malloc(sizeof(x509_cert))) == NULL) {
        SSL_DEBUG_MSG(1, ("malloc(%d bytes) failed", sizeof(x509_cert)));
        return 1;
    }
    memset(ssl->peer_cert, 0, sizeof(x509_cert));

    i = 7;
    while (i < ssl->in_hslen) {
        if (ssl->in_msg[i] != 0) {
            SSL_DEBUG_MSG(1, ("bad certificate message"));
            return EST_ERR_SSL_BAD_HS_CERTIFICATE;
        }
        n = ((uint)ssl->in_msg[i + 1] << 8) | (uint)ssl->in_msg[i + 2];
        i += 3;

        if (n < 128 || i + n > ssl->in_hslen) {
            SSL_DEBUG_MSG(1, ("bad certificate message"));
            return EST_ERR_SSL_BAD_HS_CERTIFICATE;
        }
        ret = x509parse_crt(ssl->peer_cert, ssl->in_msg + i, n);
        if (ret != 0) {
            SSL_DEBUG_RET(1, " x509parse_crt", ret);
            return ret;
        }
        i += n;
    }
    SSL_DEBUG_CRT(3, "peer certificate", ssl->peer_cert);

    if (ssl->authmode != SSL_VERIFY_NO_CHECK) {
        if (ssl->ca_chain == NULL) {
            SSL_DEBUG_MSG(1, ("got no CA chain"));
            return EST_ERR_SSL_CA_CHAIN_REQUIRED;
        }
        ret = x509parse_verify(ssl->peer_cert, ssl->ca_chain, ssl->peer_cn, &ssl->verify_result);
        if (ret != 0) {
            SSL_DEBUG_MSG(3, ("x509_verify_cert %d, verify_result %d", ret, ssl->verify_result));
        }
        if (ssl->authmode != SSL_VERIFY_REQUIRED) {
            ret = 0;
        }
    }
    SSL_DEBUG_MSG(2, ("<= parse certificate"));
    return ret;
}


int ssl_write_change_cipher_spec(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> write change cipher spec"));

    ssl->out_msgtype = SSL_MSG_CHANGE_CIPHER_SPEC;
    ssl->out_msglen = 1;
    ssl->out_msg[0] = 1;
    ssl->do_crypt = 0;
    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write change cipher spec"));
    return 0;
}


int ssl_parse_change_cipher_spec(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> parse change cipher spec"));
    ssl->do_crypt = 0;

    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_CHANGE_CIPHER_SPEC) {
        SSL_DEBUG_MSG(1, ("bad change cipher spec message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }
    if (ssl->in_msglen != 1 || ssl->in_msg[0] != 1) {
        SSL_DEBUG_MSG(1, ("bad change cipher spec message"));
        return EST_ERR_SSL_BAD_HS_CHANGE_CIPHER_SPEC;
    }
    ssl->state++;
    SSL_DEBUG_MSG(2, ("<= parse change cipher spec"));
    return 0;
}


static void ssl_calc_finished(ssl_context * ssl, uchar *buf, int from, md5_context * md5, sha1_context * sha1)
{
    int len = 12;
    char *sender;
    uchar padbuf[48];
    uchar md5sum[16];
    uchar sha1sum[20];

    SSL_DEBUG_MSG(2, ("=> calc  finished"));

    /*
       SSLv3:
         hash =
            MD5( master + pad2 +
                MD5( handshake + sender + master + pad1 ) )
         + SHA1( master + pad2 +
               SHA1( handshake + sender + master + pad1 ) )
      
       TLSv1:
         hash = PRF( master, finished_label,
                     MD5( handshake ) + SHA1( handshake ) )[0..11]
     */

    SSL_DEBUG_BUF(4, "finished  md5 state", (uchar *) md5->state, sizeof(md5->state));
    SSL_DEBUG_BUF(4, "finished sha1 state", (uchar *) sha1->state, sizeof(sha1->state));

    if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
        sender = (from == SSL_IS_CLIENT) ? (char *)"CLNT" : (char *)"SRVR";

        memset(padbuf, 0x36, 48);
        md5_update(md5, (uchar *)sender, 4);
        md5_update(md5, ssl->session->master, 48);
        md5_update(md5, padbuf, 48);
        md5_finish(md5, md5sum);

        sha1_update(sha1, (uchar *)sender, 4);
        sha1_update(sha1, ssl->session->master, 48);
        sha1_update(sha1, padbuf, 40);
        sha1_finish(sha1, sha1sum);

        memset(padbuf, 0x5C, 48);

        md5_starts(md5);
        md5_update(md5, ssl->session->master, 48);
        md5_update(md5, padbuf, 48);
        md5_update(md5, md5sum, 16);
        md5_finish(md5, buf);

        sha1_starts(sha1);
        sha1_update(sha1, ssl->session->master, 48);
        sha1_update(sha1, padbuf, 40);
        sha1_update(sha1, sha1sum, 20);
        sha1_finish(sha1, buf + 16);
        len += 24;

    } else {
        sender = (from == SSL_IS_CLIENT) ? (char *)"client finished" : (char *)"server finished";
        md5_finish(md5, padbuf);
        sha1_finish(sha1, padbuf + 16);
        tls1_prf(ssl->session->master, 48, sender, padbuf, 36, buf, len);
    }
    SSL_DEBUG_BUF(3, "calc finished result", buf, len);

    memset(md5, 0, sizeof(md5_context));
    memset(sha1, 0, sizeof(sha1_context));
    memset(padbuf, 0, sizeof(padbuf));
    memset(md5sum, 0, sizeof(md5sum));
    memset(sha1sum, 0, sizeof(sha1sum));

    SSL_DEBUG_MSG(2, ("<= calc  finished"));
}


int ssl_write_finished(ssl_context * ssl)
{
    int ret, hash_len;
    md5_context md5;
    sha1_context sha1;

    SSL_DEBUG_MSG(2, ("=> write finished"));

    memcpy(&md5, &ssl->fin_md5, sizeof(md5_context));
    memcpy(&sha1, &ssl->fin_sha1, sizeof(sha1_context));

    ssl_calc_finished(ssl, ssl->out_msg + 4, ssl->endpoint, &md5, &sha1);

    hash_len = (ssl->minor_ver == SSL_MINOR_VERSION_0) ? 36 : 12;

    ssl->out_msglen = 4 + hash_len;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_FINISHED;

    /*
       In case of session resuming, invert the client and server
       ChangeCipherSpec messages order.
     */
    if (ssl->resume != 0) {
        if (ssl->endpoint == SSL_IS_CLIENT) {
            ssl->state = SSL_HANDSHAKE_OVER;
        } else {
            ssl->state = SSL_CLIENT_CHANGE_CIPHER_SPEC;
        }
    } else {
        ssl->state++;
    }
    ssl->do_crypt = 1;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write finished"));
    return 0;
}


int ssl_parse_finished(ssl_context * ssl)
{
    int ret, hash_len;
    md5_context md5;
    sha1_context sha1;
    uchar buf[36];

    SSL_DEBUG_MSG(2, ("=> parse finished"));

    memcpy(&md5, &ssl->fin_md5, sizeof(md5_context));
    memcpy(&sha1, &ssl->fin_sha1, sizeof(sha1_context));

    ssl->do_crypt = 1;

    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad finished message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }

    hash_len = (ssl->minor_ver == SSL_MINOR_VERSION_0) ? 36 : 12;

    if (ssl->in_msg[0] != SSL_HS_FINISHED || ssl->in_hslen != 4 + hash_len) {
        SSL_DEBUG_MSG(1, ("bad finished message"));
        return EST_ERR_SSL_BAD_HS_FINISHED;
    }
    ssl_calc_finished(ssl, buf, ssl->endpoint ^ 1, &md5, &sha1);

    if (memcmp(ssl->in_msg + 4, buf, hash_len) != 0) {
        SSL_DEBUG_MSG(1, ("bad finished message"));
        return EST_ERR_SSL_BAD_HS_FINISHED;
    }

    if (ssl->resume != 0) {
        if (ssl->endpoint == SSL_IS_CLIENT) {
            ssl->state = SSL_CLIENT_CHANGE_CIPHER_SPEC;
        }
        if (ssl->endpoint == SSL_IS_SERVER) {
            ssl->state = SSL_HANDSHAKE_OVER;
        }
    } else {
        ssl->state++;
    }
    SSL_DEBUG_MSG(2, ("<= parse finished"));
    return 0;
}


#if EMBEDTHIS || 1
/*
    Default single-threaded session mgmt functios
 */
static ssl_session *slist = NULL;
static ssl_session *cur, *prv;

static int getSession(ssl_context *ssl)
{
    time_t  t;
   
    t = time(NULL);

    if (ssl->resume == 0) {
        return 1;
    }
    cur = slist;
    prv = NULL;

    while (cur) {
        prv = cur;
        cur = cur->next;
        if (ssl->timeout != 0 && t - prv->start > ssl->timeout) {
            continue;
        }
        if (ssl->session->cipher != prv->cipher || ssl->session->length != prv->length) {
            continue;
        }
        if (memcmp(ssl->session->id, prv->id, prv->length) != 0) {
            continue;
        }
        memcpy(ssl->session->master, prv->master, 48);
        return 0;
    }
    return 1;
}

static int setSession(ssl_context *ssl)
{
    time_t  t;
   
    t = time(NULL);

    cur = slist;
    prv = NULL;
    while (cur) {
        if (ssl->timeout != 0 && t - cur->start > ssl->timeout) {
            /* expired, reuse this slot */
            break;
        }
        if (memcmp(ssl->session->id, cur->id, cur->length) == 0) {
            /* client reconnected */
            break;  
        }
        prv = cur;
        cur = cur->next;
    }
    if (cur == NULL) {
        cur = (ssl_session*) malloc(sizeof(ssl_session));
        if (cur == NULL) {
            return 1;
        }
        if (prv == NULL) {
            slist = cur;
        } else {
            prv->next = cur;
        }
    }
    memcpy(cur, ssl->session, sizeof(ssl_session));
    return 0;
}
#endif


/*
    Initialize an SSL context
 */
int ssl_init(ssl_context * ssl)
{
    int len = SSL_BUFFER_LEN;

    memset(ssl, 0, sizeof(ssl_context));

    ssl->in_ctr = (uchar *)malloc(len);
    ssl->in_hdr = ssl->in_ctr + 8;
    ssl->in_msg = ssl->in_ctr + 13;

    if (ssl->in_ctr == NULL) {
        SSL_DEBUG_MSG(1, ("malloc(%d bytes) failed", len));
        return 1;
    }
    ssl->out_ctr = (uchar *)malloc(len);
    ssl->out_hdr = ssl->out_ctr + 8;
    ssl->out_msg = ssl->out_ctr + 13;

    if (ssl->out_ctr == NULL) {
        SSL_DEBUG_MSG(1, ("malloc(%d bytes) failed", len));
        free(ssl->in_ctr);
        return 1;
    }
    memset(ssl->in_ctr, 0, SSL_BUFFER_LEN);
    memset(ssl->out_ctr, 0, SSL_BUFFER_LEN);

    ssl->hostname = NULL;
    ssl->hostname_len = 0;

#if EMBEDTHIS || 1
    ssl->s_get = getSession;
    ssl->s_set = setSession;
#endif

    md5_starts(&ssl->fin_md5);
    sha1_starts(&ssl->fin_sha1);
    return 0;
}


/*
   SSL set accessors
 */
void ssl_set_endpoint(ssl_context * ssl, int endpoint)
{
    ssl->endpoint = endpoint;
}


void ssl_set_authmode(ssl_context * ssl, int authmode)
{
    ssl->authmode = authmode;
}


void ssl_set_rng(ssl_context * ssl, int (*f_rng) (void *), void *p_rng)
{
    ssl->f_rng = f_rng;
    ssl->p_rng = p_rng;
}


void ssl_set_dbg(ssl_context * ssl, void (*f_dbg) (void *, int, char *), void *p_dbg)
{
    ssl->f_dbg = f_dbg;
    ssl->p_dbg = p_dbg;
}

void ssl_set_bio(ssl_context * ssl,
     int (*f_recv) (void *, uchar *, int), void *p_recv,
     int (*f_send) (void *, uchar *, int), void *p_send)
{
    ssl->f_recv = f_recv;
    ssl->f_send = f_send;
    ssl->p_recv = p_recv;
    ssl->p_send = p_send;
}


void ssl_set_scb(ssl_context * ssl, int (*s_get) (ssl_context *), int (*s_set) (ssl_context *))
{
    ssl->s_get = s_get;
    ssl->s_set = s_set;
}


void ssl_set_session(ssl_context * ssl, int resume, int timeout, ssl_session * session)
{
    ssl->resume = resume;
    ssl->timeout = timeout;
    ssl->session = session;
}


void ssl_set_ciphers(ssl_context * ssl, int *ciphers)
{
    ssl->ciphers = ciphers;
}


void ssl_set_ca_chain(ssl_context * ssl, x509_cert * ca_chain, char *peer_cn)
{
    ssl->ca_chain = ca_chain;
    ssl->peer_cn = peer_cn;
}


void ssl_set_own_cert(ssl_context * ssl, x509_cert * own_cert, rsa_context * rsa_key)
{
    ssl->own_cert = own_cert;
    ssl->rsa_key = rsa_key;
}


int ssl_set_dh_param(ssl_context * ssl, char *dhm_P, char *dhm_G)
{
    int ret;

    if ((ret = mpi_read_string(&ssl->dhm_ctx.P, 16, dhm_P)) != 0) {
        SSL_DEBUG_RET(1, "mpi_read_string", ret);
        return ret;
    }
    if ((ret = mpi_read_string(&ssl->dhm_ctx.G, 16, dhm_G)) != 0) {
        SSL_DEBUG_RET(1, "mpi_read_string", ret);
        return ret;
    }
    return 0;
}


int ssl_set_hostname(ssl_context * ssl, char *hostname)
{
    if (hostname == NULL) {
        return EST_ERR_SSL_BAD_INPUT_DATA;
    }
    ssl->hostname_len = strlen(hostname);
    ssl->hostname = (uchar *)malloc(ssl->hostname_len + 1);
    memcpy(ssl->hostname, (uchar *)hostname, ssl->hostname_len);
    return 0;
}


/*
    SSL get accessors
 */
int ssl_get_bytes_avail(ssl_context * ssl)
{
    return ssl->in_offt == NULL ? 0 : ssl->in_msglen;
}


int ssl_get_verify_result(ssl_context * ssl)
{
    return ssl->verify_result;
}


#if UNUSED
char *ssl_get_cipher(ssl_context * ssl)
{
    switch (ssl->session->cipher) {
#if ME_EST_RC4
    case TLS_RSA_WITH_RC4_128_MD5:
        return "TLS_RSA_WITH_RC4_128_MD5";

    case TLS_RSA_WITH_RC4_128_SHA:
        return "TLS_RSA_WITH_RC4_128_SHA";
#endif

#if ME_EST_DES
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
        return "TLS_RSA_WITH_3DES_EDE_CBC_SHA";

    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        return "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA";
#endif

#if ME_EST_AES
    case TLS_RSA_WITH_AES_128_CBC_SHA:
        return "TLS_RSA_WITH_AES_128_CBC_SHA";

    case TLS_RSA_WITH_AES_256_CBC_SHA:
        return "TLS_RSA_WITH_AES_256_CBC_SHA";

    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        return "TLS_DHE_RSA_WITH_AES_256_CBC_SHA";
#endif

#if ME_EST_CAMELLIA
    case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA:
        return "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA";

    case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA:
        return "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA";

    case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA:
        return "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA";
#endif

    default:
        break;
    }
    return "unknown";
}
#endif


/*
   Perform the SSL handshake
 */
PUBLIC int ssl_handshake(ssl_context * ssl)
{
#if ME_EST_LOGGING
    char    cbuf[4096];
#endif
    int old_state = ssl->state;
    int ret = EST_ERR_SSL_FEATURE_UNAVAILABLE;

    SSL_DEBUG_MSG(2, ("=> handshake"));

#if ME_EST_CLIENT
    if (ssl->endpoint == SSL_IS_CLIENT)
        ret = ssl_handshake_client(ssl);
#endif

#if ME_EST_SERVER
    if (ssl->endpoint == SSL_IS_SERVER)
        ret = ssl_handshake_server(ssl);
#endif
    SSL_DEBUG_MSG(2, ("<= handshake"));
    
#if ME_EST_LOGGING
    if (ssl->state == SSL_HANDSHAKE_OVER && old_state != SSL_HANDSHAKE_OVER) {
        SSL_DEBUG_MSG(1, ("using cipher: %s", ssl_get_cipher(ssl)));
        if (ssl->peer_cert) {
            SSL_DEBUG_MSG(1, ("Peer certificate: %s", x509parse_cert_info("", cbuf, sizeof(cbuf), ssl->peer_cert)));
        } else {
            SSL_DEBUG_MSG(1, ("Peer supplied no certificate"));
        }
    }
#endif
    return ret;
}


/*
   Receive application data decrypted from the SSL layer
 */
int ssl_read(ssl_context * ssl, uchar *buf, int len)
{
    int ret, n;

    SSL_DEBUG_MSG(2, ("=> read"));

    if (ssl->state != SSL_HANDSHAKE_OVER) {
        if ((ret = ssl_handshake(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_handshake", ret);
            return ret;
        }
    }
    if (ssl->in_offt == NULL) {
        if ((ret = ssl_read_record(ssl)) != 0) {
            SSL_DEBUG_RET(3, "ssl_read_record", ret);
            return ret;
        }
        if (ssl->in_msglen == 0 && ssl->in_msgtype == SSL_MSG_APPLICATION_DATA) {
            /*
               OpenSSL sends empty messages to randomize the IV
             */
            if ((ret = ssl_read_record(ssl)) != 0) {
                SSL_DEBUG_RET(3, "ssl_read_record", ret);
                return ret;
            }
        }
        if (ssl->in_msgtype != SSL_MSG_APPLICATION_DATA) {
            SSL_DEBUG_MSG(1, ("bad application data message"));
            return EST_ERR_SSL_UNEXPECTED_MESSAGE;
        }
        ssl->in_offt = ssl->in_msg;
    }
    n = (len < ssl->in_msglen) ? len : ssl->in_msglen;

    memcpy(buf, ssl->in_offt, n);
    ssl->in_msglen -= n;

    if (ssl->in_msglen == 0) {
        /* all bytes consumed  */
        ssl->in_offt = NULL;
    } else {
        /* more data available */
        ssl->in_offt += n;
    }
    SSL_DEBUG_MSG(2, ("<= read"));
    return n;
}


/*
   Send application data to be encrypted by the SSL layer
 */
int ssl_write(ssl_context * ssl, uchar *buf, int len)
{
    int ret, n;

    SSL_DEBUG_MSG(2, ("=> write"));

    if (ssl->state != SSL_HANDSHAKE_OVER) {
        if ((ret = ssl_handshake(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_handshake", ret);
            return ret;
        }
    }
    n = (len < SSL_MAX_CONTENT_LEN) ? len : SSL_MAX_CONTENT_LEN;

    if (ssl->out_left != 0) {
        if ((ret = ssl_flush_output(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_flush_output", ret);
            return ret;
        }
    } else {
        ssl->out_msglen = n;
        ssl->out_msgtype = SSL_MSG_APPLICATION_DATA;
        memcpy(ssl->out_msg, buf, n);

        if ((ret = ssl_write_record(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_write_record", ret);
            return ret;
        }
    }
    SSL_DEBUG_MSG(2, ("<= write"));
    return n;
}


/*
   Notify the peer that the connection is being closed
 */
int ssl_close_notify(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> write close notify"));

    if ((ret = ssl_flush_output(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_flush_output", ret);
        return ret;
    }
    if (ssl->state == SSL_HANDSHAKE_OVER) {
        ssl->out_msgtype = SSL_MSG_ALERT;
        ssl->out_msglen = 2;
        ssl->out_msg[0] = SSL_ALERT_WARNING;
        ssl->out_msg[1] = SSL_ALERT_CLOSE_NOTIFY;

        if ((ret = ssl_write_record(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_write_record", ret);
            return ret;
        }
    }
    SSL_DEBUG_MSG(2, ("<= write close notify"));
    return ret;
}


/*
   Free an SSL context
 */
void ssl_free(ssl_context * ssl)
{
    SSL_DEBUG_MSG(2, ("=> free"));

    if (ssl->peer_cert != NULL) {
        x509_free(ssl->peer_cert);
        memset(ssl->peer_cert, 0, sizeof(x509_cert));
        free(ssl->peer_cert);
    }
    if (ssl->out_ctr != NULL) {
        memset(ssl->out_ctr, 0, SSL_BUFFER_LEN);
        free(ssl->out_ctr);
    }
    if (ssl->in_ctr != NULL) {
        memset(ssl->in_ctr, 0, SSL_BUFFER_LEN);
        free(ssl->in_ctr);
    }
#if ME_EST_DHM
    dhm_free(&ssl->dhm_ctx);
#endif
    if (ssl->hostname != NULL) {
        memset(ssl->hostname, 0, ssl->hostname_len);
        free(ssl->hostname);
        ssl->hostname_len = 0;
    }
    memset(ssl, 0, sizeof(ssl_context));
    SSL_DEBUG_MSG(2, ("<= free"));
}


#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/timing.c ************/


/*
    timing.c -- Portable interface to the CPU cycle counter

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_TIMING

#if WINDOWS
struct _hr_time {
    LARGE_INTEGER start;
};
#else
struct _hr_time {
    struct timeval start;
};
#endif

#if WINDOWS
ulong hardclock(void)
{
    LARGE_INTEGER  now;
    QueryPerformanceCounter(&now);
    return (ulong) (((uint64) now.HighPart) << 32) + now.LowPart;
}

#elif (defined(_MSC_VER) && defined(_M_IX86)) || defined(__WATCOMC__)

#elif WINDOWS
ulong hardclock(void)
{
    ulong tsc;
    __asm rdtsc 
    __asm mov[tsc], eax 
    return (tsc);
}

#elif defined(__GNUC__) && defined(__i386__)

ulong hardclock(void)
{
    ulong tsc;
    asm("rdtsc":"=a"(tsc));
    return (tsc);
}

#elif defined(__GNUC__) && (defined(__amd64__) || defined(__x86_64__))

ulong hardclock(void)
{
    ulong lo, hi;
    asm("rdtsc":"=a"(lo), "=d"(hi));
    return (lo | (hi << 32));
}

#elif defined(__GNUC__) && (defined(__powerpc__) || defined(__ppc__))

ulong hardclock(void)
{
    ulong tbl, tbu0, tbu1;

    do {
        asm("mftbu %0":"=r"(tbu0));
        asm("mftb   %0":"=r"(tbl));
        asm("mftbu %0":"=r"(tbu1));
    } while (tbu0 != tbu1);
    return (tbl);
}

#elif defined(__GNUC__) && defined(__sparc__)

ulong hardclock(void)
{
    ulong tick;
    asm(".byte 0x83, 0x41, 0x00, 0x00");
    asm("mov    %%g1, %0":"=r"(tick));
    return (tick);
}

#elif defined(__GNUC__) && defined(__alpha__)

ulong hardclock(void)
{
    ulong cc;
    asm("rpcc %0":"=r"(cc));
    return (cc & 0xFFFFFFFF);
}

#elif defined(__GNUC__) && defined(__ia64__)

ulong hardclock(void)
{
    ulong itc;
    asm("mov %0 = ar.itc":"=r"(itc));
    return (itc);
}

#else

static int hardclock_init = 0;
static struct timeval tv_init;

ulong hardclock(void)
{
    struct timeval tv_cur;

    if (hardclock_init == 0) {
        gettimeofday(&tv_init, NULL);
        hardclock_init = 1;
    }
    gettimeofday(&tv_cur, NULL);
    return ((tv_cur.tv_sec - tv_init.tv_sec) * 1000000 + (tv_cur.tv_usec - tv_init.tv_usec));
}

#endif

int alarmed = 0;

#if WINDOWS

ulong get_timer(struct hr_time *val, int reset)
{
    ulong delta;
    LARGE_INTEGER offset, hfreq;
    struct _hr_time *t = (struct _hr_time *)val;

    QueryPerformanceCounter(&offset);
    QueryPerformanceFrequency(&hfreq);

    delta = (ulong)((1000 * (offset.QuadPart - t->start.QuadPart)) / hfreq.QuadPart);
    if (reset) {
        QueryPerformanceCounter(&t->start);
    }
    return (delta);
}


DWORD WINAPI TimerProc(LPVOID uElapse)
{
    Sleep((DWORD) uElapse);
    alarmed = 1;
    return (TRUE);
}


#if UNUSED
void set_alarm(int seconds)
{
    DWORD ThreadId;
    alarmed = 0;
    CloseHandle(CreateThread(NULL, 0, TimerProc, (LPVOID) (seconds * 1000), 0, &ThreadId));
}
#endif


void m_sleep(int milliseconds)
{
    Sleep(milliseconds);
}


PUBLIC int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    #if ME_WIN_LIKE
        FILETIME        fileTime;
        Time            now;
        static int      tzOnce;

        if (NULL != tv) {
            /* Convert from 100-nanosec units to microsectonds */
            GetSystemTimeAsFileTime(&fileTime);
            now = ((((Time) fileTime.dwHighDateTime) << BITS(uint)) + ((Time) fileTime.dwLowDateTime));
            now /= 10;
            now -= TIME_GENESIS;
            tv->tv_sec = (long) (now / 1000000);
            tv->tv_usec = (long) (now % 1000000);
        }
        if (NULL != tz) {
            TIME_ZONE_INFORMATION   zone;
            int                     rc, bias;
            rc = GetTimeZoneInformation(&zone);
            bias = (int) zone.Bias;
            if (rc == TIME_ZONE_ID_DAYLIGHT) {
                tz->tz_dsttime = 1;
            } else {
                tz->tz_dsttime = 0;
            }
            tz->tz_minuteswest = bias;
        }
        return 0;

    #elif VXWORKS
        struct tm       tm;
        struct timespec now;
        time_t          t;
        char            *tze, *p;
        int             rc;

        if ((rc = clock_gettime(CLOCK_REALTIME, &now)) == 0) {
            tv->tv_sec  = now.tv_sec;
            tv->tv_usec = (now.tv_nsec + 500) / MS_PER_SEC;
            if ((tze = getenv("TIMEZONE")) != 0) {
                if ((p = strchr(tze, ':')) != 0) {
                    if ((p = strchr(tze, ':')) != 0) {
                        tz->tz_minuteswest = stoi(++p);
                    }
                }
                t = tickGet();
                tz->tz_dsttime = (localtime_r(&t, &tm) == 0) ? tm.tm_isdst : 0;
            }
        }
        return rc;
    #endif
}

#else

ulong get_timer(struct hr_time *val, int reset)
{
    ulong delta;
    struct timeval offset;
    struct _hr_time *t = (struct _hr_time *)val;

    gettimeofday(&offset, NULL);
    delta = (offset.tv_sec - t->start.tv_sec) * 1000 + (offset.tv_usec - t->start.tv_usec) / 1000;
    if (reset) {
        t->start.tv_sec = offset.tv_sec;
        t->start.tv_usec = offset.tv_usec;
    }
    return (delta);
}


static void sighandler(int signum)
{
    alarmed = 1;
    signal(signum, sighandler);
}


#if UNUSED
void set_alarm(int seconds)
{
    alarmed = 0;
    signal(SIGALRM, sighandler);
    alarm(seconds);
}
#endif


void m_sleep(int milliseconds)
{
    struct timeval tv;

    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = milliseconds * 1000;
    select(0, NULL, NULL, NULL, &tv);
}

#endif

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/x509parse.c ************/


/*
    x509parse.c -- X.509 certificate and private key decoding

    The ITU-T X.509 standard defines a certificat format for PKI.
  
    http://www.ietf.org/rfc/rfc2459.txt
    http://www.ietf.org/rfc/rfc3279.txt
  
    ftp://ftp.rsasecurity.com/pub/pkcs/ascii/pkcs-1v2.asc
  
    http://www.itu.int/ITU-T/studygroups/com17/languages/X.680-0207.pdf
    http://www.itu.int/ITU-T/studygroups/com17/languages/X.690-0207.pdf
*/



#if ME_EST_X509

/*
    ASN.1 DER decoding routines
 */
static int asn1_get_len(uchar **p, uchar *end, int *len)
{
    if ((end - *p) < 1) {
        return EST_ERR_ASN1_OUT_OF_DATA;
    }

    if ((**p & 0x80) == 0) {
        *len = *(*p)++;
    } else {
        switch (**p & 0x7F) {
        case 1:
            if ((end - *p) < 2) {
                return EST_ERR_ASN1_OUT_OF_DATA;
            }
            *len = (*p)[1];
            (*p) += 2;
            break;

        case 2:
            if ((end - *p) < 3) {
                return EST_ERR_ASN1_OUT_OF_DATA;
            }
            *len = ((*p)[1] << 8) | (*p)[2];
            (*p) += 3;
            break;

        default:
            return EST_ERR_ASN1_INVALID_LENGTH;
        }
    }
    if (*len > (int)(end - *p)) {
        return EST_ERR_ASN1_OUT_OF_DATA;
    }
    return 0;
}


static int asn1_get_tag(uchar **p, uchar *end, int *len, int tag)
{
    if ((end - *p) < 1) {
        return EST_ERR_ASN1_OUT_OF_DATA;
    }
    if (**p != tag) {
        return EST_ERR_ASN1_UNEXPECTED_TAG;
    }
    (*p)++;
    return asn1_get_len(p, end, len);
}


static int asn1_get_bool(uchar **p, uchar *end, int *val)
{
    int ret, len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_BOOLEAN)) != 0) {
        return ret;
    }
    if (len != 1) {
        return EST_ERR_ASN1_INVALID_LENGTH;
    }
    *val = (**p != 0) ? 1 : 0;
    (*p)++;
    return 0;
}


static int asn1_get_int(uchar **p, uchar *end, int *val)
{
    int ret, len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_INTEGER)) != 0) {
        return ret;
    }
    if (len > (int)sizeof(int) || (**p & 0x80) != 0) {
        return EST_ERR_ASN1_INVALID_LENGTH;
    }
    *val = 0;
    while (len-- > 0) {
        *val = (*val << 8) | **p;
        (*p)++;
    }
    return 0;
}


static int asn1_get_mpi(uchar **p, uchar *end, mpi * X)
{
    int ret, len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_INTEGER)) != 0) {
        return ret;
    }
    ret = mpi_read_binary(X, *p, len);
    *p += len;
    return ret;
}


/*
    Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
 */
static int x509_get_version(uchar **p, uchar *end, int *ver)
{
    int ret, len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONTEXT_SPECIFIC | EST_ASN1_CONSTRUCTED | 0)) != 0) {
        if (ret == EST_ERR_ASN1_UNEXPECTED_TAG) {
            return *ver = 0;
        }
        return ret;
    }
    end = *p + len;

    if ((ret = asn1_get_int(p, end, ver)) != 0) {
        return EST_ERR_X509_CERT_INVALID_VERSION | ret;
    }
    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_VERSION | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    return 0;
}


/*
    CertificateSerialNumber  ::=  INTEGER
 */
static int x509_get_serial(uchar **p, uchar *end, x509_buf * serial)
{
    int ret;

    if ((end - *p) < 1) {
        return EST_ERR_X509_CERT_INVALID_SERIAL | EST_ERR_ASN1_OUT_OF_DATA;
    }
    if (**p != (EST_ASN1_CONTEXT_SPECIFIC | EST_ASN1_PRIMITIVE | 2) && **p != EST_ASN1_INTEGER) {
        return EST_ERR_X509_CERT_INVALID_SERIAL | EST_ERR_ASN1_UNEXPECTED_TAG;
    }
    serial->tag = *(*p)++;

    if ((ret = asn1_get_len(p, end, &serial->len)) != 0) {
        return EST_ERR_X509_CERT_INVALID_SERIAL | ret;
    }
    serial->p = *p;
    *p += serial->len;
    return 0;
}


/*
    AlgorithmIdentifier  ::=  SEQUENCE  {
         algorithm               OBJECT IDENTIFIER,
         parameters              ANY DEFINED BY algorithm OPTIONAL  }
 */
static int x509_get_alg(uchar **p, uchar *end, x509_buf * alg)
{
    int ret, len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        return EST_ERR_X509_CERT_INVALID_ALG | ret;
    }
    end = *p + len;
    alg->tag = **p;

    if ((ret = asn1_get_tag(p, end, &alg->len, EST_ASN1_OID)) != 0) {
        return EST_ERR_X509_CERT_INVALID_ALG | ret;
    }
    alg->p = *p;
    *p += alg->len;

    if (*p == end) {
        return 0;
    }

    /*
        assume the algorithm parameters must be NULL
     */
    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_NULL)) != 0) {
        return EST_ERR_X509_CERT_INVALID_ALG | ret;
    }
    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_ALG | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    return 0;
}


/*
    RelativeDistinguishedName ::=
      SET OF AttributeTypeAndValue
  
    AttributeTypeAndValue ::= SEQUENCE {
      type     AttributeType,
      value    AttributeValue }
  
    AttributeType ::= OBJECT IDENTIFIER
  
    AttributeValue ::= ANY DEFINED BY AttributeType
 */
static int x509_get_name(uchar **p, uchar *end, x509_name * cur)
{
    int ret, len;
    uchar *end2;
    x509_buf *oid;
    x509_buf *val;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SET)) != 0) {
        return EST_ERR_X509_CERT_INVALID_NAME | ret;
    }
    end2 = end;
    end = *p + len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        return EST_ERR_X509_CERT_INVALID_NAME | ret;
    }
    if (*p + len != end) {
        return EST_ERR_X509_CERT_INVALID_NAME | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    oid = &cur->oid;
    oid->tag = **p;

    if ((ret = asn1_get_tag(p, end, &oid->len, EST_ASN1_OID)) != 0) {
        return EST_ERR_X509_CERT_INVALID_NAME | ret;
    }
    oid->p = *p;
    *p += oid->len;

    if ((end - *p) < 1) {
        return EST_ERR_X509_CERT_INVALID_NAME | EST_ERR_ASN1_OUT_OF_DATA;
    }
    if (**p != EST_ASN1_BMP_STRING && **p != EST_ASN1_UTF8_STRING &&
            **p != EST_ASN1_T61_STRING && **p != EST_ASN1_PRINTABLE_STRING &&
            **p != EST_ASN1_IA5_STRING && **p != EST_ASN1_UNIVERSAL_STRING) {
        return EST_ERR_X509_CERT_INVALID_NAME | EST_ERR_ASN1_UNEXPECTED_TAG;
    }

    val = &cur->val;
    val->tag = *(*p)++;

    if ((ret = asn1_get_len(p, end, &val->len)) != 0) {
        return EST_ERR_X509_CERT_INVALID_NAME | ret;
    }
    val->p = *p;
    *p += val->len;

    cur->next = NULL;

    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_NAME | EST_ERR_ASN1_LENGTH_MISMATCH;
    }

    /*
       recurse until end of SEQUENCE is reached
     */
    if (*p == end2) {
        return 0;
    }
    cur->next = (x509_name *) malloc(sizeof(x509_name));

    if (cur->next == NULL) {
        return 1;
    }
    return x509_get_name(p, end2, cur->next);
}


/*
    Validity ::= SEQUENCE {
         notBefore      Time,
         notAfter       Time }
  
    Time ::= CHOICE {
         utcTime        UTCTime,
         generalTime    GeneralizedTime }
 */
static int x509_get_dates(uchar **p, uchar *end, x509_time *from, x509_time *to)
{
    int ret, len;
    char date[64];

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        return EST_ERR_X509_CERT_INVALID_DATE | ret;
    }
    end = *p + len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_UTC_TIME)) != 0) {
        return EST_ERR_X509_CERT_INVALID_DATE | ret;
    }
    memset(date, 0, sizeof(date));
    memcpy(date, *p, (len < (int)sizeof(date) - 1) ?  len : (int)sizeof(date) - 1);

    if (sscanf(date, "%2d%2d%2d%2d%2d%2d", &from->year, &from->mon, &from->day, &from->hour, &from->min, &from->sec) < 5) {
        return EST_ERR_X509_CERT_INVALID_DATE;
    }
    from->year += 100 * (from->year < 90);
    from->year += 1900;
    *p += len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_UTC_TIME)) != 0) {
        return EST_ERR_X509_CERT_INVALID_DATE | ret;
    }
    memset(date, 0, sizeof(date));
    memcpy(date, *p, (len < (int)sizeof(date) - 1) ?  len : (int)sizeof(date) - 1);

    if (sscanf(date, "%2d%2d%2d%2d%2d%2d", &to->year, &to->mon, &to->day, &to->hour, &to->min, &to->sec) < 5) {
        return EST_ERR_X509_CERT_INVALID_DATE;
    }
    to->year += 100 * (to->year < 90);
    to->year += 1900;
    *p += len;

    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_DATE | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    return 0;
}


/*
    SubjectPublicKeyInfo  ::=  SEQUENCE  {
         algorithm            AlgorithmIdentifier,
         subjectPublicKey     BIT STRING }
 */
static int x509_get_pubkey(uchar **p, uchar *end, x509_buf * pk_alg_oid, mpi * N, mpi * E)
{
    int ret, len;
    uchar *end2;

    if ((ret = x509_get_alg(p, end, pk_alg_oid)) != 0) {
        return ret;
    }

    /*
     * only RSA public keys handled at this time
     */
    if (pk_alg_oid->len != 9 || memcmp(pk_alg_oid->p, OID_PKCS1_RSA, 9) != 0) {
        return EST_ERR_X509_CERT_UNKNOWN_PK_ALG;
    }
    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_ME_STRING)) != 0) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | ret;
    }
    if ((end - *p) < 1) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | EST_ERR_ASN1_OUT_OF_DATA;
    }
    end2 = *p + len;

    if (*(*p)++ != 0) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY;
    }

    /*
            RSAPublicKey ::= SEQUENCE {
                    modulus                   INTEGER,      -- n
                    publicExponent    INTEGER       -- e
            }
     */
    if ((ret = asn1_get_tag(p, end2, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | ret;
    }
    if (*p + len != end2) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    if ((ret = asn1_get_mpi(p, end2, N)) != 0 || (ret = asn1_get_mpi(p, end2, E)) != 0) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | ret;
    }
    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    return 0;
}


static int x509_get_sig(uchar **p, uchar *end, x509_buf * sig)
{
    int ret, len;

    sig->tag = **p;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_ME_STRING)) != 0) {
        return EST_ERR_X509_CERT_INVALID_SIGNATURE | ret;
    }
    if (--len < 1 || *(*p)++ != 0) {
        return EST_ERR_X509_CERT_INVALID_SIGNATURE;
    }
    sig->len = len;
    sig->p = *p;
    *p += len;
    return 0;
}


/*
    X.509 v2/v3 unique identifier (not parsed)
 */
static int x509_get_uid(uchar **p, uchar *end, x509_buf * uid, int n)
{
    int ret;

    if (*p == end) {
        return 0;
    }
    uid->tag = **p;

    if ((ret = asn1_get_tag(p, end, &uid->len, EST_ASN1_CONTEXT_SPECIFIC | EST_ASN1_CONSTRUCTED | n)) != 0) {
        if (ret == EST_ERR_ASN1_UNEXPECTED_TAG) {
            return 0;
        }
        return ret;
    }
    uid->p = *p;
    *p += uid->len;
    return 0;
}


/*
   X.509 v3 extensions (only BasicConstraints are parsed)
 */
static int x509_get_ext(uchar **p, uchar *end, x509_buf * ext, int *ca_istrue, int *max_pathlen)
{
    int ret, len;
    int is_critical = 1;
    int is_cacert = 0;
    uchar *end2;

    if (*p == end) {
        return 0;
    }
    ext->tag = **p;

    if ((ret = asn1_get_tag(p, end, &ext->len, EST_ASN1_CONTEXT_SPECIFIC | EST_ASN1_CONSTRUCTED | 3)) != 0) {
        if (ret == EST_ERR_ASN1_UNEXPECTED_TAG) {
            return 0;
        }
        return ret;
    }
    ext->p = *p;
    end = *p + ext->len;

    /*
       Extensions  ::=      SEQUENCE SIZE (1..MAX) OF Extension
      
       Extension  ::=  SEQUENCE      {
                    extnID          OBJECT IDENTIFIER,
                    critical        BOOLEAN DEFAULT FALSE,
                    extnValue       OCTET STRING  }
     */
    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
    }
    if (end != *p + len) {
        return EST_ERR_X509_CERT_INVALID_EXTENSIONS | EST_ERR_ASN1_LENGTH_MISMATCH;
    }

    while (*p < end) {
        if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
        }
        if (memcmp(*p, "\x06\x03\x55\x1D\x13", 5) != 0) {
            *p += len;
            continue;
        }

        *p += 5;

        if ((ret = asn1_get_bool(p, end, &is_critical)) != 0 && (ret != EST_ERR_ASN1_UNEXPECTED_TAG)) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
        }
        if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_OCTET_STRING)) != 0) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
        }
        /*
           BasicConstraints ::= SEQUENCE {
                        cA                                              BOOLEAN DEFAULT FALSE,
                        pathLenConstraint               INTEGER (0..MAX) OPTIONAL }
         */
        end2 = *p + len;

        if ((ret = asn1_get_tag(p, end2, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
        }
        if (*p == end2) {
            continue;
        }
        if ((ret = asn1_get_bool(p, end2, &is_cacert)) != 0) {
            if (ret == EST_ERR_ASN1_UNEXPECTED_TAG) {
                ret = asn1_get_int(p, end2, &is_cacert);
            }
            if (ret != 0) {
                return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
            }
            if (is_cacert != 0) {
                is_cacert = 1;
            }
        }
        if (*p == end2) {
            continue;
        }
        if ((ret = asn1_get_int(p, end2, max_pathlen)) != 0) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
        }
        if (*p != end2) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | EST_ERR_ASN1_LENGTH_MISMATCH;
        }
        max_pathlen++;
    }

    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_EXTENSIONS | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    *ca_istrue = is_critical & is_cacert;
    return 0;
}


/*
   Parse one or more certificates and add them to the chained list
 */
int x509parse_crt(x509_cert * chain, uchar *buf, int buflen)
{
    int ret, len;
    uchar *s1, *s2, *oldbuf;
    uchar *p, *end;
    x509_cert *crt;

    crt = chain;

    while (crt->version != 0) {
        crt = crt->next;
    }

    /*
       check if the certificate is encoded in base64
     */
    s1 = (uchar *)strstr((char *)buf, "-----BEGIN CERTIFICATE-----");

    if (s1 != NULL) {
        s2 = (uchar *)strstr((char *)buf, "-----END CERTIFICATE-----");

        if (s2 == NULL || s2 <= s1) {
            return EST_ERR_X509_CERT_INVALID_PEM;
        }
        s1 += 27;
        if (*s1 == '\r') {
            s1++;
        }
        if (*s1 == '\n') {
            s1++;
        } else {
            return EST_ERR_X509_CERT_INVALID_PEM;
        }

        /*
           get the DER data length and decode the buffer
         */
        len = 0;
        ret = base64_decode(NULL, &len, s1, s2 - s1);

        if (ret == EST_ERR_BASE64_INVALID_CHARACTER) {
            return EST_ERR_X509_CERT_INVALID_PEM | ret;
        }
        if ((p = (uchar *)malloc(len)) == NULL) {
            return 1;
        }
        if ((ret = base64_decode(p, &len, s1, s2 - s1)) != 0) {
            free(p);
            return EST_ERR_X509_CERT_INVALID_PEM | ret;
        }

        /*
            update the buffer size and offset
         */
        s2 += 25;
        if (*s2 == '\r') {
            s2++;
        }
        if (*s2 == '\n') {
            s2++;
        } else {
            free(p);
            return EST_ERR_X509_CERT_INVALID_PEM;
        }
        oldbuf = buf;
        buflen -= s2 - buf;
        buf = s2;

    } else {
        /*
           nope, copy the raw DER data
         */
        p = (uchar*) malloc(len = buflen);
        if (p == NULL) {
            return 1;
        }
        memcpy(p, buf, buflen);
        oldbuf = buf;
        buflen = 0;
    }

    crt->raw.p = p;
    crt->raw.len = len;
    end = p + len;

    /*
       Certificate  ::=      SEQUENCE  {
                    tbsCertificate           TBSCertificate,
                    signatureAlgorithm       AlgorithmIdentifier,
                    signatureValue           BIT STRING      }
     */
    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT;
    }
    if (len != (int)(end - p)) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | EST_ERR_ASN1_LENGTH_MISMATCH;
    }

    /*
       TBSCertificate  ::=  SEQUENCE  {
     */
    crt->tbs.p = p;
    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | ret;
    }
    end = p + len;
    crt->tbs.len = end - crt->tbs.p;

    /*
       Version      ::=      INTEGER  {      v1(0), v2(1), v3(2)  }
      
       CertificateSerialNumber      ::=      INTEGER
      
       signature                    AlgorithmIdentifier
     */
    if ((ret = x509_get_version(&p, end, &crt->version)) != 0 ||
        (ret = x509_get_serial(&p, end, &crt->serial)) != 0 ||
        (ret = x509_get_alg(&p, end, &crt->sig_oid1)) != 0) {
        x509_free(crt);
        return ret;
    }
    crt->version++;
    if (crt->version > 3) {
        x509_free(crt);
        return EST_ERR_X509_CERT_UNKNOWN_VERSION;
    }
    if (crt->sig_oid1.len != 9 || memcmp(crt->sig_oid1.p, OID_PKCS1, 8) != 0) {
        x509_free(crt);
#if UNUSED
if (buflen > 0) {
    goto error;
}
#endif
        return EST_ERR_X509_CERT_UNKNOWN_SIG_ALG;
    }
    if (crt->sig_oid1.p[8] < 2 || crt->sig_oid1.p[8] > 5) {
        x509_free(crt);
#if UNUSED
if (buflen > 0) {
    goto error;
}
#endif
        return EST_ERR_X509_CERT_UNKNOWN_SIG_ALG;
    }
    /*
        issuer Name
     */
    crt->issuer_raw.p = p;
    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | ret;
    }
    if ((ret = x509_get_name(&p, p + len, &crt->issuer)) != 0) {
        x509_free(crt);
        return ret;
    }
    crt->issuer_raw.len = p - crt->issuer_raw.p;

    /*
       Validity ::= SEQUENCE {
                    notBefore          Time,
                    notAfter           Time }
      
     */
    if ((ret = x509_get_dates(&p, end, &crt->valid_from, &crt->valid_to)) != 0) {
        x509_free(crt);
#if UNUSED
if (buflen > 0) {
    goto error;
}
#endif
        return ret;
    }

    /*
        subject Name
     */
    crt->subject_raw.p = p;

    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | ret;
    }
    if ((ret = x509_get_name(&p, p + len, &crt->subject)) != 0) {
        x509_free(crt);
        return ret;
    }
    crt->subject_raw.len = p - crt->subject_raw.p;

    /*
       SubjectPublicKeyInfo  ::=  SEQUENCE
            algorithm             AlgorithmIdentifier,
            subjectPublicKey      BIT STRING      }
     */
    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | ret;
    }
    if ((ret = x509_get_pubkey(&p, p + len, &crt->pk_oid, &crt->rsa.N, &crt->rsa.E)) != 0) {
        x509_free(crt);
        return ret;
    }
    if ((ret = rsa_check_pubkey(&crt->rsa)) != 0) {
        x509_free(crt);
        return ret;
    }
    crt->rsa.len = mpi_size(&crt->rsa.N);

    /*
        issuerUniqueID  [1]      IMPLICIT UniqueIdentifier OPTIONAL,
                                 -- If present, version shall be v2 or v3
        subjectUniqueID [2]      IMPLICIT UniqueIdentifier OPTIONAL,
                                 -- If present, version shall be v2 or v3
        extensions      [3]      EXPLICIT Extensions OPTIONAL
                                 -- If present, version shall be v3
     */
    if (crt->version == 2 || crt->version == 3) {
        ret = x509_get_uid(&p, end, &crt->issuer_id, 1);
        if (ret != 0) {
            x509_free(crt);
            return ret;
        }
    }
    if (crt->version == 2 || crt->version == 3) {
        ret = x509_get_uid(&p, end, &crt->subject_id, 2);
        if (ret != 0) {
            x509_free(crt);
            return ret;
        }
    }
    if (crt->version == 3) {
        ret = x509_get_ext(&p, end, &crt->v3_ext, &crt->ca_istrue, &crt->max_pathlen);
        if (ret != 0) {
            x509_free(crt);
            return ret;
        }
    }
    if (p != end) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    end = crt->raw.p + crt->raw.len;

    /*
        signatureAlgorithm       AlgorithmIdentifier,
        signatureValue           BIT STRING
     */
    if ((ret = x509_get_alg(&p, end, &crt->sig_oid2)) != 0) {
        x509_free(crt);
        return ret;
    }
    if (memcmp(crt->sig_oid1.p, crt->sig_oid2.p, 9) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_SIG_MISMATCH;
    }
    if ((ret = x509_get_sig(&p, end, &crt->sig)) != 0) {
        x509_free(crt);
        return ret;
    }
    if (p != end) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    crt->next = (x509_cert *) malloc(sizeof(x509_cert));

    if (crt->next == NULL) {
        x509_free(crt);
        return 1;
    }
    crt = crt->next;
    memset(crt, 0, sizeof(x509_cert));

#if UNUSED
more:
#endif
    if (buflen > 0) {
        int rc = x509parse_crt(crt, buf, buflen);
        return 0;
    }
    return 0;

#if UNUSED
error:
    {
        char msg[80], *cp;
        strncpy(msg, (char*) &oldbuf[1], sizeof(msg) - 1);
        if ((cp = strchr(msg, '\n')) != 0) {
            *cp = '\0';
        }
        printf("FAILED to parse %s\n", msg);
        memset(crt, 0, sizeof(x509_cert));
        goto more;
    }
#endif
}


/*
    Load one or more certificates and add them to the chained list
 */
int x509parse_crtfile(x509_cert * chain, char *path)
{
    int ret;
    FILE *f;
    size_t n;
    uchar *buf;

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    fseek(f, 0, SEEK_END);
    n = (size_t) ftell(f);
    fseek(f, 0, SEEK_SET);

    if ((buf = (uchar *)malloc(n + 1)) == NULL) {
        return 1;
    }
    if (fread(buf, 1, n, f) != n) {
        fclose(f);
        free(buf);
        return 1;
    }
    buf[n] = '\0';
    ret = x509parse_crt(chain, buf, (int)n);
    memset(buf, 0, n + 1);
    free(buf);
    fclose(f);
    return ret;
}


#if ME_EST_DES
/*
    Read a 16-byte hex string and convert it to binary
 */
static int x509_get_iv(uchar *s, uchar iv[8])
{
    int i, j, k;

    memset(iv, 0, 8);

    for (i = 0; i < 16; i++, s++) {
        if (*s >= '0' && *s <= '9') {
            j = *s - '0';
        } else if (*s >= 'A' && *s <= 'F') {
            j = *s - '7';
        } else if (*s >= 'a' && *s <= 'f') {
            j = *s - 'W';
        } else {
            return EST_ERR_X509_KEY_INVALID_ENC_IV;
        }
        k = ((i & 1) != 0) ? j : j << 4;
        iv[i >> 1] = (uchar)(iv[i >> 1] | k);
    }

    return 0;
}


/*
    Decrypt with 3DES-CBC, using PBKDF1 for key derivation
 */
static void x509_des3_decrypt(uchar des3_iv[8], uchar *buf, int buflen, uchar *pwd, int pwdlen)
{
    md5_context md5_ctx;
    des3_context des3_ctx;
    uchar md5sum[16];
    uchar des3_key[24];

    /*
       3DES key[ 0..15] = MD5(pwd || IV)
            key[16..23] = MD5(pwd || IV || 3DES key[ 0..15])
     */
    md5_starts(&md5_ctx);
    md5_update(&md5_ctx, pwd, pwdlen);
    md5_update(&md5_ctx, des3_iv, 8);
    md5_finish(&md5_ctx, md5sum);
    memcpy(des3_key, md5sum, 16);

    md5_starts(&md5_ctx);
    md5_update(&md5_ctx, md5sum, 16);
    md5_update(&md5_ctx, pwd, pwdlen);
    md5_update(&md5_ctx, des3_iv, 8);
    md5_finish(&md5_ctx, md5sum);
    memcpy(des3_key + 16, md5sum, 8);

    des3_set3key_dec(&des3_ctx, des3_key);
    des3_crypt_cbc(&des3_ctx, DES_DECRYPT, buflen, des3_iv, buf, buf);

    memset(&md5_ctx, 0, sizeof(md5_ctx));
    memset(&des3_ctx, 0, sizeof(des3_ctx));
    memset(md5sum, 0, 16);
    memset(des3_key, 0, 24);
}
#endif


/*
    Parse a private RSA key
 */
int x509parse_key(rsa_context * rsa, uchar *buf, int buflen, uchar *pwd, int pwdlen)
{
    int     ret, len, enc;
    uchar   *s1, *s2, *p, *end;
#if ME_EST_DES
    uchar   des3_iv[8];
#endif

    s1 = (uchar *)strstr((char *)buf, "-----BEGIN RSA PRIVATE KEY-----");

    if (s1 != NULL) {
        s2 = (uchar *)strstr((char *)buf, "-----END RSA PRIVATE KEY-----");

        if (s2 == NULL || s2 <= s1) {
            return EST_ERR_X509_KEY_INVALID_PEM;
        }
        s1 += 31;
        if (*s1 == '\r') {
            s1++;
        }
        if (*s1 == '\n') {
            s1++;
        } else {
            return EST_ERR_X509_KEY_INVALID_PEM;
        }
        enc = 0;

        if (memcmp(s1, "Proc-Type: 4,ENCRYPTED", 22) == 0) {
#if ME_EST_DES
            enc++;
            s1 += 22;
            if (*s1 == '\r') {
                s1++;
            }
            if (*s1 == '\n') {
                s1++;
            } else {
                return EST_ERR_X509_KEY_INVALID_PEM;
            }
            if (memcmp(s1, "DEK-Info: DES-EDE3-CBC,", 23) != 0) {
                return EST_ERR_X509_KEY_UNKNOWN_ENC_ALG;
            }
            s1 += 23;
            if (x509_get_iv(s1, des3_iv) != 0) {
                return EST_ERR_X509_KEY_INVALID_ENC_IV;
            }
            s1 += 16;
            if (*s1 == '\r') {
                s1++;
            }
            if (*s1 == '\n') {
                s1++;
            } else {
                return EST_ERR_X509_KEY_INVALID_PEM;
            }
#else
            return EST_ERR_X509_FEATURE_UNAVAILABLE;
#endif
        }
        len = 0;
        ret = base64_decode(NULL, &len, s1, s2 - s1);

        if (ret == EST_ERR_BASE64_INVALID_CHARACTER) {
            return ret | EST_ERR_X509_KEY_INVALID_PEM;
        }
        if ((buf = (uchar *)malloc(len)) == NULL) {
            return 1;
        }
        if ((ret = base64_decode(buf, &len, s1, s2 - s1)) != 0) {
            free(buf);
            return ret | EST_ERR_X509_KEY_INVALID_PEM;
        }
        buflen = len;

        if (enc != 0) {
#if ME_EST_DES
            if (pwd == NULL) {
                free(buf);
                return EST_ERR_X509_KEY_PASSWORD_REQUIRED;
            }
            x509_des3_decrypt(des3_iv, buf, buflen, pwd, pwdlen);

            if (buf[0] != 0x30 || buf[1] != 0x82 || buf[4] != 0x02 || buf[5] != 0x01) {
                free(buf);
                return EST_ERR_X509_KEY_PASSWORD_MISMATCH;
            }
#else
            return EST_ERR_X509_FEATURE_UNAVAILABLE;
#endif
        }
    }
    memset(rsa, 0, sizeof(rsa_context));

    p = buf;
    end = buf + buflen;

    /*
            RSAPrivateKey ::= SEQUENCE {
                    version                   Version,
                    modulus                   INTEGER,      -- n
                    publicExponent    INTEGER,      -- e
                    privateExponent   INTEGER,      -- d
                    prime1                    INTEGER,      -- p
                    prime2                    INTEGER,      -- q
                    exponent1                 INTEGER,      -- d mod (p-1)
                    exponent2                 INTEGER,      -- d mod (q-1)
                    coefficient               INTEGER,      -- (inverse of q) mod p
                    otherPrimeInfos   OtherPrimeInfos OPTIONAL
            }
     */
    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return EST_ERR_X509_KEY_INVALID_FORMAT | ret;
    }
    end = p + len;

    if ((ret = asn1_get_int(&p, end, &rsa->ver)) != 0) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return EST_ERR_X509_KEY_INVALID_FORMAT | ret;
    }
    if (rsa->ver != 0) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return ret | EST_ERR_X509_KEY_INVALID_VERSION;
    }

    if ((ret = asn1_get_mpi(&p, end, &rsa->N)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->E)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->D)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->P)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->Q)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->DP)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->DQ)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->QP)) != 0) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return ret | EST_ERR_X509_KEY_INVALID_FORMAT;
    }

    rsa->len = mpi_size(&rsa->N);

    if (p != end) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return EST_ERR_X509_KEY_INVALID_FORMAT | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    if ((ret = rsa_check_privkey(rsa)) != 0) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return ret;
    }
    if (s1 != NULL) {
        free(buf);
    }
    return 0;
}


/*
    Load and parse a private RSA key
 */
int x509parse_keyfile(rsa_context * rsa, char *path, char *pwd)
{
    int ret;
    FILE *f;
    size_t n;
    uchar *buf;

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }

    fseek(f, 0, SEEK_END);
    n = (size_t) ftell(f);
    fseek(f, 0, SEEK_SET);

    if ((buf = (uchar *)malloc(n + 1)) == NULL) {
        return 1;
    }
    if (fread(buf, 1, n, f) != n) {
        fclose(f);
        free(buf);
        return 1;
    }
    buf[n] = '\0';

    if (pwd == NULL) {
        ret = x509parse_key(rsa, buf, (int)n, NULL, 0);
    } else {
        ret = x509parse_key(rsa, buf, (int)n, (uchar *)pwd, strlen(pwd));
    }
    memset(buf, 0, n + 1);
    free(buf);
    fclose(f);
    return ret;
}


/*
    Store the name in printable form into buf; no more than (end - buf) characters will be written
 */
int x509parse_dn_gets(char *prefix, char *buf, int bufsize, x509_name * dn)
{
    x509_name   *name;
    char        *end, s[128], *p;
    int         i;
    uchar       c;

    memset(s, 0, sizeof(s));
    name = dn;
    p = buf;
    end = &buf[bufsize];

    while (name != NULL) {
        p += snfmt(p, end - p, "%s", prefix);
        if (memcmp(name->oid.p, OID_X520, 2) == 0) {
            switch (name->oid.p[2]) {
            case X520_COMMON_NAME:
                p += snfmt(p, end - p, "CN=");
                break;

            case X520_COUNTRY:
                p += snfmt(p, end - p, "C=");
                break;

            case X520_LOCALITY:
                p += snfmt(p, end - p, "L=");
                break;

            case X520_STATE:
                p += snfmt(p, end - p, "ST=");
                break;

            case X520_ORGANIZATION:
                p += snfmt(p, end - p, "O=");
                break;

            case X520_ORG_UNIT:
                p += snfmt(p, end - p, "OU=");
                break;

            default:
                p += snfmt(p, end - p, "0x%02X=", name->oid.p[2]);
                break;
            }
        } else if (memcmp(name->oid.p, OID_PKCS9, 8) == 0) {
            switch (name->oid.p[8]) {
            case PKCS9_EMAIL:
                p += snfmt(p, end - p, "EMAIL=");
                break;

            default:
                p += snfmt(p, end - p, "0x%02X=", name->oid.p[8]);
                break;
            }
        } else {
            p += snfmt(p, end - p, "\?\?=");
        }
        for (i = 0; i < name->val.len; i++) {
            if (i >= (int)sizeof(s) - 1) {
                break;
            }
            c = name->val.p[i];
            if (c < 32 || c == 127 || (c > 128 && c < 160)) {
                s[i] = '?';
            } else {
                s[i] = c;
            }
        }
        s[i] = '\0';
        p += snfmt(p, end - p, "%s", s);
        name = name->next;
        p += snfmt(p, end - p, ",");
    }
    return p - buf;
}


/*
    Return an informational string about the certificate, or NULL if memory allocation failed
 */
char *x509parse_cert_info(char *prefix, char *buf, int bufsize, x509_cert *crt)
{
    char    *end, *p, *cipher, pbuf[5120];
    int     i, n;

    p = buf;
    end = &buf[bufsize];
    p += snfmt(p, end - p, "%sVERSION=%d,%sSERIAL=", prefix, crt->version, prefix);
    n = (crt->serial.len <= 32) ? crt->serial.len : 32;
    for (i = 0; i < n; i++) {
        p += snfmt(p, end - p, "%02X%s", crt->serial.p[i], (i < n - 1) ? ":" : "");
    }
    p += snfmt(p, end - p, ",");

    snfmt(pbuf, sizeof(pbuf), "%sS_", prefix);
    p += x509parse_dn_gets(pbuf, p, end - p, &crt->subject);

    snfmt(pbuf, sizeof(pbuf), "%sI_", prefix);
    p += x509parse_dn_gets(pbuf, p, end - p, &crt->issuer);

    p += snfmt(p, end - p, "%sSTART=%04d-%02d-%02d %02d:%02d:%02d,", prefix, crt->valid_from.year, crt->valid_from.mon,
        crt->valid_from.day, crt->valid_from.hour, crt->valid_from.min, crt->valid_from.sec);

    p += snfmt(p, end - p, "%sEND=%04d-%02d-%02d %02d:%02d:%02d,", prefix, crt->valid_to.year, crt->valid_to.mon, 
        crt->valid_to.day, crt->valid_to.hour, crt->valid_to.min, crt->valid_to.sec);

    switch (crt->sig_oid1.p[8]) {
    case RSA_MD2:
        cipher = "RSA_MD2";
        break;
    case RSA_MD4:
        cipher = "RSA_MD4";
        break;
    case RSA_MD5:
        cipher = "RSA_MD5";
        break;
    case RSA_SHA1:
        cipher = "RSA_SHA1";
        break;
    default:
        cipher = "RSA";
        break;
    }
    p += snfmt(p, end - p, "%sCIPHER=%s,", prefix, cipher);
    p += snfmt(p, end - p, "%sKEYSIZE=%d,", prefix, crt->rsa.N.n * (int)sizeof(ulong) * 8);
    return buf;
}


/*
    Return 0 if the certificate is still valid, or BADCERT_EXPIRED
 */
int x509parse_expired(x509_cert * crt)
{
    struct tm *lt;
    time_t tt;

    tt = time(NULL);
    lt = localtime(&tt);

    if (lt->tm_year > crt->valid_to.year - 1900) {
        return BADCERT_EXPIRED;
    }
    if (lt->tm_year == crt->valid_to.year - 1900 && lt->tm_mon > crt->valid_to.mon - 1) {
        return BADCERT_EXPIRED;
    }
    if (lt->tm_year == crt->valid_to.year - 1900 && lt->tm_mon == crt->valid_to.mon - 1 && lt->tm_mday > crt->valid_to.day) {
        return BADCERT_EXPIRED;
    }
    return 0;
}

static void x509_hash(uchar *in, int len, int alg, uchar *out)
{
    switch (alg) {
#if ME_EST_MD2
    case RSA_MD2:
        md2(in, len, out);
        break;
#endif
#if ME_EST_MD4
    case RSA_MD4:
        md4(in, len, out);
        break;
#endif
    case RSA_MD5:
        md5(in, len, out);
        break;
    case RSA_SHA1:
        sha1(in, len, out);
        break;
    default:
        memset(out, '\xFF', len);
        break;
    }
}


/*
    Verify the certificate validity
 */
int x509parse_verify(x509_cert *crt, x509_cert *trust_ca, char *cn, int *flags)
{
    x509_cert   *cur;
    x509_name   *name;
    uchar       hash[20];
    char        *domain, *peer;
    int         cn_len, hash_id, pathlen;

    *flags = x509parse_expired(crt);

    if (cn != NULL) {
        name = &crt->subject;
        cn_len = strlen(cn);

        while (name != NULL) {
            if (memcmp(name->oid.p, OID_CN, 3) == 0) {
                peer = (char*) name->val.p;
                if (name->val.len == cn_len && memcmp(peer, cn, cn_len) == 0) {
                    break;
                }
                /* 
                    Cert peer name must be of the form *.domain.tld. i.e. *.com is not valid 
                 */
                if (name->val.len > 2 && peer[0] == '*' && peer[1] == '.' && strchr(&peer[2], '.')) {
                    /* 
                        Required peer name must have a domain portion. i.e. domain.tld 
                     */
                    if ((domain = strchr(cn, '.')) != 0 && strncmp(&peer[2], &domain[1], name->val.len - 2) == 0) {
                        break;
                    }
                }
            }
            name = name->next;
        }
        if (name == NULL) {
            *flags |= BADCERT_CN_MISMATCH;
        }
    }
    *flags |= BADCERT_NOT_TRUSTED;

    /*
        Iterate upwards in the given cert chain, ignoring any upper cert with CA != TRUE.
     */
    cur = crt->next;
    pathlen = 1;

    while (cur->version != 0) {
        if (cur->ca_istrue == 0 || crt->issuer_raw.len != cur->subject_raw.len ||
            memcmp(crt->issuer_raw.p, cur->subject_raw.p, crt->issuer_raw.len) != 0) {
            cur = cur->next;
            continue;
        }
        hash_id = crt->sig_oid1.p[8];
        x509_hash(crt->tbs.p, crt->tbs.len, hash_id, hash);
        if (rsa_pkcs1_verify(&cur->rsa, RSA_PUBLIC, hash_id, 0, hash, crt->sig.p) != 0) {
            return EST_ERR_X509_CERT_VERIFY_FAILED;
        }
        pathlen++;
        crt = cur;
        cur = crt->next;
    }

    /*
        Atempt to validate topmost cert with our CA chain.
     */
    while (trust_ca->version != 0) {
        if (crt->issuer_raw.len != trust_ca->subject_raw.len ||
                memcmp(crt->issuer_raw.p, trust_ca->subject_raw.p, crt->issuer_raw.len) != 0) {
            trust_ca = trust_ca->next;
            continue;
        }
        if (trust_ca->max_pathlen > 0 && trust_ca->max_pathlen < pathlen) {
            break;
        }
        hash_id = crt->sig_oid1.p[8];
        x509_hash(crt->tbs.p, crt->tbs.len, hash_id, hash);

        if (rsa_pkcs1_verify(&trust_ca->rsa, RSA_PUBLIC, hash_id, 0, hash, crt->sig.p) == 0) {
            /*
                cert. is signed by a trusted CA
             */
            *flags &= ~BADCERT_NOT_TRUSTED;
            break;
        }
        trust_ca = trust_ca->next;
    }
    if (*flags & BADCERT_NOT_TRUSTED) {
        if (crt->issuer_raw.len == crt->subject_raw.len && 
                memcmp(crt->issuer_raw.p, crt->subject_raw.p, crt->issuer_raw.len) == 0) {
            *flags |= BADCERT_SELF_SIGNED;
        }
    }
    if (*flags != 0) {
        return EST_ERR_X509_CERT_VERIFY_FAILED;
    }
    return 0;
}


/*
    Unallocate all certificate data
 */
void x509_free(x509_cert * crt)
{
    x509_cert *cert_cur = crt;
    x509_cert *cert_prv;
    x509_name *name_cur;
    x509_name *name_prv;

    if (crt == NULL) {
        return;
    }
    do {
        rsa_free(&cert_cur->rsa);

        name_cur = cert_cur->issuer.next;
        while (name_cur != NULL) {
            name_prv = name_cur;
            name_cur = name_cur->next;
            memset(name_prv, 0, sizeof(x509_name));
            free(name_prv);
        }
        name_cur = cert_cur->subject.next;
        while (name_cur != NULL) {
            name_prv = name_cur;
            name_cur = name_cur->next;
            memset(name_prv, 0, sizeof(x509_name));
            free(name_prv);
        }
        if (cert_cur->raw.p != NULL) {
            memset(cert_cur->raw.p, 0, cert_cur->raw.len);
            free(cert_cur->raw.p);
        }
        cert_cur = cert_cur->next;
    } while (cert_cur != NULL);

    cert_cur = crt;
    do {
        cert_prv = cert_cur;
        cert_cur = cert_cur->next;

        memset(cert_prv, 0, sizeof(x509_cert));
        if (cert_prv != crt) {
            free(cert_prv);
        }
    } while (cert_cur != NULL);
}


#endif



/********* Start of file src/xtea.c ************/


/*
    xtea.c -- An 32-bit implementation of the XTEA algorithm

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_XTEA

/*
   32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ] << 24 )    \
            | ( (ulong) (b)[(i) + 1] << 16 )    \
            | ( (ulong) (b)[(i) + 2] <<  8 )    \
            | ( (ulong) (b)[(i) + 3]       );   \
    }
#endif

#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 2] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 3] = (uchar) ( (n)       );   \
    }
#endif

/*
   XTEA key schedule
 */
void xtea_setup(xtea_context *ctx, uchar key[16])
{
    int i;

    memset(ctx, 0, sizeof(xtea_context));
    for (i = 0; i < 4; i++) {
        GET_ULONG_BE(ctx->k[i], key, i << 2);
    }
}


/*
   XTEA encrypt function
 */
void xtea_crypt_ecb(xtea_context *ctx, int mode, uchar input[8], uchar output[8])
{
    ulong *k, v0, v1, i;

    k = ctx->k;
    GET_ULONG_BE(v0, input, 0);
    GET_ULONG_BE(v1, input, 4);

    if (mode == XTEA_ENCRYPT) {
        ulong sum = 0, delta = 0x9E3779B9;

        for (i = 0; i < 32; i++) {
            v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
            sum += delta;
            v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum >> 11) & 3]);
        }
    } else {
        /* XTEA_DECRYPT */
        ulong delta = 0x9E3779B9, sum = delta * 32;

        for (i = 0; i < 32; i++) {
            v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum >> 11) & 3]);
            sum -= delta;
            v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
        }
    }
    PUT_ULONG_BE(v0, output, 0);
    PUT_ULONG_BE(v1, output, 4);
}

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */

#endif /* ME_COM_EST */


/********* Start of file ../../../src/file.c ************/


/*
    file.c -- File handler
  
    This module serves static file documents
 */

/********************************* Includes ***********************************/



/*********************************** Locals ***********************************/

static char   *websIndex;                   /* Default page name */
static char   *websDocuments;               /* Default Web page directory */

/**************************** Forward Declarations ****************************/

static void fileWriteEvent(Webs *wp);

/*********************************** Code *************************************/
/*
    Serve static files
 */
static bool fileHandler(Webs *wp)
{
    WebsFileInfo    info;
    char            *tmp, *date;
    ssize           nchars;
    int             code;

    assert(websValid(wp));
    assert(wp->method);
    assert(wp->filename && wp->filename[0]);

#if !ME_ROM
    if (smatch(wp->method, "DELETE")) {
        if (unlink(wp->filename) < 0) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot delete the URI");
        } else {
            /* No content */
            websResponse(wp, 204, 0);
        }
    } else if (smatch(wp->method, "PUT")) {
        /* Code is already set for us by processContent() */
        websResponse(wp, wp->code, 0);

    } else 
#endif /* !ME_ROM */
    {
        /*
            If the file is a directory, redirect using the nominated default page
         */
        if (websPageIsDirectory(wp)) {
            nchars = strlen(wp->path);
            if (wp->path[nchars - 1] == '/' || wp->path[nchars - 1] == '\\') {
                wp->path[--nchars] = '\0';
            }
            tmp = sfmt("%s/%s", wp->path, websIndex);
            websRedirect(wp, tmp);
            wfree(tmp);
            return 1;
        }
        if (websPageOpen(wp, O_RDONLY | O_BINARY, 0666) < 0) {
#if ME_DEBUG
            if (wp->referrer) {
                trace(1, "From %s", wp->referrer);
            }
#endif
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot open document for: %s", wp->path);
            return 1;
        }
        if (websPageStat(wp, &info) < 0) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot stat page for URL");
            return 1;
        }
        code = 200;
        if (wp->since && info.mtime <= wp->since) {
            code = 304;
            info.size = 0;
        }
        websSetStatus(wp, code);
        websWriteHeaders(wp, info.size, 0);
        if ((date = websGetDateString(&info)) != NULL) {
            websWriteHeader(wp, "Last-Modified", "%s", date);
            wfree(date);
        }
        websWriteEndHeaders(wp);

        /*
            All done if the browser did a HEAD request
         */
        if (smatch(wp->method, "HEAD")) {
            websDone(wp);
            return 1;
        }
        if (info.size > 0) {
            websSetBackgroundWriter(wp, fileWriteEvent);
        } else {
            websDone(wp);
        }
    }
    return 1;
}


/*
    Do output back to the browser in the background. This is a socket write handler.
    This bypasses the output buffer and writes directly to the socket.
 */
static void fileWriteEvent(Webs *wp)
{
    char    *buf;
    ssize   len, wrote;

    assert(wp);
    assert(websValid(wp));

    /*
        Note: websWriteSocket may return less than we wanted. It will return -1 on a socket error.
     */
    if ((buf = walloc(ME_GOAHEAD_LIMIT_BUFFER)) == NULL) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot get memory");
        return;
    }
    /*
        OPT - we could potentially save this buffer so that on short-writes, it does not need to be re-read.
     */
    while ((len = websPageReadData(wp, buf, ME_GOAHEAD_LIMIT_BUFFER)) > 0) {
        if ((wrote = websWriteSocket(wp, buf, len)) < 0) {
            break;
        }
        if (wrote != len) {
            websPageSeek(wp, - (len - wrote), SEEK_CUR);
            break;
        }
    }
    wfree(buf);
    if (len <= 0) {
        websDone(wp);
    }
}


#if !ME_ROM
PUBLIC int websProcessPutData(Webs *wp)
{
    ssize   nbytes;

    assert(wp);
    assert(wp->putfd >= 0);
    assert(wp->input.buf);

    nbytes = bufLen(&wp->input);
    wp->putLen += nbytes;
    if (wp->putLen > ME_GOAHEAD_LIMIT_PUT) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Put file too large");
        return -1;
    }
    if (write(wp->putfd, wp->input.servp, (int) nbytes) != nbytes) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR | WEBS_CLOSE, "Cannot write to file");
        return -1;
    }
    websConsumeInput(wp, nbytes);
    return 0;
}
#endif


static void fileClose()
{
    wfree(websIndex);
    websIndex = NULL;
    wfree(websDocuments);
    websDocuments = NULL;
}


PUBLIC void websFileOpen()
{
    websIndex = sclone("index.html");
    websDefineHandler("file", 0, fileHandler, fileClose, 0);
}


/*
    Get the default page for URL requests ending in "/"
 */
PUBLIC char *websGetIndex()
{
    return websIndex;
}


PUBLIC char *websGetDocuments()
{
    return websDocuments;
}


/*
    Set the default page for URL requests ending in "/"
 */
PUBLIC void websSetIndex(char *page)
{
    assert(page && *page);

    if (websIndex) {
        wfree(websIndex);
    }
    websIndex = sclone(page);
}


/*
    Set the default web directory
 */
PUBLIC void websSetDocuments(char *dir)
{
    assert(dir && *dir);
    if (websDocuments) {
        wfree(websDocuments);
    }
    websDocuments = sclone(dir);
}

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/fs.c ************/


/*
    fs.c -- File System support and support for ROM-based file systems.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/******************************** Local Data **********************************/

#if ME_ROM
static WebsHash romFs;             /* Symbol table for web pages */
#endif

/*********************************** Code *************************************/

PUBLIC int websFsOpen()
{
#if ME_ROM
    WebsRomIndex    *wip;
    char            name[ME_GOAHEAD_LIMIT_FILENAME];
    ssize           len;

    romFs = hashCreate(WEBS_HASH_INIT);
    for (wip = websRomIndex; wip->path; wip++) {
        strncpy(name, wip->path, ME_GOAHEAD_LIMIT_FILENAME);
        len = strlen(name) - 1;
        if (len > 0 &&
            (name[len] == '/' || name[len] == '\\')) {
            name[len] = '\0';
        }
        hashEnter(romFs, name, valueSymbol(wip), 0);
    }
#endif
    return 0;
}


PUBLIC void websFsClose()
{
#if ME_ROM
    hashFree(romFs);
#endif
}


PUBLIC int websOpenFile(char *path, int flags, int mode)
{
#if ME_ROM
    WebsRomIndex    *wip;
    WebsKey         *sp;

    if ((sp = hashLookup(romFs, path)) == NULL) {
        return -1;
    }
    wip = (WebsRomIndex*) sp->content.value.symbol;
    wip->pos = 0;
    return (int) (wip - websRomIndex);
#else
    return open(path, flags, mode);
#endif
}


PUBLIC void websCloseFile(int fd)
{
    if (fd >= 0) {
#if !ME_ROM
        close(fd);
#endif
    }
}


PUBLIC int websStatFile(char *path, WebsFileInfo *sbuf)
{
#if ME_ROM
    WebsRomIndex    *wip;
    WebsKey         *sp;

    assert(path && *path);

    if ((sp = hashLookup(romFs, path)) == NULL) {
        return -1;
    }
    wip = (WebsRomIndex*) sp->content.value.symbol;
    memset(sbuf, 0, sizeof(WebsFileInfo));
    sbuf->size = wip->size;
    if (wip->page == NULL) {
        sbuf->isDir = 1;
    }
    return 0;
#else
    WebsStat    s;
    int         rc;
#if ME_WIN_LIKE
    ssize       len = slen(path) - 1;

    path = sclone(path);
    if (path[len] == '/') {
        path[len] = '\0';
    } else if (path[len] == '\\') {
        path[len - 1] = '\0';
    }
    rc = stat(path, &s);
    wfree(path);
#else
    rc = stat(path, &s);
#endif
    if (rc < 0) {
        return -1;
    }
    sbuf->size = (ssize) s.st_size;
    sbuf->mtime = s.st_mtime;
    sbuf->isDir = s.st_mode & S_IFDIR;                                                                     
    return 0;  
#endif
}


PUBLIC ssize websReadFile(int fd, char *buf, ssize size)
{
#if ME_ROM
    WebsRomIndex    *wip;
    ssize           len;

    assert(buf);
    assert(fd >= 0);

    wip = &websRomIndex[fd];

    len = min(wip->size - wip->pos, size);
    memcpy(buf, &wip->page[wip->pos], len);
    wip->pos += len;
    return len;
#else
    return read(fd, buf, (uint) size);
#endif
}


PUBLIC char *websReadWholeFile(char *path)
{
    WebsFileInfo    sbuf;
    char            *buf;
    int             fd;

    if (websStatFile(path, &sbuf) < 0) {
        return 0;
    }
    buf = walloc(sbuf.size + 1);
    if ((fd = websOpenFile(path, O_RDONLY, 0)) < 0) {
        wfree(buf);
        return 0;
    }
    websReadFile(fd, buf, sbuf.size);
    buf[sbuf.size] = '\0';
    websCloseFile(fd);
    return buf;
}


Offset websSeekFile(int fd, Offset offset, int origin)
{
#if ME_ROM
    WebsRomIndex    *wip;
    Offset          pos;

    assert(origin == SEEK_SET || origin == SEEK_CUR || origin == SEEK_END);
    assert(fd >= 0);

    wip = &websRomIndex[fd];

    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) {
        errno = EINVAL;
        return -1;
    }
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    pos = offset;
    switch (origin) {
    case SEEK_CUR:
        pos = wip->pos + offset;
        break;
    case SEEK_END:
        pos = wip->size + offset;
        break;
    default:
        break;
    }
    if (pos < 0) {
        errno = EBADF;
        return -1;
    }
    return (wip->pos = pos);
#else
    return lseek(fd, (long) offset, origin);
#endif
}


PUBLIC ssize websWriteFile(int fd, char *buf, ssize size)
{
#if ME_ROM
    error("Cannot write to a rom file system");
    return -1;
#else
    return write(fd, buf, (uint) size);
#endif
}


/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/goahead.c ************/


/*
    goahead.c -- Main program for GoAhead

    Usage: goahead [options] [documents] [IP][:port] ...
        Options:
        --auth authFile        # User and role configuration
        --background           # Run as a Linux daemon
        --home directory       # Change to directory to run
        --log logFile:level    # Log to file file at verbosity level
        --route routeFile      # Route configuration file
        --verbose              # Same as --log stderr:2
        --version              # Output version information

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/********************************* Defines ************************************/

static int finished = 0;

/********************************* Forwards ***********************************/

static void initPlatform();
static void logHeader();
static void usage();

#if WINDOWS
static void windowsClose();
static int windowsInit();
static LRESULT CALLBACK websWindProc(HWND hwnd, UINT msg, UINT wp, LPARAM lp);
#endif

#if ME_UNIX_LIKE
static void sigHandler(int signo);
#endif

/*********************************** Code *************************************/

MAIN(goahead, int argc, char **argv, char **envp)
{
    char    *argp, *home, *documents, *endpoints, *endpoint, *route, *auth, *tok, *lspec;
    int     argind;

#if WINDOWS
    if (windowsInit() < 0) {
        return 0;
    }
#endif
    route = "route.txt";
    auth = "auth.txt";

    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;

        } else if (smatch(argp, "--auth") || smatch(argp, "-a")) {
            auth = argv[++argind];

#if ME_UNIX_LIKE && !MACOSX
        } else if (smatch(argp, "--background") || smatch(argp, "-b")) {
            websSetBackground(1);
#endif

        } else if (smatch(argp, "--debugger") || smatch(argp, "-d") || smatch(argp, "-D")) {
            websSetDebug(1);

        } else if (smatch(argp, "--home")) {
            if (argind >= argc) usage();
            home = argv[++argind];
            if (chdir(home) < 0) {
                error("Cannot change directory to %s", home);
                exit(-1);
            }
        } else if (smatch(argp, "--log") || smatch(argp, "-l")) {
            if (argind >= argc) usage();
            logSetPath(argv[++argind]);

        } else if (smatch(argp, "--verbose") || smatch(argp, "-v")) {
            logSetPath("stdout:2");

        } else if (smatch(argp, "--route") || smatch(argp, "-r")) {
            route = argv[++argind];

        } else if (smatch(argp, "--version") || smatch(argp, "-V")) {
            printf("%s\n", ME_VERSION);
            exit(0);

        } else if (*argp == '-' && isdigit((uchar) argp[1])) {
            lspec = sfmt("stdout:%s", &argp[1]);
            logSetPath(lspec);
            wfree(lspec);

        } else {
            usage();
        }
    }
    documents = ME_GOAHEAD_DOCUMENTS;
    if (argc > argind) {
        documents = argv[argind++];
    }
    initPlatform();
    if (websOpen(documents, route) < 0) {
        error("Cannot initialize server. Exiting.");
        return -1;
    }
    if (websLoad(auth) < 0) {
        error("Cannot load %s", auth);
        return -1;
    }
    logHeader();
    if (argind < argc) {
        while (argind < argc) {
            endpoint = argv[argind++];
            if (websListen(endpoint) < 0) {
                return -1;
            }
        }
    } else {
        endpoints = sclone(ME_GOAHEAD_LISTEN);
        for (endpoint = stok(endpoints, ", \t", &tok); endpoint; endpoint = stok(NULL, ", \t,", &tok)) {
#if !ME_COM_SSL
            if (strstr(endpoint, "https")) continue;
#endif
            if (websListen(endpoint) < 0) {
                return -1;
            }
        }
        wfree(endpoints);
    }
#if ME_ROM && KEEP
    /*
        If not using a route/auth config files, then manually create the routes like this:
        If custom matching is required, use websSetRouteMatch. If authentication is required, use websSetRouteAuth.
     */
    websAddRoute("/", "file", 0);
#endif
#if ME_UNIX_LIKE && !MACOSX
    /*
        Service events till terminated
     */
    if (websGetBackground()) {
        if (daemon(0, 0) < 0) {
            error("Cannot run as daemon");
            return -1;
        }
    }
#endif
    websServiceEvents(&finished);
    logmsg(1, "Instructed to exit");
    websClose();
#if WINDOWS
    windowsClose();
#endif
    return 0;
}


static void logHeader()
{
    char    home[ME_GOAHEAD_LIMIT_STRING];

    getcwd(home, sizeof(home));
    logmsg(2, "Configuration for %s", ME_TITLE);
    logmsg(2, "---------------------------------------------");
    logmsg(2, "Version:            %s", ME_VERSION);
    logmsg(2, "BuildType:          %s", ME_DEBUG ? "Debug" : "Release");
    logmsg(2, "CPU:                %s", ME_CPU);
    logmsg(2, "OS:                 %s", ME_OS);
    logmsg(2, "Host:               %s", websGetServer());
    logmsg(2, "Directory:          %s", home);
    logmsg(2, "Documents:          %s", websGetDocuments());
    logmsg(2, "Configure:          %s", ME_CONFIG_CMD);
    logmsg(2, "---------------------------------------------");
}


static void usage() {
    fprintf(stderr, "\n%s Usage:\n\n"
        "  %s [options] [documents] [[IPaddress][:port] ...]\n\n"
        "  Options:\n"
        "    --auth authFile        # User and role configuration\n"
#if ME_UNIX_LIKE && !MACOSX
        "    --background           # Run as a Unix daemon\n"
#endif
        "    --debugger             # Run in debug mode\n"
        "    --home directory       # Change to directory to run\n"
        "    --log logFile:level    # Log to file file at verbosity level\n"
        "    --route routeFile      # Route configuration file\n"
        "    --verbose              # Same as --log stderr:2\n"
        "    --version              # Output version information\n\n",
        ME_TITLE, ME_NAME);
    exit(-1);
}


static void initPlatform() 
{
#if ME_UNIX_LIKE
    signal(SIGTERM, sigHandler);
    signal(SIGKILL, sigHandler);
    #ifdef SIGPIPE
        signal(SIGPIPE, SIG_IGN);
    #endif
#elif ME_WIN_LIKE
    _fmode=_O_BINARY;
#endif
}


#if ME_UNIX_LIKE
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

    inst = websGetInst();
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = inst;
    wc.hIcon         = NULL;
    wc.lpfnWndProc   = (WNDPROC) websWindProc;
    wc.lpszMenuName  = wc.lpszClassName = ME_NAME;
    if (! RegisterClass(&wc)) {
        return -1;
    }
    /*
        Create a window just so we can have a taskbar to close this web server
     */
    hwnd = CreateWindow(ME_NAME, ME_TITLE, WS_MINIMIZE | WS_POPUPWINDOW, CW_USEDEFAULT, 
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
    }
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    return 0;
}


static void windowsClose()
{
    HINSTANCE   inst;

    inst = websGetInst();
    UnregisterClass(ME_NAME, inst);
}


/*
    Main menu window message handler.
 */
static LRESULT CALLBACK websWindProc(HWND hwnd, UINT msg, UINT wp, LPARAM lp)
{
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            finished++;
            return 0;

        case WM_SYSCOMMAND:
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
    Windows message handler
 */
static LRESULT CALLBACK websAboutProc(HWND hwndDlg, uint msg, uint wp, long lp)
{
    LRESULT    lResult;

    lResult = DefWindowProc(hwndDlg, msg, wp, lp);

    switch (msg) {
        case WM_CREATE:
            break;
        case WM_DESTROY:
            break;
        case WM_COMMAND:
            break;
    }
    return lResult;
}

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/http.c ************/


/*
    http.c -- GoAhead HTTP engine

    This module implements an embedded HTTP/1.1 web server. It supports
    loadable URL handlers that define the nature of URL processing performed.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/*********************************** Globals **********************************/

static int websBackground;              /* Run as a daemon */
static int websDebug;                   /* Run in debug mode and defeat timeouts */
static int defaultHttpPort;             /* Default port number for http */
static int defaultSslPort;              /* Default port number for https */

#define WEBS_TIMEOUT (ME_GOAHEAD_LIMIT_TIMEOUT * 1000)
#define PARSE_TIMEOUT (ME_GOAHEAD_LIMIT_PARSE_TIMEOUT * 1000)
#define CHUNK_LOW   128                 /* Low water mark for chunking */

/************************************ Locals **********************************/

static int          listens[WEBS_MAX_LISTEN];   /* Listen endpoints */;
static int          listenMax;
static Webs         **webs;                     /* Open connection list head */
static WebsHash     websMime;                   /* Set of mime types */
static int          websMax;                    /* List size */
static char         websHost[64];               /* Host name for the server */
static char         websIpAddr[64];             /* IP address for the server */
static char         *websHostUrl = NULL;        /* URL to access server */
static char         *websIpAddrUrl = NULL;      /* URL to access server */

#define WEBS_ENCODE_HTML    0x1                 /* Bit setting in charMatch[] */

/*
    Character escape/descape matching codes. Generated by charGen.
 */
static uchar charMatch[256] = {
    0x00,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3e,0x3c,0x3c,0x3c,0x3c,0x3c,
    0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,
    0x3c,0x0c,0x3f,0x28,0x2a,0x3c,0x2b,0x0f,0x0e,0x0e,0x0e,0x28,0x28,0x00,0x00,0x28,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0x2a,0x3f,0x28,0x3f,0x2a,
    0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3a,0x3e,0x3a,0x3e,0x00,
    0x3e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3e,0x3e,0x3e,0x02,0x3c,
    0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,
    0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,
    0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,
    0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,
    0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,
    0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,
    0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,
    0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c,0x3c 
};

/*
    Addd entries to the MimeList as required for your content
 */
static WebsMime websMimeList[] = {
    { "application/java", ".class" },
    { "application/java", ".jar" },
    { "text/html", ".asp" },
    { "text/html", ".htm" },
    { "text/html", ".html" },
    { "text/xml", ".xml" },
    { "image/gif", ".gif" },
    { "image/jpeg", ".jpg" },
    { "image/png", ".png" },
    { "image/vnd.microsoft.icon", ".ico" },
    { "text/css", ".css" },
    { "text/plain", ".txt" },
    { "application/x-javascript", ".js" },
    { "application/x-shockwave-flash", ".swf" },

    { "application/binary", ".exe" },
    { "application/compress", ".z" },
    { "application/gzip", ".gz" },
    { "application/octet-stream", ".bin" },
    { "application/oda", ".oda" },
    { "application/pdf", ".pdf" },
    { "application/postscript", ".ai" },
    { "application/postscript", ".eps" },
    { "application/postscript", ".ps" },
    { "application/rtf", ".rtf" },
    { "application/x-bcpio", ".bcpio" },
    { "application/x-cpio", ".cpio" },
    { "application/x-csh", ".csh" },
    { "application/x-dvi", ".dvi" },
    { "application/x-gtar", ".gtar" },
    { "application/x-hdf", ".hdf" },
    { "application/x-latex", ".latex" },
    { "application/x-mif", ".mif" },
    { "application/x-netcdf", ".nc" },
    { "application/x-netcdf", ".cdf" },
    { "application/x-ns-proxy-autoconfig", ".pac" },
    { "application/x-patch", ".patch" },
    { "application/x-sh", ".sh" },
    { "application/x-shar", ".shar" },
    { "application/x-sv4cpio", ".sv4cpio" },
    { "application/x-sv4crc", ".sv4crc" },
    { "application/x-tar", ".tar" },
    { "application/x-tgz", ".tgz" },
    { "application/x-tcl", ".tcl" },
    { "application/x-tex", ".tex" },
    { "application/x-texinfo", ".texinfo" },
    { "application/x-texinfo", ".texi" },
    { "application/x-troff", ".t" },
    { "application/x-troff", ".tr" },
    { "application/x-troff", ".roff" },
    { "application/x-troff-man", ".man" },
    { "application/x-troff-me", ".me" },
    { "application/x-troff-ms", ".ms" },
    { "application/x-ustar", ".ustar" },
    { "application/x-wais-source", ".src" },
    { "application/zip", ".zip" },
    { "audio/basic", ".au snd" },
    { "audio/x-aiff", ".aif" },
    { "audio/x-aiff", ".aiff" },
    { "audio/x-aiff", ".aifc" },
    { "audio/x-wav", ".wav" },
    { "audio/x-wav", ".ram" },
    { "image/ief", ".ief" },
    { "image/jpeg", ".jpeg" },
    { "image/jpeg", ".jpe" },
    { "image/tiff", ".tiff" },
    { "image/tiff", ".tif" },
    { "image/x-cmu-raster", ".ras" },
    { "image/x-portable-anymap", ".pnm" },
    { "image/x-portable-bitmap", ".pbm" },
    { "image/x-portable-graymap", ".pgm" },
    { "image/x-portable-pixmap", ".ppm" },
    { "image/x-rgb", ".rgb" },
    { "image/x-xbitmap", ".xbm" },
    { "image/x-xpixmap", ".xpm" },
    { "image/x-xwindowdump", ".xwd" },
    { "text/html", ".cfm" },
    { "text/html", ".shtm" },
    { "text/html", ".shtml" },
    { "text/richtext", ".rtx" },
    { "text/tab-separated-values", ".tsv" },
    { "text/x-setext", ".etx" },
    { "video/mpeg", ".mpeg" },
    { "video/mpeg", ".mpg" },
    { "video/mpeg", ".mpe" },
    { "video/quicktime", ".qt" },
    { "video/quicktime", ".mov" },
    { "video/mp4", ".mp4" },
    { "video/x-msvideo", ".avi" },
    { "video/x-sgi-movie", ".movie" },
    { NULL, NULL},
};

/*
    Standard HTTP error codes
 */
static WebsError websErrors[] = {
    { 200, "OK" },
    { 201, "Created" },
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 301, "Redirect" },
    { 302, "Redirect" },
    { 304, "Not Modified" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 402, "Payment required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Access Denied" },
    { 406, "Not Acceptable" },
    { 408, "Request Timeout" },
    { 413, "Request too large" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 503, "Service Unavailable" },
    { 0, NULL }
};

#if ME_GOAHEAD_ACCESS_LOG && !ME_ROM
static char     accessLog[64] = "access.log";       /* Log filename */
static int      accessFd;                           /* Log file handle */
#endif

static WebsHash sessions = -1;
static int      sessionCount = 0;
static int      pruneId;                            /* Callback ID */

/**************************** Forward Declarations ****************************/

static void     checkTimeout(void *arg, int id);
static WebsTime dateParse(WebsTime tip, char *cmd);
static bool     filterChunkData(Webs *wp);
static WebsTime getTimeSinceMark(Webs *wp);
static char     *getToken(Webs *wp, char *delim);
static void     parseFirstLine(Webs *wp);
static void     parseHeaders(Webs *wp);
static bool     processContent(Webs *wp);
static bool     parseIncoming(Webs *wp);
static void     pruneCache();
static void     readEvent(Webs *wp);
static void     reuseConn(Webs *wp);
static void     setFileLimits();
static int      setLocalHost();
static void     socketEvent(int sid, int mask, void *data);
static void     writeEvent(Webs *wp);
#if ME_GOAHEAD_ACCESS_LOG
static void     logRequest(Webs *wp, int code);
#endif

/*********************************** Code *************************************/

PUBLIC int websOpen(char *documents, char *routeFile)
{
    WebsMime    *mt;

    webs = NULL;
    websMax = 0;

    websOsOpen();
    websRuntimeOpen();
    logOpen();
    setFileLimits();
    socketOpen();
    if (setLocalHost() < 0) {
        return -1;
    }
#if ME_COM_SSL
    if (sslOpen() < 0) {
        return -1;
    }
#endif 
    if ((sessions = hashCreate(-1)) < 0) {
        return -1;
    }
    if (!websDebug) {
        pruneId = websStartEvent(WEBS_SESSION_PRUNE, (WebsEventProc) pruneCache, 0);
    }
    if (documents) {
        websSetDocuments(documents);
    }
    if (websOpenRoute() < 0) {
        return -1;
    }
#if ME_GOAHEAD_CGI && !ME_ROM
    websCgiOpen();
#endif
    websOptionsOpen();
    websActionOpen();
    websFileOpen();
#if ME_GOAHEAD_UPLOAD
    websUploadOpen();
#endif
#if ME_GOAHEAD_JAVASCRIPT
    websJstOpen();
#endif
#if ME_GOAHEAD_AUTH
    if (websOpenAuth(0) < 0) {
        return -1;
    }
#endif
    websFsOpen();
    if (websLoad(routeFile) < 0) {
        return -1;
    }
    /*
        Create a mime type lookup table for quickly determining the content type
     */
    websMime = hashCreate(WEBS_HASH_INIT * 4);
    assert(websMime >= 0);
    for (mt = websMimeList; mt->type; mt++) {
        hashEnter(websMime, mt->ext, valueString(mt->type, 0), 0);
    }

#if ME_GOAHEAD_ACCESS_LOG && !ME_ROM
    if ((accessFd = open(accessLog, O_CREAT | O_TRUNC | O_APPEND | O_WRONLY, 0666)) < 0) {
        error("Cannot open access log %s", accessLog);
        return -1;
    }
    /* Some platforms don't implement O_APPEND (VXWORKS) */
    lseek(accessFd, 0, SEEK_END);
#endif
    return 0;
}


PUBLIC void websClose() 
{
    Webs    *wp;
    int     i;

    websCloseRoute();
#if ME_GOAHEAD_AUTH
    websCloseAuth();
#endif
    if (pruneId >= 0) {
        websStopEvent(pruneId);
        pruneId = -1;
    }
    if (sessions >= 0) {
        hashFree(sessions);
        sessions = -1;
    }
    for (i = 0; i < listenMax; i++) {
        if (listens[i] >= 0) {
            socketCloseConnection(listens[i]);
            listens[i] = -1;
        }
    }
    listenMax = 0;
    for (i = websMax; webs && i >= 0; i--) {
        if ((wp = webs[i]) == NULL) {
            continue;
        }
        if (wp->sid >= 0) {
            socketCloseConnection(wp->sid);
            wp->sid = -1;
        }
        websFree(wp);
    }
    wfree(websHostUrl);
    wfree(websIpAddrUrl);
    websIpAddrUrl = websHostUrl = NULL;

#if ME_COM_SSL
    sslClose();
#endif
#if ME_GOAHEAD_ACCESS_LOG
    if (accessFd >= 0) {
        close(accessFd);
        accessFd = -1;
    }
#endif
    websFsClose();
    hashFree(websMime);
    socketClose();
    logClose();
    websRuntimeClose();
    websOsClose();
}


static void initWebs(Webs *wp, int flags, int reuse)
{
    WebsBuf     rxbuf;
    void        *ssl;
    int         wid, sid, timeout;

    assert(wp);

    if (reuse) {
        rxbuf = wp->rxbuf;
        wid = wp->wid;
        sid = wp->sid;
        timeout = wp->timeout;
        ssl = wp->ssl;
    } else {
        wid = sid = -1;
        timeout = -1;
        ssl = 0;
    }
    memset(wp, 0, sizeof(Webs));
    wp->flags = flags;
    wp->state = WEBS_BEGIN;
    wp->wid = wid;
    wp->sid = sid;
    wp->timeout = timeout;
    wp->docfd = -1;
    wp->txLen = -1;
    wp->rxLen = -1;
    wp->code = HTTP_CODE_OK;
    wp->ssl = ssl;
#if !ME_ROM
    wp->putfd = -1;
#endif
#if ME_GOAHEAD_CGI
    wp->cgifd = -1;
#endif
#if ME_GOAHEAD_UPLOAD
    wp->upfd = -1;
#endif
    if (!reuse) {
        wp->timeout = -1;
    }
    wp->vars = hashCreate(WEBS_HASH_INIT);
    /*
        Ring queues can never be totally full and are short one byte. Better to do even I/O and allocate
        a little more memory than required. The chunkbuf has extra room to fit chunk headers and trailers.
     */
    assert(ME_GOAHEAD_LIMIT_BUFFER >= 1024);
    bufCreate(&wp->output, ME_GOAHEAD_LIMIT_BUFFER + 1, ME_GOAHEAD_LIMIT_BUFFER + 1);
    bufCreate(&wp->chunkbuf, ME_GOAHEAD_LIMIT_BUFFER + 1, ME_GOAHEAD_LIMIT_BUFFER * 2);
    bufCreate(&wp->input, ME_GOAHEAD_LIMIT_BUFFER + 1, ME_GOAHEAD_LIMIT_PUT + 1);
    if (reuse) {
        wp->rxbuf = rxbuf;
    } else {
        bufCreate(&wp->rxbuf, ME_GOAHEAD_LIMIT_HEADERS, ME_GOAHEAD_LIMIT_HEADERS + ME_GOAHEAD_LIMIT_PUT);
    }
    wp->rxbuf = wp->rxbuf;
}


static void termWebs(Webs *wp, int reuse)
{
    assert(wp);

    /*
        Some of this is done elsewhere, but keep this here for when a shutdown is done and there are open connections.
     */
    bufFree(&wp->input);
    bufFree(&wp->output);
    bufFree(&wp->chunkbuf);
    if (!reuse) {
        bufFree(&wp->rxbuf);
        if (wp->sid >= 0) {
#if ME_COM_SSL
            sslFree(wp);
#endif
            socketDeleteHandler(wp->sid);
            socketCloseConnection(wp->sid);
            wp->sid = -1;
        }
    }
#if ME_GOAHEAD_CGI
    if (wp->cgifd >= 0) {
        close(wp->cgifd);
        wp->cgifd = -1;
    }
#endif
#if !ME_ROM
    if (wp->putfd >= 0) {
        close(wp->putfd);
        wp->putfd = -1;
        assert(wp->putname && wp->filename);
        if (rename(wp->putname, wp->filename) < 0) {
            error("Cannot rename PUT file from %s to %s", wp->putname, wp->filename);
        }
    }
#endif
    websPageClose(wp);
    if (wp->timeout >= 0 && !reuse) {
        websCancelTimeout(wp);
    }
    wfree(wp->authDetails);
    wfree(wp->authResponse);
    wfree(wp->authType);
    wfree(wp->contentType);
    wfree(wp->cookie);
    wfree(wp->decodedQuery);
    wfree(wp->digest);
    wfree(wp->ext);
    wfree(wp->filename);
    wfree(wp->host);
    wfree(wp->inputFile);
    wfree(wp->method);
    wfree(wp->password);
    wfree(wp->path);
    wfree(wp->protoVersion);
    wfree(wp->putname);
    wfree(wp->query);
    wfree(wp->realm);
    wfree(wp->referrer);
    wfree(wp->responseCookie);
    wfree(wp->url);
    wfree(wp->userAgent);
    wfree(wp->username);
#if ME_GOAHEAD_UPLOAD
    wfree(wp->boundary);
    wfree(wp->uploadTmp);
    wfree(wp->uploadVar);
#endif
#if ME_GOAHEAD_CGI
    wfree(wp->cgiStdin);
#endif
#if ME_DIGEST
    wfree(wp->cnonce);
    wfree(wp->digestUri);
    wfree(wp->opaque);
    wfree(wp->nc);
    wfree(wp->nonce);
    wfree(wp->qop);
#endif
    hashFree(wp->vars);

#if ME_GOAHEAD_UPLOAD
    if (wp->files) {
        websFreeUpload(wp);
    }
#endif
}


PUBLIC int websAlloc(int sid)
{
    Webs    *wp;
    int     wid;

    if ((wid = wallocObject(&webs, &websMax, sizeof(Webs))) < 0) {
        return -1;
    }
    wp = webs[wid];
    assert(wp);
    initWebs(wp, 0, 0);
    wp->wid = wid;
    wp->sid = sid;
    wp->timestamp = time(0);
    return wid;
}


static void reuseConn(Webs *wp)
{
    assert(wp);
    assert(websValid(wp));

    bufCompact(&wp->rxbuf);
    if (bufLen(&wp->rxbuf)) {
        socketReservice(wp->sid);
    }
    termWebs(wp, 1);
    initWebs(wp, wp->flags & (WEBS_KEEP_ALIVE | WEBS_SECURE | WEBS_HTTP11), 1);
}


PUBLIC void websFree(Webs *wp)
{
    assert(wp);
    assert(websValid(wp));

    termWebs(wp, 0);
    websMax = wfreeHandle(&webs, wp->wid);
    wfree(wp);
    assert(websMax >= 0);
}


/*
    Called when the request is complete. Note: it may not have fully drained from the tx buffer.
 */
PUBLIC void websDone(Webs *wp) 
{
    WebsSocket  *sp;

    assert(wp);
    assert(websValid(wp));

    if (wp->flags & WEBS_FINALIZED) {
        return;
    }
    assert(WEBS_BEGIN <= wp->state && wp->state <= WEBS_COMPLETE);
    wp->flags |= WEBS_FINALIZED;

    if (wp->state < WEBS_COMPLETE) {
        /*
            Initiate flush. If not all flushed, wait for output to drain via a socket event.
         */
        if (websFlush(wp, 0) == 0) {
            sp = socketPtr(wp->sid);
            socketCreateHandler(wp->sid, sp->handlerMask | SOCKET_WRITABLE, socketEvent, wp);
        }
    }
#if ME_GOAHEAD_ACCESS_LOG
    logRequest(wp, wp->code);
#endif
    websPageClose(wp);
    if (!(wp->flags & WEBS_RESPONSE_TRACED)) {
        trace(3 | WEBS_RAW_MSG, "Request complete: code %d", wp->code);
    }
}


static void complete(Webs *wp, int reuse) 
{
    assert(wp);
    assert(websValid(wp));
    assert(wp->state == WEBS_BEGIN || wp->state == WEBS_COMPLETE);

    if (reuse && wp->flags & WEBS_KEEP_ALIVE && wp->rxRemaining == 0) {
        reuseConn(wp);
        socketCreateHandler(wp->sid, SOCKET_READABLE, socketEvent, wp);
        trace(5, "Keep connection alive");
        return;
    }
    trace(5, "Close connection");
    assert(wp->timeout >= 0);
    websCancelTimeout(wp);
#if ME_COM_SSL
    sslFree(wp);
#endif
    if (wp->sid >= 0) {
        socketDeleteHandler(wp->sid);
        socketCloseConnection(wp->sid);
        wp->sid = -1;
    }
    bufFlush(&wp->rxbuf);
    wp->state = WEBS_BEGIN;
    wp->flags |= WEBS_CLOSED;
}


PUBLIC int websListen(char *endpoint)
{
    WebsSocket  *sp;
    char        *ip, *ipaddr;
    int         port, secure, sid;

    assert(endpoint && *endpoint);

    if (listenMax >= WEBS_MAX_LISTEN) {
        error("Too many listen endpoints");
        return -1;
    }
    socketParseAddress(endpoint, &ip, &port, &secure, 80);
    if ((sid = socketListen(ip, port, websAccept, 0)) < 0) {
        error("Unable to open socket on port %d.", port);
        return -1;
    }
    sp = socketPtr(sid);
    sp->secure = secure;
    if (sp->secure) {
        if (!defaultSslPort) {
            defaultSslPort = port;
        }
    } else if (!defaultHttpPort) {
        defaultHttpPort = port;
    }
    listens[listenMax++] = sid;
    if (ip) {
        ipaddr = smatch(ip, "::") ? "[::]" : ip;
    } else {
        ipaddr = "*";
    }
    logmsg(2, "Started %s://%s:%d", secure ? "https" : "http", ipaddr, port);

    if (!websHostUrl) {
        if (port == 80) {
            websHostUrl = sclone(ip ? ip : websIpAddr);
        } else {
            websHostUrl = sfmt("%s:%d", ip ? ip : websIpAddr, port);
        }
    }
    if (!websIpAddrUrl) {
        if (port == 80) {
            websIpAddrUrl = sclone(websIpAddr);
        } else {
            websIpAddrUrl = sfmt("%s:%d", websIpAddr, port);
        }
    }
    wfree(ip);
    return sid;
}


/*
    Accept a new connection from ipaddr:port 
 */
PUBLIC int websAccept(int sid, char *ipaddr, int port, int listenSid)
{
    Webs        *wp;
    WebsSocket  *lp;
    struct sockaddr_storage ifAddr;
    int         wid, len;

    assert(sid >= 0);
    assert(ipaddr && *ipaddr);
    assert(listenSid >= 0);
    assert(port >= 0);

    /*
        Allocate a new handle for this accepted connection. This will allocate a Webs structure in the webs[] list
     */
    if ((wid = websAlloc(sid)) < 0) {
        return -1;
    }
    wp = webs[wid];
    assert(wp);
    wp->listenSid = listenSid;
    strncpy(wp->ipaddr, ipaddr, min(sizeof(wp->ipaddr) - 1, strlen(ipaddr)));

    /*
        Get the ip address of the interface that accept the connection.
     */
    len = sizeof(ifAddr);
    if (getsockname(socketList[sid]->sock, (struct sockaddr*) &ifAddr, (Socklen*) &len) < 0) {
        error("Cannot get sockname");
        return -1;
    }
    socketAddress((struct sockaddr*) &ifAddr, (int) len, wp->ifaddr, sizeof(wp->ifaddr), NULL);

#if ME_GOAHEAD_LEGACY
    /*
        Check if this is a request from a browser on this system. This is useful to know for permitting administrative
        operations only for local access 
     */
    if (strcmp(wp->ipaddr, "127.0.0.1") == 0 || strcmp(wp->ipaddr, websIpAddr) == 0 || 
            strcmp(wp->ipaddr, websHost) == 0) {
        wp->flags |= WEBS_LOCAL;
    }
#endif

    /*
        Arrange for socketEvent to be called when read data is available
     */
    lp = socketPtr(listenSid);
    trace(4, "New connection from %s:%d to %s:%d", ipaddr, port, wp->ifaddr, lp->port);

#if ME_COM_SSL
    if (lp->secure) {
        wp->flags |= WEBS_SECURE;
        trace(4, "Upgrade connection to TLS");
        if (sslUpgrade(wp) < 0) {
            error("Cannot upgrade to TLS");
            return -1;
        }
    }
#endif
    assert(wp->timeout == -1);
    wp->timeout = websStartEvent(PARSE_TIMEOUT, checkTimeout, (void*) wp);
    socketEvent(sid, SOCKET_READABLE, wp);
    return 0;
}


/*
    The webs socket handler. Called in response to I/O. We just pass control to the relevant read or write handler. A
    pointer to the webs structure is passed as a (void*) in wptr.  
 */
static void socketEvent(int sid, int mask, void *wptr)
{
    Webs    *wp;

    wp = (Webs*) wptr;
    assert(wp);

    assert(websValid(wp));
    if (! websValid(wp)) {
        return;
    }
    if (mask & SOCKET_READABLE) {
        readEvent(wp);
    } 
    if (mask & SOCKET_WRITABLE) {
        writeEvent(wp);
    } 
    if (wp->flags & WEBS_CLOSED) {
        websFree(wp);
        /* WARNING: wp not valid here */
    }
}


/*
    Read from a connection. Return the number of bytes read if successful. This may be less than the requested "len" and
    may be zero. Return -1 for errors or EOF. Distinguish between error and EOF via socketEof().
 */
static ssize websRead(Webs *wp, char *buf, ssize len)
{
    assert(wp);
    assert(buf);
    assert(len > 0);
#if ME_COM_SSL
    if (wp->flags & WEBS_SECURE) {
        return sslRead(wp, buf, len);
    }
#endif
    return socketRead(wp->sid, buf, len);
}


/*
    The webs read handler. This is the primary read event loop. It uses a state machine to track progress while parsing
    the HTTP request.  Note: we never block as the socket is always in non-blocking mode.
 */
static void readEvent(Webs *wp)
{
    WebsBuf     *rxbuf;
    WebsSocket  *sp;
    ssize       nbytes;

    assert(wp);
    assert(websValid(wp));

    if (!websValid(wp)) {
        return;
    }
    websNoteRequestActivity(wp);
    rxbuf = &wp->rxbuf;

    if (bufRoom(rxbuf) < (ME_GOAHEAD_LIMIT_BUFFER + 1)) {
        if (!bufGrow(rxbuf, ME_GOAHEAD_LIMIT_BUFFER + 1)) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot grow rxbuf");
            websPump(wp);
            return;
        }
    }
    if ((nbytes = websRead(wp, (char*) rxbuf->endp, ME_GOAHEAD_LIMIT_BUFFER)) > 0) {
        wp->lastRead = nbytes;
        bufAdjustEnd(rxbuf, nbytes);
        bufAddNull(rxbuf);
    } 
    if (nbytes > 0 || wp->state > WEBS_BEGIN) {
        websPump(wp);
    }
    if (wp->flags & WEBS_CLOSED) {
        return;
    } else if (nbytes < 0 && socketEof(wp->sid)) {
        /* EOF or error. Allow running requests to continue. */
        if (wp->state < WEBS_READY) {
            if (wp->state > WEBS_BEGIN) {
                websError(wp, HTTP_CODE_COMMS_ERROR, "Read error: connection lost");
                websPump(wp);
            }
            complete(wp, 0);
        } else {
            socketDeleteHandler(wp->sid);
        }
    } else if (wp->state < WEBS_READY) {
        sp = socketPtr(wp->sid);
        socketCreateHandler(wp->sid, sp->handlerMask | SOCKET_READABLE, socketEvent, wp);
    }
}


PUBLIC void websPump(Webs *wp)
{
    bool    canProceed;

    for (canProceed = 1; canProceed; ) {
        switch (wp->state) {
        case WEBS_BEGIN:
            canProceed = parseIncoming(wp);
            break;
        case WEBS_CONTENT:
            canProceed = processContent(wp);
            break;
        case WEBS_READY:
            if (!websRunRequest(wp)) {
                websRouteRequest(wp);
                wp->state = WEBS_READY;
                canProceed = 1;
                continue;
            }
            canProceed = (wp->state != WEBS_RUNNING);
            break;
        case WEBS_RUNNING:
            /* Nothing to do until websDone is called */
            return;
        case WEBS_COMPLETE:
            complete(wp, 1);
            canProceed = bufLen(&wp->rxbuf) != 0;
            break;
        }
    }
}


static bool parseIncoming(Webs *wp)
{
    WebsBuf     *rxbuf;
    char        *end, c;

    rxbuf = &wp->rxbuf;
    while (*rxbuf->servp == '\r' || *rxbuf->servp == '\n') {
        bufGetc(rxbuf);
    }
    if ((end = strstr((char*) wp->rxbuf.servp, "\r\n\r\n")) == 0) {
        if (bufLen(&wp->rxbuf) >= ME_GOAHEAD_LIMIT_HEADER) {
            websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Header too large");
            return 1;
        }
        return 0;
    }    
    trace(3 | WEBS_RAW_MSG, "\n<<< Request\n");
    c = *end;
    *end = '\0';
    trace(3 | WEBS_RAW_MSG, "%s\n", wp->rxbuf.servp);
    *end = c;

    /*
        Parse the first line of the Http header
     */
    parseFirstLine(wp);
    if (wp->state == WEBS_COMPLETE) {
        return 1;
    }
    parseHeaders(wp);
    if (wp->state == WEBS_COMPLETE) {
        return 1;
    }
    wp->state = (wp->rxChunkState || wp->rxLen > 0) ? WEBS_CONTENT : WEBS_READY;

    websRouteRequest(wp);

    if (wp->state == WEBS_COMPLETE) {
        return 1;
    }
#if !ME_ROM
#if ME_GOAHEAD_CGI
    if (strstr(wp->path, ME_GOAHEAD_CGI_BIN) != 0) {
        if (smatch(wp->method, "POST")) {
            wp->cgiStdin = websGetCgiCommName();
            if ((wp->cgifd = open(wp->cgiStdin, O_CREAT | O_WRONLY | O_BINARY | O_TRUNC, 0666)) < 0) {
                websError(wp, HTTP_CODE_NOT_FOUND | WEBS_CLOSE, "Cannot open CGI file");
                return 1;
            }
        }
    }
#endif
    if (smatch(wp->method, "PUT")) {
        WebsStat    sbuf;
        wp->code = (stat(wp->filename, &sbuf) == 0 && sbuf.st_mode & S_IFDIR) ? HTTP_CODE_NO_CONTENT : HTTP_CODE_CREATED;
        wp->putname = websTempFile(ME_GOAHEAD_PUT_DIR, "put");
        if ((wp->putfd = open(wp->putname, O_BINARY | O_WRONLY | O_CREAT | O_BINARY, 0644)) < 0) {
            error("Cannot create PUT filename %s", wp->putname);
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create the put URI");
            wfree(wp->putname);
            return 1;
        }
    }
#endif
    return 1;
}


/*
    Parse the first line of a HTTP request
 */
static void parseFirstLine(Webs *wp)
{
    char    *op, *protoVer, *url, *host, *query, *path, *port, *ext, *buf;
    int     listenPort;

    assert(wp);
    assert(websValid(wp));

    /*
        Determine the request type: GET, HEAD or POST
     */
    op = getToken(wp, 0);
    if (op == NULL || *op == '\0') {
        websError(wp, HTTP_CODE_NOT_FOUND | WEBS_CLOSE, "Bad HTTP request");
        return;
    }
    wp->method = supper(sclone(op));

    url = getToken(wp, 0);
    if (url == NULL || *url == '\0') {
        websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Bad HTTP request");
        return;
    }
    if (strlen(url) > ME_GOAHEAD_LIMIT_URI) {
        websError(wp, HTTP_CODE_REQUEST_URL_TOO_LARGE | WEBS_CLOSE, "URI too big");
        return;
    }
    protoVer = getToken(wp, "\r\n");
    if (websGetLogLevel() == 2) {
        trace(2, "%s %s %s", wp->method, url, protoVer);
    }

    /*
        Parse the URL and store all the various URL components. websUrlParse returns an allocated buffer in buf which we
        must free. We support both proxied and non-proxied requests. Proxied requests will have http://host/ at the
        start of the URL. Non-proxied will just be local path names.
     */
    host = path = port = query = ext = NULL;
    if (websUrlParse(url, &buf, NULL, &host, &port, &path, &ext, NULL, &query) < 0) {
        error("Cannot parse URL: %s", url);
        websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE | WEBS_NOLOG, "Bad URL");
        return;
    }
    if ((wp->path = websValidateUriPath(path)) == 0) {
        error("Cannot normalize URL: %s", url);
        websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE | WEBS_NOLOG, "Bad URL");
        wfree(buf);
        return;
    }
    wp->url = sclone(url);
    if (ext) {
        wp->ext = sclone(slower(ext));
    }
    wp->filename = sfmt("%s%s", websGetDocuments(), wp->path);
    wp->query = sclone(query);
    wp->host = sclone(host);
    wp->protocol = wp->flags & WEBS_SECURE ? "https" : "http";
    if (smatch(protoVer, "HTTP/1.1")) {
        wp->flags |= WEBS_KEEP_ALIVE | WEBS_HTTP11;
    } else if (smatch(protoVer, "HTTP/1.0")) {
        wp->flags &= ~(WEBS_HTTP11);
    } else {
        protoVer = sclone("HTTP/1.1");
        websError(wp, WEBS_CLOSE | HTTP_CODE_NOT_ACCEPTABLE, "Unsupported HTTP protocol");
    }
    wp->protoVersion = sclone(protoVer);
    if ((listenPort = socketGetPort(wp->listenSid)) >= 0) {
        wp->port = listenPort;
    } else {
        wp->port = atoi(port);
    }
    wfree(buf);
}


/*
    Parse a full request
 */
static void parseHeaders(Webs *wp)
{
    char    *upperKey, *cp, *key, *value, *tok;
    int     count;

    assert(websValid(wp));

    /* 
        Parse the header and create the Http header keyword variables
        We rewrite the header as we go for non-local requests.  NOTE: this
        modifies the header string directly and tokenizes each line with '\0'.
    */
    for (count = 0; wp->rxbuf.servp[0] != '\r'; count++) {
        if (count >= ME_GOAHEAD_LIMIT_NUM_HEADERS) {
            websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Too many headers");
            return;
        }
        if ((key = getToken(wp, ":")) == NULL) {
            continue;
        }
        if ((value = getToken(wp, "\r\n")) == NULL) {
            value = "";
        }
        if (!key || !value) {
            websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Bad header format");
            return;
        }
        while (isspace((uchar) *value)) {
            value++;
        }
        slower(key);

        /*
            Create a variable (CGI) for each line in the header
         */
        upperKey = sfmt("HTTP_%s", key);
        for (cp = upperKey; *cp; cp++) {
            if (*cp == '-') {
                *cp = '_';
            }
        }
        supper(upperKey);
        websSetVar(wp, upperKey, value);
        wfree(upperKey);

        /*
            Track the requesting agent (browser) type
         */
        if (strcmp(key, "user-agent") == 0) {
            wp->userAgent = sclone(value);

        } else if (scaselesscmp(key, "authorization") == 0) {
            wp->authType = sclone(value);
            ssplit(wp->authType, " \t", &tok);
            wp->authDetails = sclone(tok);
            slower(wp->authType);

        } else if (strcmp(key, "connection") == 0) {
            slower(value);
            if (strcmp(value, "keep-alive") == 0) {
                wp->flags |= WEBS_KEEP_ALIVE;
            } else if (strcmp(value, "close") == 0) {
                wp->flags &= ~WEBS_KEEP_ALIVE;
            }

        } else if (strcmp(key, "content-length") == 0) {
            wp->rxLen = atoi(value);
            if (smatch(wp->method, "PUT")) {
                if (wp->rxLen > ME_GOAHEAD_LIMIT_PUT) {
                    websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Too big");
                    return;
                }
            } else {
                if (wp->rxLen > ME_GOAHEAD_LIMIT_POST) {
                    websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Too big");
                    return;
                }
            }
            if (wp->rxLen > 0 && !smatch(wp->method, "HEAD")) {
                wp->rxRemaining = wp->rxLen;
            }

        } else if (strcmp(key, "content-type") == 0) {
            wp->contentType = sclone(value);
            if (strstr(value, "application/x-www-form-urlencoded")) {
                wp->flags |= WEBS_FORM;
            } else if (strstr(value, "application/json")) {
                wp->flags |= WEBS_JSON;
            } else if (strstr(value, "multipart/form-data")) {
                wp->flags |= WEBS_UPLOAD;
            }

        } else if (strcmp(key, "cookie") == 0) {
            wp->flags |= WEBS_COOKIE;
            wp->cookie = sclone(value);

        } else if (strcmp(key, "host") == 0) {
            wfree(wp->host);
            wp->host = sclone(value);

        } else if (strcmp(key, "if-modified-since") == 0) {
            char     *cmd;
            WebsTime tip = 0;

            if ((cp = strchr(value, ';')) != NULL) {
                *cp = '\0';
            }
            cmd = sfmt("%s", value);
            wp->since = dateParse(tip, cmd);
            wfree(cmd);

        /*
            Yes Veronica, the HTTP spec does misspell Referrer
         */
        } else if (strcmp(key, "referer") == 0) {
            wp->referrer = sclone(value);

        } else if (strcmp(key, "transfer-encoding") == 0) {
            if (scaselesscmp(value, "chunked") == 0) {
                wp->rxChunkState = WEBS_CHUNK_START;
                wp->rxRemaining = MAXINT;
            }
        }
    }
    if (!wp->rxChunkState) {
        /*
            Step over "\r\n" after headers.
            Don't do this if chunked so that chunking can parse a single chunk delimiter of "\r\nSIZE ...\r\n"
         */
        assert(bufLen(&wp->rxbuf) >= 2);
        wp->rxbuf.servp += 2;
    }
    wp->eof = (wp->rxRemaining == 0);
}


static bool processContent(Webs *wp)
{
    if (!filterChunkData(wp)) {
        return 0;
    }
#if ME_GOAHEAD_CGI && !ME_ROM
    if (wp->cgifd >= 0 && websProcessCgiData(wp) < 0) {
        return 0;
    }
#endif
#if ME_GOAHEAD_UPLOAD
    if ((wp->flags & WEBS_UPLOAD) && websProcessUploadData(wp) < 0) {
        return 0;
    }
#endif
#if !ME_ROM
    if (wp->putfd >= 0 && websProcessPutData(wp) < 0) {
        return 0;
    }
#endif
    if (wp->eof) {
        wp->state = WEBS_READY;
        /* 
            Prevent reading content from the next request 
            The handler may not have been created if all the content was read in the initial read. No matter.
         */
        socketDeleteHandler(wp->sid);
        return 1;
    }
    return 0;
}


/*
    Always called when data is consumed from the input buffer
 */
PUBLIC void websConsumeInput(Webs *wp, ssize nbytes)
{
    assert(wp);
    assert(nbytes >= 0);

    assert(bufLen(&wp->input) >= nbytes);
    if (nbytes <= 0) {
        return;
    }
    bufAdjustStart(&wp->input, nbytes);
    if (bufLen(&wp->input) == 0) {
        bufReset(&wp->input);
    }
}


static bool filterChunkData(Webs *wp)
{
    WebsBuf     *rxbuf;
    ssize       chunkSize;
    char        *start, *cp;
    ssize       len, nbytes;
    bool        added;
    int         bad;

    assert(wp);
    assert(wp->rxbuf.buf);
    rxbuf = &wp->rxbuf;
    added = 0;

    while (bufLen(rxbuf) > 0) {
        switch (wp->rxChunkState) {
        case WEBS_CHUNK_UNCHUNKED:
            len = min(wp->rxRemaining, bufLen(rxbuf));
            bufPutBlk(&wp->input, rxbuf->servp, len);
            bufAddNull(&wp->input);
            bufAdjustStart(rxbuf, len);
            bufCompact(rxbuf);
            wp->rxRemaining -= len;
            if (wp->rxRemaining <= 0) {
                wp->eof = 1;
            }
            assert(wp->rxRemaining >= 0);
            return 1;

        case WEBS_CHUNK_START:
            /*  
                Expect: "\r\nSIZE.*\r\n"
             */
            if (bufLen(rxbuf) < 5) {
                return 0;
            }
            start = rxbuf->servp;
            bad = (start[0] != '\r' || start[1] != '\n');
            for (cp = &start[2]; cp < rxbuf->endp && *cp != '\n'; cp++) {}
            if (*cp != '\n' && (cp - start) < 80) {
                return 0;
            }
            bad += (cp[-1] != '\r' || cp[0] != '\n');
            if (bad) {
                websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
                return 0;
            }
            chunkSize = hextoi(&start[2]);
            if (!isxdigit((uchar) start[2]) || chunkSize < 0) {
                websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
                return 0;
            }
            if (chunkSize == 0) {
                /* On the last chunk, consume the final "\r\n" */
                if ((cp + 2) >= rxbuf->endp) {
                    /* Insufficient data */
                    return 0;
                }
                cp += 2;
                bad += (cp[-1] != '\r' || cp[0] != '\n');
                if (bad) {
                    websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad final chunk specification");
                    return 0;
                }
            }
            bufAdjustStart(rxbuf, cp - start + 1);
            wp->rxChunkSize = chunkSize;
            wp->rxRemaining = chunkSize;
            if (chunkSize == 0) {
#if ME_GOAHEAD_LEGACY
                wp->query = sclone(bufStart(&wp->input));
#endif
                wp->eof = 1;
                return 1;
            }
            trace(7, "chunkFilter: start incoming chunk of %d bytes", chunkSize);
            wp->rxChunkState = WEBS_CHUNK_DATA;
            break;

        case WEBS_CHUNK_DATA:
            len = min(bufLen(rxbuf), wp->rxRemaining);
            nbytes = min(bufRoom(&wp->input), len);
            nbytes = bufPutBlk(&wp->input, rxbuf->servp, nbytes);
            bufAddNull(&wp->input);
            bufAdjustStart(rxbuf, nbytes);
            wp->rxRemaining -= nbytes;
            if (wp->rxRemaining <= 0) {
                wp->rxChunkState = WEBS_CHUNK_START;
                bufCompact(rxbuf);
            }
            added = 1;
            if (nbytes < len) {
                return added;
            }
            break;
        }
    }
    return added;
}


/*
    Basic event loop. SocketReady returns true when a socket is ready for service. SocketSelect will block until an
    event occurs. SocketProcess will actually do the servicing.
 */
PUBLIC void websServiceEvents(int *finished)
{
    WebsTime    delay, nextEvent;

    if (finished) {
        *finished = 0;
    }
    delay = 0;
    while (!finished || !*finished) {
        if (socketSelect(-1, delay)) {
            socketProcess();
        }
#if ME_GOAHEAD_CGI && !ME_ROM
        delay = websCgiPoll();
#else
        delay = MAXINT;
#endif
        nextEvent = websRunEvents();
        delay = min(delay, nextEvent);
    }
}


/*
    NOTE: the vars variable is modified
 */
static void addFormVars(Webs *wp, char *vars)
{
    char  *keyword, *value, *prior, *tok;

    assert(wp);
    assert(vars);

    keyword = stok(vars, "&", &tok);
    while (keyword != NULL) {
        if ((value = strchr(keyword, '=')) != NULL) {
            *value++ = '\0';
            websDecodeUrl(keyword, keyword, strlen(keyword));
            websDecodeUrl(value, value, strlen(value));
        } else {
            value = "";
        }
        if (*keyword) {
            /*
                If keyword has already been set, append the new value to what has been stored.
             */
            if ((prior = websGetVar(wp, keyword, NULL)) != 0) {
                websSetVarFmt(wp, keyword, "%s %s", prior, value);
            } else {
                websSetVar(wp, keyword, value);
            }
        }
        keyword = stok(NULL, "&", &tok);
    }
}


/*
    Set the variable (CGI) environment for this request. Create variables for all standard CGI variables. Also decode
    the query string and create a variable for each name=value pair.
 */
PUBLIC void websSetEnv(Webs *wp)
{
    assert(wp);
    assert(websValid(wp));

    websSetVar(wp, "AUTH_TYPE", wp->authType);
    websSetVarFmt(wp, "CONTENT_LENGTH", "%d", wp->rxLen);
    websSetVar(wp, "CONTENT_TYPE", wp->contentType);
    if (wp->route && wp->route->dir) {
        websSetVar(wp, "DOCUMENT_ROOT", wp->route->dir);
    }
    websSetVar(wp, "GATEWAY_INTERFACE", "CGI/1.1");
    websSetVar(wp, "PATH_INFO", wp->path);
    websSetVar(wp, "PATH_TRANSLATED", wp->filename);
    websSetVar(wp, "QUERY_STRING", wp->query);
    websSetVar(wp, "REMOTE_ADDR", wp->ipaddr);
    websSetVar(wp, "REMOTE_USER", wp->username);
    websSetVar(wp, "REMOTE_HOST", wp->ipaddr);
    websSetVar(wp, "REQUEST_METHOD", wp->method);
    websSetVar(wp, "REQUEST_TRANSPORT", wp->protocol);
    websSetVar(wp, "REQUEST_URI", wp->path);
    websSetVar(wp, "SERVER_ADDR", wp->ifaddr);
    websSetVar(wp, "SERVER_HOST", websHost);
    websSetVar(wp, "SERVER_NAME", websHost);
    websSetVarFmt(wp, "SERVER_PORT", "%d", wp->port);
    websSetVar(wp, "SERVER_PROTOCOL", wp->protoVersion);
    websSetVar(wp, "SERVER_URL", websHostUrl);
    websSetVarFmt(wp, "SERVER_SOFTWARE", "GoAhead/%s", ME_VERSION);
}


PUBLIC void websSetFormVars(Webs *wp)
{
    char    *data;

    if (wp->rxLen > 0 && bufLen(&wp->input) > 0) {
        if (wp->flags & WEBS_FORM) {
            data = sclone(wp->input.servp);
            addFormVars(wp, data);
            wfree(data);
        }
    }
}


PUBLIC void websSetQueryVars(Webs *wp)
{
    /*
        Decode and create an environment query variable for each query keyword. We split into pairs at each '&', then
        split pairs at the '='.  Note: we rely on wp->decodedQuery preserving the decoded values in the symbol table.
     */
    if (wp->query && *wp->query) {
        wp->decodedQuery = sclone(wp->query);
        addFormVars(wp, wp->decodedQuery);
    }
}


/*
    Define a webs (CGI) variable for this connection. Also create in relevant scripting engines. Note: the incoming
    value may be volatile.  
 */
PUBLIC void websSetVarFmt(Webs *wp, char *var, char *fmt, ...)
{
    WebsValue   v;
    va_list     args;

    assert(websValid(wp));
    assert(var && *var);

    if (fmt) {
        va_start(args, fmt);
        v = valueString(sfmtv(fmt, args), 0);
        v.allocated = 1;
        va_end(args);
    } else {
        v = valueString("", 0);
    }
    hashEnter(wp->vars, var, v, 0);
}


PUBLIC void websSetVar(Webs *wp, char *var, char *value)
{
    WebsValue   v;

    assert(websValid(wp));
    assert(var && *var);

    if (value) {
        v = valueString(value, VALUE_ALLOCATE);
    } else {
        v = valueString("", 0);
    }
    hashEnter(wp->vars, var, v, 0);
}


/*
    Return TRUE if a webs variable exists for this connection.
 */
PUBLIC bool websTestVar(Webs *wp, char *var)
{
    WebsKey       *sp;

    assert(websValid(wp));
    assert(var && *var);

    if (var == NULL || *var == '\0') {
        return 0;
    }
    if ((sp = hashLookup(wp->vars, var)) == NULL) {
        return 0;
    }
    return 1;
}


/*
    Get a webs variable but return a default value if string not found.  Note, defaultGetValue can be NULL to permit
    testing existence.  
 */
PUBLIC char *websGetVar(Webs *wp, char *var, char *defaultGetValue)
{
    WebsKey   *sp;

    assert(websValid(wp));
    assert(var && *var);
 
    if ((sp = hashLookup(wp->vars, var)) != NULL) {
        assert(sp->content.type == string);
        if (sp->content.value.string) {
            return sp->content.value.string;
        } else {
            return "";
        }
    }
    return defaultGetValue;
}


/*
    Return TRUE if a webs variable is set to a given value
 */
PUBLIC int websCompareVar(Webs *wp, char *var, char *value)
{
    assert(websValid(wp));
    assert(var && *var);
 
    if (strcmp(value, websGetVar(wp, var, " __UNDEF__ ")) == 0) {
        return 1;
    }
    return 0;
}


/*
    Cancel the request timeout. Note may be called multiple times.
 */
PUBLIC void websCancelTimeout(Webs *wp)
{
    assert(websValid(wp));

    if (wp->timeout >= 0) {
        websStopEvent(wp->timeout);
        wp->timeout = -1;
    }
}


/*
    Output a HTTP response back to the browser. If redirect is set to a URL, the browser will be sent to this location.
 */
PUBLIC void websResponse(Webs *wp, int code, char *message)
{
    ssize   len;
    
    assert(websValid(wp));
    websSetStatus(wp, code);

    if (!smatch(wp->method, "HEAD") && message && *message) {
        len = slen(message);
        websWriteHeaders(wp, len + 2, 0);
        websWriteEndHeaders(wp);
        websWriteBlock(wp, message, len);
        websWriteBlock(wp, "\r\n", 2);
    } else {
        websWriteHeaders(wp, 0, 0);
        websWriteEndHeaders(wp);
    }
    websDone(wp);
}


static char *makeUri(char *scheme, char *host, int port, char *path)
{
    if (port <= 0) {
        port = smatch(scheme, "https") ? defaultSslPort : defaultHttpPort;
    }
    if (port == 80 || port == 443) {
        return sfmt("%s://%s%s", scheme, host, path);
    }
    return sfmt("%s://%s:%d%s", scheme, host, port, path);
}


/*
    Redirect the user to another webs page
 */
PUBLIC void websRedirect(Webs *wp, char *uri)
{
    char    *message, *location, *uribuf, *scheme, *host, *pstr;
    char    hostbuf[ME_GOAHEAD_LIMIT_STRING];
    bool    secure, fullyQualified;
    ssize   len;
    int     originalPort, port;

    assert(websValid(wp));
    assert(uri);
    message = location = uribuf = NULL;

    originalPort = port = 0;
    if ((host = (wp->host ? wp->host : websHostUrl)) != 0) {
        scopy(hostbuf, sizeof(hostbuf), host);
        pstr = strchr(hostbuf, ']');
        pstr = pstr ? pstr : hostbuf;
        if ((pstr = strchr(pstr, ':')) != 0) {
            *pstr++ = '\0';
            originalPort = atoi(pstr);
        }
    }
    if (smatch(uri, "http://") || smatch(uri, "https://")) {
        /* Protocol switch with existing Uri */
        scheme = sncmp(uri, "https", 5) == 0 ? "https" : "http";
        uri = location = makeUri(scheme, hostbuf, 0, wp->url);
    }
    secure = strstr(uri, "https://") != 0;
    fullyQualified = strstr(uri, "http://") || strstr(uri, "https://");
    if (!fullyQualified) {
        port = originalPort;
        if (wp->flags & WEBS_SECURE) {
            secure = 1;
        }
    }
    scheme = secure ? "https" : "http";
    if (port <= 0) {
        port = secure ? defaultSslPort : defaultHttpPort;
    }
    if (strstr(uri, "https:///")) {
        /* Short-hand for redirect to https */
        uri = location = makeUri(scheme, hostbuf, port, &uri[8]);

    } else if (strstr(uri, "http:///")) {
        uri = location = makeUri(scheme, hostbuf, port, &uri[7]);

    } else if (!fullyQualified) {
        uri = location = makeUri(scheme, hostbuf, port, uri);
    }
    message = sfmt("<html><head></head><body>\r\n\
        This document has moved to a new <a href=\"%s\">location</a>.\r\n\
        Please update your documents to reflect the new location.\r\n\
        </body></html>\r\n", uri);
    len = slen(message);
    websSetStatus(wp, HTTP_CODE_MOVED_TEMPORARILY);
    websWriteHeaders(wp, len + 2, uri);
    websWriteEndHeaders(wp);
    websWriteBlock(wp, message, len);
    websWriteBlock(wp, "\r\n", 2);
    websDone(wp);
    wfree(message);
    wfree(location);
    wfree(uribuf);
}


PUBLIC int websRedirectByStatus(Webs *wp, int status)
{
    WebsKey     *key;
    char        code[16], *uri;

    assert(wp);
    assert(status >= 0);

    if (wp->route && wp->route->redirects >= 0) {
        itosbuf(code, sizeof(code), status, 10);
        if ((key = hashLookup(wp->route->redirects, code)) != 0) {
            uri = key->content.value.string;
        } else {
            return -1;
        }
        websRedirect(wp, uri);
    } else {
        if (status == HTTP_CODE_UNAUTHORIZED) {
            websError(wp, status, "Access Denied. User not logged in.");
        } else {
            websError(wp, status, 0);
        }
    }
    return 0;
}


/*  
    Escape HTML to escape defined characters (prevent cross-site scripting)
 */
PUBLIC char *websEscapeHtml(char *html)
{
    char    *ip, *result, *op;
    int     len;

    if (!html) {
        return sclone("");
    }
    for (len = 1, ip = html; *ip; ip++, len++) {
        if (charMatch[(int) (uchar) *ip] & WEBS_ENCODE_HTML) {
            len += 5;
        }
    }
    if ((result = walloc(len)) == 0) {
        return 0;
    }
    /*  
        Leave room for the biggest expansion
     */
    op = result;
    while (*html != '\0') {
        if (charMatch[(uchar) *html] & WEBS_ENCODE_HTML) {
            if (*html == '&') {
                strcpy(op, "&amp;");
                op += 5;
            } else if (*html == '<') {
                strcpy(op, "&lt;");
                op += 4;
            } else if (*html == '>') {
                strcpy(op, "&gt;");
                op += 4;
            } else if (*html == '#') {
                strcpy(op, "&#35;");
                op += 5;
            } else if (*html == '(') {
                strcpy(op, "&#40;");
                op += 5;
            } else if (*html == ')') {
                strcpy(op, "&#41;");
                op += 5;
            } else if (*html == '"') {
                strcpy(op, "&quot;");
                op += 6;
            } else if (*html == '\'') {
                strcpy(op, "&#39;");
                op += 5;
            } else {
                assert(0);
            }
            html++;
        } else {
            *op++ = *html++;
        }
    }
    assert(op < &result[len]);
    *op = '\0';
    return result;
}


/*  
    Output an error message and cleanup
 */
PUBLIC void websError(Webs *wp, int code, char *fmt, ...)
{
    va_list     args;
    char        *msg, *buf;
    char        *encoded;

    assert(wp);
    if (code & WEBS_CLOSE) {
        wp->flags &= ~WEBS_KEEP_ALIVE;
    }
    code &= ~(WEBS_CLOSE | WEBS_NOLOG);
#if !ME_ROM
    if (wp->putfd >= 0) {
        close(wp->putfd);
        wp->putfd = -1;
    }
#endif
    if (wp->rxRemaining && code != 200 && code != 301 && code != 302 && code != 401) {
        /* Close connection so we don't have to consume remaining content */
        wp->flags &= ~WEBS_KEEP_ALIVE;
    }
    encoded = websEscapeHtml(wp->url);
    wfree(wp->url);
    wp->url = encoded;
    if (fmt) {
        va_start(args, fmt);
        msg = sfmtv(fmt, args);
        va_end(args);
        if (!(code & WEBS_NOLOG)) {
            trace(2, "%s", msg);
        }
        encoded = websEscapeHtml(msg);
        wfree(msg);
        msg = encoded;
        buf = sfmt("\
<html>\r\n\
    <head><title>Document Error: %s</title></head>\r\n\
    <body>\r\n\
        <h2>Access Error: %s</h2>\r\n\
        <p>%s</p>\r\n\
    </body>\r\n\
</html>\r\n", websErrorMsg(code), websErrorMsg(code), msg);
        wfree(msg);
    } else {
        buf = 0;
    }
    websResponse(wp, code, buf);
    wfree(buf);
}


/*
    Return the error message for a given code
 */
PUBLIC char *websErrorMsg(int code)
{
    WebsError   *ep;

    assert(code >= 0);
    for (ep = websErrors; ep->code; ep++) {
        if (code == ep->code) {
            return ep->msg;
        }
    }
    return websErrorMsg(HTTP_CODE_INTERNAL_SERVER_ERROR);
}


PUBLIC int websWriteHeader(Webs *wp, char *key, char *fmt, ...)
{
    va_list     vargs;
    char        *buf;
    
    assert(websValid(wp));

    if (!(wp->flags & WEBS_RESPONSE_TRACED)) {
        wp->flags |= WEBS_RESPONSE_TRACED;
        trace(3 | WEBS_RAW_MSG, "\n>>> Response\n");
    }
    if (key) {
        if (websWriteBlock(wp, key, strlen(key)) < 0) {
            return -1;
        }
        if (websWriteBlock(wp, ": ", 2) < 0) {
            return -1;
        }
        trace(3 | WEBS_RAW_MSG, "%s: ", key);
    }
    if (fmt) {
        va_start(vargs, fmt);
        if ((buf = sfmtv(fmt, vargs)) == 0) {
            error("websWrite lost data, buffer overflow");
            return -1;
        }
        va_end(vargs);
        assert(strstr(buf, "UNION") == 0);
        trace(3 | WEBS_RAW_MSG, "%s", buf);
        if (websWriteBlock(wp, buf, strlen(buf)) < 0) {
            return -1;
        }
        wfree(buf);
        if (websWriteBlock(wp, "\r\n", 2) != 2) {
            return -1;
        }
    }
    trace(3 | WEBS_RAW_MSG, "\r\n");
    return 0;
}


PUBLIC void websSetStatus(Webs *wp, int code)
{
    wp->code = code & ~WEBS_CLOSE;
    if (code & WEBS_CLOSE) {
        wp->flags &= ~WEBS_KEEP_ALIVE;
    }
}


/*
    Write a set of headers. Does not write the trailing blank line so callers can add more headers.
    Set length to -1 if unknown and transfer-chunk-encoding will be employed.
 */
PUBLIC void websWriteHeaders(Webs *wp, ssize length, char *location)
{
    WebsKey     *key;
    char        *date;

    assert(websValid(wp));

    if (!(wp->flags & WEBS_HEADERS_CREATED)) {
        if (!wp->protoVersion) {
            wp->protoVersion = sclone("HTTP/1.0");
            wp->flags &= ~WEBS_KEEP_ALIVE;
        }
        websWriteHeader(wp, NULL, "%s %d %s", wp->protoVersion, wp->code, websErrorMsg(wp->code));
        /*
            The Embedthis Open Source license does not permit modification of the Server header
         */
        websWriteHeader(wp, "Server", "GoAhead-http");

        if ((date = websGetDateString(NULL)) != NULL) {
            websWriteHeader(wp, "Date", "%s", date);
            wfree(date);
        }
        if (wp->authResponse) {
            websWriteHeader(wp, "WWW-Authenticate", "%s", wp->authResponse);
        }
        if (smatch(wp->method, "HEAD")) {
            websWriteHeader(wp, "Content-Length", "%d", (int) length);                                           
        } else if (length >= 0) {                                                                                    
            if (!((100 <= wp->code && wp->code <= 199) || wp->code == 204 || wp->code == 304)) {
                websWriteHeader(wp, "Content-Length", "%d", (int) length);                                           
            }
        }
        wp->txLen = length;
        if (wp->txLen < 0) {
            websWriteHeader(wp, "Transfer-Encoding", "chunked");
        }
        if (wp->flags & WEBS_KEEP_ALIVE) {
            websWriteHeader(wp, "Connection", "keep-alive");
        } else {
            websWriteHeader(wp, "Connection", "close");   
        }
        if (location) {
            websWriteHeader(wp, "Location", "%s", location);
        } else if ((key = hashLookup(websMime, wp->ext)) != 0) {
            websWriteHeader(wp, "Content-Type", "%s", key->content.value.string);
        }
        if (wp->responseCookie) {
            websWriteHeader(wp, "Set-Cookie", "%s", wp->responseCookie);
            websWriteHeader(wp, "Cache-Control", "%s", "no-cache=\"set-cookie\"");
        }
#if defined(ME_GOAHEAD_CLIENT_CACHE)
        if (wp->ext) {
            char *etok = sfmt("%s,", &wp->ext[1]);
            if (strstr(ME_GOAHEAD_CLIENT_CACHE ",", etok)) {
                websWriteHeader(wp, "Cache-Control", "public, max-age=%d", ME_GOAHEAD_CLIENT_CACHE_LIFESPAN);
            }
            wfree(etok);
        }
#endif
#ifdef UNUSED_ME_GOAHEAD_XFRAME_HEADER
        if (*ME_GOAHEAD_XFRAME_HEADER) {
            websWriteHeader(wp, "X-Frame-Options", "%s", ME_GOAHEAD_XFRAME_HEADER);
        }
#endif
    }
}


PUBLIC void websWriteEndHeaders(Webs *wp)
{
    assert(wp);
    /*
        By omitting the "\r\n" delimiter after the headers, chunks can emit "\r\nSize\r\n" as a single chunk delimiter
     */
    if (wp->txLen >= 0) {
        websWriteBlock(wp, "\r\n", 2);
    }
    wp->flags |= WEBS_HEADERS_CREATED;
    if (wp->txLen < 0) {
        wp->flags |= WEBS_CHUNKING;
    }
}


PUBLIC void websSetTxLength(Webs *wp, ssize length)
{
    assert(wp);
    wp->txLen = length;
}


/*
    Do formatted output to the browser. This is the public Javascript and form write procedure.
 */
PUBLIC ssize websWrite(Webs *wp, char *fmt, ...)
{
    va_list     vargs;
    char        *buf;
    ssize       rc;
    
    assert(websValid(wp));
    assert(fmt && *fmt);

    va_start(vargs, fmt);

    buf = NULL;
    rc = 0;
    if ((buf = sfmtv(fmt, vargs)) == 0) {
        error("websWrite lost data, buffer overflow");
    }
    va_end(vargs);
    assert(buf);
    if (buf) {
        rc = websWriteBlock(wp, buf, strlen(buf));
        wfree(buf);
    }
    return rc;
}


/*
    Non-blocking write to socket. 
    Returns number of bytes written. Returns -1 on errors. May return short.
 */
PUBLIC ssize websWriteSocket(Webs *wp, char *buf, ssize size)
{
    ssize   written;

    assert(wp);
    assert(buf);
    assert(size >= 0);

    if (wp->flags & WEBS_CLOSED) {
        return -1;
    }
#if ME_COM_SSL
    if (wp->flags & WEBS_SECURE) {
        if ((written = sslWrite(wp, buf, size)) < 0) {
            return -1;
        }
    } else 
#endif
    if ((written = socketWrite(wp->sid, buf, size)) < 0) {
        return -1;
    }
    wp->written += written;
    websNoteRequestActivity(wp);
    return written;
}


/*
    Write some output using transfer chunk encoding if required.
    Returns true if all the data was written. Otherwise return zero.
 */
static bool flushChunkData(Webs *wp)
{
    ssize   len, written, room;

    assert(wp);

    while (bufLen(&wp->chunkbuf) > 0) {
        /*
            Stop if there is not room for a reasonable size chunk.
            Subtract 16 to allow for the final trailer.
         */
        if ((room = bufRoom(&wp->output) - 16) <= CHUNK_LOW) {
            bufGrow(&wp->output, CHUNK_LOW - room + 1);
            if ((room = bufRoom(&wp->output) - 16) <= CHUNK_LOW) {
                return 0;
            }
        }
        switch (wp->txChunkState) {
        default:
        case WEBS_CHUNK_START:
            /* Select the chunk size so that both the prefix and data will fit */
            wp->txChunkLen = min(bufLen(&wp->chunkbuf), room - 16);
            fmt(wp->txChunkPrefix, sizeof(wp->txChunkPrefix), "\r\n%x\r\n", wp->txChunkLen);
            wp->txChunkPrefixLen = slen(wp->txChunkPrefix);
            wp->txChunkPrefixNext = wp->txChunkPrefix;
            wp->txChunkState = WEBS_CHUNK_HEADER;
            break;

        case WEBS_CHUNK_HEADER:
            if ((written = bufPutBlk(&wp->output, wp->txChunkPrefixNext, wp->txChunkPrefixLen)) < 0) {
                return 0;
            } else {
                wp->txChunkPrefixNext += written;
                wp->txChunkPrefixLen -= written;
                if (wp->txChunkPrefixLen <= 0) {
                    wp->txChunkState = WEBS_CHUNK_DATA;
                } else {
                    return 0;
                }
            }
            break;

        case WEBS_CHUNK_DATA:
            if (wp->txChunkLen > 0) {
                len = min(room, wp->txChunkLen);
                if ((written = bufPutBlk(&wp->output, wp->chunkbuf.servp, len)) != len) {
                    assert(0);
                    return -1;
                }
                bufAdjustStart(&wp->chunkbuf, written);
                wp->txChunkLen -= written;
                if (wp->txChunkLen <= 0) {
                    wp->txChunkState = WEBS_CHUNK_START;
                    bufCompact(&wp->chunkbuf);
                }
                bufAddNull(&wp->output);
            }
        }
    }
    return bufLen(&wp->chunkbuf) == 0;
}


/*
    Initiate flushing output buffer. Returns true if all data is written to the socket and the buffer is empty.
    Returns <  0 for errors
            == 0 if there is output remaining to be flushed
            == 1 if the output was fully written to the socket
 */
PUBLIC int websFlush(Webs *wp, bool block)
{
    WebsBuf     *op;
    ssize       nbytes, written;
    int         wasBlocking;

    if (block) {
        wasBlocking = socketSetBlock(wp->sid, 1);
    }
    op = &wp->output;
    if (wp->flags & WEBS_CHUNKING) {
        trace(6, "websFlush chunking finalized %d", wp->flags & WEBS_FINALIZED);
        if (flushChunkData(wp) && wp->flags & WEBS_FINALIZED) {
            trace(6, "websFlush: write chunk trailer");
            bufPutStr(op, "\r\n0\r\n\r\n");
            bufAddNull(op);
            wp->flags &= ~WEBS_CHUNKING;
        }
    }
    trace(6, "websFlush: buflen %d", bufLen(op));
    written = 0;
    while ((nbytes = bufLen(op)) > 0) {
        if ((written = websWriteSocket(wp, op->servp, nbytes)) < 0) {
            wp->flags &= ~WEBS_KEEP_ALIVE;
            bufFlush(op);
            wp->state = WEBS_COMPLETE;
            break;
        } else if (written == 0) {
            break;
        }
        trace(6, "websFlush: wrote %d to socket", written);
        bufAdjustStart(op, written);
        bufCompact(op);
        nbytes = bufLen(op);
    }
    assert(websValid(wp));

    if (bufLen(op) == 0 && wp->flags & WEBS_FINALIZED) {
        wp->state = WEBS_COMPLETE;
    }
    if (block) {
        socketSetBlock(wp->sid, wasBlocking);
    }
    if (written < 0) {
        return -1;
    }
    return bufLen(op) == 0;
}


/*
    Respond to a writable event. First write any tx buffer by calling websFlush.
    Then write body data if writeProc is defined. If all written, ensure transition to complete state.
    Calls websPump() to advance state.
 */
static void writeEvent(Webs *wp)
{
    WebsBuf     *op;

    op = &wp->output;
    if (bufLen(op) > 0) {
        websFlush(wp, 0);
    }
    if (bufLen(op) == 0 && wp->writeData) {
        (wp->writeData)(wp);
    }
    if (wp->state != WEBS_RUNNING) {
        websPump(wp);
    }
}


PUBLIC void websSetBackgroundWriter(Webs *wp, WebsWriteProc proc)
{
    WebsSocket  *sp;
    WebsBuf     *op;

    assert(proc);

    wp->writeData = proc;
    op = &wp->output;

    if (bufLen(op) > 0) {
        websFlush(wp, 0);
    }
    if (bufLen(op) == 0) {
        (wp->writeData)(wp);
    }
    if (wp->sid >= 0 && wp->state < WEBS_COMPLETE) {
        sp = socketPtr(wp->sid);
        socketCreateHandler(wp->sid, sp->handlerMask | SOCKET_WRITABLE, socketEvent, wp);
    }
}


/*
    Accessors
 */
PUBLIC char *websGetCookie(Webs *wp) { return wp->cookie; }
PUBLIC char *websGetDir(Webs *wp) { return wp->route && wp->route->dir ? wp->route->dir : websGetDocuments(); }
PUBLIC int  websGetEof(Webs *wp) { return wp->eof; }
PUBLIC char *websGetExt(Webs *wp) { return wp->ext; }
PUBLIC char *websGetFilename(Webs *wp) { return wp->filename; }
PUBLIC char *websGetHost(Webs *wp) { return wp->host; }
PUBLIC char *websGetIfaddr(Webs *wp) { return wp->ifaddr; }
PUBLIC char *websGetIpaddr(Webs *wp) { return wp->ipaddr; }
PUBLIC char *websGetMethod(Webs *wp) { return wp->method; }
PUBLIC char *websGetPassword(Webs *wp) { return wp->password; }
PUBLIC char *websGetPath(Webs *wp) { return wp->path; }
PUBLIC int   websGetPort(Webs *wp) { return wp->port; }
PUBLIC char *websGetProtocol(Webs *wp) { return wp->protocol; }
PUBLIC char *websGetQuery(Webs *wp) { return wp->query; }
PUBLIC char *websGetServer() { return websHost; } 
PUBLIC char *websGetServerAddress() { return websIpAddr; } 
PUBLIC char *websGetServerAddressUrl() { return websIpAddrUrl; } 
PUBLIC char *websGetServerUrl() { return websHostUrl; }
PUBLIC char *websGetUrl(Webs *wp) { return wp->url; }
PUBLIC char *websGetUserAgent(Webs *wp) { return wp->userAgent; }
PUBLIC char *websGetUsername(Webs *wp) { return wp->username; }

/*
    Write a block of data of length to the user's browser. Output is buffered and flushed via websFlush.
    This routine will never return "short". i.e. it will return the requested size to write or -1.
    Buffer data. Will flush as required. May return -1 on write errors.
 */
PUBLIC ssize websWriteBlock(Webs *wp, char *buf, ssize size)
{
    WebsBuf     *op;
    ssize       written, thisWrite, len, room;

    assert(wp);
    assert(websValid(wp));
    assert(buf);
    assert(size >= 0);

    if (wp->state >= WEBS_COMPLETE) {
        return -1;
    }
    op = (wp->flags & WEBS_CHUNKING) ? &wp->chunkbuf : &wp->output;
    written = len = 0;

    while (size > 0 && wp->state < WEBS_COMPLETE) {  
        if (bufRoom(op) < size) {
            /*
                This will do a blocking I/O write. Will only ever fail for I/O errors.
             */
            if (websFlush(wp, 1) < 0) {
                return -1;
            }
        }
        if ((room = bufRoom(op)) == 0) {
            break;
        }
        thisWrite = min(room, size);
        bufPutBlk(op, buf, thisWrite);
        size -= thisWrite;
        buf += thisWrite;
        written += thisWrite;
    }
    bufAddNull(op);
    if (wp->state >= WEBS_COMPLETE && written == 0) {
        return -1;
    }
    return written;
}


/*
    Decode a URL (or part thereof). Allows insitu decoding.
 */
PUBLIC void websDecodeUrl(char *decoded, char *input, ssize len)
{
    char    *ip,  *op;
    int     num, i, c;
    
    assert(decoded);
    assert(input);

    if (len < 0) {
        len = strlen(input);
    }
    op = decoded;
    for (ip = input; *ip && len > 0; ip++, op++) {
        if (*ip == '+') {
            *op = ' ';
        } else if (*ip == '%' && isxdigit((uchar) ip[1]) && isxdigit((uchar) ip[2])) {
            /*
                Convert %nn to a single character
             */
            ip++;
            for (i = 0, num = 0; i < 2; i++, ip++) {
                c = tolower((uchar) *ip);
                if (c >= 'a' && c <= 'f') {
                    num = (num * 16) + 10 + c - 'a';
                } else {
                    num = (num * 16) + c - '0';
                }
            }
            *op = (char) num;
            ip--;

        } else {
            *op = *ip;
        }
        len--;
    }
    *op = '\0';
}


#if ME_GOAHEAD_ACCESS_LOG && !ME_ROM
/*
    Output a log message in Common Log Format: See http://httpd.apache.org/docs/1.3/logs.html#common
 */
static void logRequest(Webs *wp, int code)
{
    char        *buf, timeStr[28], zoneStr[6], dataStr[16];
    ssize       len;
    WebsTime    timer;
    struct tm   localt;
#if WINDOWS
    DWORD       dwRet;
    TIME_ZONE_INFORMATION tzi;
#endif

    assert(wp);
    time(&timer);
#if WINDOWS
    localtime_s(&localt, &timer);
#else
    localtime_r(&timer, &localt);
#endif
    strftime(timeStr, sizeof(timeStr), "%d/%b/%Y:%H:%M:%S", &localt); 
    timeStr[sizeof(timeStr) - 1] = '\0';
#if WINDOWS
    dwRet = GetTimeZoneInformation(&tzi);
    fmt(zoneStr, sizeof(zoneStr), "%+03d00", -(int) (tzi.Bias/60));
#elif !VXWORKS
    fmt(zoneStr, sizeof(zoneStr), "%+03d00", (int) (localt.tm_gmtoff/3600));
#else
    zoneStr[0] = '\0';
#endif
    zoneStr[sizeof(zoneStr) - 1] = '\0';
    if (wp->written != 0) {
        fmt(dataStr, sizeof(dataStr), "%Ld", wp->written);
        dataStr[sizeof(dataStr) - 1] = '\0';
    } else {
        dataStr[0] = '-'; dataStr[1] = '\0';
    }
    buf = NULL;
    buf = sfmt("%s - %s [%s %s] \"%s %s %s\" %d %s\n", 
        wp->ipaddr, wp->username == NULL ? "-" : wp->username,
        timeStr, zoneStr, wp->method, wp->path, wp->protoVersion, code, dataStr);
    len = strlen(buf);
    write(accessFd, buf, len);
    wfree(buf);
}
#endif


/*
    Request and connection timeout. The timeout triggers if we have not read any data from the users browser in the last 
    WEBS_TIMEOUT period. If we have heard from the browser, simply re-issue the timeout.
 */
static void checkTimeout(void *arg, int id)
{
    Webs        *wp;
    WebsTime    elapsed, delay;

    wp = (Webs*) arg;
    assert(websValid(wp));

    elapsed = getTimeSinceMark(wp) * 1000;
    if (websDebug) {
        websRestartEvent(id, (int) WEBS_TIMEOUT);
        return;
    } 
    if (wp->state == WEBS_BEGIN) {
        // Idle connection websError(wp, HTTP_CODE_REQUEST_TIMEOUT | WEBS_CLOSE, "Request exceeded parse timeout");
        complete(wp, 0);
        websFree(wp);
        return;
    }
    if (elapsed >= WEBS_TIMEOUT) {
        if (!(wp->flags & WEBS_HEADERS_CREATED)) {
            if (wp->state > WEBS_BEGIN) {
                websError(wp, HTTP_CODE_REQUEST_TIMEOUT, "Request exceeded timeout");
            } else {
                websError(wp, HTTP_CODE_REQUEST_TIMEOUT, "Idle connection closed");
            }
        }
        wp->state = WEBS_COMPLETE;
        complete(wp, 0);
        websFree(wp);
        /* WARNING: wp not valid here */
        return;
    }
    delay = WEBS_TIMEOUT - elapsed;
    assert(delay > 0);
    websRestartEvent(id, (int) delay);
}


static int setLocalHost()
{
    struct in_addr  intaddr;
    char            host[128], *ipaddr;

    if (gethostname(host, sizeof(host)) < 0) {
        error("Cannot get hostname: errno %d", errno);
        return -1;
    }
#if VXWORKS
    intaddr.s_addr = (ulong) hostGetByName(host);
    ipaddr = inet_ntoa(intaddr);
    websSetIpAddr(ipaddr);
    websSetHost(ipaddr);
    #if _WRS_VXWORKS_MAJOR < 6
        free(ipaddr);
    #endif
#elif ECOS
    ipaddr = inet_ntoa(eth0_bootp_data.bp_yiaddr);
    websSetIpAddr(ipaddr);
    websSetHost(ipaddr);
#elif TIDSP
{
    struct hostent  *hp;
    if ((hp = gethostbyname(host)) == NULL) {
        error("Cannot get host address for host %s: errno %d", host, errno);
        return -1;
    }
    memcpy((char*) &intaddr, (char *) hp->h_addr[0], (size_t) hp->h_length);
    ipaddr = inet_ntoa(intaddr);
    websSetIpAddr(ipaddr);
    websSetHost(ipaddr);
}
#elif MACOSX
{
    struct hostent  *hp;
    if ((hp = gethostbyname(host)) == NULL) {
        if ((hp = gethostbyname(sfmt("%s.local", host))) == NULL) {
            error("Cannot get host address for host %s: errno %d", host, errno);
            return -1;
        }
    }
    memcpy((char*) &intaddr, (char *) hp->h_addr_list[0], (size_t) hp->h_length);
    ipaddr = inet_ntoa(intaddr);
    websSetIpAddr(ipaddr);
    websSetHost(ipaddr);
}
#else
{
    struct hostent  *hp;
    if ((hp = gethostbyname(host)) == NULL) {
        error("Cannot get host address for host %s: errno %d", host, errno);
        return -1;
    }
    memcpy((char*) &intaddr, (char *) hp->h_addr_list[0], (size_t) hp->h_length);
    ipaddr = inet_ntoa(intaddr);
    websSetIpAddr(ipaddr);
    websSetHost(ipaddr);
}
#endif
    return 0;
}


PUBLIC void websSetHost(char *host)
{
    scopy(websHost, sizeof(websHost), host);
}


PUBLIC void websSetHostUrl(char *url)
{
    assert(url && *url);

    wfree(websHostUrl);
    websHostUrl = sclone(url);
}


PUBLIC void websSetIpAddr(char *ipaddr)
{
    assert(ipaddr && *ipaddr);
    scopy(websIpAddr, sizeof(websIpAddr), ipaddr);
}


#if ME_GOAHEAD_LEGACY
PUBLIC void websSetRequestFilename(Webs *wp, char *filename)
{
    assert(websValid(wp));
    assert(filename && *filename);

    if (wp->filename) {
        wfree(wp->filename);
    }
    wp->filename = sclone(filename);
    websSetVar(wp, "PATH_TRANSLATED", wp->filename);
}
#endif


PUBLIC int websRewriteRequest(Webs *wp, char *url)
{
    char    *buf, *path;

    wfree(wp->url);
    wp->url = sclone(url);
    wfree(wp->path);
    wp->path = 0;

    if (websUrlParse(url, &buf, NULL, NULL, NULL, &path, NULL, NULL, NULL) < 0) {
        return -1;
    }
    wp->path = sclone(path);
    wfree(wp->filename);
    wp->filename = 0;
    wp->flags |= WEBS_REROUTE;
    wfree(buf);
    return 0;
}


PUBLIC bool websValid(Webs *wp)
{
    int     wid;

    for (wid = 0; wid < websMax; wid++) {
        if (wp == webs[wid]) {
            return 1;
        }
    }
    return 0;
}


/*
    Build an ASCII time string.  If sbuf is NULL we use the current time, else we use the last modified time of sbuf;
 */
PUBLIC char *websGetDateString(WebsFileInfo *sbuf)
{
    WebsTime    now;
    struct tm   tm;
    char        *cp;

    if (sbuf == NULL) {
        time(&now);
    } else {
        now = sbuf->mtime;
    }
#if ME_UNIX_LIKE
    gmtime_r(&now, &tm);
#else
    {
        struct tm *tp;
        tp = gmtime(&now);
        tm = *tp;
    }
#endif
    if ((cp = asctime(&tm)) != NULL) {
        cp[strlen(cp) - 1] = '\0';
        return sclone(cp);
    }
    return NULL;
}


/*
    Take not of the request activity and mark the time. Set a timestamp so that, later, we can return the number of seconds
    since we made the mark.
 */
PUBLIC void websNoteRequestActivity(Webs *wp)
{
    wp->timestamp = time(0);
}


/*
    Get the number of seconds since the last mark.
 */
static WebsTime getTimeSinceMark(Webs *wp)
{
    return time(0) - wp->timestamp;
}


/*  
    These functions are intended to closely mirror the syntax for HTTP-date 
    from RFC 2616 (HTTP/1.1 spec).  This code was submitted by Pete Berstrom.
    
    RFC1123Date = wkday "," SP date1 SP time SP "GMT"
    RFC850Date  = weekday "," SP date2 SP time SP "GMT"
    ASCTimeDate = wkday SP date3 SP time SP 4DIGIT
  
    Each of these functions tries to parse the value and update the index to 
    the point it leaves off parsing.
 */

typedef enum { JAN, FEB, MAR, APR, MAY, JUN, JUL, AUG, SEP, OCT, NOV, DEC } MonthEnumeration;
typedef enum { SUN, MON, TUE, WED, THU, FRI, SAT } WeekdayEnumeration;

/*  
    Parse an N-digit value
 */

static int parseNDIGIT(char *buf, int digits, int *index) 
{
    int tmpIndex, returnValue;

    returnValue = 0;
    for (tmpIndex = *index; tmpIndex < *index+digits; tmpIndex++) {
        if (isdigit((uchar) buf[tmpIndex])) {
            returnValue = returnValue * 10 + (buf[tmpIndex] - '0');
        }
    }
    *index = tmpIndex;
    return returnValue;
}


/*
    Return an index into the month array
 */

static int parseMonth(char *buf, int *index) 
{
    /*  
        "jan" | "feb" | "mar" | "apr" | "may" | "jun" | 
        "jul" | "aug" | "sep" | "oct" | "nov" | "dec"
     */
    int tmpIndex, returnValue;
    returnValue = -1;
    tmpIndex = *index;

    switch (buf[tmpIndex]) {
        case 'a':
            switch (buf[tmpIndex+1]) {
                case 'p':
                    returnValue = APR;
                    break;
                case 'u':
                    returnValue = AUG;
                    break;
            }
            break;
        case 'd':
            returnValue = DEC;
            break;
        case 'f':
            returnValue = FEB;
            break;
        case 'j':
            switch (buf[tmpIndex+1]) {
                case 'a':
                    returnValue = JAN;
                    break;
                case 'u':
                    switch (buf[tmpIndex+2]) {
                        case 'l':
                            returnValue = JUL;
                            break;
                        case 'n':
                            returnValue = JUN;
                            break;
                    }
                    break;
            }
            break;
        case 'm':
            switch (buf[tmpIndex+1]) {
                case 'a':
                    switch (buf[tmpIndex+2]) {
                        case 'r':
                            returnValue = MAR;
                            break;
                        case 'y':
                            returnValue = MAY;
                            break;
                    }
                    break;
            }
            break;
        case 'n':
            returnValue = NOV;
            break;
        case 'o':
            returnValue = OCT;
            break;
        case 's':
            returnValue = SEP;
            break;
    }
    if (returnValue >= 0) {
        *index += 3;
    }
    return returnValue;
}


/* 
    Parse a year value (either 2 or 4 digits)
 */
static int parseYear(char *buf, int *index) 
{
    int tmpIndex, returnValue;

    tmpIndex = *index;
    returnValue = parseNDIGIT(buf, 4, &tmpIndex);

    if (returnValue >= 0) {
        *index = tmpIndex;
    } else {
        returnValue = parseNDIGIT(buf, 2, &tmpIndex);
        if (returnValue >= 0) {
            /*
                Assume that any year earlier than the start of the epoch for WebsTime (1970) specifies 20xx
             */
            if (returnValue < 70) {
                returnValue += 2000;
            } else {
                returnValue += 1900;
            }
            *index = tmpIndex;
        }
    }
    return returnValue;
}


/* 
    The formulas used to build these functions are from "Calendrical Calculations", by Nachum Dershowitz, Edward M.
    Reingold, Cambridge University Press, 1997.  
 */
static const int GregorianEpoch = 1;

/*
    Determine if year is a leap year
 */
PUBLIC int GregorianLeapYearP(long year) 
{
    long    tmp;
    
    tmp = year % 400;
    return (year % 4 == 0) && (tmp != 100) && (tmp != 200) && (tmp != 300);
}


/*
    Return the fixed date from the gregorian date
 */
long FixedFromGregorian(long month, long day, long year) 
{
    long fixedDate;

    fixedDate = (long)(GregorianEpoch - 1 + 365 * (year - 1) + 
        floor((year - 1) / 4.0) -
        floor((double)(year - 1) / 100.0) + 
        floor((double)(year - 1) / 400.0) + 
        floor((367.0 * ((double)month) - 362.0) / 12.0));

    if (month <= 2) {
        fixedDate += 0;
    } else if (GregorianLeapYearP(year)) {
        fixedDate += -1;
    } else {
        fixedDate += -2;
    }
    fixedDate += day;
    return fixedDate;
}


/*
    Return the gregorian year from a fixed date
 */
long GregorianYearFromFixed(long fixedDate) 
{
    long result, d0, n400, d1, n100, d2, n4, d3, n1, year;

    d0 =    fixedDate - GregorianEpoch;
    n400 =  (long)(floor((double)d0 / (double)146097));
    d1 =    d0 % 146097;
    n100 =  (long)(floor((double)d1 / (double)36524));
    d2 =    d1 % 36524;
    n4 =    (long)(floor((double)d2 / (double)1461));
    d3 =    d2 % 1461;
    n1 =    (long)(floor((double)d3 / (double)365));
    year =  400 * n400 + 100 * n100 + 4 * n4 + n1;

    if ((n100 == 4) || (n1 == 4)) {
        result = year;
    } else {
        result = year + 1;
    }
    return result;
}


/* 
    Returns the Gregorian date from a fixed date (not needed for this use, but included for completeness)
 */
#if KEEP
PUBLIC void GregorianFromFixed(long fixedDate, long *month, long *day, long *year) 
{
    long priorDays, correction;

    *year =         GregorianYearFromFixed(fixedDate);
    priorDays =     fixedDate - FixedFromGregorian(1, 1, *year);

    if (fixedDate < FixedFromGregorian(3,1,*year)) {
        correction = 0;
    } else if (true == GregorianLeapYearP(*year)) {
        correction = 1;
    } else {
        correction = 2;
    }
    *month = (long)(floor((12.0 * (double)(priorDays + correction) + 373.0) / 367.0));
    *day = fixedDate - FixedFromGregorian(*month, 1, *year);
}
#endif


/* 
    Returns the difference between two Gregorian dates
 */
long GregorianDateDifferenc(long month1, long day1, long year1, long month2, long day2, long year2) 
{
    return FixedFromGregorian(month2, day2, year2) - FixedFromGregorian(month1, day1, year1);
}


/*
    Return the number of seconds into the current day
 */
static int parseTime(char *buf, int *index) 
{
    /*  
        Format of buf is - 2DIGIT ":" 2DIGIT ":" 2DIGIT
     */
    int returnValue, tmpIndex, hourValue, minuteValue, secondValue;

    hourValue = minuteValue = secondValue = -1;
    returnValue = -1;
    tmpIndex = *index;

    hourValue = parseNDIGIT(buf, 2, &tmpIndex);

    if (hourValue >= 0) {
        tmpIndex++;
        minuteValue = parseNDIGIT(buf, 2, &tmpIndex);
        if (minuteValue >= 0) {
            tmpIndex++;
            secondValue = parseNDIGIT(buf, 2, &tmpIndex);
        }
    }
    if ((hourValue >= 0) && (minuteValue >= 0) && (secondValue >= 0)) {
        returnValue = (((hourValue * 60) + minuteValue) * 60) + secondValue;
        *index = tmpIndex;
    }
    return returnValue;
}


#define SECONDS_PER_DAY 24*60*60

/*
    Return the equivalent of time() given a gregorian date
 */
static WebsTime dateToTimet(int year, int month, int day) 
{
    long    dayDifference;

    dayDifference = FixedFromGregorian(month + 1, day, year) - FixedFromGregorian(1, 1, 1970);
    return dayDifference * SECONDS_PER_DAY;
}


/*
    Return the number of seconds between Jan 1, 1970 and the parsed date (corresponds to documentation for time() function)
 */
static WebsTime parseDate1or2(char *buf, int *index) 
{
    /*  
        Format of buf is either
            2DIGIT SP month SP 4DIGIT
        or
            2DIGIT "-" month "-" 2DIGIT
     */
    WebsTime    returnValue;
    int         dayValue, monthValue, yearValue, tmpIndex;

    returnValue = (WebsTime) -1;
    tmpIndex = *index;

    dayValue = monthValue = yearValue = -1;

    if (buf[tmpIndex] == ',') {
        /* 
            Skip over the ", " 
         */
        tmpIndex += 2; 

        dayValue = parseNDIGIT(buf, 2, &tmpIndex);
        if (dayValue >= 0) {
            /*
                Skip over the space or hyphen
             */
            tmpIndex++; 
            monthValue = parseMonth(buf, &tmpIndex);
            if (monthValue >= 0) {
                /*
                    Skip over the space or hyphen
                 */
                tmpIndex++; 
                yearValue = parseYear(buf, &tmpIndex);
            }
        }

        if ((dayValue >= 0) &&
            (monthValue >= 0) &&
            (yearValue >= 0)) {
            if (yearValue < 1970) {
                /*              
                    Allow for Microsoft IE's year 1601 dates 
                 */
                returnValue = 0; 
            } else {
                returnValue = dateToTimet(yearValue, monthValue, dayValue);
            }
            *index = tmpIndex;
        }
    }
    return returnValue;
}


/*
    Return the number of seconds between Jan 1, 1970 and the parsed date
 */
static WebsTime parseDate3Time(char *buf, int *index) 
{
    /*
        Format of buf is month SP ( 2DIGIT | ( SP 1DIGIT ))
        Local time
     */
    WebsTime    returnValue;
    int         dayValue, monthValue, yearValue, timeValue, tmpIndex;

    returnValue = (WebsTime) -1;
    tmpIndex = *index;

    dayValue = monthValue = yearValue = timeValue = -1;

    monthValue = parseMonth(buf, &tmpIndex);
    if (monthValue >= 0) {
        /*      
            Skip over the space 
         */
        tmpIndex++; 
        if (buf[tmpIndex] == ' ') {
            /*
                Skip over this space too 
             */
            tmpIndex++; 
            dayValue = parseNDIGIT(buf, 1, &tmpIndex);
        } else {
            dayValue = parseNDIGIT(buf, 2, &tmpIndex);
        }
        /*      
            Now get the time and time SP 4DIGIT
         */
        tmpIndex++;
        timeValue = parseTime(buf, &tmpIndex);
        if (timeValue >= 0) {
            /*          
                Now grab the 4DIGIT year value
             */
            tmpIndex++;
            yearValue = parseYear(buf, &tmpIndex);
        }
    }
    if ((dayValue >= 0) && (monthValue >= 0) && (yearValue >= 0)) {
        returnValue = dateToTimet(yearValue, monthValue, dayValue);
        returnValue += timeValue;
        *index = tmpIndex;
    }
    return returnValue;
}


/*
    This calculates the buffer index by comparing with a testChar
 */
static int getInc(char *buf, int testIndex, char testChar, int foundIncrement, int notfoundIncrement) 
{
    return (buf[testIndex] == testChar) ? foundIncrement : notfoundIncrement;
}


/*
    Return an index into a logical weekday array
 */
static int parseWeekday(char *buf, int *index) 
{
    /*  
        Format of buf is either
            "mon" | "tue" | "wed" | "thu" | "fri" | "sat" | "sun"
        or
            "monday" | "tuesday" | "wednesday" | "thursday" | "friday" | "saturday" | "sunday"
     */
    int tmpIndex, returnValue;

    returnValue = -1;
    tmpIndex = *index;

    switch (buf[tmpIndex]) {
        case 'f':
            returnValue = FRI;
            *index += getInc(buf, tmpIndex+3, 'd', sizeof("Friday"), 3);
            break;
        case 'm':
            returnValue = MON;
            *index += getInc(buf, tmpIndex+3, 'd', sizeof("Monday"), 3);
            break;
        case 's':
            switch (buf[tmpIndex+1]) {
                case 'a':
                    returnValue = SAT;
                    *index += getInc(buf, tmpIndex+3, 'u', sizeof("Saturday"), 3);
                    break;
                case 'u':
                    returnValue = SUN;
                    *index += getInc(buf, tmpIndex+3, 'd', sizeof("Sunday"), 3);
                    break;
            }
            break;
        case 't':
            switch (buf[tmpIndex+1]) {
                case 'h':
                    returnValue = THU;
                    *index += getInc(buf, tmpIndex+3, 'r', sizeof("Thursday"), 3);
                    break;
                case 'u':
                    returnValue = TUE;
                    *index += getInc(buf, tmpIndex+3, 's', sizeof("Tuesday"), 3);
                    break;
            }
            break;
        case 'w':
            returnValue = WED;
            *index += getInc(buf, tmpIndex+3, 'n', sizeof("Wednesday"), 3);
            break;
    }
    return returnValue;
}


/*
    Parse the date and time string
 */
static WebsTime dateParse(WebsTime tip, char *cmd)
{
    WebsTime    parsedValue, dateValue;
    int         index, tmpIndex, weekday, timeValue;

    slower(cmd);
    parsedValue = (WebsTime) 0;
    index = timeValue = 0;
    weekday = parseWeekday(cmd, &index);

    if (weekday >= 0) {
        tmpIndex = index;
        dateValue = parseDate1or2(cmd, &tmpIndex);
        if (dateValue >= 0) {
            index = tmpIndex + 1;
            /*
                One of these two forms is being used
                wkday [","] SP date1 SP time SP "GMT"
                weekday [","] SP date2 SP time SP "GMT"
             */
            timeValue = parseTime(cmd, &index);
            if (timeValue >= 0) {
                /*              
                    Now match up that "GMT" string for completeness
                    Compute the final value if there were no problems in the parse
                 */
                if ((weekday >= 0) &&
                    (dateValue >= 0) &&
                    (timeValue >= 0)) {
                    parsedValue = dateValue + timeValue;
                }
            }
        } else {
            /* 
                Try the other form - wkday SP date3 SP time SP 4DIGIT
                NOTE: local time
             */
            tmpIndex = ++index;
            parsedValue = parseDate3Time(cmd, &tmpIndex);
        }
    }
    return parsedValue;
}


PUBLIC bool websValidUriChars(char *uri)
{
    ssize   pos;

    if (uri == 0 || *uri == 0) {
        return 1;
    }
    pos = strspn(uri, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=%");
    if (pos < slen(uri)) {
        error("Bad character in URI at \"%s\"", &uri[pos]);
        return 0;
    }
    return 1;
}


/*
    Parse the URL. A single buffer is allocated to store the parsed URL in *pbuf. This must be freed by the caller.
 */
PUBLIC int websUrlParse(char *url, char **pbuf, char **pscheme, char **phost, char **pport, char **ppath, char **pext, 
        char **preference, char **pquery)
{
    char    *tok, *delim, *host, *path, *port, *scheme, *reference, *query, *ext, *buf, *buf2;
    ssize   buflen, ulen, len;
    int     rc, sep;

    assert(pbuf);
    if (url == 0) {
        url = "";
    }
    /*
        Allocate twice. Need to null terminate the host so have to copy the path.
     */
    ulen = strlen(url);
    len = ulen + 1;
    buflen = len * 2;
    if ((buf = walloc(buflen)) == NULL) {
        return -1;
    }
    buf2 = &buf[ulen + 1];
    sncopy(buf, len, url, ulen);
    sncopy(buf2, len, url, ulen);
    url = buf;

    scheme = 0;
    host = 0;
    port = 0;
    path = 0;
    ext = 0;
    query = 0;
    reference = 0;
    tok = buf;
    sep = '/';

    /*
        [scheme://][hostname[:port]][/path[.ext]][#ref][?query]
        First trim query and then reference from the end
     */
    if ((query = strchr(tok, '?')) != NULL) {
        *query++ = '\0';
    }
    if ((reference = strchr(tok, '#')) != NULL) {
        *reference++ = '\0';
    }

    /*
        [scheme://][hostname[:port]][/path]
     */
    if ((delim = strstr(tok, "://")) != 0) {
        scheme = tok;
        *delim = '\0';
        tok = &delim[3];
    }

    /*
        [hostname[:port]][/path]
     */
    if (*tok == '[' && ((delim = strchr(tok, ']')) != 0)) {
        /* IPv6 [::] */
        host = &tok[1];
        *delim++ = '\0';
        tok = delim;

    } else if (*tok && *tok != '/' && *tok != ':' && (scheme || strchr(tok, ':'))) {
        /* 
           Supported forms:
               scheme://hostname
               hostname[:port][/path] 
         */
        host = tok;
        if ((tok = strpbrk(tok, ":/")) == 0) {
            tok = "";
        }
        /* Don't terminate the hostname yet, need to see if tok is a ':' for a port. */
        assert(tok);
    }

    /* [:port][/path] */
    if (*tok == ':') {
        /* Terminate hostname */
        *tok++ = '\0';
        port = tok;
        if ((tok = strchr(tok, '/')) == 0) {
            tok = "";
        }
    }

    /* [/path] */
    if (*tok) {
        /* 
           Terminate hostname. This zeros the leading path slash.
           This will be repaired before returning if ppath is set 
         */
        sep = *tok;
        *tok++ = '\0';
        path = tok;
        /* path[.ext[/extra]] */
        if ((tok = strrchr(path, '.')) != 0) {
            if (tok[1]) {
                if ((delim = strrchr(path, '/')) != 0) {
                    if (delim < tok) {
                        ext = tok;
                    }
                } else {
                    ext = tok;
                }
            }
        }
    }
    /*
        Pass back the requested fields
     */
    rc = 0;
    *pbuf = buf;
    if (pscheme) {
        if (scheme == 0) {
            scheme = "http";
        }
        *pscheme = scheme;
    }
    if (phost) {
        if (host == 0) {
            host = "localhost";
        }
        *phost = host;
    }
    if (pport) {
        *pport = port;
    }
    if (ppath) {
        if (path == 0) {
            path = "/";
        } else {
            /* Copy path to reinsert leading slash */
            scopy(&buf2[1], len - 1, path);
            path = buf2;
            *path = sep;
#if UNUSED && MOVED 
            if (!websValidUriChars(path)) {
                rc = -1;
            } else {
                websDecodeUrl(path, path, -1);
            }
#endif
        }
        *ppath = path;
    }
    if (pquery) {
        *pquery = query;
    }
    if (preference) {
        *preference = reference;
    }
    if (pext) {
#if ME_WIN_LIKE
        slower(ext);            
#endif
        *pext = ext;
    }
    return rc;
}


/*
    Normalize a URI path to remove "./",  "../" and redundant separators. 
    Note: this does not make an abs path and does not map separators nor change case. 
    This validates the URI and expects it to begin with "/".
 */
PUBLIC char *websNormalizeUriPath(char *pathArg)
{
    char    *dupPath, *path, *sp, *dp, *mark, **segments;
    int     firstc, j, i, nseg, len;

    if (pathArg == 0 || *pathArg == '\0') {
        return sclone("");
    }
    len = (int) slen(pathArg);
    if ((dupPath = walloc(len + 2)) == 0) {
        return NULL;
    }
    strcpy(dupPath, pathArg);

    if ((segments = walloc(sizeof(char*) * (len + 1))) == 0) {
        return NULL;
    }
    nseg = len = 0;
    firstc = *dupPath;
    for (mark = sp = dupPath; *sp; sp++) {
        if (*sp == '/') {
            *sp = '\0';
            while (sp[1] == '/') {
                sp++;
            }
            segments[nseg++] = mark;
            len += (int) (sp - mark);
            mark = sp + 1;
        }
    }
    segments[nseg++] = mark;
    len += (int) (sp - mark);
    for (j = i = 0; i < nseg; i++, j++) {
        sp = segments[i];
        if (sp[0] == '.') {
            if (sp[1] == '\0')  {
                if ((i+1) == nseg) {
                    /* Trim trailing "." */
                    segments[j] = "";
                } else {
                    j--;
                }
            } else if (sp[1] == '.' && sp[2] == '\0')  {
                j = max(j - 2, -1);
                if ((i+1) == nseg) {
                    nseg--;
                }
            } else {
                /* .more-chars */
                segments[j] = segments[i];
            }
        } else {
            segments[j] = segments[i];
        }
    }
    nseg = j;
    assert(nseg >= 0);
    if ((path = walloc(len + nseg + 1)) != 0) {
        for (i = 0, dp = path; i < nseg; ) {
            strcpy(dp, segments[i]);
            len = (int) slen(segments[i]);
            dp += len;
            if (++i < nseg || (nseg == 1 && *segments[0] == '\0' && firstc == '/')) {
                *dp++ = '/';
            }
        }
        *dp = '\0';
    }
    wfree(dupPath);
    wfree(segments);
    if ((path[0] != '/') || strchr(path, '\\')) {
        return 0;
    }
    return path;
}


/*
    Validate a URI path for use in a HTTP request line
    The URI must contain only valid characters and must being with "/" both before and after decoding.
    A decoded, normalized URI path is returned.
    The uri is modified.
 */
PUBLIC char *websValidateUriPath(char *uri)
{
    if (uri == 0 || *uri != '/') {
        return 0;
    }
    if (!websValidUriChars(uri)) {
        return 0;
    }
    websDecodeUrl(uri, uri, -1);
    if ((uri = websNormalizeUriPath(uri)) == 0) {
        return 0;
    }
    if (*uri != '/' || strchr(uri, '\\')) {
        return 0;
    }
    return uri;
}


/*
    Open a web page. filename is the local filename. path is the URL path name.
 */
PUBLIC int websPageOpen(Webs *wp, int mode, int perm)
{
    assert(websValid(wp));
    return (wp->docfd = websOpenFile(wp->filename, mode, perm));
}


PUBLIC void websPageClose(Webs *wp)
{
    assert(websValid(wp));

    if (wp->docfd >= 0) {
        websCloseFile(wp->docfd);
        wp->docfd = -1;
    }
}


PUBLIC int websPageStat(Webs *wp, WebsFileInfo *sbuf)
{
    return websStatFile(wp->filename, sbuf);
}


PUBLIC int websPageIsDirectory(Webs *wp)
{
    WebsFileInfo    sbuf;

    if (websStatFile(wp->filename, &sbuf) >= 0) {
        return(sbuf.isDir);
    }
    return 0;
}


/*
    Read a web page. Returns the number of _bytes_ read. len is the size of buf, in bytes.
 */
PUBLIC ssize websPageReadData(Webs *wp, char *buf, ssize nBytes)
{

    assert(websValid(wp));
    return websReadFile(wp->docfd, buf, nBytes);
}


/*
    Move file pointer offset bytes.
 */
PUBLIC void websPageSeek(Webs *wp, Offset offset, int origin)
{
    assert(websValid(wp));

    websSeekFile(wp->docfd, offset, origin);
}


PUBLIC void websSetCookie(Webs *wp, char *name, char *value, char *path, char *cookieDomain, WebsTime lifespan, int flags)
{
    WebsTime    when;
    char        *cp, *expiresAtt, *expires, *domainAtt, *domain, *secure, *httponly, *cookie, *old;

    assert(wp);
    assert(name && *name);

    if (path == 0) {
        path = "/";
    }
    if (!cookieDomain) {
        domain = sclone(wp->host);
        if ((cp = strchr(domain, ':')) != 0) {
            /* Strip port */
            *cp = '\0';
        }
        if (*domain && domain[strlen(domain) - 1] == '.') {
            /* Cleanup bonjour addresses with trailing dot */
            domain[strlen(domain) - 1] = '\0';
        }
    } else {
        domain = sclone(cookieDomain);
    }
    domainAtt = "";
    if (smatch(domain, "localhost")) {
        wfree(domain);
        domain = sclone("");
    } else {
        domainAtt = "; domain=";
        if (!strchr(domain, '.')) {
            old = domain;
            domain = sfmt(".%s", domain);
            wfree(old);
        }
    }
    if (lifespan > 0) {
        expiresAtt = "; expires=";
        when = time(0) + lifespan;
        if ((expires = ctime(&when)) != NULL) {
            expires[strlen(expires) - 1] = '\0';
        }

    } else {
        expiresAtt = "";
        expires = "";
    }
    /* 
       Allow multiple cookie headers. Even if the same name. Later definitions take precedence
     */
    secure = (flags & WEBS_COOKIE_SECURE) ? "; secure" : "";
    httponly = (flags & WEBS_COOKIE_HTTP) ?  "; httponly" : "";
    cookie = sfmt("%s=%s; path=%s%s%s%s%s%s%s", name, value, path, domainAtt, domain, expiresAtt, expires, secure, 
        httponly);
    if (wp->responseCookie) {
        old = wp->responseCookie;
        wp->responseCookie = sfmt("%s %s", wp->responseCookie, cookie);
        wfree(old);
        wfree(cookie);
    } else {
        wp->responseCookie = cookie;
    }
    wfree(domain);
}


static char *getToken(Webs *wp, char *delim)
{
    WebsBuf     *buf;
    char        *token, *nextToken, *endToken;

    assert(wp);
    buf = &wp->rxbuf;
    nextToken = (char*) buf->endp;

    for (token = (char*) buf->servp; (*token == ' ' || *token == '\t') && token < (char*) buf->endp; token++) {}

    if (delim == 0) {
        delim = " \t";
        if ((endToken = strpbrk(token, delim)) != 0) {
            nextToken = endToken + strspn(endToken, delim);
            *endToken = '\0';
        }
    } else {
        if ((endToken = strstr(token, delim)) != 0) {
            *endToken = '\0';
            /* Only eat one occurence of the delimiter */
            nextToken = endToken + strlen(delim);
        } else {
            nextToken = buf->endp;
        }
    }
    buf->servp = nextToken;
    return token;
}


PUBLIC int websGetBackground() 
{
    return websBackground;
}


PUBLIC void websSetBackground(int on) 
{
    websBackground = on;
}


PUBLIC int websGetDebug() 
{
    return websDebug;
}


PUBLIC void websSetDebug(int on) 
{
    websDebug = on;
}


static char *makeSessionID(Webs *wp)
{
    char        idBuf[64];
    static int  nextSession = 0;

    assert(wp);
    fmt(idBuf, sizeof(idBuf), "%08x%08x%d", PTOI(wp) + PTOI(wp->url), (int) time(0), nextSession++);
    return websMD5Block(idBuf, sizeof(idBuf), "::webs.session::");
}


WebsSession *websAllocSession(Webs *wp, char *id, WebsTime lifespan)
{
    WebsSession     *sp;

    assert(wp);

    if ((sp = walloc(sizeof(WebsSession))) == 0) {
        return 0;
    }
    sp->lifespan = lifespan;
    sp->expires = time(0) + lifespan;
    if (id == 0) {
        sp->id = makeSessionID(wp);
    } else {
        sp->id = sclone(id);
    }
    if ((sp->cache = hashCreate(WEBS_SESSION_HASH)) == 0) {
        return 0;
    }
    return sp;
}


static void freeSession(WebsSession *sp)
{
    assert(sp);

    if (sp->cache >= 0) {
        hashFree(sp->cache);
    }
    wfree(sp->id);
    wfree(sp);
}


WebsSession *websGetSession(Webs *wp, int create)
{
    WebsKey     *sym;
    char        *id;
    
    assert(wp);

    if (!wp->session) {
        id = websGetSessionID(wp);
        if ((sym = hashLookup(sessions, id)) == 0) {
            if (!create) {
                wfree(id);
                return 0;
            }
            if (sessionCount > ME_GOAHEAD_LIMIT_SESSION_COUNT) {
                error("Too many sessions %d/%d", sessionCount, ME_GOAHEAD_LIMIT_SESSION_COUNT);
                wfree(id);
                return 0;
            }
            sessionCount++;
            if ((wp->session = websAllocSession(wp, id, ME_GOAHEAD_LIMIT_SESSION_LIFE)) == 0) {
                wfree(id);
                return 0;
            }
            if ((sym = hashEnter(sessions, wp->session->id, valueSymbol(wp->session), 0)) == 0) {
                wfree(id);
                return 0;
            }
            wp->session = (WebsSession*) sym->content.value.symbol;
            websSetCookie(wp, WEBS_SESSION, wp->session->id, "/", NULL, 0, 0);
        } else {
            wp->session = (WebsSession*) sym->content.value.symbol;
        }
        wfree(id);
    }
    if (wp->session) {
        wp->session->expires = time(0) + wp->session->lifespan;
    }
    return wp->session;
}


PUBLIC char *websGetSessionID(Webs *wp)
{
    char    *cookie, *cp, *value;
    ssize   len;
    int     quoted;

    assert(wp);

    if (wp->session) {
        return wp->session->id;
    }
    cookie = wp->cookie;
    if (cookie && (value = strstr(cookie, WEBS_SESSION)) != 0) {
        value += strlen(WEBS_SESSION);
        while (isspace((uchar) *value) || *value == '=') {
            value++;
        }
        quoted = 0;
        if (*value == '"') {
            value++;
            quoted++;
        }
        for (cp = value; *cp; cp++) {
            if (quoted) {
                if (*cp == '"' && cp[-1] != '\\') {
                    break;
                }
            } else {
                if ((*cp == ',' || *cp == ';') && cp[-1] != '\\') {
                    break;
                }
            }
        }
        len = cp - value;
        if ((cp = walloc(len + 1)) == 0) {
            return 0;
        }
        strncpy(cp, value, len);
        cp[len] = '\0';
        return cp;
    }
    return 0;
}


PUBLIC char *websGetSessionVar(Webs *wp, char *key, char *defaultValue)
{
    WebsSession     *sp;
    WebsKey         *sym;

    assert(wp);
    assert(key && *key);

    if ((sp = websGetSession(wp, 1)) != 0) {
        if ((sym = hashLookup(sp->cache, key)) == 0) {
            return defaultValue;
        }
        return (char*) sym->content.value.symbol;
    }
    return 0;
}


PUBLIC void websRemoveSessionVar(Webs *wp, char *key)
{
    WebsSession     *sp;

    assert(wp);
    assert(key && *key);

    if ((sp = websGetSession(wp, 1)) != 0) {
        hashDelete(sp->cache, key);
    }
}


PUBLIC int websSetSessionVar(Webs *wp, char *key, char *value)
{
    WebsSession  *sp;

    assert(wp);
    assert(key && *key);
    assert(value);

    if ((sp = websGetSession(wp, 1)) == 0) {
        return 0;
    }
    if (hashEnter(sp->cache, key, valueString(value, VALUE_ALLOCATE), 0) == 0) {
        return -1;
    }
    return 0;
}


static void pruneCache()
{
    WebsSession     *sp;
    WebsTime        when;
    WebsKey         *sym, *next;
    int             oldCount;

    oldCount = sessionCount;
    when = time(0);
    for (sym = hashFirst(sessions); sym; sym = next) {
        next = hashNext(sessions, sym);
        sp = (WebsSession*) sym->content.value.symbol;
        if (sp->expires <= when) {
            hashDelete(sessions, sp->id);
            sessionCount--;
            freeSession(sp);
        }
    }
    if (oldCount != sessionCount || sessionCount) { 
        trace(4, "Prune %d sessions. Remaining: %d", oldCount - sessionCount, sessionCount);
    }
    websRestartEvent(pruneId, WEBS_SESSION_PRUNE);
}


/*
    One line embedding
 */
PUBLIC int websServer(char *endpoint, char *documents)
{
    int     finished = 0;

    if (websOpen(documents, "route.txt") < 0) {
        error("Cannot initialize server. Exiting.");
        return -1;
    }
    if (websLoad("auth.txt") < 0) {
        error("Cannot load auth.txt");
        return -1;
    }
    if (websListen(endpoint) < 0) {
        return -1;
    }
    websServiceEvents(&finished);
    websClose();
    return 0;
}


static void setFileLimits()
{
#if ME_UNIX_LIKE
    struct rlimit r;
    int           i, limit;

    limit = ME_GOAHEAD_LIMIT_FILES;
    if (limit == 0) {
        /*
            We need to determine a reasonable maximum possible limit value.
            There is no #define we can use for this, so we test to determine it empirically
         */
        for (limit = 0x40000000; limit > 0; limit >>= 1) {
            r.rlim_cur = r.rlim_max = limit;
            if (setrlimit(RLIMIT_NOFILE, &r) == 0) {
                for (i = (limit >> 4) * 15; i > 0; i--) {
                    r.rlim_max = r.rlim_cur = limit + i;
                    if (setrlimit(RLIMIT_NOFILE, &r) == 0) {
                        limit = 0;
                        break;
                    }
                }
                break;
            }
        }
    } else {
        r.rlim_cur = r.rlim_max = limit;
        if (setrlimit(RLIMIT_NOFILE, &r) < 0) {
            error("Cannot set file limit to %d", limit);
        }
    }
    getrlimit(RLIMIT_NOFILE, &r);
    trace(6, "Max files soft %d, max %d", r.rlim_cur, r.rlim_max);
#endif
}

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/js.c ************/


/*
    js.c -- Mini JavaScript
  
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "js.h"

#if ME_GOAHEAD_JAVASCRIPT
/********************************** Defines ***********************************/

#define     OCTAL   8
#define     HEX     16

static Js   **jsHandles;    /* List of js handles */
static int  jsMax = -1;     /* Maximum size of  */

/****************************** Forward Declarations **************************/

static Js       *jsPtr(int jid);
static void     clearString(char **ptr);
static void     setString(char **ptr, char *s);
static void     appendString(char **ptr, char *s);
static int      parse(Js *ep, int state, int flags);
static int      parseStmt(Js *ep, int state, int flags);
static int      parseDeclaration(Js *ep, int state, int flags);
static int      parseCond(Js *ep, int state, int flags);
static int      parseExpr(Js *ep, int state, int flags);
static int      parseFunctionArgs(Js *ep, int state, int flags);
static int      evalExpr(Js *ep, char *lhs, int rel, char *rhs);
static int      evalCond(Js *ep, char *lhs, int rel, char *rhs);
static int      evalFunction(Js *ep);
static void     freeFunc(JsFun *func);
static void     jsRemoveNewlines(Js *ep, int state);

static int      getLexicalToken(Js *ep, int state);
static int      tokenAddChar(Js *ep, int c);
static int      inputGetc(Js *ep);
static void     inputPutback(Js *ep, int c);
static int      charConvert(Js *ep, int base, int maxDig);

/************************************* Code ***********************************/

PUBLIC int jsOpenEngine(WebsHash variables, WebsHash functions)
{
    Js      *ep;
    int     jid, vid;

    if ((jid = wallocObject(&jsHandles, &jsMax, sizeof(Js))) < 0) {
        return -1;
    }
    ep = jsHandles[jid];
    ep->jid = jid;

    /*
        Create a top level symbol table if one is not provided for variables and functions. Variables may create other
        symbol tables for block level declarations so we use walloc to manage a list of variable tables.
     */
    if ((vid = wallocHandle(&ep->variables)) < 0) {
        jsMax = wfreeHandle(&jsHandles, ep->jid);
        return -1;
    }
    if (vid >= ep->variableMax) {
        ep->variableMax = vid + 1;
    }
    if (variables == -1) {
        ep->variables[vid] = hashCreate(64) + JS_OFFSET;
        ep->flags |= FLAGS_VARIABLES;
    } else {
        ep->variables[vid] = variables + JS_OFFSET;
    }
    if (functions == -1) {
        ep->functions = hashCreate(64);
        ep->flags |= FLAGS_FUNCTIONS;
    } else {
        ep->functions = functions;
    }
    jsLexOpen(ep);

    /*
        Define standard constants
     */
    jsSetGlobalVar(ep->jid, "null", NULL);
    return ep->jid;
}


PUBLIC void jsCloseEngine(int jid)
{
    Js      *ep;
    int     i;

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }

    wfree(ep->error);
    ep->error = NULL;
    wfree(ep->result);
    ep->result = NULL;

    jsLexClose(ep);

    for (i = ep->variableMax - 1; i >= 0; i--) {
        if (ep->flags & FLAGS_VARIABLES) {
            hashFree(ep->variables[i] - JS_OFFSET);
        }
        ep->variableMax = wfreeHandle(&ep->variables, i);
    }
    if (ep->flags & FLAGS_FUNCTIONS) {
        hashFree(ep->functions);
    }
    jsMax = wfreeHandle(&jsHandles, ep->jid);
    wfree(ep);
}


#if !ECOS && KEEP
PUBLIC char *jsEvalFile(int jid, char *path, char **emsg)
{
    WebsStat    sbuf;
    Js          *ep;
    char      *script, *rs;
    int         fd;

    assert(path && *path);

    if (emsg) {
        *emsg = NULL;
    }
    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }
    if ((fd = open(path, O_RDONLY | O_BINARY, 0666)) < 0) {
        jsError(ep, "Bad handle %d", jid);
        return NULL;
    }
    if (stat(path, &sbuf) < 0) {
        close(fd);
        jsError(ep, "Cant stat %s", path);
        return NULL;
    }
    if ((script = walloc(sbuf.st_size + 1)) == NULL) {
        close(fd);
        jsError(ep, "Cant malloc %d", sbuf.st_size);
        return NULL;
    }
    if (read(fd, script, sbuf.st_size) != (int)sbuf.st_size) {
        close(fd);
        wfree(script);
        jsError(ep, "Error reading %s", path);
        return NULL;
    }
    script[sbuf.st_size] = '\0';
    close(fd);
    rs = jsEvalBlock(jid, script, emsg);
    wfree(script);
    return rs;
}
#endif


/*
    Create a new variable scope block so that consecutive jsEval calls may be made with the same varible scope. This
    space MUST be closed with jsCloseBlock when the evaluations are complete.
 */
PUBLIC int jsOpenBlock(int jid)
{
    Js      *ep;
    int     vid;

    if((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    if ((vid = wallocHandle(&ep->variables)) < 0) {
        return -1;
    }
    if (vid >= ep->variableMax) {
        ep->variableMax = vid + 1;
    }
    ep->variables[vid] = hashCreate(64) + JS_OFFSET;
    return vid;

}


PUBLIC int jsCloseBlock(int jid, int vid)
{
    Js    *ep;

    if((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    hashFree(ep->variables[vid] - JS_OFFSET);
    ep->variableMax = wfreeHandle(&ep->variables, vid);
    return 0;

}


/*
    Create a new variable scope block and evaluate a script. All variables
    created during this context will be automatically deleted when complete.
 */
PUBLIC char *jsEvalBlock(int jid, char *script, char **emsg)
{
    char* returnVal;
    int     vid;

    assert(script);

    vid = jsOpenBlock(jid);
    returnVal = jsEval(jid, script, emsg);
    jsCloseBlock(jid, vid);
    return returnVal;
}


/*
    Parse and evaluate Javascript.
 */
PUBLIC char *jsEval(int jid, char *script, char **emsg)
{
    Js      *ep;
    JsInput *oldBlock;
    int     state;
    void    *endlessLoopTest;
    int     loopCounter;
    
    
    assert(script);

    if (emsg) {
        *emsg = NULL;
    } 
    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }
    setString(&ep->result, "");

    /*
        Allocate a new evaluation block, and save the old one
     */
    oldBlock = ep->input;
    jsLexOpenScript(ep, script);

    /*
        Do the actual parsing and evaluation
     */
    loopCounter = 0;
    endlessLoopTest = NULL;

    do {
        state = parse(ep, STATE_BEGIN, FLAGS_EXE);

        if (state == STATE_RET) {
            state = STATE_EOF;
        }
        /*
            prevent parser from going into infinite loop.  If parsing the same line 10 times then fail and report Syntax
            error.  Most normal error are caught in the parser itself.
         */
        if (endlessLoopTest == ep->input->script.servp) {
            if (loopCounter++ > 10) {
                state = STATE_ERR;
                jsError(ep, "Syntax error");
            }
        } else {
            endlessLoopTest = ep->input->script.servp;
            loopCounter = 0;
        }
    } while (state != STATE_EOF && state != STATE_ERR);

    jsLexCloseScript(ep);

    /*
        Return any error string to the user
     */
    if (state == STATE_ERR && emsg) {
        *emsg = sclone(ep->error);
    }

    /*
        Restore the old evaluation block
     */
    ep->input = oldBlock;

    if (state == STATE_EOF) {
        return ep->result;
    }
    if (state == STATE_ERR) {
        return NULL;
    }
    return ep->result;
}



/*
    Recursive descent parser for Javascript
 */
static int parse(Js *ep, int state, int flags)
{
    assert(ep);

    switch (state) {
    /*
        Any statement, function arguments or conditional expressions
     */
    case STATE_STMT:
        if ((state = parseStmt(ep, state, flags)) != STATE_STMT_DONE &&
            state != STATE_EOF && state != STATE_STMT_BLOCK_DONE &&
            state != STATE_RET) {
            state = STATE_ERR;
        }
        break;

    case STATE_DEC:
        if ((state = parseStmt(ep, state, flags)) != STATE_DEC_DONE &&
            state != STATE_EOF) {
            state = STATE_ERR;
        }
        break;

    case STATE_EXPR:
        if ((state = parseStmt(ep, state, flags)) != STATE_EXPR_DONE &&
            state != STATE_EOF) {
            state = STATE_ERR;
        }
        break;

    /*
        Variable declaration list
     */
    case STATE_DEC_LIST:
        state = parseDeclaration(ep, state, flags);
        break;

    /*
        Function argument string
     */
    case STATE_ARG_LIST:
        state = parseFunctionArgs(ep, state, flags);
        break;

    /*
        Logical condition list (relational operations separated by &&, ||)
     */
    case STATE_COND:
        state = parseCond(ep, state, flags);
        break;

    /*
     j  Expression list
     */
    case STATE_RELEXP:
        state = parseExpr(ep, state, flags);
        break;
    }

    if (state == STATE_ERR && ep->error == NULL) {
        jsError(ep, "Syntax error");
    }
    return state;
}


/*
    Parse any statement including functions and simple relational operations
 */
static int parseStmt(Js *ep, int state, int flags)
{
    JsFun       func;
    JsFun       *saveFunc;
    JsInput     condScript, endScript, bodyScript, incrScript;
    char      *value, *identifier;
    int         done, expectSemi, thenFlags, elseFlags, tid, cond, forFlags;
    int         jsVarType;

    assert(ep);

    /*
        Set these to NULL, else we try to free them if an error occurs.
     */
    endScript.putBackToken = NULL;
    bodyScript.putBackToken = NULL;
    incrScript.putBackToken = NULL;
    condScript.putBackToken = NULL;

    expectSemi = 0;
    saveFunc = NULL;

    for (done = 0; !done; ) {
        tid = jsLexGetToken(ep, state);

        switch (tid) {
        default:
            jsLexPutbackToken(ep, TOK_EXPR, ep->token);
            done++;
            break;

        case TOK_ERR:
            state = STATE_ERR;
            done++;
            break;

        case TOK_EOF:
            state = STATE_EOF;
            done++;
            break;

        case TOK_NEWLINE:
            break;

        case TOK_SEMI:
            /*
                This case is when we discover no statement and just a lone ';'
             */
            if (state != STATE_STMT) {
                jsLexPutbackToken(ep, tid, ep->token);
            }
            done++;
            break;

        case TOK_ID:
            /*
                This could either be a reference to a variable or an assignment
             */
            identifier = NULL;
            setString(&identifier, ep->token);
            /*
                Peek ahead to see if this is an assignment
             */
            tid = jsLexGetToken(ep, state);
            if (tid == TOK_ASSIGNMENT) {
                if (parse(ep, STATE_RELEXP, flags) != STATE_RELEXP_DONE) {
                    clearString(&identifier);
                    goto error;
                }
                if (flags & FLAGS_EXE) {
                    if ( state == STATE_DEC ) {
                        jsSetLocalVar(ep->jid, identifier, ep->result);
                    } else {
                        jsVarType = jsGetVar(ep->jid, identifier, &value);
                        if (jsVarType > 0) {
                            jsSetLocalVar(ep->jid, identifier, ep->result);
                        } else {
                            jsSetGlobalVar(ep->jid, identifier, ep->result);
                        }
                    }
                }

            } else if (tid == TOK_INC_DEC ) {
                value = NULL;
                if (flags & FLAGS_EXE) {
                    jsVarType = jsGetVar(ep->jid, identifier, &value);
                    if (jsVarType < 0) {
                        jsError(ep, "Undefined variable %s\n", identifier);
                        goto error;
                    }
                    setString(&ep->result, value);
                    if (evalExpr(ep, value, (int) *ep->token, "1") < 0) {
                        state = STATE_ERR;
                        break;
                    }

                    if (jsVarType > 0) {
                        jsSetLocalVar(ep->jid, identifier, ep->result);
                    } else {
                        jsSetGlobalVar(ep->jid, identifier, ep->result);
                    }
                }

            } else {
                /*
                    If we are processing a declaration, allow undefined vars
                 */
                value = NULL;
                if (state == STATE_DEC) {
                    if (jsGetVar(ep->jid, identifier, &value) > 0) {
                        jsError(ep, "Variable already declared",
                            identifier);
                        clearString(&identifier);
                        goto error;
                    }
                    jsSetLocalVar(ep->jid, identifier, NULL);
                } else {
                    if ( flags & FLAGS_EXE ) {
                        if (jsGetVar(ep->jid, identifier, &value) < 0) {
                            jsError(ep, "Undefined variable %s\n",
                                identifier);
                            clearString(&identifier);
                            goto error;
                        }
                    }
                }
                setString(&ep->result, value);
                jsLexPutbackToken(ep, tid, ep->token);
            }
            clearString(&identifier);

            if (state == STATE_STMT) {
                expectSemi++;
            }
            done++;
            break;

        case TOK_LITERAL:
            /*
                Set the result to the literal (number or string constant)
             */
            setString(&ep->result, ep->token);
            if (state == STATE_STMT) {
                expectSemi++;
            }
            done++;
            break;

        case TOK_FUNCTION:
            /*
                We must save any current ep->func value for the current stack frame
             */
            if (ep->func) {
                saveFunc = ep->func;
            }
            memset(&func, 0, sizeof(JsFun));
            setString(&func.fname, ep->token);
            ep->func = &func;

            setString(&ep->result, "");
            if (jsLexGetToken(ep, state) != TOK_LPAREN) {
                freeFunc(&func);
                goto error;
            }
            if (parse(ep, STATE_ARG_LIST, flags) != STATE_ARG_LIST_DONE) {
                freeFunc(&func);
                ep->func = saveFunc;
                goto error;
            }
            /*
                Evaluate the function if required
             */
            if (flags & FLAGS_EXE && evalFunction(ep) < 0) {
                freeFunc(&func);
                ep->func = saveFunc;
                goto error;
            }
            freeFunc(&func);
            ep->func = saveFunc;

            if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                goto error;
            }
            if (state == STATE_STMT) {
                expectSemi++;
            }
            done++;
            break;

        case TOK_IF:
            if (state != STATE_STMT) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_LPAREN) {
                goto error;
            }
            /*
                Evaluate the entire condition list "(condition)"
             */
            if (parse(ep, STATE_COND, flags) != STATE_COND_DONE) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                goto error;
            }
            /*
                This is the "then" case. We need to always parse both cases and execute only the relevant case.
             */
            if (*ep->result == '1') {
                thenFlags = flags;
                elseFlags = flags & ~FLAGS_EXE;
            } else {
                thenFlags = flags & ~FLAGS_EXE;
                elseFlags = flags;
            }
            /*
                Process the "then" case.  Allow for RETURN statement
             */
            switch (parse(ep, STATE_STMT, thenFlags)) {
            case STATE_RET:
                return STATE_RET;
            case STATE_STMT_DONE:
                break;
            default:
                goto error;
            }
            /*
                check to see if there is an "else" case
             */
            jsRemoveNewlines(ep, state);
            tid = jsLexGetToken(ep, state);
            if (tid != TOK_ELSE) {
                jsLexPutbackToken(ep, tid, ep->token);
                done++;
                break;
            }
            /*
                Process the "else" case.  Allow for return.
             */
            switch (parse(ep, STATE_STMT, elseFlags)) {
            case STATE_RET:
                return STATE_RET;
            case STATE_STMT_DONE:
                break;
            default:
                goto error;
            }
            done++;
            break;

        case TOK_FOR:
            /*
                Format for the expression is:
                    for (initial; condition; incr) {
                        body;
                    }
             */
            if (state != STATE_STMT) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_LPAREN) {
                goto error;
            }

            /*
                Evaluate the for loop initialization statement
             */
            if (parse(ep, STATE_EXPR, flags) != STATE_EXPR_DONE) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_SEMI) {
                goto error;
            }

            /*
                The first time through, we save the current input context just to each step: prior to the conditional,
                the loop increment and the loop body.  
             */
            jsLexSaveInputState(ep, &condScript);
            if (parse(ep, STATE_COND, flags) != STATE_COND_DONE) {
                goto error;
            }
            cond = (*ep->result != '0');

            if (jsLexGetToken(ep, state) != TOK_SEMI) {
                goto error;
            }

            /*
                Don't execute the loop increment statement or the body first time
             */
            forFlags = flags & ~FLAGS_EXE;
            jsLexSaveInputState(ep, &incrScript);
            if (parse(ep, STATE_EXPR, forFlags) != STATE_EXPR_DONE) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                goto error;
            }

            /*
                Parse the body and remember the end of the body script
             */
            jsLexSaveInputState(ep, &bodyScript);
            if (parse(ep, STATE_STMT, forFlags) != STATE_STMT_DONE) {
                goto error;
            }
            jsLexSaveInputState(ep, &endScript);

            /*
                Now actually do the for loop. Note loop has been rotated
             */
            while (cond && (flags & FLAGS_EXE) ) {
                /*
                    Evaluate the body
                 */
                jsLexRestoreInputState(ep, &bodyScript);

                switch (parse(ep, STATE_STMT, flags)) {
                case STATE_RET:
                    return STATE_RET;
                case STATE_STMT_DONE:
                    break;
                default:
                    goto error;
                }
                /*
                    Evaluate the increment script
                 */
                jsLexRestoreInputState(ep, &incrScript);
                if (parse(ep, STATE_EXPR, flags) != STATE_EXPR_DONE) {
                    goto error;
                }
                /*
                    Evaluate the condition
                 */
                jsLexRestoreInputState(ep, &condScript);
                if (parse(ep, STATE_COND, flags) != STATE_COND_DONE) {
                    goto error;
                }
                cond = (*ep->result != '0');
            }
            jsLexRestoreInputState(ep, &endScript);
            done++;
            break;

        case TOK_VAR:
            if (parse(ep, STATE_DEC_LIST, flags) != STATE_DEC_LIST_DONE) {
                goto error;
            }
            done++;
            break;

        case TOK_COMMA:
            jsLexPutbackToken(ep, TOK_EXPR, ep->token);
            done++;
            break;

        case TOK_LPAREN:
            if (state == STATE_EXPR) {
                if (parse(ep, STATE_RELEXP, flags) != STATE_RELEXP_DONE) {
                    goto error;
                }
                if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                    goto error;
                }
                return STATE_EXPR_DONE;
            }
            done++;
            break;

        case TOK_RPAREN:
            jsLexPutbackToken(ep, tid, ep->token);
            return STATE_EXPR_DONE;

        case TOK_LBRACE:
            /*
                This handles any code in braces except "if () {} else {}"
             */
            if (state != STATE_STMT) {
                goto error;
            }

            /*
                Parse will return STATE_STMT_BLOCK_DONE when the RBRACE is seen
             */
            do {
                state = parse(ep, STATE_STMT, flags);
            } while (state == STATE_STMT_DONE);

            /*
                Allow return statement.
             */
            if (state == STATE_RET) {
                return state;
            }

            if (jsLexGetToken(ep, state) != TOK_RBRACE) {
                goto error;
            }
            return STATE_STMT_DONE;

        case TOK_RBRACE:
            if (state == STATE_STMT) {
                jsLexPutbackToken(ep, tid, ep->token);
                return STATE_STMT_BLOCK_DONE;
            }
            goto error;

        case TOK_RETURN:
            if (parse(ep, STATE_RELEXP, flags) != STATE_RELEXP_DONE) {
                goto error;
            }
            if (flags & FLAGS_EXE) {
                while ( jsLexGetToken(ep, state) != TOK_EOF );
                done++;
                return STATE_RET;
            }
            break;
        }
    }

    if (expectSemi) {
        tid = jsLexGetToken(ep, state);
        if (tid != TOK_SEMI && tid != TOK_NEWLINE) {
            goto error;
        }
        /*
            Skip newline after semi-colon
         */
        jsRemoveNewlines(ep, state);
    }

doneParse:
    if (tid == TOK_FOR) {
        jsLexFreeInputState(ep, &condScript);
        jsLexFreeInputState(ep, &incrScript);
        jsLexFreeInputState(ep, &endScript);
        jsLexFreeInputState(ep, &bodyScript);
    }
    if (state == STATE_STMT) {
        return STATE_STMT_DONE;
    } else if (state == STATE_DEC) {
        return STATE_DEC_DONE;
    } else if (state == STATE_EXPR) {
        return STATE_EXPR_DONE;
    } else if (state == STATE_EOF) {
        return state;
    } else {
        return STATE_ERR;
    }

error:
    state = STATE_ERR;
    goto doneParse;
}


/*
    Parse variable declaration list
 */
static int parseDeclaration(Js *ep, int state, int flags)
{
    int     tid;

    assert(ep);

    /*
        Declarations can be of the following forms:
                var x;
                var x, y, z;
                var x = 1 + 2 / 3, y = 2 + 4;
      
        We set the variable to NULL if there is no associated assignment.
     */
    do {
        if ((tid = jsLexGetToken(ep, state)) != TOK_ID) {
            return STATE_ERR;
        }
        jsLexPutbackToken(ep, tid, ep->token);

        /*
            Parse the entire assignment or simple identifier declaration
         */
        if (parse(ep, STATE_DEC, flags) != STATE_DEC_DONE) {
            return STATE_ERR;
        }

        /*
            Peek at the next token, continue if comma seen
         */
        tid = jsLexGetToken(ep, state);
        if (tid == TOK_SEMI) {
            return STATE_DEC_LIST_DONE;
        } else if (tid != TOK_COMMA) {
            return STATE_ERR;
        }
    } while (tid == TOK_COMMA);

    if (tid != TOK_SEMI) {
        return STATE_ERR;
    }
    return STATE_DEC_LIST_DONE;
}


/*
    Parse function arguments
 */
static int parseFunctionArgs(Js *ep, int state, int flags)
{
    int     tid, aid;

    assert(ep);

    do {
        state = parse(ep, STATE_RELEXP, flags);
        if (state == STATE_EOF || state == STATE_ERR) {
            return state;
        }
        if (state == STATE_RELEXP_DONE) {
            aid = wallocHandle(&ep->func->args);
            ep->func->args[aid] = sclone(ep->result);
            ep->func->nArgs++;
        }
        /*
            Peek at the next token, continue if more args (ie. comma seen)
         */
        tid = jsLexGetToken(ep, state);
        if (tid != TOK_COMMA) {
            jsLexPutbackToken(ep, tid, ep->token);
        }
    } while (tid == TOK_COMMA);

    if (tid != TOK_RPAREN && state != STATE_RELEXP_DONE) {
        return STATE_ERR;
    }
    return STATE_ARG_LIST_DONE;
}


/*
    Parse conditional expression (relational ops separated by ||, &&)
 */
static int parseCond(Js *ep, int state, int flags)
{
    char  *lhs, *rhs;
    int     tid, operator;

    assert(ep);

    setString(&ep->result, "");
    rhs = lhs = NULL;
    operator = 0;

    do {
        /*
            Recurse to handle one side of a conditional. Accumulate the left hand side and the final result in ep->result.
         */
        state = parse(ep, STATE_RELEXP, flags);
        if (state != STATE_RELEXP_DONE) {
            state = STATE_ERR;
            break;
        }

        if (operator > 0) {
            setString(&rhs, ep->result);
            if (evalCond(ep, lhs, operator, rhs) < 0) {
                state = STATE_ERR;
                break;
            }
        }
        setString(&lhs, ep->result);

        tid = jsLexGetToken(ep, state);
        if (tid == TOK_LOGICAL) {
            operator = (int) *ep->token;

        } else if (tid == TOK_RPAREN || tid == TOK_SEMI) {
            jsLexPutbackToken(ep, tid, ep->token);
            state = STATE_COND_DONE;
            break;

        } else {
            jsLexPutbackToken(ep, tid, ep->token);
        }

    } while (state == STATE_RELEXP_DONE);

    if (lhs) {
        wfree(lhs);
    }

    if (rhs) {
        wfree(rhs);
    }
    return state;
}


/*
    Parse expression (leftHandSide operator rightHandSide)
 */
static int parseExpr(Js *ep, int state, int flags)
{
    char  *lhs, *rhs;
    int     rel, tid;

    assert(ep);

    setString(&ep->result, "");
    rhs = lhs = NULL;
    rel = 0;
    tid = 0;

    do {
        /*
            This loop will handle an entire expression list. We call parse to evalutate each term which returns the
            result in ep->result.  
         */
        if (tid == TOK_LOGICAL) {
            if ((state = parse(ep, STATE_RELEXP, flags)) != STATE_RELEXP_DONE) {
                state = STATE_ERR;
                break;
            }
        } else {
            if ((state = parse(ep, STATE_EXPR, flags)) != STATE_EXPR_DONE) {
                state = STATE_ERR;
                break;
            }
        }

        if (rel > 0) {
            setString(&rhs, ep->result);
            if (tid == TOK_LOGICAL) {
                if (evalCond(ep, lhs, rel, rhs) < 0) {
                    state = STATE_ERR;
                    break;
                }
            } else {
                if (evalExpr(ep, lhs, rel, rhs) < 0) {
                    state = STATE_ERR;
                    break;
                }
            }
        }
        setString(&lhs, ep->result);

        if ((tid = jsLexGetToken(ep, state)) == TOK_EXPR ||
             tid == TOK_INC_DEC || tid == TOK_LOGICAL) {
            rel = (int) *ep->token;

        } else {
            jsLexPutbackToken(ep, tid, ep->token);
            state = STATE_RELEXP_DONE;
        }

    } while (state == STATE_EXPR_DONE);

    if (rhs) {
        wfree(rhs);
    }
    if (lhs) {
        wfree(lhs);
    }
    return state;
}


/*
    Evaluate a condition. Implements &&, ||, !
 */
static int evalCond(Js *ep, char *lhs, int rel, char *rhs)
{
    char  buf[16];
    int     l, r, lval;

    assert(lhs);
    assert(rhs);
    assert(rel > 0);

    lval = 0;
    if (isdigit((uchar) *lhs) && isdigit((uchar) *rhs)) {
        l = atoi(lhs);
        r = atoi(rhs);
        switch (rel) {
        case COND_AND:
            lval = l && r;
            break;
        case COND_OR:
            lval = l || r;
            break;
        default:
            jsError(ep, "Bad operator %d", rel);
            return -1;
        }
    } else {
        if (!isdigit((uchar) *lhs)) {
            jsError(ep, "Conditional must be numeric", lhs);
        } else {
            jsError(ep, "Conditional must be numeric", rhs);
        }
    }
    itosbuf(buf, sizeof(buf), lval, 10);
    setString(&ep->result, buf);
    return 0;
}


/*
    Evaluate an operation
 */
static int evalExpr(Js *ep, char *lhs, int rel, char *rhs)
{
    char  *cp, buf[16];
    int     numeric, l, r, lval;

    assert(lhs);
    assert(rhs);
    assert(rel > 0);

    /*
        All of the characters in the lhs and rhs must be numeric
     */
    numeric = 1;
    for (cp = lhs; *cp; cp++) {
        if (!isdigit((uchar) *cp)) {
            numeric = 0;
            break;
        }
    }
    if (numeric) {
        for (cp = rhs; *cp; cp++) {
            if (!isdigit((uchar) *cp)) {
                numeric = 0;
                break;
            }
        }
    }
    if (numeric) {
        l = atoi(lhs);
        r = atoi(rhs);
        switch (rel) {
        case EXPR_PLUS:
            lval = l + r;
            break;
        case EXPR_INC:
            lval = l + 1;
            break;
        case EXPR_MINUS:
            lval = l - r;
            break;
        case EXPR_DEC:
            lval = l - 1;
            break;
        case EXPR_MUL:
            lval = l * r;
            break;
        case EXPR_DIV:
            if (r != 0) {
                lval = l / r;
            } else {
                lval = 0;
            }
            break;
        case EXPR_MOD:
            if (r != 0) {
                lval = l % r;
            } else {
                lval = 0;
            }
            break;
        case EXPR_LSHIFT:
            lval = l << r;
            break;
        case EXPR_RSHIFT:
            lval = l >> r;
            break;
        case EXPR_EQ:
            lval = l == r;
            break;
        case EXPR_NOTEQ:
            lval = l != r;
            break;
        case EXPR_LESS:
            lval = (l < r) ? 1 : 0;
            break;
        case EXPR_LESSEQ:
            lval = (l <= r) ? 1 : 0;
            break;
        case EXPR_GREATER:
            lval = (l > r) ? 1 : 0;
            break;
        case EXPR_GREATEREQ:
            lval = (l >= r) ? 1 : 0;
            break;
        case EXPR_BOOL_COMP:
            lval = (r == 0) ? 1 : 0;
            break;
        default:
            jsError(ep, "Bad operator %d", rel);
            return -1;
        }
    } else {
        switch (rel) {
        case EXPR_PLUS:
            clearString(&ep->result);
            appendString(&ep->result, lhs);
            appendString(&ep->result, rhs);
            return 0;
        case EXPR_LESS:
            lval = strcmp(lhs, rhs) < 0;
            break;
        case EXPR_LESSEQ:
            lval = strcmp(lhs, rhs) <= 0;
            break;
        case EXPR_GREATER:
            lval = strcmp(lhs, rhs) > 0;
            break;
        case EXPR_GREATEREQ:
            lval = strcmp(lhs, rhs) >= 0;
            break;
        case EXPR_EQ:
            lval = strcmp(lhs, rhs) == 0;
            break;
        case EXPR_NOTEQ:
            lval = strcmp(lhs, rhs) != 0;
            break;
        case EXPR_INC:
        case EXPR_DEC:
        case EXPR_MINUS:
        case EXPR_DIV:
        case EXPR_MOD:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
        default:
            jsError(ep, "Bad operator");
            return -1;
        }
    }
    itosbuf(buf, sizeof(buf), lval, 10);
    setString(&ep->result, buf);
    return 0;
}


/*
    Evaluate a function
 */
static int evalFunction(Js *ep)
{
    WebsKey   *sp;
    int     (*fn)(int jid, void *handle, int argc, char **argv);

    if ((sp = hashLookup(ep->functions, ep->func->fname)) == NULL) {
        jsError(ep, "Undefined procedure %s", ep->func->fname);
        return -1;
    }
    fn = (int (*)(int, void*, int, char**)) sp->content.value.symbol;
    if (fn == NULL) {
        jsError(ep, "Undefined procedure %s", ep->func->fname);
        return -1;
    }
    return (*fn)(ep->jid, ep->userHandle, ep->func->nArgs, ep->func->args);
}


/*
    Output a parse js_error message
 */
PUBLIC void jsError(Js *ep, char* fmt, ...)
{
    va_list     args;
    JsInput     *ip;
    char      *errbuf, *msgbuf;

    assert(ep);
    assert(fmt);
    ip = ep->input;

    va_start(args, fmt);
    msgbuf = sfmtv(fmt, args);
    va_end(args);

    if (ep && ip) {
        errbuf = sfmt("%s\n At line %d, line => \n\n%s\n", msgbuf, ip->lineNumber, ip->line);
        wfree(ep->error);
        ep->error = errbuf;
    }
    wfree(msgbuf);
}


static void clearString(char **ptr)
{
    assert(ptr);

    if (*ptr) {
        wfree(*ptr);
    }
    *ptr = NULL;
}


static void setString(char **ptr, char *s)
{
    assert(ptr);

    if (*ptr) {
        wfree(*ptr);
    }
    *ptr = sclone(s);
}


static void appendString(char **ptr, char *s)
{
    ssize   len, oldlen, size;

    assert(ptr);

    if (*ptr) {
        len = strlen(s);
        oldlen = strlen(*ptr);
        size = (len + oldlen + 1) * sizeof(char);
        *ptr = wrealloc(*ptr, size);
        strcpy(&(*ptr)[oldlen], s);
    } else {
        *ptr = sclone(s);
    }
}


/*
    Define a function
 */
PUBLIC int jsSetGlobalFunction(int jid, char *name, JsProc fn)
{
    Js    *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return jsSetGlobalFunctionDirect(ep->functions, name, fn);
}


/*
    Define a function directly into the function symbol table.
 */
PUBLIC int jsSetGlobalFunctionDirect(WebsHash functions, char *name, JsProc fn)
{
    if (hashEnter(functions, name, valueSymbol(fn), 0) == NULL) {
        return -1;
    }
    return 0;
}


/*
    Remove ("undefine") a function
 */
PUBLIC int jsRemoveGlobalFunction(int jid, char *name)
{
    Js    *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return hashDelete(ep->functions, name);
}


PUBLIC void *jsGetGlobalFunction(int jid, char *name)
{
    Js      *ep;
    WebsKey *sp;
    int     (*fn)(int jid, void *handle, int argc, char **argv);

    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }

    if ((sp = hashLookup(ep->functions, name)) != NULL) {
        fn = (int (*)(int, void*, int, char**)) sp->content.value.symbol;
        return (void*) fn;
    }
    return NULL;
}


/*
    Utility routine to crack Javascript arguments. Return the number of args
    seen. This routine only supports %s and %d type args.
  
    Typical usage:
  
        if (jsArgs(argc, argv, "%s %d", &name, &age) < 2) {
            error("Insufficient args");
            return -1;
        }
 */
PUBLIC int jsArgs(int argc, char **argv, char *fmt, ...)
{
    va_list vargs;
    char  *cp, **sp;
    int     *ip;
    int     argn;

    va_start(vargs, fmt);

    if (argv == NULL) {
        return 0;
    }
    for (argn = 0, cp = fmt; cp && *cp && argv[argn]; ) {
        if (*cp++ != '%') {
            continue;
        }
        switch (*cp) {
        case 'd':
            ip = va_arg(vargs, int*);
            *ip = atoi(argv[argn]);
            break;

        case 's':
            sp = va_arg(vargs, char**);
            *sp = argv[argn];
            break;

        default:
            /*
                Unsupported
             */
            assert(0);
        }
        argn++;
    }

    va_end(vargs);
    return argn;
}


PUBLIC void jsSetUserHandle(int jid, void* handle)
{
    Js    *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    ep->userHandle = handle;
}


void* jsGetUserHandle(int jid)
{
    Js    *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }
    return ep->userHandle;
}


PUBLIC int jsGetLineNumber(int jid)
{
    Js    *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return ep->input->lineNumber;
}


PUBLIC void jsSetResult(int jid, char *s)
{
    Js    *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    setString(&ep->result, s);
}


PUBLIC char *jsGetResult(int jid)
{
    Js    *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }
    return ep->result;
}

/*
    Set a variable. Note: a variable with a value of NULL means declared but undefined. The value is defined in the
    top-most variable frame.  
 */
PUBLIC void jsSetVar(int jid, char *var, char *value)
{
    Js          *ep;
    WebsValue   v;

    assert(var && *var);

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    if (value == NULL) {
        v = valueString(value, 0);
    } else {
        v = valueString(value, VALUE_ALLOCATE);
    }
    hashEnter(ep->variables[ep->variableMax - 1] - JS_OFFSET, var, v, 0);
}


/*
    Set a local variable. Note: a variable with a value of NULL means declared but undefined. The value is defined in
    the top-most variable frame.  
 */
PUBLIC void jsSetLocalVar(int jid, char *var, char *value)
{
    Js          *ep;
    WebsValue   v;

    assert(var && *var);

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    if (value == NULL) {
        v = valueString(value, 0);
    } else {
        v = valueString(value, VALUE_ALLOCATE);
    }
    hashEnter(ep->variables[ep->variableMax - 1] - JS_OFFSET, var, v, 0);
}


/*
    Set a global variable. Note: a variable with a value of NULL means declared but undefined. The value is defined in
    the global variable frame.  
 */
PUBLIC void jsSetGlobalVar(int jid, char *var, char *value)
{
    Js          *ep;
    WebsValue   v;

    assert(var && *var);

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    if (value == NULL) {
        v = valueString(value, 0);
    } else {
        v = valueString(value, VALUE_ALLOCATE);
    }
    hashEnter(ep->variables[0] - JS_OFFSET, var, v, 0);
}


/*
    Get a variable
 */
PUBLIC int jsGetVar(int jid, char *var, char **value)
{
    Js          *ep;
    WebsKey     *sp;
    int         i;

    assert(var && *var);
    assert(value);

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    i = ep->variableMax - 1;
    if ((sp = hashLookup(ep->variables[i] - JS_OFFSET, var)) == NULL) {
        i = 0;
        if ((sp = hashLookup(ep->variables[0] - JS_OFFSET, var)) == NULL) {
            return -1;
        }
    }
    assert(sp->content.type == string);
    *value = sp->content.value.string;
    return i;
}


WebsHash jsGetVariableTable(int jid)
{
    Js    *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return *ep->variables;
}


WebsHash jsGetFunctionTable(int jid)
{
    Js    *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return ep->functions;
}


/*
    Free an argument list
 */
static void freeFunc(JsFun *func)
{
    int i;

    for (i = func->nArgs - 1; i >= 0; i--) {
        wfree(func->args[i]);
        func->nArgs = wfreeHandle(&func->args, i);
    }
    if (func->fname) {
        wfree(func->fname);
        func->fname = NULL;
    }
}


/*
    Get Javascript engine pointer
 */
static Js *jsPtr(int jid)
{
    assert(0 <= jid && jid < jsMax);

    if (jid < 0 || jid >= jsMax || jsHandles[jid] == NULL) {
        jsError(NULL, "Bad handle %d", jid);
        return NULL;
    }
    return jsHandles[jid];
}


/*
    This function removes any new lines.  Used for else cases, etc.
 */
static void jsRemoveNewlines(Js *ep, int state)
{
    int tid;

    do {
        tid = jsLexGetToken(ep, state);
    } while (tid == TOK_NEWLINE);
    jsLexPutbackToken(ep, tid, ep->token);
}


PUBLIC int jsLexOpen(Js *ep)
{
    return 0;
}


PUBLIC void jsLexClose(Js *ep)
{
}


PUBLIC int jsLexOpenScript(Js *ep, char *script)
{
    JsInput     *ip;

    assert(ep);
    assert(script);

    if ((ep->input = walloc(sizeof(JsInput))) == NULL) {
        return -1;
    }
    ip = ep->input;
    memset(ip, 0, sizeof(*ip));

    assert(ip);
    assert(ip->putBackToken == NULL);
    assert(ip->putBackTokenId == 0);

    /*
        Create the parse token buffer and script buffer
     */
    if (bufCreate(&ip->tokbuf, JS_INC, -1) < 0) {
        return -1;
    }
    if (bufCreate(&ip->script, JS_SCRIPT_INC, -1) < 0) {
        return -1;
    }
    /*
        Put the Javascript into a ring queue for easy parsing
     */
    bufPutStr(&ip->script, script);

    ip->lineNumber = 1;
    ip->lineLength = 0;
    ip->lineColumn = 0;
    ip->line = NULL;

    return 0;
}


PUBLIC void jsLexCloseScript(Js *ep)
{
    JsInput     *ip;

    assert(ep);

    ip = ep->input;
    assert(ip);

    if (ip->putBackToken) {
        wfree(ip->putBackToken);
        ip->putBackToken = NULL;
    }
    ip->putBackTokenId = 0;

    if (ip->line) {
        wfree(ip->line);
        ip->line = NULL;
    }
    bufFree(&ip->tokbuf);
    bufFree(&ip->script);
    wfree(ip);
}


PUBLIC void jsLexSaveInputState(Js *ep, JsInput *state)
{
    JsInput     *ip;

    assert(ep);

    ip = ep->input;
    assert(ip);

    *state = *ip;
    if (ip->putBackToken) {
        state->putBackToken = sclone(ip->putBackToken);
    }
}


PUBLIC void jsLexRestoreInputState(Js *ep, JsInput *state)
{
    JsInput     *ip;

    assert(ep);

    ip = ep->input;
    assert(ip);

    ip->tokbuf = state->tokbuf;
    ip->script = state->script;
    ip->putBackTokenId = state->putBackTokenId;
    if (ip->putBackToken) {
        wfree(ip->putBackToken);
    }
    if (state->putBackToken) {
        ip->putBackToken = sclone(state->putBackToken);
    }
}


PUBLIC void jsLexFreeInputState(Js *ep, JsInput *state)
{
    if (state->putBackToken) {
        wfree(state->putBackToken);
        state->putBackToken = NULL;
    }
}


PUBLIC int jsLexGetToken(Js *ep, int state)
{
    ep->tid = getLexicalToken(ep, state);
    return ep->tid;
}


static int getLexicalToken(Js *ep, int state)
{
    WebsBuf     *tokq;
    JsInput     *ip;
    int         done, tid, c, quote, style;

    assert(ep);
    ip = ep->input;
    assert(ip);

    tokq = &ip->tokbuf;
    ep->tid = -1;
    tid = -1;
    ep->token = "";
    bufFlush(tokq);

    if (ip->putBackTokenId > 0) {
        bufPutStr(tokq, ip->putBackToken);
        tid = ip->putBackTokenId;
        ip->putBackTokenId = 0;
        ep->token = (char*) tokq->servp;
        return tid;
    }
    if ((c = inputGetc(ep)) < 0) {
        return TOK_EOF;
    }
    for (done = 0; !done; ) {
        switch (c) {
        case -1:
            return TOK_EOF;

        case ' ':
        case '\t':
        case '\r':
            do {
                if ((c = inputGetc(ep)) < 0)
                    break;
            } while (c == ' ' || c == '\t' || c == '\r');
            break;

        case '\n':
            return TOK_NEWLINE;

        case '(':
            tokenAddChar(ep, c);
            return TOK_LPAREN;

        case ')':
            tokenAddChar(ep, c);
            return TOK_RPAREN;

        case '{':
            tokenAddChar(ep, c);
            return TOK_LBRACE;

        case '}':
            tokenAddChar(ep, c);
            return TOK_RBRACE;

        case '+':
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c != '+' ) {
                inputPutback(ep, c);
                tokenAddChar(ep, EXPR_PLUS);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_INC);
            return TOK_INC_DEC;

        case '-':
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c != '-' ) {
                inputPutback(ep, c);
                tokenAddChar(ep, EXPR_MINUS);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_DEC);
            return TOK_INC_DEC;

        case '*':
            tokenAddChar(ep, EXPR_MUL);
            return TOK_EXPR;

        case '%':
            tokenAddChar(ep, EXPR_MOD);
            return TOK_EXPR;

        case '/':
            /*
                Handle the division operator and comments
             */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c != '*' && c != '/') {
                inputPutback(ep, c);
                tokenAddChar(ep, EXPR_DIV);
                return TOK_EXPR;
            }
            style = c;
            /*
                Eat comments. Both C and C++ comment styles are supported.
             */
            while (1) {
                if ((c = inputGetc(ep)) < 0) {
                    jsError(ep, "Syntax Error");
                    return TOK_ERR;
                }
                if (c == '\n' && style == '/') {
                    break;
                } else if (c == '*') {
                    c = inputGetc(ep);
                    if (style == '/') {
                        if (c == '\n') {
                            break;
                        }
                    } else {
                        if (c == '/') {
                            break;
                        }
                    }
                }
            }
            /*
                Continue looking for a token, so get the next character
             */
            if ((c = inputGetc(ep)) < 0) {
                return TOK_EOF;
            }
            break;

        case '<':                                   /* < and <= */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '<') {
                tokenAddChar(ep, EXPR_LSHIFT);
                return TOK_EXPR;
            } else if (c == '=') {
                tokenAddChar(ep, EXPR_LESSEQ);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_LESS);
            inputPutback(ep, c);
            return TOK_EXPR;

        case '>':                                   /* > and >= */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '>') {
                tokenAddChar(ep, EXPR_RSHIFT);
                return TOK_EXPR;
            } else if (c == '=') {
                tokenAddChar(ep, EXPR_GREATEREQ);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_GREATER);
            inputPutback(ep, c);
            return TOK_EXPR;

        case '=':                                   /* "==" */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '=') {
                tokenAddChar(ep, EXPR_EQ);
                return TOK_EXPR;
            }
            inputPutback(ep, c);
            return TOK_ASSIGNMENT;

        case '!':                                   /* "!=" or "!"*/
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '=') {
                tokenAddChar(ep, EXPR_NOTEQ);
                return TOK_EXPR;
            }
            inputPutback(ep, c);
            tokenAddChar(ep, EXPR_BOOL_COMP);
            return TOK_EXPR;

        case ';':
            tokenAddChar(ep, c);
            return TOK_SEMI;

        case ',':
            tokenAddChar(ep, c);
            return TOK_COMMA;

        case '|':                                   /* "||" */
            if ((c = inputGetc(ep)) < 0 || c != '|') {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            tokenAddChar(ep, COND_OR);
            return TOK_LOGICAL;

        case '&':                                   /* "&&" */
            if ((c = inputGetc(ep)) < 0 || c != '&') {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            tokenAddChar(ep, COND_AND);
            return TOK_LOGICAL;

        case '\"':                                  /* String quote */
        case '\'':
            quote = c;
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }

            while (c != quote) {
                /*
                    check for escape sequence characters
                 */
                if (c == '\\') {
                    c = inputGetc(ep);

                    if (isdigit((uchar) c)) {
                        /*
                            octal support, \101 maps to 65 = 'A'. put first char back so converter will work properly.
                         */
                        inputPutback(ep, c);
                        c = charConvert(ep, OCTAL, 3);

                    } else {
                        switch (c) {
                        case 'n':
                            c = '\n'; break;
                        case 'b':
                            c = '\b'; break;
                        case 'f':
                            c = '\f'; break;
                        case 'r':
                            c = '\r'; break;
                        case 't':
                            c = '\t'; break;
                        case 'x':
                            /*
                                hex support, \x41 maps to 65 = 'A'
                             */
                            c = charConvert(ep, HEX, 2);
                            break;
                        case 'u':
                            /*
                                unicode support, \x0401 maps to 65 = 'A'
                             */
                            c = charConvert(ep, HEX, 2);
                            c = c*16 + charConvert(ep, HEX, 2);

                            break;
                        case '\'':
                        case '\"':
                        case '\\':
                            break;
                        default:
                            jsError(ep, "Invalid Escape Sequence");
                            return TOK_ERR;
                        }
                    }
                    if (tokenAddChar(ep, c) < 0) {
                        return TOK_ERR;
                    }
                } else {
                    if (tokenAddChar(ep, c) < 0) {
                        return TOK_ERR;
                    }
                }
                if ((c = inputGetc(ep)) < 0) {
                    jsError(ep, "Unmatched Quote");
                    return TOK_ERR;
                }
            }
            return TOK_LITERAL;

        case '0': case '1': case '2': case '3': case '4': 
        case '5': case '6': case '7': case '8': case '9':
            do {
                if (tokenAddChar(ep, c) < 0) {
                    return TOK_ERR;
                }
                if ((c = inputGetc(ep)) < 0)
                    break;
            } while (isdigit((uchar) c));
            inputPutback(ep, c);
            return TOK_LITERAL;

        default:
            /*
                Identifiers or a function names
             */
            while (1) {
                if (c == '\\') {
                    /*
                        just ignore any \ characters.
                     */
                } else if (tokenAddChar(ep, c) < 0) {
                        break;
                }
                if ((c = inputGetc(ep)) < 0) {
                    break;
                }
                if (!isalnum((uchar) c) && c != '$' && c != '_' && c != '\\') {
                    break;
                }
            }
            if (! isalpha((uchar) *tokq->servp) && *tokq->servp != '$' && *tokq->servp != '_') {
                jsError(ep, "Invalid identifier %s", tokq->servp);
                return TOK_ERR;
            }
            /*
                Check for reserved words (only "if", "else", "var", "for" and "return" at the moment)
             */
            if (state == STATE_STMT) {
                if (strcmp(ep->token, "if") == 0) {
                    return TOK_IF;
                } else if (strcmp(ep->token, "else") == 0) {
                    return TOK_ELSE;
                } else if (strcmp(ep->token, "var") == 0) {
                    return TOK_VAR;
                } else if (strcmp(ep->token, "for") == 0) {
                    return TOK_FOR;
                } else if (strcmp(ep->token, "return") == 0) {
                    if ((c == ';') || (c == '(')) {
                        inputPutback(ep, c);
                    }
                    return TOK_RETURN;
                }
            }
            /* 
                Skip white space after token to find out whether this is a function or not.
             */ 
            while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                if ((c = inputGetc(ep)) < 0)
                    break;
            }

            tid = (c == '(') ? TOK_FUNCTION : TOK_ID;
            done++;
        }
    }

    /*
        Putback the last extra character for next time
     */
    inputPutback(ep, c);
    return tid;
}


PUBLIC void jsLexPutbackToken(Js *ep, int tid, char *string)
{
    JsInput *ip;

    assert(ep);
    ip = ep->input;
    assert(ip);

    if (ip->putBackToken) {
        wfree(ip->putBackToken);
    }
    ip->putBackTokenId = tid;
    ip->putBackToken = sclone(string);
}


static int tokenAddChar(Js *ep, int c)
{
    JsInput *ip;

    assert(ep);
    ip = ep->input;
    assert(ip);

    if (bufPutc(&ip->tokbuf, (char) c) < 0) {
        jsError(ep, "Token too big");
        return -1;
    }
    * ((char*) ip->tokbuf.endp) = '\0';
    ep->token = (char*) ip->tokbuf.servp;

    return 0;
}


static int inputGetc(Js *ep)
{
    JsInput     *ip;
    ssize       len;
    int         c;

    assert(ep);
    ip = ep->input;

    if ((len = bufLen(&ip->script)) == 0) {
        return -1;
    }
    c = bufGetc(&ip->script);
    if (c == '\n') {
        ip->lineNumber++;
        ip->lineColumn = 0;
    } else {
        if ((ip->lineColumn + 2) >= ip->lineLength) {
            ip->lineLength += JS_INC;
            ip->line = wrealloc(ip->line, ip->lineLength * sizeof(char));
        }
        ip->line[ip->lineColumn++] = c;
        ip->line[ip->lineColumn] = '\0';
    }
    return c;
}


static void inputPutback(Js *ep, int c)
{
    JsInput   *ip;

    assert(ep);

    ip = ep->input;
    bufInsertc(&ip->script, (char) c);
    /* Fix by Fred Sauer, 2002/12/23 */
    if (ip->lineColumn > 0) {
        ip->lineColumn-- ;
    }
    ip->line[ip->lineColumn] = '\0';
}


/*
    Convert a hex or octal character back to binary, return original char if not a hex digit
 */
static int charConvert(Js *ep, int base, int maxDig)
{
    int     i, c, lval, convChar;

    lval = 0;
    for (i = 0; i < maxDig; i++) {
        if ((c = inputGetc(ep)) < 0) {
            break;
        }
        /*
            Initialize to out of range value
         */
        convChar = base;
        if (isdigit((uchar) c)) {
            convChar = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            convChar = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            convChar = c - 'A' + 10;
        }
        /*
            if unexpected character then return it to buffer.
         */
        if (convChar >= base) {
            inputPutback(ep, c);
            break;
        }
        lval = (lval * base) + convChar;
    }
    return lval;
}

#endif /* ME_GOAHEAD_JAVASCRIPT */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/jst.c ************/


/*
    jst.c -- JavaScript templates
  
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/


#include    "js.h"

#if ME_GOAHEAD_JAVASCRIPT
/********************************** Locals ************************************/

static WebsHash websJstFunctions = -1;  /* Symbol table of functions */

/***************************** Forward Declarations ***************************/

static char *strtokcmp(char *s1, char *s2);
static char *skipWhite(char *s);

/************************************* Code ***********************************/
/*
    Process requests and expand all scripting commands. We read the entire web page into memory and then process. If
    you have really big documents, it is better to make them plain HTML files rather than Javascript web pages.
 */
static bool jstHandler(Webs *wp)
{
    WebsFileInfo    sbuf;
    char            *token, *lang, *result, *ep, *cp, *buf, *nextp, *last;
    ssize           len;
    int             rc, jid;

    assert(websValid(wp));
    assert(wp->filename && *wp->filename);
    assert(wp->ext && *wp->ext);

    buf = 0;
    if ((jid = jsOpenEngine(wp->vars, websJstFunctions)) < 0) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create JavaScript engine");
        goto done;
    }
    jsSetUserHandle(jid, wp);

    if (websPageStat(wp, &sbuf) < 0) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot stat %s", wp->filename);
        goto done;
    }
    if (websPageOpen(wp, O_RDONLY | O_BINARY, 0666) < 0) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot open URL: %s", wp->filename);
        goto done;
    }
    /*
        Create a buffer to hold the web page in-memory
     */
    len = sbuf.size;
    if ((buf = walloc(len + 1)) == NULL) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot get memory");
        goto done;
    }
    buf[len] = '\0';

    if (websPageReadData(wp, buf, len) != len) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Cant read %s", wp->filename);
        goto done;
    }
    websPageClose(wp);
    websWriteHeaders(wp, (ssize) -1, 0);
    websWriteHeader(wp, "Pragma", "no-cache");
    websWriteHeader(wp, "Cache-Control", "no-cache");
    websWriteEndHeaders(wp);

    /*
        Scan for the next "<%"
     */
    last = buf;
    for (rc = 0; rc == 0 && *last && ((nextp = strstr(last, "<%")) != NULL); ) {
        websWriteBlock(wp, last, (nextp - last));
        nextp = skipWhite(nextp + 2);
        /*
            Decode the language
         */
        token = "language";
        if ((lang = strtokcmp(nextp, token)) != NULL) {
            if ((cp = strtokcmp(lang, "=javascript")) != NULL) {
                /* Ignore */;
            } else {
                cp = nextp;
            }
            nextp = cp;
        }

        /*
            Find tailing bracket and then evaluate the script
         */
        if ((ep = strstr(nextp, "%>")) != NULL) {

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

                if (jsEval(jid, nextp, &result) == 0) {
                    /*
                         On an error, discard all output accumulated so far and store the error in the result buffer. 
                         Be careful if the user has called websError() already.
                     */
                    rc = -1;
                    if (websValid(wp)) {
                        if (result) {
                            websWrite(wp, "<h2><b>Javascript Error: %s</b></h2>\n", result);
                            websWrite(wp, "<pre>%s</pre>", nextp);
                            wfree(result);
                        } else {
                            websWrite(wp, "<h2><b>Javascript Error</b></h2>\n%s\n", nextp);
                        }
                        websWrite(wp, "</body></html>\n");
                        rc = 0;
                    }
                    goto done;
                }
            }

        } else {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Unterminated script in %s: \n", wp->filename);
            goto done;
        }
    }
    /*
        Output any trailing HTML page text
     */
    if (last && *last && rc == 0) {
        websWriteBlock(wp, last, strlen(last));
    }

/*
    Common exit and cleanup
 */
done:
    if (websValid(wp)) {
        websPageClose(wp);
        if (jid >= 0) {
            jsCloseEngine(jid);
        }
    }
    websDone(wp);
    wfree(buf);
    return 1;
}


static void closeJst()
{
    if (websJstFunctions != -1) {
        hashFree(websJstFunctions);
        websJstFunctions = -1;
    }
}


PUBLIC int websJstOpen()
{
    websJstFunctions = hashCreate(WEBS_HASH_INIT * 2);
    websDefineJst("write", websJstWrite);
    websDefineHandler("jst", 0, jstHandler, closeJst, 0);
    return 0;
}


/*
    Define a Javascript function. Bind an Javascript name to a C procedure.
 */
PUBLIC int websDefineJst(char *name, WebsJstProc fn)
{
    return jsSetGlobalFunctionDirect(websJstFunctions, name, (JsProc) fn);
}


/*
    Javascript write command. This implemements <% write("text"); %> command
 */
PUBLIC int websJstWrite(int jid, Webs *wp, int argc, char **argv)
{
    int     i;

    assert(websValid(wp));
    
    for (i = 0; i < argc; ) {
        assert(argv);
        if (websWriteBlock(wp, argv[i], strlen(argv[i])) < 0) {
            return -1;
        }
        if (++i < argc) {
            if (websWriteBlock(wp, " ", 2) < 0) {
                return -1;
            }
        }
    }
    return 0;
}


/*
    Find s2 in s1. We skip leading white space in s1.  Return a pointer to the location in s1 after s2 ends.
 */
static char *strtokcmp(char *s1, char *s2)
{
    ssize     len;

    s1 = skipWhite(s1);
    len = strlen(s2);
    for (len = strlen(s2); len > 0 && (tolower((uchar) *s1) == tolower((uchar) *s2)); len--) {
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


static char *skipWhite(char *s) 
{
    assert(s);

    if (s == NULL) {
        return s;
    }
    while (*s && isspace((uchar) *s)) {
        s++;
    }
    return s;
}

#endif /* ME_GOAHEAD_JAVASCRIPT */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/options.c ************/


/*
    options.c -- Options and Trace handler

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/*********************************** Code *************************************/

static bool optionsHandler(Webs *wp)
{
    assert(wp);

#if !ME_GOAHEAD_STEALTH
    if (smatch(wp->method, "OPTIONS")) {
        websSetStatus(wp, HTTP_CODE_OK);
        websWriteHeaders(wp, 0, 0);
        websWriteHeader(wp, "Allow", "DELETE,GET,HEAD,OPTIONS,POST,PUT,TRACE");
        websWriteEndHeaders(wp);
        websDone(wp);
        return 1;

    } else if (smatch(wp->method, "TRACE")) {
        websSetStatus(wp, HTTP_CODE_OK);
        websWriteHeaders(wp, 0, 0);
        websWriteEndHeaders(wp);
        websWrite(wp, "%s %s %s\r\n", wp->method, wp->url, wp->protoVersion);
        websDone(wp);
        return 1;
    }
#endif
    websResponse(wp, HTTP_CODE_NOT_ACCEPTABLE, "Unsupported method");
    return 1;
}


PUBLIC int websOptionsOpen()
{
    websDefineHandler("options", 0, optionsHandler, 0, 0);
    return 0;
}


/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/osdep.c ************/


/*
    osdep.c -- O/S dependant code

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/



/*********************************** Defines **********************************/

#if ME_WIN_LIKE
    static HINSTANCE appInstance;
    PUBLIC void syslog(int priority, char *fmt, ...);
#endif

/************************************* Code ***********************************/

PUBLIC int websOsOpen()
{
#if SOLARIS
    openlog(ME_NAME, LOG_LOCAL0);
#elif ME_UNIX_LIKE
    openlog(ME_NAME, 0, LOG_LOCAL0);
#endif
#if WINDOWS || VXWORKS || TIDSP
    rand();
#else
    random();
#endif
    return 0;
}


PUBLIC void websOsClose()
{
#if ME_UNIX_LIKE
    closelog();
#endif
}


PUBLIC char *websTempFile(char *dir, char *prefix)
{
    static int count = 0;

    if (!dir || *dir == '\0') {
#if WINCE
        dir = "/Temp";
#elif ME_WIN_LIKE
        dir = getenv("TEMP");
#elif VXWORKS
        dir = ".";
#else
        dir = "/tmp";
#endif
    }
    if (!prefix) {
        prefix = "tmp";
    }
    return sfmt("%s/%s-%d.tmp", dir, prefix, count++);
}


#if VXWORKS
/*
    Get absolute path.  In VxWorks, functions like chdir, ioctl for mkdir and ioctl for rmdir, require an absolute path.
    This function will take the path argument and convert it to an absolute path.  It is the caller's responsibility to
    deallocate the returned string. 
 */
static char *getAbsolutePath(char *path)
{
#if _WRS_VXWORKS_MAJOR >= 6
    const char  *tail;
#else
    char        *tail;
#endif
    char  *dev;

    /*
        Determine if path is relative or absolute.  If relative, prepend the current working directory to the name.
        Otherwise, use it.  Note the getcwd call below must not be getcwd or else we go into an infinite loop
    */
    if (iosDevFind(path, &tail) != NULL && path != tail) {
        return sclone(path);
    }
    dev = walloc(ME_GOAHEAD_LIMIT_FILENAME);
    getcwd(dev, ME_GOAHEAD_LIMIT_FILENAME);
    strcat(dev, "/");
    strcat(dev, path);
    return dev;
}


PUBLIC int vxchdir(char *dirname)
{
    char  *path;
    int     rc;

    path = getAbsolutePath(dirname);
    #undef chdir
    rc = chdir(path);
    wfree(path);
    return rc;
}


#endif


#if ECOS
PUBLIC int send(int s, const void *buf, size_t len, int flags)
{
    return write(s, buf, len);
}


PUBLIC int recv(int s, void *buf, size_t len, int flags)
{
    return read(s, buf, len);
}
#endif


#if ME_WIN_LIKE
PUBLIC void websSetInst(HINSTANCE inst)
{
    appInstance = inst;
}


HINSTANCE websGetInst()
{
    return appInstance;
}


PUBLIC void syslog(int priority, char *fmt, ...)
{
    va_list     args;
    HKEY        hkey;
    void        *event;
    long        errorType;
    ulong       exists;
    char        *buf, logName[ME_GOAHEAD_LIMIT_STRING], *lines[9], *cp, *value;
    int         type;
    static int  once = 0;

    va_start(args, fmt);
    buf = sfmtv(fmt, args);
    va_end(args);

    cp = &buf[slen(buf) - 1];
    while (*cp == '\n' && cp > buf) {
        *cp-- = '\0';
    }
    type = EVENTLOG_ERROR_TYPE;
    lines[0] = buf;
    lines[1] = 0;
    lines[2] = lines[3] = lines[4] = lines[5] = 0;
    lines[6] = lines[7] = lines[8] = 0;

    if (once == 0) {
        /*  Initialize the registry */
        once = 1;
        hkey = 0;

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, logName, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, &exists) ==
                ERROR_SUCCESS) {
            value = "%SystemRoot%\\System32\\netmsg.dll";
            if (RegSetValueEx(hkey, "EventMessageFile", 0, REG_EXPAND_SZ, 
                    (uchar*) value, (int) slen(value) + 1) != ERROR_SUCCESS) {
                RegCloseKey(hkey);
                wfree(buf);
                return;
            }
            errorType = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
            if (RegSetValueEx(hkey, "TypesSupported", 0, REG_DWORD, (uchar*) &errorType, sizeof(DWORD)) != 
                    ERROR_SUCCESS) {
                RegCloseKey(hkey);
                wfree(buf);
                return;
            }
            RegCloseKey(hkey);
        }
    }
    event = RegisterEventSource(0, ME_NAME);
    if (event) {
        ReportEvent(event, EVENTLOG_ERROR_TYPE, 0, 3299, NULL, sizeof(lines) / sizeof(char*), 0, (LPCSTR*) lines, 0);
        DeregisterEventSource(event);
    }
    wfree(buf);
}


PUBLIC void sleep(int secs)
{
    Sleep(secs / 1000);
}
#endif

/*
    "basename" returns a pointer to the last component of a pathname LINUX, LynxOS and Mac OS X have their own basename
 */
#if !ME_UNIX_LIKE
PUBLIC char *basename(char *name)
{
    char  *cp;

#if ME_WIN_LIKE
    if (((cp = strrchr(name, '\\')) == NULL) && ((cp = strrchr(name, '/')) == NULL)) {
        return name;
#else
    if ((cp = strrchr(name, '/')) == NULL) {
        return name;
#endif
    } else if (*(cp + 1) == '\0' && cp == name) {
        return name;
    } else if (*(cp + 1) == '\0' && cp != name) {
        return "";
    } else {
        return ++cp;
    }
}
#endif


#if TIDSP
char *inet_ntoa(struct in_addr addr) 
{
    char    result[16];
    uchar   *bytes;
   
    bytes = (uchar*) &addr;
    sprintf(result, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
    return result;
}


struct hostent* gethostbyname(char *name)
{
    static char buffer[ME_MAX_PATH];

    if(!DNSGetHostByName(name, buffer, ME_MAX_PATH)) {
        return 0;
    }
    return (struct hostent*) buffer;
}


ulong hostGetByName(char *name)
{
    struct _hostent *ent;

    ent = gethostbyname(name);
    return ent->h_addr[0];
}


int gethostname(char *host, int bufSize)
{
    return DNSGetHostname(host, bufSize);
}


int closesocket(SOCKET s)
{
    return fdClose(s);
}


int select(int maxfds, fd_set *readFds, fd_set *writeFds, fd_set *exceptFds, struct timeval *timeVal)
{
    return fdSelect(maxfds, readFds, writeFds, exceptFds, timeVal);
}

#endif /* TIDSP */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/rom-documents.c ************/


/*
   rom-documents.c 
   Compiled by webcomp: Mon Jan 14 14:13:54 2013
 */



#if ME_ROM
WebsRomIndex websRomIndex[] = {
	{ 0, 0, 0 }
};
#else
WebsRomIndex websRomIndex[] = {
	{ 0, 0, 0 }
};
#endif



/********* Start of file ../../../src/route.c ************/


/*
    route.c -- Route Management

    This module implements the loading of a route configuration file
    and the routing of requests.

    The route configuration is loaded form a text file that uses the schema (see route.txt)
        uri: type: uri: method: ability [ability...]: redirect
        user: name: password: role [role...]
        role: name: ability [ability...]

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************* Includes ***********************************/



/*********************************** Locals ***********************************/

static WebsRoute **routes = 0;
static WebsHash handlers = -1;
static int routeCount = 0;
static int routeMax = 0;

#define WEBS_MAX_ROUTE 16               /* Maximum passes over route set */

/********************************** Forwards **********************************/

static bool continueHandler(Webs *wp);
static void freeRoute(WebsRoute *route);
static void growRoutes();
static int lookupRoute(char *uri);
static bool redirectHandler(Webs *wp);

/************************************ Code ************************************/
/*
    Route the request. If wp->route is already set, test routes after that route
 */

PUBLIC void websRouteRequest(Webs *wp)
{
    WebsRoute   *route;
    WebsHandler *handler;
    ssize       plen, len;
    bool        safeMethod;
    int         i;

    assert(wp);
    assert(wp->path);
    assert(wp->method);
    assert(wp->protocol);

    safeMethod = smatch(wp->method, "POST") || smatch(wp->method, "GET") || smatch(wp->method, "HEAD");
    plen = slen(wp->path);

    /*
        Resume routine from last matched route. This permits the legacy service() callbacks to return false
        and continue routing.
     */
    if (wp->route && !(wp->flags & WEBS_REROUTE)) {
        for (i = 0; i < routeCount; i++) {
            if (wp->route == routes[i]) {
                i++;
                break;
            }
        }
        if (i >= routeCount) {
            i = 0;
        }
    } else {
        i = 0;
    }
    wp->route = 0;

    for (; i < routeCount; i++) {
        route = routes[i];
        assert(route->prefix && route->prefixLen > 0);

        if (plen < route->prefixLen) continue;
        len = min(route->prefixLen, plen);
        trace(5, "Examine route %s", route->prefix);
        /*
            Match route
         */
        if (route->protocol && !smatch(route->protocol, wp->protocol)) {
            trace(5, "Route %s does not match protocol %s", route->prefix, wp->protocol);
            continue;
        }
        if (route->methods >= 0) {
            if (!hashLookup(route->methods, wp->method)) {
                trace(5, "Route %s does not match method %s", route->prefix, wp->method);
                continue;
            }
        } else if (!safeMethod) {
            continue;
        }
        if (route->extensions >= 0 && (wp->ext == 0 || !hashLookup(route->extensions, &wp->ext[1]))) {
            trace(5, "Route %s doesn match extension %s", route->prefix, wp->ext ? wp->ext : "");
            continue;
        }
        if (strncmp(wp->path, route->prefix, len) == 0) {
            wp->route = route;
#if ME_GOAHEAD_AUTH
            if (route->authType && !websAuthenticate(wp)) {
                return;
            }
            if (route->abilities >= 0 && !websCan(wp, route->abilities)) {
                return;
            }
#endif
            if ((handler = route->handler) == 0) {
                continue;
            }
            if (!handler->match || (*handler->match)(wp)) {
                /* Handler matches */
                return;
            }
            wp->route = 0;
            if (wp->flags & WEBS_REROUTE) {
                wp->flags &= ~WEBS_REROUTE;
                if (++wp->routeCount >= WEBS_MAX_ROUTE) {
                    break;
                }
                i = 0;
            }
        }
    }
    if (wp->routeCount >= WEBS_MAX_ROUTE) {
        error("Route loop for %s", wp->url);
    }
    websError(wp, HTTP_CODE_NOT_FOUND, "Cannot find suitable route for request.");
    assert(wp->route == 0);
}


PUBLIC bool websRunRequest(Webs *wp)
{
    WebsRoute   *route;

    assert(wp);
    assert(wp->path);
    assert(wp->method);
    assert(wp->protocol);
    assert(wp->route);

    if ((route = wp->route) == 0) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Configuration error - no route for request");
        return 1;
    }
    assert(route->handler);

    if (!wp->filename || route->dir) {
        wfree(wp->filename);
        wp->filename = sfmt("%s%s", route->dir ? route->dir : websGetDocuments(), wp->path);
    }
    if (!(wp->flags & WEBS_VARS_ADDED)) {
        if (wp->query && *wp->query) {
            websSetQueryVars(wp);
        }
        if (wp->flags & WEBS_FORM) {
            websSetFormVars(wp);
        }
        wp->flags |= WEBS_VARS_ADDED;
    }
    wp->state = WEBS_RUNNING;
    trace(5, "Route %s calls handler %s", route->prefix, route->handler->name);

#if ME_GOAHEAD_LEGACY
    if (route->handler->flags & WEBS_LEGACY_HANDLER) {
        return (*(WebsLegacyHandlerProc) route->handler->service)(wp, route->prefix, route->dir, route->flags) == 0;
    } else
#endif
    return (*route->handler->service)(wp);
}


#if ME_GOAHEAD_AUTH
static bool can(Webs *wp, char *ability)
{
    assert(wp);
    assert(ability && *ability);

    if (wp->user && hashLookup(wp->user->abilities, ability)) {
        return 1;
    }
    return 0;
}


PUBLIC bool websCan(Webs *wp, WebsHash abilities) 
{
    WebsKey     *key;
    char        *ability, *cp, *start, abuf[ME_GOAHEAD_LIMIT_STRING];

    assert(wp);
    assert(abilities >= 0);

    if (!wp->user) {
        if (wp->authType) {
            if (!wp->username) {
                websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. User not logged in.");
                return 0;
            }
            if ((wp->user = websLookupUser(wp->username)) == 0) {
                websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. Unknown user.");
                return 0;
            }
        }
    }
    if (abilities >= 0) {
        if (!wp->user && wp->username) {
            wp->user = websLookupUser(wp->username);
        }
        assert(abilities);
        for (key = hashFirst(abilities); key; key = hashNext(abilities, key)) {
            ability = key->name.value.string;
            if ((cp = strchr(ability, '|')) != 0) {
                /*
                    Examine a set of alternative abilities. Need only one to match
                 */ 
                start = ability;
                do {
                    sncopy(abuf, sizeof(abuf), start, cp - start);
                    if (can(wp, abuf)) {
                        break;
                    }
                    start = &cp[1];
                } while ((cp = strchr(start, '|')) != 0);
                if (!cp) {
                    websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. Insufficient capabilities.");
                    return 0;
                }
            } else if (!can(wp, ability)) {
                websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. Insufficient capabilities.");
                return 0;
            }
        }
    }
    return 1;
}
#endif


#if KEEP
PUBLIC bool websCanString(Webs *wp, char *abilities) 
{
    WebsUser    *user;
    char        *ability, *tok;

    if (!wp->user) {
        if (!wp->username) {
            return 0;
        }
        if ((user = websLookupUser(wp->username)) == 0) {
            trace(2, "Cannot find user %s", wp->username);
            return 0;
        }
    }
    abilities = sclone(abilities);
    for (ability = stok(abilities, " \t,", &tok); ability; ability = stok(NULL, " \t,", &tok)) {
        if (hashLookup(wp->user->abilities, ability) == 0) {
            wfree(abilities);
            return 0;
        }
    }
    wfree(abilities);
    return 1;
}
#endif


/*
    If pos is < 0, then add to the end. Otherwise insert at specified position
 */
WebsRoute *websAddRoute(char *uri, char *handler, int pos)
{
    WebsRoute   *route;
    WebsKey     *key;

    if (uri == 0 || *uri == '\0') {
        error("Route has bad URI");
        return 0;
    }
    if ((route = walloc(sizeof(WebsRoute))) == 0) {
        return 0;
    }
    memset(route, 0, sizeof(WebsRoute));
    route->prefix = sclone(uri);
    route->prefixLen = slen(uri);
    route->abilities = route->extensions = route->methods = route->redirects = -1;
    if (!handler) {
        handler = "file";
    }
    if ((key = hashLookup(handlers, handler)) == 0) {
        error("Cannot find route handler %s", handler);
        return 0;
    }
    route->handler = key->content.value.symbol;

#if ME_GOAHEAD_AUTH
    route->verify = websGetPasswordStoreVerify();
#endif
    growRoutes();
    if (pos < 0) {
        pos = routeCount;
    } 
    if (pos < routeCount) {
        memmove(&routes[pos + 1], &routes[pos], sizeof(WebsRoute*) * routeCount - pos);
    }
    routes[pos] = route;
    routeCount++;
    return route;
}


PUBLIC int websSetRouteMatch(WebsRoute *route, char *dir, char *protocol, WebsHash methods, WebsHash extensions, 
        WebsHash abilities, WebsHash redirects)
{
    assert(route);

    if (dir) {
        route->dir = sclone(dir);
    }
    route->protocol = protocol ? sclone(protocol) : 0;
    route->abilities = abilities;
    route->extensions = extensions;
    route->methods = methods;
    route->redirects = redirects;
    return 0;
}


static void growRoutes()
{
    if (routeCount >= routeMax) {
        routeMax += 16;
        if ((routes = wrealloc(routes, sizeof(WebsRoute*) * routeMax)) == 0) {
            error("Cannot grow routes");
        }
    }
}


static int lookupRoute(char *uri) 
{
    WebsRoute   *route;
    int         i;

    assert(uri && *uri);

    for (i = 0; i < routeCount; i++) {
        route = routes[i];
        if (smatch(route->prefix, uri)) {
            return i;
        }
    }
    return -1;
}


static void freeRoute(WebsRoute *route)
{
    assert(route);

    if (route->abilities >= 0) {
        hashFree(route->abilities);
    }
    if (route->extensions >= 0) {
        hashFree(route->extensions);
    }
    if (route->methods >= 0) {
        hashFree(route->methods);
    }
    if (route->redirects >= 0) {
        hashFree(route->redirects);
    }
    wfree(route->prefix);
    wfree(route->dir);
    wfree(route->protocol);
    wfree(route->authType);
    wfree(route->handler);
    wfree(route);
}


PUBLIC int websRemoveRoute(char *uri) 
{
    int         i;

    assert(uri && *uri);

    if ((i = lookupRoute(uri)) < 0) {
        return -1;
    }
    freeRoute(routes[i]);
    for (; i < routeCount; i++) {
        routes[i] = routes[i+1];
    }
    routeCount--;
    return 0;
}


PUBLIC int websOpenRoute(char *path) 
{
    if ((handlers = hashCreate(-1)) < 0) {
        return -1;
    }
    websDefineHandler("continue", continueHandler, 0, 0, 0);
    websDefineHandler("redirect", redirectHandler, 0, 0, 0);
    return 0;
}


PUBLIC void websCloseRoute() 
{
    WebsHandler *handler;
    WebsKey     *key;

    if (handlers >= 0) {
        for (key = hashFirst(handlers); key; key = hashNext(handlers, key)) {
            handler = key->content.value.symbol;
            if (handler->close) {
                (*handler->close)();
            }
            wfree(handler->name);
        }
        hashFree(handlers);
        handlers = -1;
    }
    if (routes) {
        wfree(routes);
        routes = 0;
    }
    routeCount = routeMax = 0;
}


PUBLIC int websDefineHandler(char *name, WebsHandlerProc match, WebsHandlerProc service, WebsHandlerClose close, int flags)
{
    WebsHandler     *handler;

    assert(name && *name);

    if ((handler = walloc(sizeof(WebsHandler))) == 0) {
        return -1;
    }
    memset(handler, 0, sizeof(WebsHandler));
    handler->name = sclone(name);
    handler->match = match;
    handler->service = service;
    handler->close = close;
    handler->flags = flags;

    hashEnter(handlers, name, valueSymbol(handler), 0);
    return 0;
}


static void addOption(WebsHash *hash, char *keys, char *value)
{
    char    *sep, *key, *tok;

    if (*hash < 0) {
        *hash = hashCreate(-1);
    }
    sep = " \t,|";
    for (key = stok(keys, sep, &tok); key; key = stok(0, sep, &tok)) {
        if (strcmp(key, "none") == 0) {
            continue;
        }
        if (value == 0) {
            hashEnter(*hash, key, valueInteger(0), 0);
        } else {
            hashEnter(*hash, key, valueString(value, VALUE_ALLOCATE), 0);
        }
    }
}


/*
    Load route and authentication configuration files
 */
PUBLIC int websLoad(char *path)
{
    WebsRoute   *route;
    WebsHash    abilities, extensions, methods, redirects;
    char        *buf, *line, *kind, *next, *auth, *dir, *handler, *protocol, *uri, *option, *key, *value, *status;
    char        *redirectUri, *token;
    int         rc;
    
    assert(path && *path);

    rc = 0;
    if ((buf = websReadWholeFile(path)) == 0) {
        error("Cannot open config file %s", path);
        return -1;
    }
    for (line = stok(buf, "\r\n", &token); line; line = stok(NULL, "\r\n", &token)) {
        kind = stok(line, " \t", &next);
        if (kind == 0 || *kind == '\0' || *kind == '#') {
            continue;
        }
        if (smatch(kind, "route")) {
            auth = dir = handler = protocol = uri = 0;
            abilities = extensions = methods = redirects = -1;
            while ((option = stok(NULL, " \t\r\n", &next)) != 0) {
                key = stok(option, "=", &value);
                if (smatch(key, "abilities")) {
                    addOption(&abilities, value, 0);
                } else if (smatch(key, "auth")) {
                    auth = value;
                } else if (smatch(key, "dir")) {
                    dir = value;
                } else if (smatch(key, "extensions")) {
                    addOption(&extensions, value, 0);
                } else if (smatch(key, "handler")) {
                    handler = value;
                } else if (smatch(key, "methods")) {
                    addOption(&methods, value, 0);
                } else if (smatch(key, "redirect")) {
                    if (strchr(value, '@')) {
                        status = stok(value, "@", &redirectUri);
                        if (smatch(status, "*")) {
                            status = "0";
                        }
                    } else {
                        status = "0";
                        redirectUri = value;
                    }
                    if (smatch(redirectUri, "https")) redirectUri = "https://";
                    if (smatch(redirectUri, "http")) redirectUri = "http://";
                    addOption(&redirects, status, redirectUri);
                } else if (smatch(key, "protocol")) {
                    protocol = value;
                } else if (smatch(key, "uri")) {
                    uri = value;
                } else {
                    error("Bad route keyword %s", key);
                    continue;
                }
            }
            if ((route = websAddRoute(uri, handler, -1)) == 0) {
                rc = -1;
                break;
            }
            websSetRouteMatch(route, dir, protocol, methods, extensions, abilities, redirects);
#if ME_GOAHEAD_AUTH
            if (auth && websSetRouteAuth(route, auth) < 0) {
                rc = -1;
                break;
            }
        } else if (smatch(kind, "user")) {
            char *name, *password, *roles;
            name = password = roles = 0;
            while ((option = stok(NULL, " \t\r\n", &next)) != 0) {
                key = stok(option, "=", &value);
                if (smatch(key, "name")) {
                    name = value;
                } else if (smatch(key, "password")) {
                    password = value;
                } else if (smatch(key, "roles")) {
                    roles = value;
                } else {
                    error("Bad user keyword %s", key);
                    continue;
                }
            }
            if (websAddUser(name, password, roles) == 0) {
                rc = -1;
                break;
            }
        } else if (smatch(kind, "role")) {
            char *name;
            name = 0;
            abilities = -1;
            while ((option = stok(NULL, " \t\r\n", &next)) != 0) {
                key = stok(option, "=", &value);
                if (smatch(key, "name")) {
                    name = value;
                } else if (smatch(key, "abilities")) {
                    addOption(&abilities, value, 0);
                }
            }
            if (websAddRole(name, abilities) == 0) {
                rc = -1;
                break;
            }
#endif
        } else {
            error("Unknown route keyword %s", kind); 
            rc = -1;
            break;
        }
    }
    wfree(buf);
#if ME_GOAHEAD_AUTH
    websComputeAllUserAbilities();
#endif
    return rc;
}


/*
    Handler to just continue matching other routes 
 */
static bool continueHandler(Webs *wp)
{
    return 0;
}


/*
    Handler to redirect to the default (code zero) URI
 */
static bool redirectHandler(Webs *wp)
{
    return websRedirectByStatus(wp, 0) == 0;
}


#if ME_GOAHEAD_LEGACY
PUBLIC int websUrlHandlerDefine(char *prefix, char *dir, int arg, WebsLegacyHandlerProc handler, int flags)
{
    WebsRoute   *route;
    static int  legacyCount = 0;
    char        name[ME_GOAHEAD_LIMIT_STRING];

    assert(prefix && *prefix);
    assert(handler);

    fmt(name, sizeof(name), "%s-%d", prefix, legacyCount);
    if (websDefineHandler(name, 0, (WebsHandlerProc) handler, 0, WEBS_LEGACY_HANDLER) < 0) {
        return -1;
    }
    if ((route = websAddRoute(prefix, name, 0)) == 0) {
        return -1;
    }
    if (dir) {
        route->dir = sclone(dir);
    }
    return 0;
}


PUBLIC int websPublish(char *prefix, char *dir)
{
    WebsRoute   *route;

    if ((route = websAddRoute(prefix, 0, 0)) == 0) {
        return -1;
    }
    route->dir = sclone(dir);
    return 0;
}
#endif


/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/runtime.c ************/


/*
    runtime.c -- Runtime support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/



/*********************************** Defines **********************************/
/*
    This structure stores scheduled events.
 */
typedef struct Callback {
    void        (*routine)(void *arg, int id);
    void        *arg;
    WebsTime    at;
    int         id;
} Callback;

/*********************************** Defines **********************************/
/*
    Class definitions
 */
#define CLASS_NORMAL    0               /* [All other]      Normal characters */
#define CLASS_PERCENT   1               /* [%]              Begin format */
#define CLASS_MODIFIER  2               /* [-+ #,]          Modifiers */
#define CLASS_ZERO      3               /* [0]              Special modifier - zero pad */
#define CLASS_STAR      4               /* [*]              Width supplied by arg */
#define CLASS_DIGIT     5               /* [1-9]            Field widths */
#define CLASS_DOT       6               /* [.]              Introduce precision */
#define CLASS_BITS      7               /* [hlL]            Length bits */
#define CLASS_TYPE      8               /* [cdefginopsSuxX] Type specifiers */

#define STATE_NORMAL    0               /* Normal chars in format string */
#define STATE_PERCENT   1               /* "%" */
#define STATE_MODIFIER  2               /* -+ #,*/
#define STATE_WIDTH     3               /* Width spec */
#define STATE_DOT       4               /* "." */
#define STATE_PRECISION 5               /* Precision spec */
#define STATE_BITS      6               /* Size spec */
#define STATE_TYPE      7               /* Data type */
#define STATE_COUNT     8

PUBLIC char stateMap[] = {
    /*     STATES:  Normal Percent Modifier Width  Dot  Prec Bits Type */
    /* CLASS           0      1       2       3     4     5    6    7  */
    /* Normal   0 */   0,     0,      0,      0,    0,    0,   0,   0,
    /* Percent  1 */   1,     0,      1,      1,    1,    1,   1,   1,
    /* Modifier 2 */   0,     2,      2,      0,    0,    0,   0,   0,
    /* Zero     3 */   0,     2,      2,      3,    5,    5,   0,   0,
    /* Star     4 */   0,     3,      3,      0,    5,    0,   0,   0,
    /* Digit    5 */   0,     3,      3,      3,    5,    5,   0,   0,
    /* Dot      6 */   0,     4,      4,      4,    0,    0,   0,   0,
    /* Bits     7 */   0,     6,      6,      6,    6,    6,   6,   0,
    /* Types    8 */   0,     7,      7,      7,    7,    7,   7,   0,
};

/*
    Format:         %[modifier][width][precision][bits][type]
  
    The Class map will map from a specifier letter to a state.
 */
PUBLIC char classMap[] = {
    /*   0  ' '    !     "     #     $     %     &     ' */
             2,    0,    0,    2,    0,    1,    0,    0,
    /*  07   (     )     *     +     ,     -     .     / */
             0,    0,    4,    2,    2,    2,    6,    0,
    /*  10   0     1     2     3     4     5     6     7 */
             3,    5,    5,    5,    5,    5,    5,    5,
    /*  17   8     9     :     ;     <     =     >     ? */
             5,    5,    0,    0,    0,    0,    0,    0,
    /*  20   @     A     B     C     D     E     F     G */
             8,    0,    0,    0,    0,    0,    0,    0,
    /*  27   H     I     J     K     L     M     N     O */
             0,    0,    0,    0,    7,    0,    8,    0,
    /*  30   P     Q     R     S     T     U     V     W */
             0,    0,    0,    8,    0,    0,    0,    0,
    /*  37   X     Y     Z     [     \     ]     ^     _ */
             8,    0,    0,    0,    0,    0,    0,    0,
    /*  40   '     a     b     c     d     e     f     g */
             0,    0,    0,    8,    8,    8,    8,    8,
    /*  47   h     i     j     k     l     m     n     o */
             7,    8,    0,    0,    7,    0,    8,    8,
    /*  50   p     q     r     s     t     u     v     w */
             8,    0,    0,    8,    0,    8,    0,    8,
    /*  57   x     y     z  */
             8,    0,    0,
};

/*
    Flags
 */
#define SPRINTF_LEFT        0x1         /* Left align */
#define SPRINTF_SIGN        0x2         /* Always sign the result */
#define SPRINTF_LEAD_SPACE  0x4         /* put leading space for +ve numbers */
#define SPRINTF_ALTERNATE   0x8         /* Alternate format */
#define SPRINTF_LEAD_ZERO   0x10        /* Zero pad */
#define SPRINTF_SHORT       0x20        /* 16-bit */
#define SPRINTF_LONG        0x40        /* 32-bit */
#define SPRINTF_INT64       0x80        /* 64-bit */
#define SPRINTF_COMMA       0x100       /* Thousand comma separators */
#define SPRINTF_UPPER_CASE  0x200       /* As the name says for numbers */

typedef struct Format {
    uchar   *buf;
    uchar   *endbuf;
    uchar   *start;
    uchar   *end;
    ssize   growBy;
    ssize   maxsize;
    int     precision;
    int     radix;
    int     width;
    int     flags;
    int     len;
} Format;

#define BPUT(fmt, c) \
    if (1) { \
        /* Less one to allow room for the null */ \
        if ((fmt)->end >= ((fmt)->endbuf - sizeof(char))) { \
            if (growBuf(fmt) > 0) { \
                *(fmt)->end++ = (c); \
            } \
        } else { \
            *(fmt)->end++ = (c); \
        } \
    } else

#define BPUTNULL(fmt) \
    if (1) { \
        if ((fmt)->end > (fmt)->endbuf) { \
            if (growBuf(fmt) > 0) { \
                *(fmt)->end = '\0'; \
            } \
        } else { \
            *(fmt)->end = '\0'; \
        } \
    } else 

/*
    The handle list stores the length of the list and the number of used handles in the first two words.  These are
    hidden from the caller by returning a pointer to the third word to the caller.
 */
#define H_LEN       0       /* First entry holds length of list */
#define H_USED      1       /* Second entry holds number of used */
#define H_OFFSET    2       /* Offset to real start of list */
#define H_INCR      16      /* Grow handle list in chunks this size */

#define RINGQ_LEN(bp) ((bp->servp > bp->endp) ? (bp->buflen + (bp->endp - bp->servp)) : (bp->endp - bp->servp))

typedef struct HashTable {              /* Symbol table descriptor */
    WebsKey     **hash_table;           /* Allocated at run time */
    int         inuse;                  /* Is this entry in use */
    int         size;                   /* Size of the table below */
} HashTable;

#ifndef LOG_ERR
    #define LOG_ERR 0
#endif
#if ME_WIN_LIKE
    PUBLIC void syslog(int priority, char *fmt, ...);
#endif

PUBLIC int       logLevel;          /* Log verbosity level */

/************************************* Locals *********************************/

static Callback  **callbacks;
static int       callbackMax;

static HashTable **sym;             /* List of symbol tables */
static int       symMax;            /* One past the max symbol table */
static char      *logPath;          /* Log file name */
static int       logFd;             /* Log file handle */

char *embedthisGoAheadCopyright = EMBEDTHIS_GOAHEAD_COPYRIGHT;

/********************************** Forwards **********************************/

static int calcPrime(int size);
static int getBinBlockSize(int size);
static int hashIndex(HashTable *tp, char *name);
static WebsKey *hash(HashTable *tp, char *name);
static void defaultLogHandler(int level, char *buf);
static WebsLogHandler logHandler = defaultLogHandler;

static int  getState(char c, int state);
static int  growBuf(Format *fmt);
static char *sprintfCore(char *buf, ssize maxsize, char *fmt, va_list arg);
static void outNum(Format *fmt, char *prefix, uint64 val);
static void outString(Format *fmt, char *str, ssize len);
#if ME_FLOAT
static void outFloat(Format *fmt, char specChar, double value);
#endif

/************************************* Code ***********************************/

PUBLIC int websRuntimeOpen()
{
    symMax = 0;
    sym = 0;
    return 0;
}


PUBLIC void websRuntimeClose()
{
}


/*
    This function is called when a scheduled process time has come.
 */
static void callEvent(int id)
{
    Callback    *cp;

    assert(0 <= id && id < callbackMax);
    cp = callbacks[id];
    assert(cp);

    (cp->routine)(cp->arg, cp->id);
}


/*
    Schedule an event in delay milliseconds time. We will use 1 second granularity for webServer.
 */
PUBLIC int websStartEvent(int delay, WebsEventProc proc, void *arg)
{
    Callback    *s;
    int         id;

    if ((id = wallocObject(&callbacks, &callbackMax, sizeof(Callback))) < 0) {
        return -1;
    }
    s = callbacks[id];
    s->routine = proc;
    s->arg = arg;
    s->id = id;

    /*
        Round the delay up to seconds.
     */
    s->at = ((delay + 500) / 1000) + time(0);
    return id;
}


PUBLIC void websRestartEvent(int id, int delay)
{
    Callback    *s;

    if (callbacks == NULL || id == -1 || id >= callbackMax || (s = callbacks[id]) == NULL) {
        return;
    }
    s->at = ((delay + 500) / 1000) + time(0);
}


PUBLIC void websStopEvent(int id)
{
    Callback    *s;

    if (callbacks == NULL || id == -1 || id >= callbackMax || (s = callbacks[id]) == NULL) {
        return;
    }
    wfree(s);
    callbackMax = wfreeHandle(&callbacks, id);
}


WebsTime websRunEvents()
{
    Callback    *s;
    WebsTime    delay, now, nextEvent;
    int         i;

    nextEvent = (MAXINT / 1000);
    now = time(0);

    for (i = 0; i < callbackMax; i++) {
        if ((s = callbacks[i]) != NULL) {
            if ((delay = s->at - now) <= 0) {
                callEvent(i);
                delay = MAXINT / 1000;
                /* Rescan incase event scheduled or modified an event */
                i = -1;
            }
            nextEvent = min(delay, nextEvent);
        }
    }
    return nextEvent * 1000;
}


/*
    Allocating secure replacement for sprintf and vsprintf. 
 */
PUBLIC char *sfmt(char *format, ...)
{
    va_list     ap;
    char        *result;

    assert(format);

    va_start(ap, format);
    result = sprintfCore(NULL, -1, format, ap);
    va_end(ap);
    return result;
}


/*
    Replacement for sprintf
 */
PUBLIC char *fmt(char *buf, ssize bufsize, char *format, ...)
{
    va_list     ap;
    char        *result;

    assert(buf);
    assert(format);

    if (bufsize <= 0) {
        return 0;
    }
    va_start(ap, format);
    result = sprintfCore(buf, bufsize, format, ap);
    va_end(ap);
    return result;
}


/*
    Scure vsprintf replacement
 */
PUBLIC char *sfmtv(char *fmt, va_list arg)
{
    assert(fmt);
    return sprintfCore(NULL, -1, fmt, arg);
}


static int getState(char c, int state)
{
    int     chrClass;

    if (c < ' ' || c > 'z') {
        chrClass = CLASS_NORMAL;
    } else {
        assert((c - ' ') < (int) sizeof(classMap));
        chrClass = classMap[(c - ' ')];
    }
    assert((chrClass * STATE_COUNT + state) < (int) sizeof(stateMap));
    state = stateMap[chrClass * STATE_COUNT + state];
    return state;
}


static char *sprintfCore(char *buf, ssize maxsize, char *spec, va_list args)
{
    Format        fmt;
    ssize         len;
    int64         iValue;
    uint64        uValue;
    int           state;
    char          c, *safe;

    if (spec == 0) {
        spec = "";
    }
    if (buf != 0) {
        assert(maxsize > 0);
        fmt.buf = (uchar*) buf;
        fmt.endbuf = &fmt.buf[maxsize];
        fmt.growBy = -1;
    } else {
        if (maxsize <= 0) {
            maxsize = MAXINT;
        }
        len = min(256, maxsize);
        if ((buf = walloc(len)) == 0) {
            return 0;
        }
        fmt.buf = (uchar*) buf;
        fmt.endbuf = &fmt.buf[len];
        fmt.growBy = min(512, maxsize - len);
    }
    fmt.maxsize = maxsize;
    fmt.start = fmt.buf;
    fmt.end = fmt.buf;
    fmt.len = 0;
    *fmt.start = '\0';

    state = STATE_NORMAL;

    while ((c = *spec++) != '\0') {
        state = getState(c, state);

        switch (state) {
        case STATE_NORMAL:
            BPUT(&fmt, c);
            break;

        case STATE_PERCENT:
            fmt.precision = -1;
            fmt.width = 0;
            fmt.flags = 0;
            break;

        case STATE_MODIFIER:
            switch (c) {
            case '+':
                fmt.flags |= SPRINTF_SIGN;
                break;
            case '-':
                fmt.flags |= SPRINTF_LEFT;
                break;
            case '#':
                fmt.flags |= SPRINTF_ALTERNATE;
                break;
            case '0':
                fmt.flags |= SPRINTF_LEAD_ZERO;
                break;
            case ' ':
                fmt.flags |= SPRINTF_LEAD_SPACE;
                break;
            case ',':
                fmt.flags |= SPRINTF_COMMA;
                break;
            }
            break;

        case STATE_WIDTH:
            if (c == '*') {
                fmt.width = va_arg(args, int);
                if (fmt.width < 0) {
                    fmt.width = -fmt.width;
                    fmt.flags |= SPRINTF_LEFT;
                }
            } else {
                while (isdigit((uchar) c)) {
                    fmt.width = fmt.width * 10 + (c - '0');
                    c = *spec++;
                }
                spec--;
            }
            break;

        case STATE_DOT:
            fmt.precision = 0;
            break;

        case STATE_PRECISION:
            if (c == '*') {
                fmt.precision = va_arg(args, int);
            } else {
                while (isdigit((uchar) c)) {
                    fmt.precision = fmt.precision * 10 + (c - '0');
                    c = *spec++;
                }
                spec--;
            }
            break;

        case STATE_BITS:
            switch (c) {
            case 'L':
                fmt.flags |= SPRINTF_INT64;
                break;

            case 'l':
                fmt.flags |= SPRINTF_LONG;
                break;

            case 'h':
                fmt.flags |= SPRINTF_SHORT;
                break;
            }
            break;

        case STATE_TYPE:
            switch (c) {
            case 'e':
#if ME_FLOAT
            case 'g':
            case 'f':
                fmt.radix = 10;
                outFloat(&fmt, c, (double) va_arg(args, double));
                break;
#endif /* ME_FLOAT */

            case 'c':
                BPUT(&fmt, (char) va_arg(args, int));
                break;

            case 'S':
                /* Safe string */
#if ME_CHAR_LEN > 1 && KEEP
                if (fmt.flags & SPRINTF_LONG) {
                    //  UNICODE - not right wchar
                    safe = websEscapeHtml(va_arg(args, wchar*));
                    outWideString(&fmt, safe, -1);
                } else
#endif
                {
                    safe = websEscapeHtml(va_arg(args, char*));
                    outString(&fmt, safe, -1);
                }
                break;

            case 'w':
                /* Wide string of wchar characters (Same as %ls"). Null terminated. */
#if ME_CHAR_LEN > 1 && KEEP
                outWideString(&fmt, va_arg(args, wchar*), -1);
                break;
#else
                /* Fall through */
#endif

            case 's':
                /* Standard string */
#if ME_CHAR_LEN > 1 && KEEP
                if (fmt.flags & SPRINTF_LONG) {
                    outWideString(&fmt, va_arg(args, wchar*), -1);
                } else
#endif
                    outString(&fmt, va_arg(args, char*), -1);
                break;

            case 'i':
                ;

            case 'd':
                fmt.radix = 10;
                if (fmt.flags & SPRINTF_SHORT) {
                    iValue = (short) va_arg(args, int);
                } else if (fmt.flags & SPRINTF_LONG) {
                    iValue = (long) va_arg(args, long);
                } else if (fmt.flags & SPRINTF_INT64) {
                    iValue = (int64) va_arg(args, int64);
                } else {
                    iValue = (int) va_arg(args, int);
                }
                if (iValue >= 0) {
                    if (fmt.flags & SPRINTF_LEAD_SPACE) {
                        outNum(&fmt, " ", iValue);
                    } else if (fmt.flags & SPRINTF_SIGN) {
                        outNum(&fmt, "+", iValue);
                    } else {
                        outNum(&fmt, 0, iValue);
                    }
                } else {
                    outNum(&fmt, "-", -iValue);
                }
                break;

            case 'X':
                fmt.flags |= SPRINTF_UPPER_CASE;
#if ME_64
                fmt.flags &= ~(SPRINTF_SHORT|SPRINTF_LONG);
                fmt.flags |= SPRINTF_INT64;
#else
                fmt.flags &= ~(SPRINTF_INT64);
#endif
                /*  Fall through  */
            case 'o':
            case 'x':
            case 'u':
                if (fmt.flags & SPRINTF_SHORT) {
                    uValue = (ushort) va_arg(args, uint);
                } else if (fmt.flags & SPRINTF_LONG) {
                    uValue = (ulong) va_arg(args, ulong);
                } else if (fmt.flags & SPRINTF_INT64) {
                    uValue = (uint64) va_arg(args, uint64);
                } else {
                    uValue = va_arg(args, uint);
                }
                if (c == 'u') {
                    fmt.radix = 10;
                    outNum(&fmt, 0, uValue);
                } else if (c == 'o') {
                    fmt.radix = 8;
                    if (fmt.flags & SPRINTF_ALTERNATE && uValue != 0) {
                        outNum(&fmt, "0", uValue);
                    } else {
                        outNum(&fmt, 0, uValue);
                    }
                } else {
                    fmt.radix = 16;
                    if (fmt.flags & SPRINTF_ALTERNATE && uValue != 0) {
                        if (c == 'X') {
                            outNum(&fmt, "0X", uValue);
                        } else {
                            outNum(&fmt, "0x", uValue);
                        }
                    } else {
                        outNum(&fmt, 0, uValue);
                    }
                }
                break;

            case 'n':       /* Count of chars seen thus far */
                if (fmt.flags & SPRINTF_SHORT) {
                    short *count = va_arg(args, short*);
                    *count = (int) (fmt.end - fmt.start);
                } else if (fmt.flags & SPRINTF_LONG) {
                    long *count = va_arg(args, long*);
                    *count = (int) (fmt.end - fmt.start);
                } else {
                    int *count = va_arg(args, int *);
                    *count = (int) (fmt.end - fmt.start);
                }
                break;

            case 'p':       /* Pointer */
#if ME_64
                uValue = (uint64) va_arg(args, void*);
#else
                uValue = (uint) PTOI(va_arg(args, void*));
#endif
                fmt.radix = 16;
                outNum(&fmt, "0x", uValue);
                break;

            default:
                BPUT(&fmt, c);
            }
        }
    }
    BPUTNULL(&fmt);
    return (char*) fmt.buf;
}


static void outString(Format *fmt, char *str, ssize len)
{
    char    *cp;
    ssize   i;

    if (str == NULL) {
        str = "null";
        len = 4;
    } else if (fmt->flags & SPRINTF_ALTERNATE) {
        str++;
        len = (ssize) *str;
    } else if (fmt->precision >= 0) {
        for (cp = str, len = 0; len < fmt->precision; len++) {
            if (*cp++ == '\0') {
                break;
            }
        }
    } else if (len < 0) {
        len = slen(str);
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
    for (i = 0; i < len && *str; i++) {
        BPUT(fmt, *str++);
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
}


#if ME_CHAR_LEN > 1 && KEEP
static void outWideString(Format *fmt, wchar *str, ssize len)
{
    wchar     *cp;
    int         i;

    if (str == 0) {
        BPUT(fmt, (char) 'n');
        BPUT(fmt, (char) 'u');
        BPUT(fmt, (char) 'l');
        BPUT(fmt, (char) 'l');
        return;
    } else if (fmt->flags & SPRINTF_ALTERNATE) {
        str++;
        len = (ssize) *str;
    } else if (fmt->precision >= 0) {
        for (cp = str, len = 0; len < fmt->precision; len++) {
            if (*cp++ == 0) {
                break;
            }
        }
    } else if (len < 0) {
        for (cp = str, len = 0; *cp++ == 0; len++) ;
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
    for (i = 0; i < len && *str; i++) {
        BPUT(fmt, *str++);
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
}
#endif


static void outNum(Format *fmt, char *prefix, uint64 value)
{
    char    numBuf[64];
    char    *cp;
    char    *endp;
    char    c;
    int     letter, len, leadingZeros, i, fill;

    endp = &numBuf[sizeof(numBuf) - 1];
    *endp = '\0';
    cp = endp;

    /*
     *  Convert to ascii
     */
    if (fmt->radix == 16) {
        do {
            letter = (int) (value % fmt->radix);
            if (letter > 9) {
                if (fmt->flags & SPRINTF_UPPER_CASE) {
                    letter = 'A' + letter - 10;
                } else {
                    letter = 'a' + letter - 10;
                }
            } else {
                letter += '0';
            }
            *--cp = letter;
            value /= fmt->radix;
        } while (value > 0);

    } else if (fmt->flags & SPRINTF_COMMA) {
        i = 1;
        do {
            *--cp = '0' + (int) (value % fmt->radix);
            value /= fmt->radix;
            if ((i++ % 3) == 0 && value > 0) {
                *--cp = ',';
            }
        } while (value > 0);
    } else {
        do {
            *--cp = '0' + (int) (value % fmt->radix);
            value /= fmt->radix;
        } while (value > 0);
    }

    len = (int) (endp - cp);
    fill = fmt->width - len;

    if (prefix != 0) {
        fill -= (int) slen(prefix);
    }
    leadingZeros = (fmt->precision > len) ? fmt->precision - len : 0;
    fill -= leadingZeros;

    if (!(fmt->flags & SPRINTF_LEFT)) {
        c = (fmt->flags & SPRINTF_LEAD_ZERO) ? '0': ' ';
        for (i = 0; i < fill; i++) {
            BPUT(fmt, c);
        }
    }
    if (prefix != 0) {
        while (*prefix) {
            BPUT(fmt, *prefix++);
        }
    }
    for (i = 0; i < leadingZeros; i++) {
        BPUT(fmt, '0');
    }
    while (*cp) {
        BPUT(fmt, *cp);
        cp++;
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = 0; i < fill; i++) {
            BPUT(fmt, ' ');
        }
    }
}


#if ME_FLOAT
static void outFloat(Format *fmt, char specChar, double value)
{
    char    result[ME_GOAHEAD_LIMIT_STRING], *cp;
    int     c, fill, i, len, index;

    result[0] = '\0';
    if (specChar == 'f') {
        sprintf(result, "%.*f", fmt->precision, value);

    } else if (specChar == 'g') {
        sprintf(result, "%*.*g", fmt->width, fmt->precision, value);

    } else if (specChar == 'e') {
        sprintf(result, "%*.*e", fmt->width, fmt->precision, value);
    }
    len = (int) slen(result);
    fill = fmt->width - len;
    if (fmt->flags & SPRINTF_COMMA) {
        if (((len - 1) / 3) > 0) {
            fill -= (len - 1) / 3;
        }
    }

    if (fmt->flags & SPRINTF_SIGN && value > 0) {
        BPUT(fmt, '+');
        fill--;
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        c = (fmt->flags & SPRINTF_LEAD_ZERO) ? '0': ' ';
        for (i = 0; i < fill; i++) {
            BPUT(fmt, c);
        }
    }
    index = len;
    for (cp = result; *cp; cp++) {
        BPUT(fmt, *cp);
        if (fmt->flags & SPRINTF_COMMA) {
            if ((--index % 3) == 0 && index > 0) {
                BPUT(fmt, ',');
            }
        }
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = 0; i < fill; i++) {
            BPUT(fmt, ' ');
        }
    }
    BPUTNULL(fmt);
}
#endif /* ME_FLOAT */


/*
    Grow the buffer to fit new data. Return 1 if the buffer can grow. 
    Grow using the growBy size specified when creating the buffer. 
 */
static int growBuf(Format *fmt)
{
    uchar   *newbuf;
    ssize   buflen;

    buflen = (int) (fmt->endbuf - fmt->buf);
    if (fmt->maxsize >= 0 && buflen >= fmt->maxsize) {
        return 0;
    }
    if (fmt->growBy <= 0) {
        /*
            User supplied buffer
         */
        return 0;
    }
    if ((newbuf = walloc(buflen + fmt->growBy)) == 0) {
        return -1;
    }
    if (fmt->buf) {
        memcpy(newbuf, fmt->buf, buflen);
        wfree(fmt->buf);
    }
    buflen += fmt->growBy;
    fmt->end = newbuf + (fmt->end - fmt->buf);
    fmt->start = newbuf + (fmt->start - fmt->buf);
    fmt->buf = newbuf;
    fmt->endbuf = &fmt->buf[buflen];

    /*
        Increase growBy to reduce overhead
     */
    if ((buflen + (fmt->growBy * 2)) < fmt->maxsize) {
        fmt->growBy *= 2;
    }
    return 1;
}


WebsValue valueInteger(long value)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = integer;
    v.value.integer = value;
    return v;
}


WebsValue valueSymbol(void *value)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = symbol;
    v.value.symbol = value;
    return v;
}


WebsValue valueString(char *value, int flags)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = string;
    if (flags & VALUE_ALLOCATE) {
        v.allocated = 1;
        v.value.string = sclone(value);
    } else {
        v.allocated = 0;
        v.value.string = value;
    }
    return v;
}


PUBLIC void valueFree(WebsValue* v)
{
    if (v->valid && v->allocated && v->type == string && v->value.string != NULL) {
        wfree(v->value.string);
    }
    v->type = undefined;
    v->valid = 0;
    v->allocated = 0;
}


static void defaultLogHandler(int flags, char *buf)
{
    char    prefix[ME_GOAHEAD_LIMIT_STRING];

    if (logFd >= 0) {
        if (flags & WEBS_RAW_MSG) {
            write(logFd, buf, (int) slen(buf));
        } else {
            fmt(prefix, sizeof(prefix), "%s: %d: ", ME_NAME, flags & WEBS_LEVEL_MASK);
            write(logFd, prefix, (int) slen(prefix));
            write(logFd, buf, (int) slen(buf));
            write(logFd, "\n", 1);
#if ME_WIN_LIKE || ME_UNIX_LIKE
            if (flags & WEBS_ERROR_MSG && websGetBackground()) {
                syslog(LOG_ERR, "%s", buf);
            }
#endif
        }
    }
}


PUBLIC void error(char *fmt, ...)
{
    va_list     args;
    char        *message;

    if (!logHandler) {
        return;
    }
    va_start(args, fmt);
    message = sfmtv(fmt, args);
    va_end(args);
    logHandler(WEBS_ERROR_MSG, message);
    wfree(message);
}


PUBLIC void assertError(WEBS_ARGS_DEC, char *fmt, ...)
{
    va_list     args;
    char        *fmtBuf, *message;

    va_start(args, fmt);
    fmtBuf = sfmtv(fmt, args);

    message = sfmt("Assertion %s, failed at %s %d\n", fmtBuf, WEBS_ARGS); 
    va_end(args);
    wfree(fmtBuf);
    if (logHandler) {
        logHandler(WEBS_ASSERT_MSG, message);
    }
    wfree(message);
}


PUBLIC void logmsgProc(int level, char *fmt, ...)
{
    va_list     args;
    char        *message;

    if ((level & WEBS_LEVEL_MASK) <= logLevel && logHandler) {
        va_start(args, fmt);
        message = sfmtv(fmt, args);
        logHandler(level | WEBS_LOG_MSG, message);
        wfree(message);
        va_end(args);
    }
}


PUBLIC void traceProc(int level, char *fmt, ...)
{
    va_list     args;
    char        *message;

    if ((level & WEBS_LEVEL_MASK) <= logLevel && logHandler) {
        va_start(args, fmt);
        message = sfmtv(fmt, args);
        logHandler(level | WEBS_TRACE_MSG, message);
        wfree(message);
        va_end(args);
    }
}


PUBLIC int websGetLogLevel() 
{
    return logLevel;
}


WebsLogHandler logGetHandler()
{
    return logHandler;
}


/*
    Replace the default trace handler. Return a pointer to the old handler.
 */
WebsLogHandler logSetHandler(WebsLogHandler handler)
{
    WebsLogHandler    oldHandler;

    oldHandler = logHandler;
    if (handler) {
        logHandler = handler;
    }
    return oldHandler;
}


PUBLIC int logOpen()
{
    if (!logPath) {
        /* This defintion comes from main.bit and me.h */
        logSetPath(ME_GOAHEAD_LOGFILE);
    }
    if (smatch(logPath, "stdout")) {
        logFd = 1;
    } else if (smatch(logPath, "stderr")) {
        logFd = 2;
#if !ME_ROM
    } else {
        if ((logFd = open(logPath, O_CREAT | O_TRUNC | O_APPEND | O_WRONLY, 0666)) < 0) {
            return -1;
        }
        lseek(logFd, 0, SEEK_END);
#endif
    }
    logSetHandler(logHandler);
    return 0;
}


PUBLIC void logClose()
{
    if (logFd >= 0) {
        close(logFd);                                                                              
        logFd = -1;                                                                                
    }                                                                                                    
}


PUBLIC void logSetPath(char *path)
{
    char  *lp;
    
    wfree(logPath);
    logPath = sclone(path);
    if ((lp = strchr(logPath, ':')) != 0) {
        *lp++ = '\0';
        logLevel = atoi(lp);
    }
}


/*
    Convert a string to lower case
 */
PUBLIC char *slower(char *string)
{
    char  *s;

    assert(string);

    if (string == NULL) {
        return NULL;
    }
    s = string;
    while (*s) {
        if (isupper(*s)) {
            *s = (char) tolower((uchar) *s);
        }
        s++;
    }
    *s = '\0';
    return string;
}


/* 
    Convert a string to upper case
 */
PUBLIC char *supper(char *string)
{
    char  *s;

    assert(string);
    if (string == NULL) {
        return NULL;
    }
    s = string;
    while (*s) {
        if (islower((uchar) *s)) {
            *s = (char) toupper((uchar) *s);
        }
        s++;
    }
    *s = '\0';
    return string;
}


PUBLIC char *itosbuf(char *buf, ssize size, int64 value, int radix)
{
    char    *cp, *end;
    char    digits[] = "0123456789ABCDEF";
    int     negative;

    if ((radix != 10 && radix != 16) || size < 2) {
        return 0;
    }
    end = cp = &buf[size];
    *--cp = '\0';

    if (value < 0) {
        negative = 1;
        value = -value;
        size--;
    } else {
        negative = 0;
    }
    do {
        *--cp = digits[value % radix];
        value /= radix;
    } while (value > 0 && cp > buf);

    if (negative) {
        if (cp <= buf) {
            return 0;
        }
        *--cp = '-';
    }
    if (buf < cp) {
        /* Move the null too */
        memmove(buf, cp, end - cp + 1);
    }
    return buf;
}


/*
    Allocate a new file handle. On the first call, the caller must set the handle map to be a pointer to a null
    pointer.  map points to the second element in the handle array.
 */
PUBLIC int wallocHandle(void *mapArg)
{
    void    ***map;
    ssize   *mp;
    int     handle, len, memsize, incr;

    map = (void***) mapArg;
    assert(map);

    if (*map == NULL) {
        incr = H_INCR;
        memsize = (incr + H_OFFSET) * sizeof(void*);
        if ((mp = walloc(memsize)) == NULL) {
            return -1;
        }
        memset(mp, 0, memsize);
        mp[H_LEN] = incr;
        mp[H_USED] = 0;
        *map = (void*) &mp[H_OFFSET];
    } else {
        mp = &((*(ssize**)map)[-H_OFFSET]);
    }
    len = (int) mp[H_LEN];

    /*
        Find the first null handle
     */
    if (mp[H_USED] < mp[H_LEN]) {
        for (handle = 0; handle < len; handle++) {
            if (mp[handle + H_OFFSET] == 0) {
                mp[H_USED]++;
                return handle;
            }
        }
    } else {
        handle = len;
    }

    /*
        No free handle so grow the handle list. Grow list in chunks of H_INCR.
     */
    len += H_INCR;
    memsize = (len + H_OFFSET) * sizeof(void*);
    if ((mp = wrealloc(mp, memsize)) == NULL) {
        return -1;
    }
    *map = (void*) &mp[H_OFFSET];
    mp[H_LEN] = len;
    memset(&mp[H_OFFSET + len - H_INCR], 0, sizeof(ssize*) * H_INCR);
    mp[H_USED]++;
    return handle;
}


/*
    Free a handle. This function returns the value of the largest handle in use plus 1, to be saved as a max value.
 */
PUBLIC int wfreeHandle(void *mapArg, int handle)
{
    void    ***map;
    ssize   *mp;
    int     len;

    map = (void***) mapArg;
    assert(map);
    mp = &((*(ssize**)map)[-H_OFFSET]);
    assert(mp[H_LEN] >= H_INCR);

    assert(mp[handle + H_OFFSET]);
    assert(mp[H_USED]);
    mp[handle + H_OFFSET] = 0;
    if (--(mp[H_USED]) == 0) {
        wfree((void*) mp);
        *map = NULL;
    }
    /*
        Find the greatest handle number in use.
     */
    if (*map == NULL) {
        handle = -1;
    } else {
        len = (int) mp[H_LEN];
        if (mp[H_USED] < mp[H_LEN]) {
            for (handle = len - 1; handle >= 0; handle--) {
                if (mp[handle + H_OFFSET])
                    break;
            }
        } else {
            handle = len;
        }
    }
    return handle + 1;
}


/*
    Allocate an entry in the halloc array
 */
PUBLIC int wallocObject(void *listArg, int *max, int size)
{
    void    ***list;
    char    *cp;
    int     id;

    list = (void***) listArg;
    assert(list);
    assert(max);

    if ((id = wallocHandle(list)) < 0) {
        return -1;
    }
    if (size > 0) {
        if ((cp = walloc(size)) == NULL) {
            wfreeHandle(list, id);
            return -1;
        }
        assert(cp);
        memset(cp, 0, size);
        (*list)[id] = (void*) cp;
    }
    if (id >= *max) {
        *max = id + 1;
    }
    return id;
}

    
/*
    Create a new buf. "increment" is the amount to increase the size of the buf should it need to grow to accomodate
    data being added. "maxsize" is an upper limit (sanity level) beyond which the buffer must not grow. Set maxsize to -1 to
    imply no upper limit. The buffer for the buf is always *  dynamically allocated. Set maxsize
 */
PUBLIC int bufCreate(WebsBuf *bp, int initSize, int maxsize)
{
    int increment;

    assert(bp);
    
    if (initSize <= 0) {
        initSize = ME_GOAHEAD_LIMIT_BUFFER;
    }
    if (maxsize <= 0) {
        maxsize = initSize;
    }
    assert(initSize >= 0);

    increment = getBinBlockSize(initSize);
    if ((bp->buf = walloc((increment))) == NULL) {
        return -1;
    }
    bp->maxsize = maxsize;
    bp->buflen = increment;
    bp->increment = increment;
    bp->endbuf = &bp->buf[bp->buflen];
    bp->servp = bp->buf;
    bp->endp = bp->buf;
    *bp->servp = '\0';
    return 0;
}


/*
    Delete a buf and free the buf buffer.
 */
PUBLIC void bufFree(WebsBuf *bp)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp == NULL) {
        return;
    }
    bufFlush(bp);
    wfree((char*) bp->buf);
    bp->buf = NULL;
}


/*
    Return the length of the data in the buf. Users must fill the queue to a high water mark of at most one less than
    the queue size.  
 */
PUBLIC ssize bufLen(WebsBuf *bp)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp->servp > bp->endp) {
        return bp->buflen + bp->endp - bp->servp;
    } else {
        return bp->endp - bp->servp;
    }
}


/*
    Return the reference to the start of data in the buffer
 */
PUBLIC char *bufStart(WebsBuf *bp)
{
    assert(bp);
    return bp->servp;
}


/*
    Get a byte from the queue
 */
PUBLIC int bufGetc(WebsBuf *bp)
{
    char    c, *cp;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp->servp == bp->endp) {
        return -1;
    }

    cp = (char*) bp->servp;
    c = *cp++;
    bp->servp = cp;
    if (bp->servp >= bp->endbuf) {
        bp->servp = bp->buf;
    }
    return (int) ((uchar) c);
}


/*
    Add a char to the queue. Note if being used to store wide strings this does not add a trailing '\0'. Grow the buffer as
    required.  
 */
PUBLIC int bufPutc(WebsBuf *bp, char c)
{
    char *cp;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if ((bufRoom(bp) < (int) sizeof(char)) && !bufGrow(bp, 0)) {
        return -1;
    }
    cp = (char*) bp->endp;
    *cp++ = (char) c;
    bp->endp = cp;
    if (bp->endp >= bp->endbuf) {
        bp->endp = bp->buf;
    }
    return 0;
}


/*
    Insert a wide character at the front of the queue
 */
PUBLIC int bufInsertc(WebsBuf *bp, char c)
{
    char *cp;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bufRoom(bp) < (int) sizeof(char) && !bufGrow(bp, 0)) {
        return -1;
    }
    if (bp->servp <= bp->buf) {
        bp->servp = bp->endbuf;
    }
    cp = (char*) bp->servp;
    *--cp = (char) c;
    bp->servp = cp;
    return 0;
}


/*
    Add a string to the queue. Add a trailing null (maybe two nulls)
 */
PUBLIC ssize bufPutStr(WebsBuf *bp, char *str)
{
    ssize   rc;

    assert(bp);
    assert(str);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    rc = bufPutBlk(bp, str, strlen(str) * sizeof(char));
    *((char*) bp->endp) = (char) '\0';
    return rc;
}


/*
    Add a null terminator. This does NOT increase the size of the queue
 */
PUBLIC void bufAddNull(WebsBuf *bp)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bufRoom(bp) < (int) sizeof(char)) {
        bufGrow(bp, 0);
        return;
    }
    *((char*) bp->endp) = (char) '\0';
}


#if UNICODE
/*
    Get a byte from the queue
 */
PUBLIC int bufGetcA(WebsBuf *bp)
{
    uchar   c;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp->servp == bp->endp) {
        return -1;
    }

    c = *bp->servp++;
    if (bp->servp >= bp->endbuf) {
        bp->servp = bp->buf;
    }
    return c;
}


/*
    Add a byte to the queue. Note if being used to store strings this does not add a trailing '\0'. 
    Grow the buffer as required.
 */
PUBLIC int bufPutcA(WebsBuf *bp, char c)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bufRoom(bp) == 0 && !bufGrow(bp)) {
        return -1;
    }
    *bp->endp++ = (uchar) c;
    if (bp->endp >= bp->endbuf) {
        bp->endp = bp->buf;
    }
    return 0;
}


/*
    Insert a byte at the front of the queue
 */
PUBLIC int bufInsertcA(WebsBuf *bp, char c)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bufRoom(bp) == 0 && !bufGrow(bp)) {
        return -1;
    }
    if (bp->servp <= bp->buf) {
        bp->servp = bp->endbuf;
    }
    *--bp->servp = (uchar) c;
    return 0;
}


/*
    Add a string to the queue. Add a trailing null (not really in the q). ie. beyond the last valid byte.
 */
PUBLIC int bufPutStrA(WebsBuf *bp, char *str)
{
    int     rc;

    assert(bp);
    assert(str);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    rc = (int) bufPutBlk(bp, str, strlen(str));
    bp->endp[0] = '\0';
    return rc;
}
#endif


/*
    Add a block of data to the buf. Return the number of bytes added. Grow the buffer as required.
 */
PUBLIC ssize bufPutBlk(WebsBuf *bp, char *buf, ssize size)
{
    ssize   this, added;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(buf);
    assert(0 <= size);

    /*
        Loop adding the maximum bytes we can add in a single straight line copy
     */
    added = 0;
    while (size > 0) {
        this = min(bufRoom(bp), size);
        if (this <= 0) {
            if (! bufGrow(bp, 0)) {
                break;
            }
            this = min(bufRoom(bp), size);
        }
        memcpy(bp->endp, buf, this);
        buf += this;
        bp->endp += this;
        size -= this;
        added += this;
        if (bp->endp >= bp->endbuf) {
            bp->endp = bp->buf;
        }
    }
    return added;
}


/*
    Get a block of data from the buf. Return the number of bytes returned.
 */
PUBLIC ssize bufGetBlk(WebsBuf *bp, char *buf, ssize size)
{
    ssize   this, bytes_read;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(buf);
    assert(0 <= size && size < bp->buflen);

    /*
        Loop getting the maximum bytes we can get in a single straight line copy
     */
    bytes_read = 0;
    while (size > 0) {
        this = bufGetBlkMax(bp);
        this = min(this, size);
        if (this <= 0) {
            break;
        }
        memcpy(buf, bp->servp, this);
        buf += this;
        bp->servp += this;
        size -= this;
        bytes_read += this;

        if (bp->servp >= bp->endbuf) {
            bp->servp = bp->buf;
        }
    }
    return bytes_read;
}


/*
    Return the maximum number of bytes the buffer can accept via a single block copy. Useful if the user is doing their
    own data insertion. 
 */
PUBLIC ssize bufRoom(WebsBuf *bp)
{
    ssize   space, in_a_line;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    
    space = bp->buflen - RINGQ_LEN(bp) - 1;
    in_a_line = bp->endbuf - bp->endp;

    return min(in_a_line, space);
}


/*
    Return the maximum number of bytes the buffer can provide via a single block copy. Useful if the user is doing their
    own data retrieval.  
 */
PUBLIC ssize bufGetBlkMax(WebsBuf *bp)
{
    ssize   len, in_a_line;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    len = RINGQ_LEN(bp);
    in_a_line = bp->endbuf - bp->servp;

    return min(in_a_line, len);
}


/*
    Adjust the endp pointer after the user has copied data into the queue.
 */
PUBLIC void bufAdjustEnd(WebsBuf *bp, ssize size)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(0 <= size && size < bp->buflen);

    bp->endp += size;
    if (bp->endp >= bp->endbuf) {
        bp->endp -= bp->buflen;
    }
    /*
        Flush the queue if the endp pointer is corrupted via a bad size
     */
    if (bp->endp >= bp->endbuf) {
        error("Bad end pointer");
        bufFlush(bp);
    }
}


/*
    Adjust the servp pointer after the user has copied data from the queue.
 */
PUBLIC void bufAdjustStart(WebsBuf *bp, ssize size)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(0 <= size && size < bp->buflen);

    bp->servp += size;
    if (bp->servp >= bp->endbuf) {
        bp->servp -= bp->buflen;
    }
    /*
        Flush the queue if the servp pointer is corrupted via a bad size
     */
    if (bp->servp >= bp->endbuf) {
        error("Bad serv pointer");
        bufFlush(bp);
    }
}


/*
    Flush all data in a buffer. Reset the pointers.
 */
PUBLIC void bufFlush(WebsBuf *bp)
{
    assert(bp);
    assert(bp->servp);

    if (bp->buf) {
        bp->servp = bp->buf;
        bp->endp = bp->buf;
        if (bp->servp) {
            *bp->servp = '\0';
        }
    }
}


PUBLIC void bufCompact(WebsBuf *bp)
{
    ssize   len;
    
    if (bp->buf) {
        if ((len = bufLen(bp)) > 0) {
            if (bp->servp < bp->endp && bp->servp > bp->buf) {
                bufAddNull(bp);
                memmove(bp->buf, bp->servp, len + 1);
                bp->endp -= bp->servp - bp->buf;
                bp->servp = bp->buf;
            }
        } else {
            bp->servp = bp->endp = bp->buf;
            *bp->servp = '\0';
        }
    }
}


/*
    Reset pointers if empty
 */
PUBLIC void bufReset(WebsBuf *bp)
{
    if (bp->buf && bp->servp == bp->endp && bp->servp > bp->buf) {
        bp->servp = bp->endp = bp->buf;
        *bp->servp = '\0';
    }
}


/*
    Grow the buffer. Return true if the buffer can be grown. Grow using the increment size specified when opening the
    buf. Don't grow beyond the maximum possible size.
 */
PUBLIC bool bufGrow(WebsBuf *bp, ssize room)
{
    char    *newbuf;
    ssize   len;

    assert(bp);

    if (room == 0) {
        if (bp->maxsize >= 0 && bp->buflen >= bp->maxsize) {
#if BLD_DEBUG
            error("Cannot grow buf. Needed %d, max %d", room, bp->maxsize);
#endif
            return 0;
        }
        room = bp->increment;
        /*
            Double the increment so the next grow will line up with walloc'ed memory
         */
        bp->increment = getBinBlockSize(2 * bp->increment);
    }
    len = bufLen(bp);
    if ((newbuf = walloc(bp->buflen + room)) == NULL) {
        return 0;
    }
    bufGetBlk(bp, newbuf, bufLen(bp));
    wfree((char*) bp->buf);

    bp->buflen += room;
    bp->endp = newbuf;
    bp->servp = newbuf;
    bp->buf = newbuf;
    bp->endbuf = &bp->buf[bp->buflen];
    bufPutBlk(bp, newbuf, len);
    return 1;
}


/*
    Find the smallest binary memory size that "size" will fit into.  This makes the buf and bufGrow routines much
    more efficient. The walloc routine likes powers of 2 minus 1.
 */
static int  getBinBlockSize(int size)
{
    int q;

    size = size >> WEBS_SHIFT;
    for (q = 0; size; size >>= 1) {
        q++;
    }
    return (1 << (WEBS_SHIFT + q));
}


WebsHash hashCreate(int size)
{
    WebsHash    sd;
    HashTable      *tp;

    if (size < 0) {
        size = WEBS_SMALL_HASH;
    }
    assert(size > 2);

    /*
        Create a new handle for this symbol table
     */
    if ((sd = wallocHandle(&sym)) < 0) {
        return -1;
    }

    /*
        Create a new symbol table structure and zero
     */
    if ((tp = (HashTable*) walloc(sizeof(HashTable))) == NULL) {
        symMax = wfreeHandle(&sym, sd);
        return -1;
    }
    memset(tp, 0, sizeof(HashTable));
    if (sd >= symMax) {
        symMax = sd + 1;
    }
    assert(0 <= sd && sd < symMax);
    sym[sd] = tp;

    /*
        Now create the hash table for fast indexing.
     */
    tp->size = calcPrime(size);
    tp->hash_table = (WebsKey**) walloc(tp->size * sizeof(WebsKey*));
    assert(tp->hash_table);
    memset(tp->hash_table, 0, tp->size * sizeof(WebsKey*));
    return sd;
}


/*
    Close this symbol table. Call a cleanup function to allow the caller to free resources associated with each symbol
    table entry.  
 */
PUBLIC void hashFree(WebsHash sd)
{
    HashTable      *tp;
    WebsKey     *sp, *forw;
    int         i;

    if (sd < 0) {
        return;
    }
    assert(0 <= sd && sd < symMax);
    tp = sym[sd];
    assert(tp);

    /*
        Free all symbols in the hash table, then the hash table itself.
     */
    for (i = 0; i < tp->size; i++) {
        for (sp = tp->hash_table[i]; sp; sp = forw) {
            forw = sp->forw;
            valueFree(&sp->name);
            valueFree(&sp->content);
            wfree((void*) sp);
            sp = forw;
        }
    }
    wfree((void*) tp->hash_table);
    symMax = wfreeHandle(&sym, sd);
    wfree((void*) tp);
}


/*
    Return the first symbol in the hashtable if there is one. This call is used as the first step in traversing the
    table. A call to hashFirst should be followed by calls to hashNext to get all the rest of the entries.
 */
WebsKey *hashFirst(WebsHash sd)
{
    HashTable      *tp;
    WebsKey     *sp;
    int         i;

    assert(0 <= sd && sd < symMax);
    tp = sym[sd];
    assert(tp);

    /*
        Find the first symbol in the hashtable and return a pointer to it.
     */
    for (i = 0; i < tp->size; i++) {
        if ((sp = tp->hash_table[i]) != 0) {
            return sp;
        }
    }
    return 0;
}


/*
    Return the next symbol in the hashtable if there is one. See hashFirst.
 */
WebsKey *hashNext(WebsHash sd, WebsKey *last)
{
    HashTable      *tp;
    WebsKey     *sp;
    int         i;

    assert(0 <= sd && sd < symMax);
    if (sd < 0) {
        return 0;
    }
    tp = sym[sd];
    assert(tp);
    if (last == 0) {
        return hashFirst(sd);
    }
    if (last->forw) {
        return last->forw;
    }
    for (i = last->bucket + 1; i < tp->size; i++) {
        if ((sp = tp->hash_table[i]) != 0) {
            return sp;
        }
    }
    return NULL;
}


/*
    Lookup a symbol and return a pointer to the symbol entry. If not present then return a NULL.
 */
WebsKey *hashLookup(WebsHash sd, char *name)
{
    HashTable      *tp;
    WebsKey     *sp;
    char        *cp;

    assert(0 <= sd && sd < symMax);
    if (sd < 0 || (tp = sym[sd]) == NULL) {
        return NULL;
    }
    if (name == NULL || *name == '\0') {
        return NULL;
    }
    /*
        Do an initial hash and then follow the link chain to find the right entry
     */
    for (sp = hash(tp, name); sp; sp = sp->forw) {
        cp = sp->name.value.string;
        if (cp[0] == name[0] && strcmp(cp, name) == 0) {
            break;
        }
    }
    return sp;
}


/*
    Enter a symbol into the table. If already there, update its value.  Always succeeds if memory available. We allocate
    a copy of "name" here so it can be a volatile variable. The value "v" is just a copy of the passed in value, so it
    MUST be persistent.
 */
WebsKey *hashEnter(WebsHash sd, char *name, WebsValue v, int arg)
{
    HashTable      *tp;
    WebsKey     *sp, *last;
    char        *cp;
    int         hindex;

    assert(name);
    assert(0 <= sd && sd < symMax);
    tp = sym[sd];
    assert(tp);

    /*
        Calculate the first daisy-chain from the hash table. If non-zero, then we have daisy-chain, so scan it and look
        for the symbol.  
     */
    last = NULL;
    hindex = hashIndex(tp, name);
    if ((sp = tp->hash_table[hindex]) != NULL) {
        for (; sp; sp = sp->forw) {
            cp = sp->name.value.string;
            if (cp[0] == name[0] && strcmp(cp, name) == 0) {
                break;
            }
            last = sp;
        }
        if (sp) {
            /*
                Found, so update the value If the caller stores handles which require freeing, they will be lost here.
                It is the callers responsibility to free resources before overwriting existing contents. We will here
                free allocated strings which occur due to value_instring().  We should consider providing the cleanup
                function on the open rather than the close and then we could call it here and solve the problem.
             */
            if (sp->content.valid) {
                valueFree(&sp->content);
            }
            sp->content = v;
            sp->arg = arg;
            return sp;
        }
        /*
            Not found so allocate and append to the daisy-chain
         */
        sp = (WebsKey*) walloc(sizeof(WebsKey));
        if (sp == NULL) {
            return NULL;
        }
        sp->name = valueString(name, VALUE_ALLOCATE);
        sp->content = v;
        sp->forw = (WebsKey*) NULL;
        sp->arg = arg;
        sp->bucket = hindex;
        last->forw = sp;

    } else {
        /*
            Daisy chain is empty so we need to start the chain
         */
        sp = (WebsKey*) walloc(sizeof(WebsKey));
        if (sp == NULL) {
            return NULL;
        }
        tp->hash_table[hindex] = sp;
        tp->hash_table[hashIndex(tp, name)] = sp;

        sp->forw = (WebsKey*) NULL;
        sp->content = v;
        sp->arg = arg;
        sp->name = valueString(name, VALUE_ALLOCATE);
        sp->bucket = hindex;
    }
    return sp;
}


/*
    Delete a symbol from a table
 */
PUBLIC int hashDelete(WebsHash sd, char *name)
{
    HashTable      *tp;
    WebsKey     *sp, *last;
    char        *cp;
    int         hindex;

    assert(name && *name);
    assert(0 <= sd && sd < symMax);
    tp = sym[sd];
    assert(tp);

    /*
        Calculate the first daisy-chain from the hash table. If non-zero, then we have daisy-chain, so scan it and look
        for the symbol.  
     */
    last = NULL;
    hindex = hashIndex(tp, name);
    if ((sp = tp->hash_table[hindex]) != NULL) {
        for ( ; sp; sp = sp->forw) {
            cp = sp->name.value.string;
            if (cp[0] == name[0] && strcmp(cp, name) == 0) {
                break;
            }
            last = sp;
        }
    }
    if (sp == (WebsKey*) NULL) {              /* Not Found */
        return -1;
    }
    /*
         Unlink and free the symbol. Last will be set if the element to be deleted is not first in the chain.
     */
    if (last) {
        last->forw = sp->forw;
    } else {
        tp->hash_table[hindex] = sp->forw;
    }
    valueFree(&sp->name);
    valueFree(&sp->content);
    wfree((void*) sp);
    return 0;
}


/*
    Hash a symbol and return a pointer to the hash daisy-chain list. All symbols reside on the chain (ie. none stored in
    the hash table itself) 
 */
static WebsKey *hash(HashTable *tp, char *name)
{
    assert(tp);

    return tp->hash_table[hashIndex(tp, name)];
}


/*
    Compute the hash function and return an index into the hash table We use a basic additive function that is then made
    modulo the size of the table.
 */
static int hashIndex(HashTable *tp, char *name)
{
    uint        sum;
    int         i;

    assert(tp);
    /*
        Add in each character shifted up progressively by 7 bits. The shift amount is rounded so as to not shift too
        far. It thus cycles with each new cycle placing character shifted up by one bit.
     */
    i = 0;
    sum = 0;
    while (*name) {
        sum += (((int) *name++) << i);
        i = (i + 7) % (BITS(int) - BITSPERBYTE);
    }
    return sum % tp->size;
}


/*
    Check if this number is a prime
 */
static int isPrime(int n)
{
    int     i, max;

    assert(n > 0);

    max = n / 2;
    for (i = 2; i <= max; i++) {
        if (n % i == 0) {
            return 0;
        }
    }
    return 1;
}


/*
    Calculate the largest prime smaller than size.
 */
static int calcPrime(int size)
{
    int count;

    assert(size > 0);

    for (count = size; count > 0; count--) {
        if (isPrime(count)) {
            return count;
        }
    }
    return 1;
}


/*
    Convert a wide unicode string into a multibyte string buffer. If count is supplied, it is used as the source length 
    in characters. Otherwise set to -1. DestCount is the max size of the dest buffer in bytes. At most destCount - 1 
    characters will be stored. The dest buffer will always have a trailing null appended.  If dest is NULL, don't copy 
    the string, just return the length of characters. Return a count of bytes copied to the destination or -1 if an 
    invalid unicode sequence was provided in src.
    NOTE: does not allocate.
 */
PUBLIC ssize wtom(char *dest, ssize destCount, wchar *src, ssize count)
{
    ssize   len;

    if (destCount < 0) {
        destCount = MAXSSIZE;
    }
    if (count > 0) {
#if ME_CHAR_LEN == 1
        if (dest) {
            len = scopy(dest, destCount, src);
        } else {
            len = min(slen(src), destCount - 1);
        }
#elif ME_WIN_LIKE
        len = WideCharToMultiByte(CP_ACP, 0, src, count, dest, (DWORD) destCount - 1, NULL, NULL);
#else
        len = wcstombs(dest, src, destCount - 1);
#endif
        if (dest) {
            if (len >= 0) {
                dest[len] = 0;
            }
        } else if (len >= destCount) {
            return -1;
        }
    } else {
        len = 0;
    }
    return len;
}


/*
    Convert a multibyte string to a unicode string. If count is supplied, it is used as the source length in bytes.
    Otherwise set to -1. DestCount is the max size of the dest buffer in characters. At most destCount - 1 
    characters will be stored. The dest buffer will always have a trailing null characters appended.  If dest is NULL, 
    don't copy the string, just return the length of characters. Return a count of characters copied to the destination 
    or -1 if an invalid multibyte sequence was provided in src.
    NOTE: does not allocate.
 */
PUBLIC ssize mtow(wchar *dest, ssize destCount, char *src, ssize count) 
{
    ssize      len;

    if (destCount < 0) {
        destCount = MAXSSIZE;
    }
    if (destCount > 0) {
#if ME_CHAR_LEN == 1
        if (dest) {
            len = scopy(dest, destCount, src);
        } else {
            len = min(slen(src), destCount - 1);
        }
#elif ME_WIN_LIKE
        len = MultiByteToWideChar(CP_ACP, 0, src, count, dest, (DWORD) destCount - 1);
#else
        len = mbstowcs(dest, src, destCount - 1);
#endif
        if (dest) {
            if (len >= 0) {
                dest[len] = 0;
            }
        } else if (len >= destCount) {
            return -1;
        }
    } else {
        len = 0;
    }
    return len;
}


wchar *amtow(char *src, ssize *lenp)
{
    wchar   *dest;
    ssize   len;

    len = mtow(NULL, MAXSSIZE, src, -1);
    if (len < 0) {
        return NULL;
    }
    if ((dest = walloc((len + 1) * sizeof(wchar))) != NULL) {
        mtow(dest, len + 1, src, -1);
    }
    if (lenp) {
        *lenp = len;
    }
    return dest;
}


//  FUTURE UNICODE - need a version that can supply a length

PUBLIC char *awtom(wchar *src, ssize *lenp)
{
    char    *dest;
    ssize   len;

    len = wtom(NULL, MAXSSIZE, src, -1);
    if (len < 0) {
        return NULL;
    }
    if ((dest = walloc(len + 1)) != 0) {
        wtom(dest, len + 1, src, -1);
    }
    if (lenp) {
        *lenp = len;
    }
    return dest;
}


/*
    Convert a hex string to an integer
 */
uint hextoi(char *hexstring)
{
    char      *h;
    uint        c, v;

    v = 0;
    h = hexstring;
    if (*h == '0' && (*(h+1) == 'x' || *(h+1) == 'X')) {
        h += 2;
    }
    while ((c = (uint)*h++) != 0) {
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'a' && c <= 'f') {
            c = (c - 'a') + 10;
        } else if (c >=  'A' && c <= 'F') {
            c = (c - 'A') + 10;
        } else {
            break;
        }
        v = (v * 0x10) + c;
    }
    return v;
}


PUBLIC char *sclone(char *s)
{
    char    *buf;

    if (s == NULL) {
        s = "";
    }
    buf = walloc(strlen(s) + 1);
    strcpy(buf, s);
    return buf;
}


/*
    Convert a string to an integer. If the string starts with "0x" or "0X" a hexidecimal conversion is done.
 */
uint strtoi(char *s)
{
    if (*s == '0' && (*(s+1) == 'x' || *(s+1) == 'X')) {
        s += 2;
        return hextoi(s);
    }
    return atoi(s);
}


PUBLIC int scaselesscmp(char *s1, char *s2)
{
    if (s1 == 0 || s2 == 0) {
        return -1;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return sncaselesscmp(s1, s2, max(slen(s1), slen(s2)));
}


PUBLIC bool scaselessmatch(char *s1, char *s2)
{
    return scaselesscmp(s1, s2) == 0;
}


PUBLIC bool smatch(char *s1, char *s2)
{
    return scmp(s1, s2) == 0;
}


PUBLIC int scmp(char *s1, char *s2)
{
    if (s1 == s2) {
        return 0;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return sncmp(s1, s2, max(slen(s1), slen(s2)));
}


PUBLIC ssize slen(char *s)
{
    return s ? strlen(s) : 0;
}


PUBLIC ssize scopy(char *dest, ssize destMax, char *src)
{
    ssize      len;

    assert(src);
    assert(dest);
    assert(0 < dest && destMax < MAXINT);

    len = slen(src);
    if (destMax <= len) {
        return -1;
    }
    strcpy(dest, src);
    return len;
}


/*
    This routine copies at most "count" characters from a string. It ensures the result is always null terminated and 
    the buffer does not overflow. Returns -1 if the buffer is too small.
 */
PUBLIC ssize sncopy(char *dest, ssize destMax, char *src, ssize count)
{
    ssize      len;

    assert(dest);
    assert(src);
    assert(src != dest);
    assert(0 <= count && count < MAXINT);
    assert(0 < destMax && destMax < MAXINT);

    //  OPT need snlen(src, count);
    len = slen(src);
    len = min(len, count);
    if (destMax <= len) {
        return -1;
    }
    if (len > 0) {
        memcpy(dest, src, len);
        dest[len] = '\0';
    } else {
        *dest = '\0';
        len = 0;
    } 
    return len;
}


#if KEEP
/*
    Return the length of a string limited by a given length
 */
PUBLIC ssize strnlen(char *s, ssize n)
{
    ssize   len;

    len = strlen(s);
    return min(len, n);
}
#endif


/*
    Case sensitive string comparison. Limited by length
 */
PUBLIC int sncmp(char *s1, char *s2, ssize n)
{
    int     rc;

    assert(0 <= n && n < MAXINT);

    if (s1 == 0 && s2 == 0) {
        return 0;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    for (rc = 0; n > 0 && *s1 && rc == 0; s1++, s2++, n--) {
        rc = *s1 - *s2;
    }
    if (rc) {
        return (rc > 0) ? 1 : -1;
    } else if (n == 0) {
        return 0;
    } else if (*s1 == '\0' && *s2 == '\0') {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else if (*s2 == '\0') {
        return 1;
    }
    return 0;
}


PUBLIC int sncaselesscmp(char *s1, char *s2, ssize n)
{
    int     rc;

    assert(0 <= n && n < MAXINT);

    if (s1 == 0 || s2 == 0) {
        return -1;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    for (rc = 0; n > 0 && *s1 && rc == 0; s1++, s2++, n--) {
        rc = tolower((uchar) *s1) - tolower((uchar) *s2);
    }
    if (rc) {
        return (rc > 0) ? 1 : -1;
    } else if (n == 0) {
        return 0;
    } else if (*s1 == '\0' && *s2 == '\0') {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else if (*s2 == '\0') {
        return 1;
    }
    return 0;
}


/*
    Split a string at a delimiter and return the parts.
    This differs from stok in that it never returns null. Also, stok eats leading deliminators, whereas
    ssplit will return an empty string if there are leading deliminators.
    Note: Modifies the original string and returns the string for chaining.
 */
PUBLIC char *ssplit(char *str, cchar *delim, char **last)
{
    char    *end;

    if (last) {
        *last = "";
    }
    if (str == 0) {
        return "";
    }
    if (delim == 0 || *delim == '\0') {
        return str;
    }
    if ((end = strpbrk(str, delim)) != 0) {
        *end++ = '\0';
        end += strspn(end, delim);
    } else {
        end = "";
    }
    if (last) {
        *last = end;
    }
    return str;
}


/*
    Note "str" is modifed as per strtok()
    WARNING:  this does not allocate
 */
PUBLIC char *stok(char *str, char *delim, char **last)
{
    char  *start, *end;
    ssize   i;

    start = str ? str : *last;
    if (start == 0) {
        *last = 0;
        return 0;
    }
    i = strspn(start, delim);
    start += i;
    if (*start == '\0') {
        *last = 0;
        return 0;
    }
    end = strpbrk(start, delim);
    if (end) {
        *end++ = '\0';
        i = strspn(end, delim);
        end += i;
    }
    *last = end;
    return start;
}


PUBLIC char *strim(char *str, char *set, int where)
{
    char    *s;
    ssize   len, i;

    if (str == 0 || set == 0) {
        return 0;
    }
    if (where & WEBS_TRIM_START) {
        i = strspn(str, set);
    } else {
        i = 0;
    }
    s = (char*) &str[i];
    if (where & WEBS_TRIM_END) {
        len = strlen(s);
        while (len > 0 && strspn(&s[len - 1], set) > 0) {
            s[len - 1] = '\0';
            len--;
        }
    }
    return s;
}


/*
    Parse the args and return the count of args. If argv is NULL, the args are parsed read-only. If argv is set,
    then the args will be extracted, back-quotes removed and argv will be set to point to all the args.
    NOTE: this routine does not allocate.
 */
PUBLIC int websParseArgs(char *args, char **argv, int maxArgc)
{
    char    *dest, *src, *start;
    int     quote, argc;

    /*
        Example     "showColors" red 'light blue' "yellow white" 'Cannot \"render\"'
        Becomes:    ["showColors", "red", "light blue", "yellow white", "Cannot \"render\""]
     */
    for (argc = 0, src = args; src && *src != '\0' && argc < maxArgc; argc++) {
        while (isspace((uchar) *src)) {
            src++;
        }
        if (*src == '\0')  {
            break;
        }
        start = dest = src;
        if (*src == '"' || *src == '\'') {
            quote = *src;
            src++; 
            dest++;
        } else {
            quote = 0;
        }
        if (argv) {
            argv[argc] = src;
        }
        while (*src) {
            if (*src == '\\' && src[1] && (src[1] == '\\' || src[1] == '"' || src[1] == '\'')) { 
                src++;
            } else {
                if (quote) {
                    if (*src == quote && !(src > start && src[-1] == '\\')) {
                        break;
                    }
                } else if (*src == ' ') {
                    break;
                }
            }
            if (argv) {
                *dest++ = *src;
            }
            src++;
        }
        if (*src != '\0') {
            src++;
        }
        if (argv) {
            *dest++ = '\0';
        }
    }
    return argc;
}


#if ME_GOAHEAD_LEGACY
PUBLIC int fmtValloc(char **sp, int n, char *format, va_list args)
{
    *sp = sfmtv(format, args);
    return (int) slen(*sp);
}


PUBLIC int fmtAlloc(char **sp, int n, char *format, ...)
{
    va_list     args;

    va_start(args, format);
    *sp = sfmtv(format, args);
    va_end(args);
    return (int) slen(*sp);
}
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/socket.c ************/


/*
    socket.c -- Sockets layer

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



/************************************ Locals **********************************/

WebsSocket      **socketList;           /* List of open sockets */
PUBLIC int      socketMax;              /* Maximum size of socket */
PUBLIC Socket   socketHighestFd = -1;   /* Highest socket fd opened */
PUBLIC int      socketOpenCount = 0;    /* Number of task using sockets */
static int      hasIPv6;                /* System supports IPv6 */

/***************************** Forward Declarations ***************************/

static int ipv6(char *ip);
static void socketAccept(WebsSocket *sp);
static void socketDoEvent(WebsSocket *sp);

/*********************************** Code *************************************/

PUBLIC int socketOpen()
{
    Socket  fd;

    if (++socketOpenCount > 1) {
        return 0;
    }

#if ME_WIN_LIKE
{
    WSADATA     wsaData;
    if (WSAStartup(MAKEWORD(1,1), &wsaData) != 0) {
        return -1;
    }
    if (wsaData.wVersion != MAKEWORD(1,1)) {
        WSACleanup();
        return -1;
    }
}
#endif
    socketList = NULL;
    socketMax = 0;
    socketHighestFd = -1;
    if ((fd = socket(AF_INET6, SOCK_STREAM, 0)) != -1) { 
        hasIPv6 = 1;
        closesocket(fd);
    } else {
        trace(1, "This system does not have IPv6 support");
    }
    return 0;
}


PUBLIC void socketClose()
{
    int     i;

    if (--socketOpenCount <= 0) {
        for (i = socketMax; i >= 0; i--) {
            if (socketList && socketList[i]) {
                socketCloseConnection(i);
            }
        }
        socketOpenCount = 0;
    }
}


PUBLIC bool socketHasDualNetworkStack() 
{
    bool dual;

#if defined(ME_COMPILER_HAS_SINGLE_STACK) || VXWORKS
    dual = 0;
#elif WINDOWS
    {
        OSVERSIONINFO info;
        info.dwOSVersionInfoSize = sizeof(info);
        GetVersionEx(&info);
        /* Vista or later */
        dual = info.dwMajorVersion >= 6;
    }
#else
    dual = hasIPv6;
#endif
    return dual;
}


PUBLIC bool socketHasIPv6() 
{
    return hasIPv6;
}


PUBLIC int socketListen(char *ip, int port, SocketAccept accept, int flags)
{
    WebsSocket              *sp;
    struct sockaddr_storage addr;
    Socklen                 addrlen;
    char                    *sip;
    int                     family, protocol, sid, enable;

    if (port > SOCKET_PORT_MAX) {
        return -1;
    }
    if ((sid = socketAlloc(ip, port, accept, flags)) < 0) {
        return -1;
    }
    sp = socketList[sid];
    assert(sp);

    /*
        Change null IP address to be an IPv6 endpoint if the system is dual-stack. That way we can listen on
        both IPv4 and IPv6
    */
    sip = ((ip == 0 || *ip == '\0') && socketHasDualNetworkStack()) ? "::" : ip;

    /*
        Bind to the socket endpoint and the call listen() to start listening
     */
    if (socketInfo(sip, port, &family, &protocol, &addr, &addrlen) < 0) {
        return -1;
    }
    if ((sp->sock = socket(family, SOCK_STREAM, protocol)) == SOCKET_ERROR) {
        socketFree(sid);
        return -1;
    }
    socketHighestFd = max(socketHighestFd, sp->sock);

#if ME_COMPILER_HAS_FCNTL
    fcntl(sp->sock, F_SETFD, FD_CLOEXEC);
#endif
    enable = 1;
#if ME_UNIX_LIKE || VXWORKS
    if (setsockopt(sp->sock, SOL_SOCKET, SO_REUSEADDR, (char*) &enable, sizeof(enable)) != 0) {
        error("Cannot set reuseaddr, errno %d", errno);
    }
#if defined(SO_REUSEPORT) && KEEP && UNUSED
    /*
        This permits multiple servers listening on the same endpoint
     */
    if (setsockopt(sp->sock, SOL_SOCKET, SO_REUSEPORT, (char*) &enable, sizeof(enable)) != 0) {
        error("Cannot set reuseport, errno %d", errno);
    }
#endif
#elif ME_WIN_LIKE && defined(SO_EXCLUSIVEADDRUSE)
    if (setsockopt(sp->sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*) &enable, sizeof(enable)) != 0) {
        error("Cannot set exclusiveaddruse, errno %d", WSAGetLastError());
    }
#endif

#if defined(IPV6_V6ONLY)
    /*
        By default, most stacks listen on both IPv6 and IPv4 if ip == 0, except windows which inverts this.
        So we explicitly control.
     */
    if (hasIPv6) {
        if (ip == 0) {
            enable = 0;
            setsockopt(sp->sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*) &enable, sizeof(enable));
        } else if (ipv6(ip)) {
            enable = 1;
            setsockopt(sp->sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*) &enable, sizeof(enable));
        }
    }
#endif
    if (bind(sp->sock, (struct sockaddr*) &addr, addrlen) == SOCKET_ERROR) {
        error("Cannot bind to address %s:%d, errno %d", ip ? ip : "*", port, errno);
        socketFree(sid);
        return -1;
    }
    if (listen(sp->sock, SOMAXCONN) < 0) {
        socketFree(sid);
        return -1;
    }
    sp->flags |= SOCKET_LISTENING | SOCKET_NODELAY;
    sp->handlerMask |= SOCKET_READABLE;
    socketSetBlock(sid, (flags & SOCKET_BLOCK));
    if (sp->flags & SOCKET_NODELAY) {
        socketSetNoDelay(sid, 1);
    }
    return sid;
}


#if KEEP
PUBLIC int socketConnect(char *ip, int port, int flags)
{
    WebsSocket                *sp;
    struct sockaddr_storage addr;
    socklen_t               addrlen;
    int                     family, protocol, sid, rc;

    if (port > SOCKET_PORT_MAX) {
        return -1;
    }
    if ((sid = socketAlloc(ip, port, NULL, flags)) < 0) {
        return -1;
    }
    sp = socketList[sid];
    assert(sp);

    if (socketInfo(ip, port, &family, &protocol, &addr, &addrlen) < 0) {
        return -1;       
    }
    if ((sp->sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR) {
        socketFree(sid);
        return -1;
    }
    socketHighestFd = max(socketHighestFd, sp->sock);

#if ME_COMPILER_HAS_FCNTL
    fcntl(sp->sock, F_SETFD, FD_CLOEXEC);
#endif

    /*
        Connect to the remote server in blocking mode, then go into non-blocking mode if desired.
     */
    if (!(sp->flags & SOCKET_BLOCK)) {
#if ME_WIN_LIKE
        /*
            Set to non-blocking for an async connect
         */
        int flag = 1;
        if (ioctlsocket(sp->sock, FIONBIO, &flag) == SOCKET_ERROR) {
            socketFree(sid);
            return -1;
        }
        sp->flags |= SOCKET_ASYNC;
#else
        socketSetBlock(sid, 1);
#endif
    }
    if ((rc = connect(sp->sock, (struct sockaddr*) &addr, sizeof(addr))) < 0 && 
        (rc = tryAlternateConnect(sp->sock, (struct sockaddr*) &addr)) < 0) {
#if ME_WIN_LIKE
        if (socketGetError() != EWOULDBLOCK) {
            socketFree(sid);
            return -1;
        }
#else
        socketFree(sid);
        return -1;

#endif
    }
    socketSetBlock(sid, (flags & SOCKET_BLOCK));
    return sid;
}


/*
    If the connection failed, swap the first two bytes in the sockaddr structure.  This is a kludge due to a change in
    VxWorks between versions 5.3 and 5.4, but we want the product to run on either.
 */
static int tryAlternateConnect(int sock, struct sockaddr *sockaddr)
{
#if VXWORKS || TIDSP
    char *ptr;

    ptr = (char*) sockaddr;
    *ptr = *(ptr+1);
    *(ptr+1) = 0;
    return connect(sock, sockaddr, sizeof(struct sockaddr));
#else
    return -1;
#endif
}
#endif


PUBLIC void socketCloseConnection(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    socketFree(sid);
}


/*
    Accept a connection. Called as a callback on incoming connection.
 */
static void socketAccept(WebsSocket *sp)
{
    struct sockaddr_storage addrStorage;
    struct sockaddr         *addr;
    WebsSocket              *nsp;
    Socket                  newSock;
    size_t                  len;
    char                    ipbuf[1024];
    int                     port, nid;

    assert(sp);

    /*
        Accept the connection and prevent inheriting by children (F_SETFD)
     */
    len = sizeof(addrStorage);
    addr = (struct sockaddr*) &addrStorage;
    if ((newSock = accept(sp->sock, addr, (Socklen*) &len)) == SOCKET_ERROR) {
        return;
    }
#if ME_COMPILER_HAS_FCNTL
    fcntl(newSock, F_SETFD, FD_CLOEXEC);
#endif
    socketHighestFd = max(socketHighestFd, newSock);

    /*
        Create a socket structure and insert into the socket list
     */
    nid = socketAlloc(sp->ip, sp->port, sp->accept, sp->flags);
    if ((nsp = socketList[nid]) == 0) {
        return;
    }
    assert(nsp);
    nsp->sock = newSock;
    nsp->flags &= ~SOCKET_LISTENING;
    socketSetBlock(nid, (nsp->flags & SOCKET_BLOCK));
    if (nsp->flags & SOCKET_NODELAY) {
        socketSetNoDelay(nid, 1);
    }

    /*
        Call the user accept callback. The user must call socketCreateHandler to register for further events of interest.
     */
    if (sp->accept != NULL) {
        /* Get the remote client address */
        socketAddress(addr, (int) len, ipbuf, sizeof(ipbuf), &port);
        if ((sp->accept)(nid, ipbuf, port, sp->sid) < 0) {
            socketFree(nid);
        }
    }
}


PUBLIC void socketRegisterInterest(int sid, int handlerMask)
{
    WebsSocket  *sp;

    assert(socketPtr(sid));
    sp = socketPtr(sid);
    sp->handlerMask = handlerMask;
    if (sp->flags & SOCKET_BUFFERED_READ) {
        sp->handlerMask |= SOCKET_READABLE;
    }
    if (sp->flags & SOCKET_BUFFERED_WRITE) {
        sp->handlerMask |= SOCKET_WRITABLE;
    }
}


/*
    Wait until an event occurs on a socket. Return zero on success, -1 on failure.
 */
PUBLIC int socketWaitForEvent(WebsSocket *sp, int handlerMask)
{
    int mask;

    assert(sp);

    mask = sp->handlerMask;
    sp->handlerMask |= handlerMask;
    while (socketSelect(sp->sid, 1000)) {
        if (sp->currentEvents & (handlerMask | SOCKET_EXCEPTION)) {
            break;
        }
    }
    sp->handlerMask = mask;
    if (sp->currentEvents & SOCKET_EXCEPTION) {
        return -1;
    } else if (sp->currentEvents & handlerMask) {
        return 0;
    }
    return 0;
}


PUBLIC void socketHiddenData(WebsSocket *sp, ssize len, int dir)
{
    if (len > 0) {
        sp->flags |= (dir == SOCKET_READABLE) ? SOCKET_BUFFERED_READ : SOCKET_BUFFERED_WRITE;
        if (sp->handler) {
            socketReservice(sp->sid);
        }
    } else {
        sp->flags &= ~((dir == SOCKET_READABLE) ? SOCKET_BUFFERED_READ : SOCKET_BUFFERED_WRITE);
    }
}


/*
    Wait for a handle to become readable or writable and return a number of noticed events. Timeout is in milliseconds.
 */
#if ME_WIN_LIKE
PUBLIC int socketSelect(int sid, WebsTime timeout)
{
    struct timeval  tv;
    WebsSocket      *sp;
    fd_set          readFds, writeFds, exceptFds;
    int             nEvents;
    int             all, socketHighestFd;   /* Highest socket fd opened */

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);
    socketHighestFd = -1;

    tv.tv_sec = (long) (timeout / 1000);
    tv.tv_usec = (DWORD) (timeout % 1000) * 1000;

    /*
        Set the select event masks for events to watch
     */
    all = nEvents = 0;

    if (sid < 0) {
        all++;
        sid = 0;
    }

    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            continue;
        }
        assert(sp);
        /*
            Set the appropriate bit in the ready masks for the sp->sock.
         */
        if (sp->handlerMask & SOCKET_READABLE /* UNUSED || sp->flags & SOCKET_BUFFERED_READ */) {
            FD_SET(sp->sock, &readFds);
            nEvents++;
        }
        if (sp->handlerMask & SOCKET_WRITABLE /* UNUSED || sp->flags & SOCKET_BUFFERED_READ */) {
            FD_SET(sp->sock, &writeFds);
            nEvents++;
        }
        if (sp->handlerMask & SOCKET_EXCEPTION) {
            FD_SET(sp->sock, &exceptFds);
            nEvents++;
        }
        if (sp->flags & SOCKET_RESERVICE) {
            tv.tv_sec = 0;
            tv.tv_usec = 0;
        }
        if (! all) {
            break;
        }
    }
    /*
        Windows select() fails if no descriptors are set, instead of just sleeping like other, nice select() calls. 
        So, if WINDOWS, sleep.  
     */
    if (nEvents == 0) {
        Sleep((DWORD) timeout);
        return 0;
    }
    /*
        Wait for the event or a timeout.
     */
    nEvents = select(socketHighestFd + 1, &readFds, &writeFds, &exceptFds, &tv);

    if (all) {
        sid = 0;
    }
    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            continue;
        }
        if (sp->flags & SOCKET_RESERVICE) {
            if (sp->handlerMask & SOCKET_READABLE) {
                sp->currentEvents |= SOCKET_READABLE;
            }
            if (sp->handlerMask & SOCKET_WRITABLE) {
                sp->currentEvents |= SOCKET_WRITABLE;
            }
            sp->flags &= ~SOCKET_RESERVICE;
        }
        if (FD_ISSET(sp->sock, &readFds)) {
            sp->currentEvents |= SOCKET_READABLE;
        }
        if (FD_ISSET(sp->sock, &writeFds)) {
            sp->currentEvents |= SOCKET_WRITABLE;
        }
        if (FD_ISSET(sp->sock, &exceptFds)) {
            sp->currentEvents |= SOCKET_EXCEPTION;
        }
        if (! all) {
            break;
        }
    }
    return nEvents;
}

#else /* !ME_WIN_LIKE */


PUBLIC int socketSelect(int sid, WebsTime timeout)
{
    WebsSocket      *sp;
    struct timeval  tv;
    fd_mask         *readFds, *writeFds, *exceptFds;
    int             all, len, nwords, index, bit, nEvents;

    /*
        Allocate and zero the select masks
     */
    nwords = (socketHighestFd + NFDBITS) / NFDBITS;
    len = nwords * sizeof(fd_mask);

    readFds = walloc(len);
    memset(readFds, 0, len);
    writeFds = walloc(len);
    memset(writeFds, 0, len);
    exceptFds = walloc(len);
    memset(exceptFds, 0, len);

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    /*
        Set the select event masks for events to watch
     */
    all = nEvents = 0;

    if (sid < 0) {
        all++;
        sid = 0;
    }

    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            if (all == 0) {
                break;
            } else {
                continue;
            }
        }
        assert(sp);
        /*
            Initialize the ready masks and compute the mask offsets.
         */
        index = sp->sock / (NBBY * sizeof(fd_mask));
        bit = 1 << (sp->sock % (NBBY * sizeof(fd_mask)));
        /*
            Set the appropriate bit in the ready masks for the sp->sock.
         */
        if (sp->handlerMask & SOCKET_READABLE) {
            readFds[index] |= bit;
            nEvents++;
        }
        if (sp->handlerMask & SOCKET_WRITABLE) {
            writeFds[index] |= bit;
            nEvents++;
        }
        if (sp->handlerMask & SOCKET_EXCEPTION) {
            exceptFds[index] |= bit;
            nEvents++;
        }
        if (sp->flags & SOCKET_RESERVICE) {
            tv.tv_sec = 0;
            tv.tv_usec = 0;
        }
        if (! all) {
            break;
        }
    }
    /*
        Wait for the event or a timeout. Reset nEvents to be the number of actual events now.
     */
    nEvents = select(socketHighestFd + 1, (fd_set *) readFds, (fd_set *) writeFds, (fd_set *) exceptFds, &tv);
    if (all) {
        sid = 0;
    }
    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            if (all == 0) {
                break;
            } else {
                continue;
            }
        }
        index = sp->sock / (NBBY * sizeof(fd_mask));
        bit = 1 << (sp->sock % (NBBY * sizeof(fd_mask)));

        if (sp->flags & SOCKET_RESERVICE) {
            if (sp->handlerMask & SOCKET_READABLE) {
                sp->currentEvents |= SOCKET_READABLE;
            }
            if (sp->handlerMask & SOCKET_WRITABLE) {
                sp->currentEvents |= SOCKET_WRITABLE;
            }
            sp->flags &= ~SOCKET_RESERVICE;
            nEvents++;
        }
        if (readFds[index] & bit) {
            sp->currentEvents |= SOCKET_READABLE;
        }
        if (writeFds[index] & bit) {
            sp->currentEvents |= SOCKET_WRITABLE;
        }
        if (exceptFds[index] & bit) {
            sp->currentEvents |= SOCKET_EXCEPTION;
        }
        if (! all) {
            break;
        }
    }
    wfree(readFds);
    wfree(writeFds);
    wfree(exceptFds);
    return nEvents;
}
#endif /* WINDOWS || CE */


PUBLIC void socketProcess()
{
    WebsSocket    *sp;
    int         sid;

    for (sid = 0; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) != NULL) {
            if (sp->currentEvents & sp->handlerMask) {
                socketDoEvent(sp);
            }
        }
    }
}


static void socketDoEvent(WebsSocket *sp)
{
    int     sid;

    assert(sp);

    sid = sp->sid;
    if (sp->currentEvents & SOCKET_READABLE) {
        if (sp->flags & SOCKET_LISTENING) { 
            socketAccept(sp);
            sp->currentEvents = 0;
            return;
        } 
    }
    /*
        Now invoke the users socket handler. NOTE: the handler may delete the
        socket, so we must be very careful after calling the handler.
     */
    if (sp->handler && (sp->handlerMask & sp->currentEvents)) {
        (sp->handler)(sid, sp->handlerMask & sp->currentEvents, sp->handler_data);
        /*
            Make sure socket pointer is still valid, then reset the currentEvents.
         */ 
        if (socketList && sid < socketMax && socketList[sid] == sp) {
            sp->currentEvents = 0;
        }
    }
}


/*
    Set the socket blocking mode. Return the previous mode.
 */
PUBLIC int socketSetBlock(int sid, int on)
{
    WebsSocket  *sp;
    int         oldBlock;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return 0;
    }
    oldBlock = (sp->flags & SOCKET_BLOCK);
    sp->flags &= ~(SOCKET_BLOCK);
    if (on) {
        sp->flags |= SOCKET_BLOCK;
    }
    /*
        Put the socket into block / non-blocking mode
     */
    if (sp->flags & SOCKET_BLOCK) {
#if ME_WIN_LIKE
        ulong flag = !on;
        ioctlsocket(sp->sock, FIONBIO, &flag);
#elif ECOS
        int off;
        off = 0;
        ioctl(sp->sock, FIONBIO, &off);
#elif VXWORKS
        int iflag = !on;
        ioctl(sp->sock, FIONBIO, (int) &iflag);
#elif TIDSP
        setsockopt((SOCKET)sp->sock, SOL_SOCKET, SO_BLOCKING, &on, sizeof(on));
#else
        fcntl(sp->sock, F_SETFL, fcntl(sp->sock, F_GETFL) & ~O_NONBLOCK);
#endif

    } else {
#if ME_WIN_LIKE
        ulong flag = !on;
        int rc = ioctlsocket(sp->sock, FIONBIO, &flag);
        rc = rc;
#elif ECOS
        int on = 1;
        ioctl(sp->sock, FIONBIO, &on);
#elif VXWORKS
        int iflag = !on;
        ioctl(sp->sock, FIONBIO, (int) &iflag);
#elif TIDSP
        setsockopt((SOCKET)sp->sock, SOL_SOCKET, SO_BLOCKING, &on, sizeof(on));
#else
        fcntl(sp->sock, F_SETFL, fcntl(sp->sock, F_GETFL) | O_NONBLOCK);
#endif
    }
#if MACOSX
    /* Prevent SIGPIPE when writing to closed socket on OS X */
    int iflag = 1;
    setsockopt(sp->sock, SOL_SOCKET, SO_NOSIGPIPE, (void*) &iflag, sizeof(iflag));
#endif
    return oldBlock;
}


/*  
    Set the TCP delay behavior (nagle algorithm)
 */
PUBLIC int socketSetNoDelay(int sid, bool on)
{
    WebsSocket  *sp;
    int         oldDelay;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return 0;
    }
    oldDelay = sp->flags & SOCKET_NODELAY;
    if (on) {
        sp->flags |= SOCKET_NODELAY;
    } else {
        sp->flags &= ~(SOCKET_NODELAY);
    }
#if ME_WIN_LIKE
    {
        BOOL    noDelay;
        noDelay = on ? 1 : 0;
        setsockopt(sp->sock, IPPROTO_TCP, TCP_NODELAY, (FAR char*) &noDelay, sizeof(BOOL));
    }
#else
    {
        int     noDelay;
        noDelay = on ? 1 : 0;
        setsockopt(sp->sock, IPPROTO_TCP, TCP_NODELAY, (char*) &noDelay, sizeof(int));
    }
#endif /* ME_WIN_LIKE */
    return oldDelay;
}


/*
    Write to a socket. Absorb as much data as the socket can buffer. Block if the socket is in blocking mode. Returns -1
    on error, otherwise the number of bytes written.
 */
PUBLIC ssize socketWrite(int sid, void *buf, ssize bufsize)
{
    WebsSocket    *sp;
    ssize       len, written, sofar;
    int         errCode;

    if (buf == 0 || (sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    if (sp->flags & SOCKET_EOF) {
        return -1;
    }
    len = bufsize;
    sofar = 0;
    while (len > 0) {
        if ((written = send(sp->sock, (char*) buf + sofar, (int) len, 0)) < 0) {
            errCode = socketGetError();
            if (errCode == EINTR) {
                continue;
            } else if (errCode == EWOULDBLOCK || errCode == EAGAIN) {
                return sofar;
            }
            return -errCode;
        }
        len -= written;
        sofar += written;
    }
    return sofar;
}


/*
    Read from a socket. Return the number of bytes read if successful. This may be less than the requested "bufsize" and
    may be zero. This routine may block if the socket is in blocking mode.
    Return -1 for errors or EOF. Distinguish between error and EOF via socketEof().
 */
PUBLIC ssize socketRead(int sid, void *buf, ssize bufsize)
{
    WebsSocket    *sp;
    ssize       bytes;
    int         errCode;

    assert(buf);
    assert(bufsize > 0);

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    if (sp->flags & SOCKET_EOF) {
        return -1;
    }
    if ((bytes = recv(sp->sock, buf, (int) bufsize, 0)) < 0) {
        errCode = socketGetError();
        if (errCode == EAGAIN || errCode == EWOULDBLOCK) {
            bytes = 0;
        } else {
            /* Conn reset or Some other error */
            sp->flags |= SOCKET_EOF;
            bytes = -errCode;
        }

    } else if (bytes == 0) {
        sp->flags |= SOCKET_EOF;
        bytes = -1;
    }
    return bytes;
}


/*
    Return true if EOF
 */
PUBLIC bool socketEof(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return 1;
    }
    return sp->flags & SOCKET_EOF;
}


PUBLIC void socketReservice(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    sp->flags |= SOCKET_RESERVICE;
}


/*
    Create a user handler for this socket. The handler called whenever there
    is an event of interest as defined by handlerMask (SOCKET_READABLE, ...)
 */
PUBLIC void socketCreateHandler(int sid, int handlerMask, SocketHandler handler, void *data)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    sp->handler = handler;
    sp->handler_data = data;
    socketRegisterInterest(sid, handlerMask);
}


PUBLIC void socketDeleteHandler(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    sp->handler = NULL;
    socketRegisterInterest(sid, 0);
}


/*
    Allocate a new socket structure
 */
PUBLIC int socketAlloc(char *ip, int port, SocketAccept accept, int flags)
{
    WebsSocket    *sp;
    int         sid;

    if ((sid = wallocObject(&socketList, &socketMax, sizeof(WebsSocket))) < 0) {
        return -1;
    }
    sp = socketList[sid];
    sp->sid = sid;
    sp->accept = accept;
    sp->port = port;
    sp->fileHandle = -1;
    sp->saveMask = -1;
    if (ip) {
        sp->ip = sclone(ip);
    }
    sp->flags = flags & (SOCKET_BLOCK | SOCKET_LISTENING | SOCKET_NODELAY);
    return sid;
}


/*
    Free a socket structure
 */
PUBLIC void socketFree(int sid)
{
    WebsSocket  *sp;
    char        buf[256];
    int         i;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    /*
        To close a socket, remove any registered interests, set it to non-blocking so that the recv which follows won't
        block, do a shutdown on it so peers on the other end will receive a FIN, then read any data not yet retrieved
        from the receive buffer, and finally close it.  If these steps are not all performed RESETs may be sent to the
        other end causing problems.
     */
    socketRegisterInterest(sid, 0);
    if (sp->sock >= 0) {
        socketSetBlock(sid, 0);
        while (recv(sp->sock, buf, sizeof(buf), 0) > 0) {}
        if (shutdown(sp->sock, SHUT_RDWR) >= 0) {
            while (recv(sp->sock, buf, sizeof(buf), 0) > 0) {}
        }
        closesocket(sp->sock);
    }
    wfree(sp->ip);
    wfree(sp);
    socketMax = wfreeHandle(&socketList, sid);
    /*
        Calculate the new highest socket number
     */
    socketHighestFd = -1;
    for (i = 0; i < socketMax; i++) {
        if ((sp = socketList[i]) == NULL) {
            continue;
        } 
        socketHighestFd = max(socketHighestFd, sp->sock);
    }
}


/*
    Return the socket object reference
 */
WebsSocket *socketPtr(int sid)
{
    if (sid < 0 || sid >= socketMax || socketList[sid] == NULL) {
        assert(NULL);
        errno = EBADF;
        return NULL;
    }
    assert(socketList[sid]);
    return socketList[sid];
}


/*
    Get the operating system error code
 */
PUBLIC int socketGetError()
{
#if ME_WIN_LIKE
    switch (WSAGetLastError()) {
    case WSAEWOULDBLOCK:
        return EWOULDBLOCK;
    case WSAECONNRESET:
        return ECONNRESET;
    case WSAENETDOWN:
        return ENETDOWN;
    case WSAEPROCLIM:
        return EAGAIN;
    case WSAEINTR:
        return EINTR;
    default:
        return EINVAL;
    }
#else
    return errno;
#endif
}


/*
    Return the underlying socket handle
 */
PUBLIC Socket socketGetHandle(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    return sp->sock;
}


/*
    Get blocking mode
 */
PUBLIC int socketGetBlock(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return 0;
    }
    return (sp->flags & SOCKET_BLOCK);
}


PUBLIC int socketGetMode(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return 0;
    }
    return sp->flags;
}


PUBLIC void socketSetMode(int sid, int mode)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return;
    }
    sp->flags = mode;
}


PUBLIC int socketGetPort(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    return sp->port;
}


#if ME_UNIX_LIKE || WINDOWS
/*  
    Get a socket address from a host/port combination. If a host provides both IPv4 and IPv6 addresses, 
    prefer the IPv4 address. This routine uses getaddrinfo.
    Caller must free addr.
 */
PUBLIC int socketInfo(char *ip, int port, int *family, int *protocol, struct sockaddr_storage *addr, Socklen *addrlen)
{
    struct addrinfo     hints, *res, *r;
    char                portBuf[16];
    int                 v6;

    assert(addr);
    memset((char*) &hints, '\0', sizeof(hints));

    /*
        Note that IPv6 does not support broadcast, there is no 255.255.255.255 equivalent.
        Multicast can be used over a specific link, but the user must provide that address plus %scope_id.
     */
    if (ip == 0 || ip[0] == '\0') {
        ip = 0;
        hints.ai_flags |= AI_PASSIVE;           /* Bind to 0.0.0.0 and :: */
    }
    v6 = ipv6(ip);
    hints.ai_socktype = SOCK_STREAM;
    if (ip) {
        hints.ai_family = v6 ? AF_INET6 : AF_INET;
    } else {
        hints.ai_family = AF_UNSPEC;
    }
    itosbuf(portBuf, sizeof(portBuf), port, 10);

    /*  
        Try to sleuth the address to avoid duplicate address lookups. Then try IPv4 first then IPv6.
     */
    res = 0;
    if (getaddrinfo(ip, portBuf, &hints, &res) != 0) {
        return -1;
    }
    /*
        Prefer IPv4 if IPv6 not requested
     */
    for (r = res; r; r = r->ai_next) {
        if (v6) {
            if (r->ai_family == AF_INET6) {
                break;
            }
        } else {
            if (r->ai_family == AF_INET) {
                break;
            }
        }
    }
    if (r == NULL) {
        r = res;
    }
    memset(addr, 0, sizeof(*addr));
    memcpy((char*) addr, (char*) r->ai_addr, (int) r->ai_addrlen);

    *addrlen = (int) r->ai_addrlen;
    *family = r->ai_family;
    *protocol = r->ai_protocol;
    freeaddrinfo(res);
    return 0;
}
#else

PUBLIC int socketInfo(char *ip, int port, int *family, int *protocol, struct sockaddr_storage *addr, Socklen *addrlen)
{
    struct sockaddr_in  sa;

    memset((char*) &sa, '\0', sizeof(struct sockaddr_in));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((short) (port & 0xFFFF));

    if (scmp(ip, "") != 0) {
        sa.sin_addr.s_addr = inet_addr((char*) ip);
    } else {
        sa.sin_addr.s_addr = INADDR_ANY;
    }
    if (sa.sin_addr.s_addr == INADDR_NONE) {
#if VXWORKS
        /*
            VxWorks only supports one interface and this code only supports IPv4
         */
        sa.sin_addr.s_addr = (ulong) hostGetByName((char*) ip);
        if (sa.sin_addr.s_addr < 0) {
            assert(0);
            return 0;
        }
#else
        struct hostent *hostent;
        hostent = gethostbyname2(ip, AF_INET);
        if (hostent == 0) {
            hostent = gethostbyname2(ip, AF_INET6);
            if (hostent == 0) {
                return -1;
            }
        }
        memcpy((char*) &sa.sin_addr, (char*) hostent->h_addr_list[0], (ssize) hostent->h_length);
#endif
    }
    memcpy((char*) addr, (char*) &sa, sizeof(sa));
    *addrlen = sizeof(struct sockaddr_in);
    *family = sa.sin_family;
    *protocol = 0;
    return 0;
}
#endif


/*  
    Return a numerical IP address and port for the given socket info
 */
PUBLIC int socketAddress(struct sockaddr *addr, int addrlen, char *ip, int ipLen, int *port)
{
#if (ME_UNIX_LIKE || WINDOWS)
    char    service[NI_MAXSERV];

#ifdef IN6_IS_ADDR_V4MAPPED
    if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*) addr;
        if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
            struct sockaddr_in addr4;
            memset(&addr4, 0, sizeof(addr4));
            addr4.sin_family = AF_INET;
            addr4.sin_port = addr6->sin6_port;
            memcpy(&addr4.sin_addr.s_addr, addr6->sin6_addr.s6_addr + 12, sizeof(addr4.sin_addr.s_addr));
            memcpy(addr, &addr4, sizeof(addr4));
            addrlen = sizeof(addr4);
        }
    }
#endif
    if (getnameinfo(addr, addrlen, ip, ipLen, service, sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV | NI_NOFQDN)) {
        return -1;
    }
    if (port) {
        *port = atoi(service);
    }

#else
    struct sockaddr_in  *sa;

#if HAVE_NTOA_R
    sa = (struct sockaddr_in*) addr;
    inet_ntoa_r(sa->sin_addr, ip, ipLen);
#else
    uchar   *cp;
    sa = (struct sockaddr_in*) addr;
    cp = (uchar*) &sa->sin_addr;
    fmt(ip, ipLen, "%d.%d.%d.%d", cp[0], cp[1], cp[2], cp[3]);
#endif
    if (port) {
        *port = ntohs(sa->sin_port);
    }
#endif
    return 0;
}


/*
    Looks like an IPv6 address if it has 2 or more colons
 */
static int ipv6(char *ip)
{
    char  *cp;
    int     colons;

    if (ip == 0 || *ip == 0) {
        /*
            Listening on just a bare port means IPv4 only.
         */
        return 0;
    }
    colons = 0;
    for (cp = (char*) ip; ((*cp != '\0') && (colons < 2)) ; cp++) {
        if (*cp == ':') {
            colons++;
        }
    }
    return colons >= 2;
}


/*  
    Parse address and return the IP address and port components. Handles ipv4 and ipv6 addresses. 
    If the IP portion is absent, *pip is set to null. If the port portion is absent, port is set to the defaultPort.
    If a ":*" port specifier is used, *pport is set to -1;
    When an address contains an ipv6 port it should be written as:

        aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh:iiii
    or
        [aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh:iiii]:port

    If supplied an IPv6 address, the backets are stripped in the returned IP address.

    This routine parses any "https://" prefix.
    Caller must free *pip
 */
PUBLIC int socketParseAddress(char *address, char **pip, int *pport, int *secure, int defaultPort)
{
    char    *ip, *cp;

    ip = 0;
    if (defaultPort < 0) {
        defaultPort = 80;
    }
    *secure = strncmp(address, "https", 5) == 0;
    if ((cp = strstr(address, "://")) != 0) {
        address = &cp[3];
    }
    if (ipv6(address)) {
        /*  
            IPv6. If port is present, it will follow a closing bracket ']'
         */
        if ((cp = strchr(address, ']')) != 0) {
            cp++;
            if ((*cp) && (*cp == ':')) {
                *pport = (*++cp == '*') ? -1 : atoi(cp);

                /* Set ipAddr to ipv6 address without brackets */
                ip = sclone(address + 1);
                cp = strchr(ip, ']');
                *cp = '\0';

            } else {
                /* Handles [a:b:c:d:e:f:g:h:i] case (no port)- should not occur */
                ip = sclone(address + 1);
                if ((cp = strchr(ip, ']')) != 0) {
                    *cp = '\0';
                }
                if (*ip == '\0') {
                    ip = 0;
                }
                /* No port present, use callers default */
                *pport = defaultPort;
            }
        } else {
            /* Handles a:b:c:d:e:f:g:h:i case (no port) */
            ip = sclone(address);

            /* No port present, use callers default */
            *pport = defaultPort;
        }

    } else {
        /*  
            ipv4 
         */
        ip = sclone(address);
        if ((cp = strchr(ip, ':')) != 0) {
            *cp++ = '\0';
            if (*cp == '*') {
                *pport = -1;
            } else {
                *pport = atoi(cp);
            }
            if (*ip == '*' || *ip == '\0') {
                wfree(ip);
                ip = 0;
            }
            
        } else if (strchr(ip, '.')) {
            *pport = defaultPort;
            
        } else {
            if (isdigit((uchar) *ip)) {
                *pport = atoi(ip);
                ip = 0;
            } else {
                /* No port present, use callers default */
                *pport = defaultPort;
            }
        }
    }
    if (pip) {
        *pip = ip;
    } else if (ip) {
        wfree(ip);
    }
    return 0;
}


PUBLIC bool socketIsV6(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return 0;
    }
    return sp->ip && ipv6(sp->ip);
}


PUBLIC bool socketAddressIsV6(char *ip)
{
    return ip && ipv6(ip);
}

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/ssl/est.c ************/


/*
    est.c - Embedded Secure Transport SSL for GoAhead

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/************************************ Include *********************************/



#if ME_COM_EST

#include    "est.h"

/************************************* Defines ********************************/

typedef struct EstConfig {
    rsa_context     rsa;                /* RSA context */
    x509_cert       cert;               /* Certificate (own) */
    x509_cert       ca;                 /* Certificate authority bundle to verify peer */
    int             *ciphers;           /* Set of acceptable ciphers */
} EstConfig;

/*
    Per socket state
 */
typedef struct EstSocket {
    havege_state    hs;                 /* Random HAVEGE state */
    ssl_context     ctx;                /* SSL state */
    ssl_session     session;            /* SSL sessions */
} EstSocket;

static EstConfig estConfig;

/*
    Regenerate using: dh_genprime
    Generated on 1/1/2014
 */
static char *dhg = "4";
static char *dhKey =
    "E4004C1F94182000103D883A448B3F80"
    "2CE4B44A83301270002C20D0321CFD00"
    "11CCEF784C26A400F43DFB901BCA7538"
    "F2C6B176001CF5A0FD16D2C48B1D0C1C"
    "F6AC8E1DA6BCC3B4E1F96B0564965300"
    "FFA1D0B601EB2800F489AA512C4B248C"
    "01F76949A60BB7F00A40B1EAB64BDD48"
    "E8A700D60B7F1200FA8E77B0A979DABF";

/************************************ Forwards ********************************/

static int estHandshake(Webs *wp);
static void estTrace(void *fp, int level, char *str);

/************************************** Code **********************************/

PUBLIC int sslOpen()
{
    trace(7, "Initializing EST SSL"); 

    /*
        Set the server certificate and key files
     */
    if (*ME_GOAHEAD_KEY) {
        /*
            Load a decrypted PEM format private key. The last arg is the private key.
         */
        if (x509parse_keyfile(&estConfig.rsa, ME_GOAHEAD_KEY, 0) < 0) {
            error("EST: Unable to read key file %s", ME_GOAHEAD_KEY); 
            return -1;
        }
    }
    if (*ME_GOAHEAD_CERTIFICATE) {
        /*
            Load a PEM format certificate file
         */
        if (x509parse_crtfile(&estConfig.cert, ME_GOAHEAD_CERTIFICATE) < 0) {
            error("EST: Unable to read certificate %s", ME_GOAHEAD_CERTIFICATE); 
            return -1;
        }
    }
    if (*ME_GOAHEAD_CA) {
        if (x509parse_crtfile(&estConfig.ca, ME_GOAHEAD_CA) != 0) {
            error("Unable to parse certificate bundle %s", *ME_GOAHEAD_CA); 
            return -1;
        }
    }
    estConfig.ciphers = ssl_create_ciphers(ME_GOAHEAD_CIPHERS);
    return 0;
}


PUBLIC void sslClose()
{
}


PUBLIC int sslUpgrade(Webs *wp)
{
    EstSocket   *est;
    WebsSocket  *sp;

    assert(wp);
    if ((est = walloc(sizeof(EstSocket))) == 0) {
        return -1;
    }
    memset(est, 0, sizeof(EstSocket));
    wp->ssl = est;

    ssl_free(&est->ctx);
    havege_init(&est->hs);
    ssl_init(&est->ctx);
	ssl_set_endpoint(&est->ctx, 1);
	ssl_set_authmode(&est->ctx, ME_GOAHEAD_VERIFY_PEER ? SSL_VERIFY_OPTIONAL : SSL_VERIFY_NO_CHECK);
    ssl_set_rng(&est->ctx, havege_rand, &est->hs);
	ssl_set_dbg(&est->ctx, estTrace, NULL);
    sp = socketPtr(wp->sid);
	ssl_set_bio(&est->ctx, net_recv, &sp->sock, net_send, &sp->sock);
    ssl_set_ciphers(&est->ctx, estConfig.ciphers);
	ssl_set_session(&est->ctx, 1, 0, &est->session);
	memset(&est->session, 0, sizeof(ssl_session));

	ssl_set_ca_chain(&est->ctx, *ME_GOAHEAD_CA ? &estConfig.ca : NULL, NULL);
    if (*ME_GOAHEAD_CERTIFICATE && *ME_GOAHEAD_KEY) {
        ssl_set_own_cert(&est->ctx, &estConfig.cert, &estConfig.rsa);
    }
	ssl_set_dh_param(&est->ctx, dhKey, dhg);

    if (estHandshake(wp) < 0) {
        return -1;
    }
    return 0;
}
    

PUBLIC void sslFree(Webs *wp)
{
    EstSocket   *est;

    est = wp->ssl;
    if (est) {
        ssl_free(&est->ctx);
        wfree(est);
        wp->ssl = 0;
    }
}


/*
    Initiate or continue SSL handshaking with the peer. This routine does not block.
    Return -1 on errors, 0 incomplete and awaiting I/O, 1 if successful
 */
static int estHandshake(Webs *wp)
{
    WebsSocket  *sp;
    EstSocket   *est;
    int         rc, vrc, trusted;

    est = (EstSocket*) wp->ssl;
    trusted = 1;
    rc = 0;

    sp = socketPtr(wp->sid);
    sp->flags |= SOCKET_HANDSHAKING;

    while (est->ctx.state != SSL_HANDSHAKE_OVER) {
        if ((rc = ssl_handshake(&est->ctx)) != 0) {
            if (rc == EST_ERR_NET_TRY_AGAIN) {
                return 0;
            }
            break;
        }
    }
    sp->flags &= ~SOCKET_HANDSHAKING;

    /*
        Analyze the handshake result
     */
    if (rc < 0) {
        if (rc == EST_ERR_SSL_PRIVATE_KEY_REQUIRED && !(*ME_GOAHEAD_KEY || *ME_GOAHEAD_CERTIFICATE)) {
            error("Missing required certificate and key");
        } else {
            error("Cannot handshake: error -0x%x", -rc);
        }
        sp->flags |= SOCKET_EOF;
        errno = EPROTO;
        return -1;
       
    } else if ((vrc = ssl_get_verify_result(&est->ctx)) != 0) {
        if (vrc & BADCERT_EXPIRED) {
            logmsg(2, "Certificate expired");

        } else if (vrc & BADCERT_REVOKED) {
            logmsg(2, "Certificate revoked");

        } else if (vrc & BADCERT_CN_MISMATCH) {
            logmsg(2, "Certificate common name mismatch");

        } else if (vrc & BADCERT_NOT_TRUSTED) {
            if (vrc & BADCERT_SELF_SIGNED) {                                                               
                logmsg(2, "Self-signed certificate");
            } else {
                logmsg(2, "Certificate not trusted");
            }
            trusted = 0;

        } else {
            if (est->ctx.client_auth && !*ME_GOAHEAD_CERTIFICATE) {
                logmsg(2, "Server requires a client certificate");
            } else if (rc == EST_ERR_NET_CONN_RESET) {
                logmsg(2, "Peer disconnected");
            } else {
                logmsg(2, "Cannot handshake: error -0x%x", -rc);
            }
        }
        if (ME_GOAHEAD_VERIFY_PEER) {
            /* 
               If not verifying the issuer, permit certs that are only untrusted (no other error).
               This allows self-signed certs.
             */
            if (!ME_GOAHEAD_VERIFY_ISSUER && !trusted) {
                return 1;
            } else {
                sp->flags |= SOCKET_EOF;
                errno = EPROTO;
                return -1;
            }
        }
#if UNUSED
    } else {
        /* Being emitted when no cert supplied */
        logmsg(3, "Certificate verified");
#endif
    }
    return 1;
}


PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    WebsSocket      *sp;
    EstSocket       *est;
    int             rc;

    if (!wp->ssl) {
        assert(0);
        return -1;
    }
    est = (EstSocket*) wp->ssl;
    assert(est);
    sp = socketPtr(wp->sid);

    if (est->ctx.state != SSL_HANDSHAKE_OVER) {
        if ((rc = estHandshake(wp)) <= 0) {
            return rc;
        }
    }
    while (1) {
        rc = ssl_read(&est->ctx, buf, (int) len);
        trace(5, "EST: ssl_read %d", rc);
        if (rc < 0) {
            if (rc == EST_ERR_NET_TRY_AGAIN)  {
                continue;
            } else if (rc == EST_ERR_SSL_PEER_CLOSE_NOTIFY) {
                trace(5, "EST: connection was closed gracefully");
                sp->flags |= SOCKET_EOF;
                return -1;
            } else if (rc == EST_ERR_NET_CONN_RESET) {
                trace(5, "EST: connection reset");
                sp->flags |= SOCKET_EOF;
                return -1;
            } else {
                trace(4, "EST: read error -0x%", -rc);                                                    
                sp->flags |= SOCKET_EOF;                                                               
                return -1; 
            }
        }
        break;
    }
    socketHiddenData(sp, ssl_get_bytes_avail(&est->ctx), SOCKET_READABLE);
    return rc;
}


PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    EstSocket   *est;
    ssize       totalWritten;
    int         rc;

    if (wp->ssl == 0 || len <= 0) {
        assert(0);
        return -1;
    }
    est = (EstSocket*) wp->ssl;
    if (est->ctx.state != SSL_HANDSHAKE_OVER) {
        if ((rc = estHandshake(wp)) <= 0) {
            return rc;
        }
    }
    totalWritten = 0;
    do {
        rc = ssl_write(&est->ctx, (uchar*) buf, (int) len);
        trace(7, "EST: written %d, requested len %d", rc, len);
        if (rc <= 0) {
            if (rc == EST_ERR_NET_TRY_AGAIN) {                                                          
                break;
            }
            if (rc == EST_ERR_NET_CONN_RESET) {                                                         
                trace(4, "ssl_write peer closed");
                return -1;
            } else {
                trace(4, "ssl_write failed rc %d", rc);
                return -1;
            }
        } else {
            totalWritten += rc;
            buf = (void*) ((char*) buf + rc);
            len -= rc;
            trace(7, "EST: write: len %d, written %d, total %d", len, rc, totalWritten);
        }
    } while (len > 0);

    socketHiddenData(socketPtr(wp->sid), est->ctx.in_left, SOCKET_WRITABLE);
    return totalWritten;
}


static void estTrace(void *fp, int level, char *str)
{
    level += 3;
    if (level <= websGetLogLevel()) {
        str = sclone(str);
        str[slen(str) - 1] = '\0';
        trace(level, "%s", str);
    }
}

#else
void estDummy() {}
#endif /* ME_COM_EST */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/ssl/matrixssl.c ************/


/*
    matrixssl.c - SSL socket layer for MatrixSSL

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/************************************ Includes ********************************/

#include    "me.h"

#if ME_COM_MATRIXSSL
/*
    Matrixssl defines int*, uint*, but does not provide HAS_XXX to disable.
    So must include matrixsslApi.h first and then workaround. Ugh!
*/
#if WIN32
    #include   <winsock2.h>
    #include   <windows.h>
#endif
    /* Indent to not create a dependency on this file if not enabled */
    #include    "matrixsslApi.h"

#define     HAS_INT16 1
#define     HAS_UINT16 1
#define     HAS_INT32 1
#define     HAS_UINT32 1
#define     HAS_INT64 1
#define     HAS_UINT64 1



/************************************ Defines *********************************/

/*
    MatrixSSL per-socket state
 */
typedef struct Ms {
    ssl_t   *handle;
    char    *outbuf;        /* Pending output data */
    ssize   outlen;         /* Length of outbuf */
    ssize   written;        /* Number of unencoded bytes written */
    int     more;           /* MatrixSSL stack has buffered data */
} Ms;

static sslKeys_t *sslKeys = NULL;

/************************************ Code ************************************/

PUBLIC int sslOpen()
{
    char    *password, *cert, *key, *ca;

    if (matrixSslOpen() < 0) {
        return -1;
    }
    if (matrixSslNewKeys(&sslKeys) < 0) {
        error("Failed to allocate keys in sslOpen\n");
        return -1;
    }
    password = 0;
    cert = *ME_GOAHEAD_CERTIFICATE ? ME_GOAHEAD_CERTIFICATE : 0;
    key = *ME_GOAHEAD_KEY ? ME_GOAHEAD_KEY : 0;
    ca = *ME_GOAHEAD_CA ? ME_GOAHEAD_CA: 0;
    if (matrixSslLoadRsaKeys(sslKeys, cert, key, password, ca) < 0) {
        error("Failed to read certificate, key or ca file\n");
        return -1;
    }
    return 0;
}


PUBLIC void sslClose()
{
    if (sslKeys) {
        matrixSslDeleteKeys(sslKeys);
        sslKeys = 0;
    }
    matrixSslClose();
}


PUBLIC void sslFree(Webs *wp)
{
    Ms          *ms;
    WebsSocket  *sp;
    uchar       *buf;
    int         len;
    
    ms = wp->ssl;
    if (ms) {
        assert(wp->sid >= 0);
        if ((sp = socketPtr(wp->sid)) == 0) {
            return;
        }
        if (!(sp->flags & SOCKET_EOF)) {
            /*
                Flush data. Append a closure alert to any buffered output data, and try to send it.
                Don't bother retrying or blocking, we're just closing anyway.
            */
            matrixSslEncodeClosureAlert(ms->handle);
            if ((len = matrixSslGetOutdata(ms->handle, &buf)) > 0) {
                sslWrite(wp, buf, len);
            }
        }
        if (ms->handle) {
            matrixSslDeleteSession(ms->handle);
        }
        wfree(ms);
        wp->ssl = 0;
    }
}


PUBLIC int sslUpgrade(Webs *wp)
{
    Ms      *ms;
    ssl_t   *handle;
    int     flags;
    
    flags = ME_GOAHEAD_VERIFY_PEER ? SSL_FLAGS_CLIENT_AUTH : 0;
    if (matrixSslNewServerSession(&handle, sslKeys, NULL, flags) < 0) {
        error("Cannot create new matrixssl session");
        return -1;
    }
    if ((ms = walloc(sizeof(Ms))) == 0) {
        return -1;
    }
    memset(ms, 0x0, sizeof(Ms));
    ms->handle = handle;
    wp->ssl = ms;
    return 0;
}


static ssize blockingWrite(Webs *wp, void *buf, ssize len)
{
    ssize   written, bytes;
    int     prior;

    prior = socketSetBlock(wp->sid, 1);
    for (written = 0; len > 0; ) {
        if ((bytes = socketWrite(wp->sid, buf, len)) < 0) {
            socketSetBlock(wp->sid, prior);
            return bytes;
        }
        buf += bytes;
        len -= bytes;
        written += bytes;
    }
    socketSetBlock(wp->sid, prior);
    return written;
}


static ssize processIncoming(Webs *wp, char *buf, ssize size, ssize nbytes, int *readMore)
{
    Ms      *ms;
    uchar   *data, *obuf;
    ssize   toWrite, written, copied, sofar;
    uint32  dlen;
    int     rc;

    ms = (Ms*) wp->ssl;
    *readMore = 0;
    sofar = 0;

    /*
        Process the received data. If there is application data, it is returned in data/dlen
     */
    rc = matrixSslReceivedData(ms->handle, (int) nbytes, &data, &dlen);

    while (1) {
        switch (rc) {
        case PS_SUCCESS:
            return sofar;

        case MATRIXSSL_REQUEST_SEND:
            toWrite = matrixSslGetOutdata(ms->handle, &obuf);
            if ((written = blockingWrite(wp, obuf, toWrite)) < 0) {
                error("MatrixSSL: Error in process");
                return -1;
            }
            matrixSslSentData(ms->handle, (int) written);
            if (ms->handle->err != SSL_ALERT_NONE && ms->handle->err != SSL_ALLOW_ANON_CONNECTION) {
                return -1;
            }
            *readMore = 1;
            return 0;

        case MATRIXSSL_REQUEST_RECV:
            /* Partial read. More read data required */
            *readMore = 1;
            ms->more = 1;
            return 0;

        case MATRIXSSL_HANDSHAKE_COMPLETE:
            *readMore = 0;
            return 0;

        case MATRIXSSL_RECEIVED_ALERT:
            assert(dlen == 2);
            if (data[0] == SSL_ALERT_LEVEL_FATAL) {
                return -1;
            } else if (data[1] == SSL_ALERT_CLOSE_NOTIFY) {
                //  ignore - graceful close
                return 0;
            }
            rc = matrixSslProcessedData(ms->handle, &data, &dlen);
            break;

        case MATRIXSSL_APP_DATA:
            copied = min((ssize) dlen, size);
            memcpy(buf, data, copied);
            buf += copied;
            size -= copied;
            data += copied;
            dlen = dlen - (int) copied;
            sofar += copied;
            ms->more = ((ssize) dlen > size) ? 1 : 0;
            if (!ms->more) {
                /* The MatrixSSL buffer has been consumed, see if we can get more data */
                rc = matrixSslProcessedData(ms->handle, &data, &dlen);
                break;
            }
            return sofar;

        default:
            return -1;
        }
    }
}


/*
    Return number of bytes read. Return -1 on errors and EOF.
 */
static ssize innerRead(Webs *wp, char *buf, ssize size)
{
    Ms          *ms;
    uchar       *mbuf;
    ssize       nbytes;
    int         msize, readMore;

    ms = (Ms*) wp->ssl;
    do {
        if ((msize = matrixSslGetReadbuf(ms->handle, &mbuf)) < 0) {
            return -1;
        }
        readMore = 0;
        if ((nbytes = socketRead(wp->sid, mbuf, msize)) < 0) {
            return nbytes;
        } else if (nbytes > 0) {
            nbytes = processIncoming(wp, buf, size, nbytes, &readMore);
            if (nbytes < 0) {
                sp = socketPtr(wp->sid);
                sp->flags |= SOCKET_EOF;
                return nbytes;
            }
            if (nbytes > 0) {
                return nbytes;
            }
        }
    } while (readMore);
    return 0;
}


/*
    Return number of bytes read. Return -1 on errors and EOF.
 */
PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    Ms          *ms;
    WebsSocket  *sp;
    ssize       bytes;

    if (len <= 0) {
        return -1;
    }
    bytes = innerRead(wp, buf, len);
    ms = (Ms*) wp->ssl;
    if (ms->more) {
        if ((sp = socketPtr(wp->sid)) != 0) {
            socketHiddenData(sp, ms->more, SOCKET_READABLE);
        }
    }
    return bytes;
}


/*
    Non-blocking write data. Return the number of bytes written or -1 on errors.
    Returns zero if part of the data was written.

    Encode caller's data buffer into an SSL record and write to socket. The encoded data will always be 
    bigger than the incoming data because of the record header (5 bytes) and MAC (16 bytes MD5 / 20 bytes SHA1)
    This would be fine if we were using blocking sockets, but non-blocking presents an interesting problem.  Example:

        A 100 byte input record is encoded to a 125 byte SSL record
        We can send 124 bytes without blocking, leaving one buffered byte
        We can't return 124 to the caller because it's more than they requested
        We can't return 100 to the caller because they would assume all data
        has been written, and we wouldn't get re-called to send the last byte

    We handle the above case by returning 0 to the caller if the entire encoded record could not be sent. Returning 
    0 will prompt us to select this socket for write events, and we'll be called again when the socket is writable.  
    We'll use this mechanism to flush the remaining encoded data, ignoring the bytes sent in, as they have already 
    been encoded.  When it is completely flushed, we return the originally requested length, and resume normal 
    processing.
 */
PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    Ms      *ms;
    uchar   *obuf;
    ssize   encoded, nbytes, written;

    ms = (Ms*) wp->ssl;
    while (len > 0 || ms->outlen > 0) {
        if ((encoded = matrixSslGetOutdata(ms->handle, &obuf)) <= 0) {
            if (ms->outlen <= 0) {
                ms->outbuf = (char*) buf;
                ms->outlen = len;
                ms->written = 0;
                len = 0;
            }
            nbytes = min(ms->outlen, SSL_MAX_PLAINTEXT_LEN);
            if ((encoded = matrixSslEncodeToOutdata(ms->handle, (uchar*) buf, (int) nbytes)) < 0) {
                return encoded;
            }
            ms->outbuf += nbytes;
            ms->outlen -= nbytes;
            ms->written += nbytes;
        }
        if ((written = socketWrite(wp->sid, obuf, encoded)) < 0) {
            return written;
        } else if (written == 0) {
            break;
        }
        matrixSslSentData(ms->handle, (int) written);
    }
    /*
        Only signify all the data has been written if MatrixSSL has absorbed all the data
     */
    return ms->outlen == 0 ? ms->written : 0;
}

#else
void matrixsslDummy() {}
#endif /* ME_COM_MATRIXSSL */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/ssl/nanossl.c ************/


/*
    nanossl.c - Mocana NanoSSL

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "me.h"

#if ME_COM_NANOSSL
 #include "goahead.h"

 #include "common/moptions.h"
 #include "common/mdefs.h"
 #include "common/mtypes.h"
 #include "common/merrors.h"
 #include "common/mrtos.h"
 #include "common/mtcp.h"
 #include "common/mocana.h"
 #include "common/random.h"
 #include "common/vlong.h"
 #include "crypto/hw_accel.h"
 #include "crypto/crypto.h"
 #include "crypto/pubcrypto.h"
 #include "crypto/ca_mgmt.h"
 #include "ssl/ssl.h"
 #include "asn1/oiddefs.h"

/************************************* Defines ********************************/

static certDescriptor  cert;
static certDescriptor  ca;

/*
    Per socket state
 */
typedef struct Nano {
    int             fd;
    sbyte4          handle;
    int             connected;
} Nano;

#if ME_DEBUG
    #define SSL_HELLO_TIMEOUT   15000
    #define SSL_RECV_TIMEOUT    300000
#else
    #define SSL_HELLO_TIMEOUT   15000000
    #define SSL_RECV_TIMEOUT    30000000
#endif

#define KEY_SIZE                1024
#define MAX_CIPHERS             16

/***************************** Forward Declarations ***************************/

static void     nanoLog(sbyte4 module, sbyte4 severity, sbyte *msg);
static void     DEBUG_PRINT(void *where, void *msg);
static void     DEBUG_PRINTNL(void *where, void *msg);

/************************************* Code ***********************************/
/*
    Create the Openssl module. This is called only once
 */
PUBLIC int sslOpen()
{
    sslSettings     *settings;
    char            *certificate, *key, *cacert;
    int             rc;

    if (MOCANA_initMocana() < 0) {
        error("NanoSSL initialization failed");
        return -1;
    }
    MOCANA_initLog(nanoLog);

    if (SSL_init(SOMAXCONN, 0) < 0) {
        error("SSL_init failed");
        return -1;
    }

    certificate = *ME_GOAHEAD_CERTIFICATE ? ME_GOAHEAD_CERTIFICATE : 0;
    key = *ME_GOAHEAD_KEY ? ME_GOAHEAD_KEY : 0;
    cacert = *ME_GOAHEAD_CA ? ME_GOAHEAD_CA: 0;

    if (certificate) {
        certDescriptor tmp;
        if ((rc = MOCANA_readFile((sbyte*) certificate, &tmp.pCertificate, &tmp.certLength)) < 0) {
            error("NanoSSL: Unable to read certificate %s", certificate); 
            CA_MGMT_freeCertificate(&tmp);
            return -1;
        }
        assert(__ENABLE_MOCANA_PEM_CONVERSION__);
        if ((rc = CA_MGMT_decodeCertificate(tmp.pCertificate, tmp.certLength, &cert.pCertificate, 
                &cert.certLength)) < 0) {
            error("NanoSSL: Unable to decode PEM certificate %s", certificate); 
            CA_MGMT_freeCertificate(&tmp);
            return -1;
        }
        MOCANA_freeReadFile(&tmp.pCertificate);
    }
    if (key) {
        certDescriptor tmp;
        if ((rc = MOCANA_readFile((sbyte*) key, &tmp.pKeyBlob, &tmp.keyBlobLength)) < 0) {
            error("NanoSSL: Unable to read key file %s", key); 
            CA_MGMT_freeCertificate(&cert);
        }
        if ((rc = CA_MGMT_convertKeyPEM(tmp.pKeyBlob, tmp.keyBlobLength, 
                &cert.pKeyBlob, &cert.keyBlobLength)) < 0) {
            error("NanoSSL: Unable to decode PEM key file %s", key); 
            CA_MGMT_freeCertificate(&tmp);
            return -1;
        }
        MOCANA_freeReadFile(&tmp.pKeyBlob);    
    }
    if (cacert) {
        certDescriptor tmp;
        if ((rc = MOCANA_readFile((sbyte*) cacert, &tmp.pCertificate, &tmp.certLength)) < 0) {
            error("NanoSSL: Unable to read CA certificate file %s", cacert); 
            CA_MGMT_freeCertificate(&tmp);
            return -1;
        }
        if ((rc = CA_MGMT_decodeCertificate(tmp.pCertificate, tmp.certLength, &ca.pCertificate, 
                &ca.certLength)) < 0) {
            error("NanoSSL: Unable to decode PEM certificate %s", cacert); 
            CA_MGMT_freeCertificate(&tmp);
            return -1;
        }
        MOCANA_freeReadFile(&tmp.pCertificate);
    }

    if (SSL_initServerCert(&cert, FALSE, 0)) {
        error("SSL_initServerCert failed");
        return -1;
    }
    settings = SSL_sslSettings();
    settings->sslTimeOutHello = SSL_HELLO_TIMEOUT;
    settings->sslTimeOutReceive = SSL_RECV_TIMEOUT;
    return 0;
}


PUBLIC void sslClose() 
{
    SSL_releaseTables();
    MOCANA_freeMocana();
    CA_MGMT_freeCertificate(&cert);
}


PUBLIC void sslFree(Webs *wp)
{
    Nano        *np;
    
    if (wp->ssl) {
        np = wp->ssl;
        if (np->handle) {
            SSL_closeConnection(np->handle);
            np->handle = 0;
        }
        wfree(np);
        wp->ssl = 0;
    }
}


/*
    Upgrade a standard socket to use TLS
 */
PUBLIC int sslUpgrade(Webs *wp)
{
    Nano        *np;

    assert(wp);

    if ((np = walloc(sizeof(Nano))) == 0) {
        return -1;
    }
    memset(np, 0, sizeof(Nano));
    wp->ssl = np;
    if ((np->handle = SSL_acceptConnection(socketGetHandle(wp->sid))) < 0) {
        return -1;
    }
    return 0;
}


/*
    Initiate or continue SSL handshaking with the peer. This routine does not block.
    Return -1 on errors, 0 incomplete and awaiting I/O, 1 if successful
*/
static int nanoHandshake(Webs *wp)
{
    Nano        *np;
    ubyte4      flags;
    int         rc;

    np = (Nano*) wp->ssl;
    wp->flags |= SOCKET_HANDSHAKING;
    SSL_getSessionFlags(np->handle, &flags);
    if (ME_GOAHEAD_VERIFY_PEER) {
        flags |= SSL_FLAG_REQUIRE_MUTUAL_AUTH;
    } else {
        flags |= SSL_FLAG_NO_MUTUAL_AUTH_REQUEST;
    }
    SSL_setSessionFlags(np->handle, flags);
    rc = 0;

    while (!np->connected) {
        if ((rc = SSL_negotiateConnection(np->handle)) < 0) {
            break;
        }
        np->connected = 1;
        break;
    }
    wp->flags &= ~SOCKET_HANDSHAKING;

    /*
        Analyze the handshake result
    */
    if (rc < 0) {
        if (rc == ERR_SSL_UNKNOWN_CERTIFICATE_AUTHORITY) {
            logmsg(3, "Unknown certificate authority");

        /* Common name mismatch, cert revoked */
        } else if (rc == ERR_SSL_PROTOCOL_PROCESS_CERTIFICATE) {
            logmsg(3, "Bad certificate");
        } else if (rc == ERR_SSL_NO_SELF_SIGNED_CERTIFICATES) {
            logmsg(3, "Self-signed certificate");
        } else if (rc == ERR_SSL_CERT_VALIDATION_FAILED) {
            logmsg(3, "Certificate does not validate");
        }
        DISPLAY_ERROR(0, rc); 
        logmsg(4, "NanoSSL: Cannot handshake: error %d", rc);
        errno = EPROTO;
        return -1;
    }
    return 1;
}


/*
    Return the number of bytes read. Return -1 on errors and EOF.
 */
PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    Nano        *np;
    WebsSocket  *sp;
    sbyte4      nbytes, count;
    int         rc;

    np = (Nano*) wp->ssl;
    assert(np);
    sp = socketPtr(wp->sid);

    if (!np->connected && (rc = nanoHandshake(wp)) <= 0) {
        return rc;
    }
    while (1) {
        /*
            This will do the actual blocking I/O
         */
        rc = SSL_recv(np->handle, buf, (sbyte4) len, &nbytes, 0);
        logmsg(5, "NanoSSL: ssl_read %d", rc);
        if (rc < 0) {
            if (rc != ERR_TCP_READ_ERROR) {
                sp->flags |= SOCKET_EOF;
            }
            return -1;
        }
        break;
    }
    SSL_recvPending(np->handle, &count);
    if (count > 0) {
        socketHiddenData(sp, count, SOCKET_READABLE);
    }
    return nbytes;
}


/*
    Write data. Return the number of bytes written or -1 on errors.
 */
PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    Nano        *np;
    WebsSocket  *sp;
    ssize       totalWritten;
    int         rc, count, sent;

    np = (Nano*) wp->ssl;
    if (len <= 0) {
        assert(0);
        return -1;
    }
    if (!np->connected && (rc = nanoHandshake(wp)) <= 0) {
        return rc;
    }
    totalWritten = 0;
    do {
        rc = sent = SSL_send(np->handle, (sbyte*) buf, (int) len);
        logmsg(7, "NanoSSL: written %d, requested len %d", sent, len);
        if (rc <= 0) {
            logmsg(0, "NanoSSL: SSL_send failed sent %d", rc);
            sp = socketPtr(wp->sid);
            sp->flags |= SOCKET_EOF;
            return -1;
        }
        totalWritten += sent;
        buf = (void*) ((char*) buf + sent);
        len -= sent;
        logmsg(7, "NanoSSL: write: len %d, written %d, total %d", len, sent, totalWritten);
    } while (len > 0);

    SSL_sendPending(np->handle, &count);
    if (count > 0) {
        socketReservice(wp->sid);
    }
    return totalWritten;
}


static void DEBUG_PRINT(void *where, void *msg)
{
    logmsg(2, "%s", msg);
}

static void DEBUG_PRINTNL(void *where, void *msg)
{
    logmsg(2, "%s", msg);
}

static void nanoLog(sbyte4 module, sbyte4 severity, sbyte *msg)
{
    logmsg(3, "%s", (cchar*) msg);
}

#else
void nanosslDummy() {}
#endif /* ME_COM_NANOSSL */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file ../../../src/ssl/openssl.c ************/


/*
    openssl.c - SSL socket layer for OpenSSL

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/************************************ Include *********************************/

#include    "me.h"
#include    "osdep.h"

#if ME_COM_OPENSSL

/* Clashes with WinCrypt.h */
#undef OCSP_RESPONSE
#include    <openssl/ssl.h>
#include    <openssl/evp.h>
#include    <openssl/rand.h>
#include    <openssl/err.h>
#include    <openssl/dh.h>



/************************************* Defines ********************************/

static SSL_CTX *sslctx = NULL;

typedef struct RandBuf {
    time_t      now;
    int         pid;
} RandBuf;

#define VERIFY_DEPTH 10

/************************************ Forwards ********************************/

static RSA *rsaCallback(SSL *ssl, int isExport, int keyLength);
static int sslSetCertFile(char *certFile);
static int sslSetKeyFile(char *keyFile);
static int verifyX509Certificate(int ok, X509_STORE_CTX *ctx);

/************************************** Code **********************************/

PUBLIC int sslOpen()
{
    RandBuf     randBuf;

    trace(7, "Initializing SSL"); 

    randBuf.now = time(0);
    randBuf.pid = getpid();
    RAND_seed((void*) &randBuf, sizeof(randBuf));
#if ME_UNIX_LIKE
    trace(6, "OpenSsl: Before calling RAND_load_file");
    RAND_load_file("/dev/urandom", 256);
    trace(6, "OpenSsl: After calling RAND_load_file");
#endif

    CRYPTO_malloc_init(); 
#if !ME_WIN_LIKE
    OpenSSL_add_all_algorithms();
#endif
    SSL_library_init();
    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();

    if ((sslctx = SSL_CTX_new(SSLv23_server_method())) == 0) {
        error("Unable to create SSL context"); 
        return -1;
    }

    /*
          Set the client certificate verification locations
     */
    if (*ME_GOAHEAD_CA) {
        if ((!SSL_CTX_load_verify_locations(sslctx, ME_GOAHEAD_CA, NULL)) || (!SSL_CTX_set_default_verify_paths(sslctx))) {
            error("Unable to read cert verification locations");
            sslClose();
            return -1;
        }
    }
    /*
          Set the server certificate and key files
     */
    if (*ME_GOAHEAD_KEY && sslSetKeyFile(ME_GOAHEAD_KEY) < 0) {
        sslClose();
        return -1;
    }
    if (*ME_GOAHEAD_CERTIFICATE && sslSetCertFile(ME_GOAHEAD_CERTIFICATE) < 0) {
        sslClose();
        return -1;
    }
    SSL_CTX_set_tmp_rsa_callback(sslctx, rsaCallback);

    if (ME_GOAHEAD_VERIFY_PEER) {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verifyX509Certificate);
        SSL_CTX_set_verify_depth(sslctx, VERIFY_DEPTH);
    } else {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_NONE, verifyX509Certificate);
    }
    /*
          Set the certificate authority list for the client
     */
    if (ME_GOAHEAD_CA && *ME_GOAHEAD_CA) {
        SSL_CTX_set_client_CA_list(sslctx, SSL_load_client_CA_file(ME_GOAHEAD_CA));
    }
    if (ME_GOAHEAD_CIPHERS && *ME_GOAHEAD_CIPHERS) {
        SSL_CTX_set_cipher_list(sslctx, ME_GOAHEAD_CIPHERS);
    }
    SSL_CTX_set_options(sslctx, SSL_OP_ALL);
    SSL_CTX_sess_set_cache_size(sslctx, 128);
#ifdef SSL_OP_NO_TICKET
    SSL_CTX_set_options(sslctx, SSL_OP_NO_TICKET);
#endif
#ifdef SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION                                                       
    SSL_CTX_set_options(sslctx, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
#endif                                                                                                     
    SSL_CTX_set_mode(sslctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_AUTO_RETRY | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
    SSL_CTX_set_options(sslctx, SSL_OP_MSIE_SSLV2_RSA_PADDING);
#endif
#ifdef SSL_OP_NO_COMPRESSION
    SSL_CTX_set_options(sslctx, SSL_OP_NO_COMPRESSION);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(sslctx, SSL_MODE_RELEASE_BUFFERS);
#endif
#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
    SSL_CTX_set_mode(sslctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
#endif

    /*
        Disable both SSLv2 and SSLv3 by default - they are insecure
     */
    SSL_CTX_set_options(sslctx, SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(sslctx, SSL_OP_NO_SSLv3);

    /* 
        Ensure we generate a new private key for each connection
     */
    SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_DH_USE);
    return 0;
}


PUBLIC void sslClose()
{
    if (sslctx != NULL) {
        SSL_CTX_free(sslctx);
        sslctx = NULL;
    }
}


PUBLIC int sslUpgrade(Webs *wp)
{
    WebsSocket      *sptr;
    BIO             *bio;

    assert(wp);

    sptr = socketPtr(wp->sid);
    if ((wp->ssl = SSL_new(sslctx)) == 0) {
        return -1;
    }
    if ((bio = BIO_new_socket((int) sptr->sock, BIO_NOCLOSE)) == 0) {
        return -1;
    }
    SSL_set_bio(wp->ssl, bio, bio);
    SSL_set_accept_state(wp->ssl);
    SSL_set_app_data(wp->ssl, (void*) wp);
    return 0;
}
    

PUBLIC void sslFree(Webs *wp)
{
    if (wp->ssl) {
        SSL_set_shutdown(wp->ssl, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
        SSL_free(wp->ssl);
    }
}


PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    WebsSocket      *sp;
    char            ebuf[ME_GOAHEAD_LIMIT_STRING];
    ulong           serror;
    int             rc, error, retries, i;

    if (wp->ssl == 0 || len <= 0) {
        assert(0);
        return -1;
    }
    /*  
        Limit retries on WANT_READ. If non-blocking and no data, then this can spin forever.
     */
    sp = socketPtr(wp->sid);
    retries = 5;
    for (i = 0; i < retries; i++) {
        rc = SSL_read(wp->ssl, buf, (int) len);
        if (rc < 0) {
            error = SSL_get_error(wp->ssl, rc);
            if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_CONNECT || error == SSL_ERROR_WANT_ACCEPT) {
                continue;
            }
            serror = ERR_get_error();
            ERR_error_string_n(serror, ebuf, sizeof(ebuf) - 1);
            trace(5, "SSL_read %s", ebuf);
        }
        break;
    }
    if (rc <= 0) {
        error = SSL_get_error(wp->ssl, rc);
        if (error == SSL_ERROR_WANT_READ) {
            rc = 0;
        } else if (error == SSL_ERROR_WANT_WRITE) {
            sleep(0);
            rc = 0;
        } else if (error == SSL_ERROR_ZERO_RETURN) {
            sp->flags |= SOCKET_EOF;
            rc = -1;
        } else if (error == SSL_ERROR_SYSCALL) {
            sp->flags |= SOCKET_EOF;
            rc = -1;
        } else if (error != SSL_ERROR_ZERO_RETURN) {
            /* SSL_ERROR_SSL */
            serror = ERR_get_error();
            ERR_error_string_n(serror, ebuf, sizeof(ebuf) - 1);
            trace(4, "OpenSSL: connection with protocol error: %s", ebuf);
            rc = -1;
            sp->flags |= SOCKET_EOF;
        }
    } else if (SSL_pending(wp->ssl) > 0) {
        socketHiddenData(sp, SSL_pending(wp->ssl), SOCKET_READABLE);
    }
    return rc;
}


PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    ssize   totalWritten;
    int     error, rc;

    if (wp->ssl == 0 || len <= 0) {
        assert(0);
        return -1;
    }
    totalWritten = 0;
    ERR_clear_error();

    do {
        rc = SSL_write(wp->ssl, buf, (int) len);
        trace(7, "OpenSSL: written %d, requested len %d", rc, len);
        if (rc <= 0) {
            error = SSL_get_error(wp->ssl, rc);
            if (error == SSL_ERROR_NONE) {
                break;
            } else if (error == SSL_ERROR_WANT_WRITE) {
                break;
            } else if (error == SSL_ERROR_WANT_READ) {
                //  AUTO-RETRY should stop this
                return -1;
            } else {
                trace(7, "OpenSSL: error %d", error);
                return -1;
            }
            break;
        }
        totalWritten += rc;
        buf = (void*) ((char*) buf + rc);
        len -= rc;
        trace(7, "OpenSSL: write: len %d, written %d, total %d, error %d", len, rc, totalWritten, 
            SSL_get_error(wp->ssl, rc));
    } while (len > 0);
    return totalWritten;
}


/*
    Set certificate file for SSL context
 */
static int sslSetCertFile(char *certFile)
{
    assert(sslctx);
    assert(certFile);

    if (sslctx == NULL) {
        return -1;
    }
    if (SSL_CTX_use_certificate_file(sslctx, certFile, SSL_FILETYPE_PEM) <= 0) {
        if (SSL_CTX_use_certificate_file(sslctx, certFile, SSL_FILETYPE_ASN1) <= 0) {
            error("Unable to read certificate file: %s", certFile); 
            return -1;
        }
        return -1;
    }
    /*        
          Confirm that the certificate and the private key jive.
     */
    if (!SSL_CTX_check_private_key(sslctx)) {
        return -1;
    }
    return 0;
}


/*
      Set key file for SSL context
 */
static int sslSetKeyFile(char *keyFile)
{
    assert(sslctx);
    assert(keyFile);

    if (sslctx == NULL) {
        return -1;
    }
    if (SSL_CTX_use_PrivateKey_file(sslctx, keyFile, SSL_FILETYPE_PEM) <= 0) {
        if (SSL_CTX_use_PrivateKey_file(sslctx, keyFile, SSL_FILETYPE_ASN1) <= 0) {
            error("Unable to read private key file: %s", keyFile); 
            return -1;
        }
        return -1;
    }
    return 0;
}


static int verifyX509Certificate(int ok, X509_STORE_CTX *xContext)
{
    X509            *cert;
    char            subject[260], issuer[260], peer[260];
    int             error, depth;
    
    subject[0] = issuer[0] = '\0';

    cert = X509_STORE_CTX_get_current_cert(xContext);
    error = X509_STORE_CTX_get_error(xContext);
    depth = X509_STORE_CTX_get_error_depth(xContext);

    ok = 1;
    if (X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject) - 1) < 0) {
        ok = 0;
    }
    if (X509_NAME_oneline(X509_get_issuer_name(xContext->current_cert), issuer, sizeof(issuer) - 1) < 0) {
        ok = 0;
    }
    if (X509_NAME_get_text_by_NID(X509_get_subject_name(xContext->current_cert), NID_commonName, peer, 
            sizeof(peer) - 1) < 0) {
        ok = 0;
    }
    if (ok && VERIFY_DEPTH < depth) {
        if (error == 0) {
            error = X509_V_ERR_CERT_CHAIN_TOO_LONG;
        }
    }
    switch (error) {
    case X509_V_OK:
        break;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        if (ME_GOAHEAD_VERIFY_ISSUER) {
            logmsg(3, "Self-signed certificate");
            ok = 0;
        }

    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        if (ME_GOAHEAD_VERIFY_ISSUER) {
            logmsg(3, "Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
        if (ME_GOAHEAD_VERIFY_ISSUER) {
            logmsg(3, "Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_INVALID_CA:
    default:
        logmsg(3, "Certificate verification error %d", error);
        ok = 0;
        break;
    }
    if (ok) {
        trace(3, "OpenSSL: Certificate verified: subject %s", subject);
    } else {
        trace(1, "OpenSSL: Certification failed: subject %s (more trace at level 4)", subject);
        trace(4, "OpenSSL: Error: %d: %s", error, X509_verify_cert_error_string(error));
    }
    trace(4, "OpenSSL: Issuer: %s", issuer);
    trace(4, "OpenSSL: Peer: %s", peer);
    return ok;
}


static RSA *rsaCallback(SSL *ssl, int isExport, int keyLength)
{
    static RSA *rsaTemp = NULL;

    if (rsaTemp == NULL) {
        rsaTemp = RSA_generate_key(keyLength, RSA_F4, NULL, NULL);
    }
    return rsaTemp;
}


#else
void opensslDummy() {}
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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



/********* Start of file ../../../src/upload.c ************/


/*
    upload.c -- File upload handler

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/



#if ME_GOAHEAD_UPLOAD
/************************************ Locals **********************************/
/*
    Upload states
 */
#define UPLOAD_REQUEST_HEADER    1   /* Request header */
#define UPLOAD_BOUNDARY          2   /* Boundary divider */
#define UPLOAD_CONTENT_HEADER    3   /* Content part header */
#define UPLOAD_CONTENT_DATA      4   /* Content encoded data */
#define UPLOAD_CONTENT_END       5   /* End of multipart message */

static char *uploadDir;

/*********************************** Forwards *********************************/

static void defineUploadVars(Webs *wp);
static char *getBoundary(Webs *wp, char *buf, ssize bufLen);
static int initUpload(Webs *wp);
static int processContentBoundary(Webs *wp, char *line);
static int processContentData(Webs *wp);
static int processUploadHeader(Webs *wp, char *line);

/************************************* Code ***********************************/
/*
    The upload handler functions as a filter. It never actually handles a request
 */
static bool uploadHandler(Webs *wp)
{
    assert(websValid(wp));

    if (!(wp->flags & WEBS_UPLOAD)) {
        return 0;
    }
    return initUpload(wp);
}


static int initUpload(Webs *wp)
{
    char    *boundary;
    
    if (wp->uploadState == 0) {
        wp->uploadState = UPLOAD_BOUNDARY;
        if ((boundary = strstr(wp->contentType, "boundary=")) != 0) {
            boundary += 9;
            wp->boundary = sfmt("--%s", boundary);
            wp->boundaryLen = strlen(wp->boundary);
        }
        if (wp->boundaryLen == 0 || *wp->boundary == '\0') {
            websError(wp, HTTP_CODE_BAD_REQUEST, "Bad boundary");
            return -1;
        }
        websSetVar(wp, "UPLOAD_DIR", uploadDir);
        wp->files = hashCreate(11);
    }
    return 0;
}


static void freeUploadFile(WebsUpload *up)
{
    if (up->filename) {
        unlink(up->filename);
        wfree(up->filename);
    }
    wfree(up->clientFilename);
    wfree(up->contentType);
    wfree(up);
}


PUBLIC void websFreeUpload(Webs *wp)
{
    WebsUpload  *up;
    WebsKey     *s;

    for (s = hashFirst(wp->files); s; s = hashNext(wp->files, s)) {
        up = s->content.value.symbol;
        freeUploadFile(up);
        if (up == wp->currentFile) {
            wp->currentFile = 0;
        }
    }
    hashFree(wp->files);
    if (wp->currentFile) {
        freeUploadFile(wp->currentFile);
        wp->currentFile = 0;
    }
    if (wp->upfd >= 0) {
        close(wp->upfd);
        wp->upfd = -1;
    }
}


PUBLIC int websProcessUploadData(Webs *wp) 
{
    char    *line, *nextTok;
    ssize   len, nbytes;
    int     done, rc;
    
    for (done = 0, line = 0; !done; ) {
        if  (wp->uploadState == UPLOAD_BOUNDARY || wp->uploadState == UPLOAD_CONTENT_HEADER) {
            /*
                Parse the next input line
             */
            line = wp->input.servp;
            if ((nextTok = memchr(line, '\n', bufLen(&wp->input))) == 0) {
                /* Incomplete line */
                break;
            }
            *nextTok++ = '\0';
            nbytes = nextTok - line;
            websConsumeInput(wp, nbytes);
            strim(line, "\r", WEBS_TRIM_END);
            len = strlen(line);
            if (line[len - 1] == '\r') {
                line[len - 1] = '\0';
            }
        }
        switch (wp->uploadState) {
        case 0:
            if (initUpload(wp) < 0) {
                done++;
            }
            break;

        case UPLOAD_BOUNDARY:
            if (processContentBoundary(wp, line) < 0) {
                done++;
            }
            break;

        case UPLOAD_CONTENT_HEADER:
            if (processUploadHeader(wp, line) < 0) {
                done++;
            }
            break;

        case UPLOAD_CONTENT_DATA:
            if ((rc = processContentData(wp)) < 0) {
                done++;
            }
            if (bufLen(&wp->input) < wp->boundaryLen) {
                /*  Incomplete boundary - return to get more data */
                done++;
            }
            break;

        case UPLOAD_CONTENT_END:
            done++;
            break;
        }
    }
    if (!websValid(wp)) {
        return -1;
    }
    bufCompact(&wp->input);
    return 0;
}


static int processContentBoundary(Webs *wp, char *line)
{
    /*
        Expecting a multipart boundary string
     */
    if (strncmp(wp->boundary, line, wp->boundaryLen) != 0) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Incomplete boundary");
        return -1;
    }
    if (line[wp->boundaryLen] && strcmp(&line[wp->boundaryLen], "--") == 0) {
        wp->uploadState = UPLOAD_CONTENT_END;
    } else {
        wp->uploadState = UPLOAD_CONTENT_HEADER;
    }
    return 0;
}


static int processUploadHeader(Webs *wp, char *line)
{
    WebsUpload  *file;
    char            *key, *headerTok, *rest, *nextPair, *value;

    if (line[0] == '\0') {
        wp->uploadState = UPLOAD_CONTENT_DATA;
        return 0;
    }
    trace(7, "Header line: %s", line);

    headerTok = line;
    stok(line, ": ", &rest);

    if (scaselesscmp(headerTok, "Content-Disposition") == 0) {
        /*  
            The content disposition header describes either a form variable or an uploaded file.
        
            Content-Disposition: form-data; name="field1"
            >>blank line
            Field Data
            ---boundary
     
            Content-Disposition: form-data; name="field1" filename="user.file"
            >>blank line
            File data
            ---boundary
         */
        key = rest;
        wp->uploadVar = wp->clientFilename = 0;
        while (key && stok(key, ";\r\n", &nextPair)) {

            key = strim(key, " ", WEBS_TRIM_BOTH);
            ssplit(key, "= ", &value);
            value = strim(value, "\"", WEBS_TRIM_BOTH);

            if (scaselesscmp(key, "form-data") == 0) {
                /* Nothing to do */

            } else if (scaselesscmp(key, "name") == 0) {
                wp->uploadVar = sclone(value);

            } else if (scaselesscmp(key, "filename") == 0) {
                if (wp->uploadVar == 0) {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Missing name field");
                    return -1;
                }
                wp->clientFilename = sclone(value);
                /*  
                    Create the file to hold the uploaded data
                 */
                if ((wp->uploadTmp = websTempFile(uploadDir, "tmp")) == 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, 
                        "Cannot create upload temp file %s. Check upload temp dir %s", wp->uploadTmp, uploadDir);
                    return -1;
                }
                trace(5, "File upload of: %s stored as %s", wp->clientFilename, wp->uploadTmp);

                if ((wp->upfd = open(wp->uploadTmp, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot open upload temp file %s", wp->uploadTmp);
                    return -1;
                }
                /*  
                    Create the files[id]
                 */
                file = wp->currentFile = walloc(sizeof(WebsUpload));
                memset(file, 0, sizeof(WebsUpload));
                file->clientFilename = sclone(wp->clientFilename);
                file->filename = sclone(wp->uploadTmp);
            }
            key = nextPair;
        }

    } else if (scaselesscmp(headerTok, "Content-Type") == 0) {
        if (wp->clientFilename) {
            trace(5, "Set files[%s][CONTENT_TYPE] = %s", wp->uploadVar, rest);
            wp->currentFile->contentType = sclone(rest);
        }
    }
    return 0;
}


static void defineUploadVars(Webs *wp)
{
    WebsUpload      *file;
    char            key[64];

    file = wp->currentFile;
    fmt(key, sizeof(key), "FILE_CLIENT_FILENAME_%s", wp->uploadVar);
    websSetVar(wp, key, file->clientFilename);

    fmt(key, sizeof(key), "FILE_CONTENT_TYPE_%s", wp->uploadVar);
    websSetVar(wp, key, file->contentType);

    fmt(key, sizeof(key), "FILE_FILENAME_%s", wp->uploadVar);
    websSetVar(wp, key, file->filename);

    fmt(key, sizeof(key), "FILE_SIZE_%s", wp->uploadVar);
    websSetVarFmt(wp, key, "%d", (int) file->size);
}


static int writeToFile(Webs *wp, char *data, ssize len)
{
    WebsUpload      *file;
    ssize           rc;

    file = wp->currentFile;

    if ((file->size + len) > ME_GOAHEAD_LIMIT_UPLOAD) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Uploaded file exceeds maximum %d", (int) ME_GOAHEAD_LIMIT_UPLOAD);
        return -1;
    }
    if (len > 0) {
        /*  
            File upload. Write the file data.
         */
        if ((rc = write(wp->upfd, data, (int) len)) != len) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot write to upload temp file %s, rc %d", wp->uploadTmp, rc);
            return -1;
        }
        file->size += len;
        trace(7, "uploadFilter: Wrote %d bytes to %s", len, wp->uploadTmp);
    }
    return 0;
}


static int processContentData(Webs *wp)
{
    WebsUpload  *file;
    WebsBuf         *content;
    ssize           size, nbytes;
    char            *data, *bp;

    content = &wp->input;
    file = wp->currentFile;

    size = bufLen(content);
    if (size < wp->boundaryLen) {
        /*  Incomplete boundary. Return and get more data */
        return 0;
    }
    if ((bp = getBoundary(wp, content->servp, size)) == 0) {
        trace(7, "uploadFilter: Got boundary filename %x", wp->clientFilename);
        if (wp->clientFilename) {
            /*  
                No signature found yet. probably more data to come. Must handle split boundaries.
             */
            data = content->servp;
            nbytes = ((int) (content->endp - data)) - (wp->boundaryLen - 1);
            if (nbytes > 0 && writeToFile(wp, content->servp, nbytes) < 0) {
                return -1;
            }
            websConsumeInput(wp, nbytes);
            /* Get more data */
            return 0;
        }
    }
    data = content->servp;
    nbytes = (bp) ? (bp - data) : bufLen(content);

    if (nbytes > 0) {
        websConsumeInput(wp, nbytes);
        /*  
            This is the CRLF before the boundary
         */
        if (nbytes >= 2 && data[nbytes - 2] == '\r' && data[nbytes - 1] == '\n') {
            nbytes -= 2;
        }
        if (wp->clientFilename) {
            /*  
                Write the last bit of file data and add to the list of files and define environment variables
             */
            if (writeToFile(wp, data, nbytes) < 0) {
                return -1;
            }
            hashEnter(wp->files, wp->uploadVar, valueSymbol(file), 0);
            defineUploadVars(wp);

        } else {
            /*  
                Normal string form data variables
             */
            data[nbytes] = '\0'; 
            trace(5, "uploadFilter: form[%s] = %s", wp->uploadVar, data);
            websDecodeUrl(wp->uploadVar, wp->uploadVar, -1);
            websDecodeUrl(data, data, -1);
            websSetVar(wp, wp->uploadVar, data);
        }
    }
    if (wp->clientFilename) {
        /*  
            Now have all the data (we've seen the boundary)
         */
        close(wp->upfd);
        wp->upfd = -1;
        wp->clientFilename = 0;
        wfree(wp->uploadTmp);
        wp->uploadTmp = 0;
    }
    wp->uploadState = UPLOAD_BOUNDARY;
    return 0;
}


/*  
    Find the boundary signature in memory. Returns pointer to the first match.
 */ 
static char *getBoundary(Webs *wp, char *buf, ssize bufLen)
{
    char    *cp, *endp;
    char    first;

    assert(buf);

    first = *wp->boundary;
    cp = buf;
    if (bufLen < wp->boundaryLen) {
        return 0;
    }
    endp = cp + (bufLen - wp->boundaryLen) + 1;
    while (cp < endp) {
        cp = (char *) memchr(cp, first, endp - cp);
        if (!cp) {
            return 0;
        }
        if (memcmp(cp, wp->boundary, wp->boundaryLen) == 0) {
            return cp;
        }
        cp++;
    }
    return 0;
}



WebsUpload *websLookupUpload(Webs *wp, char *key)
{
    WebsKey     *sp;

    if (wp->files) {
        if ((sp = hashLookup(wp->files, key)) == 0) {
            return 0;
        }
        return sp->content.value.symbol;
    }
    return 0;
}


WebsHash websGetUpload(Webs *wp)
{
    return wp->files;
}


PUBLIC void websUploadOpen()
{
    uploadDir = ME_GOAHEAD_UPLOAD_DIR;
    if (*uploadDir == '\0') {
#if ME_WIN_LIKE
        uploadDir = getenv("TEMP");
#else
        uploadDir = "/tmp";
#endif
    }
    trace(4, "Upload directory is %s", uploadDir);
    websDefineHandler("upload", 0, uploadHandler, 0, 0);
}

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

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

#endif /* ME_COM_GOAHEAD */