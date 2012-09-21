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

#include    "goahead.h"

/*********************************** Locals ***********************************/

static WebsRoute **routes = 0;
static int routeCount = 0;
static int routeMax = 0;

/********************************** Forwards **********************************/

static int lookupRoute(char *uri);
static void growRoutes();
static void freeRoute(WebsRoute *route);
static int loadRouteConfig(char *path);

/************************************ Code ************************************/

WebsRoute *websSelectRoute(Webs *wp)
{
    WebsRoute   *route;
    ssize       plen, len;
    int         i;

    plen = slen(wp->path);
    for (i = 0, route = 0; i < routeCount; i++) {
        route = routes[i];
        if (plen < route->prefixLen) continue;
        len = min(route->prefixLen, plen);
        if (strncmp(wp->path, route->prefix, len) == 0) {
            return route;
        }
    }
    websError(wp, 500, "Can't find suitable route for request.");
    return 0;
}


bool websRouteRequest(Webs *wp)
{
    return (wp->route = websSelectRoute(wp)) != 0;
}


static bool can(Webs *wp, char *ability)
{
    if (wp->user && symLookup(wp->user->abilities, ability)) {
        return 1;
    }
    if (smatch(ability, "SECURE")) {
        if (wp->flags & WEBS_SECURE) {
            return 1;
        }
        /* Secure is a special ability and is always mandatory if specified */
        websError(wp, 405, "Access Denied. Secure access is required.");
        return 0;
    }
    if (smatch(ability, wp->method)) {
        return 1;
    }
    return 0;
}


bool websCan(Webs *wp, WebsHash abilities) 
{
    WebsKey     *key;
    char        *ability, *cp, *start, abuf[BIT_LIMIT_STRING];

    if (!wp->user) {
        if (wp->authType) {
            if (!wp->username) {
                websError(wp, 401, "Access Denied. User not logged in.");
                return 0;
            }
            if ((wp->user = websLookupUser(wp->username)) == 0) {
                websError(wp, 401, "Access Denied. Unknown user.");
                return 0;
            }
        }
    }
    if (abilities >= 0) {
        if (!wp->user && wp->username) {
            wp->user = websLookupUser(wp->username);
        }
        gassert(abilities);
        for (key = symFirst(abilities); key; key = symNext(abilities, key)) {
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
                    if (websComplete(wp)) {
                        return 0;
                    }
                    start = &cp[1];
                } while ((cp = strchr(start, '|')) != 0);
                if (!cp) {
                    websError(wp, 401, "Access Denied. Insufficient capabilities.");
                    return 0;
                }
            } else if (!can(wp, ability)) {
                websError(wp, 401, "Access Denied. Insufficient capabilities.");
                return 0;
            }
        }
    }
    return 1;
}


//  MOB fix
bool websCanString(Webs *wp, char *abilities) 
{
    WebsUser    *user;
    char        *ability, *tok;

    if (!wp->user) {
        if (!wp->username) {
            return 0;
        }
        if ((user = websLookupUser(wp->username)) == 0) {
            trace(2, "Can't find user %s\n", wp->username);
            return 0;
        }
    }
    abilities = strdup(abilities);
    for (ability = stok(abilities, " \t,", &tok); ability; ability = stok(NULL, " \t,", &tok)) {
        if (symLookup(wp->user->abilities, ability) == 0) {
            gfree(abilities);
            return 0;
        }
    }
    gfree(abilities);
    return 1;
}


WebsRoute *websAddRoute(char *type, char *uri, char *abilities, char *loginPage, 
        WebsAskLogin askLogin, WebsParseAuth parseAuth, WebsVerify verify)
{
    WebsRoute   *route;
    char        *ability, *tok;

    if (lookupRoute(uri) >= 0) {
        error("URI %s already exists", uri);
        return 0;
    }
    if ((route = galloc(sizeof(WebsRoute))) == 0) {
        return 0;
    }
    memset(route, 0, sizeof(WebsRoute));
    route->abilities = -1;
    route->prefix = strdup(uri);
    route->prefixLen = slen(uri);
    if (loginPage) {
        route->loginPage = strdup(loginPage);
    }
    route->askLogin = askLogin;
    route->parseAuth = parseAuth;
    route->verify = verify;

    if (type) {
        route->authType = strdup(type);
    }
    if (abilities && *abilities) {
        if ((route->abilities = symOpen(WEBS_SMALL_HASH)) < 0) {
            return 0;
        }
        abilities = strdup(abilities);
        for (ability = stok(abilities, " \t,|", &tok); ability; ability = stok(NULL, " \t,", &tok)) {
            if (strcmp(ability, "none") == 0) {
                continue;
            }
            if (symEnter(route->abilities, ability, valueInteger(0), 0) == 0) {
                gfree(abilities);
                return 0;
            }
        }
        gfree(abilities);
    }
    growRoutes();
    routes[routeCount++] = route;
    return route;
}


static void growRoutes()
{
    if (routeCount >= routeMax) {
        routeMax += 16;
        //  RC
        routes = grealloc(routes, sizeof(WebsRoute*) * routeMax);
    }
}


static int lookupRoute(char *uri) 
{
    WebsRoute   *route;
    int         i;

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
    symClose(route->abilities);
    gfree(route->prefix);
    gfree(route->loginPage);
    gfree(route->loggedInPage);
    gfree(route);
}


int websRemoveRoute(char *uri) 
{
    int         i;

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


int websOpenRoute(char *path) 
{
    if (path) {
        return loadRouteConfig(path);
    }
    return 0;
}


void websCloseRoute() 
{
    gfree(routes);
    routes = 0;
    routeCount = routeMax = 0;
}


static int loadRouteConfig(char *path)
{
    WebsParseAuth parseAuth;
    WebsAskLogin  askLogin;
    WebsVerify    verify;
    WebsRoute     *route;
    FILE          *fp;
    char          buf[512], *type, *uri, *abilities, *line, *kind, *redirect;
    int           i;
    
    redirect = 0;

    //  MOB - don't use fopen for ROM
    if ((fp = fopen(path, "rt")) == 0) {
        error("Can't open route config file %s", path);
        return -1;
    }
    buf[sizeof(buf) - 1] = '\0';
    while ((line = fgets(buf, sizeof(buf) -1, fp)) != 0) {
        kind = strtok(buf, " \t:\r\n");
        // for (cp = kind; cp && isspace((uchar) *cp); cp++) { }
        if (kind == 0 || *kind == '\0' || *kind == '#') {
            continue;
        }
        if (smatch(kind, "uri")) {
            type = strtok(NULL, " \t:");
            uri = strtok(NULL, " \t:\r\n");
            abilities = strtok(NULL, ":\r\n");
            redirect = strtok(NULL, " \t\r\n");
            askLogin = 0;
            parseAuth = 0;
#if BIT_PAM
            verify = websVerifyPamUser;
#else
            verify = websVerifyUser;
#endif
            if (!redirect) {
                redirect = "/";
            }
            if (smatch(type, "basic")) {
                askLogin = websBasicLogin;
                parseAuth = websParseBasicDetails;
#if BIT_DIGEST
            } else if (smatch(type, "digest")) {
                askLogin = websDigestLogin;
                parseAuth = websParseDigestDetails;
#endif
            } else if (smatch(type, "form")) {
                if (lookupRoute("/proc/login") < 0) {
                    if ((route = websAddRoute(0, "/proc/login", 0, redirect, 0, 0, verify)) == 0) {
                        return -1;
                    }
                    route->loggedInPage = strdup(uri);
                }
                if (lookupRoute("/proc/logout") < 0 && 
                        !websAddRoute(0, "/proc/logout", "POST", redirect, 0, 0, verify)) {
                    return -1;
                }
                askLogin = websFormLogin;
            } else {
                type = 0;
            }
            if (websAddRoute(type, uri, abilities, redirect, askLogin, parseAuth, verify) < 0) {
                return -1;
            }
        } else if (smatch(kind, "user")) {
            char *name = strtok(NULL, " \t:");
            char *password = strtok(NULL, " \t:");
            char *roles = strtok(NULL, "\r\n");
            if (websAddUser(name, password, roles) == 0) {
                return -1;
            }
        } else if (smatch(kind, "role")) {
            char *name = strtok(NULL, " \t:");
            abilities = strtok(NULL, "\r\n");
            if (websAddRole(name, abilities) < 0) {
                return -1;
            }
        }
    }
    fclose(fp);
    /*
        Ensure there is a route for "/", if not, create it.
     */
    for (i = 0, route = 0; i < routeCount; i++) {
        route = routes[i];
        if (strcmp(route->prefix, "/") == 0) {
            break;
        }
    }
    if (i >= routeCount) {
        websAddRoute(0, 0, "/", 0, 0, 0, 0);
    }
    websComputeAllUserAbilities();
    return 0;
}


int websSaveRoute(char *path)
{
    //  MOB TODO
    return 0;
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
