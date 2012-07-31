/*
    rom.c -- Support for ROMed page retrieval.

    This module provides web page retrieval from compiled web pages. Use the webcomp program to compile web pages and
    link into the GoAhead WebServer.  This module uses a hashed symbol table for fast page lookup.
  
    Usage: webcomp -f webPageFileList -p Prefix >webrom.c

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    <stdlib.h>

#include    "wsIntrn.h"

/******************************** Local Data **********************************/

#if BIT_ROM

sym_fd_t    romTab;                     /* Symbol table for web pages */

/*********************************** Code *************************************/

int websRomOpen()
{
    websRomPageIndexType    *wip;
    int                     nchars;
    char_t                  name[SYM_MAX];

    romTab = symOpen(WEBS_SYM_INIT);

    for (wip = websRomPageIndex; wip->path; wip++) {
        gstrncpy(name, wip->path, SYM_MAX);
        nchars = gstrlen(name) - 1;
        if (nchars > 0 &&
            (name[nchars] == '/' || name[nchars] == '\\')) {
            name[nchars] = '\0';
        }
        symEnter(romTab, name, valueInteger((int) wip), 0);
    }
    return 0;
}

void websRomClose()
{
    symClose(romTab);
}


int websRomPageOpen(webs_t wp, char_t *path, int mode, int perm)
{
    websRomPageIndexType    *wip;
    sym_t                   *sp;

    a_assert(websValid(wp));
    a_assert(path && *path);

    if ((sp = symLookup(romTab, path)) == NULL) {
        return -1;
    }
    wip = (websRomPageIndexType*) sp->content.value.integer;
    wip->pos = 0;
    return (wp->docfd = wip - websRomPageIndex);
}


void websRomPageClose(int fd)
{
}


int websRomPageStat(char_t *path, websStatType *sbuf)
{
    websRomPageIndexType    *wip;
    sym_t                   *sp;

    a_assert(path && *path);

    if ((sp = symLookup(romTab, path)) == NULL) {
        return -1;
    }
    wip = (websRomPageIndexType*) sp->content.value.integer;

    memset(sbuf, 0, sizeof(websStatType));
    sbuf->size = wip->size;
    if (wip->page == NULL) {
        sbuf->isDir = 1;
    }
    return 0;
}


int websRomPageReadData(webs_t wp, char *buf, int nBytes)
{
    websRomPageIndexType    *wip;
    int                     len;

    a_assert(websValid(wp));
    a_assert(buf);
    a_assert(wp->docfd >= 0);

    wip = &websRomPageIndex[wp->docfd];

    len = min(wip->size - wip->pos, nBytes);
    memcpy(buf, &wip->page[wip->pos], len);
    wip->pos += len;
    return len;
}


long websRomPageSeek(webs_t wp, long offset, int origin)
{
    websRomPageIndexType    *wip;
    long pos;

    a_assert(websValid(wp));
    a_assert(origin == SEEK_SET || origin == SEEK_CUR || origin == SEEK_END);
    a_assert(wp->docfd >= 0);

    wip = &websRomPageIndex[wp->docfd];

    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) {
        errno = EINVAL;
        return -1;
    }
    if (wp->docfd < 0) {
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
}

#endif /* WEBS_PAGE_ROM */

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
