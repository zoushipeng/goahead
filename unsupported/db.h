/************************************* Database ********************************/

#if BIT_DATABASE
//  MOB - prefix
#define     T_INT                   0
#define     T_STRING                1
#define     DB_OK                   0
#define     DB_ERR_GENERAL          -1
#define     DB_ERR_COL_NOT_FOUND    -2
#define     DB_ERR_COL_DELETED      -3
#define     DB_ERR_ROW_NOT_FOUND    -4
#define     DB_ERR_ROW_DELETED      -5
#define     DB_ERR_TABLE_NOT_FOUND  -6
#define     DB_ERR_TABLE_DELETED    -7
#define     DB_ERR_BAD_FORMAT       -8
#define     DB_CASE_INSENSITIVE     1

//  MOB - remove _s 
typedef struct dbTable_s {
    char_t  *name;
    int     nColumns;
    char_t  **columnNames;
    int     *columnTypes;
    int     nRows;
    ssize   **rows;
} dbTable_t;

/*
    Add a schema to the module-internal schema database
    MOB - sort
 */
extern int      dbAddRow(int did, char_t *table);
extern void     dbClose(int did);
extern int      dbDeleteRow(int did, char_t *table, int rid);
//  MOB - revise args
extern int      dbOpen(char_t *databasename, char_t *filename, int (*gettime)(int did), int flags);
extern int      dbGetTableId(int did, char_t *tname);
extern char_t   *dbGetTableName(int did, int tid);
extern int      dbReadInt(int did, char_t *table, char_t *column, int row, int *returnValue);
extern int      dbReadStr(int did, char_t *table, char_t *column, int row, char_t **returnValue);
extern int      dbWriteInt(int did, char_t *table, char_t *column, int row, int idata);
extern int      dbWriteStr(int did, char_t *table, char_t *column, int row, char_t *s);
extern int      dbRegisterDBSchema(dbTable_t *sTable);
extern int      dbSetTableNrow(int did, char_t *table, int nNewRows);
extern int      dbGetTableNrow(int did, char_t *table);

/*
    Dump the contents of a database to file
 */
extern int      dbSave(int did, char_t *filename, int flags);

/*
    Load the contents of a database to file
 */
extern int      dbLoad(int did, char_t *filename, int flags);
extern int      dbSearchStr(int did, char_t *table, char_t *column, char_t *value, int flags);
extern void     dbZero(int did);

#endif /* BIT_DATABASE */

