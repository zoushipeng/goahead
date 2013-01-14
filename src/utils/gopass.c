/*
    gopass.c -- Authorization password management.

    This file provides facilities for creating passwords in a route configuration file. 
    It uses basic encoding and decoding using the base64 encoding scheme and MD5 Message-Digest algorithms.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/********************************** Locals ************************************/

#define MAX_PASS    64

/********************************* Forwards ***********************************/

static char *getPassword();
static void printUsage();
static int writeAuthFile(char *path);

#if BIT_WIN_LIKE || VXWORKS
static char *getpass(char *prompt);
#endif

/*********************************** Code *************************************/

int main(int argc, char *argv[])
{
    WebsBuf     buf;
    char        *password, *authFile, *username, *encodedPassword, *realm, *cp, *roles;
    int         i, errflg, create, nextArg;

    username = 0;
    create = errflg = 0;
    password = 0;

    for (i = 1; i < argc && !errflg; i++) {
        if (argv[i][0] != '-') {
            break;
        }
        for (cp = &argv[i][1]; *cp && !errflg; cp++) {
            if (*cp == 'c') {
                create++;

            } else if (*cp == 'p') {
                if (++i == argc) {
                    errflg++;
                } else {
                    password = argv[i];
                    break;
                }

            } else {
                errflg++;
            }
        }
    }
    nextArg = i;

    if ((nextArg + 3) > argc) {
        errflg++;
    }
    if (errflg) {
        printUsage();
        exit(2);
    }   
    authFile = argv[nextArg++];
    realm = argv[nextArg++];
    username = argv[nextArg++];

    bufCreate(&buf, 0, 0);
    for (i = nextArg; i < argc; ) {
        bufPutStr(&buf, argv[i]);
        if (++i < argc) {
            bufPutc(&buf, ',');
        }
    }
    roles = sclone(buf.servp);
    websOpenAuth(1);
    
    if (!create) {
        if (websLoad(authFile) < 0) {
            exit(2);
        }
        if (access(authFile, W_OK) < 0) {
            error("Can't write to %s", authFile);
            exit(4);
        }
    } else if (access(authFile, R_OK) < 0) {
        error("Can't create %s, already exists", authFile);
        exit(5);
    }
    if (!password && (password = getPassword()) == 0) {
        exit(1);
    }
    encodedPassword = websMD5(sfmt("%s:%s:%s", username, realm, password));

    websRemoveUser(username);
    if (websAddUser(username, encodedPassword, roles) < 0) {
        exit(7);
    }
    if (writeAuthFile(authFile) < 0) {
        exit(6);
    }
    websCloseAuth();
    return 0;
}


static int writeAuthFile(char *path)
{
    FILE        *fp;
    WebsKey     *kp, *ap;
    WebsRole    *role;
    WebsUser    *user;
    WebsHash    roles, users;
    char        *tempFile;

    assert(path && *path);

    tempFile = tempnam(NULL, "tmp");
    if ((fp = fopen(tempFile, "w" FILE_TEXT)) == 0) {
        error("Can't open %s", tempFile);
        return -1;
    }
    fprintf(fp, "#\n#   %s - Authorization data\n#\n\n", basename(path));

    roles = websGetRoles();
    if (roles >= 0) {
        for (kp = hashFirst(roles); kp; kp = hashNext(roles, kp)) {
            role = kp->content.value.symbol;
            fprintf(fp, "role name=%s abilities=", kp->name.value.string);
            for (ap = hashFirst(role->abilities); ap; ap = hashNext(role->abilities, ap)) {
                fprintf(fp, "%s,", ap->name.value.string);
            }
            fputc('\n', fp);
        }
        fputc('\n', fp);
    }
    users = websGetUsers();
    if (users >= 0) {
        for (kp = hashFirst(users); kp; kp = hashNext(users, kp)) {
            user = kp->content.value.symbol;
            fprintf(fp, "user name=%s password=%s roles=%s", user->name, user->password, user->roles);
            fputc('\n', fp);
        }
    }
    fclose(fp);
    unlink(path);
    if (rename(tempFile, path) < 0) {
        error("Can't create new %s", path);
        return -1;
    }
    return 0;
}


static char *getPassword()
{
    char    *password, *confirm;

    password = getpass("New password: ");
    confirm = getpass("Confirm password: ");
    if (smatch(password, confirm)) {
        return password;
    }
    error("Password not verified");
    return 0;
}



#if WINCE
static char *getpass(char *prompt)
{
    return "NOT-SUPPORTED";
}

#elif BIT_WIN_LIKE || VXWORKS
static char *getpass(char *prompt)
{
    static char password[MAX_PASS];
    int     c, i;

    fputs(prompt, stderr);
    for (i = 0; i < (int) sizeof(password) - 1; i++) {
#if VXWORKS
        c = getchar();
#else
        c = _getch();
#endif
        if (c == '\r' || c == EOF) {
            break;
        }
        if ((c == '\b' || c == 127) && i > 0) {
            password[--i] = '\0';
            fputs("\b \b", stderr);
            i--;
        } else if (c == 26) {           /* Control Z */
            c = EOF;
            break;
        } else if (c == 3) {            /* Control C */
            fputs("^C\n", stderr);
            exit(255);
        } else if (!iscntrl((uchar) c) && (i < (int) sizeof(password) - 1)) {
            password[i] = c;
            fputc('*', stderr);
        } else {
            fputc('', stderr);
            i--;
        }
    }
    if (c == EOF) {
        return "";
    }
    fputc('\n', stderr);
    password[i] = '\0';
    return sclone(password);
}

#endif /* BIT_WIN_LIKE */
 
/*
    Display the usage
 */

static void printUsage()
{
    error("usage: gopass [-c] [-p password] authFile realm user roles...\n"
        "Options:\n"
        "    -c              Create the password file\n"
        "    -p passWord     Use the specified password\n"
        "    -e              Enable (default)\n"
        "    -d              Disable\n");
}


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2013. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
