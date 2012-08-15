/******************************** User Management *****************************/

//  MOB - prefix on type name and AM_NAME
//  MOB - convert to defines with numbers
typedef enum {
    AM_NONE = 0,
    AM_FULL,
    AM_BASIC,
    AM_DIGEST,
    AM_INVALID
} accessMeth_t;

//  MOB - remove bool_t
typedef short bool_t;

#if BIT_USER_MANAGEMENT
/*
    Error Return Flags
 */
#define UM_OK               0
#define UM_ERR_GENERAL      -1
#define UM_ERR_NOT_FOUND    -2
#define UM_ERR_PROTECTED    -3
#define UM_ERR_DUPLICATE    -4
#define UM_ERR_IN_USE       -5
#define UM_ERR_BAD_NAME     -6
#define UM_ERR_BAD_PASSWORD -7

/*
    Privilege Masks
 */
//  MOB - prefix
#define PRIV_NONE   0x00
#define PRIV_READ   0x01
#define PRIV_WRITE  0x02
#define PRIV_ADMIN  0x04

extern int umOpen(char_t *path);
extern void umClose();
extern int umSave();

extern int umAddUser(char_t *user, char_t *password, char_t *group, bool_t protect, bool_t disabled);
extern int umDeleteUser(char_t *user);
extern char_t *umGetFirstUser();
extern char_t *umGetNextUser(char_t *lastUser);
extern bool_t umUserExists(char_t *user);
extern char_t *umGetUserPassword(char_t *user);
extern int umSetUserPassword(char_t *user, char_t *password);
extern char_t *umGetUserGroup(char_t *user);
extern int umSetUserGroup(char_t *user, char_t *password);
extern bool_t umGetUserEnabled(char_t *user);
extern int umSetUserEnabled(char_t *user, bool_t enabled);
extern bool_t umGetUserProtected(char_t *user);
extern int umSetUserProtected(char_t *user, bool_t protect);
extern int umAddGroup(char_t *group, short privilege, accessMeth_t am, bool_t protect, bool_t disabled);
extern int umDeleteGroup(char_t *group);
extern char_t *umGetFirstGroup();
extern char_t *umGetNextGroup(char_t *lastUser);
extern bool_t umGroupExists(char_t *group);
extern bool_t umGetGroupInUse(char_t *group);
extern accessMeth_t umGetGroupAccessMethod(char_t *group);
extern int umSetGroupAccessMethod(char_t *group, accessMeth_t am);
extern bool_t umGetGroupEnabled(char_t *group);
extern int umSetGroupEnabled(char_t *group, bool_t enabled);
extern short umGetGroupPrivilege(char_t *group);
extern int umSetGroupPrivilege(char_t *group, short privileges);
extern bool_t umGetGroupProtected(char_t *group);
extern int umSetGroupProtected(char_t *group, bool_t protect);
extern int umAddAccessLimit(char_t *url, accessMeth_t am, short secure, char_t *group);
extern int umDeleteAccessLimit(char_t *url);
extern char_t *umGetFirstAccessLimit();
extern char_t *umGetNextAccessLimit(char_t *lastUser);
extern char_t *umGetAccessLimit(char_t *url);
extern bool_t umAccessLimitExists(char_t *url);
extern accessMeth_t umGetAccessLimitMethod(char_t *url);
extern int umSetAccessLimitMethod(char_t *url, accessMeth_t am);
extern short umGetAccessLimitSecure(char_t *url);
extern int umSetAccessLimitSecure(char_t *url, short secure);
extern char_t *umGetAccessLimitGroup(char_t *url);
extern int umSetAccessLimitGroup(char_t *url, char_t *group);
extern accessMeth_t umGetAccessMethodForURL(char_t *url);
extern bool_t umUserCanAccessURL(char_t *user, char_t *url);
extern void formDefineUserMgmt(void);
