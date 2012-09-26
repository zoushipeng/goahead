/*
    runtime.c -- Runtime support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/*********************************** Defines **********************************/
/*
    This structure stores scheduled events.
 */
typedef struct Callback {
    void    (*routine)(void *arg, int id);
    void    *arg;
    time_t  at;
    int     id;
} Callback;


/*********************************** Defines **********************************/
/*
    Class definitions
 */
#define CLASS_NORMAL    0               /* [All other]      Normal characters */
#define CLASS_PERCENT   1               /* [%]              Begin format */
#define CLASS_MODIFIER  2               /* [-+ #,]          Modifiers */
#define CLASS_ZERO      3               /* [0]              Special modifier - zero pad */
#define CLASS_STAR      4               /* [*]              Width supplied by arg */
#define CLASS_DIGIT     5               /* [1-9]            Field widths */
#define CLASS_DOT       6               /* [.]              Introduce precision */
#define CLASS_BITS      7               /* [hlL]            Length bits */
#define CLASS_TYPE      8               /* [cdefginopsSuxX] Type specifiers */

#define STATE_NORMAL    0               /* Normal chars in format string */
#define STATE_PERCENT   1               /* "%" */
#define STATE_MODIFIER  2               /* -+ #,*/
#define STATE_WIDTH     3               /* Width spec */
#define STATE_DOT       4               /* "." */
#define STATE_PRECISION 5               /* Precision spec */
#define STATE_BITS      6               /* Size spec */
#define STATE_TYPE      7               /* Data type */
#define STATE_COUNT     8

char stateMap[] = {
    /*     STATES:  Normal Percent Modifier Width  Dot  Prec Bits Type */
    /* CLASS           0      1       2       3     4     5    6    7  */
    /* Normal   0 */   0,     0,      0,      0,    0,    0,   0,   0,
    /* Percent  1 */   1,     0,      1,      1,    1,    1,   1,   1,
    /* Modifier 2 */   0,     2,      2,      0,    0,    0,   0,   0,
    /* Zero     3 */   0,     2,      2,      3,    5,    5,   0,   0,
    /* Star     4 */   0,     3,      3,      0,    5,    0,   0,   0,
    /* Digit    5 */   0,     3,      3,      3,    5,    5,   0,   0,
    /* Dot      6 */   0,     4,      4,      4,    0,    0,   0,   0,
    /* Bits     7 */   0,     6,      6,      6,    6,    6,   6,   0,
    /* Types    8 */   0,     7,      7,      7,    7,    7,   7,   0,
};

/*
    Format:         %[modifier][width][precision][bits][type]
  
    The Class map will map from a specifier letter to a state.
 */
char classMap[] = {
    /*   0  ' '    !     "     #     $     %     &     ' */
             2,    0,    0,    2,    0,    1,    0,    0,
    /*  07   (     )     *     +     ,     -     .     / */
             0,    0,    4,    2,    2,    2,    6,    0,
    /*  10   0     1     2     3     4     5     6     7 */
             3,    5,    5,    5,    5,    5,    5,    5,
    /*  17   8     9     :     ;     <     =     >     ? */
             5,    5,    0,    0,    0,    0,    0,    0,
    /*  20   @     A     B     C     D     E     F     G */
             8,    0,    0,    0,    0,    0,    0,    0,
    /*  27   H     I     J     K     L     M     N     O */
             0,    0,    0,    0,    7,    0,    8,    0,
    /*  30   P     Q     R     S     T     U     V     W */
             0,    0,    0,    8,    0,    0,    0,    0,
    /*  37   X     Y     Z     [     \     ]     ^     _ */
             8,    0,    0,    0,    0,    0,    0,    0,
    /*  40   '     a     b     c     d     e     f     g */
             0,    0,    0,    8,    8,    8,    8,    8,
    /*  47   h     i     j     k     l     m     n     o */
             7,    8,    0,    0,    7,    0,    8,    8,
    /*  50   p     q     r     s     t     u     v     w */
             8,    0,    0,    8,    0,    8,    0,    8,
    /*  57   x     y     z  */
             8,    0,    0,
};

/*
    Flags
 */
#define SPRINTF_LEFT        0x1         /* Left align */
#define SPRINTF_SIGN        0x2         /* Always sign the result */
#define SPRINTF_LEAD_SPACE  0x4         /* put leading space for +ve numbers */
#define SPRINTF_ALTERNATE   0x8         /* Alternate format */
#define SPRINTF_LEAD_ZERO   0x10        /* Zero pad */
#define SPRINTF_SHORT       0x20        /* 16-bit */
#define SPRINTF_LONG        0x40        /* 32-bit */
#define SPRINTF_INT64       0x80        /* 64-bit */
#define SPRINTF_COMMA       0x100       /* Thousand comma separators */
#define SPRINTF_UPPER_CASE  0x200       /* As the name says for numbers */

typedef struct Format {
    uchar   *buf;
    uchar   *endbuf;
    uchar   *start;
    uchar   *end;
    ssize   growBy;
    ssize   maxsize;
    int     precision;
    int     radix;
    int     width;
    int     flags;
    int     len;
} Format;

#define BPUT(fmt, c) \
    if (1) { \
        /* Less one to allow room for the null */ \
        if ((fmt)->end >= ((fmt)->endbuf - sizeof(char))) { \
            if (growBuf(fmt) > 0) { \
                *(fmt)->end++ = (c); \
            } \
        } else { \
            *(fmt)->end++ = (c); \
        } \
    } else

#define BPUTNULL(fmt) \
    if (1) { \
        if ((fmt)->end > (fmt)->endbuf) { \
            if (growBuf(fmt) > 0) { \
                *(fmt)->end = '\0'; \
            } \
        } else { \
            *(fmt)->end = '\0'; \
        } \
    } else 

/*
    The handle list stores the length of the list and the number of used handles in the first two words.  These are
    hidden from the caller by returning a pointer to the third word to the caller.
 */
#define H_LEN       0       /* First entry holds length of list */
#define H_USED      1       /* Second entry holds number of used */
#define H_OFFSET    2       /* Offset to real start of list */
#define H_INCR      16      /* Grow handle list in chunks this size */

#define RINGQ_LEN(rq) ((rq->servp > rq->endp) ? (rq->buflen + (rq->endp - rq->servp)) : (rq->endp - rq->servp))

typedef struct SymTab {                 /* Symbol table descriptor */
    WebsKey     **hash_table;           /* Allocated at run time */
    int         inuse;                  /* Is this entry in use */
    int         size;                   /* Size of the table below */
} SymTab;

#ifndef LOG_ERR
    #define LOG_ERR 0
#endif
#if BIT_WIN_LIKE
    static HINSTANCE appInstance;
    static void syslog(int priority, char *fmt, ...);
#endif

/************************************* Locals *********************************/

static Callback **callbacks;
static int      callbackMax;

static SymTab   **sym;              /* List of symbol tables */
static int      symMax;             /* One past the max symbol table */
static char     *tracePath;
static int      traceFd;            /* Log file handle */
static int      traceLevel;

char* embedthisGoAheadCopyright = EMBEDTHIS_GOAHEAD_COPYRIGHT;

/********************************** Forwards **********************************/

static int calcPrime(int size);
static int getBinBlockSize(int size);
static int hashIndex(SymTab *tp, char *name);
static WebsKey *hash(SymTab *tp, char *name);
static void defaultTraceHandler(int level, char *buf);
static WebsTraceHandler traceHandler = defaultTraceHandler;

static int  getState(char c, int state);
static int  growBuf(Format *fmt);
static char *sprintfCore(char *buf, ssize maxsize, char *fmt, va_list arg);
static void outNum(Format *fmt, char *prefix, uint64 val);
static void outString(Format *fmt, char *str, ssize len);
#if BIT_CHAR_LEN > 1 && UNUSED && KEEP
static void outWideString(Format *fmt, wchar *str, ssize len);
#endif
#if BIT_FLOAT
static void outFloat(Format *fmt, char specChar, double value);
#endif

/************************************* Code ***********************************/
/*
    This function is called when a scheduled process time has come.
 */
static void callEvent(int id)
{
    Callback    *cp;

    gassert(0 <= id && id < callbackMax);
    cp = callbacks[id];
    gassert(cp);

    (cp->routine)(cp->arg, cp->id);
}


/*
    Schedule an event in delay milliseconds time. We will use 1 second granularity for webServer.
 */
int websStartEvent(int delay, WebsEventProc proc, void *arg)
{
    Callback    *s;
    int         id;

    if ((id = gallocEntry(&callbacks, &callbackMax, sizeof(Callback))) < 0) {
        return -1;
    }
    s = callbacks[id];
    s->routine = proc;
    s->arg = arg;
    s->id = id;

    /*
        Round the delay up to seconds.
     */
    s->at = ((delay + 500) / 1000) + time(0);
    return id;
}


void websRestartEvent(int id, int delay)
{
    Callback    *s;

    if (callbacks == NULL || id == -1 || id >= callbackMax || (s = callbacks[id]) == NULL) {
        return;
    }
    s->at = ((delay + 500) / 1000) + time(0);
}


void websStopEvent(int id)
{
    Callback    *s;

    if (callbacks == NULL || id == -1 || id >= callbackMax || (s = callbacks[id]) == NULL) {
        return;
    }
    gfree(s);
    callbackMax = gfreeHandle(&callbacks, id);
}


void websRunEvents()
{
    Callback    *s;
    int         id;
    static int  next = 0;   

    /*
        If callbackMax is 0, there are no tasks scheduled, so just return.
     */
    if (callbackMax <= 0) {
        return;
    }
    /*
        If next >= callbackMax, the schedule queue was reduced in our absence
        so reset next to 0 to start from the begining of the queue again.
     */
    if (next >= callbackMax) {
        next = 0;
    }
    id = next;
    for (;;) {
        if ((s = callbacks[id]) != NULL && (int)s->at <= (int)time(0)) {
            callEvent(id);
            next = id + 1;
            return;
        }
        if (++id >= callbackMax) {
            id = 0;
        }
        if (id == next) {
            /*
                We've gone all the way through the queue without finding anything to do so just return.
             */
            return;
        }
    }
}

#if UNUSED
/*
    Returns a pointer to the directory component of a pathname. bufsize is the size of the buffer in BYTES!
 */
char *dirname(char *buf, char *name, ssize bufsize)
{
    char  *cp;
    ssize   len;

    gassert(name);
    gassert(buf);
    gassert(bufsize > 0);

#if BIT_WIN_LIKE
    if ((cp = strrchr(name, '/')) == NULL && (cp = strrchr(name, '\\')) == NULL)
#else
    if ((cp = strrchr(name, '/')) == NULL)
#endif
    {
        strcpy(buf, ".");
        return buf;
    }
    if ((*(cp + 1) == '\0' && cp == name)) {
        strncpy(buf, ".", TSZ(bufsize));
        strcpy(buf, ".");
        return buf;
    }
    len = cp - name;
    if (len < bufsize) {
        strncpy(buf, name, len);
        buf[len] = '\0';
    } else {
        strncpy(buf, name, TSZ(bufsize));
        buf[bufsize - 1] = '\0';
    }
    return buf;
}
#endif


/*
    Allocating secure replacement for sprintf and vsprintf. 
 */
char *sfmt(char *format, ...)
{
    va_list     ap;
    char        *result;

    gassert(format);

    va_start(ap, format);
    result = sprintfCore(NULL, -1, format, ap);
    va_end(ap);
    return result;
}


/*
    Replacement for sprintf
 */
char *fmt(char *buf, ssize bufsize, char *format, ...)
{
    va_list     ap;
    char        *result;

    gassert(buf);
    gassert(format);
    gassert(bufsize <= BIT_LIMIT_STRING);

    if (bufsize <= 0) {
        return 0;
    }
    va_start(ap, format);
    result = sprintfCore(buf, bufsize, format, ap);
    va_end(ap);
    return result;
}


/*
    Scure vsprintf replacement
 */
char *sfmtv(char *fmt, va_list arg)
{
    gassert(fmt);
    return sprintfCore(NULL, -1, fmt, arg);
}


static int getState(char c, int state)
{
    int     chrClass;

    if (c < ' ' || c > 'z') {
        chrClass = CLASS_NORMAL;
    } else {
        gassert((c - ' ') < (int) sizeof(classMap));
        chrClass = classMap[(c - ' ')];
    }
    gassert((chrClass * STATE_COUNT + state) < (int) sizeof(stateMap));
    state = stateMap[chrClass * STATE_COUNT + state];
    return state;
}


static char *sprintfCore(char *buf, ssize maxsize, char *spec, va_list args)
{
    Format        fmt;
    ssize         len;
    int64         iValue;
    uint64        uValue;
    int           state;
    char          c, *safe;

    if (spec == 0) {
        spec = "";
    }
    if (buf != 0) {
        gassert(maxsize > 0);
        fmt.buf = (uchar*) buf;
        fmt.endbuf = &fmt.buf[maxsize];
        fmt.growBy = -1;
    } else {
        if (maxsize <= 0) {
            maxsize = MAXINT;
        }
        len = min(256, maxsize);
        if ((buf = galloc(len)) == 0) {
            return 0;
        }
        fmt.buf = (uchar*) buf;
        fmt.endbuf = &fmt.buf[len];
        fmt.growBy = min(512, maxsize - len);
    }
    fmt.maxsize = maxsize;
    fmt.start = fmt.buf;
    fmt.end = fmt.buf;
    fmt.len = 0;
    *fmt.start = '\0';

    state = STATE_NORMAL;

    while ((c = *spec++) != '\0') {
        state = getState(c, state);

        switch (state) {
        case STATE_NORMAL:
            BPUT(&fmt, c);
            break;

        case STATE_PERCENT:
            fmt.precision = -1;
            fmt.width = 0;
            fmt.flags = 0;
            break;

        case STATE_MODIFIER:
            switch (c) {
            case '+':
                fmt.flags |= SPRINTF_SIGN;
                break;
            case '-':
                fmt.flags |= SPRINTF_LEFT;
                break;
            case '#':
                fmt.flags |= SPRINTF_ALTERNATE;
                break;
            case '0':
                fmt.flags |= SPRINTF_LEAD_ZERO;
                break;
            case ' ':
                fmt.flags |= SPRINTF_LEAD_SPACE;
                break;
            case ',':
                fmt.flags |= SPRINTF_COMMA;
                break;
            }
            break;

        case STATE_WIDTH:
            if (c == '*') {
                fmt.width = va_arg(args, int);
                if (fmt.width < 0) {
                    fmt.width = -fmt.width;
                    fmt.flags |= SPRINTF_LEFT;
                }
            } else {
                while (isdigit((uchar) c)) {
                    fmt.width = fmt.width * 10 + (c - '0');
                    c = *spec++;
                }
                spec--;
            }
            break;

        case STATE_DOT:
            fmt.precision = 0;
            break;

        case STATE_PRECISION:
            if (c == '*') {
                fmt.precision = va_arg(args, int);
            } else {
                while (isdigit((uchar) c)) {
                    fmt.precision = fmt.precision * 10 + (c - '0');
                    c = *spec++;
                }
                spec--;
            }
            break;

        case STATE_BITS:
            switch (c) {
            case 'L':
                fmt.flags |= SPRINTF_INT64;
                break;

            case 'l':
                fmt.flags |= SPRINTF_LONG;
                break;

            case 'h':
                fmt.flags |= SPRINTF_SHORT;
                break;
            }
            break;

        case STATE_TYPE:
            switch (c) {
            case 'e':
#if BIT_FLOAT
            case 'g':
            case 'f':
                fmt.radix = 10;
                outFloat(&fmt, c, (double) va_arg(args, double));
                break;
#endif /* BIT_FLOAT */

            case 'c':
                BPUT(&fmt, (char) va_arg(args, int));
                break;

            case 'S':
                /* Safe string */
#if BIT_CHAR_LEN > 1 && UNUSED && KEEP
                if (fmt.flags & SPRINTF_LONG) {
                    //  UNICODE - not right wchar
                    safe = websEscapeHtml(va_arg(args, wchar*));
                    outWideString(&fmt, safe, -1);
                } else
#endif
                {
                    safe = websEscapeHtml(va_arg(args, char*));
                    outString(&fmt, safe, -1);
                }
                break;

            case 'w':
                /* Wide string of wchar characters (Same as %ls"). Null terminated. */
#if BIT_CHAR_LEN > 1 && UNUSED && KEEP
                outWideString(&fmt, va_arg(args, wchar*), -1);
                break;
#else
                /* Fall through */
#endif

            case 's':
                /* Standard string */
#if BIT_CHAR_LEN > 1 && UNUSED && KEEP
                if (fmt.flags & SPRINTF_LONG) {
                    outWideString(&fmt, va_arg(args, wchar*), -1);
                } else
#endif
                    outString(&fmt, va_arg(args, char*), -1);
                break;

            case 'i':
                ;

            case 'd':
                fmt.radix = 10;
                if (fmt.flags & SPRINTF_SHORT) {
                    iValue = (short) va_arg(args, int);
                } else if (fmt.flags & SPRINTF_LONG) {
                    iValue = (long) va_arg(args, long);
                } else if (fmt.flags & SPRINTF_INT64) {
                    iValue = (int64) va_arg(args, int64);
                } else {
                    iValue = (int) va_arg(args, int);
                }
                if (iValue >= 0) {
                    if (fmt.flags & SPRINTF_LEAD_SPACE) {
                        outNum(&fmt, " ", iValue);
                    } else if (fmt.flags & SPRINTF_SIGN) {
                        outNum(&fmt, "+", iValue);
                    } else {
                        outNum(&fmt, 0, iValue);
                    }
                } else {
                    outNum(&fmt, "-", -iValue);
                }
                break;

            case 'X':
                fmt.flags |= SPRINTF_UPPER_CASE;
#if BIT_64
                fmt.flags &= ~(SPRINTF_SHORT|SPRINTF_LONG);
                fmt.flags |= SPRINTF_INT64;
#else
                fmt.flags &= ~(SPRINTF_INT64);
#endif
                /*  Fall through  */
            case 'o':
            case 'x':
            case 'u':
                if (fmt.flags & SPRINTF_SHORT) {
                    uValue = (ushort) va_arg(args, uint);
                } else if (fmt.flags & SPRINTF_LONG) {
                    uValue = (ulong) va_arg(args, ulong);
                } else if (fmt.flags & SPRINTF_INT64) {
                    uValue = (uint64) va_arg(args, uint64);
                } else {
                    uValue = va_arg(args, uint);
                }
                if (c == 'u') {
                    fmt.radix = 10;
                    outNum(&fmt, 0, uValue);
                } else if (c == 'o') {
                    fmt.radix = 8;
                    if (fmt.flags & SPRINTF_ALTERNATE && uValue != 0) {
                        outNum(&fmt, "0", uValue);
                    } else {
                        outNum(&fmt, 0, uValue);
                    }
                } else {
                    fmt.radix = 16;
                    if (fmt.flags & SPRINTF_ALTERNATE && uValue != 0) {
                        if (c == 'X') {
                            outNum(&fmt, "0X", uValue);
                        } else {
                            outNum(&fmt, "0x", uValue);
                        }
                    } else {
                        outNum(&fmt, 0, uValue);
                    }
                }
                break;

            case 'n':       /* Count of chars seen thus far */
                if (fmt.flags & SPRINTF_SHORT) {
                    short *count = va_arg(args, short*);
                    *count = (int) (fmt.end - fmt.start);
                } else if (fmt.flags & SPRINTF_LONG) {
                    long *count = va_arg(args, long*);
                    *count = (int) (fmt.end - fmt.start);
                } else {
                    int *count = va_arg(args, int *);
                    *count = (int) (fmt.end - fmt.start);
                }
                break;

            case 'p':       /* Pointer */
#if BIT_64
                uValue = (uint64) va_arg(args, void*);
#else
                uValue = (uint) PTOI(va_arg(args, void*));
#endif
                fmt.radix = 16;
                outNum(&fmt, "0x", uValue);
                break;

            default:
                BPUT(&fmt, c);
            }
        }
    }
    BPUTNULL(&fmt);
    return (char*) fmt.buf;
}


static void outString(Format *fmt, char *str, ssize len)
{
    char    *cp;
    ssize   i;

    if (str == NULL) {
        str = "null";
        len = 4;
    } else if (fmt->flags & SPRINTF_ALTERNATE) {
        str++;
        len = (ssize) *str;
    } else if (fmt->precision >= 0) {
        for (cp = str, len = 0; len < fmt->precision; len++) {
            if (*cp++ == '\0') {
                break;
            }
        }
    } else if (len < 0) {
        len = slen(str);
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
    for (i = 0; i < len && *str; i++) {
        BPUT(fmt, *str++);
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
}


#if BIT_CHAR_LEN > 1 && UNUSED && KEEP
static void outWideString(Format *fmt, wchar *str, ssize len)
{
    wchar     *cp;
    int         i;

    if (str == 0) {
        BPUT(fmt, (char) 'n');
        BPUT(fmt, (char) 'u');
        BPUT(fmt, (char) 'l');
        BPUT(fmt, (char) 'l');
        return;
    } else if (fmt->flags & SPRINTF_ALTERNATE) {
        str++;
        len = (ssize) *str;
    } else if (fmt->precision >= 0) {
        for (cp = str, len = 0; len < fmt->precision; len++) {
            if (*cp++ == 0) {
                break;
            }
        }
    } else if (len < 0) {
        for (cp = str, len = 0; *cp++ == 0; len++) ;
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
    for (i = 0; i < len && *str; i++) {
        BPUT(fmt, *str++);
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
}
#endif


static void outNum(Format *fmt, char *prefix, uint64 value)
{
    char    numBuf[64];
    char    *cp;
    char    *endp;
    char    c;
    int     letter, len, leadingZeros, i, fill;

    endp = &numBuf[sizeof(numBuf) - 1];
    *endp = '\0';
    cp = endp;

    /*
     *  Convert to ascii
     */
    if (fmt->radix == 16) {
        do {
            letter = (int) (value % fmt->radix);
            if (letter > 9) {
                if (fmt->flags & SPRINTF_UPPER_CASE) {
                    letter = 'A' + letter - 10;
                } else {
                    letter = 'a' + letter - 10;
                }
            } else {
                letter += '0';
            }
            *--cp = letter;
            value /= fmt->radix;
        } while (value > 0);

    } else if (fmt->flags & SPRINTF_COMMA) {
        i = 1;
        do {
            *--cp = '0' + (int) (value % fmt->radix);
            value /= fmt->radix;
            if ((i++ % 3) == 0 && value > 0) {
                *--cp = ',';
            }
        } while (value > 0);
    } else {
        do {
            *--cp = '0' + (int) (value % fmt->radix);
            value /= fmt->radix;
        } while (value > 0);
    }

    len = (int) (endp - cp);
    fill = fmt->width - len;

    if (prefix != 0) {
        fill -= (int) slen(prefix);
    }
    leadingZeros = (fmt->precision > len) ? fmt->precision - len : 0;
    fill -= leadingZeros;

    if (!(fmt->flags & SPRINTF_LEFT)) {
        c = (fmt->flags & SPRINTF_LEAD_ZERO) ? '0': ' ';
        for (i = 0; i < fill; i++) {
            BPUT(fmt, c);
        }
    }
    if (prefix != 0) {
        while (*prefix) {
            BPUT(fmt, *prefix++);
        }
    }
    for (i = 0; i < leadingZeros; i++) {
        BPUT(fmt, '0');
    }
    while (*cp) {
        BPUT(fmt, *cp);
        cp++;
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = 0; i < fill; i++) {
            BPUT(fmt, ' ');
        }
    }
}


#if BIT_FLOAT
static void outFloat(Format *fmt, char specChar, double value)
{
    char    result[BIT_LIMIT_STRING], *cp;
    int     c, fill, i, len, index;

    result[0] = '\0';
    if (specChar == 'f') {
        sprintf(result, "%.*f", fmt->precision, value);

    } else if (specChar == 'g') {
        sprintf(result, "%*.*g", fmt->width, fmt->precision, value);

    } else if (specChar == 'e') {
        sprintf(result, "%*.*e", fmt->width, fmt->precision, value);
    }
    len = (int) slen(result);
    fill = fmt->width - len;
    if (fmt->flags & SPRINTF_COMMA) {
        if (((len - 1) / 3) > 0) {
            fill -= (len - 1) / 3;
        }
    }

    if (fmt->flags & SPRINTF_SIGN && value > 0) {
        BPUT(fmt, '+');
        fill--;
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        c = (fmt->flags & SPRINTF_LEAD_ZERO) ? '0': ' ';
        for (i = 0; i < fill; i++) {
            BPUT(fmt, c);
        }
    }
    index = len;
    for (cp = result; *cp; cp++) {
        BPUT(fmt, *cp);
        if (fmt->flags & SPRINTF_COMMA) {
            if ((--index % 3) == 0 && index > 0) {
                BPUT(fmt, ',');
            }
        }
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = 0; i < fill; i++) {
            BPUT(fmt, ' ');
        }
    }
    BPUTNULL(fmt);
}
#endif /* BIT_FLOAT */


/*
    Grow the buffer to fit new data. Return 1 if the buffer can grow. 
    Grow using the growBy size specified when creating the buffer. 
 */
static int growBuf(Format *fmt)
{
    uchar   *newbuf;
    ssize   buflen;

    buflen = (int) (fmt->endbuf - fmt->buf);
    if (fmt->maxsize >= 0 && buflen >= fmt->maxsize) {
        return 0;
    }
    if (fmt->growBy <= 0) {
        /*
            User supplied buffer
         */
        return 0;
    }
    if ((newbuf = galloc(buflen + fmt->growBy)) == 0) {
        return -1;
    }
    if (fmt->buf) {
        memcpy(newbuf, fmt->buf, buflen);
    }
    buflen += fmt->growBy;
    fmt->end = newbuf + (fmt->end - fmt->buf);
    fmt->start = newbuf + (fmt->start - fmt->buf);
    fmt->buf = newbuf;
    fmt->endbuf = &fmt->buf[buflen];

    /*
        Increase growBy to reduce overhead
     */
    if ((buflen + (fmt->growBy * 2)) < fmt->maxsize) {
        fmt->growBy *= 2;
    }
    return 1;
}


/*
    For easy debug trace
 */
int print(cchar *fmt, ...)
{
    va_list     ap;
    int         len;

    va_start(ap, fmt);
    len = vprintf(fmt, ap);
    va_end(ap);
    return len;
}


WebsValue valueInteger(long value)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = integer;
    v.value.integer = value;
    return v;
}


WebsValue valueSymbol(void *value)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = symbol;
    v.value.symbol = value;
    return v;
}


WebsValue valueString(char *value, int flags)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = string;
    if (flags & VALUE_ALLOCATE) {
        v.allocated = 1;
        v.value.string = sclone(value);
    } else {
        v.allocated = 0;
        v.value.string = value;
    }
    return v;
}


void valueFree(WebsValue* v)
{
    if (v->valid && v->allocated && v->type == string && v->value.string != NULL) {
        gfree(v->value.string);
    }
    v->type = undefined;
    v->valid = 0;
    v->allocated = 0;
}


static void defaultTraceHandler(int level, char *buf)
{
    char    prefix[BIT_LIMIT_STRING];

    if (traceFd >= 0) {
        if (!(level & WEBS_LOG_RAW)) {
            fmt(prefix, sizeof(prefix), "%s: %d: ", BIT_PRODUCT, level & WEBS_LOG_MASK);
            write(traceFd, prefix, (int) slen(prefix));
        }
        write(traceFd, buf, (int) slen(buf));
        if (level & WEBS_LOG_NEWLINE) {
            write(traceFd, "\n", 1);
        }
    }
}


void error(char *fmt, ...)
{
    va_list     args;
    char      *message;

    if (!traceHandler) {
        return;
    }
    va_start(args, fmt);
    message = sfmtv(fmt, args);
    va_end(args);
    traceHandler(WEBS_LOG_NEWLINE, message);
#if BIT_WIN_LIKE || BIT_UNIX_LIKE
    if (websGetBackground()) {
        syslog(LOG_ERR, "%s", message);
    }
#endif
    gfree(message);
}


void gassertError(WEBS_ARGS_DEC, char *fmt, ...)
{
    va_list     args;
    char        *fmtBuf, *message;

    va_start(args, fmt);
    fmtBuf = sfmtv(fmt, args);

    message = sfmt("Assertion %s, failed at %s %d\n", fmtBuf, WEBS_ARGS); 
    va_end(args);
    gfree(fmtBuf);
    if (traceHandler) {
        traceHandler(-1, message);
    }
    gfree(message);
}


/*
    Trace log. Customize this function to log trace output
 */
void trace(int level, char *fmt, ...)
{
    va_list     args;
    char      *message;

    if ((level & WEBS_LOG_MASK) <= traceLevel) {    
        va_start(args, fmt);
        message = sfmtv(fmt, args);
        if (traceHandler) {
            traceHandler(level, message);
        }
        gfree(message);
        va_end(args);
    }
}


#if UNUSED
/*
    Trace log. Customize this function to log trace output
 */
void traceRaw(char *buf)
{
    if (traceHandler) {
        traceHandler(WEBS_LOG_RAW, buf);
    }
}
#endif


/*
    Replace the default trace handler. Return a pointer to the old handler.
 */
WebsTraceHandler traceSetHandler(WebsTraceHandler handler)
{
    WebsTraceHandler    oldHandler;

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
    if (smatch(tracePath, "stdout")) {
        traceFd = 1;
    } else if (smatch(tracePath, "stderr")) {
        traceFd = 2;
    } else if ((traceFd = open(tracePath, O_CREAT | O_TRUNC | O_APPEND | O_WRONLY, 0666)) < 0) {
        return -1;
    }
    lseek(traceFd, 0, SEEK_END);
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


void traceSetPath(char *path)
{
    char  *lp;
    
    gfree(tracePath);
    tracePath = strdup(path);
    if ((lp = strchr(tracePath, ':')) != 0) {
        *lp++ = '\0';
        traceLevel = atoi(lp);
    }
}


/*
    Convert a string to lower case
 */
char *slower(char *string)
{
    char  *s;

    gassert(string);

    if (string == NULL) {
        return NULL;
    }
    s = string;
    while (*s) {
        if (isupper(*s)) {
            *s = (char) tolower((uchar) *s);
        }
        s++;
    }
    *s = '\0';
    return string;
}


/* 
    Convert a string to upper case
 */
char *supper(char *string)
{
    char  *s;

    gassert(string);
    if (string == NULL) {
        return NULL;
    }
    s = string;
    while (*s) {
        if (islower((uchar) *s)) {
            *s = (char) toupper((uchar) *s);
        }
        s++;
    }
    *s = '\0';
    return string;
}


char *itosbuf(char *buf, ssize size, int64 value, int radix)
{
    char    *cp, *end;
    char    digits[] = "0123456789ABCDEF";
    int     negative;

    if ((radix != 10 && radix != 16) || size < 2) {
        return 0;
    }
    end = cp = &buf[size];
    *--cp = '\0';

    if (value < 0) {
        negative = 1;
        value = -value;
        size--;
    } else {
        negative = 0;
    }
    do {
        *--cp = digits[value % radix];
        value /= radix;
    } while (value > 0 && cp > buf);

    if (negative) {
        if (cp <= buf) {
            return 0;
        }
        *--cp = '-';
    }
    if (buf < cp) {
        /* Move the null too */
        memmove(buf, cp, end - cp + 1);
    }
    return buf;
}


/*
    Allocate a new file handle.  On the first call, the caller must set the handle map to be a pointer to a null
    pointer.  map points to the second element in the handle array.
 */
int gallocHandle(void *mapArg)
{
    void    ***map;
    ssize   *mp;
    int     handle, len, memsize, incr;

    map = (void***) mapArg;
    gassert(map);

    if (*map == NULL) {
        incr = H_INCR;
        memsize = (incr + H_OFFSET) * sizeof(void*);
        if ((mp = galloc(memsize)) == NULL) {
            return -1;
        }
        memset(mp, 0, memsize);
        mp[H_LEN] = incr;
        mp[H_USED] = 0;
        *map = (void*) &mp[H_OFFSET];
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
    memsize = (len + H_OFFSET) * sizeof(void*);
    if ((mp = grealloc(mp, memsize)) == NULL) {
        return -1;
    }
    *map = (void*) &mp[H_OFFSET];
    mp[H_LEN] = len;
    memset(&mp[H_OFFSET + len - H_INCR], 0, sizeof(ssize*) * H_INCR);
    mp[H_USED]++;
    return handle;
}


/*
    Free a handle. This function returns the value of the largest handle in use plus 1, to be saved as a max value.
 */
int gfreeHandle(void *mapArg, int handle)
{
    void    ***map;
    ssize   *mp;
    int     len;

    map = (void***) mapArg;
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
int gallocEntry(void *listArg, int *max, int size)
{
    void    ***list;
    char    *cp;
    int     id;

    list = (void***) listArg;
    gassert(list);
    gassert(max);

    if ((id = gallocHandle(list)) < 0) {
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
int ringqOpen(WebsBuf *rq, int initSize, int maxsize)
{
    int increment;

    gassert(rq);
    
    if (initSize <= 0) {
        initSize = BIT_LIMIT_BUFFER;
    }
    if (maxsize <= 0) {
        maxsize = initSize;
    }
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
void ringqClose(WebsBuf *rq)
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
ssize ringqLen(WebsBuf *rq)
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
int ringqGetc(WebsBuf *rq)
{
    char    c, *cp;

    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if (rq->servp == rq->endp) {
        return -1;
    }

    cp = (char*) rq->servp;
    c = *cp++;
    rq->servp = cp;
    if (rq->servp >= rq->endbuf) {
        rq->servp = rq->buf;
    }
    return (int) ((uchar) c);
}


/*
    Add a char to the queue. Note if being used to store wide strings this does not add a trailing '\0'. Grow the q as
    required.  
 */
int ringqPutc(WebsBuf *rq, char c)
{
    char *cp;

    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if ((ringqPutBlkMax(rq) < (int) sizeof(char)) && !ringqGrow(rq, 0)) {
        return -1;
    }
    cp = (char*) rq->endp;
    *cp++ = (char) c;
    rq->endp = cp;
    if (rq->endp >= rq->endbuf) {
        rq->endp = rq->buf;
    }
    return 0;
}


/*
    Insert a wide character at the front of the queue
 */
int ringqInsertc(WebsBuf *rq, char c)
{
    char *cp;

    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if (ringqPutBlkMax(rq) < (int) sizeof(char) && !ringqGrow(rq, 0)) {
        return -1;
    }
    if (rq->servp <= rq->buf) {
        rq->servp = rq->endbuf;
    }
    cp = (char*) rq->servp;
    *--cp = (char) c;
    rq->servp = cp;
    return 0;
}


/*
    Add a string to the queue. Add a trailing null (maybe two nulls)
 */
ssize ringqPutStr(WebsBuf *rq, char *str)
{
    ssize   rc;

    gassert(rq);
    gassert(str);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    rc = ringqPutBlk(rq, str, strlen(str) * sizeof(char));
    *((char*) rq->endp) = (char) '\0';
    return rc;
}


/*
    Add a null terminator. This does NOT increase the size of the queue
 */
void ringqAddNull(WebsBuf *rq)
{
    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    if (ringqPutBlkMax(rq) < (int) sizeof(char)) {
        ringqGrow(rq, 0);
        return;
    }
    *((char*) rq->endp) = (char) '\0';
}


#if UNICODE
/*
    Get a byte from the queue
 */
int ringqGetcA(WebsBuf *rq)
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
int ringqPutcA(WebsBuf *rq, char c)
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
int ringqInsertcA(WebsBuf *rq, char c)
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
int ringqPutStrA(WebsBuf *rq, char *str)
{
    int     rc;

    gassert(rq);
    gassert(str);
    gassert(rq->buflen == (rq->endbuf - rq->buf));

    rc = (int) ringqPutBlk(rq, str, strlen(str));
    rq->endp[0] = '\0';
    return rc;
}
#endif


/*
    Add a block of data to the ringq. Return the number of bytes added. Grow the q as required.
 */
ssize ringqPutBlk(WebsBuf *rq, char *buf, ssize size)
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
            if (! ringqGrow(rq, 0)) {
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
ssize ringqGetBlk(WebsBuf *rq, char *buf, ssize size)
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
    MOB - rename Room
 */
ssize ringqPutBlkMax(WebsBuf *rq)
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
ssize ringqGetBlkMax(WebsBuf *rq)
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
void ringqPutBlkAdj(WebsBuf *rq, ssize size)
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
        error("Bad end pointer");
        ringqFlush(rq);
    }
}


/*
    Adjust the servp pointer after the user has copied data from the queue.
 */
void ringqGetBlkAdj(WebsBuf *rq, ssize size)
{
    gassert(rq);
    gassert(rq->buflen == (rq->endbuf - rq->buf));
    gassert(0 <= size && size < rq->buflen);

    rq->servp += size;
    if (rq->servp >= rq->endbuf) {
        rq->servp -= rq->buflen;
    }
    /*
        Flush the queue if the servp pointer is corrupted via a bad size
     */
    if (rq->servp >= rq->endbuf) {
        error("Bad serv pointer");
        ringqFlush(rq);
    }
}


/*
    Flush all data in a ring q. Reset the pointers.
 */
void ringqFlush(WebsBuf *rq)
{
    gassert(rq);
    gassert(rq->servp);

    if (rq->buf) {
        rq->servp = rq->buf;
        rq->endp = rq->buf;
        if (rq->servp) {
            *rq->servp = '\0';
        }
    }
}


void ringqCompact(WebsBuf *rq)
{
    ssize   len;
    
    if (rq->buf) {
        if (rq->servp < rq->endp && rq->servp > rq->buf) {
            ringqAddNull(rq);
            len = ringqLen(rq) + 1;
            memmove(rq->buf, rq->servp, len);
            rq->endp -= rq->servp - rq->buf;
            rq->servp = rq->buf;
        } else {
            rq->servp = rq->endp = rq->buf;
            *rq->servp = '\0';
        }
    }
}


/*
    Reset pointers if empty
 */
void ringqReset(WebsBuf *rq)
{
    if (rq->buf && rq->servp == rq->endp && rq->servp > rq->buf) {
        rq->servp = rq->endp = rq->buf;
        *rq->servp = '\0';
    }
}


/*
    Grow the buffer. Return true if the buffer can be grown. Grow using the increment size specified when opening the
    ringq. Don't grow beyond the maximum possible size.
 */
bool ringqGrow(WebsBuf *rq, ssize room)
{
    char    *newbuf;
    ssize   len;

    gassert(rq);

    if (room == 0) {
        if (rq->maxsize >= 0 && rq->buflen >= rq->maxsize) {
#if BLD_DEBUG
            error("Can't grow ringq. Needed %d, max %d", room, rq->maxsize);
#endif
            return 0;
        }
        room = rq->increment;
        /*
            Double the increment so the next grow will line up with galloc'ed memory
         */
        rq->increment = getBinBlockSize(2 * rq->increment);
    }
    len = ringqLen(rq);
    if ((newbuf = galloc(rq->buflen + room)) == NULL) {
        return 0;
    }
    ringqGetBlk(rq, newbuf, ringqLen(rq));
    gfree((char*) rq->buf);

    rq->buflen += room;
    rq->endp = newbuf;
    rq->servp = newbuf;
    rq->buf = newbuf;
    rq->endbuf = &rq->buf[rq->buflen];
    ringqPutBlk(rq, newbuf, len);
    return 1;
}


/*
    Find the smallest binary memory size that "size" will fit into.  This makes the ringq and ringqGrow routines much
    more efficient.  The galloc routine likes powers of 2 minus 1.
 */
static int  getBinBlockSize(int size)
{
    int q;

    size = size >> WEBS_SHIFT;
    for (q = 0; size; size >>= 1) {
        q++;
    }
    return (1 << (WEBS_SHIFT + q));
}


WebsHash symOpen(int size)
{
    WebsHash    sd;
    SymTab      *tp;

    if (size < 0) {
        size = WEBS_SMALL_HASH;
    }
    gassert(size > 2);

    /*
        Create a new handle for this symbol table
     */
    if ((sd = gallocHandle(&sym)) < 0) {
        return -1;
    }

    /*
        Create a new symbol table structure and zero
     */
    if ((tp = (SymTab*) galloc(sizeof(SymTab))) == NULL) {
        symMax = gfreeHandle(&sym, sd);
        return -1;
    }
    memset(tp, 0, sizeof(SymTab));
    if (sd >= symMax) {
        symMax = sd + 1;
    }
    gassert(0 <= sd && sd < symMax);
    sym[sd] = tp;

    /*
        Now create the hash table for fast indexing.
     */
    tp->size = calcPrime(size);
    tp->hash_table = (WebsKey**) galloc(tp->size * sizeof(WebsKey*));
    gassert(tp->hash_table);
    memset(tp->hash_table, 0, tp->size * sizeof(WebsKey*));
    return sd;
}


/*
    Close this symbol table. Call a cleanup function to allow the caller to free resources associated with each symbol
    table entry.  
 */
void symClose(WebsHash sd)
{
    SymTab      *tp;
    WebsKey     *sp, *forw;
    int         i;

    if (sd < 0) {
        return;
    }
    gassert(0 <= sd && sd < symMax);
    tp = sym[sd];
    gassert(tp);

    /*
        Free all symbols in the hash table, then the hash table itself.
     */
    for (i = 0; i < tp->size; i++) {
        for (sp = tp->hash_table[i]; sp; sp = forw) {
            forw = sp->forw;
            valueFree(&sp->name);
            valueFree(&sp->content);
            gfree((void*) sp);
            sp = forw;
        }
    }
    gfree((void*) tp->hash_table);
    symMax = gfreeHandle(&sym, sd);
    gfree((void*) tp);
}


/*
    Return the first symbol in the hashtable if there is one. This call is used as the first step in traversing the
    table. A call to symFirst should be followed by calls to symNext to get all the rest of the entries.
 */
WebsKey *symFirst(WebsHash sd)
{
    SymTab      *tp;
    WebsKey     *sp;
    int         i;

    gassert(0 <= sd && sd < symMax);
    tp = sym[sd];
    gassert(tp);

    /*
        Find the first symbol in the hashtable and return a pointer to it.
     */
    for (i = 0; i < tp->size; i++) {
        if ((sp = tp->hash_table[i]) != 0) {
            return sp;
        }
    }
    return 0;
}


/*
    Return the next symbol in the hashtable if there is one. See symFirst.
 */
WebsKey *symNext(WebsHash sd, WebsKey *last)
{
    SymTab      *tp;
    WebsKey     *sp;
    int         i;

    gassert(0 <= sd && sd < symMax);
    if (sd < 0) {
        return 0;
    }
    tp = sym[sd];
    gassert(tp);
    if (last == 0) {
        return symFirst(sd);
    }
    if (last->forw) {
        return last->forw;
    }
    for (i = last->bucket + 1; i < tp->size; i++) {
        if ((sp = tp->hash_table[i]) != 0) {
            return sp;
        }
    }
    return NULL;
}


/*
    Lookup a symbol and return a pointer to the symbol entry. If not present then return a NULL.
 */
WebsKey *symLookup(WebsHash sd, char *name)
{
    SymTab      *tp;
    WebsKey     *sp;
    char        *cp;

    gassert(0 <= sd && sd < symMax);
    if (sd < 0 || (tp = sym[sd]) == NULL) {
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
        if (cp[0] == name[0] && strcmp(cp, name) == 0) {
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
WebsKey *symEnter(WebsHash sd, char *name, WebsValue v, int arg)
{
    SymTab      *tp;
    WebsKey     *sp, *last;
    char        *cp;
    int         hindex;

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
            if (cp[0] == name[0] && strcmp(cp, name) == 0) {
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
        sp = (WebsKey*) galloc(sizeof(WebsKey));
        if (sp == NULL) {
            return NULL;
        }
        sp->name = valueString(name, VALUE_ALLOCATE);
        sp->content = v;
        sp->forw = (WebsKey*) NULL;
        sp->arg = arg;
        sp->bucket = hindex;
        last->forw = sp;

    } else {
        /*
            Daisy chain is empty so we need to start the chain
         */
        sp = (WebsKey*) galloc(sizeof(WebsKey));
        if (sp == NULL) {
            return NULL;
        }
        tp->hash_table[hindex] = sp;
        tp->hash_table[hashIndex(tp, name)] = sp;

        sp->forw = (WebsKey*) NULL;
        sp->content = v;
        sp->arg = arg;
        sp->name = valueString(name, VALUE_ALLOCATE);
        sp->bucket = hindex;
    }
    return sp;
}


/*
    Delete a symbol from a table
 */
int symDelete(WebsHash sd, char *name)
{
    SymTab      *tp;
    WebsKey     *sp, *last;
    char        *cp;
    int         hindex;

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
            if (cp[0] == name[0] && strcmp(cp, name) == 0) {
                break;
            }
            last = sp;
        }
    }
    if (sp == (WebsKey*) NULL) {              /* Not Found */
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
static WebsKey *hash(SymTab *tp, char *name)
{
    gassert(tp);

    return tp->hash_table[hashIndex(tp, name)];
}


/*
    Compute the hash function and return an index into the hash table We use a basic additive function that is then made
    modulo the size of the table.
 */
static int hashIndex(SymTab *tp, char *name)
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
    return sum % tp->size;
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


#if UNUSED
int gopen(char *path, int oflags, int mode)
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


/*
    Convert an ansi string to a unicode string. On an error, we return the original ansi string which is better than
    returning NULL. nBytes is the size of the destination buffer (ubuf) in _bytes_.
 */
char *guni(char *ubuf, char *str, ssize nBytes)
{
#if UNICODE
    if (MultiByteToWideChar(CP_ACP, 0, str, nBytes / sizeof(char), ubuf, nBytes / sizeof(char)) < 0) {
        return (char*) str;
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
char *gasc(char *buf, char *ustr, ssize nBytes)
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
char *gallocAscToUni(char *cp, ssize alen)
{
    char  *unip;
    ssize   ulen;

    ulen = (alen + 1) * sizeof(char);
    if ((unip = galloc(ulen)) == NULL) {
        return NULL;
    }
    guni(unip, cp, ulen);
    unip[alen] = 0;
    return unip;
}


/*
    Allocate (galloc) a buffer and do unicode to ascii conversion into it.  unip points to the unicoded string. ulen is
    the number of characters in the unicode string not including a teminating null.  Return a pointer to the ascii
    buffer which must be gfree'd later.  Return NULL on failure to get buffer.  The buffer returned is NULL terminated.
 */
char *gallocUniToAsc(char *unip, ssize ulen)
{
    char    *cp;

    if ((cp = galloc(ulen+1)) == NULL) {
        return NULL;
    }
    gasc(cp, unip, ulen);
    cp[ulen] = '\0';
    return cp;
}
#else

/*
    Convert a wide unicode string into a multibyte string buffer. If count is supplied, it is used as the source length 
    in characters. Otherwise set to -1. DestCount is the max size of the dest buffer in bytes. At most destCount - 1 
    characters will be stored. The dest buffer will always have a trailing null appended.  If dest is NULL, don't copy 
    the string, just return the length of characters. Return a count of bytes copied to the destination or -1 if an 
    invalid unicode sequence was provided in src.
    NOTE: does not allocate.
 */
ssize wtom(char *dest, ssize destCount, wchar *src, ssize count)
{
    ssize   len;

    if (destCount < 0) {
        destCount = MAXSSIZE;
    }
    if (count > 0) {
#if BIT_CHAR_LEN == 1
        if (dest) {
            len = scopy(dest, destCount, src);
        } else {
            len = min(slen(src), destCount - 1);
        }
#elif BIT_WIN_LIKE
        len = WideCharToMultiByte(CP_ACP, 0, src, count, dest, (DWORD) destCount - 1, NULL, NULL);
#else
        //  MOB - does this support dest == NULL?
        //  MOB - count is ignored
        len = wcstombs(dest, src, destCount - 1);
#endif
        if (dest) {
            if (len >= 0) {
                dest[len] = 0;
            }
        } else if (len >= destCount) {
            return -1;
        }
    } else {
        len = 0;
    }
    return len;
}


/*
    Convert a multibyte string to a unicode string. If count is supplied, it is used as the source length in bytes.
    Otherwise set to -1. DestCount is the max size of the dest buffer in characters. At most destCount - 1 
    characters will be stored. The dest buffer will always have a trailing null characters appended.  If dest is NULL, 
    don't copy the string, just return the length of characters. Return a count of characters copied to the destination 
    or -1 if an invalid multibyte sequence was provided in src.
    NOTE: does not allocate.
 */
ssize mtow(wchar *dest, ssize destCount, char *src, ssize count) 
{
    ssize      len;

    if (destCount < 0) {
        destCount = MAXSSIZE;
    }
    if (destCount > 0) {
#if BIT_CHAR_LEN == 1
        if (dest) {
            len = scopy(dest, destCount, src);
        } else {
            len = min(slen(src), destCount - 1);
        }
#elif BIT_WIN_LIKE
        len = MultiByteToWideChar(CP_ACP, 0, src, count, dest, (DWORD) destCount - 1);
#else
        //  MOB - does this support dest == NULL
        //  MOB - count is ignored
        len = mbstowcs(dest, src, destCount - 1);
#endif
        if (dest) {
            if (len >= 0) {
                dest[len] = 0;
            }
        } else if (len >= destCount) {
            return -1;
        }
    } else {
        len = 0;
    }
    return len;
}


wchar *amtow(char *src, ssize *lenp)
{
    wchar   *dest;
    ssize   len;

    len = mtow(NULL, MAXSSIZE, src, -1);
    if (len < 0) {
        return NULL;
    }
    if ((dest = galloc((len + 1) * sizeof(wchar))) != NULL) {
        mtow(dest, len + 1, src, -1);
    }
    if (lenp) {
        *lenp = len;
    }
    return dest;
}


//  FUTURE UNICODE - need a version that can supply a length

char *awtom(wchar *src, ssize *lenp)
{
    char    *dest;
    ssize   len;

    len = wtom(NULL, MAXSSIZE, src, -1);
    if (len < 0) {
        return NULL;
    }
    if ((dest = galloc(len + 1)) != 0) {
        wtom(dest, len + 1, src, -1);
    }
    if (lenp) {
        *lenp = len;
    }
    return dest;
}
#endif /* UNUSED */


/*
    Convert a hex string to an integer
 */
uint ghextoi(char *hexstring)
{
    char      *h;
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


char *sclone(char *s)
{
    char    *buf;

    if (s == NULL) {
        s = "";
    }
    buf = galloc(strlen(s) + 1);
    strcpy(buf, s);
    return buf;
}


/*
    Convert a string to an integer. If the string starts with "0x" or "0X" a hexidecimal conversion is done.
 */
uint strtoi(char *s)
{
    if (*s == '0' && (*(s+1) == 'x' || *(s+1) == 'X')) {
        s += 2;
        return ghextoi(s);
    }
    return atoi(s);
}


int scaselesscmp(char *s1, char *s2)
{
    if (s1 == 0 || s2 == 0) {
        return -1;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return sncaselesscmp(s1, s2, max(slen(s1), slen(s2)));
}


bool scaselessmatch(char *s1, char *s2)
{
    return scaselesscmp(s1, s2) == 0;
}


bool smatch(char *s1, char *s2)
{
    return scmp(s1, s2) == 0;
}


int scmp(char *s1, char *s2)
{
    if (s1 == s2) {
        return 0;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return sncmp(s1, s2, max(slen(s1), slen(s2)));
}


ssize slen(char *s)
{
    return s ? strlen(s) : 0;
}



ssize scopy(char *dest, ssize destMax, char *src)
{
    ssize      len;

    gassert(src);
    gassert(dest);
    gassert(0 < dest && destMax < MAXINT);

    len = slen(src);
    if (destMax <= len) {
        return -1;
    }
    strcpy(dest, src);
    return len;
}


/*
    This routine copies at most "count" characters from a string. It ensures the result is always null terminated and 
    the buffer does not overflow. Returns -1 if the buffer is too small.
 */
ssize sncopy(char *dest, ssize destMax, char *src, ssize count)
{
    ssize      len;

    gassert(dest);
    gassert(src);
    gassert(src != dest);
    gassert(0 <= count && count < MAXINT);
    gassert(0 < destMax && destMax < MAXINT);

    //  OPT need snlen(src, count);
    len = slen(src);
    len = min(len, count);
    if (destMax <= len) {
        return -1;
    }
    if (len > 0) {
        memcpy(dest, src, len);
        dest[len] = '\0';
    } else {
        *dest = '\0';
        len = 0;
    } 
    return len;
}


#if UNUSED && KEEP
/*
    Return the length of a string limited by a given length
 */
ssize strnlen(char *s, ssize n)
{
    ssize   len;

    len = strlen(s);
    return min(len, n);
}
#endif


/*
    Case sensitive string comparison. Limited by length
 */
int sncmp(char *s1, char *s2, ssize n)
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


int sncaselesscmp(char *s1, char *s2, ssize n)
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
    Note "str" is modifed as per strtok()
    WARNING:  this does not allocate
 */
char *stok(char *str, char *delim, char **last)
{
    char  *start, *end;
    ssize   i;

    start = str ? str : *last;
    if (start == 0) {
        *last = 0;
        return 0;
    }
    i = strspn(start, delim);
    start += i;
    if (*start == '\0') {
        *last = 0;
        return 0;
    }
    end = strpbrk(start, delim);
    if (end) {
        *end++ = '\0';
        i = strspn(end, delim);
        end += i;
    }
    *last = end;
    return start;
}


char *strim(char *str, char *set, int where)
{
    char    *s;
    ssize   len, i;

    if (str == 0 || set == 0) {
        return 0;
    }
    if (where & WEBS_TRIM_START) {
        i = strspn(str, set);
    } else {
        i = 0;
    }
    s = (char*) &str[i];
    if (where & WEBS_TRIM_END) {
        len = strlen(s);
        while (len > 0 && strspn(&s[len - 1], set) > 0) {
            s[len - 1] = '\0';
            len--;
        }
    }
    return s;
}


/*
    Parse the args and return the count of args. If argv is NULL, the args are parsed read-only. If argv is set,
    then the args will be extracted, back-quotes removed and argv will be set to point to all the args.
    NOTE: this routine does not allocate.
 */
int websParseArgs(char *args, char **argv, int maxArgc)
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
static char *getAbsolutePath(char *path)
{
#if _WRS_VXWORKS_MAJOR >= 6
    const char  *tail;
#else
    char        *tail;
#endif
    char  *dev;

    /*
        Determine if path is relative or absolute.  If relative, prepend the current working directory to the name.
        Otherwise, use it.  Note the getcwd call below must not be getcwd or else we go into an infinite loop
    */
    if (iosDevFind(path, &tail) != NULL && path != tail) {
        return sclone(path);
    }
    dev = galloc(BIT_LIMIT_FILENAME);
    getcwd(dev, BIT_LIMIT_FILENAME);
    strcat(dev, "/");
    strcat(dev, path);
    return dev;
}


int vxchdir(char *dirname)
{
    char  *path;
    int     rc;

    path = getAbsolutePath(dirname);
    #undef chdir
    rc = chdir(path);
    gfree(path);
    return rc;
}


char *tempnam(char *dir, char *pfx)
{
    static int count = 0;
    if (!pfx) {
        pfx = "tmp";
    }
    return sfmt("%s-%d.tmp", pfx, count++);
}
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


#if BIT_WIN_LIKE
void websSetInst(HINSTANCE inst)
{
    appInstance = inst;
}


HINSTANCE websGetInst()
{
    return appInstance;
}


static void syslog(int priority, char *fmt, ...)
{
    va_list     args;
    HKEY        hkey;
    void        *event;
    long        errorType;
    ulong       exists;
    char        *buf, logName[BIT_LIMIT_STRING], *lines[9], *cp, *value;
    int         type;
    static int  once = 0;

    va_start(args, fmt);
    buf = sfmtv(fmt, args);
    va_end(args);

    cp = &buf[slen(buf) - 1];
    while (*cp == '\n' && cp > buf) {
        *cp-- = '\0';
    }
    type = EVENTLOG_ERROR_TYPE;
    lines[0] = buf;
    lines[1] = 0;
    lines[2] = lines[3] = lines[4] = lines[5] = 0;
    lines[6] = lines[7] = lines[8] = 0;

    if (once == 0) {
        /*  Initialize the registry */
        once = 1;
        hkey = 0;

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, logName, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, &exists) ==
                ERROR_SUCCESS) {
            value = "%SystemRoot%\\System32\\netmsg.dll";
            if (RegSetValueEx(hkey, "EventMessageFile", 0, REG_EXPAND_SZ, 
                    (uchar*) value, (int) slen(value) + 1) != ERROR_SUCCESS) {
                RegCloseKey(hkey);
                gfree(buf);
                return;
            }
            errorType = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
            if (RegSetValueEx(hkey, "TypesSupported", 0, REG_DWORD, (uchar*) &errorType, sizeof(DWORD)) != 
                    ERROR_SUCCESS) {
                RegCloseKey(hkey);
                gfree(buf);
                return;
            }
            RegCloseKey(hkey);
        }
    }
    event = RegisterEventSource(0, BIT_PRODUCT);
    if (event) {
        ReportEvent(event, EVENTLOG_ERROR_TYPE, 0, 3299, NULL, sizeof(lines) / sizeof(char*), 0, (LPCSTR*) lines, 0);
        DeregisterEventSource(event);
    }
    gfree(buf);
}
#endif

/*
    "basename" returns a pointer to the last component of a pathname LINUX, LynxOS and Mac OS X have their own basename
    function 
 */

#if !BIT_UNIX_LIKE
char *basename(char *name)
{
    char  *cp;

#if BIT_WIN_LIKE
    if (((cp = strrchr(name, '\\')) == NULL) && ((cp = strrchr(name, '/')) == NULL)) {
        return name;
#else
    if ((cp = strrchr(name, '/')) == NULL) {
        return name;
#endif
    } else if (*(cp + 1) == '\0' && cp == name) {
        return name;
    } else if (*(cp + 1) == '\0' && cp != name) {
        return "";
    } else {
        return ++cp;
    }
}
#endif

#if BIT_WIN_LIKE
void sleep(int secs)
{
    Sleep(secs / 1000);
}
#endif


#if BIT_LEGACY
int fmtValloc(char **sp, int n, char *format, va_list args)
{
    *sp = sfmtv(format, args);
    return (int) slen(*sp);
}


int fmtAlloc(char **sp, int n, char *format, ...)
{
    va_list     args;

    va_start(args, format);
    *sp = sfmtv(format, args);
    va_end(args);
    return (int) slen(*sp);
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
