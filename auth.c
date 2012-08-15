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
static Uri **uris = 0;
static int uriCount = 0;
static int uriMax = 0;
static char_t *secret;

static char_t websRealm[BIT_LIMIT_PASSWORD] = T(BIT_AUTH_REALM);

/********************************** Forwards **********************************/

static char *addString(char_t *field, char_t *word);
static void computeAllCapabilities();
static void computeCapabilities(User *user);
static int decodeBasicDetails(webs_t wp);
static char_t *findString(char_t *field, char_t *word);
static int loadAuth(char_t *path);
static int removeString(char_t *field, char_t *word);

#if BIT_DIGEST_AUTH
static char_t *calcDigest(webs_t wp, char_t *userName, char_t *password);
static char_t *createDigestNonce(webs_t wp);
static int decodeDigestDetails(webs_t wp);
static void digestLogin(webs_t wp, int why);
static int parseDigestNonce(char_t *nonce, char_t **secret, char_t **realm, time_t *when);
static bool verifyDigestPassword(webs_t wp);
#endif
static void basicLogin(webs_t wp, int why);
static bool verifyBasicPassword(webs_t wp);
static void formLogin(webs_t wp, int why);
static bool verifyFormPassword(webs_t wp);

/************************************ Code ************************************/

int amOpen(char_t *path) 
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
    return 0;
}


void amClose() 
{
    int     i;
   
    //  MOB - not good enough. Must free all users, roles and uris
    symClose(users);
    symClose(roles);
    for (i = 0; i < uriCount; i++) {
        gfree(uris[i]);
    }
    gfree(uris);
    users = roles = -1;
    uris = 0;
    uriCount = uriMax = 0;
    gfree(secret);
}


static void growUris()
{
    if (uriCount >= uriMax) {
        uriMax += 16;
        //  RC
        uris = grealloc(uris, sizeof(Uri*) * uriMax);
    }
}


/*
    Load an auth database. Schema:
        user: enable: name: password: role [role...]
        role: enable: name: capability [capability...]
        uri: enable: uri: method: capability [capability...]
 */
static int loadAuth(char_t *path)
{
    AmLogin     login;
    AmVerify    verify;
    FILE        *fp;
    char        buf[512], *name, *enabled, *method, *password, *uri, *capabilities, *roles, *line, *kind, *loginUri;

    if ((fp = fopen(path, "rt")) == 0) {
        return -1;
    }
    buf[sizeof(buf) - 1] = '\0';
    while ((line = fgets(buf, sizeof(buf) -1, fp)) != 0) {
        kind = strtok(buf, " :\t\r\n");
        // for (cp = kind; cp && isspace((uchar) *cp); cp++) { }
        if (kind == 0 || *kind == '\0' || *kind == '#') {
            continue;
        }
        enabled = strtok(NULL, " :\t");
        if (*enabled != '1') continue;
        if (gmatch(kind, "user")) {
            name = strtok(NULL, " :\t");
            password = strtok(NULL, " :\t");
            roles = strtok(NULL, "\r\n");
            if (amAddUser(name, password, roles) < 0) {
                return -1;
            }

        } else if (gmatch(kind, "role")) {
            name = strtok(NULL, " :\t");
            capabilities = strtok(NULL, "\r\n");
            if (amAddRole(name, capabilities) < 0) {
                return -1;
            }

        } else if (gmatch(kind, "uri")) {
            method = strtok(NULL, " :\t");
            uri = strtok(NULL, " :\t");
            capabilities = strtok(NULL, " :\t\r\n");
            if (gmatch(method, "basic")) {
                login = basicLogin;
                verify = verifyBasicPassword;
#if BIT_DIGEST_AUTH
            } else if (gmatch(method, "digest")) {
                login = digestLogin;
                verify = verifyDigestPassword;
#endif
            } else if (gmatch(method, "form")) {
                login = formLogin;
                verify = verifyFormPassword;
                loginUri = strtok(NULL, " \t\r\n");
            } else {
                login = 0;
                verify = 0;
                loginUri = 0;
            }
            if (amAddUri(uri, capabilities, loginUri, login, verify) < 0) {
                return -1;
            }
        }
    }
    fclose(fp);
    computeAllCapabilities();
    return 0;
}


int amSave(char_t *path)
{
    //  MOB TODO
    return 0;
}


int amAddUser(char_t *name, char_t *password, char_t *roles)
{
    User    *up;

    if (symLookup(users, name)) {
        error(T("User %s already exists"), name);
        /* Already exists */
        return -1;
    }
    if ((up = galloc(sizeof(User))) == 0) {
        return 0;
    }
    up->name = gstrdup(name);
    if (roles) {
        gfmtAlloc(&up->roles, -1, " %s ", roles);
    } else {
        up->roles = gstrdup(T(" "));
    }
    up->password = gstrdup(password);
    up->capabilities = 0;
    up->enable = 1;
    if (symEnter(users, name, valueSymbol(up), 0) == 0) {
        return -1;
    }
    return 0;
}


int amSetPassword(char_t *name, char_t *password) 
{
    sym_t   *sym;
    User    *up;

    if ((sym = symLookup(users, name)) == 0) {
        return -1;
    }
    up = (User*) sym->content.value.symbol;
    gassert(up);
    //  MOB - MD5
    up->password = gstrdup(password);
    return 0;
}


bool amFormLogin(webs_t wp, char_t *userName, char_t *password)
{
    User        *up;
    sym_t       *sym;

    gfree(wp->userName);
    wp->userName = gstrdup(userName);
    wp->password = gstrdup(password);

    if ((sym = symLookup(users, userName)) == 0) {
        trace(2, T("amFormLogin: Unknown user %s\n"), userName);
        return 0;
    }
    up = (User*) sym->content.value.symbol;

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
        // (up->login)(wp, AM_BAD_PASSWORD);
        return 0;
    }
    return 1;
}


int amEnableUser(char_t *name, int enable) 
{
    sym_t   *sym;
    User    *user;

    if ((sym = symLookup(users, name)) == 0) {
        return -1;
    }
    user = (User*) sym->content.value.symbol;
    user->enable = enable;
    return 0;
}


int amRemoveUser(char_t *name) 
{
    return symDelete(users, name);
}


int amAddUserRole(char_t *name, char_t *role) 
{
    sym_t   *sym;
    User    *user;

    if ((sym = symLookup(users, name)) == 0) {
        return -1;
    }
    user = (User*) sym->content.value.symbol;
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
static void computeCapabilities(User *user)
{
    ringq_t     buf;
    sym_fd_t    capabilities;
    sym_t       *sym;
    Role        *rp;
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
        rp = (Role*) sym->content.value.symbol;
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
    trace(4, "User \"%s\" has capabilities:%s\n", user->name, user->capabilities);
}


static void computeAllCapabilities()
{
    sym_t   *sym;
    User    *up;

    for (sym = symFirst(users); sym; sym = symNext(users, sym)) {
        up = (User*) sym->content.value.symbol;
        computeCapabilities(up);
    }
}


int amRemoveUserRole(char_t *name, char_t *role) 
{
    sym_t   *sym;
    User    *up;

    if ((sym = symLookup(users, name)) == 0) {
        return -1;
    }
    up = (User*) sym->content.value.symbol;
    return removeString(up->roles, role);
}


bool amUserHasCapability(char_t *name, char_t *capability) 
{
    sym_t   *sym;
    User    *up;

    if ((sym = symLookup(users, name)) == 0) {
        return -1;
    }
    up = (User*) sym->content.value.symbol;
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
    Role        *rp;
    sym_t       *sym;
    char_t      *word, *tok;

    str = gstrdup(str);
    for (word = gtok(str, T(" "), &tok); word; word = gtok(NULL, T(" "), &tok)) {
        if ((sym = symLookup(roles, word)) != 0) {
            rp = (Role*) sym->content.value.symbol;
            expandCapabilities(buf, rp->capabilities);
        } else {
            ringqPutc(buf, ' ');
            ringqPutStr(buf, word);
        }
    }
}


int amAddRole(char_t *name, char_t *capabilities)
{
    Role        *rp;
    ringq_t     buf;

    if (symLookup(roles, name)) {
        error(T("Role %s already exists"), name);
        /* Already exists */
        return -1;
    }
    if ((rp = galloc(sizeof(Role))) == 0) {
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


int amRemoveRole(char_t *name) 
{
    //  MOB - need to free full role
    return symDelete(roles, name);
}


int amAddRoleCapability(char_t *name, char_t *capability) 
{
    Role    *rp;
    sym_t   *sym;

    if ((sym = symLookup(roles, name)) == 0) {
        return -1;
    }
    rp = (Role*) sym->content.value.symbol;
    if (findString(rp->capabilities, capability)) {
        return 0;
    }
    rp->capabilities = addString(rp->capabilities, capability);
    return 0;
}


int amRemoveRoleCapability(char_t *name, char_t *capability) 
{
    sym_t   *sym;
    Role    *rp;

    if ((sym = symLookup(roles, name)) == 0) {
        return -1;
    }
    rp = (Role*) sym->content.value.symbol;
    return removeString(rp->capabilities, capability);
}


int amAddUri(char_t *uri, char_t *capabilities, char_t *loginUri, AmLogin login, AmVerify verify)
{
    //  MOB - using up as both uri and user 
    Uri     *up;
    int     i;

    for (i = 0; i < uriCount; i++) {
        up = uris[i];
        if (gmatch(up->prefix, uri)) {
            error(T("URI %s already exists"), uri);
            /* Already exists */
            return -1;
        }
    }
    if ((up = galloc(sizeof(Uri))) == 0) {
        return 0;
    }
    up->prefix = gstrdup(uri);
    up->prefixLen = glen(uri);
    up->loginUri = gstrdup(loginUri);
#if BIT_PACK_SSL
    up->secure = (capabilities && strstr(capabilities, "SECURE")) ? 1 : 0;
#endif
    /* Always have a leading and trailing space to make matching quicker */
    gfmtAlloc(&up->capabilities, -1, " %s", capabilities ? capabilities : "");
    up->login = login;
    up->verify = verify;
    up->enable = 1;
    growUris();
    uris[uriCount++] = up;
    return 0;
}


static int lookupUri(char_t *uri) 
{
    Uri     *up;
    int     i;

    for (i = 0; i < uriCount; i++) {
        up = uris[i];
        if (gmatch(up->prefix, uri)) {
            return i;
        }
    }
    return -1;
}


//  MOB - when is this called
int amRemoveUri(char_t *uri) 
{
    int     i;

    if ((i = lookupUri(uri)) == 0) {
        return -1;
    }
    for (; i < uriCount; i++) {
        uris[i] = uris[i+1];
    }
    uriCount++;
    return 0;
}


static void basicLogin(webs_t wp, int why)
{
    gfree(wp->authResponse);
    gfmtAlloc(&wp->authResponse, -1, T("Basic realm=\"%s\"\r\n"), websRealm);
    websError(wp, 401, T("Access Denied\nUser not logged in."));
}


static void formLogin(webs_t wp, int why)
{
    websRedirect(wp, wp->uri->loginUri);
}


#if BIT_DIGEST_AUTH
static void digestLogin(webs_t wp, int why)
{
    char_t  *nonce, *opaque;

    nonce = createDigestNonce(wp);
    //  MOB - opaque should be one time?  Appweb too.
    opaque = T("5ccc069c403ebaf9f0171e9517f40e41");
    gfmtAlloc(&wp->authResponse, -1,
        "Digest realm=\"%s\", domain=\"%s\", qop=\"%s\", nonce=\"%s\", opaque=\"%s\", algorithm=\"%s\", stale=\"%s\"\r\n",
        websRealm, websGetHostUrl(), T("auth"), nonce, opaque, T("MD5"), T("FALSE"));
    gfree(nonce);
    websError(wp, 401, T("Access Denied\nUser not logged in."));
}


static bool verifyDigestPassword(webs_t wp)
{
    char_t  *digest, *secret, *realm;
    time_t  when;

    /*
        Validate the nonce value - prevents replay attacks
    */
    when = 0; secret = 0; realm = 0;
    parseDigestNonce(wp->nonce, &secret, &realm, &when);

    if (!gmatch(secret, secret) || !gmatch(realm, websRealm)) {
        trace(2, T("Access denied: Nonce mismatch\n"));
        return 0;
    } else if ((when + (5 * 60)) < time(0)) {
        //  MOB - this time period should be configurable 
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



static bool verifyBasicPassword(webs_t wp)
{
    char_t  passbuf[BIT_LIMIT_PASSWORD * 3 + 3], *password;
    bool    rc;
    
    gfmtStatic(passbuf, sizeof(passbuf), "%s:%s:%s", wp->userName, websRealm, wp->password);
    password = websMD5(passbuf);
    rc = gmatch(wp->user->password, password);
    gfree(password);
    return rc;
}


//  MOB - same as verifyBasicPassword
static bool verifyFormPassword(webs_t wp)
{
    char_t  passbuf[BIT_LIMIT_PASSWORD * 3 + 3], *password;
    int     rc;

    if (!wp->user) {
        return 0;
    }
    gfmtStatic(passbuf, sizeof(passbuf), "%s:%s:%s", wp->userName, websRealm, wp->password);
    password = websMD5(passbuf);
    rc = gmatch(password, wp->user->password);
    gfree(password);
    return rc;
}


bool amVerifyUri(webs_t wp)
{
    sym_t   *sym;
    Uri     *up;
    ssize   plen, len;
    int     i;

    plen = glen(wp->path);
    for (i = 0; i < uriCount; i++) {
        up = uris[i];
        if (plen < up->prefixLen) continue;
        len = min(up->prefixLen, plen);
        if (strncmp(wp->path, up->prefix, len) == 0) {
            wp->uri = up;
            break;
        }
    }
    if (up->secure && !(wp->flags & WEBS_SECURE)) {
        websStats.access++;
        websError(wp, 405, T("Access Denied\nSecure access is required."));
        return 0;
    }
    if (gmatch(up->capabilities, " ")) {
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
            (up->login)(wp, AM_LOGIN_REQUIRED);
            return 0;
        }
        if ((sym = symLookup(users, wp->userName)) == 0) {
            (up->login)(wp, AM_BAD_USERNAME);
            return 0;
        }
        wp->user = (User*) sym->content.value.symbol;
        if (!(up->verify)(wp)) {
            (up->login)(wp, AM_BAD_PASSWORD);
            return 0;
        }
    }
    gassert(wp->user);
    return amCan(wp, up->capabilities);
}


bool amCan(webs_t wp, char_t *capability) 
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
        wp->user = (User*) sym->content.value.symbol;
    }
    return findString(wp->user->capabilities, capability) ? 1 : 0;
}


bool can(char_t *capability)
{
    //  TODO
    return 1;
} 


static char_t *findString(char_t *field, char_t *word)
{
    char_t  str[AM_MAX_WORD], *cp;

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


static int decodeBasicDetails(webs_t wp)
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
static int decodeDigestDetails(webs_t wp)
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
    if (wp->userName == 0 || wp->realm == 0 || wp->nonce == 0 || wp->uri == 0 || wp->password == 0) {
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
static char_t *createDigestNonce(webs_t wp)
{
    char_t       nonce[256];
    static int64 next = 0;

    gfmtStatic(nonce, sizeof(nonce), "%s:%s:%x:%x", secret, websRealm, time(0), next++);
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
    *when = hextoi(whenStr);
    return 0;
}


/*
   Get a Digest value using the MD5 algorithm -- See RFC 2617 to understand this code.
*/
static char_t *calcDigest(webs_t wp, char_t *userName, char_t *password)
{
    char_t  a1Buf[BIT_LIMIT_PASSWORD * 3 + 3], a2Buf[BIT_LIMIT_PASSWORD * 3 + 3], digestBuf[BIT_LIMIT_PASSWORD * 3 + 3];
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
