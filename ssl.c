/*
    ssl.c -- SSL Layer

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/********************************* Includes ***********************************/

#include "goahead.h"

#if BIT_PACK_SSL
/******************************************************************************/

//  MOB - can we just remove this file and rely on matrixssl, openssl, mocana
int websSSLOpen()
{
    if (sslOpen() < 0) {
        return -1;
    }
    return 0;
}


void websSSLClose()
{
    sslClose();
}


int websSSLUpgrade(Webs *wp)
{
    return sslUpgrade(wp);
}


ssize websSSLRead(Webs *wp, char_t *buf, ssize len)
{
    return sslRead(wp, buf, len);
}


#if UNUSED
/*
      Handler for SSL Read Events
 */
static void sslReadEvent(Webs *wp)
{
#if MOB
    socket_t    *sp;
    sslConn_t   *sslConn;
    int         ret, sock, resume;
    
    gassert (wp);
    gassert(websValid(wp));
    sp = socketPtr(wp->sid);
    gassert(sp);
    
    sock = sp->sock;
    //  What do do about this
    if (wp->wsp == NULL) {
        resume = 0;
    } else {
        resume = 1;
        sslConn = (wp->wsp)->sslConn;
    }

    /*
        This handler is essentially two back-to-back calls to sslRead.  The first is here in sslAccept where the
        handshake is to take place.  The second is in websReadEvent below where it is expected the client was contacting
        us to send an HTTP request and we need to read that off.
        
        The introduction of non-blocking sockets has made this a little more confusing becuase it is possible to return
        from either of these sslRead calls in a WOULDBLOCK state.  It doesn't ultimately matter, however, because
        sslRead is fully stateful so it doesn't matter how/when it gets invoked later (as long as the correct sslConn is
        used (which is what the resume variable is all about))
    */
    if (sslAccept(&sslConn, sock, sslKeys, resume, NULL) < 0) {
        websTimeoutCancel(wp);
        socketCloseConnection(wp->sid);
        websFree(wp);
        return;
    }
    if (resume == 0) {
        (wp->wsp)->sslConn = sslConn;
    }
    websReadEvent(wp);
#else
    if (wp->flags & WEBS_ACCEPTED) {
        websReadEvent(wp);
    } else {
        if (sslAccept(wp) < 0) {
            websError(wp, 400 | WEBS_CLOSE, T("Accept error"));
            return;
        }
        wp->flags |= WEBS_ACCEPTED;
    }
#endif
}


/*
    The webs socket handler.  Called in response to I/O. We just pass control to the relevant read or write handler. A
    pointer to the webs structure is passed as a (void *) in iwp.
 */
void websSSLSocketEvent(int sid, int mask, void *iwp)
{
    Webs      *wp;
    
    wp = (Webs*) iwp;
    gassert(wp);
    
    if (! websValid(wp)) {
        return;
    }
    if (mask & SOCKET_READABLE) {
        sslReadEvent(wp);
    }
    if (mask & SOCKET_WRITABLE) {
        if (wp->writable) {
            (*wp->writable)(wp);
        }
    }
}
#endif


ssize websSSLWrite(Webs *wp, char_t *buf, ssize len)
{
    return sslWrite(wp, buf, len);
}


int websSSLEof(Webs *wp)
{
    return socketEof(wp->sid);
}


//  MOB - do we need this. Should it be return int?
void websSSLFlush(Webs *wp)
{
    sslFlush(wp);
}


void websSSLFree(Webs *wp)
{
    sslFree(wp);
}


#endif /* BIT_PACK_SSL */

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
