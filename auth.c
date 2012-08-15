/*
    auth.c -- Authorization Management

    Copyright (c) All Rights Reserved. See details at the end of the file.

*/

/********************************* Includes ***********************************/

#include    "goahead.h"

#if BIT_AUTH
/*********************************** Locals ***********************************/

//  MOB - rethink
#define NONCE_PREFIX    T("onceuponatimeinparadise")
#define NONCE_SIZE      34
#define HASH_SIZE       16

static sym_fd_t users = -1;
static sym_fd_t roles = -1;
static WebsRoute **routes = 0;
static int routeCount = 0;
static int routeMax = 0;
static char_t *secret;

#if UNUSED
static char_t websRealm[BIT_LIMIT_PASSWORD] = T("Acme Inc");
#endif

/********************************** Forwards **********************************/

static char *addString(char_t *field, char_t *word);
static int aspCan(int ejid, Webs *wp, int argc, char_t **argv);
static void computeAllCapabilities();
static void computeCapabilities(WebsUser *user);
static int decodeBasicDetails(Webs *wp);
static char_t *findString(char_t *field, char_t *word);
static int loadAuth(char_t *path);
static int removeString(char_t *field, char_t *word);
static char_t *trimSpace(char_t *s);

#if BIT_DIGEST_AUTH
static char_t *calcDigest(Webs *wp, char_t *userName, char_t *password);
static char_t *createDigestNonce(Webs *wp);
static int decodeDigestDetails(Webs *wp);
static void digestLogin(Webs *wp, int why);
static int parseDigestNonce(char_t *nonce, char_t **secret, char_t **realm, time_t *when);
static bool verifyDigestPassword(Webs *wp);
#endif
static void basicLogin(Webs *wp, int why);
static bool verifyBasicPassword(Webs *wp);
static void formLogin(Webs *wp, int why);
static bool verifyFormPassword(Webs *wp);

/************************************ Code ************************************/

int websOpenAuth(char_t *path) 
{
    char    sbuf[64];
    
    if ((users = symOpen(G_SMALL_HASH)) < 0) {
        return -1;
    }
    if ((roles = symOpen(G_SMALL_HASH)) < 0) {
        return -1;
    }
    gfmtStatic(sbuf, sizeof(sbuf), "%x:%x", rand(), time(0));
    secret = websMD5(sbuf);
    if (path) {
        return loadAuth(path);
    }
#if BIT_JAVASCRIPT
    websJsDefine(T("can"), aspCan);
#endif
    return 0;
}


void websCloseAuth() 
{
    int     i;
   
    //  MOB - not good enough. Must free all users, roles and routes
    symClose(users);
    symClose(roles);
    for (i = 0; i < routeCount; i++) {
        gfree(routes[i]);
    }
    gfree(routes);
    users = roles = -1;
    routes = 0;
    routeCount = routeMax = 0;
    gfree(secret);
}


static void growRoutes()
{
    if (routeCount >= routeMax) {
        routeMax += 16;
        //  RC
        routes = grealloc(routes, sizeof(WebsRoute*) * routeMax);
    }
}


/*
    Load an auth database. Schema:
        user: enable: realm: name: password: role [role...]
        role: enable: name: capability [capability...]
        uri: enable: realm: type: uri: method: capability [capability...]
 */
static int loadAuth(char_t *path)
{
    WebsLogin   login;
    WebsVerify  verify;
    FILE        *fp;
    char        buf[512], *name, *enabled, *type, *password, *uri, *capabilities, *roles, *line, *kind, *loginUri, *realm;

    if ((fp = fopen(path, "rt")) == 0) {
        return -1;
    }
    buf[sizeof(buf) - 1] = '\0';
    while ((line = fgets(buf, sizeof(buf) -1, fp)) != 0) {
        kind = strtok(buf, " \t:\r\n");
        // for (cp = kind; cp && isspace((uchar) *cp); cp++) { }
        if (kind == 0 || *kind == '\0' || *kind == '#') {
            continue;
        }
        enabled = strtok(NULL, " \t:");
        if (*enabled != '1') continue;
        if (gmatch(kind, "user")) {
            realm = trimSpace(strtok(NULL, ":"));
            name = strtok(NULL, " \t:");
            password = strtok(NULL, " \t:");
            roles = strtok(NULL, "\r\n");
            if (websAddUser(realm, name, password, roles) < 0) {
                return -1;
            }
        } else if (gmatch(kind, "role")) {
            name = strtok(NULL, " \t:");
            capabilities = strtok(NULL, "\r\n");
            if (websAddRole(name, capabilities) < 0) {
                return -1;
            }
        } else if (gmatch(kind, "uri")) {
            realm = trimSpace(strtok(NULL, ":"));
            type = strtok(NULL, " \t:");
            uri = strtok(NULL, " \t:");
            capabilities = strtok(NULL, " \t:\r\n");
            if (gmatch(type, "basic")) {
                login = basicLogin;
                verify = verifyBasicPassword;
#if BIT_DIGEST_AUTH
            } else if (gmatch(type, "digest")) {
                login = digestLogin;
                verify = verifyDigestPassword;
#endif
            } else if (gmatch(type, "form")) {
                login = formLogin;
                verify = verifyFormPassword;
                loginUri = strtok(NULL, " \t\r\n");
            } else {
                login = 0;
                verify = 0;
                loginUri = 0;
            }
            if (websAddRoute(realm, uri, capabilities, loginUri, login, verify) < 0) {
                return -1;
            }
        }
    }
    fclose(fp);
    computeAllCapabilities();
    return 0;
}


int websSave(char_t *path)
{
    //  MOB TODO
    return 0;
}


int websAddUser(char_t *realm, char_t *name, char_t *password, char_t *roles)
{
    WebsUser    *up;

    if (symLookup(users, name)) {
        error(T("User %s already exists"), name);
        /* Already exists */
        return -1;
    }
    if ((up = galloc(sizeof(WebsUser))) == 0) {
        return 0;
    }
    up->name = gstrdup(name);
    if (roles) {
        gfmtAlloc(&up->roles, -1, " %s ", roles);
    } else {
        up->roles = gstrdup(T(" "));
    }
    up->realm = gstrdup(realm);
    up->password = gstrdup(password);
    up->capabilities = 0;
    up->enable = 1;
    if (symEnter(users, name, valueSymbol(up), 0) == 0) {
        return -1;
    }
    return 0;
}


#if UNUSED
int websSetPassword(char_t *name, char_t *password) 
{
    WebsUser    *up;
    sym_t       *sym;

    if ((sym = symLookup(users, name)) == 0) {
        return -1;
    }
    up = (WebsUser*) sym->content.value.symbol;
    gassert(up);
    //  MOB - MD5
    up->password = gstrdup(password);
    return 0;
}
#endif


bool websFormLogin(Webs *wp, char_t *userName, char_t *password)
{
    WebsUser    *up;
    sym_t       *sym;

    gfree(wp->userName);
    wp->userName = gstrdup(userName);
    wp->password = gstrdup(password);

    if ((sym = symLookup(users, userName)) == 0) {
        trace(2, T("websFormLogin: Unknown user %s\n"), userName);
        return 0;
    }
    up = (WebsUser*) sym->content.value.symbol;
#if BIT_SESSION
{
    Session     *sp;
    if ((sp = websGetSession(wp, 1)) == 0) {
        error(T("Can't get session\n"));
        return 0;
    }
    sp->user = up;
}
#endif
    wp->user = up;
    if (!verifyFormPassword(wp)) {
        trace(2, T("Form password does not match\n"));
        // (up->login)(wp, WEBS_BAD_PASSWORD);
        return 0;
    }
    return 1;
}


int websEnableUser(char_t *name, int enable) 
{
    WebsUser    *user;
    sym_t       *sym;

    if ((sym = symLookup(users, name)) == 0) {
        return -1;
    }
    user = (WebsUser*) sym->content.value.symbol;
    user->enable = enable;
    return 0;
}


int websRemoveUser(char_t *name) 
{
    //  MOB - must free all memory for user
    return symDelete(users, name);
}


int websAddUserRole(char_t *name, char_t *role) 
{
    WebsUser    *user;
    sym_t       *sym;

    if ((sym = symLookup(users, name)) == 0) {
        return -1;
    }
    user = (WebsUser*) sym->content.value.symbol;
    if (findString(user->roles, role)) {
        return 0;
    }
    user->roles = addString(user->roles, role);
    computeCapabilities(user);
    return 0;
}


/*
    Update the user->capabilities string. This is a unique set of capabilities
 */
static void computeCapabilities(WebsUser *user)
{
    WebsRole    *rp;
    ringq_t     buf;
    sym_fd_t    capabilities;
    sym_t       *sym;
    char_t      *role, *capability, *rcaps, *uroles, *utok, *ctok;

    if ((capabilities = symOpen(G_SMALL_HASH)) == 0) {
        return;
    }
    uroles = gstrdup(user->roles);
    for (role = gtok(uroles, T(" "), &utok); role; role = gtok(NULL, T(" "), &utok)) {
        if ((sym = symLookup(roles, role)) == 0) {
            /* Role not found: Interpret the role as a capability */
            if (symEnter(capabilities, role, valueInteger(0), 0) == 0) {
                error(T("Can't add capability"));
                break;
            }
            continue;
        }
        rp = (WebsRole*) sym->content.value.symbol;
        rcaps = gstrdup(rp->capabilities);
        for (capability = gtok(rcaps, T(" "), &ctok); capability; capability = gtok(NULL, T(" "), &ctok)) {
            if (symLookup(capabilities, capability)) {
                continue;
            }
            if (symEnter(capabilities, capability, valueInteger(0), 0) == 0) {
                error(T("Can't add capability"));
                break;
            }
        }
        gfree(rcaps);
    }
    gfree(uroles);
    ringqOpen(&buf, 80, -1);
    ringqPutc(&buf, ' ');
    for (sym = symFirst(capabilities); sym; sym = symNext(capabilities, sym)) {
        ringqPutStr(&buf, sym->name.value.string);
        ringqPutc(&buf, ' ');
    }
    ringqAddNull(&buf);
    user->capabilities = gstrdup((char*) buf.servp);
    symClose(capabilities);
    trace(4, "User \"%s\" in realm \"%s\" has capabilities:%s\n", user->name, user->realm, user->capabilities);
}


static void computeAllCapabilities()
{
    WebsUser    *up;
    sym_t       *sym;

    for (sym = symFirst(users); sym; sym = symNext(users, sym)) {
        up = (WebsUser*) sym->content.value.symbol;
        computeCapabilities(up);
    }
}


//  MOB - remove role from user
//  MOB - must recompute capabilities
int websRemoveUserRole(char_t *name, char_t *role) 
{
    WebsUser    *up;
    sym_t       *sym;

    if ((sym = symLookup(users, name)) == 0) {
        return -1;
    }
    up = (WebsUser*) sym->content.value.symbol;
    return removeString(up->roles, role);
}


bool websUserHasCapability(char_t *name, char_t *capability) 
{
    WebsUser    *up;
    sym_t       *sym;

    if ((sym = symLookup(users, name)) == 0) {
        return -1;
    }
    up = (WebsUser*) sym->content.value.symbol;
    if (findString(up->capabilities, capability)) {
        return 0;
    }
    return -1;
}


/*
    Expand any capabilites that are roles
 */
static void expandCapabilities(ringq_t *buf, char_t *str)
{
    WebsRole    *rp;
    sym_t       *sym;
    char_t      *word, *tok;

    str = gstrdup(str);
    for (word = gtok(str, T(" "), &tok); word; word = gtok(NULL, T(" "), &tok)) {
        if ((sym = symLookup(roles, word)) != 0) {
            rp = (WebsRole*) sym->content.value.symbol;
            expandCapabilities(buf, rp->capabilities);
        } else {
            ringqPutc(buf, ' ');
            ringqPutStr(buf, word);
        }
    }
}


int websAddRole(char_t *name, char_t *capabilities)
{
    WebsRole    *rp;
    ringq_t     buf;

    if (symLookup(roles, name)) {
        error(T("Role %s already exists"), name);
        /* Already exists */
        return -1;
    }
    if ((rp = galloc(sizeof(WebsRole))) == 0) {
        return 0;
    }
    ringqOpen(&buf, 80, -1);
    expandCapabilities(&buf, capabilities);
    ringqPutc(&buf, ' ');
    ringqAddNull(&buf);
    rp->capabilities = gstrdup((char*) buf.servp);
    rp->enable = 1;
    if (symEnter(roles, name, valueSymbol(rp), 0) == 0) {
        return -1;
    }
    trace(4, T("Role \"%s\" has capabilities:%s\n"), name, rp->capabilities);
    return 0;
}


//  MOB - note. Does not recompute user capabilities
int websRemoveRole(char_t *name) 
{
    //  MOB - need to free full role
    return symDelete(roles, name);
}


int websAddRoleCapability(char_t *name, char_t *capability) 
{
    WebsRole    *rp;
    sym_t       *sym;

    if ((sym = symLookup(roles, name)) == 0) {
        return -1;
    }
    rp = (WebsRole*) sym->content.value.symbol;
    if (findString(rp->capabilities, capability)) {
        return 0;
    }
    rp->capabilities = addString(rp->capabilities, capability);
    return 0;
}


int websRemoveRoleCapability(char_t *name, char_t *capability) 
{
    WebsRole    *rp;
    sym_t       *sym;

    if ((sym = symLookup(roles, name)) == 0) {
        return -1;
    }
    rp = (WebsRole*) sym->content.value.symbol;
    return removeString(rp->capabilities, capability);
}


int websAddRoute(char_t *realm, char_t *uri, char_t *capabilities, char_t *loginUri, WebsLogin login, WebsVerify verify)
{
    WebsRoute   *rp;
    int         i;

    for (i = 0; i < routeCount; i++) {
        rp = routes[i];
        if (gmatch(rp->prefix, uri)) {
            error(T("URI %s already exists"), uri);
            /* Already exists */
            return -1;
        }
    }
    if ((rp = galloc(sizeof(WebsUser))) == 0) {
        return 0;
    }
    rp->realm = gstrdup(realm);
    rp->prefix = gstrdup(uri);
    rp->prefixLen = glen(uri);
    rp->loginUri = gstrdup(loginUri);
#if BIT_PACK_SSL
    rp->secure = (capabilities && strstr(capabilities, "SECURE")) ? 1 : 0;
#endif
    /* Always have a leading and trailing space to make matching quicker */
    gfmtAlloc(&rp->capabilities, -1, " %s", capabilities ? capabilities : "");
    rp->login = login;
    rp->verify = verify;
    rp->enable = 1;
    growRoutes();
    routes[routeCount++] = rp;
    trace(4, T("Route \"%s\" in realm \%s\" requires capabilities: %s\n"), uri, realm, capabilities);
    return 0;
}


static int lookupRoute(char_t *uri) 
{
    WebsRoute    *rp;
    int         i;

    for (i = 0; i < routeCount; i++) {
        rp = routes[i];
        if (gmatch(rp->prefix, uri)) {
            return i;
        }
    }
    return -1;
}


int amRemoveRoute(char_t *uri) 
{
    int     i;

    if ((i = lookupRoute(uri)) == 0) {
        return -1;
    }
    for (; i < routeCount; i++) {
        routes[i] = routes[i+1];
    }
    routeCount++;
    //  MOB - must free URI
    return 0;
}


static void basicLogin(Webs *wp, int why)
{
    gassert(wp->route);
    gfree(wp->authResponse);
    gfmtAlloc(&wp->authResponse, -1, T("Basic realm=\"%s\""), wp->route->realm);
    websError(wp, 401, T("Access Denied. User not logged in."));
}


static void formLogin(Webs *wp, int why)
{
    websRedirect(wp, wp->route->loginUri);
}


#if BIT_DIGEST_AUTH
static void digestLogin(Webs *wp, int why)
{
    char_t  *nonce, *opaque;

    gassert(wp->route);
    nonce = createDigestNonce(wp);
    //  MOB - opaque should be one time?  Appweb too.
    opaque = T("5ccc069c403ebaf9f0171e9517f40e41");
    gfmtAlloc(&wp->authResponse, -1,
        "Digest realm=\"%s\", domain=\"%s\", qop=\"%s\", nonce=\"%s\", opaque=\"%s\", algorithm=\"%s\", stale=\"%s\"",
        wp->route->realm, websGetHostUrl(), T("auth"), nonce, opaque, T("MD5"), T("FALSE"));
    gfree(nonce);
    websError(wp, 401, T("Access Denied. User not logged in."));
}


static bool verifyDigestPassword(Webs *wp)
{
    char_t  *digest, *secret, *realm;
    time_t  when;

    if (!wp->user || !wp->route) {
        return 0;
    }
    /*
        Validate the nonce value - prevents replay attacks
     */
    when = 0; secret = 0; realm = 0;
    parseDigestNonce(wp->nonce, &secret, &realm, &when);

    if (!gmatch(secret, secret)) {
        trace(2, T("Access denied: Nonce mismatch\n"));
        return 0;
    } else if (!gmatch(realm, wp->route->realm)) {
        trace(2, T("Access denied: Realm mismatch\n"));
        return 0;
    } else if (!gmatch(wp->qop, "auth")) {
        trace(2, T("Access denied: Bad qop\n"));
        return 0;
    } else if ((when + (5 * 60)) < time(0)) {
        trace(2, T("Access denied: Nonce is stale\n"));
        return 0;
    }
    /*
        Verify the password
     */
    digest = calcDigest(wp, 0, wp->user->password);
    if (gmatch(wp->password, digest) == 0) {
        trace(2, T("Access denied: password mismatch\n"));
        return 0;
    }
    gfree(digest);
    return 1;
}
#endif


static bool verifyBasicPassword(Webs *wp)
{
    char_t  passbuf[BIT_LIMIT_PASSWORD * 3 + 3], *password;
    bool    rc;
    
    if (!wp->user || !wp->route) {
        return 0;
    }
    gassert(wp->route);
    gfmtStatic(passbuf, sizeof(passbuf), "%s:%s:%s", wp->userName, wp->route->realm, wp->password);
    password = websMD5(passbuf);
    rc = gmatch(wp->user->password, password);
    gfree(password);
    return rc;
}


//  MOB - same as verifyBasicPassword
static bool verifyFormPassword(Webs *wp)
{
    char_t  passbuf[BIT_LIMIT_PASSWORD * 3 + 3], *password;
    int     rc;

    if (!wp->user || !wp->route) {
        return 0;
    }
    gfmtStatic(passbuf, sizeof(passbuf), "%s:%s:%s", wp->userName, wp->route->realm, wp->password);
    password = websMD5(passbuf);
    rc = gmatch(password, wp->user->password);
    gfree(password);
    return rc;
}


bool websVerifyRoute(Webs *wp)
{
    WebsRoute   *rp;
    sym_t       *sym;
    ssize       plen, len;
    int         i;

    plen = glen(wp->path);
    for (i = 0; i < routeCount; i++) {
        rp = routes[i];
        if (plen < rp->prefixLen) continue;
        len = min(rp->prefixLen, plen);
        if (strncmp(wp->path, rp->prefix, len) == 0) {
            wp->route = rp;
            break;
        }
    }
    if (rp->secure && !(wp->flags & WEBS_SECURE)) {
        websStats.access++;
        websError(wp, 405, T("Access Denied. Secure access is required."));
        return 0;
    }
    if (gmatch(rp->capabilities, " ")) {
        /* URI does not require any capabilities, return success */
        return 1;
    }
    if (wp->authDetails) {
        //  MOB - need a hook for this
        if (gcaselessmatch(wp->authType, T("basic"))) {
            decodeBasicDetails(wp);
#if BIT_DIGEST_AUTH
        } else if (gcaselessmatch(wp->authType, T("digest"))) {
            decodeDigestDetails(wp);
#endif
        }
    }
    //  MOB - is user always null at this point?
    if (!wp->user) {
        if (!wp->userName || !*wp->userName) {
            websStats.access++;
            (rp->login)(wp, WEBS_LOGIN_REQUIRED);
            return 0;
        }
        if ((sym = symLookup(users, wp->userName)) == 0) {
            (rp->login)(wp, WEBS_BAD_USERNAME);
            return 0;
        }
        wp->user = (WebsUser*) sym->content.value.symbol;
        if (!(rp->verify)(wp)) {
            (rp->login)(wp, WEBS_BAD_PASSWORD);
            return 0;
        }
    }
    gassert(wp->user);
    return websCan(wp, rp->capabilities);
}


//  MOB - should this be capabilities?
bool websCan(Webs *wp, char_t *capabilities) 
{
    sym_t   *sym;

    if (!wp->user) {
        if (!wp->userName) {
            return 0;
        }
        if ((sym = symLookup(users, wp->userName)) == 0) {
            trace(2, T("Can't find user %s\n"), wp->userName);
            return 0;
        }
        wp->user = (WebsUser*) sym->content.value.symbol;
    }
    //  MOB - not implemented using multiple capabilities
    return findString(wp->user->capabilities, capabilities) ? 1 : 0;
}


static int aspCan(int ejid, Webs *wp, int argc, char_t **argv)
{
    if (websCan(wp, argv[0])) {
        //  MOB - how to set return 
        return 0;
    }
    return 1;
} 


static char_t *findString(char_t *field, char_t *word)
{
    char_t  str[WEBS_MAX_WORD], *cp;

    if (!field) {
        return 0;
    }
    str[0] = ' ';
    gstrncpy(&str[1], str, sizeof(str) - 1);
    if ((cp = strstr(field, word)) != 0) {
        return &cp[1];
    }
    return 0;
}


/*
    The field always has a leading and trailing space
 */
static char *addString(char_t *field, char_t *word)
{
    char_t  *old;

    old = field;
    gfmtAlloc(&field, -1, "%s%s ", old, word);
    gfree(old);
    return field;
}


static int removeString(char_t *field, char_t *word)
{
    char_t  *old, *cp;

    if ((cp = findString(field, word)) == 0) {
        return -1;
    }
    old = field;
    cp[-1] = '\0';
    gfmtAlloc(&field, -1, "%s%s ", field, &cp[strlen(word)]);
    gfree(old);
    return 0;
}


static int decodeBasicDetails(Webs *wp)
{
    char    *cp, *userAuth;

    /*
        Split userAuth into userid and password
     */
    userAuth = websDecode64(wp->authDetails);
    if ((cp = gstrchr(userAuth, ':')) != NULL) {
        *cp++ = '\0';
    }
    if (cp) {
        wp->userName = gstrdup(userAuth);
        wp->password = gstrdup(cp);
    } else {
        wp->userName = gstrdup(T(""));
        wp->password = gstrdup(T(""));
    }
    gfree(userAuth);
    wp->flags |= WEBS_BASIC_AUTH;
    return 0;
}


#if BIT_DIGEST_AUTH
static int decodeDigestDetails(Webs *wp)
{
    char        *value, *tok, *key, *dp, *sp;
    int         seenComma;

    key = gstrdup(wp->authDetails);

    while (*key) {
        while (*key && isspace((uchar) *key)) {
            key++;
        }
        tok = key;
        while (*tok && !isspace((uchar) *tok) && *tok != ',' && *tok != '=') {
            tok++;
        }
        *tok++ = '\0';

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
        *tok++ = '\0';

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
            if (gcaselesscmp(key, "algorithm") == 0) {
                break;
            } else if (gcaselesscmp(key, "auth-param") == 0) {
                break;
            }
            break;

        case 'c':
            if (gcaselesscmp(key, "cnonce") == 0) {
                wp->cnonce = gstrdup(value);
            }
            break;

        case 'd':
            if (gcaselesscmp(key, "domain") == 0) {
                break;
            }
            break;

        case 'n':
            if (gcaselesscmp(key, "nc") == 0) {
                wp->nc = gstrdup(value);
            } else if (gcaselesscmp(key, "nonce") == 0) {
                wp->nonce = gstrdup(value);
            }
            break;

        case 'o':
            if (gcaselesscmp(key, "opaque") == 0) {
                wp->opaque = gstrdup(value);
            }
            break;

        case 'q':
            if (gcaselesscmp(key, "qop") == 0) {
                wp->qop = gstrdup(value);
            }
            break;

        case 'r':
            if (gcaselesscmp(key, "realm") == 0) {
                wp->realm = gstrdup(value);
            } else if (gcaselesscmp(key, "response") == 0) {
                /* Store the response digest in the password field */
                wp->password = gstrdup(value);
            }
            break;

        case 's':
            if (gcaselesscmp(key, "stale") == 0) {
                break;
            }
        
        case 'u':
            if (gcaselesscmp(key, "uri") == 0) {
                wp->digestUri = gstrdup(value);
            } else if (gcaselesscmp(key, "username") == 0 || gcaselesscmp(key, "user") == 0) {
                wp->userName = gstrdup(value);
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
    if (wp->userName == 0 || wp->realm == 0 || wp->nonce == 0 || wp->route == 0 || wp->password == 0) {
        return -1;
    }
    if (wp->qop && (wp->cnonce == 0 || wp->nc == 0)) {
        return -1;
    }
    if (wp->qop == 0) {
        wp->qop = gstrdup("");
    }
    wp->flags |= WEBS_DIGEST_AUTH;
    return 0;
}


/*
    Create a nonce value for digest authentication (RFC 2617)
 */
static char_t *createDigestNonce(Webs *wp)
{
    char_t       nonce[256];
    static int64 next = 0;

    gassert(wp->route);
    gfmtStatic(nonce, sizeof(nonce), "%s:%s:%x:%x", secret, wp->route->realm, time(0), next++);
    return websEncode64(nonce);
}


static int parseDigestNonce(char_t *nonce, char_t **secret, char_t **realm, time_t *when)
{
    char_t    *tok, *decoded, *whenStr;                                                                      
                                                                                                           
    if ((decoded = websDecode64(nonce)) == 0) {                                                             
        return -1;
    }                                                                                                      
    *secret = gtok(decoded, ":", &tok);
    *realm = gtok(NULL, ":", &tok);
    whenStr = gtok(NULL, ":", &tok);
    *when = ghextoi(whenStr);
    return 0;
}


/*
   Get a Digest value using the MD5 algorithm -- See RFC 2617 to understand this code.
*/
static char_t *calcDigest(Webs *wp, char_t *userName, char_t *password)
{
    char_t  a1Buf[256], a2Buf[256], digestBuf[256];
    char_t  *ha1, *ha2, *method, *result;

    /*
        Compute HA1. If userName == 0, then the password is already expected to be in the HA1 format 
        (MD5(userName:realm:password).
     */
    if (userName == 0) {
        ha1 = gstrdup(password);
    } else {
        gfmtStatic(a1Buf, sizeof(a1Buf), "%s:%s:%s", userName, wp->realm, password);
        ha1 = websMD5(a1Buf);
    }

    /*
        HA2
     */ 
    method = websGetVar(wp, T("REQUEST_METHOD"), NULL);
    gfmtStatic(a2Buf, sizeof(a2Buf), "%s:%s", method, wp->digestUri);
    ha2 = websMD5(a2Buf);

    /*
        H(HA1:nonce:HA2)
     */
    if (gcmp(wp->qop, "auth") == 0) {
        gfmtStatic(digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, wp->nonce, wp->nc, wp->cnonce, wp->qop, ha2);

    } else if (gcmp(wp->qop, "auth-int") == 0) {
        gfmtStatic(digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, wp->nonce, wp->nc, wp->cnonce, wp->qop, ha2);

    } else {
        gfmtStatic(digestBuf, sizeof(digestBuf), "%s:%s:%s", ha1, wp->nonce, ha2);
    }
    result = websMD5(digestBuf);
    gfree(ha1);
    gfree(ha2);
    return result;
}
#endif /* BIT_DIGEST_AUTH */


#if UNUSED
/*
    Store the new realm name
    MOB -rename and legacy preserve old name
 */
void websSetRealm(char_t *realmName)
{
    gassert(realmName);

    gstrncpy(websRealm, realmName, TSZ(websRealm));
}


/*
    Return the realm name (used for authorization)
 */
char_t *websGetRealm()
{
    return websRealm;
}
#endif


static char_t *trimSpace(char_t *s)
{
    char_t  *end;
    
    while (*s && isspace((int) *s)) {
        s++;
    }
    for (end = &s[strlen(s) - 1]; end > s && isspace((int) *end); ) {
        end--;
    }
    end[1] = '\0';
    return s;
}

#endif /* BIT_AUTH */

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
