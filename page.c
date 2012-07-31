/*
    page.c -- Support for page retrieval.

    This file provides page retrieval handling. It provides support for reading web pages from file systems and has
    expansion for ROMed web pages.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "wsIntrn.h"

/*********************************** Code *************************************/
/*
    Open a web page. lpath is the local filename. path is the URL path name.
 */
int websPageOpen(webs_t wp, char_t *lpath, char_t *path, int mode, int perm)
{
    //  MOB
#if WIN
    errno_t error;
#endif
    a_assert(websValid(wp));

#if BIT_ROM
    return websRomPageOpen(wp, path, mode, perm);
#elif WIN
    error = _sopen_s(&(wp->docfd), lpath, mode, _SH_DENYNO, _S_IREAD);
    return (wp->docfd = gopen(lpath, mode, _S_IREAD));
#else
    return (wp->docfd = gopen(lpath, mode, perm));
#endif /* WEBS_PAGE_ROM */
}


void websPageClose(webs_t wp)
{
    a_assert(websValid(wp));

#if BIT_ROM
    websRomPageClose(wp->docfd);
#else
    if (wp->docfd >= 0) {
        close(wp->docfd);
        wp->docfd = -1;
    }
#endif
}


/*
    Stat a web page lpath is the local filename. path is the URL path name.
 */
int websPageStat(webs_t wp, char_t *lpath, char_t *path, websStatType* sbuf)
{
#if BIT_ROM
    return websRomPageStat(path, sbuf);
#else
    gstat_t s;

    if (gstat(lpath, &s) < 0) {
        return -1;
    }
    sbuf->size = s.st_size;
    sbuf->mtime = s.st_mtime;
    sbuf->isDir = s.st_mode & S_IFDIR;
    return 0;
#endif
}


int websPageIsDirectory(char_t *lpath)
{
#if BIT_ROM
    websStatType    sbuf;

    if (websRomPageStat(lpath, &sbuf) >= 0) {
        return(sbuf.isDir);
    } else {
        return 0;
    }
#else
    gstat_t sbuf;

    if (gstat(lpath, &sbuf) >= 0) {
        return(sbuf.st_mode & S_IFDIR);
    } else {
        return 0;
    }
#endif
}



/*
    Read a web page. Returns the number of _bytes_ read. len is the size of buf, in bytes.
 */
int websPageReadData(webs_t wp, char *buf, int nBytes)
{

#if BIT_ROM
    a_assert(websValid(wp));
    return websRomPageReadData(wp, buf, nBytes);
#else
    a_assert(websValid(wp));
    return read(wp->docfd, buf, nBytes);
#endif
}


/*
    Move file pointer offset bytes.
 */
void websPageSeek(webs_t wp, long offset)
{
    a_assert(websValid(wp));

#if BIT_ROM
    websRomPageSeek(wp, offset, SEEK_CUR);
#else
    lseek(wp->docfd, offset, SEEK_CUR);
#endif
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
