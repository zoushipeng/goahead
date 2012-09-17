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

#if BIT_ROUTE
/*********************************** Locals ***********************************/

static WebsRoute **routes = 0;
static int routeCount = 0;
static int routeMax = 0;

/********************************** Forwards **********************************/

static int lookupRoute(char_t *uri);
static void growRoutes();
static void freeRoute(WebsRoute *route);
static int loadRouteConfig(char_t *path);

/************************************ Code ************************************/

WebsRoute *websSelectRoute(Webs *wp)
{
    WebsRoute   *route;
    ssize       plen, len;
    int         i;

    plen = glen(wp->path);
    for (i = 0, route = 0; i < routeCount; i++) {
        route = routes[i];
        if (plen < route->prefixLen) continue;
        len = min(route->prefixLen, plen);
        if (strncmp(wp->path, route->prefix, len) == 0) {
            if ((route->flags & WEBS_ROUTE_SECURE) && !(wp->flags & WEBS_SECURE)) {
                websError(wp, 405, T("Access Denied. Secure access is required."));
                return 0;
            }
            if (wp->flags & (WEBS_PUT | WEBS_DELETE) && !(route->flags & WEBS_ROUTE_PUTDEL)) {
                continue;
            }
            return wp->route = route;
        }
    }
    return 0;
}


bool websRouteRequest(Webs *wp)
{
    WebsRoute       *route;

    if ((route = websSelectRoute(wp)) == 0) {
        if (wp->flags & (WEBS_PUT | WEBS_DELETE)) {
            websError(wp, 500, T("Can't find route support PUT|DELETE method."));
        } else {
            websError(wp, 500, T("Can't find suitable route for request."));
        }
        return 0;
    }
    return 1;
}


WebsRoute *websAddRoute(char_t *type, char_t *uri, char_t *abilities, char_t *loginPage, 
        WebsAskLogin askLogin, WebsParseAuth parseAuth, WebsVerify verify)
{
    WebsRoute   *route;
    char        *ability, *tok;

    if (lookupRoute(uri) >= 0) {
        error(T("URI %s already exists"), uri);
        return 0;
    }
    if ((route = galloc(sizeof(WebsRoute))) == 0) {
        return 0;
    }
    memset(route, 0, sizeof(WebsRoute));
    route->prefix = gstrdup(uri);
    route->prefixLen = glen(uri);
    if (loginPage) {
        route->loginPage = gstrdup(loginPage);
    }
    route->askLogin = askLogin;
    route->parseAuth = parseAuth;
    route->verify = verify;

    if (type) {
        route->authType = gstrdup(type);
        if ((route->abilities = symOpen(WEBS_SMALL_HASH)) < 0) {
            return 0;
        }
        abilities = gstrdup(abilities);
        for (ability = gtok(abilities, T(" "), &tok); ability; ability = gtok(NULL, T(" "), &tok)) {
            if (strcmp(ability, "SECURE") == 0) {
                route->flags |= WEBS_ROUTE_SECURE;
            } else if (strcmp(ability, "PUTDEL") == 0) {
                route->flags |= WEBS_ROUTE_PUTDEL;
            } else if (strcmp(ability, "none") == 0) {
                continue;
            } else {
                if (symEnter(route->abilities, ability, valueInteger(0), 0) == 0) {
                    gfree(abilities);
                    return 0;
                }
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


static int lookupRoute(char_t *uri) 
{
    WebsRoute   *route;
    int         i;

    for (i = 0; i < routeCount; i++) {
        route = routes[i];
        if (gmatch(route->prefix, uri)) {
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


int websRemoveRoute(char_t *uri) 
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


int websOpenRoute(char_t *path) 
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


static int loadRouteConfig(char_t *path)
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
        if (gmatch(kind, "uri")) {
            type = strtok(NULL, " \t:");
            uri = strtok(NULL, " \t:\r\n");
            abilities = strtok(NULL, " \t:\r\n");
            redirect = strtok(NULL, " \t\r\n");
            askLogin = 0;
            parseAuth = 0;
#if BIT_PAM
            verify = websVerifyPamUser;
#else
            verify = websVerifyUser;
#endif
#if BIT_AUTH
            if (!redirect) {
                redirect = "/";
            }
            if (gmatch(type, "basic")) {
                askLogin = websBasicLogin;
                parseAuth = websParseBasicDetails;
#if BIT_DIGEST
            } else if (gmatch(type, "digest")) {
                askLogin = websDigestLogin;
                parseAuth = websParseDigestDetails;
#endif
            } else if (gmatch(type, "post")) {
                if (lookupRoute("/form/login") < 0) {
                    if ((route = websAddRoute(0, "/form/login", 0, redirect, 0, 0, verify)) == 0) {
                        return -1;
                    }
                    route->loggedInPage = gstrdup(uri);
                }
                if (lookupRoute("/form/logout") < 0 && !websAddRoute(0, "/form/logout", 0, redirect, 0, 0, verify)) {
                    return -1;
                }
                askLogin = websPostLogin;
            } else {
                type = 0;
            }
#endif
            if (websAddRoute(type, uri, abilities, redirect, askLogin, parseAuth, verify) < 0) {
                return -1;
            }
#if BIT_AUTH
        } else if (gmatch(kind, "user")) {
            char *name = strtok(NULL, " \t:");
            char *password = strtok(NULL, " \t:");
            char *roles = strtok(NULL, "\r\n");
            if (websAddUser(name, password, roles) == 0) {
                return -1;
            }
        } else if (gmatch(kind, "role")) {
            char *name = strtok(NULL, " \t:");
            abilities = strtok(NULL, "\r\n");
            if (websAddRole(name, abilities) < 0) {
                return -1;
            }
#endif
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
#if BIT_AUTH
    websComputeAllUserAbilities();
#endif
    return 0;
}


int websSaveRoute(char_t *path)
{
    //  MOB TODO
    return 0;
}

#endif /* BIT_ROUTE */

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
