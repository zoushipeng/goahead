/*
    balloc.c -- Block allocation module

    This file implements a fast block allocation scheme suitable for operating systems whoes malloc suffers from
    fragmentation.  This allocator maintains block class queues for rapid allocation and minimal fragmentation. This
    allocator does not coalesce blocks. The storage space may be populated statically or via the traditional malloc
    mechanisms. Large blocks greater than the maximum class size may be allocated from the O/S or run-time system via
    malloc. To permit the use of malloc, call bopen with flags set to B_USE_MALLOC (this is the default).  It is
    recommended that bopen be called first thing in the application.  If it is not, it will be called with default
    values on the first call to balloc(). Note that this code is not designed for multi-threading purposes and it
    depends on newly declared variables being initialized to zero.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


/********************************* Includes ***********************************/
#include    "uemf.h"

#if BIT_REPLACE_MALLOC
/********************************* Defines ************************************/
/*
    ROUNDUP4(size) returns the next higher integer value of size that is divisible by 4, or the value of size if size is
    divisible by 4. ROUNDUP4() is used in aligning memory allocations on 4-byte boundaries.
    Note:  ROUNDUP4() is only required on some operating systems.
 */
#define ROUNDUP4(size) ((size) % 4) ? (size) + (4 - ((size) % 4)) : (size)

/*
    bQhead blocks are created as the original memory allocation is freed up. See bfree.
 */
static bType            *bQhead[B_MAX_CLASS];   /* Per class block q head */
static char             *bFreeBuf;              /* Pointer to free memory */
static char             *bFreeNext;             /* Pointer to next free mem */
static int              bFreeSize;              /* Size of free memory */
static int              bFreeLeft;              /* Size of free left for use */
static int              bFlags = B_USE_MALLOC;  /* Default to auto-malloc */
static int              bopenCount = 0;         /* Num tasks using balloc */

/*************************** Forward Declarations *****************************/

static int ballocGetSize(int size, int *q);

/********************************** Code **************************************/
/*
    Initialize the balloc module. bopen should be called the very first thing after the application starts and bclose
    should be called the last thing before exiting. If bopen is not called, it will be called on the first allocation
    with default values. "buf" points to memory to use of size "bufsize". If buf is NULL, memory is allocated using
    malloc. flags may be set to B_USE_MALLOC if using malloc is okay. This routine will allocate *  an initial buffer of
    size bufsize for use by the application.
 */
int bopen(void *buf, int bufsize, int flags)
{
    bFlags = flags;

    /*
        If bopen already called by a shared process, just increment the count and return
     */
    if (++bopenCount > 1) {
        return 0;
    }

        if (bufsize == 0) {
            bufsize = B_DEFAULT_MEM;
        }
        bufsize = ROUNDUP4(bufsize);
        if ((buf = malloc(bufsize)) == NULL) {
            /* 
                Resetting bopenCount lets client code decide to attempt to call bopen() again with a smaller memory request.
            */
             --bopenCount;
            return -1;
        }
    } else {
        bFlags |= B_USER_BUF;
    }
    bFreeSize = bFreeLeft = bufsize;
    bFreeBuf = bFreeNext = buf;
    memset(bQhead, 0, sizeof(bQhead));
    return 0;
}

void bclose()
{
    if (--bopenCount <= 0 && !(bFlags & B_USER_BUF)) {
        free(bFreeBuf);
        bopenCount = 0;
    }
}

/*
    Allocate a block of the requested size. First check the block queues for a suitable one.
 */
void *balloc(int size)
{
    bType   *bp;
    int     q, memSize;

    /*
        Call bopen with default values if the application has not yet done so
     */
    if (bFreeBuf == NULL) {
        if (bopen(NULL, B_DEFAULT_MEM, 0) < 0) {
            return NULL;
        }
    }
    if (size < 0) {
        return NULL;
    }
    memSize = ballocGetSize(size, &q);

    if (q >= B_MAX_CLASS) {
        /*
            Size if bigger than the maximum class. Malloc if use has been okayed
         */
        if (bFlags & B_USE_MALLOC) {
            memSize = ROUNDUP4(memSize);
            bp = (bType*) malloc(memSize);
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
        bp->u.size = memSize - sizeof(bType);
        bp->flags = B_MALLOCED;

    } else if ((bp = bQhead[q]) != NULL) {
        /*
            Take first block off the relevant q if non-empty
         */
        bQhead[q] = bp->u.next;
        bp->u.size = memSize - sizeof(bType);
        bp->flags = 0;

    } else {
        if (bFreeLeft > memSize) {
            /*
                The q was empty, and the free list has spare memory so create a new block out of the primary free block
             */
            bp = (bType*) bFreeNext;
            bFreeNext += memSize;
            bFreeLeft -= memSize;
            bp->u.size = memSize - sizeof(bType);
            bp->flags = 0;

        } else if (bFlags & B_USE_MALLOC) {
            /*
                Nothing left on the primary free list, so malloc a new block
             */
            memSize = ROUNDUP4(memSize);
            if ((bp = (bType*) malloc(memSize)) == NULL) {
                traceRaw(T("B: malloc failed\n"));
                return NULL;
            }
            bp->u.size = memSize - sizeof(bType);
            bp->flags = B_MALLOCED;

        } else {
            traceRaw(T("B: malloc failed\n"));
            return NULL;
        }
    }
    bp->flags |= B_INTEGRITY;
    return (void*) ((char*) bp + sizeof(bType));
}


/*
    Free a block back to the relevant free q. We don't free back to the O/S or run time system unless the block is
    greater than the maximum class size. We also do not coalesce blocks.  
 */
void bfree(void *mp)
{
    bType   *bp;
    int     q, memSize;

    bp = (bType*) ((char*) mp - sizeof(bType));
    a_assert((bp->flags & B_INTEGRITY_MASK) == B_INTEGRITY);
    if ((bp->flags & B_INTEGRITY_MASK) != B_INTEGRITY) {
        return;
    }
    memSize = ballocGetSize(bp->u.size, &q);
    if (bp->flags & B_MALLOCED) {
        free(bp);
        return;
    }
    /*
        Simply link onto the head of the relevant q
     */
    bp->u.next = bQhead[q];
    bQhead[q] = bp;
    bp->flags = B_FILL_WORD;
}


void bfreeSafe(void *mp)
{
    if (mp) {
        bfree(mp);
    }
}


#if UNICODE
/*
    Duplicate a string, allow NULL pointers and then dup an empty string.
 */
char *bstrdupA(char *s)
{
    char    *cp;
    int     len;

    if (s == NULL) {
        s = "";
    }
    len = strlen(s) + 1;
    if (cp = balloc(len)) {
        strcpy(cp, s);
    }
    return cp;
}

#endif /* UNICODE */

/*
    Duplicate an ascii string, allow NULL pointers and then dup an empty string. If UNICODE, bstrdup above works with
    wide chars, so we need this routine *  for ascii strings. 
 */
char_t *bstrdup(char_t *s)
{
    char_t  *cp;
    int     len;

    if (s == NULL) {
        s = T("");
    }
    len = gstrlen(s) + 1;
    if ((cp = balloc(len * sizeof(char_t))) != NULL) {
        gstrncpy(cp, s, len * sizeof(char_t));
    }
    return cp;
}


/*
    Reallocate a block. Allow NULL pointers and just do a malloc. Note: if the realloc fails, we return NULL and the
    previous buffer is preserved.
 */
void *brealloc(void *mp, int newsize)
{
    bType   *bp;
    void    *newbuf;

    if (mp == NULL) {
        return balloc(newsize);
    }
    bp = (bType*) ((char*) mp - sizeof(bType));
    a_assert((bp->flags & B_INTEGRITY_MASK) == B_INTEGRITY);

    /*
        If the allocated memory already has enough room just return the previously allocated address.
     */
    if (bp->u.size >= newsize) {
        return mp;
    }
    if ((newbuf = balloc(newsize)) != NULL) {
        memcpy(newbuf, mp, bp->u.size);
        bfree(mp);
    }
    return newbuf;
}


/*
    Find the size of the block to be balloc'ed.  It takes in a size, finds the smallest binary block it fits into, adds
    an overhead amount and returns.  q is the binary size used to keep track of block sizes in use.  Called from both
    balloc and bfree.
 */
static int ballocGetSize(int size, int *q)
{
    int mask;

    mask = (size == 0) ? 0 : (size-1) >> B_SHIFT;
    for (*q = 0; mask; mask >>= 1) {
        *q = *q + 1;
    }
    return ((1 << (B_SHIFT + *q)) + sizeof(bType));
}


#else /* !BIT_REPLACE_MALLOC */
int bopen(void *buf, int bufsize, int flags) { return 0; }
void bclose() { }

char_t *bstrdupNoBalloc(char_t *s)
{
#if UNICODE
    if (s) {
        return wcsdup(s);
    } else {
        return wcsdup(T(""));
    }
#else
    return bstrdupANoBalloc(s);
#endif
}


char *bstrdupANoBalloc(char *s)
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
