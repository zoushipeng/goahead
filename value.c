/*
    value.c -- Generic type (holds all types)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "uemf.h"

/*********************************** Code *************************************/

value_t valueInteger(long value)
{
    value_t v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = integer;
    v.value.integer = value;
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
        v.value.string = gstrdup(B_L, value);
    } else {
        v.allocated = 0;
        v.value.string = value;
    }
    return v;
}


void valueFree(value_t* v)
{
    if (v->valid && v->allocated && v->type == string && v->value.string != NULL) {
        bfree(B_L, v->value.string);
    }
    v->type = undefined;
    v->valid = 0;
    v->allocated = 0;
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
