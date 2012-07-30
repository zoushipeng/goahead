/* 
    ej.h -- Ejscript header
  
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_EJ
#define _h_EJ 1

/********************************* Includes ***********************************/

#include    "uemf.h"

/******************************** Prototypes **********************************/

extern int      ejArgs(int argc, char_t **argv, char_t *fmt, ...);
extern void     ejSetResult(int eid, char_t *s);
extern int      ejOpenEngine(sym_fd_t variables, sym_fd_t functions);
extern void     ejCloseEngine(int eid);
extern int      ejSetGlobalFunction(int eid, char_t *name, int (*fn)(int eid, void *handle, int argc, char_t **argv));
extern void     ejSetVar(int eid, char_t *var, char_t *value);
extern int      ejGetVar(int eid, char_t *var, char_t **value);
extern char_t   *ejEval(int eid, char_t *script, char_t **emsg);

#endif /* _h_EJ */

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
