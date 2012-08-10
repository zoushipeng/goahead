/*
    galloc.c -- Block allocation module

    This file implements a fast block allocation scheme suitable for operating systems whoes malloc suffers from
    fragmentation.  This allocator maintains block class queues for rapid allocation and minimal fragmentation. This
    allocator does not coalesce blocks. The storage space may be populated statically or via the traditional malloc
    mechanisms. Large blocks greater than the maximum class size may be allocated from the O/S or run-time system via
    malloc. To permit the use of malloc, call gopen with flags set to G_USE_MALLOC (this is the default).  It is
    recommended that gopen be called first thing in the application.  If it is not, it will be called with default
    values on the first call to galloc(). Note that this code is not designed for multi-threading purposes and it
    depends on newly declared variables being initialized to zero.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


/********************************* Includes ***********************************/
#include    "goahead.h"

#if BIT_REPLACE_MALLOC
/********************************* Defines ************************************/
/*
    ROUNDUP4(size) returns the next higher integer value of size that is divisible by 4, or the value of size if size is
    divisible by 4. ROUNDUP4() is used in aligning memory allocations on 4-byte boundaries.
    Note:  ROUNDUP4() is only required on some operating systems.
 */
#define ROUNDUP4(size) ((size) % 4) ? (size) + (4 - ((size) % 4)) : (size)

/*
    gQhead blocks are created as the original memory allocation is freed up. See gfree.
 */
static gType    *gQhead[G_MAX_CLASS];   /* Per class block q head */
static char     *gfreeBuf;              /* Pointer to free memory */
static char     *gfreeNext;             /* Pointer to next free mem */
static int      gfreeSize;              /* Size of free memory */
static int      gfreeLeft;              /* Size of free left for use */
static int      gFlags = G_USE_MALLOC;  /* Default to auto-malloc */
static int      gopenCount = 0;         /* Num tasks using galloc */

/*************************** Forward Declarations *****************************/

static int gallocGetSize(ssize size, int *q);

/********************************** Code **************************************/
/*
    Initialize the galloc module. gopen should be called the very first thing after the application starts and bclose
    should be called the last thing before exiting. If gopen is not called, it will be called on the first allocation
    with default values. "buf" points to memory to use of size "bufsize". If buf is NULL, memory is allocated using
    malloc. flags may be set to G_USE_MALLOC if using malloc is okay. This routine will allocate *  an initial buffer of
    size bufsize for use by the application.
 */
int gopenAlloc(void *buf, int bufsize, int flags)
{
    gFlags = flags;

    /*
        If gopen already called by a shared process, just increment the count and return
     */
    if (++gopenCount > 1) {
        return 0;
    }
    if (buf == NULL) {
        if (bufsize == 0) {
            bufsize = G_DEFAULT_MEM;
        }
        bufsize = ROUNDUP4(bufsize);
        if ((buf = malloc(bufsize)) == NULL) {
            /* 
                Resetting gopenCount lets client code decide to attempt to call gopen() again with a smaller memory request.
            */
             --gopenCount;
            return -1;
        }
    } else {
        gFlags |= G_USER_BUF;
    }
    gfreeSize = gfreeLeft = bufsize;
    gfreeBuf = gfreeNext = buf;
    memset(gQhead, 0, sizeof(gQhead));
    return 0;
}


void gcloseAlloc()
{
    if (--gopenCount <= 0 && !(gFlags & G_USER_BUF)) {
        free(gfreeBuf);
        gopenCount = 0;
    }
}


/*
    Allocate a block of the requested size. First check the block queues for a suitable one.
 */
void *galloc(ssize size)
{
    gType   *bp;
    int     q, memSize;

    /*
        Call gopen with default values if the application has not yet done so
     */
    if (gfreeBuf == NULL) {
        if (gopenAlloc(NULL, G_DEFAULT_MEM, 0) < 0) {
            return NULL;
        }
    }
    if (size < 0) {
        return NULL;
    }
    memSize = gallocGetSize(size, &q);

    if (q >= G_MAX_CLASS) {
        /*
            Size if bigger than the maximum class. Malloc if use has been okayed
         */
        if (gFlags & G_USE_MALLOC) {
            memSize = ROUNDUP4(memSize);
            bp = (gType*) malloc(memSize);
            if (bp == NULL) {
                traceRaw(T("B: malloc failed\n"));
                return NULL;
            }
        } else {
            traceRaw(T("B: malloc failed\n"));
            return NULL;
        }
        /*
            the u.size is the actual size allocated for data
         */
        bp->u.size = memSize - sizeof(gType);
        bp->flags = G_MALLOCED;

    } else if ((bp = gQhead[q]) != NULL) {
        /*
            Take first block off the relevant q if non-empty
         */
        gQhead[q] = bp->u.next;
        bp->u.size = memSize - sizeof(gType);
        bp->flags = 0;

    } else {
        if (gfreeLeft > memSize) {
            /*
                The q was empty, and the free list has spare memory so create a new block out of the primary free block
             */
            bp = (gType*) gfreeNext;
            gfreeNext += memSize;
            gfreeLeft -= memSize;
            bp->u.size = memSize - sizeof(gType);
            bp->flags = 0;

        } else if (gFlags & G_USE_MALLOC) {
            /*
                Nothing left on the primary free list, so malloc a new block
             */
            memSize = ROUNDUP4(memSize);
            if ((bp = (gType*) malloc(memSize)) == NULL) {
                traceRaw(T("B: malloc failed\n"));
                return NULL;
            }
            bp->u.size = memSize - sizeof(gType);
            bp->flags = G_MALLOCED;

        } else {
            traceRaw(T("B: malloc failed\n"));
            return NULL;
        }
    }
    bp->flags |= G_INTEGRITY;
    return (void*) ((char*) bp + sizeof(gType));
}


/*
    Free a block back to the relevant free q. We don't free back to the O/S or run time system unless the block is
    greater than the maximum class size. We also do not coalesce blocks.  
 */
void gfree(void *mp)
{
    gType   *bp;
    int     q;

    if (mp == 0) {
        return;
    }
    bp = (gType*) ((char*) mp - sizeof(gType));
    gassert((bp->flags & G_INTEGRITY_MASK) == G_INTEGRITY);
    if ((bp->flags & G_INTEGRITY_MASK) != G_INTEGRITY) {
        return;
    }
    gallocGetSize(bp->u.size, &q);
    if (bp->flags & G_MALLOCED) {
        free(bp);
        return;
    }
    /*
        Simply link onto the head of the relevant q
     */
    bp->u.next = gQhead[q];
    gQhead[q] = bp;
    bp->flags = G_FILL_WORD;
}


#if UNICODE
/*
    Duplicate a string, allow NULL pointers and then dup an empty string.
 */
char *gstrdupA(char *s)
{
    char    *cp;
    int     len;

    if (s == NULL) {
        s = "";
    }
    len = strlen(s) + 1;
    if (cp = galloc(len)) {
        strcpy(cp, s);
    }
    return cp;
}

#endif /* UNICODE */

/*
    Duplicate an ascii string, allow NULL pointers and then dup an empty string. If UNICODE, gstrdup above works with
    wide chars, so we need this routine *  for ascii strings. 
 */
char_t *gstrdup(char_t *s)
{
    char_t  *cp;
    ssize   len;

    if (s == NULL) {
        s = T("");
    }
    len = gstrlen(s) + 1;
    if ((cp = galloc(len * sizeof(char_t))) != NULL) {
        gstrncpy(cp, s, len * sizeof(char_t));
    }
    return cp;
}


/*
    Reallocate a block. Allow NULL pointers and just do a malloc. Note: if the realloc fails, we return NULL and the
    previous buffer is preserved.
 */
void *grealloc(void *mp, ssize newsize)
{
    gType   *bp;
    void    *newbuf;

    if (mp == NULL) {
        return galloc(newsize);
    }
    bp = (gType*) ((char*) mp - sizeof(gType));
    gassert((bp->flags & G_INTEGRITY_MASK) == G_INTEGRITY);

    /*
        If the allocated memory already has enough room just return the previously allocated address.
     */
    if (bp->u.size >= newsize) {
        return mp;
    }
    if ((newbuf = galloc(newsize)) != NULL) {
        memcpy(newbuf, mp, bp->u.size);
        gfree(mp);
    }
    return newbuf;
}


/*
    Find the size of the block to be galloc'ed.  It takes in a size, finds the smallest binary block it fits into, adds
    an overhead amount and returns.  q is the binary size used to keep track of block sizes in use.  Called from both
    galloc and gfree.
 */
static int gallocGetSize(ssize size, int *q)
{
    int mask;

    mask = (size == 0) ? 0 : (size-1) >> G_SHIFT;
    for (*q = 0; mask; mask >>= 1) {
        *q = *q + 1;
    }
    return ((1 << (G_SHIFT + *q)) + sizeof(gType));
}


#else /* !BIT_REPLACE_MALLOC */

/*
    Stubs
 */
int gopenAlloc(void *buf, int bufsize, int flags) { return 0; }
void gcloseAlloc() { }

char_t *gstrdupNoAlloc(char_t *s)
{
#if UNICODE
    if (s) {
        return wcsdup(s);
    } else {
        return wcsdup(T(""));
    }
#else
    return gstrdupANoAlloc(s);
#endif
}


char *gstrdupANoAlloc(char *s)
{
    char*   buf;

    if (s == NULL) {
        s = "";
    }
    buf = malloc(strlen(s)+1);
    strcpy(buf, s);
    return buf;
}

#endif /* BIT_REPLACE_MALLOC */

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
