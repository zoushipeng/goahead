/*
    runtime.c -- Runtime support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

//  MOB sort file

/*********************************** Defines **********************************/
/*
    This structure stores scheduled events.
 */
typedef struct {
    void    (*routine)(void *arg, int id);
    void    *arg;
    time_t  at;
    int     schedid;
} sched_t;

/*
    Sprintf buffer structure. Make the increment 64 so that a galloc can use a 64 byte block.
 */

#define STR_REALLOC     0x1             /* Reallocate the buffer as required */
#define STR_INC         64              /* Growth increment */

typedef struct {
    char_t  *s;                         /* Pointer to buffer */
    ssize   size;                       /* Current buffer size */
    ssize   max;                        /* Maximum buffer size */
    ssize   count;                      /* Buffer count */
    int     flags;                      /* Allocation flags */
} strbuf_t;

/*
    Sprintf formatting flags
 */
enum flag {
    flag_none = 0,
    flag_minus = 1,
    flag_plus = 2,
    flag_space = 4,
    flag_hash = 8,
    flag_zero = 16,
    flag_short = 32,
    flag_long = 64
};

/*
    The handle list stores the length of the list and the number of used handles in the first two words.  These are
    hidden from the caller by returning a pointer to the third word to the caller.
 */
#define H_LEN       0       /* First entry holds length of list */
#define H_USED      1       /* Second entry holds number of used */
#define H_OFFSET    2       /* Offset to real start of list */

#define H_INCR      16      /* Grow handle list in chunks this size */

#define RINGQ_LEN(rq) ((rq->servp > rq->endp) ? (rq->buflen + (rq->endp - rq->servp)) : (rq->endp - rq->servp))

typedef struct {                        /* Symbol table descriptor */
    int     inuse;                      /* Is this entry in use */
    int     hash_size;                  /* Size of the table below */
    sym_t   **hash_table;               /* Allocated at run time */
} sym_tabent_t;

#if WINDOWS
static HINSTANCE appInstance;
#endif

/********************************* Globals ************************************/

static sym_tabent_t **sym;              /* List of symbol tables */
static int          symMax;             /* One past the max symbol table */
static int          symOpenCount = 0;   /* Count of apps using sym */

static int          htIndex;            /* Current location in table */
static sym_t*       next;               /* Next symbol in iteration */

static sched_t      **sched;
static int          schedMax;

#if BIT_DEBUG_LOG
//  MOB - rename
static char_t   *tracePath;
static int      traceFd;                        /* Log file handle */
static int      traceLevel;
#endif

char* embedthisGoAheadCopyright = EMBEDTHIS_GOAHEAD_COPYRIGHT;

/********************************** Forwards **********************************/

static ssize dsnprintf(char_t **s, ssize size, char_t *fmt, va_list arg, ssize msize);
static void put_char(strbuf_t *buf, char_t c);
static void put_string(strbuf_t *buf, char_t *s, ssize len, ssize width, int prec, enum flag f);
static void put_ulong(strbuf_t *buf, ulong value, int base, int upper, char_t *prefix, int width, int prec, enum flag f);
static ssize gstrnlen(char_t *s, ssize n);

#if BIT_DEBUG_LOG
static void defaultTraceHandler(int level, char_t *buf);
static TraceHandler traceHandler = defaultTraceHandler;
#endif

static int ringqGrow(ringq_t *rq);
static int getBinBlockSize(int size);

static int hashIndex(sym_tabent_t *tp, char_t *name);
static sym_t *hash(sym_tabent_t *tp, char_t *name);
static int calcPrime(int size);

/************************************* Code ***********************************/
/*
    This function is called when a scheduled process time has come.
    MOB - why caps?  Static?
 */
static void timerProc(int schedid)
{
    sched_t *s;

    gassert(0 <= schedid && schedid < schedMax);
    s = sched[schedid];
    gassert(s);

    (s->routine)(s->arg, s->schedid);
}


/*
    Schedule an event in delay milliseconds time. We will use 1 second granularity for webServer.
 */
int gSchedCallback(int delay, GSchedProc *proc, void *arg)
{
    sched_t *s;
    int     schedid;

    if ((schedid = gallocEntry((void***) &sched, &schedMax, sizeof(sched_t))) < 0) {
        return -1;
    }
    s = sched[schedid];
    s->routine = proc;
    s->arg = arg;
    s->schedid = schedid;

    /*
        Round the delay up to seconds.
     */
    s->at = ((delay + 500) / 1000) + time(0);
    return schedid;
}


/*
    Reschedule to a new delay.
 */
void gReschedCallback(int schedid, int delay)
{
    sched_t *s;

    if (sched == NULL || schedid == -1 || schedid >= schedMax || (s = sched[schedid]) == NULL) {
        return;
    }
    s->at = ((delay + 500) / 1000) + time(0);
}


void gUnschedCallback(int schedid)
{
    sched_t *s;

    if (sched == NULL || schedid == -1 || schedid >= schedMax || (s = sched[schedid]) == NULL) {
        return;
    }
    gfree(s);
    schedMax = gfreeHandle((void***) &sched, schedid);
}


/*
    Take tasks off the queue in a round robin fashion.
 */
void gSchedProcess()
{
    sched_t     *s;
    int         schedid;
    static int  next = 0;   

    /*
        If schedMax is 0, there are no tasks scheduled, so just return.
     */
    if (schedMax <= 0) {
        return;
    }

    /*
        If next >= schedMax, the schedule queue was reduced in our absence
        so reset next to 0 to start from the begining of the queue again.
     */
    if (next >= schedMax) {
        next = 0;
    }

    schedid = next;
    for (;;) {
        if ((s = sched[schedid]) != NULL && (int)s->at <= (int)time(0)) {
            timerProc(schedid);
            next = schedid + 1;
            return;
        }
        if (++schedid >= schedMax) {
            schedid = 0;
        }
        if (schedid == next) {
            /*
                We've gone all the way through the queue without finding anything to do so just return.
             */
            return;
        }
    }
}

/************************************ Code ************************************/
/*
    "basename" returns a pointer to the last component of a pathname LINUX, LynxOS and Mac OS X have their own basename
    function 
 */

#if !BIT_UNIX_LIKE
char_t *basename(char_t *name)
{
    char_t  *cp;

#if BIT_WIN_LIKE
    if (((cp = gstrrchr(name, '\\')) == NULL) && ((cp = gstrrchr(name, '/')) == NULL)) {
        return name;
#else
    if ((cp = gstrrchr(name, '/')) == NULL) {
        return name;
#endif
    } else if (*(cp + 1) == '\0' && cp == name) {
        return name;
    } else if (*(cp + 1) == '\0' && cp != name) {
        return T("");
    } else {
        return ++cp;
    }
}
#endif


/*
    Returns a pointer to the directory component of a pathname. bufsize is the size of the buffer in BYTES!
 */
char_t *dirname(char_t *buf, char_t *name, ssize bufsize)
{
    char_t  *cp;
    ssize   len;

    gassert(name);
    gassert(buf);
    gassert(bufsize > 0);

#if WINDOWS
    if ((cp = gstrrchr(name, '/')) == NULL && (cp = gstrrchr(name, '\\')) == NULL)
#else
    if ((cp = gstrrchr(name, '/')) == NULL)
#endif
    {
        gstrcpy(buf, T("."));
        return buf;
    }
    if ((*(cp + 1) == '\0' && cp == name)) {
        gstrncpy(buf, T("."), TSZ(bufsize));
        gstrcpy(buf, T("."));
        return buf;
    }
    len = cp - name;
    if (len < bufsize) {
        gstrncpy(buf, name, len);
        buf[len] = '\0';
    } else {
        gstrncpy(buf, name, TSZ(bufsize));
        buf[bufsize - 1] = '\0';
    }
    return buf;
}


/*
    sprintf and vsprintf are bad, ok. You can easily clobber memory. Use gfmtAlloc and gfmtValloc instead! These functions
    do _not_ support floating point, like %e, %f, %g...
 */
ssize gfmtAlloc(char_t **s, ssize n, char_t *fmt, ...)
{
    va_list ap;
    ssize   result;

    gassert(s);
    gassert(fmt);

    *s = NULL;
    va_start(ap, fmt);
    result = dsnprintf(s, n, fmt, ap, 0);
    va_end(ap);
    return result;
}


/*
    Support a static buffer version for small buffers only!
 */
ssize gfmtStatic(char_t *s, ssize n, char_t *fmt, ...)
{
    va_list ap;
    ssize   result;

    gassert(s);
    gassert(fmt);
    gassert(n <= 256);

    if (n <= 0) {
        return -1;
    }
    va_start(ap, fmt);
    result = dsnprintf(&s, n, fmt, ap, 0);
    va_end(ap);
    return result;
}


#if UNUSED && KEEP
/*
    This function appends the formatted string to the supplied string, reallocing if required.
 */
ssize fmtRealloc(char_t **s, ssize n, ssize msize, char_t *fmt, ...)
{
    va_list     ap;
    ssize       result;

    gassert(s);
    gassert(fmt);

    if (msize == -1) {
        *s = NULL;
    }
    va_start(ap, fmt);
    result = dsnprintf(s, n, fmt, ap, msize);
    va_end(ap);
    return result;
}
#endif


/*
    A vsprintf replacement
 */
ssize gfmtValloc(char_t **s, ssize n, char_t *fmt, va_list arg)
{
    gassert(s);
    gassert(fmt);

    *s = NULL;
    return dsnprintf(s, n, fmt, arg, 0);
}


/*
    Dynamic sprintf implementation. Supports dynamic buffer allocation. This function can be called multiple times to
    grow an existing allocated buffer. In this case, msize is set to the size of the previously allocated buffer. The
    buffer will be realloced, as required. If msize is set, we return the size of the allocated buffer for use with the
    next call. For the first call, msize can be set to -1.
 */
static ssize dsnprintf(char_t **s, ssize size, char_t *fmt, va_list arg, ssize msize)
{
    strbuf_t    buf;
    char_t      c;

    gassert(s);
    gassert(fmt);

    if (size < 0) {
        size = BIT_LIMIT_STRING;
    }
    memset(&buf, 0, sizeof(buf));
    buf.s = *s;

    if (*s == NULL || msize != 0) {
        buf.max = size;
        buf.flags |= STR_REALLOC;
        if (msize != 0) {
            buf.size = max(msize, 0);
        }
        if (*s != NULL && msize != 0) {
            buf.count = gstrlen(*s);
        }
    } else {
        buf.size = size;
    }
    while ((c = *fmt++) != '\0') {
        if (c != '%' || (c = *fmt++) == '%') {
            put_char(&buf, c);
        } else {
            enum flag f = flag_none;
            int width = 0;
            int prec = -1;
            for ( ; c != '\0'; c = *fmt++) {
                if (c == '-') { 
                    f |= flag_minus; 
                } else if (c == '+') { 
                    f |= flag_plus; 
                } else if (c == ' ') { 
                    f |= flag_space; 
                } else if (c == '#') { 
                    f |= flag_hash; 
                } else if (c == '0') { 
                    f |= flag_zero; 
                } else {
                    break;
                }
            }
            if (c == '*') {
                width = va_arg(arg, int);
                if (width < 0) {
                    f |= flag_minus;
                    width = -width;
                }
                c = *fmt++;
            } else {
                for ( ; gisdigit((int)c); c = *fmt++) {
                    width = width * 10 + (c - '0');
                }
            }
            if (c == '.') {
                f &= ~flag_zero;
                c = *fmt++;
                if (c == '*') {
                    prec = va_arg(arg, int);
                    c = *fmt++;
                } else {
                    for (prec = 0; gisdigit((int)c); c = *fmt++) {
                        prec = prec * 10 + (c - '0');
                    }
                }
            }
            if (c == 'h' || c == 'l') {
                //  MOB - need support for %Ld
                f |= (c == 'h' ? flag_short : flag_long);
                c = *fmt++;
            }
            if (c == 'd' || c == 'i') {
                long int value;
                if (f & flag_short) {
                    value = (short int) va_arg(arg, int);
                } else if (f & flag_long) {
                    value = va_arg(arg, long int);
                } else {
                    value = va_arg(arg, int);
                }
                if (value >= 0) {
                    if (f & flag_plus) {
                        put_ulong(&buf, value, 10, 0, T("+"), width, prec, f);
                    } else if (f & flag_space) {
                        put_ulong(&buf, value, 10, 0, T(" "), width, prec, f);
                    } else {
                        put_ulong(&buf, value, 10, 0, NULL, width, prec, f);
                    }
                } else {
                    put_ulong(&buf, -value, 10, 0, T("-"), width, prec, f);
                }
            } else if (c == 'o' || c == 'u' || c == 'x' || c == 'X') {
                ulong value;
                if (f & flag_short) {
                    value = (ushort) va_arg(arg, uint);
                } else if (f & flag_long) {
                    value = va_arg(arg, ulong);
                } else {
                    value = va_arg(arg, uint);
                }
                if (c == 'o') {
                    if (f & flag_hash && value != 0) {
                        put_ulong(&buf, value, 8, 0, T("0"), width, prec, f);
                    } else {
                        put_ulong(&buf, value, 8, 0, NULL, width, prec, f);
                    }
                } else if (c == 'u') {
                    put_ulong(&buf, value, 10, 0, NULL, width, prec, f);
                } else {
                    if (f & flag_hash && value != 0) {
                        if (c == 'x') {
                            put_ulong(&buf, value, 16, 0, T("0x"), width, prec, f);
                        } else {
                            put_ulong(&buf, value, 16, 1, T("0X"), width, prec, f);
                        }
                    } else {
                        put_ulong(&buf, value, 16, ('X' == c) , NULL, width, prec, f);
                    }
                }

            } else if (c == 'c') {
                char_t value = va_arg(arg, int);
                put_char(&buf, value);

            } else if (c == 's' || c == 'S') {
                char_t *value = va_arg(arg, char_t *);
                if (value == NULL) {
                    put_string(&buf, T("(null)"), -1, width, prec, f);
                } else if (f & flag_hash) {
                    put_string(&buf, value + 1, (char_t) *value, width, prec, f);
                } else {
                    put_string(&buf, value, -1, width, prec, f);
                }
            } else if (c == 'p') {
                void *value = va_arg(arg, void *);
                put_ulong(&buf, (ulong) value, 16, 0, T("0x"), width, prec, f);
            } else if (c == 'n') {
                if (f & flag_short) {
                    short *value = va_arg(arg, short*);
                    *value = (short) buf.count;
                } else if (f & flag_long) {
                    long *value = va_arg(arg, long*);
                    *value = buf.count;
                } else {
                    ssize *value = va_arg(arg, ssize*);
                    *value = buf.count;
                }
            } else {
                put_char(&buf, c);
            }
        }
    }
    if (buf.s == NULL) {
        put_char(&buf, '\0');
    }
    /*
        If the user requested a dynamic buffer (*s == NULL), ensure it is returned.
     */
    if (*s == NULL || msize != 0) {
        *s = buf.s;
    }
    if (*s != NULL && size > 0) {
        if (buf.count < size) {
            (*s)[buf.count] = '\0';
        } else {
            (*s)[buf.size - 1] = '\0';
        }
    }

    if (msize != 0) {
        return buf.size;
    }
    return buf.count;
}


/*
    Return the length of a string limited by a given length
 */
static ssize gstrnlen(char_t *s, ssize n)
{
    ssize   len;

    len = gstrlen(s);
    return min(len, n);
}


/*
    Add a character to a string buffer
 */
static void put_char(strbuf_t *buf, char_t c)
{
    if (buf->count >= (buf->size - 1)) {
        if (! (buf->flags & STR_REALLOC)) {
            return;
        }
        buf->size += STR_INC;
        if (buf->size > buf->max && buf->size > STR_INC) {
            /*
                Caller should increase the size of the calling buffer
             */
            buf->size -= STR_INC;
            return;
        }
        if (buf->s == NULL) {
            buf->s = galloc(buf->size * sizeof(char_t));
        } else {
            buf->s = grealloc(buf->s, buf->size * sizeof(char_t));
        }
    }
    buf->s[buf->count] = c;
    if (c != '\0') {
        ++buf->count;
    }
}


/*
    Add a string to a string buffer
 */
static void put_string(strbuf_t *buf, char_t *s, ssize len, ssize width, int prec, enum flag f)
{
    ssize   i;

    if (len < 0) { 
        len = gstrnlen(s, prec >= 0 ? prec : MAXSSIZE);
    } else if (prec >= 0 && prec < len) { 
        len = prec; 
    }
    if (width > len && !(f & flag_minus)) {
        for (i = len; i < width; ++i) { 
            put_char(buf, ' '); 
        }
    }
    for (i = 0; i < len; ++i) { 
        put_char(buf, s[i]); 
    }
    if (width > len && f & flag_minus) {
        for (i = len; i < width; ++i) { 
            put_char(buf, ' '); 
        }
    }
}


/*
    Add a long to a string buffer
 */
static void put_ulong(strbuf_t *buf, ulong value, int base, int upper, char_t *prefix, int width, int prec, enum flag f) 
{
    ulong       x, x2;
    int         len, zeros, i;

    for (len = 1, x = 1; x < ULONG_MAX / base; ++len, x = x2) {
        x2 = x * base;
        if (x2 > value) { 
            break; 
        }
    }
    zeros = (prec > len) ? prec - len : 0;
    width -= zeros + len;
    if (prefix != NULL) { 
        width -= gstrnlen(prefix, MAXSSIZE); 
    }
    if (!(f & flag_minus)) {
        if (f & flag_zero) {
            for (i = 0; i < width; ++i) { 
                put_char(buf, '0'); 
            }
        } else {
            for (i = 0; i < width; ++i) { 
                put_char(buf, ' '); 
            }
        }
    }
    if (prefix != NULL) { 
        put_string(buf, prefix, -1, 0, -1, flag_none); 
    }
    for (i = 0; i < zeros; ++i) { 
        put_char(buf, '0'); 
    }
    for ( ; x > 0; x /= base) {
        int digit = (value / x) % base;
        put_char(buf, (char) ((digit < 10 ? '0' : (upper ? 'A' : 'a') - 10) +
            digit));
    }
    if (f & flag_minus) {
        for (i = 0; i < width; ++i) { 
            put_char(buf, ' '); 
        }
    }
}


/*
    Convert an ansi string to a unicode string. On an error, we return the original ansi string which is better than
    returning NULL. nBytes is the size of the destination buffer (ubuf) in _bytes_.
 */
char_t *gAscToUni(char_t *ubuf, char *str, ssize nBytes)
{
#if UNICODE
    if (MultiByteToWideChar(CP_ACP, 0, str, nBytes / sizeof(char_t), ubuf, nBytes / sizeof(char_t)) < 0) {
        return (char_t*) str;
    }
#else
   memcpy(ubuf, str, nBytes);
#endif
    return ubuf;
}


/*
    Convert a unicode string to an ansi string. On an error, return the original unicode string which is better than
    returning NULL.  N.B. nBytes is the number of _bytes_ in the destination buffer, buf.
 */
char *gUniToAsc(char *buf, char_t *ustr, ssize nBytes)
{
#if UNICODE
    if (WideCharToMultiByte(CP_ACP, 0, ustr, nBytes, buf, nBytes, NULL, NULL) < 0) {
        return (char*) ustr;
    }
#else
    memcpy(buf, ustr, nBytes);
#endif
    return (char*) buf;
}


/*
    Allocate (galloc) a buffer and do ascii to unicode conversion into it.  cp points to the ascii buffer.  alen is the
    length of the buffer to be converted not including a terminating NULL.  Return a pointer to the unicode buffer which
    must be gfree'd later.  Return NULL on failure to get buffer.  The buffer returned is NULL terminated.
 */
char_t *gallocAscToUni(char *cp, ssize alen)
{
    char_t  *unip;
    ssize   ulen;

    ulen = (alen + 1) * sizeof(char_t);
    if ((unip = galloc(ulen)) == NULL) {
        return NULL;
    }
    gAscToUni(unip, cp, ulen);
    unip[alen] = 0;
    return unip;
}


/*
    Allocate (galloc) a buffer and do unicode to ascii conversion into it.  unip points to the unicoded string. ulen is
    the number of characters in the unicode string not including a teminating null.  Return a pointer to the ascii
    buffer which must be gfree'd later.  Return NULL on failure to get buffer.  The buffer returned is NULL terminated.
 */
char *gallocUniToAsc(char_t *unip, ssize ulen)
{
    char    *cp;

    if ((cp = galloc(ulen+1)) == NULL) {
        return NULL;
    }
    gUniToAsc(cp, unip, ulen);
    cp[ulen] = '\0';
    return cp;
}


/*
    Convert a hex string to an integer. The end of the string or a non-hex character will indicate the end of the hex
    specification.  
 */
uint hextoi(char_t *hexstring)
{
    char_t      *h;
    uint        c, v;

    v = 0;
    h = hexstring;
    if (*h == '0' && (*(h+1) == 'x' || *(h+1) == 'X')) {
        h += 2;
    }
    while ((c = (uint)*h++) != 0) {
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'a' && c <= 'f') {
            c = (c - 'a') + 10;
        } else if (c >=  'A' && c <= 'F') {
            c = (c - 'A') + 10;
        } else {
            break;
        }
        v = (v * 0x10) + c;
    }
    return v;
}


/*
    Convert a string to an integer. If the string starts with "0x" or "0X" a hexidecimal conversion is done.
 */
uint gstrtoi(char_t *s)
{
    if (*s == '0' && (*(s+1) == 'x' || *(s+1) == 'X')) {
        s += 2;
        return hextoi(s);
    }
    return gatoi(s);
}


value_t valueInteger(long value)
{
    value_t v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = integer;
    v.value.integer = value;
    return v;
}


value_t valueSymbol(void *value)
{
    value_t v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = symbol;
    v.value.symbol = value;
    return v;
}


value_t valueString(char_t* value, int flags)
{
    value_t v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = string;
    if (flags & VALUE_ALLOCATE) {
        v.allocated = 1;
        v.value.string = gstrdup(value);
    } else {
        v.allocated = 0;
        v.value.string = value;
    }
    return v;
}


void valueFree(value_t* v)
{
    if (v->valid && v->allocated && v->type == string && v->value.string != NULL) {
        gfree(v->value.string);
    }
    v->type = undefined;
    v->valid = 0;
    v->allocated = 0;
}


#if BIT_DEBUG_LOG
/*
    Error message that doesn't need user attention. Customize this code
    to direct error messages to wherever the developer wishes
 */
void error(char_t *fmt, ...)
{
    va_list     args;
    char_t      *buf;

    va_start(args, fmt);
    gfmtValloc(&buf, BIT_LIMIT_STRING, fmt, args);
    va_end(args);
    if (traceHandler) {
        traceHandler(-1, buf);
    }
    gfree(buf);
}


void gassertError(G_ARGS_DEC, char_t *fmt, ...)
{
    va_list     args;
    char_t      *fmtBuf, *buf;

    va_start(args, fmt);
    gfmtValloc(&fmtBuf, BIT_LIMIT_STRING, fmt, args);

    gfmtAlloc(&buf, BIT_LIMIT_STRING, T("Assertion %s, failed at %s %d\n"), fmtBuf, G_ARGS); 
    va_end(args);
    gfree(fmtBuf);
    if (traceHandler) {
        traceHandler(-1, buf);
    }
    gfree(buf);
}


/*
    Trace log. Customize this function to log trace output
 */
void trace(int level, char_t *fmt, ...)
{
    va_list     args;
    char_t      *buf;

    if (level <= traceLevel) {    
        va_start(args, fmt);
        gfmtValloc(&buf, BIT_LIMIT_STRING, fmt, args);
        if (traceHandler) {
            traceHandler(level, buf);
        }
        gfree(buf);
        va_end(args);
    }
}


/*
    Trace log. Customize this function to log trace output
 */
void traceRaw(char_t *buf)
{
    if (traceHandler) {
        traceHandler(0, buf);
    }
}


/*
    Replace the default trace handler. Return a pointer to the old handler.
 */
TraceHandler traceSetHandler(TraceHandler handler)
{
    TraceHandler    oldHandler;

    oldHandler = traceHandler;
    if (handler) {
        traceHandler = handler;
    }
    return oldHandler;
}


int traceOpen()
{
    if (!tracePath) {
        /* This defintion comes from main.bit and bit.h */
        traceSetPath(BIT_TRACE);
    }
    if (gmatch(tracePath, "stdout")) {
        traceFd = 1;
    } else if (gmatch(tracePath, "stderr")) {
        traceFd = 2;
    } else if ((traceFd = gopen(tracePath, O_CREAT | O_TRUNC | O_APPEND | O_WRONLY, 0666)) < 0) {
        return -1;
    }
    traceSetHandler(traceHandler);
    return 0;
}


void traceClose()
{
    if (traceFd >= 0) {
        close(traceFd);                                                                              
        traceFd = -1;                                                                                
    }                                                                                                    
}


void traceSetPath(char_t *path)
{
    char_t  *lp;
    
    gfree(tracePath);
    tracePath = gstrdup(path);
    if ((lp = strchr(tracePath, ':')) != 0) {
        *lp++ = '\0';
        traceLevel = atoi(lp);
    }
}


static void defaultTraceHandler(int level, char_t *buf)
{
    char    *abuf;
    ssize   len;

    if (traceFd >= 0) {
        len = gstrlen(buf);
        //  MOB OPT
        abuf = gallocUniToAsc(buf, len + 1);
        write(traceFd, abuf, len);
        gfree(abuf);
    }
}

#else /* !BIT_DEBUG_LOG */
void error(char_t *fmt, ...) { }
void trace(int level, char_t *fmt, ...) { }
#endif /* BIT_DEBUG_LOG */

/*
    Convert a string to lower case
 */
char_t *gstrlower(char_t *string)
{
    char_t  *s;

    gassert(string);

    if (string == NULL) {
        return NULL;
    }
    s = string;
    while (*s) {
        if (gisupper(*s)) {
            *s = (char_t) gtolower(*s);
        }
        s++;
    }
    *s = '\0';
    return string;
}


/* 
    Convert a string to upper case
 */
char_t *gstrupper(char_t *string)
{
    char_t  *s;

    gassert(string);
    if (string == NULL) {
        return NULL;
    }
    s = string;
    while (*s) {
        if (gislower(*s)) {
            *s = (char_t) gtoupper(*s);
        }
        s++;
    }
    *s = '\0';
    return string;
}


/*
    Convert integer to ascii string. Allow a NULL string in which case we allocate a dynamic buffer. 
 */
char_t *gstritoa(int n, char_t *string, int width)
{
    char_t  *cp, *lim, *s;
    char_t  buf[16];                        /* Just temp to hold number */
    int     next, minus;

    gassert(string && width > 0);

    if (string == NULL) {
        if (width == 0) {
            width = 10;
        }
        if ((string = galloc(width + 1)) == NULL) {
            return NULL;
        }
    }
    if (n < 0) {
        minus = 1;
        n = -n;
        width--;
    } else {
        minus = 0;
    }
    cp = buf;
    lim = &buf[width - 1];
    while (n > 9 && cp < lim) {
        next = n;
        n /= 10;
        *cp++ = (char_t) (next - n * 10 + '0');
    }
    if (cp < lim) {
        *cp++ = (char_t) (n + '0');
    }
    s = string;
    if (minus) {
        *s++ = '-';
    }
    while (cp > buf) {
        *s++ = *--cp;
    }
    *s++ = '\0';
    return string;
}


/*
    Allocate a new file handle.  On the first call, the caller must set the handle map to be a pointer to a null
    pointer.  map points to the second element in the handle array.
 */
int gallocHandle(void ***map)
{
    ssize   *mp;
    int     handle, len, memsize, incr;

    gassert(map);

    if (*map == NULL) {
        incr = H_INCR;
        memsize = (incr + H_OFFSET) * sizeof(void**);
        if ((mp = galloc(memsize)) == NULL) {
            return -1;
        }
        memset(mp, 0, memsize);
        mp[H_LEN] = incr;
        mp[H_USED] = 0;
        *map = (void**) &mp[H_OFFSET];
    } else {
        mp = &((*(ssize**)map)[-H_OFFSET]);
    }
    len = (int) mp[H_LEN];

    /*
        Find the first null handle
     */
    if (mp[H_USED] < mp[H_LEN]) {
        for (handle = 0; handle < len; handle++) {
            if (mp[handle + H_OFFSET] == 0) {
                mp[H_USED]++;
                return handle;
            }
        }
    } else {
        handle = len;
    }

    /*
        No free handle so grow the handle list. Grow list in chunks of H_INCR.
     */
    len += H_INCR;
    memsize = (len + H_OFFSET) * sizeof(void**);
    if ((mp = grealloc(mp, memsize)) == NULL) {
        return -1;
    }
    *map = (void**) &mp[H_OFFSET];
    mp[H_LEN] = len;
    memset(&mp[H_OFFSET + len - H_INCR], 0, sizeof(ssize*) * H_INCR);
    mp[H_USED]++;
    return handle;
}


/*
    Free a handle. This function returns the value of the largest handle in use plus 1, to be saved as a max value.
 */
int gfreeHandle(void ***map, int handle)
{
    ssize   *mp;
    int     len;

    gassert(map);
    mp = &((*(ssize**)map)[-H_OFFSET]);
    gassert(mp[H_LEN] >= H_INCR);

    gassert(mp[handle + H_OFFSET]);
    gassert(mp[H_USED]);
    mp[handle + H_OFFSET] = 0;
    if (--(mp[H_USED]) == 0) {
        gfree((void*) mp);
        *map = NULL;
    }
    /*
        Find the greatest handle number in use.
     */
    if (*map == NULL) {
        handle = -1;
    } else {
        len = (int) mp[H_LEN];
        if (mp[H_USED] < mp[H_LEN]) {
            for (handle = len - 1; handle >= 0; handle--) {
                if (mp[handle + H_OFFSET])
                    break;
            }
        } else {
            handle = len;
        }
    }
    return handle + 1;
}


/*
    Allocate an entry in the halloc array
 */
int gallocEntry(void ***list, int *max, int size)
{
    char_t  *cp;
    int     id;

    gassert(list);
    gassert(max);

    if ((id = gallocHandle((void***) list)) < 0) {
        return -1;
    }
    if (size > 0) {
        if ((cp = galloc(size)) == NULL) {
            gfreeHandle(list, id);
            return -1;
        }
        gassert(cp);
        memset(cp, 0, size);
        (*list)[id] = (void*) cp;
    }
    if (id >= *max) {
        *max = id + 1;
    }
    return id;
}

    
/*
    Create a new ringq. "increment" is the amount to increase the size of the ringq should it need to grow to accomodate
    data being added. "maxsize" is an upper limit (sanity level) beyond which the q must not grow. Set maxsize to -1 to
    imply no upper limit. The buffer for the ringq is always *  dynamically allocated. Set maxsize
 */
int ringqOpen(ringq_t *rq, int initSize, int maxsize)
{
    int increment;

    gassert(rq);
    gassert(initSize >= 0);

    increment = getBinBlockSize(initSize);
    if ((rq->buf = galloc((increment))) == NULL) {
        return -1;
    }
    rq->maxsize = maxsize;
    rq->buflen = increment;
    rq->increment = increment;
    rq->endbuf = &rq->buf[rq->buflen];
    rq->servp = rq->buf;
    rq->endp = rq->buf;
    *rq->servp = '\0';
    return 0;
}


/*
    Delete a ringq and free the ringq buffer.
 */
void ringqClose(ringq_t *rq)
{
    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if (rq == NULL) {
        return;
    }

    ringqFlush(rq);
    gfree((char*) rq->buf);
    rq->buf = NULL;
}


/*
    Return the length of the data in the ringq. Users must fill the queue to a high water mark of at most one less than
    the queue size.  
 */
ssize ringqLen(ringq_t *rq)
{
    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if (rq->servp > rq->endp) {
        return rq->buflen + rq->endp - rq->servp;
    } else {
        return rq->endp - rq->servp;
    }
}


/*
    Get a byte from the queue
 */
int ringqGetc(ringq_t *rq)
{
    char_t  c;
    char_t* cp;

    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if (rq->servp == rq->endp) {
        return -1;
    }

    cp = (char_t*) rq->servp;
    c = *cp++;
    rq->servp = (uchar *) cp;
    if (rq->servp >= rq->endbuf) {
        rq->servp = rq->buf;
    }
    return (int) ((uchar) c);
}


/*
    Add a char to the queue. Note if being used to store wide strings this does not add a trailing '\0'. Grow the q as
    required.  
 */
int ringqPutc(ringq_t *rq, char_t c)
{
    char_t *cp;

    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if ((ringqPutBlkMax(rq) < (int) sizeof(char_t)) && !ringqGrow(rq)) {
        return -1;
    }

    cp = (char_t*) rq->endp;
    *cp++ = (char_t) c;
    rq->endp = (uchar *) cp;
    if (rq->endp >= rq->endbuf) {
        rq->endp = rq->buf;
    }
    return 0;
}


/*
    Insert a wide character at the front of the queue
 */
int ringqInsertc(ringq_t *rq, char_t c)
{
    char_t *cp;

    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if (ringqPutBlkMax(rq) < (int) sizeof(char_t) && !ringqGrow(rq)) {
        return -1;
    }
    if (rq->servp <= rq->buf) {
        rq->servp = rq->endbuf;
    }
    cp = (char_t*) rq->servp;
    *--cp = (char_t) c;
    rq->servp = (uchar *) cp;
    return 0;
}


/*
    Add a string to the queue. Add a trailing null (maybe two nulls)
 */
ssize ringqPutStr(ringq_t *rq, char_t *str)
{
    ssize   rc;

    gassert(rq);
    gassert(str);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    rc = ringqPutBlk(rq, (uchar*) str, gstrlen(str) * sizeof(char_t));
    *((char_t*) rq->endp) = (char_t) '\0';
    return rc;
}


/*
    Add a null terminator. This does NOT increase the size of the queue
 */
void ringqAddNull(ringq_t *rq)
{
    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    *((char_t*) rq->endp) = (char_t) '\0';
}


#if UNICODE
//  MOB - this should be the same code?

/*
    Get a byte from the queue
 */
int ringqGetcA(ringq_t *rq)
{
    uchar   c;

    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if (rq->servp == rq->endp) {
        return -1;
    }

    c = *rq->servp++;
    if (rq->servp >= rq->endbuf) {
        rq->servp = rq->buf;
    }
    return c;
}


/*
    Add a byte to the queue. Note if being used to store strings this does not add a trailing '\0'. Grow the q as required.
 */
int ringqPutcA(ringq_t *rq, char c)
{
    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if (ringqPutBlkMax(rq) == 0 && !ringqGrow(rq)) {
        return -1;
    }
    *rq->endp++ = (uchar) c;
    if (rq->endp >= rq->endbuf) {
        rq->endp = rq->buf;
    }
    return 0;
}


/*
    Insert a byte at the front of the queue
 */
int ringqInsertcA(ringq_t *rq, char c)
{
    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if (ringqPutBlkMax(rq) == 0 && !ringqGrow(rq)) {
        return -1;
    }
    if (rq->servp <= rq->buf) {
        rq->servp = rq->endbuf;
    }
    *--rq->servp = (uchar) c;
    return 0;
}


/*
    Add a string to the queue. Add a trailing null (not really in the q). ie. beyond the last valid byte.
 */
int ringqPutStrA(ringq_t *rq, char *str)
{
    int     rc;

    gassert(rq);
    gassert(str);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    rc = ringqPutBlk(rq, (uchar*) str, strlen(str));
    rq->endp[0] = '\0';
    return rc;
}

#endif /* UNICODE */

/*
    Add a block of data to the ringq. Return the number of bytes added. Grow the q as required.
 */
ssize ringqPutBlk(ringq_t *rq, uchar *buf, ssize size)
{
    ssize   this, bytes_put;

    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));
    gassert(buf);
    gassert(0 <= size);

    /*
        Loop adding the maximum bytes we can add in a single straight line copy
     */
    bytes_put = 0;
    while (size > 0) {
        this = min(ringqPutBlkMax(rq), size);
        if (this <= 0) {
            if (! ringqGrow(rq)) {
                break;
            }
            this = min(ringqPutBlkMax(rq), size);
        }
        memcpy(rq->endp, buf, this);
        buf += this;
        rq->endp += this;
        size -= this;
        bytes_put += this;

        if (rq->endp >= rq->endbuf) {
            rq->endp = rq->buf;
        }
    }
    return bytes_put;
}


/*
    Get a block of data from the ringq. Return the number of bytes returned.
 */
ssize ringqGetBlk(ringq_t *rq, uchar *buf, ssize size)
{
    ssize   this, bytes_read;

    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));
    gassert(buf);
    gassert(0 <= size && size < rq->buflen);

    /*
        Loop getting the maximum bytes we can get in a single straight line copy
     */
    bytes_read = 0;
    while (size > 0) {
        this = ringqGetBlkMax(rq);
        this = min(this, size);
        if (this <= 0) {
            break;
        }
        memcpy(buf, rq->servp, this);
        buf += this;
        rq->servp += this;
        size -= this;
        bytes_read += this;

        if (rq->servp >= rq->endbuf) {
            rq->servp = rq->buf;
        }
    }
    return bytes_read;
}


/*
    Return the maximum number of bytes the ring q can accept via a single block copy. Useful if the user is doing their
    own data insertion.  
 */
ssize ringqPutBlkMax(ringq_t *rq)
{
    ssize   space, in_a_line;

    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));
    
    space = rq->buflen - RINGQ_LEN(rq) - 1;
    in_a_line = rq->endbuf - rq->endp;

    return min(in_a_line, space);
}


/*
    Return the maximum number of bytes the ring q can provide via a single block copy. Useful if the user is doing their
    own data retrieval.  
 */
ssize ringqGetBlkMax(ringq_t *rq)
{
    ssize   len, in_a_line;

    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    len = RINGQ_LEN(rq);
    in_a_line = rq->endbuf - rq->servp;

    return min(in_a_line, len);
}


/*
    Adjust the endp pointer after the user has copied data into the queue.
 */
void ringqPutBlkAdj(ringq_t *rq, ssize size)
{
    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));
    gassert(0 <= size && size < rq->buflen);

    rq->endp += size;
    if (rq->endp >= rq->endbuf) {
        rq->endp -= rq->buflen;
    }
    /*
        Flush the queue if the endp pointer is corrupted via a bad size
     */
    if (rq->endp >= rq->endbuf) {
        error(T("Bad end pointer"));
        ringqFlush(rq);
    }
}


/*
    Adjust the servp pointer after the user has copied data from the queue.
 */
void ringqGetBlkAdj(ringq_t *rq, ssize size)
{
    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));
    gassert(0 < size && size < rq->buflen);

    rq->servp += size;
    if (rq->servp >= rq->endbuf) {
        rq->servp -= rq->buflen;
    }
    /*
        Flush the queue if the servp pointer is corrupted via a bad size
     */
    if (rq->servp >= rq->endbuf) {
        error(T("Bad serv pointer"));
        ringqFlush(rq);
    }
}


/*
    Flush all data in a ring q. Reset the pointers.
 */
void ringqFlush(ringq_t *rq)
{
    gassert(rq);
    gassert(rq->servp);

    rq->servp = rq->buf;
    rq->endp = rq->buf;
    if (rq->servp) {
        *rq->servp = '\0';
    }
}


/*
    Grow the buffer. Return true if the buffer can be grown. Grow using the increment size specified when opening the
    ringq. Don't grow beyond the maximum possible size.
 */
static int ringqGrow(ringq_t *rq)
{
    uchar   *newbuf;
    ssize   len;

    gassert(rq);

    if (rq->maxsize >= 0 && rq->buflen >= rq->maxsize) {
        return 0;
    }
    len = ringqLen(rq);
    if ((newbuf = galloc(rq->buflen + rq->increment)) == NULL) {
        return 0;
    }
    ringqGetBlk(rq, newbuf, ringqLen(rq));
    gfree((char*) rq->buf);

    rq->buflen += rq->increment;
    rq->endp = newbuf;
    rq->servp = newbuf;
    rq->buf = newbuf;
    rq->endbuf = &rq->buf[rq->buflen];
    ringqPutBlk(rq, newbuf, len);

    /*
        Double the increment so the next grow will line up with galloc'ed memory
     */
    rq->increment = getBinBlockSize(2 * rq->increment);
    return 1;
}


/*
    Find the smallest binary memory size that "size" will fit into.  This makes the ringq and ringqGrow routines much
    more efficient.  The galloc routine likes powers of 2 minus 1.
 */
static int  getBinBlockSize(int size)
{
    int q;

    size = size >> G_SHIFT;
    for (q = 0; size; size >>= 1) {
        q++;
    }
    return (1 << (G_SHIFT + q));
}


int symSubOpen()
{
    //  MOB - why keep count?
    if (++symOpenCount == 1) {
        symMax = 0;
        sym = NULL;
    }
    return 0;
}


void symSubClose()
{
    if (--symOpenCount <= 0) {
        symOpenCount = 0;
    }
}


sym_fd_t symOpen(int hash_size)
{
    sym_fd_t        sd;
    sym_tabent_t    *tp;

    gassert(hash_size > 2);

    /*
        Create a new handle for this symbol table
     */
    if ((sd = gallocHandle((void***) &sym)) < 0) {
        return -1;
    }

    /*
        Create a new symbol table structure and zero
     */
    if ((tp = (sym_tabent_t*) galloc(sizeof(sym_tabent_t))) == NULL) {
        symMax = gfreeHandle((void***) &sym, sd);
        return -1;
    }
    memset(tp, 0, sizeof(sym_tabent_t));
    if (sd >= symMax) {
        symMax = sd + 1;
    }
    gassert(0 <= sd && sd < symMax);
    sym[sd] = tp;

    /*
        Now create the hash table for fast indexing.
     */
    tp->hash_size = calcPrime(hash_size);
    tp->hash_table = (sym_t**) galloc(tp->hash_size * sizeof(sym_t*));
    gassert(tp->hash_table);
    memset(tp->hash_table, 0, tp->hash_size * sizeof(sym_t*));

    return sd;
}


/*
    Close this symbol table. Call a cleanup function to allow the caller to free resources associated with each symbol
    table entry.  
 */
void symClose(sym_fd_t sd)
{
    sym_tabent_t    *tp;
    sym_t           *sp, *forw;
    int             i;

    gassert(0 <= sd && sd < symMax);
    tp = sym[sd];
    gassert(tp);

    /*
        Free all symbols in the hash table, then the hash table itself.
     */
    for (i = 0; i < tp->hash_size; i++) {
        for (sp = tp->hash_table[i]; sp; sp = forw) {
            forw = sp->forw;
            valueFree(&sp->name);
            valueFree(&sp->content);
            gfree((void*) sp);
            sp = forw;
        }
    }
    gfree((void*) tp->hash_table);
    symMax = gfreeHandle((void***) &sym, sd);
    gfree((void*) tp);
}


/*
    Return the first symbol in the hashtable if there is one. This call is used as the first step in traversing the
    table. A call to symFirst should be followed by calls to symNext to get all the rest of the entries.
 */
sym_t* symFirst(sym_fd_t sd)
{
    sym_tabent_t    *tp;
    sym_t           *sp, *forw;
    int             i;

    gassert(0 <= sd && sd < symMax);
    tp = sym[sd];
    gassert(tp);

    /*
        Find the first symbol in the hashtable and return a pointer to it.
     */
    for (i = 0; i < tp->hash_size; i++) {
        for (sp = tp->hash_table[i]; sp; sp = forw) {
            forw = sp->forw;

            if (forw == NULL) {
                htIndex = i + 1;
                next = tp->hash_table[htIndex];
            } else {
                htIndex = i;
                next = forw;
            }
            return sp;
        }
    }
    return NULL;
}


/*
    Return the next symbol in the hashtable if there is one. See symFirst.
 */
sym_t* symNext(sym_fd_t sd)
{
    sym_tabent_t    *tp;
    sym_t           *sp, *forw;
    int             i;

    gassert(0 <= sd && sd < symMax);
    tp = sym[sd];
    gassert(tp);

    /*
        Find the first symbol in the hashtable and return a pointer to it.
     */
    for (i = htIndex; i < tp->hash_size; i++) {
        for (sp = next; sp; sp = forw) {
            forw = sp->forw;

            if (forw == NULL) {
                htIndex = i + 1;
                next = tp->hash_table[htIndex];
            } else {
                htIndex = i;
                next = forw;
            }
            return sp;
        }
        next = tp->hash_table[i + 1];
    }
    return NULL;
}


/*
    Lookup a symbol and return a pointer to the symbol entry. If not present then return a NULL.
 */
sym_t *symLookup(sym_fd_t sd, char_t *name)
{
    sym_tabent_t    *tp;
    sym_t           *sp;
    char_t          *cp;

    gassert(0 <= sd && sd < symMax);
    if ((tp = sym[sd]) == NULL) {
        return NULL;
    }

    if (name == NULL || *name == '\0') {
        return NULL;
    }

    /*
        Do an initial hash and then follow the link chain to find the right entry
     */
    for (sp = hash(tp, name); sp; sp = sp->forw) {
        cp = sp->name.value.string;
        if (cp[0] == name[0] && gstrcmp(cp, name) == 0) {
            break;
        }
    }
    return sp;
}


/*
    Enter a symbol into the table. If already there, update its value.  Always succeeds if memory available. We allocate
    a copy of "name" here so it can be a volatile variable. The value "v" is just a copy of the passed in value, so it
    MUST be persistent.
 */
sym_t *symEnter(sym_fd_t sd, char_t *name, value_t v, int arg)
{
    sym_tabent_t    *tp;
    sym_t           *sp, *last;
    char_t          *cp;
    int             hindex;

    gassert(name);
    gassert(0 <= sd && sd < symMax);
    tp = sym[sd];
    gassert(tp);

    /*
        Calculate the first daisy-chain from the hash table. If non-zero, then we have daisy-chain, so scan it and look
        for the symbol.  
     */
    last = NULL;
    hindex = hashIndex(tp, name);
    if ((sp = tp->hash_table[hindex]) != NULL) {
        for (; sp; sp = sp->forw) {
            cp = sp->name.value.string;
            if (cp[0] == name[0] && gstrcmp(cp, name) == 0) {
                break;
            }
            last = sp;
        }
        if (sp) {
            /*
                Found, so update the value If the caller stores handles which require freeing, they will be lost here.
                It is the callers responsibility to free resources before overwriting existing contents. We will here
                free allocated strings which occur due to value_instring().  We should consider providing the cleanup
                function on the open rather than the close and then we could call it here and solve the problem.
             */
            if (sp->content.valid) {
                valueFree(&sp->content);
            }
            sp->content = v;
            sp->arg = arg;
            return sp;
        }
        /*
            Not found so allocate and append to the daisy-chain
         */
        sp = (sym_t*) galloc(sizeof(sym_t));
        if (sp == NULL) {
            return NULL;
        }
        sp->name = valueString(name, VALUE_ALLOCATE);
        sp->content = v;
        sp->forw = (sym_t*) NULL;
        sp->arg = arg;
        last->forw = sp;

    } else {
        /*
            Daisy chain is empty so we need to start the chain
         */
        sp = (sym_t*) galloc(sizeof(sym_t));
        if (sp == NULL) {
            return NULL;
        }
        tp->hash_table[hindex] = sp;
        tp->hash_table[hashIndex(tp, name)] = sp;

        sp->forw = (sym_t*) NULL;
        sp->content = v;
        sp->arg = arg;
        sp->name = valueString(name, VALUE_ALLOCATE);
    }
    return sp;
}


/*
    Delete a symbol from a table
 */
int symDelete(sym_fd_t sd, char_t *name)
{
    sym_tabent_t    *tp;
    sym_t           *sp, *last;
    char_t          *cp;
    int             hindex;

    gassert(name && *name);
    gassert(0 <= sd && sd < symMax);
    tp = sym[sd];
    gassert(tp);

    /*
        Calculate the first daisy-chain from the hash table. If non-zero, then we have daisy-chain, so scan it and look
        for the symbol.  
     */
    last = NULL;
    hindex = hashIndex(tp, name);
    if ((sp = tp->hash_table[hindex]) != NULL) {
        for ( ; sp; sp = sp->forw) {
            cp = sp->name.value.string;
            if (cp[0] == name[0] && gstrcmp(cp, name) == 0) {
                break;
            }
            last = sp;
        }
    }
    if (sp == (sym_t*) NULL) {              /* Not Found */
        return -1;
    }

    /*
         Unlink and free the symbol. Last will be set if the element to be deleted is not first in the chain.
     */
    if (last) {
        last->forw = sp->forw;
    } else {
        tp->hash_table[hindex] = sp->forw;
    }
    valueFree(&sp->name);
    valueFree(&sp->content);
    gfree((void*) sp);

    return 0;
}


/*
    Hash a symbol and return a pointer to the hash daisy-chain list. All symbols reside on the chain (ie. none stored in
    the hash table itself) 
 */
static sym_t *hash(sym_tabent_t *tp, char_t *name)
{
    gassert(tp);

    return tp->hash_table[hashIndex(tp, name)];
}


/*
    Compute the hash function and return an index into the hash table We use a basic additive function that is then made
    modulo the size of the table.
 */
static int hashIndex(sym_tabent_t *tp, char_t *name)
{
    uint        sum;
    int         i;

    gassert(tp);
    /*
        Add in each character shifted up progressively by 7 bits. The shift amount is rounded so as to not shift too
        far. It thus cycles with each new cycle placing character shifted up by one bit.
     */
    i = 0;
    sum = 0;
    while (*name) {
        sum += (((int) *name++) << i);
        i = (i + 7) % (BITS(int) - BITSPERBYTE);
    }
    return sum % tp->hash_size;
}


/*
    Check if this number is a prime
 */
static int isPrime(int n)
{
    int     i, max;

    gassert(n > 0);

    max = n / 2;
    for (i = 2; i <= max; i++) {
        if (n % i == 0) {
            return 0;
        }
    }
    return 1;
}


/*
    Calculate the largest prime smaller than size.
 */
static int calcPrime(int size)
{
    int count;

    gassert(size > 0);

    for (count = size; count > 0; count--) {
        if (isPrime(count)) {
            return count;
        }
    }
    return 1;
}


int gopen(char_t *path, int oflags, int mode)
{
    int     fd;

#if BIT_WIN_LIKE
    #if UNICODE
        fd = _wopen(path, oflags, mode);
    #else
        if (_sopen_s(&fd, path, oflags, _SH_DENYNO, mode & 0600) != 0) {
			fd = -1;
		}
    #endif
#else
    fd = open(path, oflags, mode);
#endif
#if VXWORKS
    if (oflags & O_APPEND) {
        lseek(fd, 0, SEEK_END);
    }
#endif
    return fd;
}

int gcaselesscmp(char_t *s1, char_t *s2)
{
    if (s1 == 0 || s2 == 0) {
        return -1;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return gncaselesscmp(s1, s2, max(glen(s1), glen(s2)));
}


bool gcaselessmatch(char_t *s1, char_t *s2)
{
    return gcaselesscmp(s1, s2) == 0;
}


bool gmatch(char_t *s1, char_t *s2)
{
    return gcmp(s1, s2) == 0;
}


int gcmp(char_t *s1, char_t *s2)
{
    if (s1 == s2) {
        return 0;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return gncmp(s1, s2, max(glen(s1), glen(s2)));
}


ssize glen(char_t *s)
{
    return s ? strlen(s) : 0;
}


/*
    Case sensitive string comparison. Limited by length
 */
int gncmp(char_t *s1, char_t *s2, ssize n)
{
    int     rc;

    gassert(0 <= n && n < MAXINT);

    if (s1 == 0 && s2 == 0) {
        return 0;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    for (rc = 0; n > 0 && *s1 && rc == 0; s1++, s2++, n--) {
        rc = *s1 - *s2;
    }
    if (rc) {
        return (rc > 0) ? 1 : -1;
    } else if (n == 0) {
        return 0;
    } else if (*s1 == '\0' && *s2 == '\0') {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else if (*s2 == '\0') {
        return 1;
    }
    return 0;
}


int gncaselesscmp(char_t *s1, char_t *s2, ssize n)
{
    int     rc;

    gassert(0 <= n && n < MAXINT);

    if (s1 == 0 || s2 == 0) {
        return -1;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    for (rc = 0; n > 0 && *s1 && rc == 0; s1++, s2++, n--) {
        rc = tolower((uchar) *s1) - tolower((uchar) *s2);
    }
    if (rc) {
        return (rc > 0) ? 1 : -1;
    } else if (n == 0) {
        return 0;
    } else if (*s1 == '\0' && *s2 == '\0') {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else if (*s2 == '\0') {
        return 1;
    }
    return 0;
}


/*
    Parse the args and return the count of args. If argv is NULL, the args are parsed read-only. If argv is set,
    then the args will be extracted, back-quotes removed and argv will be set to point to all the args.
    NOTE: this routine does not allocate.
 */
int egParseArgs(char *args, char **argv, int maxArgc)
{
    char    *dest, *src, *start;
    int     quote, argc;

    /*
        Example     "showColors" red 'light blue' "yellow white" 'Can\'t \"render\"'
        Becomes:    ["showColors", "red", "light blue", "yellow white", "Can't \"render\""]
     */
    for (argc = 0, src = args; src && *src != '\0' && argc < maxArgc; argc++) {
        while (isspace((uchar) *src)) {
            src++;
        }
        if (*src == '\0')  {
            break;
        }
        start = dest = src;
        if (*src == '"' || *src == '\'') {
            quote = *src;
            src++; 
            dest++;
        } else {
            quote = 0;
        }
        if (argv) {
            argv[argc] = src;
        }
        while (*src) {
            if (*src == '\\' && src[1] && (src[1] == '\\' || src[1] == '"' || src[1] == '\'')) { 
                src++;
            } else {
                if (quote) {
                    if (*src == quote && !(src > start && src[-1] == '\\')) {
                        break;
                    }
                } else if (*src == ' ') {
                    break;
                }
            }
            if (argv) {
                *dest++ = *src;
            }
            src++;
        }
        if (*src != '\0') {
            src++;
        }
        if (argv) {
            *dest++ = '\0';
        }
    }
    return argc;
}

#if VXWORKS

/*
    Get absolute path.  In VxWorks, functions like chdir, ioctl for mkdir and ioctl for rmdir, require an absolute path.
    This function will take the path argument and convert it to an absolute path.  It is the caller's responsibility to
    deallocate the returned string. 
 */
static char_t *getAbsolutePath(char_t *path)
{
#if _WRS_VXWORKS_MAJOR >= 6
    const char_t  *tail;
#else
    char_t        *tail;
#endif
    char_t  *dev;

    /*
        Determine if path is relative or absolute.  If relative, prepend the current working directory to the name.
        Otherwise, use it.  Note the getcwd call below must not be ggetcwd or else we go into an infinite loop
    */

    if (iosDevFind(path, &tail) != NULL && path != tail) {
        return gstrdup(path);
    }
    dev = galloc(BIT_LIMIT_FILENAME);
    getcwd(dev, BIT_LIMIT_FILENAME);
    strcat(dev, "/");
    strcat(dev, path);
    return dev;
}


#if UNUSED
int vxchdir(char_t *dirname)
{
    char_t  *path;
    int     rc;

    path = getAbsolutePath(dirname);
    rc = chdir(path);
    gfree(path);
    return rc;
}
#endif
#endif

#if ECOS
int send(int s, const void *buf, size_t len, int flags)
{
    return write(s, buf, len);
}


int recv(int s, void *buf, size_t len, int flags)
{
    return read(s, buf, len);
}
#endif

#if WINDOWS

void egSetInst(HINSTANCE inst)
{
    appInstance = inst;
}


HINSTANCE egGetInst()
{
    return appInstance;
}
#endif

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
