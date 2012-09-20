/*
    um.c -- User Management

    User Management routines for adding/deleting/changing users and groups
    Also, routines for determining user access

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

#if BIT_USER_MANAGEMENT
/********************************** Defines ***********************************/

#define UM_TXT_FILENAME T("umconfig.db")

/*
    Table names
 */
#define UM_USER_TABLENAME   T("users")
#define UM_GROUP_TABLENAME  T("groups")
#define UM_ACCESS_TABLENAME T("access")

/*
    Column names
 */
#define UM_NAME         T("name")
#define UM_PASS         T("password")
#define UM_GROUP        T("group")
#define UM_PROT         T("prot")
#define UM_DISABLE      T("disable")
#define UM_METHOD       T("method")
#define UM_PRIVILEGE    T("priv")
#define UM_SECURE       T("secure")

/*
    XOR encryption mask
    Note:This string should be modified for individual sites
        in order to enhance user password security.
    MOB - generate or move to config.
 */
#define UM_SALT  T("*j7a(L#yZ98sSd5HfSgGjMj8;Ss;d)(*&^#@$a2s0i3g")

#define NONE_OPTION     T("<NONE>")
#define MSG_START       T("<body><h2>")
#define MSG_END         T("</h2></body>")

/********************************** Defines ***********************************/
/*
    User table definition
 */
#define NUMBER_OF_USER_COLUMNS  5

static char *userColumnNames[NUMBER_OF_USER_COLUMNS] = {
        UM_NAME, UM_PASS, UM_GROUP, UM_PROT, UM_DISABLE
};

static int userColumnTypes[NUMBER_OF_USER_COLUMNS] = {
        T_STRING, T_STRING, T_STRING, T_INT, T_INT
};

static dbTable_t userTable = {
    UM_USER_TABLENAME,
    NUMBER_OF_USER_COLUMNS,
    userColumnNames,
    userColumnTypes,
    0,
    NULL
};

/*
    Group table definition
 */
#define NUMBER_OF_GROUP_COLUMNS 5

static char *groupColumnNames[NUMBER_OF_GROUP_COLUMNS] = {
        UM_NAME, UM_PRIVILEGE, UM_METHOD, UM_PROT, UM_DISABLE
};

static int groupColumnTypes[NUMBER_OF_GROUP_COLUMNS] = {
    T_STRING, T_INT, T_INT, T_INT, T_INT
};

static dbTable_t groupTable = {
    UM_GROUP_TABLENAME,
    NUMBER_OF_GROUP_COLUMNS,
    groupColumnNames,
    groupColumnTypes,
    0,
    NULL
};

/*
    Access Limit table definition
 */
#define NUMBER_OF_ACCESS_COLUMNS    4

static char *accessColumnNames[NUMBER_OF_ACCESS_COLUMNS] = {
    UM_NAME, UM_METHOD, UM_SECURE, UM_GROUP
};

static int accessColumnTypes[NUMBER_OF_ACCESS_COLUMNS] = {
    T_STRING, T_INT, T_INT, T_STRING
};

static dbTable_t accessTable = {
    UM_ACCESS_TABLENAME,
    NUMBER_OF_ACCESS_COLUMNS,
    accessColumnNames,
    accessColumnTypes,
    0,
    NULL
};

/* 
    Database Identifier returned from dbOpen()
 */
static int udb = -1; 

/* 
    User database
    MOB - rename
 */
static char *umDbName = NULL;

/********************************** Forwards **********************************/

static bool_t   checkName(char *name);
static void     cryptPassword(char *textString);

#if BIT_USER_MANAGEMENT_GUI
static int      jsGenerateUserList(int eid, Webs *wp, int argc, char **argv);
static int      jsGenerateGroupList(int eid, Webs *wp, int argc, char **argv);
static int      jsGenerateAccessLimitList(int eid, Webs *wp, int argc, char **argv);
static int      jsGenerateAccessMethodList(int eid, Webs *wp, int argc, char **argv);
static int      jsGeneratePrivilegeList(int eid, Webs *wp, int argc, char **argv);
static void     formAddUser(Webs *wp, char *path, char *query);
static void     formDeleteUser(Webs *wp, char *path, char *query);
static void     formDisplayUser(Webs *wp, char *path, char *query);
static void     formAddGroup(Webs *wp, char *path, char *query);
static void     formDeleteGroup(Webs *wp, char *path, char *query);
static void     formAddAccessLimit(Webs *wp, char *path, char *query);
static void     formDeleteAccessLimit(Webs *wp, char *path, char *query);
#endif

/*********************************** Code *************************************/

int umOpen(char *path)
{
    if ((udb = dbOpen(UM_USER_TABLENAME, NULL, NULL, 0)) < 0) {
        return -1;
    }
    dbRegisterDBSchema(&userTable);
    dbRegisterDBSchema(&groupTable);
    dbRegisterDBSchema(&accessTable);
    umDbName = strdup(path ? path : UM_TXT_FILENAME);
#if UNUSED
    dbZero(udb);
#endif
    return dbLoad(udb, umDbName, 0);
}


void umClose()
{
    if (udb >= 0) {
        dbClose(udb);
        udb = -1;
    }
    gfree(umDbName);
    umDbName = NULL;
}


int umSave()
{
    trace(3, T("Save user configuration to: %s"), umDbName);
    return dbSave(udb, umDbName, 0);
}


/*
    Return a pointer to the first non-blank key value in the given column for the given table.
 */
static char *umGetFirstRowData(char *tableName, char *columnName)
{
    char  *columnData;
    int     row;
    int     check;

    gassert(tableName && *tableName);
    gassert(columnName && *columnName);

    row = 0;
    /*
        Move through table until we retrieve the first row with non-null column data.
     */
    columnData = NULL;
    while ((check = dbReadStr(udb, tableName, columnName, row++, &columnData)) == 0 || (check == DB_ERR_ROW_DELETED)) {
        if (columnData && *columnData) {
            return columnData;
        }
    }
    return NULL;
}


/*
    Return a pointer to the first non-blank key value following the given one.
 */
static char *umGetNextRowData(char *tableName, char *columnName, char *keyLast)
{
    char  *key;
    int     row;
    int     check;

    gassert(tableName && *tableName);
    gassert(columnName && *columnName);
    gassert(keyLast && *keyLast);

    /*
        Position row counter to row where the given key value was found
     */
    row = 0;
    key = NULL;

    while ((((check = dbReadStr(udb, tableName, columnName, row++, &key)) == 0) || (check == DB_ERR_ROW_DELETED)) &&
        ((key == NULL) || (strcmp(key, keyLast) != 0))) { }
    /*
        If the last key value was not found, return NULL
     */
    if (!key || strcmp(key, keyLast) != 0) {
        return NULL;
    }
    /*
        Move through table until we retrieve the next row with a non-null key
     */
    while (((check = dbReadStr(udb, tableName, columnName, row++, &key)) == 0) || (check == DB_ERR_ROW_DELETED)) {
        if (key && *key && (strcmp(key, keyLast) != 0)) {
            return key;
        }
    }

    return NULL;
}


/*
    Adds a user to the "users" table
 */
int umAddUser(char *user, char *pass, char *group, bool_t prot, bool_t disabled)
{
    int     row;
    char  *password;

    gassert(user && *user);
    gassert(pass && *pass);
    gassert(group && *group);

    trace(3, T("UM: Adding User <%s>"), user);

    /*
        Do not allow duplicates
     */
    if (umUserExists(user)) {
        return UM_ERR_DUPLICATE;
    }
    if (!checkName(user)) {
        return UM_ERR_BAD_NAME;
    }
    if (!checkName(pass)) {
        return UM_ERR_BAD_PASSWORD;
    }
    if (!umGroupExists(group)) {
        return UM_ERR_NOT_FOUND;
    }

    /*
        Now create the user record
     */
    row = dbAddRow(udb, UM_USER_TABLENAME);

    if (row < 0) {
        return UM_ERR_GENERAL;
    }
    if (dbWriteStr(udb, UM_USER_TABLENAME, UM_NAME, row, user) != 0) {
        return UM_ERR_GENERAL;
    }
    password = strdup(pass);
    cryptPassword(password);
    dbWriteStr(udb, UM_USER_TABLENAME, UM_PASS, row, password);
    gfree(password);

    dbWriteStr(udb, UM_USER_TABLENAME, UM_GROUP, row, group);
    dbWriteInt(udb, UM_USER_TABLENAME, UM_PROT, row, prot); 
    dbWriteInt(udb, UM_USER_TABLENAME, UM_DISABLE, row, disabled);
    return 0;
}


int umDeleteUser(char *user)
{
    int row;

    gassert(user && *user);
    trace(3, T("UM: Deleting User <%s>"), user);

    /*
        Check to see if user is delete-protected
     */
    if (umGetUserProtected(user)) {
        return UM_ERR_PROTECTED;
    } 
    if ((row = dbSearchStr(udb, UM_USER_TABLENAME, UM_NAME, user, 0)) >= 0) {
        return dbDeleteRow(udb, UM_USER_TABLENAME, row);
    } 
    return UM_ERR_NOT_FOUND;
}


/*
    Returns the user ID of the first user found in the "users" table
 */
char *umGetFirstUser()
{
    return umGetFirstRowData(UM_USER_TABLENAME, UM_NAME);
}


/*
    Returns the next user found in the "users" table after the given user.
 */
char *umGetNextUser(char *userLast)
{
    return umGetNextRowData(UM_USER_TABLENAME, UM_NAME, userLast);
}


bool_t umUserExists(char *user)
{
    gassert(user && *user);
    return dbSearchStr(udb, UM_USER_TABLENAME, UM_NAME, user, 0) >= 0;
}


/*
    Returns a decrypted copy of the user password
 */
char *umGetUserPassword(char *user)
{
    char  *password, *pass;
    int     row;

    gassert(user && *user);

    password = NULL;
    if ((row = dbSearchStr(udb, UM_USER_TABLENAME, UM_NAME, user, 0)) >= 0) {
        pass = NULL;
        dbReadStr(udb, UM_USER_TABLENAME, UM_PASS, row, &pass);
        /*
            Decrypt password. This returns a copy of the password, which must be deleted at some time in the future.
         */
        password = strdup(pass);
        cryptPassword(password);
    }
    return password;
}


/*
    Updates the user password in the user "table" after encrypting the given password
 */
int umSetUserPassword(char *user, char *pass)
{
    char  *password;
    int     row, rc;

    gassert(user && *user);
    gassert(pass && *pass);
    trace(3, T("UM: Attempting to change the password for user <%s>"), user);

    /*
        Find the row of the user
     */
    if ((row = dbSearchStr(udb, UM_USER_TABLENAME, UM_NAME, user, 0)) < 0) {
        return UM_ERR_NOT_FOUND;
    }
    password = strdup(pass);
    cryptPassword(password);
    rc = dbWriteStr(udb, UM_USER_TABLENAME, UM_PASS, row, password);
    gfree(password);
    return rc;
}


char *umGetUserGroup(char *user)
{
    char  *group;
    int     row;

    gassert(user && *user);
    /*
        Find the row of the user
     */
    row = dbSearchStr(udb, UM_USER_TABLENAME, UM_NAME, user, 0);
    group = NULL;
    if (row >= 0) {
        dbReadStr(udb, UM_USER_TABLENAME, UM_GROUP, row, &group);
    }
    return group;
}


/*
    Set the name of the user group for the user
 */
int umSetUserGroup(char *user, char *group)
{
    int row;

    gassert(user && *user);
    gassert(group && *group);

    /*
        Find the row of the user
     */
    row = dbSearchStr(udb, UM_USER_TABLENAME, UM_NAME, user, 0);
    if (row >= 0) {
        return dbWriteStr(udb, UM_USER_TABLENAME, UM_GROUP, row, group);
    } else {
        return UM_ERR_NOT_FOUND;
    }
}

/*
    Returns if the user is enabled. Returns FALSE if the user is not found.
 */
bool_t  umGetUserEnabled(char *user)
{
    int disabled, row;

    gassert(user && *user);
    disabled = 1;
    /*
        Find the row of the user
     */
    row = dbSearchStr(udb, UM_USER_TABLENAME, UM_NAME, user, 0);
    if (row >= 0) {
        dbReadInt(udb, UM_USER_TABLENAME, UM_DISABLE, row, &disabled);
    }
    return (bool_t)!disabled;
}


int umSetUserEnabled(char *user, bool_t enabled)
{
    int row;

    gassert(user && *user);
    /*
        Find the row of the user
     */
    row = dbSearchStr(udb, UM_USER_TABLENAME, UM_NAME, user, 0);
    if (row >= 0) {
        return dbWriteInt(udb, UM_USER_TABLENAME, UM_DISABLE, row, !enabled);
    } else {
        return UM_ERR_NOT_FOUND;
    }
}


/*
    Determine deletability of user
 */
bool_t umGetUserProtected(char *user)
{
    int protect, row;

    gassert(user && *user);
    /*
        Find the row of the user
     */
    row = dbSearchStr(udb, UM_USER_TABLENAME, UM_NAME, user, 0);
    protect = 0;
    if (row >= 0) {
        dbReadInt(udb, UM_USER_TABLENAME, UM_PROT, row, &protect);
    }
    return (bool_t) protect;
}


/*
    Sets the delete protection for the user
 */
int umSetUserProtected(char *user, bool_t protect)
{
    int row;

    gassert(user && *user);
    /*
        Find the row of the user
     */
    row = dbSearchStr(udb, UM_USER_TABLENAME, UM_NAME, user, 0);
    if (row >= 0) {
        return dbWriteInt(udb, UM_USER_TABLENAME, UM_PROT, row, protect);
    } else {
        return UM_ERR_NOT_FOUND;
    }
}


int umAddGroup(char *group, short priv, accessMeth_t am, bool_t prot, bool_t disabled)
{
    int row;

    gassert(group && *group);
    trace(3, T("UM: Adding group <%s>"), group);
    
    /*
        Do not allow duplicates
     */
    if (umGroupExists(group)) {
        return UM_ERR_DUPLICATE;
    }
    if (!checkName(group)) {
        return UM_ERR_BAD_NAME;
    }
    if ((row = dbAddRow(udb, UM_GROUP_TABLENAME)) < 0) {
        return UM_ERR_GENERAL;
    }
    /*
        Write the key field
     */
    if (dbWriteStr(udb, UM_GROUP_TABLENAME, UM_NAME, row, group) != 0) {
        return UM_ERR_GENERAL;
    }
    /*
        Write the remaining fields
     */
    dbWriteInt(udb, UM_GROUP_TABLENAME, UM_PRIVILEGE, row, priv);
    dbWriteInt(udb, UM_GROUP_TABLENAME, UM_METHOD, row, (int) am);
    dbWriteInt(udb, UM_GROUP_TABLENAME, UM_PROT, row, prot);
    dbWriteInt(udb, UM_GROUP_TABLENAME, UM_DISABLE, row, disabled);
    return 0;
}


int umDeleteGroup(char *group)
{
    int row;

    gassert(group && *group);
    trace(3, T("UM: Deleting Group <%s>"), group);

    /*
        Check to see if the group is in use
     */
    if (umGetGroupInUse(group)) {
        return UM_ERR_IN_USE;
    } 
    /*
        Check to see if the group is delete-protected
     */
    if (umGetGroupProtected(group)) {
        return UM_ERR_PROTECTED;
    } 
    /*
        Find the row of the group to delete
     */
    if ((row = dbSearchStr(udb, UM_GROUP_TABLENAME, UM_NAME, group, 0)) < 0) {
        return UM_ERR_NOT_FOUND;
    }
    return dbDeleteRow(udb, UM_GROUP_TABLENAME, row);
}


bool_t umGroupExists(char *group)
{
    gassert(group && *group);
    return dbSearchStr(udb, UM_GROUP_TABLENAME, UM_NAME, group, 0) >= 0;
}


/*
    Returns TRUE if the group is referenced by a user or by an access limit.
 */
bool_t umGetGroupInUse(char *group)
{
    gassert(group && *group);

    /*
        First, check the user table
     */
    if (dbSearchStr(udb, UM_USER_TABLENAME, UM_GROUP, group, 0) >= 0) {
        return 1;
    } 
    /*
        Second, check the access limit table
     */
    if (dbSearchStr(udb, UM_ACCESS_TABLENAME, UM_GROUP, group, 0) >= 0) {
        return 1;
    } 
    return 0;
}


/*
    Return a pointer to the first non-blank group name
 */
char *umGetFirstGroup()
{
    return umGetFirstRowData(UM_GROUP_TABLENAME, UM_NAME);
}


/*
    Return a pointer to the first non-blank group name following the given group name
 */
char *umGetNextGroup(char *groupLast)
{
    return umGetNextRowData(UM_GROUP_TABLENAME, UM_NAME, groupLast);
}


/*
    Returns the default access method to use for a given group
 */
accessMeth_t umGetGroupAccessMethod(char *group)
{
    int am, row;

    gassert(group && *group);
    row = dbSearchStr(udb, UM_GROUP_TABLENAME, UM_NAME, group, 0);
    if (row >= 0) {
        dbReadInt(udb, UM_GROUP_TABLENAME, UM_METHOD, row, (int *)&am);
    } else {
        am = AM_INVALID;
    }
    return (accessMeth_t) am;
}


/*
    Set the default access method to use for a given group
 */
int umSetGroupAccessMethod(char *group, accessMeth_t am)
{
    int row;

    gassert(group && *group);
    row = dbSearchStr(udb, UM_GROUP_TABLENAME, UM_NAME, group, 0);
    if (row >= 0) {
        return dbWriteInt(udb, UM_GROUP_TABLENAME, UM_METHOD, row, (int) am);
    } else {
        return UM_ERR_NOT_FOUND;
    }
}


/*
    Returns the privilege mask for a given group
 */
short umGetGroupPrivilege(char *group)
{
    int privilege, row;

    gassert(group && *group);
    privilege = -1;
    row = dbSearchStr(udb, UM_GROUP_TABLENAME, UM_NAME, group, 0);
    if (row >= 0) {
        dbReadInt(udb, UM_GROUP_TABLENAME, UM_PRIVILEGE, row, &privilege);
    }
    return (short) privilege;
}


/*
    Set the privilege mask for a given group
 */
int umSetGroupPrivilege(char *group, short privilege)
{
    int row;

    gassert(group && *group);
    row = dbSearchStr(udb, UM_GROUP_TABLENAME, UM_NAME, group, 0);

    if (row >= 0) {
        return dbWriteInt(udb, UM_GROUP_TABLENAME, UM_PRIVILEGE, row, (int)privilege);
    } else {
        return UM_ERR_NOT_FOUND;
    }
}


/*
    Returns the enabled setting for a given group. Returns FALSE if group is not found.
 */
bool_t umGetGroupEnabled(char *group)
{
    int disabled, row;

    gassert(group && *group);
    row = dbSearchStr(udb, UM_GROUP_TABLENAME, UM_NAME, group, 0);
    disabled = 1;

    if (row >= 0) {
        dbReadInt(udb, UM_GROUP_TABLENAME, UM_DISABLE, row, &disabled);
    }
    return (bool_t) !disabled;
}


/*
    Sets the enabled setting for a given group.
 */
int umSetGroupEnabled(char *group, bool_t enabled)
{
    int row;

    gassert(group && *group);
    row = dbSearchStr(udb, UM_GROUP_TABLENAME, UM_NAME, group, 0);

    if (row >= 0) {
        return dbWriteInt(udb, UM_GROUP_TABLENAME, UM_DISABLE, row, (int) !enabled);
    } else {
        return UM_ERR_NOT_FOUND;
    }
}


/*
    Returns the protected setting for a given group. Returns FALSE if user is not found
 */
bool_t umGetGroupProtected(char *group)
{
    int protect, row;

    gassert(group && *group);

    protect = 0;
    row = dbSearchStr(udb, UM_GROUP_TABLENAME, UM_NAME, group, 0);
    if (row >= 0) {
        dbReadInt(udb, UM_GROUP_TABLENAME, UM_PROT, row, &protect);
    }
    return (bool_t) protect;
}


/*
    Sets the protected setting for a given group
 */
int umSetGroupProtected(char *group, bool_t protect)
{
    int row;

    gassert(group && *group);
    row = dbSearchStr(udb, UM_GROUP_TABLENAME, UM_NAME, group, 0);

    if (row >= 0) {
        return dbWriteInt(udb, UM_GROUP_TABLENAME, UM_PROT, row, (int) protect);
    } else {
        return UM_ERR_NOT_FOUND;
    }
}


/*
    Add an access limit to the "access" table
 */
int umAddAccessLimit(char *url, accessMeth_t am, short secure, char *group)
{
    int row;

    gassert(url && *url);
    trace(3, T("UM: Adding Access Limit for <%s>"), url);

    /*
        Do not allow duplicates
     */
    if (umAccessLimitExists(url)) {
        return UM_ERR_DUPLICATE;
    }
    /*
        Add a new row to the table
     */
    if ((row = dbAddRow(udb, UM_ACCESS_TABLENAME)) < 0) {
        return UM_ERR_GENERAL;
    }
    /*
        Write the key field
     */
    if(dbWriteStr(udb, UM_ACCESS_TABLENAME, UM_NAME, row, url) < 0) {
        return UM_ERR_GENERAL;
    }

    /*
        Write the remaining fields
     */
    dbWriteInt(udb, UM_ACCESS_TABLENAME, UM_METHOD, row, (int)am);
    dbWriteInt(udb, UM_ACCESS_TABLENAME, UM_SECURE, row, (int)secure);
    dbWriteStr(udb, UM_ACCESS_TABLENAME, UM_GROUP, row, group);
    return 0;
}


int umDeleteAccessLimit(char *url)
{
    int row;

    gassert(url && *url);
    trace(3, T("UM: Deleting Access Limit for <%s>"), url);
    /*
        Find the row of the access limit to delete
     */
    if ((row = dbSearchStr(udb, UM_ACCESS_TABLENAME, UM_NAME, url, 0)) < 0) {
        return UM_ERR_NOT_FOUND;
    }
    return dbDeleteRow(udb, UM_ACCESS_TABLENAME, row);
}


/*
    Return a pointer to the first non-blank access limit
 */
char *umGetFirstAccessLimit()
{
    return umGetFirstRowData(UM_ACCESS_TABLENAME, UM_NAME);
}


/*
    Return a pointer to the first non-blank access limit following the given one
 */
char *umGetNextAccessLimit(char *urlLast)
{
    return umGetNextRowData(UM_ACCESS_TABLENAME, UM_NAME, urlLast);
}


/*
    umAccessLimitExists() returns TRUE if this access limit exists
 */
bool_t  umAccessLimitExists(char *url)
{
    gassert(url && *url);
    return dbSearchStr(udb, UM_ACCESS_TABLENAME, UM_NAME, url, 0) >= 0;
}


/*
    Returns the Access Method for the URL
 */
accessMeth_t umGetAccessLimitMethod(char *url)
{
    int am, row;

    am = (int) AM_INVALID;
    row = dbSearchStr(udb, UM_ACCESS_TABLENAME, UM_NAME, url, 0);
    if (row >= 0) {
        dbReadInt(udb, UM_ACCESS_TABLENAME, UM_METHOD, row, &am);
    } 
    return (accessMeth_t) am;
}


/*
    Set Access Method for Access Limit
 */
int umSetAccessLimitMethod(char *url, accessMeth_t am)
{
    int row;

    gassert(url && *url);
    row = dbSearchStr(udb, UM_ACCESS_TABLENAME, UM_NAME, url, 0);
    if (row >= 0) {
        return dbWriteInt(udb, UM_ACCESS_TABLENAME, UM_METHOD, row, (int) am);
    } else {
        return UM_ERR_NOT_FOUND;
    }
}


/*
    Returns secure switch for access limit
 */
short umGetAccessLimitSecure(char *url)
{
    int secure, row;

    gassert(url && *url);
    secure = -1;
    row = dbSearchStr(udb, UM_ACCESS_TABLENAME, UM_NAME, url, 0);

    if (row >= 0) {
        dbReadInt(udb, UM_ACCESS_TABLENAME, UM_SECURE, row, &secure);
    }
    return (short)secure;
}


/*
    Sets the secure flag for the URL
 */
int umSetAccessLimitSecure(char *url, short secure)
{
    int row;

    gassert(url && *url);
    row = dbSearchStr(udb, UM_ACCESS_TABLENAME, UM_NAME, url, 0);

    if (row >= 0) {
        return dbWriteInt(udb, UM_ACCESS_TABLENAME, UM_SECURE, row, (int)secure);
    } else {
        return UM_ERR_NOT_FOUND;
    }
}


/*
    Returns the user group of the access limit
 */
char *umGetAccessLimitGroup(char *url)
{
    char  *group;
    int     row;

    gassert(url && *url);
    group = NULL;
    row = dbSearchStr(udb, UM_ACCESS_TABLENAME, UM_NAME, url, 0);
    if (row >= 0) {
        dbReadStr(udb, UM_ACCESS_TABLENAME, UM_GROUP, row, &group);
    }
    return group;
}


/*
    Sets the user group for the access limit.
 */
int umSetAccessLimitGroup(char *url, char *group)
{
    int row;

    gassert(url && *url);
    row = dbSearchStr(udb, UM_ACCESS_TABLENAME, UM_NAME, url, 0);
    if (row >= 0) {
        return dbWriteStr(udb, UM_ACCESS_TABLENAME, UM_GROUP, row, group);
    } else {
        return UM_ERR_NOT_FOUND;
    }
}


/*
    Returns the access limit to use for a given URL, by checking for URLs up the directory tree.  Creates a new string
    that must be deleted.  
 */
char *umGetAccessLimit(char *url)
{
    char  *urlRet, *urlCheck, *lastChar;
    ssize   len;
    
    gassert(url && *url);
    urlRet = NULL;
    urlCheck = strdup(url);
    gassert(urlCheck);
    len = strlen(urlCheck);

    /*
        Scan back through URL to see if there is a "parent" access limit
     */
    while (len && !urlRet) {
        if (umAccessLimitExists(urlCheck)) {
            urlRet = strdup(urlCheck);
        } else {
            /*
                Trim the end portion of the URL to the previous directory marker
             */
            lastChar = urlCheck + len;
            lastChar--;

            while ((lastChar >= urlCheck) && ((*lastChar == '/') || 
                (*lastChar == '\\'))) {
                *lastChar = 0;
                lastChar--;
            }
            while ((lastChar >= urlCheck) && (*lastChar != '/') && 
                (*lastChar != '\\')) {
                *lastChar = 0;
                lastChar--;
            }
            len = strlen(urlCheck);
        }
    }
    gfree (urlCheck);

    return urlRet;
}


/*
    Returns the access method to use for a given URL
 */
accessMeth_t umGetAccessMethodForURL(char *url)
{
    accessMeth_t    amRet;
    char          *urlHavingLimit, *group;
    
    urlHavingLimit = umGetAccessLimit(url);
    if (urlHavingLimit) {
        group = umGetAccessLimitGroup(urlHavingLimit);

        if (group && *group) {
            amRet = umGetGroupAccessMethod(group);
        } else {
            amRet = umGetAccessLimitMethod(urlHavingLimit);
        }
        gfree(urlHavingLimit);
    } else {
        amRet = AM_FULL;
    }
    return amRet;
}


/*
    Returns TRUE if user can access URL
 */
bool_t umUserCanAccessURL(char *user, char *url)
{
    accessMeth_t    amURL;
    char          *group, *usergroup, *urlHavingLimit;
    short           priv;
    
    gassert(user && *user);
    gassert(url && *url);

    /*
        Make sure user exists
     */
    if (!umUserExists(user)) {
        return 0;
    }
    /*
        Make sure user is enabled
     */
    if (!umGetUserEnabled(user)) {
        return 0;
    }
    /*
        Make sure user has sufficient privileges (any will do)
     */
    usergroup = umGetUserGroup(user);
    if ((priv = umGetGroupPrivilege(usergroup)) == 0) {
        return 0;
    }
    if (!umGetGroupEnabled(usergroup)) {
        return 0;
    }
    /*
        The access method of the user group must not be AM_NONE
     */
    if (umGetGroupAccessMethod(usergroup) == AM_NONE) {
        return 0;
    }

    /*
        Check to see if there is an Access Limit for this URL
     */
    urlHavingLimit = umGetAccessLimit(url);
    if (urlHavingLimit) {
        amURL = umGetAccessLimitMethod(urlHavingLimit);
        group = umGetAccessLimitGroup(urlHavingLimit);
        gfree(urlHavingLimit);
    } else {
        /*
            If there isn't an access limit for the URL, user has full access
         */
        return 1;
    }
    /*
        If the access method for the URL is AM_NONE then the file "doesn't exist".
     */
    if (amURL == AM_NONE) {
        return 0;
    } 
    
    /*
        If Access Limit has a group specified, then the user must be a member of that group
     */
    if (group && *group) {
        if (usergroup && (strcmp(group, usergroup) != 0)) {
            return 0;
        }
    } 
    /*
        Otherwise, user can access the URL 
     */
    return 1;

}


/*
    Returns TRUE if given name has only valid chars
 */
static bool_t checkName(char *name)
{
    gassert(name && *name);

    if (name && *name) {
        while (*name) {
            if (isspace(*name)) {
                return 0;
            }
            name++;
        }
        return 1;
    }
    return 0;
}


/*
    Encrypt/Decrypt a text string
 */
static void cryptPassword(char *textString)
{
    char  c, *salt;

    gassert(textString);

    salt = UM_SALT;
    while (*textString) {
        c = *textString ^ *salt;
        /*
            Do not produce encrypted text with embedded linefeeds or tabs. Simply use existing character.
         */
        if (c && !isspace(c)) {
            *textString = c;
        }
        /*
            Increment all pointers.
         */
        salt++;
        textString++;
        /*
            Wrap encryption mask pointer if at end of length.
         */
        if (*salt == '\0') {
            salt = UM_SALT;
        }
    }
}


#if BIT_USER_MANAGEMENT_GUI
void formDefineUserMgmt(void)
{
    websJsDefine(T("MakeGroupList"), jsGenerateGroupList);
    websJsDefine(T("MakeUserList"), jsGenerateUserList);
    websJsDefine(T("MakeAccessLimitList"), jsGenerateAccessLimitList);
    websJsDefine(T("MakeAccessMethodList"), jsGenerateAccessMethodList);
    websJsDefine(T("MakePrivilegeList"), jsGeneratePrivilegeList);

    websFormDefine(T("AddUser"), formAddUser);
    websFormDefine(T("DeleteUser"), formDeleteUser);
    websFormDefine(T("DisplayUser"), formDisplayUser);
    websFormDefine(T("AddGroup"), formAddGroup);
    websFormDefine(T("DeleteGroup"), formDeleteGroup);
    websFormDefine(T("AddAccessLimit"), formAddAccessLimit);
    websFormDefine(T("DeleteAccessLimit"), formDeleteAccessLimit);

#if UNUSED
    websFormDefine(T("SaveUserManagement"), formSaveUserManagement);
    websFormDefine(T("LoadUserManagement"), formLoadUserManagement);
#endif
}


static void formAddUser(Webs *wp, char *path, char *query)
{
    char  *userid, *pass1, *pass2, *group, *enabled, *ok;
    bool_t bDisable;
    int nCheck;

    gassert(wp);

    userid = websGetVar(wp, T("user"), T("")); 
    pass1 = websGetVar(wp, T("password"), T("")); 
    pass2 = websGetVar(wp, T("passconf"), T("")); 
    group = websGetVar(wp, T("group"), T("")); 
    enabled = websGetVar(wp, T("enabled"), T("")); 
    ok = websGetVar(wp, T("ok"), T("")); 

    websHeader(wp);
    websWrite(wp, MSG_START);

    if (scaselesscmp(ok, T("ok")) != 0) {
        websWrite(wp, T("Add User Cancelled"));
    } else if (strcmp(pass1, pass2) != 0) {
        websWrite(wp, T("Confirmation Password did not match."));
    } else {
        if (enabled && *enabled && (strcmp(enabled, T("on")) == 0)) {
            bDisable = 0;
        } else {
            bDisable = 1;
        }
        nCheck = umAddUser(userid, pass1, group, 0, bDisable);
        if (nCheck != 0) {
            char * strError;

            switch (nCheck) {
            case UM_ERR_DUPLICATE:
                strError = T("User already exists.");
                break;

            case UM_ERR_BAD_NAME:
                strError = T("Invalid user name.");
                break;

            case UM_ERR_BAD_PASSWORD:
                strError = T("Invalid password.");
                break;

            case UM_ERR_NOT_FOUND:
                strError = T("Invalid or unselected group.");
                break;

            default:
                strError = T("Error writing user record.");
                break;
            }
            websWrite(wp, T("Unable to add user, \"%s\".  %s"), userid, strError);
        } else {
            websWrite(wp, T("User, \"%s\" was successfully added."), userid);
        }
        if (umSave() < 0) {
            websWrite(wp, T("Unable to save user database"));
        }
    }
    websWrite(wp, MSG_END);
    websFooter(wp);
    websDone(wp, 200);
}


static void formDeleteUser(Webs *wp, char *path, char *query)
{
    char  *userid, *ok;

    gassert(wp);

    userid = websGetVar(wp, T("user"), T("")); 
    ok = websGetVar(wp, T("ok"), T("")); 

    websHeader(wp);
    websWrite(wp, MSG_START);

    if (scaselesscmp(ok, T("ok")) != 0) {
        websWrite(wp, T("Delete User Cancelled"));
    } else if (umUserExists(userid) == 0) {
        websWrite(wp, T("ERROR: User \"%s\" not found"), userid);
    } else if (umGetUserProtected(userid)) {
        websWrite(wp, T("ERROR: User, \"%s\" is delete-protected."), userid);
    } else if (umDeleteUser(userid) != 0) {
        websWrite(wp, T("ERROR: Unable to delete user, \"%s\" "), userid);
    } else {
        websWrite(wp, T("User, \"%s\" was successfully deleted."), userid);
    }
    if (umSave() < 0) {
        websWrite(wp, T("Unable to save user database"));
    }
    websWrite(wp, MSG_END);
    websFooter(wp);
    websDone(wp, 200);
}


static void formDisplayUser(Webs *wp, char *path, char *query)
{
    char  *userid, *ok, *temp;
    bool_t  enabled;

    gassert(wp);

    userid = websGetVar(wp, T("user"), T("")); 
    ok = websGetVar(wp, T("ok"), T("")); 

    websHeader(wp);
    websWrite(wp, T("<body>"));

    if (scaselesscmp(ok, T("ok")) != 0) {
        websWrite(wp, T("Display User Cancelled"));
    } else if (umUserExists(userid) == 0) {
        websWrite(wp, T("ERROR: User <b>%s</b> not found.\n"), userid);
    } else {
        websWrite(wp, T("<h2>User ID: <b>%s</b></h2>\n"), userid);
        temp = umGetUserGroup(userid);
        websWrite(wp, T("<h3>User Group: <b>%s</b></h3>\n"), temp);
        enabled = umGetUserEnabled(userid);
        websWrite(wp, T("<h3>Enabled: <b>%d</b></h3>\n"), enabled);
    }
    if (umSave() < 0) {
        websWrite(wp, T("Unable to save user database"));
    }
    websWrite(wp, T("</body>\n"));
    websFooter(wp);
    websDone(wp, 200);
}


static int jsGenerateUserList(int eid, Webs *wp, int argc, char **argv)
{
    char  *userid;
    ssize   nBytes, nBytesSent;
    int     row;

    gassert(wp);

    nBytes = websWrite(wp, T("<SELECT NAME=\"user\" SIZE=\"3\" TITLE=\"Select a User\">"));
    row = 0;
    userid = umGetFirstUser();
    nBytesSent = 0;

    while (userid && (nBytes > 0)) {
        nBytes = websWrite(wp, T("<OPTION VALUE=\"%s\">%s\n"), userid, userid);
        userid = umGetNextUser(userid);
        nBytesSent += nBytes;
    }
    nBytesSent += websWrite(wp, T("</SELECT>"));
    return (int) nBytesSent;
}


static void formAddGroup(Webs *wp, char *path, char *query)
{
    char          *group, *enabled, *privilege, *method, *ok, *pChar;
    int             nCheck;
    short           priv;
    accessMeth_t    am;
    bool_t          bDisable;

    gassert(wp);

    group = websGetVar(wp, T("group"), T("")); 
    method = websGetVar(wp, T("method"), T("")); 
    enabled = websGetVar(wp, T("enabled"), T("")); 
    privilege = websGetVar(wp, T("privilege"), T("")); 
    ok = websGetVar(wp, T("ok"), T("")); 

    websHeader(wp);
    websWrite(wp, MSG_START);

    if (scaselesscmp(ok, T("ok")) != 0) {
        websWrite(wp, T("Add Group Cancelled."));
    } else if ((group == NULL) || (*group == 0)) {
        websWrite(wp, T("No Group Name was entered."));
    } else if (umGroupExists(group)) {
        websWrite(wp, T("ERROR: Group, \"%s\" already exists."), group);
    } else {
        if (privilege && *privilege) {
            /*
                privilege is a mulitple <SELECT> var, and must be parsed. Values for these variables are space delimited.
             */
            priv = 0;
            for (pChar = privilege; *pChar; pChar++) {
                if (*pChar == ' ') {
                    *pChar = '\0';
                    priv |= atoi(privilege);
                    *pChar = ' ';
                    privilege = pChar + 1;
                }
            }
            priv |= atoi(privilege);
        } else {
            priv = 0;
        }
        if (method && *method) {
            am = (accessMeth_t) atoi(method);
        } else {
            am = AM_FULL;
        }
        if (enabled && *enabled && (strcmp(enabled, T("on")) == 0)) {
            bDisable = 0;
        } else {
            bDisable = 1;
        }
        nCheck = umAddGroup(group, priv, am, 0, bDisable);
        if (nCheck != 0) {
            websWrite(wp, T("Unable to add group, \"%s\", code: %d "), group, nCheck);
        } else {
            websWrite(wp, T("Group, \"%s\" was successfully added."), group);
        }
        if (umSave() < 0) {
            websWrite(wp, T("Unable to save user database"));
        }
    }
    websWrite(wp, MSG_END);
    websFooter(wp);
    websDone(wp, 200);
}


static void formDeleteGroup(Webs *wp, char *path, char *query)
{
    char  *group, *ok;

    gassert(wp);

    group = websGetVar(wp, T("group"), T("")); 
    ok = websGetVar(wp, T("ok"), T("")); 

    websHeader(wp);
    websWrite(wp, MSG_START);

    if (scaselesscmp(ok, T("ok")) != 0) {
        websWrite(wp, T("Delete Group Cancelled."));
    } else if ((group == NULL) || (*group == '\0')) {
        websWrite(wp, T("ERROR: No group was selected."));
    } else if (umGetGroupProtected(group)) {
        websWrite(wp, T("ERROR: Group, \"%s\" is delete-protected."), group);
    } else if (umGetGroupInUse(group)) {
        websWrite(wp, T("ERROR: Group, \"%s\" is being used."), group);
    } else if (umDeleteGroup(group) != 0) {
        websWrite(wp, T("ERROR: Unable to delete group, \"%s\" "), group);
    } else {
        websWrite(wp, T("Group, \"%s\" was successfully deleted."), group);
    }
    if (umSave() < 0) {
        websWrite(wp, T("Unable to save user database"));
    }
    websWrite(wp, MSG_END);
    websFooter(wp);
    websDone(wp, 200);
}


static int jsGenerateGroupList(int eid, Webs *wp, int argc, char **argv)
{
    char  *group;
    ssize   nBytesSent, nBytes;
    int     row;

    gassert(wp);

    row = 0;
    nBytesSent = 0;
    nBytes = websWrite(wp, T("<SELECT NAME=\"group\" SIZE=\"3\" TITLE=\"Select a Group\">"));

    /*
     r  Add a special "<NONE>" element to allow de-selection
     */
    nBytes = websWrite(wp, T("<OPTION VALUE=\"\">[NONE]\n"));

    group = umGetFirstGroup();
    while (group && (nBytes > 0)) {
        nBytes = websWrite(wp, T("<OPTION VALUE=\"%s\">%s\n"), group, group);
        group = umGetNextGroup(group);
        nBytesSent += nBytes;
    }
    nBytesSent += websWrite(wp, T("</SELECT>"));
    return (int) nBytesSent;
}


static void formAddAccessLimit(Webs *wp, char *path, char *query)
{
    char          *url, *method, *group, *secure, *ok;
    accessMeth_t    am;
    int             nCheck;
    //  MOB - change type
    short           nSecure;

    gassert(wp);

    url = websGetVar(wp, T("url"), T("")); 
    group = websGetVar(wp, T("group"), T("")); 
    method = websGetVar(wp, T("method"), T("")); 
    secure = websGetVar(wp, T("secure"), T("")); 
    ok = websGetVar(wp, T("ok"), T("")); 

    websHeader(wp);
    websWrite(wp, MSG_START);

    if (scaselesscmp(ok, T("ok")) != 0) {
        websWrite(wp, T("Add Access Limit Cancelled."));
    } else if ((url == NULL) || (*url == 0)) {
        websWrite(wp, T("ERROR:  No URL was entered."));
    } else if (umAccessLimitExists(url)) {
        websWrite(wp, T("ERROR:  An Access Limit for [%s] already exists."), url);
    } else {
        if (method && *method) {
            am = (accessMeth_t) atoi(method);
        } else {
            am = AM_FULL;
        }
        if (secure && *secure) {
            nSecure = (short) atoi(secure);
        } else {
            nSecure = 0;
        }
        nCheck = umAddAccessLimit(url, am, nSecure, group);
        if (nCheck != 0) {
            websWrite(wp, T("Unable to add Access Limit for [%s]"), url);
        } else {
            websWrite(wp, T("Access limit for [%s], was successfully added."), url);
        }
        if (umSave() < 0) {
            websWrite(wp, T("Unable to save user database"));
        }
    }
    websWrite(wp, MSG_END);
    websFooter(wp);
    websDone(wp, 200);
}


static void formDeleteAccessLimit(Webs *wp, char *path, char *query)
{
    char  *url, *ok;

    gassert(wp);

    url = websGetVar(wp, T("url"), T("")); 
    ok = websGetVar(wp, T("ok"), T("")); 

    websHeader(wp);
    websWrite(wp, MSG_START);

    if (scaselesscmp(ok, T("ok")) != 0) {
        websWrite(wp, T("Delete Access Limit Cancelled"));
    } else if (umDeleteAccessLimit(url) != 0) {
        websWrite(wp, T("ERROR: Unable to delete Access Limit for [%s]"), url);
    } else {
        websWrite(wp, T("Access Limit for [%s], was successfully deleted."), url);
    }
    if (umSave() < 0) {
        websWrite(wp, T("Unable to save user database"));
    }
    websWrite(wp, MSG_END);
    websFooter(wp);
    websDone(wp, 200);
}


static int jsGenerateAccessLimitList(int eid, Webs *wp, int argc, char **argv)
{
    char  *url;
    ssize   nBytesSent, nBytes;
    int     row;

    gassert(wp);

    row = nBytesSent = 0;
    url = umGetFirstAccessLimit();
    nBytes = websWrite(wp, T("<SELECT NAME=\"url\" SIZE=\"3\" TITLE=\"Select a URL\">"));

    while (url && (nBytes > 0)) {
        nBytes = websWrite(wp, T("<OPTION VALUE=\"%s\">%s\n"), url, url);
        url = umGetNextAccessLimit(url);
        nBytesSent += nBytes;
    }
    nBytesSent += websWrite(wp, T("</SELECT>"));
    return (int) nBytesSent;
}


static int jsGenerateAccessMethodList(int eid, Webs *wp, int argc, char **argv)
{
    ssize     nBytes;

    gassert(wp);
    nBytes = websWrite(wp, T("<SELECT NAME=\"method\" SIZE=\"3\" TITLE=\"Select a Method\">"));
    nBytes += websWrite(wp, T("<OPTION VALUE=\"%d\">FULL ACCESS\n"), AM_FULL);
    nBytes += websWrite(wp, T("<OPTION VALUE=\"%d\">BASIC ACCESS\n"), AM_BASIC);
    nBytes += websWrite(wp, T("<OPTION VALUE=\"%d\" SELECTED>DIGEST ACCESS\n"), AM_DIGEST);
    nBytes += websWrite(wp, T("<OPTION VALUE=\"%d\">NO ACCESS\n"), AM_NONE);
    nBytes += websWrite(wp, T("</SELECT>")); 
    return (int) nBytes;
}


static int jsGeneratePrivilegeList(int eid, Webs *wp, int argc, char **argv)
{
    ssize     nBytes;

    gassert(wp);
    nBytes = websWrite(wp, T("<SELECT NAME=\"privilege\" SIZE=\"3\" "));
    nBytes += websWrite(wp, T("MULTIPLE TITLE=\"Choose Privileges\">"));
    nBytes += websWrite(wp, T("<OPTION VALUE=\"%d\">READ\n"), PRIV_READ);
    nBytes += websWrite(wp, T("<OPTION VALUE=\"%d\">EXECUTE\n"), PRIV_WRITE);
    nBytes += websWrite(wp, T("<OPTION VALUE=\"%d\">ADMINISTRATE\n"), PRIV_ADMIN);
    nBytes += websWrite(wp, T("</SELECT>"));
    return (int) nBytes;
}


#if UNUSED
static void formSaveUserManagement(Webs *wp, char *path, char *query)
{
    char  *ok;

    gassert(wp);
    ok = websGetVar(wp, T("ok"), T("")); 
    websHeader(wp);
    websWrite(wp, MSG_START);

    if (scaselesscmp(ok, T("ok")) != 0) {
        websWrite(wp, T("Save Cancelled."));
//  MOB
    } else if (umSave() != 0) {
        websWrite(wp, T("ERROR: Unable to save user configuration."));
    } else {
        websWrite(wp, T("User configuration was saved successfully."));
    }
    websWrite(wp, MSG_END);
    websFooter(wp);
    websDone(wp, 200);
}


static void formLoadUserManagement(Webs *wp, char *path, char *query)
{
    char  *ok;

    gassert(wp);
    ok = websGetVar(wp, T("ok"), T("")); 
    websHeader(wp);
    websWrite(wp, MSG_START);
    if (scaselesscmp(ok, T("ok")) != 0) {
        websWrite(wp, T("Load Cancelled."));
    } else if (umRestore(NULL) != 0) {
        websWrite(wp, T("ERROR: Unable to load user configuration."));
    } else {
        websWrite(wp, T("User configuration was re-loaded successfully."));
    }
    websWrite(wp, MSG_END);
    websFooter(wp);
    websDone(wp, 200);
}
#endif

#endif /* BIT_USER_MANAGEMENT_GUI */

#endif /* BIT_USER_MANAGEMENT */

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
