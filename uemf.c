/*
    uemf.c -- GoAhead Micro Embedded Management Framework
    MOB - get rid of this term

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include	"uemf.h"

/********************************** Local Data ********************************/

int emfInst;							/* Application instance handle */

/****************************** Forward Declarations **************************/

extern void defaultErrorHandler(int etype, char_t *buf);
static void (*errorHandler)(int etype, char_t *msg) = defaultErrorHandler;

extern void	defaultTraceHandler(int level, char_t *buf);
static void (*traceHandler)(int level, char_t *buf) = defaultTraceHandler;

/************************************* Code ***********************************/
/*
  	Error message that doesn't need user attention. Customize this code
  	to direct error messages to wherever the developer wishes
 */

void error(E_ARGS_DEC, int etype, char_t *fmt, ...)
{
	va_list 	args;
	char_t		*fmtBuf, *buf;

	va_start(args, fmt);
	fmtValloc(&fmtBuf, E_MAX_ERROR, fmt, args);

	if (etype == E_LOG) {
		fmtAlloc(&buf, E_MAX_ERROR, T("%s\n"), fmtBuf);
	} else if (etype == E_ASSERT) {
		fmtAlloc(&buf, E_MAX_ERROR, T("Assertion %s, failed at %s %d\n"), fmtBuf, E_ARGS); 
	} else if (etype == E_USER) {
		fmtAlloc(&buf, E_MAX_ERROR, T("%s\n"), fmtBuf);
	} else {
      fmtAlloc(&buf, E_MAX_ERROR, T("Unknown error"));
    }
    va_end(args);
	bfree(B_L, fmtBuf);
	if (errorHandler) {
		errorHandler(etype, buf);
	}
	bfreeSafe(B_L, buf);
}


/*
  	Replace the default error handler. Return pointer to old error handler.
 */
void (*errorSetHandler(void (*function)(int etype, char_t *msg))) (int etype, char_t *msg)
{
    //  MOB - typedef for this
	void (*oldHandler)(int etype, char_t *buf);

	oldHandler = errorHandler;
	errorHandler = function;
	return oldHandler;
}


/*
  	Trace log. Customize this function to log trace output
 */
void trace(int level, char_t *fmt, ...)
{
	va_list 	args;
	char_t		*buf;

	va_start(args, fmt);
	fmtValloc(&buf, VALUE_MAX_STRING, fmt, args);

	if (traceHandler) {
		traceHandler(level, buf);
	}
	bfreeSafe(B_L, buf);
	va_end(args);
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
void (*traceSetHandler(void (*function)(int level, char_t *buf))) (int level, char *buf)
{
	void (*oldHandler)(int level, char_t *buf);

	oldHandler = traceHandler;
	if (function) {
		traceHandler = function;
	}
	return oldHandler;
}


void emfInstSet(int inst)
{
	emfInst = inst;
}


int emfInstGet()
{
	return emfInst;
}


/*
  	Convert a string to lower case
 */
char_t *strlower(char_t *string)
{
	char_t	*s;

	a_assert(string);

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
char_t *strupper(char_t *string)
{
	char_t	*s;

	a_assert(string);
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
char_t *stritoa(int n, char_t *string, int width)
{
	char_t	*cp, *lim, *s;
	char_t	buf[16];						/* Just temp to hold number */
	int		next, minus;

	a_assert(string && width > 0);

	if (string == NULL) {
		if (width == 0) {
			width = 10;
		}
		if ((string = balloc(B_L, width + 1)) == NULL) {
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

/******************************************************************************/
/*
    Default error handler.  The developer should insert code to handle error messages in the desired manner.
 */
void defaultErrorHandler(int etype, char_t *msg)
{
    //  MOB - why?
#if 0
    write(1, msg, gstrlen(msg));
#endif
}


/*
    Trace log. Customize this function to log trace output
 */
void defaultTraceHandler(int level, char_t *buf)
{
    /*
        The following code would write all trace regardless of level to stdout.
     */
    //  MOB - why
#if 0
    if (buf) {
        write(1, buf, gstrlen(buf));
    }
#endif
}


/*
    Stubs
    MOB - remove
 */
char_t *basicGetProduct()
{
	return T("uemf");
}

char_t *basicGetAddress()
{
	return T("localhost");
}

int errorOpen(char_t *pname)
{
	return 0;
}

void errorClose()
{
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
