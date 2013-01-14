/*
    webcomp -- Compile web pages into C source

    Usage: webcomp --prefix prefix filelist >webrom.c
    Where: 
        filelist is a file containing the pathnames of all web pages
        prefix is a path prefix to remove from all the web page pathnames
        webrom.c is the resulting C source file to compile and link.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/**************************** Forward Declarations ****************************/

static int  compile(char *fileList, char *prefix);
static void usage();

/*********************************** Code *************************************/

int main(int argc, char* argv[])
{
    char    *argp, *fileList, *prefix;
    int     argind;

    fileList = NULL;
    prefix = "";

    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;
        } else if (strcmp(argp, "--prefix") == 0) {
            if (argind >= argc) usage();
            prefix = argv[++argind];
        }
    }
    if (argind >= argc) {
        usage();
    }
    fileList = argv[argind];
    if (compile(fileList, prefix) < 0) {
        return -1;
    }
    return 0;
}


static void usage()
{
    fprintf(stderr, "usage: webcomp [--prefix prefix] filelist >output.c\n\
        --prefix specifies is a path prefix to remove from all the web page pathnames\n\
        filelist is a file containing the pathnames of all web pages\n\
        output.c is the resulting C source file to compile and link.\n");
    exit(2);
}


static int compile(char *fileList, char *prefix)
{
    WebsStat        sbuf;
    WebsTime        now;
    FILE            *lp;
    char            buf[512], file[BIT_GOAHEAD_LIMIT_FILENAME], *cp, *sl;
    uchar           *p;
    ssize           len;
    int             j, i, fd, nFile;

    if ((lp = fopen(fileList, "r")) == NULL) {
        fprintf(stderr, "Can't open file list %s\n", fileList);
        return -1;
    }
    time(&now);
    fprintf(stdout, "/*\n   rom-documents.c \n");
    fprintf(stdout, "   Compiled by webcomp: %s */\n\n", ctime(&now));
    fprintf(stdout, "#include \"goahead.h\"\n\n");
    fprintf(stdout, "#if BIT_ROM\n\n");

    /*
        Open each input file and compile each web page
     */
    nFile = 0;
    while (fgets(file, sizeof(file), lp) != NULL) {
        if ((p = (uchar*) strchr(file, '\n')) || (p = (uchar*) strchr(file, '\r'))) {
            *p = '\0';
        }
        if (*file == '\0') {
            continue;
        }
        if (stat(file, &sbuf) == 0 && sbuf.st_mode & S_IFDIR) {
            continue;
        } 
        if ((fd = open(file, O_RDONLY | O_BINARY, 0644)) < 0) {
            fprintf(stderr, "Can't open file %s\n", file);
            return -1;
        }
        fprintf(stdout, "/* %s */\n", file);
        fprintf(stdout, "static uchar p%d[] = {\n", nFile);

        while ((len = read(fd, buf, sizeof(buf))) > 0) {
            p = (uchar*)buf;
            for (i = 0; i < len; ) {
                fprintf(stdout, "\t");
                for (j = 0; p< (uchar*)(&buf[len]) && j < 16; j++, p++) {
                    fprintf(stdout, "%4d,", *p);
                }
                i += j;
                fprintf(stdout, "\n");
            }
        }
        fprintf(stdout, "\t   0\n};\n\n");
        close(fd);
        nFile++;
    }
    fclose(lp);

    /*
        Output the page index
     */
    fprintf(stdout, "WebsRomIndex websRomIndex[] = {\n");

    if ((lp = fopen(fileList, "r")) == NULL) {
        fprintf(stderr, "Can't open file list %s\n", fileList);
        return -1;
    }
    nFile = 0;
    while (fgets(file, sizeof(file), lp) != NULL) {
        if ((p = (uchar*) strchr(file, '\n')) || (p = (uchar *) strchr(file, '\r'))) {
            *p = '\0';
        }
        if (*file == '\0') {
            continue;
        }
        /*
            Remove the prefix and add a leading "/" when we print the path
         */
        if (strncmp(file, prefix, strlen(prefix)) == 0) {
            cp = &file[strlen(prefix)];
        } else {
            cp = file;
        }
        while((sl = strchr(file, '\\')) != NULL) {
            *sl = '/';
        }
        if (*cp == '/') {
            cp++;
        }

        if (stat(file, &sbuf) == 0 && sbuf.st_mode & S_IFDIR) {
            fprintf(stdout, "\t{ \"%s\", 0, 0 },\n", cp);
            continue;
        }
        fprintf(stdout, "\t{ \"%s\", p%d, %d },\n", cp, nFile, (int) sbuf.st_size);
        nFile++;
    }
    fclose(lp); 
    fprintf(stdout, "\t{ 0, 0, 0 }\n");
    fprintf(stdout, "};\n");
    fprintf(stdout, "#else\n");
    fprintf(stdout, "WebsRomIndex websRomIndex[] = {\n");
    fprintf(stdout, "\t{ 0, 0, 0 }\n};\n");
    fprintf(stdout, "#endif\n");
    fflush(stdout);
    return 0;
}

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2013. All Rights Reserved.

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
