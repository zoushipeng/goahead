/*
    rom.c -- Support for ROMed page retrieval.

    This module provides web page retrieval from compiled web pages. Use the webcomp program to compile web pages and
    link into the GoAhead WebServer.  This module uses a hashed symbol table for fast page lookup.
  
    Usage: webcomp -f webPageFileList -p Prefix >webrom.c

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/******************************** Local Data **********************************/

#if BIT_ROM
WebsHash    romTab;                     /* Symbol table for web pages */
#endif

/*********************************** Code *************************************/

PUBLIC int websRomOpen()
{
#if BIT_ROM
    WebsRomIndex    *wip;
    char            name[BIT_GOAHEAD_LIMIT_FILENAME];
    ssize           len;

    romTab = hashCreate(WEBS_HASH_INIT);
    for (wip = websRomIndex; wip->path; wip++) {
        strncpy(name, wip->path, BIT_GOAHEAD_LIMIT_FILENAME);
        len = strlen(name) - 1;
        if (len > 0 &&
            (name[len] == '/' || name[len] == '\\')) {
            name[len] = '\0';
        }
        hashEnter(romTab, name, valueSymbol(wip), 0);
    }
#endif
    return 0;
}


PUBLIC void websRomClose()
{
#if BIT_ROM
    hashFree(romTab);
#endif
}


PUBLIC int websOpenFile(char *path, int flags, int mode)
{
#if BIT_ROM
    WebsRomIndex    *wip;
    WebsKey         *sp;

    if ((sp = hashLookup(romTab, path)) == NULL) {
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
#if !BIT_ROM
        close(fd);
#endif
    }
}


//  MOB - should this be sbuf or WebsFileInfo?

PUBLIC int websStatFile(char *path, WebsFileInfo *sbuf)
{
#if BIT_ROM
    WebsRomIndex    *wip;
    WebsKey         *sp;

    assert(path && *path);

    if ((sp = hashLookup(romTab, path)) == NULL) {
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
    if (stat(path, &s) < 0) {
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
#if BIT_ROM
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
    return read(fd, buf, size);
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
    buf = malloc(sbuf.size + 1);
    if ((fd = websOpenFile(path, O_RDONLY, 0)) < 0) {
        return 0;
    }
    websReadFile(fd, buf, sbuf.size);
    buf[sbuf.size] = '\0';
    websCloseFile(fd);
    return buf;
}


Offset websSeekFile(int fd, Offset offset, int origin)
{
#if BIT_ROM
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
    return lseek(fd, offset, origin);
#endif
}


PUBLIC ssize websWriteFile(int fd, char *buf, ssize size)
{
#if BIT_ROM
    error("Cannot write to a rom file system");
    return -1;
#else
    return write(fd, buf, size);
#endif
}


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2013. All Rights Reserved.

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
