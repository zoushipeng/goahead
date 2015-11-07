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
    Return true to indicate the request was handled, even for errors.
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
PUBLIC int websDefineAction(cchar *name, void *fn)
{
    assert(name && *name);
    assert(fn);

    if (fn == NULL) {
        return -1;
    }
    hashEnter(actionTable, (char*) name, valueSymbol(fn), 0);
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



/********************************** Locals ************************************/

static WebsMemNotifier memNotifier;

PUBLIC void websSetMemNotifier(WebsMemNotifier cback)
{
    memNotifier = cback;
}


/********************************* Defines ************************************/
#if ME_GOAHEAD_REPLACE_MALLOC
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

static int wallocGetSize(ssize size, int *q);

#endif /* ME_GOAHEAD_REPLACE_MALLOC */

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
#if ME_GOAHEAD_REPLACE_MALLOC
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
#endif /* ME_GOAHEAD_REPLACE_MALLOC */
    return 0;
}


PUBLIC void wcloseAlloc()
{
#if ME_GOAHEAD_REPLACE_MALLOC
    if (--wopenCount <= 0 && !(controlFlags & WEBS_USER_BUF)) {
        free(freeBuf);
        wopenCount = 0;
    }
#endif /* ME_GOAHEAD_REPLACE_MALLOC */
}


#if ME_GOAHEAD_REPLACE_MALLOC
/*
    Allocate a block of the requested size. First check the block queues for a suitable one.
 */
PUBLIC void *walloc(ssize size)
{
    WebsAlloc   *bp;
    int         q, memSize;

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
    int         q;

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

PUBLIC void *walloc(ssize num) 
{
    void    *mem;

    if ((mem = malloc(num)) == 0) {
        if (memNotifier) {
            (memNotifier)(num);
        }
    }
    return mem;
}


PUBLIC void wfree(void *mem) 
{
    if (mem) { 
        free(mem); 
    }   
}


PUBLIC void *wrealloc(void *mem, ssize num) 
{
    if ((mem = realloc(mem, num)) == 0) {
        if (memNotifier) {
            (memNotifier)(num);
        }
    }
    return mem;  
}

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
static char *masterSecret;
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
#if ME_GOAHEAD_DIGEST
static char *calcDigest(Webs *wp, char *username, char *password);
static char *createDigestNonce(Webs *wp);
static char *parseDigestNonce(char *nonce, char **secret, char **realm, WebsTime *when);
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
            wfree(wp->username);
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
                wp->username = 0;
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
        masterSecret = websMD5(sbuf);
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

    wfree(masterSecret);
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

    assert(username);
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


PUBLIC int websSetUserPassword(char *username, char *password)
{
    WebsUser    *user;

    assert(username);
    if ((user = websLookupUser(username)) == 0) {
        return -1;
    }
    wfree(user->password);
    user->password = sclone(password);
    return 0;
}


PUBLIC int websSetUserRoles(char *username, char *roles)
{
    WebsUser    *user;

    assert(username);
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

    assert(username);
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


PUBLIC bool websLogoutUser(Webs *wp)
{
    assert(wp);
    websRemoveSessionVar(wp, WEBS_SESSION_USERNAME);
    if (smatch(wp->authType, "basic") || smatch(wp->authType, "digest")) {
        websError(wp, HTTP_CODE_UNAUTHORIZED, "Logged out.");
        return 0;
    }
    websRedirectByStatus(wp, HTTP_CODE_OK);
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
    websLogoutUser(wp);
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
    wfree(wp->username);
    wfree(wp->password);
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


#if ME_GOAHEAD_DIGEST
static void digestLogin(Webs *wp)
{
    char  *nonce, *opaque;

    assert(wp);
    assert(wp->route);

    nonce = createDigestNonce(wp);
    /* Opaque is unused. Set to anything */
    opaque = "5ccc069c403ebaf9f0171e9517f40e41";
    wfree(wp->authResponse);
    wp->authResponse = sfmt(
        "Digest realm=\"%s\", domain=\"%s\", qop=\"%s\", nonce=\"%s\", opaque=\"%s\", algorithm=\"%s\", stale=\"%s\"",
        ME_GOAHEAD_REALM, websGetServerUrl(), "auth", nonce, opaque, "MD5", "FALSE");
    wfree(nonce);
}


static bool parseDigestDetails(Webs *wp)
{
    WebsTime    when;
    char        *decoded, *value, *tok, *key, *keyBuf, *dp, *sp, *secret, *realm;
    int         seenComma;

    assert(wp);
    key = keyBuf = sclone(wp->authDetails);

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
                wfree(wp->cnonce);
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
                wfree(wp->nc);
                wp->nc = sclone(value);
            } else if (scaselesscmp(key, "nonce") == 0) {
                wfree(wp->nonce);
                wp->nonce = sclone(value);
            }
            break;

        case 'o':
            if (scaselesscmp(key, "opaque") == 0) {
                wfree(wp->opaque);
                wp->opaque = sclone(value);
            }
            break;

        case 'q':
            if (scaselesscmp(key, "qop") == 0) {
                wfree(wp->qop);
                wp->qop = sclone(value);
            }
            break;

        case 'r':
            if (scaselesscmp(key, "realm") == 0) {
                wfree(wp->realm);
                wp->realm = sclone(value);
            } else if (scaselesscmp(key, "response") == 0) {
                /* Store the response digest in the password field. This is MD5(user:realm:password) */
                wfree(wp->password);
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
                wfree(wp->digestUri);
                wp->digestUri = sclone(value);
            } else if (scaselesscmp(key, "username") == 0 || scaselesscmp(key, "user") == 0) {
                wfree(wp->username);
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
    wfree(keyBuf);

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
    decoded = parseDigestNonce(wp->nonce, &secret, &realm, &when);
    if (!smatch(masterSecret, secret)) {
        trace(2, "Access denied: Nonce mismatch");
        wfree(decoded);
        return 0;
    } else if (!smatch(realm, ME_GOAHEAD_REALM)) {
        trace(2, "Access denied: Realm mismatch");
        wfree(decoded);
        return 0;
    } else if (!smatch(wp->qop, "auth")) {
        trace(2, "Access denied: Bad qop");
        wfree(decoded);
        return 0;
    } else if ((when + (5 * 60)) < time(0)) {
        trace(2, "Access denied: Nonce is stale");
        wfree(decoded);
        return 0;
    }
    if (!wp->user) {
        if ((wp->user = websLookupUser(wp->username)) == 0) {
            trace(2, "Access denied: user is unknown");
            wfree(decoded);
            return 0;
        }
    }
    wfree(decoded);
    wfree(wp->digest);
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

    fmt(nonce, sizeof(nonce), "%s:%s:%x:%x", masterSecret, ME_GOAHEAD_REALM, time(0), next++);
    return websEncode64(nonce);
}


static char *parseDigestNonce(char *nonce, char **secret, char **realm, WebsTime *when)
{
    char    *tok, *decoded, *whenStr;

    assert(nonce && *nonce);
    assert(secret);
    assert(realm);
    assert(when);

    if ((decoded = websDecode64(nonce)) == 0) {
        return 0;
    }
    *secret = stok(decoded, ":", &tok);
    *realm = stok(NULL, ":", &tok);
    whenStr = stok(NULL, ":", &tok);
    *when = hextoi(whenStr);
    return decoded;
}


/*
   Get a Digest value using the MD5 algorithm -- See RFC 2617 to understand this code.
 */
static char *calcDigest(Webs *wp, char *username, char *password)
{
    char  a1Buf[256], a2Buf[256], digestBuf[256];
    char  *ha1, *ha2, *method, *result;

    assert(wp);
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
#endif /* ME_GOAHEAD_DIGEST */


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
#if ME_GOAHEAD_DIGEST
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

#if ME_WIN_LIKE
    typedef HANDLE CgiPid;
#else
    typedef pid_t CgiPid;
#endif

typedef struct Cgi {            /* Struct for CGI tasks which have completed */
    Webs    *wp;                /* Connection object */
    char    *stdIn;             /* File desc. for task's temp input fd */
    char    *stdOut;            /* File desc. for task's temp output fd */
    char    *cgiPath;           /* Path to executable process file */
    char    **argp;             /* Pointer to buf containing argv tokens */
    char    **envp;             /* Pointer to array of environment strings */
    CgiPid  handle;             /* Process handle of the task */
    off_t   fplacemark;         /* Seek location for CGI output file */
} Cgi;

static Cgi      **cgiList;      /* walloc chain list of wp's to be closed */
static int      cgiMax;         /* Size of walloc list */

/************************************ Forwards ********************************/

static int checkCgi(CgiPid handle);
static CgiPid launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut);

/************************************* Code ***********************************/
/*
    Process a form request.
    Return true to indicate the request was handled, even for errors.
 */
PUBLIC bool cgiHandler(Webs *wp)
{
    Cgi         *cgip;
    WebsKey     *s;
    char        cgiPrefix[ME_GOAHEAD_LIMIT_FILENAME], *stdIn, *stdOut, cwd[ME_GOAHEAD_LIMIT_FILENAME];
    char        *cp, *cgiName, *cgiPath, **argp, **envp, **ep, *tok, *query, *dir, *extraPath, *exe;
    CgiPid      pHandle;
    int         n, envpsize, argpsize, cid;

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
            exe = sfmt("%s.exe", cgiPath);
            if (stat(exe, &sbuf) == 0 && (sbuf.st_mode & S_IFREG)) {
                wfree(cgiPath);
                cgiPath = exe;
            } else {
                error("Cannot find CGI program: ", cgiPath);
                websError(wp, HTTP_CODE_NOT_FOUND | WEBS_NOLOG, "CGI program file does not exist");
                wfree(cgiPath);
                return 1;
            }
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
    if (wp->cgifd >= 0) {
        close(wp->cgifd);
        wp->cgifd = -1;
    }

    /*
        Now launch the process.  If not successful, do the cleanup of resources.  If successful, the cleanup will be
        done after the process completes.
     */
    if ((pHandle = launchCgi(cgiPath, argp, envp, stdIn, stdOut)) == (CgiPid) -1) {
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


PUBLIC bool websProcessCgiData(Webs *wp)
{
    ssize   nbytes;

    nbytes = bufLen(&wp->input);
    trace(5, "cgi: write %d bytes to CGI program", nbytes);
    if (write(wp->cgifd, wp->input.servp, (int) nbytes) != nbytes) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR| WEBS_CLOSE, "Cannot write to CGI gateway");
    } else {
        trace(5, "cgi: write %d bytes to CGI program", nbytes);
    }
    websConsumeInput(wp, nbytes);
    return 1;
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
    *end = '\0';
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
                skip = 0;
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
int websCgiPoll()
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
                wfree(cgip->stdOut);
                wfree(cgip->argp);
                wfree(cgip->envp);
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
    Returns a pointer to an allocated qualified unique temporary file name. This filename must eventually be deleted with
    wfree().
 */
PUBLIC char *websGetCgiCommName()
{
    return websTempFile(NULL, "cgi");
}


#if WINCE
/*
     Launch the CGI process and return a handle to it.  CE note: This function is not complete.  The missing piece is
     the ability to redirect stdout.
 */
static CgiPid launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
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
static int checkCgi(CgiPid handle)
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
static CgiPid launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
{
    int     fdin, fdout, pid;

    trace(5, "cgi: run %s", cgiPath);

    if ((fdin = open(stdIn, O_RDWR | O_CREAT | O_BINARY, 0666)) < 0) {
        error("Cannot open CGI stdin: ", cgiPath);
        return -1;
    }
    if ((fdout = open(stdOut, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666)) < 0) {
        error("Cannot open CGI stdout: ", cgiPath);
        return -1;
    }

    pid = vfork();
    if (pid == 0) {
        /*
            Child
         */
        if (dup2(fdin, 0) < 0) {
            printf("content-type: text/html\n\nDup of stdin failed\n");
            _exit(1);

        } else if (dup2(fdout, 1) < 0) {
            printf("content-type: text/html\n\nDup of stdout failed\n");
            _exit(1);

        } else if (execve(cgiPath, argp, envp) == -1) {
            printf("content-type: text/html\n\nExecution of cgi process failed\n");
        }
        _exit(0);
    }
    /*
        Parent
     */
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
static int checkCgi(CgiPid handle)
{
    int     pid;

    /*
        Check to see if the CGI child process has terminated or not yet.
     */
    if ((pid = waitpid((CgiPid) handle, NULL, WNOHANG)) == handle) {
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
static CgiPid launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
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
static int checkCgi(CgiPid handle)
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
    Windows launchCgi
    Create a temporary stdout file and launch the CGI process. Returns a handle to the spawned CGI process.
 */
static CgiPid launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
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
        if (pArgs[1]) {
            strcat(cmdLine, " ");
        }
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
    if (newinfo.hStdOutput == (HANDLE) -1) {
        error("Cannot open CGI stdin file");
        return (CgiPid) -1;
    }

    /*
        Stdout file is created and file pointer is reset to start.
     */
    newinfo.hStdOutput = CreateFile(stdOut, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ + FILE_SHARE_WRITE,
            &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (newinfo.hStdOutput == (HANDLE) -1) {
        error("Cannot create CGI stdout file");
        CloseHandle(newinfo.hStdInput);
        return (CgiPid) -1;
    }
    SetFilePointer(newinfo.hStdOutput, 0, NULL, FILE_END);
    newinfo.hStdError = newinfo.hStdOutput;

    dwCreateFlags = CREATE_NEW_CONSOLE;
    pEnvData = tableToBlock(envp);

    /*
        CreateProcess returns errors sometimes, even when the process was started correctly.  The cause is not evident.
        Detect an error by checking the value of procinfo.hProcess after the call.
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
        return (CgiPid) -1;
    } else {
        return procinfo.hProcess;
    }
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
static int checkCgi(CgiPid handle)
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
    Return true to indicate the request was handled, even for errors.
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

    if ((buf = walloc(ME_GOAHEAD_LIMIT_BUFFER)) == NULL) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot get memory");
        return;
    }
    while ((len = websPageReadData(wp, buf, ME_GOAHEAD_LIMIT_BUFFER)) > 0) {
        if ((wrote = websWriteSocket(wp, buf, len)) < 0) {
            /* May be an error or just socket full (EAGAIN) */
            websPageSeek(wp, -len, SEEK_CUR);
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
PUBLIC bool websProcessPutData(Webs *wp)
{
    ssize   nbytes;

    assert(wp);
    assert(wp->putfd >= 0);
    assert(wp->input.buf);

    nbytes = bufLen(&wp->input);
    wp->putLen += nbytes;
    if (wp->putLen > ME_GOAHEAD_LIMIT_PUT) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Put file too large");

    } else if (write(wp->putfd, wp->input.servp, (int) nbytes) != nbytes) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR | WEBS_CLOSE, "Cannot write to file");
    }
    websConsumeInput(wp, nbytes);
    return 1;
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
/* 
    Symbol table for web pages and files
 */
static WebsHash romFs;
static WebsRomIndex *lookup(WebsHash fs, char *path);
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
        if (len > 0 && (name[len] == '/' || name[len] == '\\')) {
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

    if ((wip = lookup(romFs, path)) == NULL) {
        return -1;
    }
    wip->pos = 0;
    return (int) (wip - websRomIndex);
#else
    return open(path, flags, mode);
#endif
}


PUBLIC void websCloseFile(int fd)
{
#if !ME_ROM
    if (fd >= 0) {
        close(fd);
    }
#endif
}


PUBLIC int websStatFile(char *path, WebsFileInfo *sbuf)
{
#if ME_ROM
    WebsRomIndex    *wip;

    assert(path && *path);

    if ((wip = lookup(romFs, path)) == NULL) {
        return -1;
    }
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
        path[len] = '\0';
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


#if ME_ROM
static WebsRomIndex *lookup(WebsHash fs, char *path)
{
    WebsKey     *sp;
    ssize       len;

    if ((sp = hashLookup(fs, path)) == NULL) {
        if (path[0] != '/') {
            path = sfmt("/%s", path);
        } else {
            path = sclone(path);
        }
        len = slen(path) - 1;
        if (path[len] == '/') {
            path[len] = '\0';
        }
        if ((sp = hashLookup(fs, path)) == NULL) {
            wfree(path);
            return 0;
        }
        wfree(path);
    }
    return (WebsRomIndex*) sp->content.value.symbol;
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
        --verbose              # Same as --log stdout:2
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
            if (argind >= argc) usage();
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
                wfree(endpoints);
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
        "    --verbose              # Same as --log stdout:2\n"
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



/********************************* Defines ************************************/

#define WEBS_TIMEOUT (ME_GOAHEAD_LIMIT_TIMEOUT * 1000)
#define PARSE_TIMEOUT (ME_GOAHEAD_LIMIT_PARSE_TIMEOUT * 1000)
#define CHUNK_LOW   128                 /* Low water mark for chunking */

/************************************ Locals **********************************/

static int          websBackground;             /* Run as a daemon */
static int          websDebug;                  /* Run in debug mode and defeat timeouts */
static int          defaultHttpPort;            /* Default port number for http */
static int          defaultSslPort;             /* Default port number for https */
static int          listens[WEBS_MAX_LISTEN];   /* Listen endpoints */;
static int          listenMax;                  /* Max entry in listens */
static Webs         **webs;                     /* Open connection list head */
static WebsHash     websMime;                   /* Set of mime types */
static int          websMax;                    /* List size */
static char         websHost[ME_MAX_IP];        /* Host name for the server */
static char         websIpAddr[ME_MAX_IP];      /* IP address for the server */
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
static bool     filterChunkData(Webs *wp);
static int      getTimeSinceMark(Webs *wp);
static char     *getToken(Webs *wp, char *delim);
static void     parseFirstLine(Webs *wp);
static void     parseHeaders(Webs *wp);
static bool     processContent(Webs *wp);
static bool     parseIncoming(Webs *wp);
static void     pruneSessions();
static void     freeSessions();
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
    websTimeOpen();
    websFsOpen();
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
        pruneId = websStartEvent(WEBS_SESSION_PRUNE, (WebsEventProc) pruneSessions, 0);
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
#if ME_GOAHEAD_UPLOAD && !ME_ROM
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
        freeSessions();
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
    websTimeClose();
    websRuntimeClose();
    websOsClose();
}


static void initWebs(Webs *wp, int flags, int reuse)
{
    WebsBuf     rxbuf;
    void        *ssl;
    char        ipaddr[ME_MAX_IP], ifaddr[ME_MAX_IP];
    int         wid, sid, timeout;

    assert(wp);

    if (reuse) {
        rxbuf = wp->rxbuf;
        wid = wp->wid;
        sid = wp->sid;
        timeout = wp->timeout;
        ssl = wp->ssl;
        scopy(ipaddr, sizeof(ipaddr), wp->ipaddr);
        scopy(ifaddr, sizeof(ifaddr), wp->ifaddr);
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
#if ME_GOAHEAD_CGI && !ME_ROM
    wp->cgifd = -1;
#endif
#if ME_GOAHEAD_UPLOAD && !ME_ROM
    wp->files = -1;
    wp->upfd = -1;
#endif
    if (reuse) {
        scopy(wp->ipaddr, sizeof(wp->ipaddr), ipaddr);
        scopy(wp->ifaddr, sizeof(wp->ifaddr), ifaddr);
    } else {
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
#if !ME_ROM
#if ME_GOAHEAD_CGI
    if (wp->cgifd >= 0) {
        close(wp->cgifd);
        wp->cgifd = -1;
    }
#endif
    if (wp->putfd >= 0) {
        close(wp->putfd);
        wp->putfd = -1;
        assert(wp->putname && wp->filename);
        if (rename(wp->putname, wp->filename) < 0) {
            error("Cannot rename PUT file from %s to %s", wp->putname, wp->filename);
        }
    }
    wfree(wp->clientFilename);
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
#if ME_GOAHEAD_UPLOAD && !ME_ROM
    wfree(wp->boundary);
    wfree(wp->uploadTmp);
    wfree(wp->uploadVar);
#endif
#if ME_GOAHEAD_CGI && !ME_ROM
    wfree(wp->cgiStdin);
#endif
#if ME_GOAHEAD_DIGEST
    wfree(wp->cnonce);
    wfree(wp->digestUri);
    wfree(wp->opaque);
    wfree(wp->nc);
    wfree(wp->nonce);
    wfree(wp->qop);
#endif
    hashFree(wp->vars);

#if ME_GOAHEAD_UPLOAD && !ME_ROM
    if (wp->files >= 0) {
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

    if (wp->finalized) {
        return;
    }
    assert(WEBS_BEGIN <= wp->state && wp->state <= WEBS_COMPLETE);
#if DEPRECATED || 1
    wp->flags |= WEBS_FINALIZED;
#endif
    wp->finalized = 1;

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
    if (!(wp->flags & WEBS_RESPONSE_TRACED)) {
        trace(3 | WEBS_RAW_MSG, "Request complete: code %d", wp->code);
    }
}


static int complete(Webs *wp, int reuse)
{
    assert(wp);
    assert(websValid(wp));
    assert(wp->state == WEBS_BEGIN || wp->state == WEBS_COMPLETE);

    if (reuse && wp->flags & WEBS_KEEP_ALIVE && wp->rxRemaining == 0) {
        reuseConn(wp);
        socketCreateHandler(wp->sid, SOCKET_READABLE, socketEvent, wp);
        trace(5, "Keep connection alive");
        return 1;
    }
    trace(5, "Close connection");
    wp->state = WEBS_BEGIN;
    wp->flags |= WEBS_CLOSED;
    return 0;
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
    if (getsockname(socketPtr(sid)->sock, (struct sockaddr*) &ifAddr, (Socklen*) &len) < 0) {
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
            } else {
                complete(wp, 0);
            }
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
                /* Reroute if the handler re-wrote the request */
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
            canProceed = complete(wp, 1);
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
    if (wp->route && wp->route->handler && wp->route->handler->service == cgiHandler) {
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
        wfree(wp->putname);
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
        protoVer = "HTTP/1.1";
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
    char    *combined, *prior, *upperKey, *cp, *key, *value, *tok;
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
            Create a header variable for each line in the header
         */
        upperKey = sfmt("HTTP_%s", key);
        for (cp = upperKey; *cp; cp++) {
            if (*cp == '-') {
                *cp = '_';
            }
        }
        supper(upperKey);
        if ((prior = websGetVar(wp, upperKey, 0)) != 0) {
            combined = sfmt("%s, %s", prior, value);
            websSetVar(wp, upperKey, combined);
            wfree(combined);
        } else {
            websSetVar(wp, upperKey, value);
        }
        wfree(upperKey);

        /*
            Track the requesting agent (browser) type
         */
        if (strcmp(key, "user-agent") == 0) {
            wfree(wp->userAgent);
            wp->userAgent = sclone(value);

        } else if (scaselesscmp(key, "authorization") == 0) {
            wfree(wp->authType);
            wp->authType = sclone(value);
            ssplit(wp->authType, " \t", &tok);
            wfree(wp->authDetails);
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
            wfree(wp->contentType);
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
            if (wp->cookie) {
                char *prior = wp->cookie;
                wp->cookie = sfmt("%s; %s", prior, value);
                wfree(prior);
            } else {
                wp->cookie = sclone(value);
            }

        } else if (strcmp(key, "host") == 0) {
            if ((int) strspn(value, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-.[]:")
                    < (int) slen(value)) {
                websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad host header");
                return;
            }
            wfree(wp->host);
            wp->host = sclone(value);

        } else if (strcmp(key, "if-modified-since") == 0) {
            if ((cp = strchr(value, ';')) != NULL) {
                *cp = '\0';
            }
            websParseDateTime(&wp->since, value, 0);

        /*
            Yes Veronica, the HTTP spec does misspell Referrer
         */
        } else if (strcmp(key, "referer") == 0) {
            wfree(wp->referrer);
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
    bool    canProceed;

    canProceed = filterChunkData(wp);
    if (!canProceed || wp->finalized) {
        return canProceed;
    }
#if !ME_ROM
#if ME_GOAHEAD_UPLOAD
    if (wp->flags & WEBS_UPLOAD) {
        canProceed = websProcessUploadData(wp);
        if (!canProceed || wp->finalized) {
            return canProceed;
        }
    }
#endif
    if (wp->putfd >= 0) {
        canProceed = websProcessPutData(wp);
        if (!canProceed || wp->finalized) {
            return canProceed;
        }
    }
#if ME_GOAHEAD_CGI
    if (wp->cgifd >= 0) {
        canProceed = websProcessCgiData(wp);
        if (!canProceed || wp->finalized) {
            return canProceed;
        }
    }
#endif
#endif
    if (wp->eof) {
        wp->state = WEBS_READY;
        /*
            Prevent reading content from the next request
            The handler may not have been created if all the content was read in the initial read. No matter.
         */
        socketDeleteHandler(wp->sid);
    }
    return canProceed;
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
    int         bad;

    assert(wp);
    assert(wp->rxbuf.buf);
    rxbuf = &wp->rxbuf;

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
                /* Insufficient data */
                return 0;
            }
            bad += (cp[-1] != '\r' || cp[0] != '\n');
            if (bad) {
                websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
                return 1;
            }
            chunkSize = hextoi(&start[2]);
            if (!isxdigit((uchar) start[2]) || chunkSize < 0) {
                websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
                return 1;
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
                    return 1;
                }
            }
            bufAdjustStart(rxbuf, cp - start + 1);
            wp->rxChunkSize = chunkSize;
            wp->rxRemaining = chunkSize;
            if (chunkSize == 0) {
#if ME_GOAHEAD_LEGACY
                wfree(wp->query);
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
            break;
        }
    }
    return 0;
}


/*
    Basic event loop. SocketReady returns true when a socket is ready for service. SocketSelect will block until an
    event occurs. SocketProcess will actually do the servicing.
 */
PUBLIC void websServiceEvents(int *finished)
{
    int     delay, nextEvent;

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
        wfree(wp->decodedQuery);
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
    char    *message, *location, *scheme, *host, *pstr;
    char    hostbuf[ME_GOAHEAD_LIMIT_STRING];
    bool    secure, fullyQualified;
    ssize   len;
    int     originalPort, port;

    assert(websValid(wp));
    assert(uri);
    message = location = NULL;

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
    Returns an allocated string.
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
    wp->code = (code & WEBS_CODE_MASK);
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
    char        *date, *protoVersion;

    assert(websValid(wp));

    if (!(wp->flags & WEBS_HEADERS_CREATED)) {
        protoVersion = wp->protoVersion;
        if (!protoVersion) {
            protoVersion = "HTTP/1.0";
            wp->flags &= ~WEBS_KEEP_ALIVE;
        }
        websWriteHeader(wp, NULL, "%s %d %s", protoVersion, wp->code, websErrorMsg(wp->code));
#if !ME_GOAHEAD_STEALTH
        websWriteHeader(wp, "Server", "GoAhead-http");
#endif
        if ((date = websGetDateString(NULL)) != NULL) {
            websWriteHeader(wp, "Date", "%s", date);
            wfree(date);
        }
        if (wp->authResponse) {
            websWriteHeader(wp, "WWW-Authenticate", "%s", wp->authResponse);
        }
        if (length >= 0) {
            if (smatch(wp->method, "HEAD")) {
                websWriteHeader(wp, "Content-Length", "%d", (int) length);
            } else if (!((100 <= wp->code && wp->code <= 199) || wp->code == 204 || wp->code == 304)) {
                /* Server must not emit a content length header for 1XX, 204 and 304 status */
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
            return written;
        }
    } else
#endif
    if ((written = socketWrite(wp->sid, buf, size)) < 0) {
        return written;
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
    int         errCode, wasBlocking;

    if (block) {
        wasBlocking = socketSetBlock(wp->sid, 1);
    }
    op = &wp->output;
    if (wp->flags & WEBS_CHUNKING) {
        trace(6, "websFlush chunking finalized %d", wp->finalized);
        if (flushChunkData(wp) && wp->finalized) {
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
            errCode = socketGetError();
            if (errCode == EWOULDBLOCK || errCode == EAGAIN) {
                /* Not an error */
                written = 0;
                break;
            }
            /*
                Connection Error
             */
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

    if (bufLen(op) == 0 && wp->finalized) {
        wp->state = WEBS_COMPLETE;
    }
    if (block) {
        socketSetBlock(wp->sid, wasBlocking);
    }
    if (written < 0) {
        /* I/O Error */
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
    int         elapsed, delay;

    wp = (Webs*) arg;
    assert(websValid(wp));

    elapsed = getTimeSinceMark(wp) * 1000;
    if (websDebug) {
        websRestartEvent(id, (int) WEBS_TIMEOUT);
        return;
    }
    if (wp->state == WEBS_BEGIN) {
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
    websRestartEvent(id, delay);
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
    wfree(wp->filename);
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
static int getTimeSinceMark(Webs *wp)
{
    return (int) (time(0) - wp->timestamp);
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
    int     sep;

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
            scopy(buf2, 1, "/");
            path = buf2;
        } else {
            /* Copy path to reinsert leading slash */
            scopy(&buf2[1], len - 1, path);
            path = buf2;
            *path = sep;
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
    return 0;
}


/*
    Normalize a URI path to remove "./",  "../" and redundant separators.
    Note: this does not make an abs path and does not map separators nor change case.
    This validates the URI and expects it to begin with "/".
    Returns an allocated path, caller must free.
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
    return path;
}


/*
    Validate a URI path for use in a HTTP request line
    The URI must contain only valid characters and must being with "/" both before and after decoding.
    A decoded, normalized URI path is returned.
    The uri is modified. Returns an allocated path. Caller must free.
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
        wfree(uri);
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


PUBLIC void websSetCookie(Webs *wp, char *name, char *value, char *path, char *cookieDomain, int lifespan, int flags)
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


/*
    Return the next token in the input stream. Does not allocate
 */
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
    return websMD5Block(idBuf, slen(idBuf), "::webs.session::");
}


WebsSession *websAllocSession(Webs *wp, char *id, int lifespan)
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
        wfree(sp->id);
        wfree(sp);
        return 0;
    }
    return sp;
}


static void freeSession(WebsSession *sp)
{
    assert(sp);

    if (sp->cache >= 0) {
        hashFree(sp->cache);
        sp->cache = -1;
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


static void pruneSessions()
{
    WebsSession     *sp;
    WebsTime        when;
    WebsKey         *sym, *next;
    int             oldCount;

    if (sessions >= 0) {
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
    }
    websRestartEvent(pruneId, WEBS_SESSION_PRUNE);
}


static void freeSessions()
{
    WebsSession     *sp;
    WebsKey         *sym, *next;

    if (sessions >= 0) {
        for (sym = hashFirst(sessions); sym; sym = next) {
            next = hashNext(sessions, sym);
            sp = (WebsSession*) sym->content.value.symbol;
            hashDelete(sessions, sp->id);
            freeSession(sp);
        }
        hashFree(sessions);
        sessions = -1;
    }
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
    Output an error message and cleanup
 */
PUBLIC void websError(Webs *wp, int code, char *fmt, ...)
{
    va_list     args;
    char        *msg, *buf;
    char        *encoded;
    int         status;

    assert(wp);
    wp->error++;
    if (code & WEBS_CLOSE) {
        wp->flags &= ~WEBS_KEEP_ALIVE;
        wp->connError++;
    }
    status = code & WEBS_CODE_MASK;
#if !ME_ROM
    if (wp->putfd >= 0) {
        close(wp->putfd);
        wp->putfd = -1;
    }
#endif
    if (wp->rxRemaining && status != 200 && status != 301 && status != 302 && status != 401) {
        /* Close connection so we don't have to consume remaining content */
        wp->flags &= ~WEBS_KEEP_ALIVE;
    }
    encoded = websEscapeHtml(wp->url);
    wfree(wp->url);
    wp->url = encoded;
    if (fmt) {
        if (!(code & WEBS_NOLOG)) {
            va_start(args, fmt);
            msg = sfmtv(fmt, args);
            va_end(args);
            trace(2, "%s", msg);
            wfree(msg);
        }
        buf = sfmt("\
<html>\r\n\
    <head><title>Document Error: %s</title></head>\r\n\
    <body>\r\n\
        <h2>Access Error: %s</h2>\r\n\
    </body>\r\n\
</html>\r\n", websErrorMsg(code), websErrorMsg(code));
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
    code &= WEBS_CODE_MASK;
    for (ep = websErrors; ep->code; ep++) {
        if (code == ep->code) {
            return ep->msg;
        }
    }
    return websErrorMsg(HTTP_CODE_INTERNAL_SERVER_ERROR);
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
        jsError(ep, "Cannot stat %s", path);
        return NULL;
    }
    if ((script = walloc(sbuf.st_size + 1)) == NULL) {
        close(fd);
        jsError(ep, "Cannot allocate %d", sbuf.st_size);
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
    Return true to indicate the request was handled, even for errors.
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
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot read %s", wp->filename);
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
            if (websWriteBlock(wp, " ", 1) < 0) {
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

/*
    Handle OPTIONS and TRACE methods.
    Return true to indicate the request was handled, even for errors.
 */
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
    char   sep;

    sep = '/';
    if (!dir || *dir == '\0') {
#if WINCE
        dir = "/Temp";
        sep = '\\';
#elif ME_WIN_LIKE
        dir = getenv("TEMP");
        sep = '\\';
#elif VXWORKS
        dir = ".";
#else
        dir = "/tmp";
#endif
    }
    if (!prefix) {
        prefix = "tmp";
    }
    return sfmt("%s%c%s-%d.tmp", dir, sep, prefix, count++);
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
#if ME_ROM
    dev[0] = '\0';
#else
    getcwd(dev, ME_GOAHEAD_LIMIT_FILENAME);
#endif
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
   Compiled by webcomp: Fri Nov  6 17:40:03 2015
 */



#if ME_ROM

/* web/favicon.ico */
static uchar p0[] = {
	   0,   0,   1,   0,   2,   0,  32,  32,  16,   0,   0,   0,   0,   0, 232,   2,
	   0,   0,  38,   0,   0,   0,  32,  32,   0,   0,   0,   0,   0,   0, 168,   8,
	   0,   0,  14,   3,   0,   0,  40,   0,   0,   0,  32,   0,   0,   0,  64,   0,
	   0,   0,   1,   0,   4,   0,   0,   0,   0,   0, 128,   2,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0, 128,   0,   0, 128,   0,   0,   0, 128, 128,   0, 128,   0,
	   0,   0, 128,   0, 128,   0, 128, 128,   0,   0, 192, 192, 192,   0, 128, 128,
	 128,   0,   0,   0, 255,   0,   0, 255,   0,   0,   0, 255, 255,   0, 255,   0,
	   0,   0, 255,   0, 255,   0, 255, 255,   0,   0, 255, 255, 255,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0, 112,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0, 255, 120, 240,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0, 248, 136, 135, 240,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,  15,   4,  56, 135,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0, 116,  64, 136, 135,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   6, 204,  64, 136, 143,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   7,  76, 196,  64, 136, 127,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0, 132, 204, 196,  67, 136, 127,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0, 244, 204, 204, 196,   8, 136, 112,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   8,  76, 204, 204, 196,   8, 136, 240,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,  76, 204, 204, 204,  68,   8, 135, 240,   0,   0,   0,   0,
	   0,   0,   0,   0, 116, 204, 204, 204, 204,  68,  56, 135, 240,   0,   0,   0,
	   0,   0,   0,   0,   8,  76, 204, 204, 204, 204,  68, 136, 127,   0,   0,   0,
	   0,   0,   0,   0,  15,  68,  68,  68,  68,  68,  68,   8, 255,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 255, 240,   0,   0,   0,
	 255,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   7, 136, 136, 136, 136, 136, 136, 143,   0,   0,   0,   0,   0,   0,   0,   7,
	 136,  51,  51,  51,  51,  51,  48, 135,   0,   0,   0,   0,   0,   0,   0,   0,
	 120,  59, 187, 187, 187, 187, 179,  72, 240,   0,   0,   0,   0,   0,   0,   0,
	   0, 120,  59, 187, 187, 187, 187,  56, 128,   0,   0,   0,   0,   0,   0,   0,
	   0,   0, 115,  59, 187, 187, 187, 176, 135,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0, 115, 187, 187, 187, 179, 136, 240,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,  15, 131, 187, 187, 187,  56, 112,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,  15, 131, 187, 187,  52, 143,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   7, 131, 187, 179, 136,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   7,  51, 187,   8, 112,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0, 248,  59,  56, 143,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0, 248,  51, 119, 240,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0, 120, 127, 240,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 143,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 255, 255,
	 255, 255, 255, 127, 255, 255, 252,  31, 255, 255, 252,   7, 255, 255, 254,   3,
	 255, 255, 255,   0, 255, 255, 255, 128,  63, 255, 255, 128,  15, 255, 255, 192,
	   3, 255, 255, 192,   1, 255, 255, 224,   0, 127, 255, 240,   0,  31, 255, 240,
	   0,   7, 255, 248,   0,   3, 255, 248,   0,   3, 255, 255, 255, 199, 243, 255,
	 255, 255, 248,   0,  15, 255, 224,   0,  15, 255, 240,   0,   7, 255, 252,   0,
	   7, 255, 255,   0,   3, 255, 255, 192,   1, 255, 255, 224,   1, 255, 255, 248,
	   0, 255, 255, 254,   0, 255, 255, 255, 128, 127, 255, 255, 192,  63, 255, 255,
	 240,  31, 255, 255, 252,  31, 255, 255, 255,  63, 255, 255, 255, 255,  40,   0,
	   0,   0,  32,   0,   0,   0,  64,   0,   0,   0,   1,   0,   8,   0,   0,   0,
	   0,   0, 128,   4,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 128,   0,   0, 128,
	   0,   0,   0, 128, 128,   0, 128,   0,   0,   0, 128,   0, 128,   0, 128, 128,
	   0,   0, 192, 192, 192,   0, 192, 220, 192,   0, 240, 202, 166,   0,   4,   4,
	   4,   0,   8,   8,   8,   0,  12,  12,  12,   0,  17,  17,  17,   0,  22,  22,
	  22,   0,  28,  28,  28,   0,  34,  34,  34,   0,  41,  41,  41,   0,  85,  85,
	  85,   0,  77,  77,  77,   0,  66,  66,  66,   0,  57,  57,  57,   0, 128, 124,
	 255,   0,  80,  80, 255,   0, 147,   0, 214,   0, 255, 236, 204,   0, 198, 214,
	 239,   0, 214, 231, 231,   0, 144, 169, 173,   0,   0,   0,  51,   0,   0,   0,
	 102,   0,   0,   0, 153,   0,   0,   0, 204,   0,   0,  51,   0,   0,   0,  51,
	  51,   0,   0,  51, 102,   0,   0,  51, 153,   0,   0,  51, 204,   0,   0,  51,
	 255,   0,   0, 102,   0,   0,   0, 102,  51,   0,   0, 102, 102,   0,   0, 102,
	 153,   0,   0, 102, 204,   0,   0, 102, 255,   0,   0, 153,   0,   0,   0, 153,
	  51,   0,   0, 153, 102,   0,   0, 153, 153,   0,   0, 153, 204,   0,   0, 153,
	 255,   0,   0, 204,   0,   0,   0, 204,  51,   0,   0, 204, 102,   0,   0, 204,
	 153,   0,   0, 204, 204,   0,   0, 204, 255,   0,   0, 255, 102,   0,   0, 255,
	 153,   0,   0, 255, 204,   0,  51,   0,   0,   0,  51,   0,  51,   0,  51,   0,
	 102,   0,  51,   0, 153,   0,  51,   0, 204,   0,  51,   0, 255,   0,  51,  51,
	   0,   0,  51,  51,  51,   0,  51,  51, 102,   0,  51,  51, 153,   0,  51,  51,
	 204,   0,  51,  51, 255,   0,  51, 102,   0,   0,  51, 102,  51,   0,  51, 102,
	 102,   0,  51, 102, 153,   0,  51, 102, 204,   0,  51, 102, 255,   0,  51, 153,
	   0,   0,  51, 153,  51,   0,  51, 153, 102,   0,  51, 153, 153,   0,  51, 153,
	 204,   0,  51, 153, 255,   0,  51, 204,   0,   0,  51, 204,  51,   0,  51, 204,
	 102,   0,  51, 204, 153,   0,  51, 204, 204,   0,  51, 204, 255,   0,  51, 255,
	  51,   0,  51, 255, 102,   0,  51, 255, 153,   0,  51, 255, 204,   0,  51, 255,
	 255,   0, 102,   0,   0,   0, 102,   0,  51,   0, 102,   0, 102,   0, 102,   0,
	 153,   0, 102,   0, 204,   0, 102,   0, 255,   0, 102,  51,   0,   0, 102,  51,
	  51,   0, 102,  51, 102,   0, 102,  51, 153,   0, 102,  51, 204,   0, 102,  51,
	 255,   0, 102, 102,   0,   0, 102, 102,  51,   0, 102, 102, 102,   0, 102, 102,
	 153,   0, 102, 102, 204,   0, 102, 153,   0,   0, 102, 153,  51,   0, 102, 153,
	 102,   0, 102, 153, 153,   0, 102, 153, 204,   0, 102, 153, 255,   0, 102, 204,
	   0,   0, 102, 204,  51,   0, 102, 204, 153,   0, 102, 204, 204,   0, 102, 204,
	 255,   0, 102, 255,   0,   0, 102, 255,  51,   0, 102, 255, 153,   0, 102, 255,
	 204,   0, 204,   0, 255,   0, 255,   0, 204,   0, 153, 153,   0,   0, 153,  51,
	 153,   0, 153,   0, 153,   0, 153,   0, 204,   0, 153,   0,   0,   0, 153,  51,
	  51,   0, 153,   0, 102,   0, 153,  51, 204,   0, 153,   0, 255,   0, 153, 102,
	   0,   0, 153, 102,  51,   0, 153,  51, 102,   0, 153, 102, 153,   0, 153, 102,
	 204,   0, 153,  51, 255,   0, 153, 153,  51,   0, 153, 153, 102,   0, 153, 153,
	 153,   0, 153, 153, 204,   0, 153, 153, 255,   0, 153, 204,   0,   0, 153, 204,
	  51,   0, 102, 204, 102,   0, 153, 204, 153,   0, 153, 204, 204,   0, 153, 204,
	 255,   0, 153, 255,   0,   0, 153, 255,  51,   0, 153, 204, 102,   0, 153, 255,
	 153,   0, 153, 255, 204,   0, 153, 255, 255,   0, 204,   0,   0,   0, 153,   0,
	  51,   0, 204,   0, 102,   0, 204,   0, 153,   0, 204,   0, 204,   0, 153,  51,
	   0,   0, 204,  51,  51,   0, 204,  51, 102,   0, 204,  51, 153,   0, 204,  51,
	 204,   0, 204,  51, 255,   0, 204, 102,   0,   0, 204, 102,  51,   0, 153, 102,
	 102,   0, 204, 102, 153,   0, 204, 102, 204,   0, 153, 102, 255,   0, 204, 153,
	   0,   0, 204, 153,  51,   0, 204, 153, 102,   0, 204, 153, 153,   0, 204, 153,
	 204,   0, 204, 153, 255,   0, 204, 204,   0,   0, 204, 204,  51,   0, 204, 204,
	 102,   0, 204, 204, 153,   0, 204, 204, 204,   0, 204, 204, 255,   0, 204, 255,
	   0,   0, 204, 255,  51,   0, 153, 255, 102,   0, 204, 255, 153,   0, 204, 255,
	 204,   0, 204, 255, 255,   0, 204,   0,  51,   0, 255,   0, 102,   0, 255,   0,
	 153,   0, 204,  51,   0,   0, 255,  51,  51,   0, 255,  51, 102,   0, 255,  51,
	 153,   0, 255,  51, 204,   0, 255,  51, 255,   0, 255, 102,   0,   0, 255, 102,
	  51,   0, 204, 102, 102,   0, 255, 102, 153,   0, 255, 102, 204,   0, 204, 102,
	 255,   0, 255, 153,   0,   0, 255, 153,  51,   0, 255, 153, 102,   0, 255, 153,
	 153,   0, 255, 153, 204,   0, 255, 153, 255,   0, 255, 204,   0,   0, 255, 204,
	  51,   0, 255, 204, 102,   0, 255, 204, 153,   0, 255, 204, 204,   0, 255, 204,
	 255,   0, 255, 255,  51,   0, 204, 255, 102,   0, 255, 255, 153,   0, 255, 255,
	 204,   0, 102, 102, 255,   0, 102, 255, 102,   0, 102, 255, 255,   0, 255, 102,
	 102,   0, 255, 102, 255,   0, 255, 255, 102,   0,  33,   0, 165,   0,  95,  95,
	  95,   0, 119, 119, 119,   0, 134, 134, 134,   0, 150, 150, 150,   0, 203, 203,
	 203,   0, 178, 178, 178,   0, 215, 215, 215,   0, 221, 221, 221,   0, 227, 227,
	 227,   0, 234, 234, 234,   0, 241, 241, 241,   0, 248, 248, 248,   0, 240, 251,
	 255,   0, 164, 160, 160,   0, 128, 128, 128,   0,   0,   0, 255,   0,   0, 255,
	   0,   0,   0, 255, 255,   0, 255,   0,   0,   0, 255,   0, 255,   0, 255, 255,
	   0,   0, 255, 255, 255,   0,  10,  10,  10,  10,  10,  10,  10,  15, 109, 243,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10, 236,  60, 102,
	 239,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  20, 166,
	 101,  19, 188,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,   7, 101,
	 199, 166,  60, 109, 242,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  18,
	 166, 199, 199, 166,  66, 236, 244,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10, 242,
	  66, 199, 199, 199, 199, 101,  20, 239,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	 248, 101, 199, 199, 199, 199, 199,  95,  19, 240,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	 255,  17, 199, 199, 199, 199, 199, 199, 166,  60, 109, 243,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10, 239,  95, 199, 199, 199, 199, 199, 199, 199, 166,  16, 237, 244,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10, 255,  20, 166, 199, 199, 199, 199, 199, 199, 199, 199, 101,  20, 239,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10, 240,  60, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,  95,  19,
	 188,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10, 235, 166, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 166,
	  60, 109, 243,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10, 242,  16, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
	 199, 166, 101, 237, 244,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10, 146,  60, 101,  66,  66,  66,  66,  66,  66,  66,  66,  66,
	 101, 101,  60,  10,  19,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244,
	 244, 244, 244, 244, 242,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10, 243, 244, 244, 244, 244, 244, 244,
	 244, 244, 244, 244, 244, 244, 244, 244, 244,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10, 234,  11,  34,  41,  41,  41,  41,
	  41,  41,  41,  41,  41,  41,  41,  41,  34, 236,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10, 244, 247,  21,  48, 251, 251, 251,
	 251, 251, 251, 251, 251, 251, 251, 251, 251,  41, 240,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10, 244, 248,  41,  55, 251,
	 251, 251, 251, 251, 251, 251, 251, 251, 251,  55,  18,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10, 240,  18,  41,
	  55, 251, 251, 251, 251, 251, 251, 251, 251, 251,  41, 239,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,   7,
	  20,   3, 251, 251, 251, 251, 251, 251, 251, 251,  55,  20, 244,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	 255, 247,  34,  48, 251, 251, 251, 251, 251, 251, 251,   3, 237,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10, 244, 235,  34,  55, 251, 251, 251, 251, 251, 251,  34, 242,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10, 240,  18,  41,  55, 251, 251, 251, 251,  55, 109,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10, 239,  19,   3, 251, 251, 251, 251,  41,   7,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10, 255, 247,  21,  48, 251, 251,  55,  20,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10, 244, 236,  34,  55, 251,   3, 146,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10, 241, 234,  41,  55,  74,
	 243,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,   7,  74,  34,
	 235,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
	  10,  10,  10,  10,  10,  10, 254,  63, 255, 255, 254,  31, 255, 255, 255,   7,
	 255, 255, 255,   1, 255, 255, 255, 128, 127, 255, 255, 128,  63, 255, 255, 192,
	  15, 255, 255, 192,   3, 255, 255, 224,   0, 255, 255, 224,   0, 127, 255, 240,
	   0,  31, 255, 248,   0,   7, 255, 248,   0,   1, 255, 252,   0,   1, 255, 254,
	   0,   1, 255, 255, 255, 255, 255, 255, 255, 255, 224,   0,  31, 255, 224,   0,
	  15, 255, 224,   0,   7, 255, 248,   0,   7, 255, 254,   0,   3, 255, 255, 128,
	   1, 255, 255, 192,   1, 255, 255, 240,   0, 255, 255, 252,   0, 255, 255, 255,
	   0, 127, 255, 255, 128, 127, 255, 255, 224,  63, 255, 255, 248,  31, 255, 255,
	 254,  31, 255, 255, 255, 255,  10,
	   0
};

/* web/index.html */
static uchar p1[] = {
	  60,  33,  68,  79,  67,  84,  89,  80,  69,  32, 104, 116, 109, 108,  62,  10,
	  60, 104, 116, 109, 108,  32, 108,  97, 110, 103,  61,  34, 101, 110,  34,  62,
	  10,  60, 104, 101,  97, 100,  62,  10,  60, 116, 105, 116, 108, 101,  62,  72,
	 111, 109, 101,  32,  80,  97, 103, 101,  60,  47, 116, 105, 116, 108, 101,  62,
	  10,  60,  47, 104, 101,  97, 100,  62,  10,  10,  60,  98, 111, 100, 121,  62,
	  10,  32,  32,  32,  32,  60, 112,  62,  67, 111, 110, 103, 114,  97, 116, 117,
	 108,  97, 116, 105, 111, 110, 115,  33,  32,  84, 104, 101,  32, 115, 101, 114,
	 118, 101, 114,  32, 105, 115,  32, 117, 112,  32,  97, 110, 100,  32, 114, 117,
	 110, 110, 105, 110, 103,  46,  60, 112,  47,  62,  10,  60,  47,  98, 111, 100,
	 121,  62,  10,  60,  47, 104, 116, 109, 108,  62,  10,
	   0
};

/* auth.txt */
static uchar p2[] = {
	  35,  10,  35,  32,  32,  32,  97, 117, 116, 104,  46, 116, 120, 116,  32,  45,
	  32,  65, 117, 116, 104, 111, 114, 105, 122,  97, 116, 105, 111, 110,  32, 100,
	  97, 116,  97,  10,  35,  10,  35,  32,  32,  32,  83,  99, 104, 101, 109,  97,
	  10,  35,  32,  32,  32,  32,  32,  32,  32, 117, 115, 101, 114,  32, 110,  97,
	 109, 101,  61,  78,  65,  77,  69,  32, 112,  97, 115, 115, 119, 111, 114, 100,
	  61,  80,  65,  83,  83,  87,  79,  82,  68,  32, 114, 111, 108, 101, 115,  61,
	  82,  79,  76,  69,  83,  10,  35,  32,  32,  32,  32,  32,  32,  32, 114, 111,
	 108, 101,  32, 110,  97, 109, 101,  61,  78,  65,  77,  69,  32,  97,  98, 105,
	 108, 105, 116, 105, 101, 115,  61,  65,  66,  73,  76,  73,  84,  73,  69,  83,
	  10,  35,  10,  35,  32,  32,  32,  82, 111, 117, 116, 101, 115,  32,  40, 115,
	 101, 101,  32, 114, 111, 117, 116, 101,  46, 116, 120, 116,  41,  32, 109,  97,
	 121,  32, 114, 101, 113, 117, 105, 114, 101,  32,  97, 117, 116, 104, 101, 110,
	 116, 105,  99,  97, 116, 105, 111, 110,  32,  97, 110, 100,  32, 116, 104,  97,
	 116,  32, 117, 115, 101, 114, 115,  32, 112, 111, 115, 115, 101, 115, 115,  32,
	  99, 101, 114, 116,  97, 105, 110,  32,  97,  98, 105, 108, 105, 116, 105, 101,
	 115,  46,  10,  35,  10,  35,  32,  32,  32,  69, 120,  97, 109, 112, 108, 101,
	 115,  58,  10,  35,  10,  35,  32,  32,  32,  68, 101, 102, 105, 110, 101,  32,
	 114, 111, 108, 101, 115,  10,  35,  32,  32,  32,  32,  32,  32,  32, 114, 111,
	 108, 101,  32, 110,  97, 109, 101,  61, 109,  97, 110,  97, 103, 101, 114,  32,
	  97,  98, 105, 108, 105, 116, 105, 101, 115,  61, 118, 105, 101, 119,  44, 101,
	 100, 105, 116,  44, 100, 101, 108, 101, 116, 101,  10,  35,  10,  35,  32,  32,
	  32,  68, 101, 102, 105, 110, 101,  32,  97,  32, 117, 115, 101, 114,  10,  35,
	  32,  32,  32,  32,  32,  32,  32, 117, 115, 101, 114,  32, 110,  97, 109, 101,
	  61, 106, 111, 115, 104, 117,  97,  32, 112,  97, 115, 115, 119, 111, 114, 100,
	  61,  50, 102, 100,  54, 101,  52,  55, 102, 102,  57,  98,  98,  55,  48,  99,
	  48,  52,  54,  53, 102, 100,  50, 102,  53,  99,  56, 101,  53,  51,  48,  53,
	 101,  32, 114, 111, 108, 101, 115,  61, 109,  97, 110,  97, 103, 101, 114,  44,
	 112, 117, 114,  99, 104,  97, 115, 101, 114,  10,  35,  10,
	   0
};

/* route.txt */
static uchar p3[] = {
	  35,  10,  35,  32,  32,  32, 114, 111, 117, 116, 101,  46, 116, 120, 116,  32,
	  45,  32,  82, 111, 117, 116, 101,  32,  97, 110, 100,  32,  97, 117, 116, 104,
	 111, 114, 105, 122,  97, 116, 105, 111, 110,  32,  99, 111, 110, 102, 105, 103,
	 117, 114,  97, 116, 105, 111, 110,  10,  35,  10,  35,  32,  32,  32,  83,  99,
	 104, 101, 109,  97,  10,  35,  32,  32,  32,  32,  32,  32,  32, 114, 111, 117,
	 116, 101,  32, 117, 114, 105,  61,  85,  82,  73,  32, 112, 114, 111, 116, 111,
	  99, 111, 108,  61,  80,  82,  79,  84,  79,  67,  79,  76,  32, 109, 101, 116,
	 104, 111, 100, 115,  61,  77,  69,  84,  72,  79,  68,  83,  32, 104,  97, 110,
	 100, 108, 101, 114,  61,  72,  65,  78,  68,  76,  69,  82,  32, 114, 101, 100,
	 105, 114, 101,  99, 116,  61,  83,  84,  65,  84,  85,  83,  64,  85,  82,  73,
	  32,  92,  10,  35,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32, 101,
	 120, 116, 101, 110, 115, 105, 111, 110, 115,  61,  69,  88,  84,  69,  78,  83,
	  73,  79,  78,  83,  32,  97,  98, 105, 108, 105, 116, 105, 101, 115,  61,  65,
	  66,  73,  76,  73,  84,  73,  69,  83,  32,  10,  35,  10,  35,  32,  32,  32,
	  82, 111, 117, 116, 101, 115,  32, 109,  97, 121,  32, 114, 101, 113, 117, 105,
	 114, 101,  32,  97, 117, 116, 104, 101, 110, 116, 105,  99,  97, 116, 105, 111,
	 110,  32,  97, 110, 100,  32, 116, 104,  97, 116,  32, 117, 115, 101, 114, 115,
	  32, 112, 111, 115, 115, 101, 115, 115,  32,  99, 101, 114, 116,  97, 105, 110,
	  32,  97,  98, 105, 108, 105, 116, 105, 101, 115,  46,  10,  35,  32,  32,  32,
	  84, 104, 101,  32,  97,  98, 105, 108, 105, 116, 105, 101, 115,  44,  32, 101,
	 120, 116, 101, 110, 115, 105, 111, 110, 115,  44,  32, 109, 101, 116, 104, 111,
	 100, 115,  32,  97, 110, 100,  32, 114, 101, 100, 105, 114, 101,  99, 116,  32,
	 107, 101, 121, 119, 111, 114, 100, 115,  32, 117, 115, 101,  32,  99, 111, 109,
	 109,  97,  32, 115, 101, 112,  97, 114,  97, 116, 101, 100,  32, 116, 111, 107,
	 101, 110, 115,  32, 116, 111,  32, 101, 120, 112, 114, 101, 115, 115,  32,  97,
	  32, 115, 101, 116,  32, 111, 102,  32,  10,  35,  32,  32,  32,  32,  32,  32,
	  32, 114, 101, 113, 117, 105, 114, 101, 100,  32, 111, 112, 116, 105, 111, 110,
	 115,  44,  32, 111, 114,  32, 117, 115, 101,  32,  34, 124,  34,  32, 115, 101,
	 112,  97, 114,  97, 116, 101, 100,  32, 116, 111, 107, 101, 110, 115,  32, 102,
	 111, 114,  32,  97,  32, 115, 101, 116,  32, 111, 102,  32,  97, 108, 116, 101,
	 114, 110,  97, 116, 105, 118, 101,  32, 111, 112, 116, 105, 111, 110, 115,  46,
	  32,  84, 104, 105, 115,  32, 105, 109, 112, 108, 101, 109, 101, 110, 116, 115,
	  32,  65,  78,  68,  47,  79,  82,  46,  10,  35,  32,  32,  32,  84, 104, 101,
	  32, 112, 114, 111, 116, 111,  99, 111, 108,  32, 107, 101, 121, 119, 111, 114,
	 100,  32, 109,  97, 121,  32,  98, 101,  32, 115, 101, 116,  32, 116, 111,  32,
	 104, 116, 116, 112,  32, 111, 114,  32, 104, 116, 116, 112, 115,  46,  32,  84,
	 104, 101,  32, 114, 101, 100, 105, 114, 101,  99, 116,  32, 115, 116,  97, 116,
	 117, 115,  32, 109,  97, 121,  32,  98, 101,  32,  34,  42,  34,  32, 116, 111,
	  32, 109,  97, 116,  99, 104,  32,  97, 108, 108,  32,  72,  84,  84,  80,  32,
	 115, 116,  97, 116, 117, 115,  32,  99, 111, 100, 101, 115,  46,  10,  35,  32,
	  32,  32,  77, 117, 108, 116, 105, 112, 108, 101,  32, 114, 101, 100, 105, 114,
	 101,  99, 116,  32, 102, 105, 101, 108, 100, 115,  32,  97, 114, 101,  32, 112,
	 101, 114, 109, 105, 115, 115,  97,  98, 108, 101,  10,  35,  10,  35,  32,  32,
	  32,  69, 120,  97, 109, 112, 108, 101, 115,  58,  10,  35,  10,  35,  32,  32,
	  32,  85, 110, 105, 118, 101, 114, 115,  97, 108, 108, 121,  32, 114, 101, 100,
	 105, 114, 101,  99, 116,  32, 104, 116, 116, 112,  32, 116, 111,  32, 104, 116,
	 116, 112, 115,  32, 102, 111, 114,  32, 115, 101,  99, 117, 114, 101,  32,  99,
	 111, 109, 109, 117, 110, 105,  99,  97, 116, 105, 111, 110, 115,  10,  35,  32,
	  32,  32,  32,  32,  32,  32, 114, 111, 117, 116, 101,  32, 117, 114, 105,  61,
	  47,  32, 112, 114, 111, 116, 111,  99, 111, 108,  61, 104, 116, 116, 112,  32,
	 114, 101, 100, 105, 114, 101,  99, 116,  61,  42,  64, 104, 116, 116, 112, 115,
	  32, 104,  97, 110, 100, 108, 101, 114,  61, 114, 101, 100, 105, 114, 101,  99,
	 116,  10,  35,  10,  35,  32,  32,  32,  70, 111, 114, 109,  32,  98,  97, 115,
	 101, 100,  32, 108, 111, 103, 105, 110,  32, 112,  97, 116, 116, 101, 114, 110,
	  46,  32,  10,  35,  32,  32,  32,  32,  32,  32,  32, 114, 111, 117, 116, 101,
	  32, 117, 114, 105,  61,  47, 112, 117,  98,  47,  10,  35,  32,  32,  32,  32,
	  32,  32,  32, 114, 111, 117, 116, 101,  32, 117, 114, 105,  61,  47,  97,  99,
	 116, 105, 111, 110,  47, 108, 111, 103, 105, 110,  32, 109, 101, 116, 104, 111,
	 100, 115,  61,  80,  79,  83,  84,  32, 104,  97, 110, 100, 108, 101, 114,  61,
	  97,  99, 116, 105, 111, 110,  32, 114, 101, 100, 105, 114, 101,  99, 116,  61,
	  50,  48,  48,  64,  47,  32, 114, 101, 100, 105, 114, 101,  99, 116,  61,  52,
	  48,  49,  64,  47, 112, 117,  98,  47, 108, 111, 103, 105, 110,  46, 104, 116,
	 109, 108,  10,  35,  32,  32,  32,  32,  32,  32,  32, 114, 111, 117, 116, 101,
	  32, 117, 114, 105,  61,  47,  97,  99, 116, 105, 111, 110,  47, 108, 111, 103,
	 111, 117, 116,  32, 109, 101, 116, 104, 111, 100, 115,  61,  80,  79,  83,  84,
	  32, 104,  97, 110, 100, 108, 101, 114,  61,  97,  99, 116, 105, 111, 110,  32,
	 114, 101, 100, 105, 114, 101,  99, 116,  61,  50,  48,  48,  64,  47, 112, 117,
	  98,  47, 108, 111, 103, 105, 110,  46, 104, 116, 109, 108,  10,  35,  32,  32,
	  32,  32,  32,  32,  32, 114, 111, 117, 116, 101,  32, 117, 114, 105,  61,  47,
	  32,  97, 117, 116, 104,  61, 102, 111, 114, 109,  32, 104,  97, 110, 100, 108,
	 101, 114,  61,  99, 111, 110, 116, 105, 110, 117, 101,  32, 114, 101, 100, 105,
	 114, 101,  99, 116,  61,  52,  48,  49,  64,  47, 112, 117,  98,  47, 108, 111,
	 103, 105, 110,  46, 104, 116, 109, 108,  10,  35,  10,  35,  32,  32,  32,  83,
	  97, 109, 112, 108, 101,  32,  98,  97, 115, 105,  99,  32, 111, 114,  32, 100,
	 105, 103, 101, 115, 116,  32,  97, 117, 116, 104, 101, 110, 116, 105,  99,  97,
	 116, 105, 111, 110,  32, 102, 111, 114,  32, 117, 115, 101, 114,  32,  34, 106,
	 111, 115, 104, 117,  97,  34,  32,  10,  35,  32,  32,  32,  32,  32,  32,  32,
	 114, 111, 117, 116, 101,  32, 117, 114, 105,  61,  47,  97, 117, 116, 104,  47,
	  98,  97, 115, 105,  99,  47,  32,  97, 117, 116, 104,  61,  98,  97, 115, 105,
	  99,  32,  97,  98, 105, 108, 105, 116, 105, 101, 115,  61, 109,  97, 110,  97,
	 103, 101,  10,  35,  32,  32,  32,  32,  32,  32,  32, 114, 111, 117, 116, 101,
	  32, 117, 114, 105,  61,  47,  97, 117, 116, 104,  47, 100, 105, 103, 101, 115,
	 116,  47,  32,  97, 117, 116, 104,  61, 100, 105, 103, 101, 115, 116,  32,  97,
	  98, 105, 108, 105, 116, 105, 101, 115,  61, 109,  97, 110,  97, 103, 101,  10,
	  35,  10,  35,  32,  32,  32,  69,  97, 110,  97,  98, 108, 101,  32, 116, 104,
	 101,  32,  80,  85,  84,  32, 111, 114,  32,  68,  69,  76,  69,  84,  69,  32,
	 109, 101, 116, 104, 111, 100, 115,  32,  40, 111, 110, 108, 121,  41,  32, 102,
	 111, 114,  32, 116, 104, 101,  32,  66,  73,  84,  95,  71,  79,  65,  72,  69,
	  65,  68,  95,  80,  85,  84,  95,  68,  73,  82,  32, 100, 105, 114, 101,  99,
	 116, 111, 114, 121,  10,  35,  32,  32,  32,  32,  32,  32,  32, 114, 111, 117,
	 116, 101,  32, 117, 114, 105,  61,  47, 112, 117, 116,  47,  32, 109, 101, 116,
	 104, 111, 100, 115,  61,  80,  85,  84, 124,  68,  69,  76,  69,  84,  69,  10,
	  35,  10,  35,  32,  32,  32,  83, 116,  97, 110, 100,  97, 114, 100,  32, 114,
	 111, 117, 116, 101, 115,  10,  35,  10,  35,  32, 114, 111, 117, 116, 101,  32,
	 117, 114, 105,  61,  47,  99, 103, 105,  45,  98, 105, 110,  32, 100, 105, 114,
	  61,  99, 103, 105,  45,  98, 105, 110,  32, 104,  97, 110, 100, 108, 101, 114,
	  61,  99, 103, 105,  10, 114, 111, 117, 116, 101,  32, 117, 114, 105,  61,  47,
	  97,  99, 116, 105, 111, 110,  32, 104,  97, 110, 100, 108, 101, 114,  61,  97,
	  99, 116, 105, 111, 110,  10,  35,  32, 114, 111, 117, 116, 101,  32, 117, 114,
	 105,  61,  47,  32, 101, 120, 116, 101, 110, 115, 105, 111, 110, 115,  61, 106,
	 115, 116,  32, 104,  97, 110, 100, 108, 101, 114,  61, 106, 115, 116,  10, 114,
	 111, 117, 116, 101,  32, 117, 114, 105,  61,  47,  32, 109, 101, 116, 104, 111,
	 100, 115,  61,  79,  80,  84,  73,  79,  78,  83, 124,  84,  82,  65,  67,  69,
	  32, 104,  97, 110, 100, 108, 101, 114,  61, 111, 112, 116, 105, 111, 110, 115,
	  10,  10,  35,  32,  32,  32,  70, 111, 114,  32, 108, 101, 103,  97,  99, 121,
	  32,  71, 111,  65, 104, 101,  97, 100,  32,  97, 112, 112, 108, 105,  99,  97,
	 116, 105, 111, 110, 115,  32, 117, 115, 105, 110, 103,  32,  47, 103, 111, 102,
	 111, 114, 109,  10, 114, 111, 117, 116, 101,  32, 117, 114, 105,  61,  47, 103,
	 111, 102, 111, 114, 109,  32, 104,  97, 110, 100, 108, 101, 114,  61,  97,  99,
	 116, 105, 111, 110,  10,  10,  35,  10,  35,  32,  32,  32,  67,  97, 116,  99,
	 104,  45,  97, 108, 108,  32, 114, 111, 117, 116, 101,  32, 119, 105, 116, 104,
	 111, 117, 116,  32,  97, 117, 116, 104, 101, 110, 116, 105,  99,  97, 116, 105,
	 111, 110,  32, 102, 111, 114,  32,  97, 108, 108,  32, 111, 116, 104, 101, 114,
	  32,  85,  82,  73, 115,  10,  35,  10, 114, 111, 117, 116, 101,  32, 117, 114,
	 105,  61,  47,  10,  10,
	   0
};

/* certs/samples/self.crt */
static uchar p4[] = {
	  45,  45,  45,  45,  45,  66,  69,  71,  73,  78,  32,  67,  69,  82,  84,  73,
	  70,  73,  67,  65,  84,  69,  45,  45,  45,  45,  45,  10,  77,  73,  73,  68,
	  47,  84,  67,  67,  65, 117,  87, 103,  65, 119,  73,  66,  65, 103,  73,  74,
	  65,  78,  86,  68, 110,  71,  65,  55, 119,  69,  50, 117,  77,  65,  48,  71,
	  67,  83, 113,  71,  83,  73,  98,  51,  68,  81,  69,  66,  67, 119,  85,  65,
	  77,  73,  71,  85,  77,  81, 115, 119,  67,  81,  89,  68,  10,  86,  81,  81,
	  71,  69, 119,  74,  86,  85, 122,  69,  84,  77,  66,  69,  71,  65,  49,  85,
	  69,  67,  65, 119,  75,  86,  50,  70, 122,  97,  71, 108, 117,  90,  51,  82,
	 118,  98, 106,  69,  81,  77,  65,  52,  71,  65,  49,  85,  69,  66, 119, 119,
	  72,  85,  50,  86, 104, 100,  72,  82, 115,  90,  84,  69,  85,  10,  77,  66,
	  73,  71,  65,  49,  85,  69,  67, 103, 119,  76,  82,  88, 104, 104,  98,  88,
	  66, 115,  90,  83,  53, 106,  98,  50,  48, 120,  70,  68,  65,  83,  66, 103,
	  78,  86,  66,  65, 115,  77,  67,  48,  82, 108, 100, 109,  86, 115,  98,  51,
	  66, 116,  90,  87,  53,  48,  77,  82,  73, 119,  69,  65,  89,  68,  10,  86,
	  81,  81,  68,  68,  65, 108, 115,  98,  50,  78, 104,  98,  71, 104, 118,  99,
	  51,  81, 120,  72, 106,  65,  99,  66, 103, 107, 113, 104, 107, 105,  71,  57,
	 119,  48,  66,  67,  81,  69,  87,  68,  50,  82, 108, 100, 107,  66, 108, 101,
	  71,  70, 116,  99,  71, 120, 108,  76, 109,  78, 118,  98,  84,  65, 101,  10,
	  70, 119,  48, 120,  78,  84,  69, 119,  77, 106, 107, 119,  77,  68,  85,  51,
	  77,  84,  86,  97,  70, 119,  48, 121,  78,  84,  69, 119,  77, 106,  89, 119,
	  77,  68,  85,  51,  77,  84,  86,  97,  77,  73,  71,  85,  77,  81, 115, 119,
	  67,  81,  89,  68,  86,  81,  81,  71,  69, 119,  74,  86,  85, 122,  69,  84,
	  10,  77,  66,  69,  71,  65,  49,  85,  69,  67,  65, 119,  75,  86,  50,  70,
	 122,  97,  71, 108, 117,  90,  51,  82, 118,  98, 106,  69,  81,  77,  65,  52,
	  71,  65,  49,  85,  69,  66, 119, 119,  72,  85,  50,  86, 104, 100,  72,  82,
	 115,  90,  84,  69,  85,  77,  66,  73,  71,  65,  49,  85,  69,  67, 103, 119,
	  76,  10,  82,  88, 104, 104,  98,  88,  66, 115,  90,  83,  53, 106,  98,  50,
	  48, 120,  70,  68,  65,  83,  66, 103,  78,  86,  66,  65, 115,  77,  67,  48,
	  82, 108, 100, 109,  86, 115,  98,  51,  66, 116,  90,  87,  53,  48,  77,  82,
	  73, 119,  69,  65,  89,  68,  86,  81,  81,  68,  68,  65, 108, 115,  98,  50,
	  78, 104,  10,  98,  71, 104, 118,  99,  51,  81, 120,  72, 106,  65,  99,  66,
	 103, 107, 113, 104, 107, 105,  71,  57, 119,  48,  66,  67,  81,  69,  87,  68,
	  50,  82, 108, 100, 107,  66, 108, 101,  71,  70, 116,  99,  71, 120, 108,  76,
	 109,  78, 118,  98,  84,  67,  67,  65,  83,  73, 119,  68,  81,  89,  74,  75,
	 111,  90,  73,  10, 104, 118,  99,  78,  65,  81,  69,  66,  66,  81,  65,  68,
	 103, 103,  69,  80,  65,  68,  67,  67,  65,  81, 111,  67, 103, 103,  69,  66,
	  65,  75,  50,  85,  77,  43,  74,  97,  71, 103,  69, 101, 116,  80, 101,  97,
	  73,  56,  74,  71,  52,  81,  98, 121, 121, 118,  48,  67, 122,  88, 122, 112,
	  77, 101, 116, 106,  10,  75,  48, 120, 106, 118, 105,  53,  47,  69,  97,  82,
	  86, 121,  81, 103, 118, 115,  85,  68, 117,  89, 105, 121, 116,  90,  83,  48,
	 104, 115,  70,  75,  48, 122, 102,  48,  87, 118, 114, 117, 109,  43, 109,  90,
	  78, 119, 120,  55,  86, 122,  98, 100,  48, 119,  83,  84,  65,  47,  49,  83,
	  76, 102, 105, 114, 122,  10, 109,  84,  90, 111,  75, 114,  55, 104,  66,  83,
	 108, 115,  88,  85,  68,  90,  80,  54,  50,  85,  85, 113,  88, 101,  98, 109,
	 102,  49,  55, 111, 109,  47,  65,  85, 104, 105,  83,  43,  51, 105,  80,  68,
	  85,  89,  53,  72,  79,  81, 120,  68, 105,  99,  54,  53,  80,  47,  75,  73,
	 118, 101, 117,  47, 112,  71,  10,  71,  74,  74, 113, 114, 113,  65,  80,  76,
	  49,  77,  43, 107, 112, 118,  51,  70,  75, 113,  73,  74, 122,  69,  86,  88,
	  99,  43,  82,  67,  67,  98,  84, 100,  67,  69,  80, 118,  77,  47, 117, 106,
	  79,  74,  87, 120,  97,  51,  52, 117,  71,  66,  47,  70, 102, 103,  83,  80,
	 122, 108, 105, 113,  76,  97,  66,  10, 114,  89,  83, 108, 114,  97, 104,  78,
	 108,  83,  53,  82,  51,  76,  50,  77, 121,  67,  43,  56,  48,  73,  73,  84,
	  83, 109,  85,  56, 112,  65,  73, 115,  87,  97,  74, 103, 121,  67,  79, 113,
	  73,  79, 102,  81,  99,  54, 117, 103,  69,  84,  73,  85,  78, 117, 115, 106,
	  54, 111, 108,  75, 109, 105, 118, 115,  10,  81, 110,  99, 111,  57,  97, 107,
	  88,  85,  84,  69,  50, 106,  53,  88, 118,  82,  97,  65, 111,  67,  89,  88,
	  90,  43,  56, 107,  73,  89,  43,  74,  74,  76,  88,  73,  43,  43,  82,  50,
	  83,  43, 122,  67, 107,  52,  54,  98, 101, 118, 118,  85,  67,  65, 119,  69,
	  65,  65,  97,  78,  81,  77,  69,  52, 119,  10,  72,  81,  89,  68,  86,  82,
	  48,  79,  66,  66,  89,  69,  70,  68, 114,  84,  65, 107, 109, 100,  99,  50,
	 110,  75, 102,  68,  89,  79,  76,  50,  86,  80, 119,  56, 106,  81, 114, 110,
	  87,  90,  77,  66,  56,  71,  65,  49,  85, 100,  73, 119,  81,  89,  77,  66,
	  97,  65,  70,  68, 114,  84,  65, 107, 109, 100,  10,  99,  50, 110,  75, 102,
	  68,  89,  79,  76,  50,  86,  80, 119,  56, 106,  81, 114, 110,  87,  90,  77,
	  65, 119,  71,  65,  49,  85, 100,  69, 119,  81,  70,  77,  65,  77,  66,  65,
	 102,  56, 119,  68,  81,  89,  74,  75, 111,  90,  73, 104, 118,  99,  78,  65,
	  81,  69,  76,  66,  81,  65,  68, 103, 103,  69,  66,  10,  65,  69,  73, 101,
	  88,  53,  79,  77,  73, 106,  49, 113,  68,  65,  68,  54, 115,  54,  88,  79,
	 107,  89,  69,  86,  54,  67,  57, 114,  65,  72, 107, 113,  87,  47,  83, 107,
	 100,  52, 121, 108,  47,  68,  89,  54,  84,  74,  49,  81,  97,  80,  82, 107,
	  71,  50,  82,  97,  50,  90,  99,  57,  67,  53,  54, 109,  10,  53, 120,  84,
	 107,  52, 112, 109,  87, 114,  53,  43,  55, 121,  70, 108,  73,  81, 106,  90,
	  83,  98,  79,  86,  70,  48, 102, 105, 116,  74,  75,  67, 120,  65, 109, 106,
	 102,  77,  71,  47, 119,  67, 108, 110,  90,  79,  80,  71,  81,  67, 122,  68,
	  54, 108,  88,  65, 113,  81, 110,  65,  54, 101, 116,  67,  85,  10,  75,  69,
	 103,  57,  54,  77,  88,  86,  76, 122,  50,  55, 119, 114,  69,  68,  90,  77,
	  49,  51,  56,  74,  77,  69,  86,  49,  66,  69, 118, 119,  77, 116,  69,  88,
	  49,  85, 117,  51,  49, 118,  48, 101,  51, 114,  54, 118,  90, 108,  74,  88,
	  65,  75,  65, 119,  57,  78,  83, 122, 111,  98, 105, 113,  52, 117,  10, 108,
	 105,  67, 100, 115,  79,  80,  85, 104,  86,  87,  89, 105,  48,  82, 117, 107,
	  76,  73,  71, 116,  55, 112,  56,  98, 102,  82, 100,  71,  55,  56,  83, 118,
	 119,  82,  74,  75, 113,  57, 110,  72, 105, 103,  48, 114, 105, 109, 118, 106,
	  81,  87,  53, 110, 108,  75,  78,  65,  52,  72,  71, 121,  97,  77, 120,  10,
	 122, 108, 100,  43,  87, 118,  75,  66,  86,  49, 111, 100,  67, 114,  70,  97,
	 103,  68,  82, 121,  57,  52,  78, 107, 117,  76, 100,  98,  67, 108,  74, 119,
	  57,  47,  66,  83,  68, 102,  76, 115,  73,  85,  86,  81,  52,  50, 105, 112,
	  53,  43, 101,  99, 104,  86, 116,  52,  79,  57,  77,  56, 114, 111,  50,  66,
	  10,  72, 103,  82,  75,  88,  52,  86, 118,  53,  57, 113, 112,  90,  49, 119,
	  82,  55, 108, 110, 106,  70,  84,  52,  61,  10,  45,  45,  45,  45,  45,  69,
	  78,  68,  32,  67,  69,  82,  84,  73,  70,  73,  67,  65,  84,  69,  45,  45,
	  45,  45,  45,  10,
	   0
};

/* certs/samples/self.key */
static uchar p5[] = {
	  45,  45,  45,  45,  45,  66,  69,  71,  73,  78,  32,  82,  83,  65,  32,  80,
	  82,  73,  86,  65,  84,  69,  32,  75,  69,  89,  45,  45,  45,  45,  45,  10,
	  77,  73,  73,  69, 112,  65,  73,  66,  65,  65,  75,  67,  65,  81,  69,  65,
	 114,  90,  81, 122,  52, 108, 111,  97,  65,  82,  54,  48,  57,  53, 111, 106,
	 119, 107,  98, 104,  66, 118,  76,  75,  47,  81,  76,  78, 102,  79, 107, 120,
	  54,  50,  77, 114,  84,  71,  79,  43,  76, 110,  56,  82, 112,  70,  88,  74,
	  10,  67,  67,  43, 120,  81,  79,  53, 105,  76,  75,  49, 108,  76,  83,  71,
	 119,  85, 114,  84,  78,  47,  82,  97,  43, 117,  54,  98,  54,  90, 107,  51,
	  68,  72, 116,  88,  78, 116,  51,  84,  66,  74,  77,  68,  47,  86,  73, 116,
	  43,  75, 118,  79,  90,  78, 109, 103, 113, 118, 117,  69,  70,  75,  87, 120,
	 100,  10,  81,  78, 107,  47, 114,  90,  82,  83, 112, 100,  53, 117,  90,  47,
	  88, 117, 105,  98,  56,  66,  83,  71,  74,  76,  55, 101,  73,  56,  78,  82,
	 106, 107,  99,  53,  68,  69,  79,  74, 122, 114, 107,  47,  56, 111, 105,  57,
	  54,  55,  43, 107,  89,  89, 107, 109, 113, 117, 111,  65,  56, 118,  85, 122,
	  54,  83,  10, 109,  47,  99,  85, 113, 111, 103, 110,  77,  82,  86, 100, 122,
	  53,  69,  73,  74, 116,  78,  48,  73,  81,  43,  56, 122,  43,  54,  77,  52,
	 108,  98,  70, 114, 102, 105,  52,  89,  72,  56,  86,  43,  66,  73,  47,  79,
	  87,  75, 111, 116, 111,  71, 116, 104,  75,  87, 116, 113,  69,  50,  86,  76,
	 108,  72,  99,  10, 118,  89, 122,  73,  76,  55, 122,  81, 103, 104,  78,  75,
	  90,  84, 121, 107,  65, 105, 120,  90, 111, 109,  68,  73,  73,  54, 111, 103,
	  53,  57,  66, 122, 113,  54,  65,  82,  77, 104,  81,  50,  54, 121,  80, 113,
	 105,  85, 113,  97,  75,  43, 120,  67, 100, 121, 106,  49, 113,  82, 100,  82,
	  77,  84,  97,  80,  10, 108, 101,  57,  70, 111,  67, 103,  74, 104, 100, 110,
	  55, 121,  81, 104, 106,  52, 107, 107, 116,  99, 106,  55,  53,  72,  90,  76,
	  55,  77,  75,  84, 106, 112, 116,  54,  43,  57,  81,  73,  68,  65,  81,  65,
	  66,  65, 111,  73,  66,  65,  66,  76, 100, 110,  56,  73,  68,  84, 118,  52,
	  43,  75, 121,  99,  97,  10,  76, 114,  98,  72,  52,  49,  57,  81,  57, 106,
	  85,  72, 110,  47,  80, 113, 109,  73,  49, 102,  54, 117,  51,  84, 106,  88,
	  69,  97,  83,  72,  50,  88,  65,  86,  71,  78, 100,  88,  78,  56,  78,  86,
	 108, 114, 119, 121, 112, 106, 108, 102, 108,  51,  81,  48, 103,  49, 116,  54,
	 100,  72, 110,  56,  71,  77,  10, 120,  66,  57,  79,  49,  53, 119,  80,  54,
	  89, 111, 111,  71,  57, 104,  66, 106,  81,  88,  52,  69, 102, 102, 121,  51,
	  71, 113,  65, 112,  73,  72,  55,  69,  67, 117, 106,  77,  83,  79,  90, 107,
	 108,  52,  75, 122, 101,  56, 111, 115,  76,  78, 120,  43, 104,  54,  50,  54,
	  69,  47,  68, 110, 103,  50,  98,  10,  86, 103,  69, 108, 103,  57,  52, 118,
	  86,  47,  57, 117,  53,  83,  90,  71, 111,  67,  71,  48, 105, 113, 107, 110,
	  82,  84,  71,  75,  74, 119, 111,  74,  54,  56, 117,  98, 114, 119, 107,  97,
	 109, 102,  48,  56,  72,  47,  82,  67, 107,  73,  69,  98,  97,  78,  88, 118,
	  79,  79,  73,  52, 106, 110, 117,  89,  10, 108,  99, 119,  65, 102,  56,  77,
	  51, 121,  75,  83,  54, 122,  54, 100,  76,  99,  52, 114,  78, 119, 117,  90,
	  98,  48, 103,  79, 116,  57,  49,  80,  90, 108, 108, 118, 113, 120, 106,  54,
	  97,  70,  84, 113, 101, 105, 117, 112, 101,  47,  81, 109,  49,  78,  99, 120,
	  72,  98,  47, 121,  65,  98,  83,  50,  48,  10,  88, 110,  97,  68,  43,  89,
	  50,  49,  84,  47, 109, 104,  98,  76,  86,  52, 101, 113,  55,  87,  47,  74,
	 111,  76, 112,  85, 118, 121, 121,  49,  83, 107,  57,  89,  65,  50,  86, 117,
	 120,  51,  54,  98, 116, 109,  54, 109,  80,  76,  80, 100, 112,  65,  78, 118,
	  67,  55,  65,  49,  77,  89,  66,  55,  74,  89,  10,  82,  54,  89,  99, 120,
	  43,  69,  67, 103,  89,  69,  65,  52,  90,  86,  49,  82,  52,  53, 119, 111,
	  65,  76,  69,  67,  79,  85,  65,  76, 102, 119, 106,  51,  84,  50,  67, 100,
	  88, 116,  56,  98, 114,  65,  52,  55,  75, 121,  52, 103, 122,  82,  87, 120,
	  72,  48,  73,  76, 107,  85,  99, 112,  68, 109, 113,  10, 104,  86,  66,  74,
	  78, 119, 114,  83, 101, 103, 122,  80,  89, 115,  97, 108, 118,  89,  50,  65,
	 107,  75,  53,  70,  57,  88,  67,  86,  80,  48, 111, 113,  89,  68, 102, 105,
	 101,  74, 101,  67,  82, 106,  54, 121, 120,  80,  89,  77,  82, 107,  97,  77,
	  70,  57,  66, 116,  55,  85,  99,  79,  65,  53,  89,  56,  10, 105, 108, 120,
	  71,  90,  47,  70,  79,  97,  70,  48,  85, 122,  51, 112, 119,  57, 109,  70,
	  85, 119,  89, 100,  57, 101,  78,  80, 108,  53,  75, 107, 117,  78,  77, 112,
	  56,  84,  99,  65,  55,  83,  79,  73,  51, 114,  52,  55,  77,  82,  50,  76,
	  65, 101,  57, 107,  67, 103,  89,  69,  65, 120,  80, 117, 116,  10,  72,  48,
	 117,  66,  72,  81, 113, 100,  74,  47, 105,  78,  53, 118,  89,  55, 121,  50,
	  53,  77,  69, 114,  66,  97,  57,  84, 113,  66,  80,  71,  57,  77, 111,  69,
	  72,  88,  51, 118, 116,  80,  70, 114,  67, 107, 108,  85, 102,  89,  48,  69,
	  76,  89,  81, 121,  89,  57,  73,  53, 105, 101,  85,  55,  69, 105,  10,  57,
	  55,  85, 103,  52,  70,  43,  48, 100, 110, 117, 122,  43,  69,  47, 120,  81,
	 100, 119,  47,  53,  57,  70,  73,  56,  54,  54, 117, 111,  78,  89, 108,  69,
	  76, 103,  77,  76,  68,  89,  57,  76,  76,  57,  81,  83, 116,  83,  74,  65,
	  98,  89,  83,  68,  89,  85,  55,  51,  87, 118, 114,  48,  89,  54,  67,  10,
	  80, 101, 108,  49, 102, 106,  87,  73,  75, 100, 119,  87, 100,  78,  83,  74,
	  70,  73,  48,  50, 114,  73,  52,  75,  84,  75,  78,  50,  79,  78,  74,  53,
	  81, 120,  88,  71, 116, 110,  48,  67, 103,  89,  69,  65, 119, 107,  72, 109,
	  81,  76, 121,  85, 108,  47,  90,  74, 119, 105,  56,  82, 118,  73, 102,  73,
	  10, 105, 112, 105,  82,  89,  52,  72,  51,  89,  89, 111,  51,  79, 118, 104,
	 120,  50,  43, 113,  83,  88,  90, 111, 101,  70,  86, 112,  71, 107,  57,  86,
	 107,  49, 110,  67,  81,  50,  43, 108, 104,  51, 117,  66, 120,  74, 109, 112,
	  83,  66,  54,  49,  50,  55,  83,  66, 108,  57, 106, 111,  66,  68,  65,  78,
	 101,  10,  65, 111,  87, 100,  98,  75,  78, 114,  57,  47, 109,  43,  69,  73,
	  97, 104, 103,  49,  76,  49,  67,  80, 122, 102,  55,  78, 105, 110,  90,  77,
	 100,  88,  85, 118,  54, 114,  85, 111,  56, 109,  75, 104,  69, 100,  47, 112,
	  48,  57,  56,  97,  49,  88,  81,  50,  68, 100,  49,  57,  77,  66, 119,  97,
	 115,  43,  10, 115, 119,  56,  55, 103,  43, 106, 113,  74,  74,  86,  74, 120,
	 116,  66,  57,  99,  87, 118,  69, 118, 101,  69,  67, 103,  89,  66,  99, 122,
	  47,  83, 110,  80,  83,  48, 116, 111,  78,  87, 104, 103, 114,  82, 104, 115,
	  88,  49,  70,  65,  99, 103, 111,  79, 104,  51, 120,  85,  56, 113,  87, 122,
	 104,  52,  57,  10, 109,  67, 104,  57,  99,  77,  50,  51,  57,  67, 122, 111,
	  66,  88,  48,  55, 121,  51,  77,  89,  87,  73, 107,  81,  72,  53,  88, 104,
	  43, 121,  70, 116, 115,  49,  77,  49,  82,  81,  76,  68, 110,  77,  43,  85,
	  88,  82,  53, 103, 109,  77,  85, 110, 108, 101,  50,  89, 109,  54,  75,  43,
	  78,  52, 103, 112,  10,  85,  51,  54, 119, 121, 106,  71,  83, 120, 117,  87,
	 118,  68,  71, 115, 116, 101, 106, 115,  82,  85,  99, 104,  43,  77,  74,  53,
	 104,  69,  53,  78,  56,  86, 109,  97,  56, 117,  55,  53,  88,  99, 116,  76,
	  70,  52,  50, 112, 102,  87,  53,  88,  54,  81,  99,  98,  75, 105,  50,  53,
	 108, 111,  89,  51, 118,  10,  78,  80,  66,  51,  66,  81,  75,  66, 103,  81,
	  68, 101, 101,  85,  72, 110,  55, 118,  67,  82, 109,  52,  97,  84, 101,  89,
	  47,  68,  80, 107,  73,  53, 117,  97,  77, 111, 100,  90, 107,  80, 106, 118,
	  71,  90, 117,  56, 112,  66, 111,  87,  87,  76, 110, 109, 117, 120,  72, 115,
	  89, 114, 102,  74, 119,  48,  10, 106, 114,  90,  97,  49, 106, 115, 107, 110,
	 120, 104,  49, 111,  67,  53, 117,  83, 108,  70,  69,  55, 120,  82,  99, 121,
	 121,  53,  71, 114,  47,  71, 118,  74, 100,  97,  74,  57, 109, 113, 113, 110,
	 115,  88, 115,  97,  47, 118, 113,  89,  85,  69,  89,  71,  75, 112, 105,  84,
	 121,  77,  98,  49, 119,  54, 102,  10,  65,  80, 106,  43,  54,  50,  51,  99,
	  75,  47,  53,  47,  50,  80, 120,  53, 100,  66,  49, 112, 101, 112,  65,  54,
	 121,  65,  66, 112,  98,  51,  74, 111,  47, 108,  73, 105,  65, 104,  67,  65,
	 118,  68,  80, 114,  99,  80,  56,  71, 103, 109,  74,  50, 114, 119,  61,  61,
	  10,  45,  45,  45,  45,  45,  69,  78,  68,  32,  82,  83,  65,  32,  80,  82,
	  73,  86,  65,  84,  69,  32,  75,  69,  89,  45,  45,  45,  45,  45,  10,
	   0
};

WebsRomIndex websRomIndex[] = {
	{ "/web", 0, 0 },
	{ "/web/favicon.ico", p0, 2999 },
	{ "/web/index.html", p1, 155 },
	{ "/auth.txt", p2, 444 },
	{ "/route.txt", p3, 1829 },
	{ "/certs/samples/self.crt", p4, 1444 },
	{ "/certs/samples/self.key", p5, 1679 },
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
        if (strncmp(wp->path, route->prefix, len) != 0) {
            continue;
        }
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
    if (!route->handler) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Configuration error - no handler for route");
        return 1;
    }
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
    if (!route->handler->service) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Configuration error - no handler service callback");
        return 1;
    }
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
        wfree(route->prefix);
        wfree(route);
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


PUBLIC int websOpenRoute()
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
    int         i;

    if (handlers >= 0) {
        for (key = hashFirst(handlers); key; key = hashNext(handlers, key)) {
            handler = key->content.value.symbol;
            if (handler->close) {
                (*handler->close)();
            }
            wfree(handler->name);
            wfree(handler);
        }
        hashFree(handlers);
        handlers = -1;
    }
    if (routes) {
        for (i = 0; i < routeCount; i++) {
            freeRoute(routes[i]);
        }
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


int websRunEvents()
{
    Callback    *s;
    WebsTime    now;
    int         i, delay, nextEvent;

    nextEvent = (MAXINT / 1000);
    now = time(0);

    for (i = 0; i < callbackMax; i++) {
        if ((s = callbacks[i]) != NULL) {
            if (s->at <= now) {
                callEvent(i);
                delay = MAXINT / 1000;
                /* Rescan incase event scheduled or modified an event */
                i = -1;
            } else {
                delay = (int) min(s->at - now, MAXINT);
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
                    wfree(safe);
                } else
#endif
                {
                    safe = websEscapeHtml(va_arg(args, char*));
                    outString(&fmt, safe, -1);
                    wfree(safe);
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
        /* This defintion comes from main.me goahead.logfile */
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
        memmove(buf, cp, end - cp);
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


PUBLIC ssize bufPut(WebsBuf *bp, char *fmt, ...)
{
    va_list     ap;
    char        *str;
    ssize       rc;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (fmt == 0) {
        return 0;
    }
    va_start(ap, fmt);
    str = sfmtv(fmt, ap);
    va_end(ap);

    rc = bufPutBlk(bp, str, strlen(str) * sizeof(char));
    *((char*) bp->endp) = (char) '\0';
    return rc;
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
    bp->buf = newbuf;
    bp->endbuf = &bp->buf[bp->buflen];
    bp->servp = newbuf;
    bp->endp = &newbuf[len];
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
    HashTable   *tp;

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
    if ((tp->hash_table = (WebsKey**) walloc(tp->size * sizeof(WebsKey*))) == 0) {
        wfreeHandle(&sym, sd);
        wfree(tp);
        return -1;
    }
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
    HashTable   *tp;
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
    HashTable   *tp;
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
    HashTable   *tp;
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
    HashTable   *tp;
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


void *hashLookupSymbol(WebsHash sd, char *name)
{
    WebsKey     *kp;

    if ((kp = hashLookup(sd, name)) == 0) {
        return 0;
    }
    return kp->content.value.symbol;
}


/*
    Enter a symbol into the table. If already there, update its value.  Always succeeds if memory available. We allocate
    a copy of "name" here so it can be a volatile variable. The value "v" is just a copy of the passed in value, so it
    MUST be persistent.
 */
WebsKey *hashEnter(WebsHash sd, char *name, WebsValue v, int arg)
{
    HashTable   *tp;
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
        if ((sp = (WebsKey*) walloc(sizeof(WebsKey))) == 0) {
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
        if ((sp = (WebsKey*) walloc(sizeof(WebsKey))) == 0) {
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
    HashTable   *tp;
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


#if DEPRECATE || 1
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
#endif


/*
    Convert a hex string to an integer
 */
uint hextoi(char *hexstring)
{
    char      *h;
    uint        c, v;

    if (!hexstring) {
        return 0;
    }
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
    if ((buf = walloc(strlen(s) + 1)) != 0) {
        strcpy(buf, s);
    }
    return buf;
}


PUBLIC bool snumber(cchar *s)
{
    if (!s) {
        return 0;
    }
    if (*s == '-' || *s == '+') {
        s++;
    }
    return s && *s && strspn(s, "1234567890") == strlen(s);
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
    if (s1 == 0) {
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

    if (s1 == 0) {
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

PUBLIC WebsSocket   **socketList;           /* List of open sockets */
PUBLIC int          socketMax;              /* Maximum size of socket */
PUBLIC Socket       socketHighestFd = -1;   /* Highest socket fd opened */
PUBLIC int          socketOpenCount = 0;    /* Number of task using sockets */

static int          hasIPv6;                /* System supports IPv6 */

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
#if defined(SO_REUSEPORT) && KEEP
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
PUBLIC int socketSelect(int sid, int timeout)
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
        if (sp->handlerMask & SOCKET_READABLE) {
            FD_SET(sp->sock, &readFds);
            nEvents++;
        }
        if (sp->handlerMask & SOCKET_WRITABLE) {
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
        Wait for the event or a timeout
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
            nEvents++;
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


PUBLIC int socketSelect(int sid, int timeout)
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
        }
        if (sp->handlerMask & SOCKET_WRITABLE) {
            writeFds[index] |= bit;
        }
        if (sp->handlerMask & SOCKET_EXCEPTION) {
            exceptFds[index] |= bit;
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
        Wait for the event or a timeout
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
                if (sofar) {
                    /*
                        If some data was written, we mask the EAGAIN for this time. Caller should recall and then
                        will get a negative return code with EAGAIN.
                     */
                    return sofar;
                }
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


PUBLIC void socketSetError(int error)
{
#if ME_WIN_LIKE
    SetLastError(error);
#elif ME_UNIX_LIKE || VXWORKS
    errno = error;
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

    if (ip && *ip) {
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
#if (ME_UNIX_LIKE || ME_WIN_LIKE)
    char    service[NI_MAXSERV];

#if ME_WIN_LIKE || defined(IN6_IS_ADDR_V4MAPPED)
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


PUBLIC WebsSocket **socketGetList()
{
    return socketList;
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



/********* Start of file ../../../src/time.c ************/


/**
    time.c - Date and Time handling

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/********************************** Defines ***********************************/

#define SEC_PER_MIN  (60)
#define SEC_PER_HOUR (60 * 60)
#define SEC_PER_DAY  (86400)
#define SEC_PER_YEAR (INT64(31556952))

/*
    Token types or'd into the TimeToken value
 */
#define TOKEN_DAY       0x01000000
#define TOKEN_MONTH     0x02000000
#define TOKEN_ZONE      0x04000000
#define TOKEN_OFFSET    0x08000000
#define TOKEN_MASK      0xFF000000

typedef struct TimeToken {
    char    *name;
    int     value;
} TimeToken;

static WebsHash timeTokens = -1;

static TimeToken days[] = {
    { "sun",  0 | TOKEN_DAY },
    { "mon",  1 | TOKEN_DAY },
    { "tue",  2 | TOKEN_DAY },
    { "wed",  3 | TOKEN_DAY },
    { "thu",  4 | TOKEN_DAY },
    { "fri",  5 | TOKEN_DAY },
    { "sat",  6 | TOKEN_DAY },
    { 0, 0 },
};

static TimeToken fullDays[] = {
    { "sunday",     0 | TOKEN_DAY },
    { "monday",     1 | TOKEN_DAY },
    { "tuesday",    2 | TOKEN_DAY },
    { "wednesday",  3 | TOKEN_DAY },
    { "thursday",   4 | TOKEN_DAY },
    { "friday",     5 | TOKEN_DAY },
    { "saturday",   6 | TOKEN_DAY },
    { 0, 0 },
};

/*
    Make origin 1 to correspond to user date entries 10/28/2014
 */
static TimeToken months[] = {
    { "jan",  1 | TOKEN_MONTH },
    { "feb",  2 | TOKEN_MONTH },
    { "mar",  3 | TOKEN_MONTH },
    { "apr",  4 | TOKEN_MONTH },
    { "may",  5 | TOKEN_MONTH },
    { "jun",  6 | TOKEN_MONTH },
    { "jul",  7 | TOKEN_MONTH },
    { "aug",  8 | TOKEN_MONTH },
    { "sep",  9 | TOKEN_MONTH },
    { "oct", 10 | TOKEN_MONTH },
    { "nov", 11 | TOKEN_MONTH },
    { "dec", 12 | TOKEN_MONTH },
    { 0, 0 },
};

static TimeToken fullMonths[] = {
    { "january",    1 | TOKEN_MONTH },
    { "february",   2 | TOKEN_MONTH },
    { "march",      3 | TOKEN_MONTH },
    { "april",      4 | TOKEN_MONTH },
    { "may",        5 | TOKEN_MONTH },
    { "june",       6 | TOKEN_MONTH },
    { "july",       7 | TOKEN_MONTH },
    { "august",     8 | TOKEN_MONTH },
    { "september",  9 | TOKEN_MONTH },
    { "october",   10 | TOKEN_MONTH },
    { "november",  11 | TOKEN_MONTH },
    { "december",  12 | TOKEN_MONTH },
    { 0, 0 }
};

static TimeToken ampm[] = {
    { "am", 0 | TOKEN_OFFSET },
    { "pm", (12 * 3600) | TOKEN_OFFSET },
    { 0, 0 },
};


static TimeToken zones[] = {
    { "ut",      0 | TOKEN_ZONE},
    { "utc",     0 | TOKEN_ZONE},
    { "gmt",     0 | TOKEN_ZONE},
    { "edt",  -240 | TOKEN_ZONE},
    { "est",  -300 | TOKEN_ZONE},
    { "cdt",  -300 | TOKEN_ZONE},
    { "cst",  -360 | TOKEN_ZONE},
    { "mdt",  -360 | TOKEN_ZONE},
    { "mst",  -420 | TOKEN_ZONE},
    { "pdt",  -420 | TOKEN_ZONE},
    { "pst",  -480 | TOKEN_ZONE},
    { 0, 0 },
};


static TimeToken offsets[] = {
    { "tomorrow",    86400 | TOKEN_OFFSET},
    { "yesterday",  -86400 | TOKEN_OFFSET},
    { "next week",   (86400 * 7) | TOKEN_OFFSET},
    { "last week",  -(86400 * 7) | TOKEN_OFFSET},
    { 0, 0 },
};

static int timeSep = ':';

static int normalMonthStart[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 0,
};
static int leapMonthStart[] = {
    0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 0
};

static int leapYear(int year);
static void validateTime(struct tm *tm, struct tm *defaults);

/************************************ Code ************************************/

PUBLIC int websTimeOpen()
{
    TimeToken           *tt;

    timeTokens = hashCreate(59);
    for (tt = days; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = fullDays; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = months; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = fullMonths; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = ampm; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = zones; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = offsets; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    return 0;
}


PUBLIC void websTimeClose()
{
    if (timeTokens >= 0) {
        hashFree(timeTokens);
        timeTokens = -1;
    }
}


static int leapYear(int year)
{
    if (year % 4) {
        return 0;
    } else if (year % 400 == 0) {
        return 1;
    } else if (year % 100 == 0) {
        return 0;
    }
    return 1;
}


static int daysSinceEpoch(int year)
{
    int     days;

    days = 365 * (year - 1970);
    days += ((year-1) / 4) - (1970 / 4);
    days -= ((year-1) / 100) - (1970 / 100);
    days += ((year-1) / 400) - (1970 / 400);
    return days;
}


static WebsTime makeTime(struct tm *tp)
{
    int     days, year, month;

    year = tp->tm_year + 1900 + tp->tm_mon / 12;
    month = tp->tm_mon % 12;
    if (month < 0) {
        month += 12;
        --year;
    }
    days = daysSinceEpoch(year);
    days += leapYear(year) ? leapMonthStart[month] : normalMonthStart[month];
    days += tp->tm_mday - 1;
    return (days * SEC_PER_DAY) + ((((((tp->tm_hour * 60)) + tp->tm_min) * 60) + tp->tm_sec));
}


static int lookupSym(char *token, int kind)
{
    TimeToken   *tt;

    if ((tt = (TimeToken*) hashLookupSymbol(timeTokens, token)) == 0) {
        return -1;
    }
    if (kind != (tt->value & TOKEN_MASK)) {
        return -1;
    }
    return tt->value & ~TOKEN_MASK;
}


static int getNum(char **token, int sep)
{
    int     num;

    if (*token == 0) {
        return 0;
    }

    num = atoi(*token);
    *token = strchr(*token, sep);
    if (*token) {
        *token += 1;
    }
    return num;
}


static int getNumOrSym(char **token, int sep, int kind, int *isAlpah)
{
    char    *cp;
    int     num;

    assert(token && *token);

    if (*token == 0) {
        return 0;
    }
    if (isalpha((uchar) **token)) {
        *isAlpah = 1;
        cp = strchr(*token, sep);
        if (cp) {
            *cp++ = '\0';
        }
        num = lookupSym(*token, kind);
        *token = cp;
        return num;
    }
    num = atoi(*token);
    *token = strchr(*token, sep);
    if (*token) {
        *token += 1;
    }
    *isAlpah = 0;
    return num;
}


static void swapDayMonth(struct tm *tp)
{
    int     tmp;

    tmp = tp->tm_mday;
    tp->tm_mday = tp->tm_mon;
    tp->tm_mon = tmp;
}


/*
    Parse the a date/time string and return the result in *time. Missing date items may be provided
    via the defaults argument. This is a tolerant parser. It is not validating and will do its best
    to parse any possible date string.
 */
PUBLIC int websParseDateTime(WebsTime *time, char *dateString, struct tm *defaults)
{
    TimeToken       *tt;
    struct tm       tm;
    char            *str, *next, *token, *cp, *sep;
    int64           value;
    int             kind, hour, min, negate, value1, value2, value3, alpha, alpha2, alpha3;
    int             dateSep, offset, zoneOffset, fullYear;

    if (!dateString) {
        dateString = "";
    }
    offset = 0;
    zoneOffset = 0;
    sep = ", \t";
    cp = 0;
    next = 0;
    fullYear = 0;

    /*
        Set these mandatory values to -1 so we can tell if they are set to valid values
        WARNING: all the calculations use tm_year with origin 0, not 1900. It is fixed up below.
     */
    tm.tm_year = -MAXINT;
    tm.tm_mon = tm.tm_mday = tm.tm_hour = tm.tm_sec = tm.tm_min = tm.tm_wday = -1;
    tm.tm_min = tm.tm_sec = tm.tm_yday = -1;

#if ME_UNIX_LIKE && !CYGWIN
    tm.tm_gmtoff = 0;
    tm.tm_zone = 0;
#endif

    /*
        Set to -1 to try to determine if DST is in effect
     */
    tm.tm_isdst = -1;
    str = slower(dateString);

    /*
        Handle ISO dates: "2009-05-21t16:06:05.000z
     */
    if (strchr(str, ' ') == 0 && strchr(str, '-') && str[slen(str) - 1] == 'z') {
        for (cp = str; *cp; cp++) {
            if (*cp == '-') {
                *cp = '/';
            } else if (*cp == 't' && cp > str && isdigit((uchar) cp[-1]) && isdigit((uchar) cp[1]) ) {
                *cp = ' ';
            }
        }
    }
    token = stok(str, sep, &next);

    while (token && *token) {
        if (snumber(token)) {
            /*
                Parse either day of month or year. Priority to day of month. Format: <29> Jan <15> <2014>
             */
            value = atoi(token);
            if (value > 3000) {
                *time = value;
                return 0;
            } else if (value > 32 || (tm.tm_mday >= 0 && tm.tm_year == -MAXINT)) {
                if (value >= 1000) {
                    fullYear = 1;
                }
                tm.tm_year = (int) value - 1900;
            } else if (tm.tm_mday < 0) {
                tm.tm_mday = (int) value;
            }

        } else if ((*token == '+') || (*token == '-') ||
                ((strncmp(token, "gmt", 3) == 0 || strncmp(token, "utc", 3) == 0) &&
                ((cp = strchr(&token[3], '+')) != 0 || (cp = strchr(&token[3], '-')) != 0))) {
            /*
                Timezone. Format: [GMT|UTC][+-]NN[:]NN
             */
            if (!isalpha((uchar) *token)) {
                cp = token;
            }
            negate = *cp == '-' ? -1 : 1;
            cp++;
            hour = getNum(&cp, timeSep);
            if (hour >= 100) {
                hour /= 100;
            }
            min = getNum(&cp, timeSep);
            zoneOffset = negate * (hour * 60 + min);

        } else if (isalpha((uchar) *token)) {
            if ((tt = (TimeToken*) hashLookupSymbol(timeTokens, token)) != 0) {
                kind = tt->value & TOKEN_MASK;
                value = tt->value & ~TOKEN_MASK;
                switch (kind) {

                case TOKEN_DAY:
                    tm.tm_wday = (int) value;
                    break;

                case TOKEN_MONTH:
                    tm.tm_mon = (int) value;
                    break;

                case TOKEN_OFFSET:
                    /* Named timezones or symbolic names like: tomorrow, yesterday, next week ... */
                    /* Units are seconds */
                    offset += (int) value;
                    break;

                case TOKEN_ZONE:
                    zoneOffset = (int) value;
                    break;

                default:
                    /* Just ignore unknown values */
                    break;
                }
            }

        } else if ((cp = strchr(token, timeSep)) != 0 && isdigit((uchar) token[0])) {
            /*
                Time:  10:52[:23]
                Must not parse GMT-07:30
             */
            tm.tm_hour = getNum(&token, timeSep);
            tm.tm_min = getNum(&token, timeSep);
            tm.tm_sec = getNum(&token, timeSep);

        } else {
            dateSep = '/';
            if (strchr(token, dateSep) == 0) {
                dateSep = '-';
                if (strchr(token, dateSep) == 0) {
                    dateSep = '.';
                    if (strchr(token, dateSep) == 0) {
                        dateSep = 0;
                    }
                }
            }
            if (dateSep) {
                /*
                    Date:  07/28/2014, 07/28/08, Jan/28/2014, Jaunuary-28-2014, 28-jan-2014
                    Support order: dd/mm/yy, mm/dd/yy and yyyy/mm/dd
                    Support separators "/", ".", "-"
                 */
                value1 = getNumOrSym(&token, dateSep, TOKEN_MONTH, &alpha);
                value2 = getNumOrSym(&token, dateSep, TOKEN_MONTH, &alpha2);
                value3 = getNumOrSym(&token, dateSep, TOKEN_MONTH, &alpha3);

                if (value1 > 31) {
                    /* yy/mm/dd */
                    tm.tm_year = value1;
                    tm.tm_mon = value2;
                    tm.tm_mday = value3;

                } else if (value1 > 12 || alpha2) {
                    /*
                        dd/mm/yy
                        Cannot detect 01/02/03  This will be evaluated as Jan 2 2003 below.
                     */
                    tm.tm_mday = value1;
                    tm.tm_mon = value2;
                    tm.tm_year = value3;

                } else {
                    /*
                        The default to parse is mm/dd/yy unless the mm value is out of range
                     */
                    tm.tm_mon = value1;
                    tm.tm_mday = value2;
                    tm.tm_year = value3;
                }
            }
        }
        token = stok(NULL, sep, &next);
    }

    /*
        Y2K fix and rebias
     */
    if (0 <= tm.tm_year && tm.tm_year < 100 && !fullYear) {
        if (tm.tm_year < 50) {
            tm.tm_year += 2000;
        } else {
            tm.tm_year += 1900;
        }
    }
    if (tm.tm_year >= 1900) {
        tm.tm_year -= 1900;
    }

    /*
        Convert back to origin 0 for months
     */
    if (tm.tm_mon > 0) {
        tm.tm_mon--;
    }

    /*
        Validate and fill in missing items with defaults
     */
    validateTime(&tm, defaults);
    *time = makeTime(&tm);
    *time += -(zoneOffset * SEC_PER_MIN);
    *time += offset;
    return 0;
}


static void validateTime(struct tm *tp, struct tm *defaults)
{
    struct tm   empty;

    /*
        Fix apparent day-mon-year ordering issues. Cannot fix everything!
     */
    if ((12 <= tp->tm_mon && tp->tm_mon <= 31) && 0 <= tp->tm_mday && tp->tm_mday <= 11) {
        /*
            Looks like day month are swapped
         */
        swapDayMonth(tp);
    }

    if (tp->tm_year != -MAXINT && tp->tm_mon >= 0 && tp->tm_mday >= 0 && tp->tm_hour >= 0) {
        /*  Everything defined */
        return;
    }

    /*
        Use empty time if missing
     */
    if (defaults == NULL) {
        memset(&empty, 0, sizeof(empty));
        defaults = &empty;
        empty.tm_mday = 1;
        empty.tm_year = 70;
    }
    if (tp->tm_hour < 0 && tp->tm_min < 0 && tp->tm_sec < 0) {
        tp->tm_hour = defaults->tm_hour;
        tp->tm_min = defaults->tm_min;
        tp->tm_sec = defaults->tm_sec;
    }

    /*
        Get weekday, if before today then make next week
     */
    if (tp->tm_wday >= 0 && tp->tm_year == -MAXINT && tp->tm_mon < 0 && tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday + (tp->tm_wday - defaults->tm_wday + 7) % 7;
        tp->tm_mon = defaults->tm_mon;
        tp->tm_year = defaults->tm_year;
    }

    /*
        Get month, if before this month then make next year
     */
    if (tp->tm_mon >= 0 && tp->tm_mon <= 11 && tp->tm_mday < 0) {
        if (tp->tm_year == -MAXINT) {
            tp->tm_year = defaults->tm_year + (((tp->tm_mon - defaults->tm_mon) < 0) ? 1 : 0);
        }
        tp->tm_mday = defaults->tm_mday;
    }

    /*
        Get date, if before current time then make tomorrow
     */
    if (tp->tm_hour >= 0 && tp->tm_year == -MAXINT && tp->tm_mon < 0 && tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday + ((tp->tm_hour - defaults->tm_hour) < 0 ? 1 : 0);
        tp->tm_mon = defaults->tm_mon;
        tp->tm_year = defaults->tm_year;
    }
    if (tp->tm_year == -MAXINT) {
        tp->tm_year = defaults->tm_year;
    }
    if (tp->tm_mon < 0) {
        tp->tm_mon = defaults->tm_mon;
    }
    if (tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday;
    }
    if (tp->tm_yday < 0) {
        tp->tm_yday = (leapYear(tp->tm_year + 1900) ?
            leapMonthStart[tp->tm_mon] : normalMonthStart[tp->tm_mon]) + tp->tm_mday - 1;
    }
    if (tp->tm_hour < 0) {
        tp->tm_hour = defaults->tm_hour;
    }
    if (tp->tm_min < 0) {
        tp->tm_min = defaults->tm_min;
    }
    if (tp->tm_sec < 0) {
        tp->tm_sec = defaults->tm_sec;
    }
}


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



/********* Start of file ../../../src/upload.c ************/


/*
    upload.c -- File upload handler

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/



#if ME_GOAHEAD_UPLOAD && !ME_ROM
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
static void initUpload(Webs *wp);
static void processContentBoundary(Webs *wp, char *line);
static bool processContentData(Webs *wp);
static void processUploadHeader(Webs *wp, char *line);

/************************************* Code ***********************************/
/*
    The upload handler functions as a filter.
    Return false because the upload handler is not a terminal handler.
 */
static bool uploadHandler(Webs *wp)
{
    return 0;
}


static void initUpload(Webs *wp)
{
    char    *boundary;

    if (wp->uploadState == 0) {
        wp->uploadState = UPLOAD_BOUNDARY;
        if ((boundary = strstr(wp->contentType, "boundary=")) != 0) {
            boundary += 9;
            wfree(wp->boundary);
            wp->boundary = sfmt("--%s", boundary);
            wp->boundaryLen = strlen(wp->boundary);
        }
        if (wp->boundaryLen == 0 || *wp->boundary == '\0') {
            websError(wp, HTTP_CODE_BAD_REQUEST, "Bad boundary");
        } else {
            websSetVar(wp, "UPLOAD_DIR", uploadDir);
            wp->files = hashCreate(11);
        }
    }
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

    if (wp->files >= 0) {
        for (s = hashFirst(wp->files); s; s = hashNext(wp->files, s)) {
            up = s->content.value.symbol;
            freeUploadFile(up);
            if (up == wp->currentFile) {
                wp->currentFile = 0;
            }
        }
        hashFree(wp->files);
    }
    if (wp->currentFile) {
        freeUploadFile(wp->currentFile);
        wp->currentFile = 0;
    }
    if (wp->upfd >= 0) {
        close(wp->upfd);
        wp->upfd = -1;
    }
}


PUBLIC bool websProcessUploadData(Webs *wp)
{
    char    *line, *nextTok;
    ssize   len, nbytes;
    bool    canProceed;

    line = 0;
    canProceed = 1; 
    while (canProceed && !wp->finalized && wp->uploadState != UPLOAD_CONTENT_END) {
        if  (wp->uploadState == UPLOAD_BOUNDARY || wp->uploadState == UPLOAD_CONTENT_HEADER) {
            /*
                Parse the next input line
             */
            line = wp->input.servp;
            if ((nextTok = memchr(line, '\n', bufLen(&wp->input))) == 0) {
                /* Incomplete line */
                canProceed = 0;
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
            initUpload(wp);
            break;

        case UPLOAD_BOUNDARY:
            processContentBoundary(wp, line);
            break;

        case UPLOAD_CONTENT_HEADER:
            processUploadHeader(wp, line);
            break;

        case UPLOAD_CONTENT_DATA:
            canProceed = processContentData(wp);
            if (bufLen(&wp->input) < wp->boundaryLen) {
                /*  Incomplete boundary - return to get more data */
                canProceed = 0;
            }
            break;

        case UPLOAD_CONTENT_END:
            break;
        }
    }
    bufCompact(&wp->input);
    return canProceed;
}


static void processContentBoundary(Webs *wp, char *line)
{
    /*
        Expecting a multipart boundary string
     */
    if (strncmp(wp->boundary, line, wp->boundaryLen) != 0) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Incomplete boundary");

    } else if (line[wp->boundaryLen] && strcmp(&line[wp->boundaryLen], "--") == 0) {
        wp->uploadState = UPLOAD_CONTENT_END;

    } else {
        wp->uploadState = UPLOAD_CONTENT_HEADER;
    }
}


static void processUploadHeader(Webs *wp, char *line)
{
    WebsUpload  *file;
    char        *key, *headerTok, *rest, *nextPair, *value;

    if (line[0] == '\0') {
        wp->uploadState = UPLOAD_CONTENT_DATA;
        return;
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
        wfree(wp->uploadVar);
        wfree(wp->clientFilename);
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
                    return;
                }
                value = websNormalizeUriPath(value);
                if (*value == '.' || !websValidUriChars(value) || strpbrk(value, "\\/:*?<>|~\"'%`^\n\r\t\f")) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Bad upload client filename");
                    wfree(value);
                    return;
                }
                wfree(wp->clientFilename);
                wp->clientFilename = value;

                /*
                    Create the file to hold the uploaded data
                 */
                wfree(wp->uploadTmp);
                if ((wp->uploadTmp = websTempFile(uploadDir, "tmp")) == 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR,
                        "Cannot create upload temp file %s. Check upload temp dir %s", wp->uploadTmp, uploadDir);
                    return;
                }
                trace(5, "File upload of: %s stored as %s", wp->clientFilename, wp->uploadTmp);

                if ((wp->upfd = open(wp->uploadTmp, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot open upload temp file %s", wp->uploadTmp);
                    return;
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


static bool processContentData(Webs *wp)
{
    WebsUpload  *file;
    WebsBuf     *content;
    ssize       size, nbytes;
    char        *data, *bp;

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
            if (writeToFile(wp, content->servp, nbytes) < 0) {
                /* Proceed to handle error */
                return 1;
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
                /* Proceed to handle error */
                return 1;
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
        wfree(wp->clientFilename);
        wp->clientFilename = 0;
        wfree(wp->uploadTmp);
        wp->uploadTmp = 0;
    }
    wp->uploadState = UPLOAD_BOUNDARY;
    return 1;
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

    if (wp->files >= 0) {
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
