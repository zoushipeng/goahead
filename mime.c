/*
    mime.c -- Web server mime types

    Mime types and file extensions. This module maps URL extensions to content types.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "wsIntrn.h"

/******************************** Global Data *********************************/
/*
    Addd entries to the MimeList as required for your content
    MOB - compare with appweb
 */
//  MOB - move to config?
#define MORE_MIME_TYPES 1
websMimeType websMimeList[] = {
    { T("application/java"), T(".class") },
    { T("application/java"), T(".jar") },
    { T("text/html"), T(".asp") },
    { T("text/html"), T(".htm") },
    { T("text/html"), T(".html") },
    { T("text/xml"), T(".xml") },
    { T("image/gif"), T(".gif") },
    { T("image/jpeg"), T(".jpg") },
    { T("image/png"), T(".png") },
    { T("image/vnd.microsoft.icon"), T(".ico") },
    { T("text/css"), T(".css") },
    { T("text/plain"), T(".txt") },
   { T("application/x-javascript"), T(".js") },
   { T("application/x-shockwave-flash"), T(".swf") },

#ifdef MORE_MIME_TYPES
    { T("application/binary"), T(".exe") },
    { T("application/compress"), T(".z") },
    { T("application/gzip"), T(".gz") },
    { T("application/octet-stream"), T(".bin") },
    { T("application/oda"), T(".oda") },
    { T("application/pdf"), T(".pdf") },
    { T("application/postscript"), T(".ai") },
    { T("application/postscript"), T(".eps") },
    { T("application/postscript"), T(".ps") },
    { T("application/rtf"), T(".rtf") },
    { T("application/x-bcpio"), T(".bcpio") },
    { T("application/x-cpio"), T(".cpio") },
    { T("application/x-csh"), T(".csh") },
    { T("application/x-dvi"), T(".dvi") },
    { T("application/x-gtar"), T(".gtar") },
    { T("application/x-hdf"), T(".hdf") },
    { T("application/x-latex"), T(".latex") },
    { T("application/x-mif"), T(".mif") },
    { T("application/x-netcdf"), T(".nc") },
    { T("application/x-netcdf"), T(".cdf") },
    { T("application/x-ns-proxy-autoconfig"), T(".pac") },
    { T("application/x-patch"), T(".patch") },
    { T("application/x-sh"), T(".sh") },
    { T("application/x-shar"), T(".shar") },
    { T("application/x-sv4cpio"), T(".sv4cpio") },
    { T("application/x-sv4crc"), T(".sv4crc") },
    { T("application/x-tar"), T(".tar") },
    { T("application/x-tgz"), T(".tgz") },
    { T("application/x-tcl"), T(".tcl") },
    { T("application/x-tex"), T(".tex") },
    { T("application/x-texinfo"), T(".texinfo") },
    { T("application/x-texinfo"), T(".texi") },
    { T("application/x-troff"), T(".t") },
    { T("application/x-troff"), T(".tr") },
    { T("application/x-troff"), T(".roff") },
    { T("application/x-troff-man"), T(".man") },
    { T("application/x-troff-me"), T(".me") },
    { T("application/x-troff-ms"), T(".ms") },
    { T("application/x-ustar"), T(".ustar") },
    { T("application/x-wais-source"), T(".src") },
    { T("application/zip"), T(".zip") },
    { T("audio/basic"), T(".au snd") },
    { T("audio/x-aiff"), T(".aif") },
    { T("audio/x-aiff"), T(".aiff") },
    { T("audio/x-aiff"), T(".aifc") },
    { T("audio/x-wav"), T(".wav") },
    { T("audio/x-wav"), T(".ram") },
    { T("image/ief"), T(".ief") },
    { T("image/jpeg"), T(".jpeg") },
    { T("image/jpeg"), T(".jpe") },
    { T("image/tiff"), T(".tiff") },
    { T("image/tiff"), T(".tif") },
    { T("image/x-cmu-raster"), T(".ras") },
    { T("image/x-portable-anymap"), T(".pnm") },
    { T("image/x-portable-bitmap"), T(".pbm") },
    { T("image/x-portable-graymap"), T(".pgm") },
    { T("image/x-portable-pixmap"), T(".ppm") },
    { T("image/x-rgb"), T(".rgb") },
    { T("image/x-xbitmap"), T(".xbm") },
    { T("image/x-xpixmap"), T(".xpm") },
    { T("image/x-xwindowdump"), T(".xwd") },
    { T("text/html"), T(".cfm") },
    { T("text/html"), T(".shtm") },
    { T("text/html"), T(".shtml") },
    { T("text/richtext"), T(".rtx") },
    { T("text/tab-separated-values"), T(".tsv") },
    { T("text/x-setext"), T(".etx") },
    { T("video/mpeg"), T(".mpeg mpg mpe") },
    { T("video/quicktime"), T(".qt") },
    { T("video/quicktime"), T(".mov") },
    { T("video/x-msvideo"), T(".avi") },
    { T("video/x-sgi-movie"), T(".movie") },
#endif
    { NULL, NULL},
};


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
