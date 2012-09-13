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

#include    "goahead.h"

#if BIT_HAS_PAM && BIT_PAM
 #include    <security/pam_appl.h>
#endif

#if BIT_AUTH
/*********************************** Locals ***********************************/

static WebsHash users = -1;
static WebsHash roles = -1;
static char_t *secret;
static int autoLogin = BIT_AUTO_LOGIN;

#if BIT_HAS_PAM && BIT_PAM
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

static void computeAbilities(WebsHash abilities, char *role);
static void computeUserAbilities(WebsUser *user);
static WebsUser *createUser(char_t *username, char_t *password, char_t *roles);
static void freeRole(WebsRole *rp);
static void freeUser(WebsUser *up);
static void logoutServiceProc(Webs *wp);
static void loginServiceProc(Webs *wp);

#if BIT_JAVASCRIPT
static int jsCan(int jsid, Webs *wp, int argc, char_t **argv);
#endif
#if BIT_DIGEST
static char_t *calcDigest(Webs *wp, char_t *username, char_t *password);
static char_t *createDigestNonce(Webs *wp);
static int parseDigestNonce(char_t *nonce, char_t **secret, char_t **realm, time_t *when);
#endif

#if BIT_HAS_PAM && BIT_PAM
static int pamChat(int msgCount, const struct pam_message **msg, struct pam_response **resp, void *data);
#endif

/************************************ Code ************************************/

bool websAuthenticate(Webs *wp)
{
    WebsRoute       *route;
    int             cached;

    route = wp->route;

    if (!route->authType || autoLogin) {
        /* Authentication not required */
        return 1;
    }
    cached = 0;
#if BIT_SESSIONS
    if (wp->cookie && websGetSession(wp, 0) != 0) {
        char    *username;
        /*
            Retrieve authentication state from the session storage. Faster than re-authenticating.
         */
        if ((username = (char*) websGetSessionVar(wp, WEBS_SESSION_USERNAME, 0)) != 0) {
            cached = 1;
            wp->username = gstrdup(username);
        }
    }
#endif
    if (!cached) {
        if (wp->authType && !gcaselessmatch(wp->authType, route->authType)) {
            websError(wp, HTTP_CODE_BAD_REQUEST, "Access denied. Wrong authentication protocol type.");
            return 0;
        }
        if (wp->authDetails) {
            if (!(route->parseAuth)(wp)) {
                return 0;
            }
        }
        if (!wp->username || !*wp->username) {
            (route->askLogin)(wp);
            return 0;
        }
        if (!(route->verify)(wp)) {
            (route->askLogin)(wp);
            return 0;
        }
#if BIT_SESSIONS
        /*
            Store authentication state and user in session storage                                         
        */                                                                                                
        if (websGetSession(wp, 1) != 0) {                                                    
            websSetSessionVar(wp, WEBS_SESSION_USERNAME, wp->username);                                
        }
#endif
    }
    gassert(wp->user);
    if (!websCanUser(wp, route->abilities)) {
        websError(wp, HTTP_CODE_FORBIDDEN, "Access denied. User does not have the required capabilities.");
        return 0;
    }
    return 1;
}


int websOpenAuth() 
{
    char    sbuf[64];
    
    if ((users = symOpen(WEBS_SMALL_HASH)) < 0) {
        return -1;
    }
    if ((roles = symOpen(WEBS_SMALL_HASH)) < 0) {
        return -1;
    }
    gfmtStatic(sbuf, sizeof(sbuf), "%x:%x", rand(), time(0));
    secret = websMD5(sbuf);
#if BIT_JAVASCRIPT
    websJsDefine(T("can"), jsCan);
#endif
    websFormDefine("login", loginServiceProc);
    websFormDefine("logout", logoutServiceProc);
    return 0;
}


void websCloseAuth() 
{
    WebsKey     *key;

    gfree(secret);
    for (key = symFirst(users); key; key = symNext(users, key)) {
        freeUser(key->content.value.symbol);
    }
    symClose(users);
    for (key = symFirst(roles); key; key = symNext(roles, key)) {
        freeRole(key->content.value.symbol);
    }
    symClose(roles);
    users = roles = -1;
}


static WebsUser *createUser(char_t *username, char_t *password, char_t *roles)
{
    WebsUser    *user;

    if ((user = galloc(sizeof(WebsUser))) == 0) {
        return 0;
    }
    user->name = gstrdup(username);
    if (roles) {
        gfmtAlloc(&user->roles, -1, " %s ", roles);
    } else {
        user->roles = gstrdup(T(" "));
    }
    user->password = gstrdup(password);
    return user;
}


int websAddUser(char_t *username, char_t *password, char_t *roles)
{
    WebsUser    *user;

    if (websLookupUser(username)) {
        error(T("User %s already exists"), username);
        /* Already exists */
        return -1;
    }
    if ((user = createUser(username, password, roles)) == 0) {
        return 0;
    }
    if (symEnter(users, username, valueSymbol(user), 0) == 0) {
        return -1;
    }
    return 0;
}


int websRemoveUser(char_t *username) 
{
    return symDelete(users, username);
}


static void freeUser(WebsUser *up)
{
    symClose(up->abilities);
    gfree(up->name);
    gfree(up->password);
    gfree(up->roles);
    gfree(up);
}


int websSetUserRoles(char_t *username, char_t *roles)
{
    WebsUser    *user;

    if ((user = websLookupUser(username)) == 0) {
        return -1;
    }
    gfree(user->roles);
    user->roles = gstrdup(roles);
    computeUserAbilities(user);
    return 0;
}


WebsUser *websLookupUser(char *username)
{
    WebsKey     *key;

    if ((key = symLookup(users, username)) == 0) {
        return 0;
    }
    return (WebsUser*) key->content.value.symbol;
}


static void computeAbilities(WebsHash abilities, char *role)
{
    WebsRole    *rp;
    WebsKey     *key;

    if ((key = symLookup(roles, role)) != 0) {
        rp = (WebsRole*) key->content.value.symbol;
        for (key = symFirst(rp->abilities); key; key = symNext(rp->abilities, key)) {
            if (!symLookup(abilities, key->name.value.string)) {
                symEnter(abilities, key->name.value.string, valueInteger(0), 0);
            }
        }
    } else {
        symEnter(abilities, role, valueInteger(0), 0);
    }
}


static void computeUserAbilities(WebsUser *user)
{
    char        *ability, *roles, *tok;

    if ((user->abilities = symOpen(WEBS_SMALL_HASH)) == 0) {
        return;
    }
    roles = gstrdup(user->roles);
    for (ability = gtok(roles, T(" "), &tok); ability; ability = gtok(NULL, T(" "), &tok)) {
        computeAbilities(user->abilities, ability);
    }
#if BIT_DEBUG
    {
        WebsKey *key;
        trace(2, "User \"%s\" has abilities: ", user->name);
        for (key = symFirst(user->abilities); key; key = symNext(user->abilities, key)) {
            trace(2, "%s ", key->name.value.string);
            ability = key->name.value.string;
        }
        trace(2, "\n");
    }
#endif
    gfree(roles);
}


void websComputeAllUserAbilities()
{
    WebsUser    *user;
    WebsKey     *sym;

    for (sym = symFirst(users); sym; sym = symNext(users, sym)) {
        user = (WebsUser*) sym->content.value.symbol;
        computeUserAbilities(user);
    }
}


bool websCanUser(Webs *wp, WebsHash abilities) 
{
    WebsKey     *key;
    char        *ability;

    if (!wp->user) {
        if (!wp->username) {
            return 0;
        }
        if ((wp->user = websLookupUser(wp->username)) == 0) {
            trace(2, T("Can't find user %s\n"), wp->username);
            return 0;
        }
    }
    for (key = symFirst(abilities); key; key = symNext(abilities, key)) {
        ability = key->name.value.string;
        if (symLookup(wp->user->abilities, ability) == 0) {
            return 0;
        }
    }
    return 1;
}


bool websCanUserString(Webs *wp, char *abilities) 
{
    WebsUser    *user;
    char        *ability, *tok;

    if (!wp->user) {
        if (!wp->username) {
            return 0;
        }
        if ((user = websLookupUser(wp->username)) == 0) {
            trace(2, T("Can't find user %s\n"), wp->username);
            return 0;
        }
    }
    abilities = gstrdup(abilities);
    for (ability = gtok(abilities, T(" "), &tok); ability; ability = gtok(NULL, T(" "), &tok)) {
        if (symLookup(wp->user->abilities, ability) == 0) {
            gfree(abilities);
            return 0;
        }
    }
    gfree(abilities);
    return 1;
}


#if UNUSED
bool websUserHasAbility(Webs *wp, char_t *ability) 
{
    WebsUser    *user;

    if ((user = websLookupUser(wp->username)) == 0) {
        return -1;
    }
    return symLookup(user->abilities, ability) ? 1 : 0; 
}
#endif


int websAddRole(char_t *name, char_t *abilities)
{
    WebsRole    *rp;
    char        *ability, *tok;

    if (symLookup(roles, name)) {
        error(T("Role %s already exists"), name);
        /* Already exists */
        return -1;
    }
    if ((rp = galloc(sizeof(WebsRole))) == 0) {
        return -1;
    }
    if ((rp->abilities = symOpen(WEBS_SMALL_HASH)) < 0) {
        return -1;
    }
    trace(5, T("Role \"%s\" has abilities:%s\n"), name, abilities);
    abilities = gstrdup(abilities);
    for (ability = gtok(abilities, T(" "), &tok); ability; ability = gtok(NULL, T(" "), &tok)) {
        if (symEnter(rp->abilities, ability, valueInteger(0), 0) == 0) {
            gfree(abilities);
            return -1;
        }
    }
    if (symEnter(roles, name, valueSymbol(rp), 0) == 0) {
        gfree(abilities);
        return -1;
    }
    gfree(abilities);
    return 0;
}


static void freeRole(WebsRole *rp)
{
    symClose(rp->abilities);
    gfree(rp);
}


/*
    Remove a role. Does not recompute abilities for users that use this role
 */
int websRemoveRole(char_t *name) 
{
    WebsRole    *rp;
    WebsKey       *sym;

    if ((sym = symLookup(roles, name)) == 0) {
        return -1;
    }
    rp = sym->content.value.symbol;
    symClose(rp->abilities);
    gfree(rp);
    return symDelete(roles, name);
}


bool websLoginUser(Webs *wp, char_t *username, char_t *password)
{
    gassert(wp->route);
    if (!wp->route || !wp->route->verify) {
        return 0;
    }
    gfree(wp->username);
    wp->username = gstrdup(username);
    gfree(wp->password);
    wp->password = gstrdup(password);

    if (!(wp->route->verify)(wp)) {
        trace(2, T("Password does not match\n"));
        return 0;
    }
#if BIT_SESSIONS
    websSetSessionVar(wp, WEBS_SESSION_USERNAME, wp->username);                                
#endif
    return 1;
}


/*
    Internal login service routine for Post-based auth
 */
static void loginServiceProc(Webs *wp)
{
    WebsRoute   *route;

    if (!(wp->flags & WEBS_POST)) {
        websError(wp, 401, T("Login must be invoked with a post request."));
        return;
    }
    route = wp->route;
    gassert(route);
    
    if (websLoginUser(wp, websGetVar(wp, "username", 0), websGetVar(wp, "password", 0))) {
#if BIT_SESSIONS
        char *referrer;
        if ((referrer = websGetSessionVar(wp, "referrer", 0)) != 0) {
            websRedirect(wp, referrer);
        } else
#endif
            websRedirect(wp, route->loggedInPage);
    } else {
        websRedirect(wp, route->loginPage);
    }
}


static void logoutServiceProc(Webs *wp)
{
    if (!(wp->flags & WEBS_POST)) {
        websError(wp, 401, T("Logout must be invoked with a post request."));
        return;
    }
#if BIT_SESSIONS
    websRemoveSessionVar(wp, WEBS_SESSION_USERNAME);
#endif
    websRedirect(wp, wp->route->loginPage);
}


void websBasicLogin(Webs *wp)
{
    gassert(wp->route);
    gfree(wp->authResponse);
    gfmtAlloc(&wp->authResponse, -1, T("Basic realm=\"%s\""), BIT_REALM);
    websError(wp, 401, T("Access Denied. User not logged in."));
}


void websPostLogin(Webs *wp)
{
    websRedirect(wp, wp->route->loginPage);
}


#if BIT_DIGEST
void websDigestLogin(Webs *wp)
{
    char_t  *nonce, *opaque;

    gassert(wp->route);
    nonce = createDigestNonce(wp);
    /* Opaque is unused. Set to anything */
    opaque = T("5ccc069c403ebaf9f0171e9517f40e41");
    gfmtAlloc(&wp->authResponse, -1,
        "Digest realm=\"%s\", domain=\"%s\", qop=\"%s\", nonce=\"%s\", opaque=\"%s\", algorithm=\"%s\", stale=\"%s\"",
        BIT_REALM, websGetHostUrl(), T("auth"), nonce, opaque, T("MD5"), T("FALSE"));
    gfree(nonce);
    websError(wp, 401, T("Access Denied. User not logged in."));
}
#endif


bool websVerifyUser(Webs *wp)
{
    char_t      passbuf[BIT_LIMIT_PASSWORD * 3 + 3];
    bool        success;

    if (!wp->encoded) {
        gfmtStatic(passbuf, sizeof(passbuf), "%s:%s:%s", wp->username, BIT_REALM, wp->password);
        gfree(wp->password);
        wp->password = websMD5(passbuf);
        wp->encoded = 1;
    }
    if (!wp->user && (wp->user = websLookupUser(wp->username)) == 0) {
        trace(5, "verifyUser: Unknown user \"%s\"", wp->username);
        return 0;
    }
    /*
        Verify the password
     */
    if (wp->digest) {
        success = gmatch(wp->password, wp->digest);
    } else {
        success = gmatch(wp->password, wp->user->password);
    }
    if (success) {
        trace(5, "User \"%s\" authenticated", wp->username);
    } else {
        trace(5, "Password for user \"%s\" failed to authenticate", wp->username);
    }
    return success;
}


#if BIT_JAVASCRIPT
static int jsCan(int jsid, Webs *wp, int argc, char_t **argv)
{
    if (websCanUserString(wp, argv[0])) {
        //  MOB - how to set return 
        return 0;
    }
    return 1;
} 
#endif


bool websParseBasicDetails(Webs *wp)
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
        wp->username = gstrdup(userAuth);
        wp->password = gstrdup(cp);
        wp->encoded = 0;
    } else {
        wp->username = gstrdup(T(""));
        wp->password = gstrdup(T(""));
    }
    gfree(userAuth);
    return 1;
}


#if BIT_DIGEST
bool websParseDigestDetails(Webs *wp)
{
    time_t  when;
    char    *value, *tok, *key, *dp, *sp, *secret, *realm;
    int     seenComma;

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
                /* Store the response digest in the password field. This is MD5(user:realm:password) */
                wp->password = gstrdup(value);
                wp->encoded = 1;
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
                wp->username = gstrdup(value);
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
        wp->qop = gstrdup("");
    }
    /*
        Validate the nonce value - prevents replay attacks
     */
    when = 0; secret = 0; realm = 0;
    parseDigestNonce(wp->nonce, &secret, &realm, &when);
    if (!gmatch(secret, secret)) {
        trace(2, T("Access denied: Nonce mismatch\n"));
        return 0;
    } else if (!gmatch(realm, BIT_REALM)) {
        trace(2, T("Access denied: Realm mismatch\n"));
        return 0;
    } else if (!gmatch(wp->qop, "auth")) {
        trace(2, T("Access denied: Bad qop\n"));
        return 0;
    } else if ((when + (5 * 60)) < time(0)) {
        trace(2, T("Access denied: Nonce is stale\n"));
        return 0;
    }
    if (!wp->user) {
        if ((wp->user = websLookupUser(wp->username)) == 0) {
            trace(2, T("Access denied: user is unknown\n"));
            return 0;
        }
    }
    wp->digest = calcDigest(wp, 0, wp->user->password);
    return 1;
}


/*
    Create a nonce value for digest authentication (RFC 2617)
 */
static char_t *createDigestNonce(Webs *wp)
{
    char_t       nonce[256];
    static int64 next = 0;

    gassert(wp->route);
    gfmtStatic(nonce, sizeof(nonce), "%s:%s:%x:%x", secret, BIT_REALM, time(0), next++);
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
static char_t *calcDigest(Webs *wp, char_t *username, char_t *password)
{
    char_t  a1Buf[256], a2Buf[256], digestBuf[256];
    char_t  *ha1, *ha2, *method, *result;

    /*
        Compute HA1. If username == 0, then the password is already expected to be in the HA1 format 
        (MD5(username:realm:password).
     */
    if (username == 0) {
        ha1 = gstrdup(password);
    } else {
        gfmtStatic(a1Buf, sizeof(a1Buf), "%s:%s:%s", username, wp->realm, password);
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
#endif /* BIT_DIGEST */


#if BIT_HAS_PAM && BIT_PAM
bool websVerifyPamUser(Webs *wp)
{
    ringq_t             abilities;
    pam_handle_t        *pamh;
    UserInfo            info;
    struct pam_conv     conv = { pamChat, &info };
    struct group        *gp;
    int                 res, i;
   
    gassert(wp->username);
    gassert(wp->password);
    gassert(!wp->encoded);

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
            ringqOpen(&abilities, 128, -1);
            for (i = 0; i < ngroups; i++) {
                if ((gp = getgrgid(groups[i])) != 0) {
                    ringqPutStr(&abilities, gp->gr_name);
                    ringqPutc(&abilities, ' ');
                }
            }
            ringqAddNull(&abilities);
            trace(5, "Create temp user \"%s\" with abilities: %s", wp->username, abilities.servp);
            wp->user = createUser(wp->username, 0, abilities.servp);
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
            reply[i].resp = strdup(info->name);
            break;

        case PAM_PROMPT_ECHO_OFF:
            /* Retrieve the user password and pass onto pam */
            reply[i].resp = strdup(info->password);
            break;

        default:
            free(reply);
            return PAM_CONV_ERR;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;
}
#endif /* BIT_HAS_PAM */

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
