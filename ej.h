/* 
  	ej.h -- Ejscript(TM) header
  
  	See the file "license.txt" for information on usage and redistribution

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_EJ
#define _h_EJ 1

/******************************** Description *********************************/

/* 
 *	GoAhead Ejscript(TM) header. This defines the Ejscript API and internal
 *	structures.
 */

/********************************* Includes ***********************************/

#include	"uemf.h"

/********************************** Defines ***********************************/

/******************************** Prototypes **********************************/

extern int 		ejArgs(int argc, char_t **argv, char_t *fmt, ...);
extern void		ejSetResult(int eid, char_t *s);
extern int		ejOpenEngine(sym_fd_t variables, sym_fd_t functions);
extern void		ejCloseEngine(int eid);
extern int 		ejSetGlobalFunction(int eid, char_t *name, 
					int (*fn)(int eid, void *handle, int argc, char_t **argv));
extern void		ejSetVar(int eid, char_t *var, char_t *value);
extern int		ejGetVar(int eid, char_t *var, char_t **value);
extern char_t	*ejEval(int eid, char_t *script, char_t **emsg);

#endif /* _h_EJ */

/*****************************************************************************/

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.
    Copyright (c) GoAhead Software, 2003. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis GoAhead open source license or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details.

    This software is open source; you can redistribute it and/or modify it
    under the terms of the Embedthis GoAhead Open Source License as published 
    at:

        http://embedthis.com/products/goahead/goahead-license.pdf 

    This Embedthis GoAhead Open Source license does NOT generally permit 
    incorporating this software into proprietary programs. If you are unable 
    to comply with the Embedthis Open Source license, you must acquire a 
    commercial license to use this software. Commercial licenses for this 
    software and support services are available from Embedthis Software at:

        http://embedthis.com

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
