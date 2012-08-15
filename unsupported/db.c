/*
    db.c -- Simple text file database. Supports one open database at a time.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

#if BIT_DATABASE
/********************************* Defines ************************************/

#define KEYWORD_TABLE   T("TABLE")
#define KEYWORD_ROW     T("ROW")

/*
   galloc chain list of table schemas to be closed 
 */
static int          dbMaxTables = 0;
static dbTable_t    **dbListTables = NULL;

/********************************** Forwards **********************************/

static int      crack(char_t *buf, char_t **key, char_t **val);
static char_t   *trim(char_t *str);
static int      getColumnIndex(int tid, char_t *colName);

/******************************************************************************/

//  MOB - filename is unused?
//  MOB - gettime is unused
//  MOB - tablename not used
int dbOpen(char_t *tablename, char_t *filename, int (*gettime)(int did), int flags)
{
#if UNUSED
    //  MOB - this seems to prevent multiple tables being opened?
    dbMaxTables = 0;
    dbListTables = NULL;
    //  MOB - not returning did
#endif
    return 0;
}


/*
    Delete all the rows of the tables, and all of the tables
 */
void dbClose(int did)
{
    int         table, column;
    dbTable_t   *pTable;

    /*
        Before doing anything, delete all the contents of the database
     */
    dbZero(did);

    /*
        Now delete the tables
     */
    for (table = 0; table < dbMaxTables; table++) {
        pTable = dbListTables[table];

        if (pTable != NULL) {
            /*
                Delete the table schema
             */
            if (pTable->nColumns) {
                for (column = 0; column < pTable->nColumns; column++) {
                    gfree(pTable->columnNames[column]);
                }
                gfree(pTable->columnNames);
                gfree(pTable->columnTypes);
            }
            /*
                Delete the table name
             */
            gfree(pTable->name);
            /*
                Free the table
             */
            gfree(pTable);
            gfree((void ***) &dbListTables, table);
        }
    }

    if (dbListTables) {
        gfree(dbListTables);
    }

    /*
        Set the global table list to a safe value
     */
    dbListTables = NULL;
    dbMaxTables = 0;
}


/*
    Delete all the data records in all tables
 */
void dbZero(int did)
{
    dbTable_t   *pTable;
    ssize       *pRow;
    int         table, row, column, nRows, nColumns;

    /*
        Delete all data from all tables
     */
    for (table = 0; table < dbMaxTables; table++) {
        pTable = dbListTables[table];
        /*
                Delete the row data contained within the schema
         */
        if (pTable) {
            nColumns = pTable->nColumns;
            nRows = pTable->nRows;
            for (row = 0; row < nRows; row++) {
                pRow = pTable->rows[row];
                if (pRow) {
                    /*
                        Only delete the contents of rows not previously deleted!
                     */
                    for (column = 0; column < nColumns; column++) {
                        if (pTable->columnTypes[column] == T_STRING) {
                            gfree((char_t*) pRow[column]);
                            pRow[column] = 0;
                        }
                    }
                    gfree(pRow);
                    gfree((void ***) &pTable->rows, row);
                }
            }
            pTable->rows = NULL;
            pTable->nRows = 0;
        }
    }
}


/* 
    Add a schema to the module-internal schema database 
    MOB - fix hungarian naming
 */
int dbRegisterDBSchema(dbTable_t *pTableRegister)
{
    dbTable_t   *pTable;
    int         tid;

    gassert(pTableRegister);

    trace(4, T("DB: Registering database table <%s>"), pTableRegister->name);

    /*
        Bump up the size of the table array
     */
    tid = gallocEntry((void***) &dbListTables, &dbMaxTables, sizeof(dbTable_t));    

    /*
        Copy the table schema to the last spot in schema array
     */
    gassert(dbListTables);
    pTable = dbListTables[tid];
    gassert(pTable);

    /*
        Copy the name of the table and column
     */
    pTable->name = gstrdup(pTableRegister->name);
    pTable->nColumns = pTableRegister->nColumns;

    /*
        Copy the column definitions
     */
    if (pTable->nColumns > 0) {
        int i;
        pTable->columnNames = galloc(sizeof(char_t *) * pTable->nColumns);
        pTable->columnTypes = galloc(sizeof(int *) * pTable->nColumns);

        for (i = 0; (i < pTableRegister->nColumns); i++) {
            pTable->columnNames[i] = gstrdup(pTableRegister->columnNames[i]);
            pTable->columnTypes[i] = pTableRegister->columnTypes[i];
        }

    } else {
        pTable->columnNames = NULL;
        pTable->columnTypes = NULL;
    }
    /*
        Zero out the table's data (very important!)
     */
    pTable->nRows = 0;
    pTable->rows = NULL;
    return 0;
}


/*
    Find the a row in the table with the given string in the given column
 */
int dbSearchStr(int did, char_t *tablename, char_t *colName, char_t *value, int flags)
{
    dbTable_t   *pTable;
    char_t      *compareVal;
    ssize       *pRow;
    int         row, tid, nRows, column, match;

    gassert(tablename);
    gassert(colName);
    gassert(value);

    tid = dbGetTableId(0, tablename);
    if ((tid >= 0) && (tid < dbMaxTables) && (dbListTables[tid] != NULL)) {
        pTable = dbListTables[tid];
    } else {
        return DB_ERR_TABLE_NOT_FOUND;
    }
    nRows = pTable->nRows;
    column = getColumnIndex(tid, colName);
    gassert (column >= 0);

    if (column >= 0) {
        /*
            Scan through rows until we find a match. Note that some of these rows may be deleted!
         */
        row = 0;
        match = 0;

        while (row < nRows) {
            pRow = pTable->rows[row];
            if (pRow) {
                compareVal = (char_t*) (pRow[column]); 
                if (NULL != compareVal) {
                    if (DB_CASE_INSENSITIVE == flags) {
                        match = gcaselesscmp(compareVal, value);
                    } else {
                        match = gstrcmp(compareVal, value);
                    }
                    if (0 == match) {
                        return row;
                    }
                }
            }
            row++;
        }
    } else { 
        /*
            Return -2 if search column was not found
         */
        trace(3, T("DB: Unable to find column <%s> in table <%s>"), colName, tablename);
        return DB_ERR_COL_NOT_FOUND;
    }
    return -1;
}


/*
    Add a new row to the given table.  Return the new row ID.
 */
int dbAddRow(int did, char_t *tablename)
{
    int         tid, size;
    dbTable_t   *pTable;

    gassert(tablename);

    tid = dbGetTableId(0, tablename);
    gassert(tid >= 0);

    if ((tid >= 0) && (tid < dbMaxTables) && (dbListTables[tid] != NULL)) {
        pTable = dbListTables[tid];
    } else {
        return DB_ERR_TABLE_NOT_FOUND;
    }
    gassert(pTable);

    if (pTable) {
        trace(5, T("DB: Adding a row to table <%s>"), tablename);
        size = pTable->nColumns * sizeof(ssize);
        return gallocEntry((void***) &(pTable->rows), &(pTable->nRows), size);
    } 
    return -1;
}


/*
    Delete a row in the table.  
 */
int dbDeleteRow(int did, char_t *tablename, int row)
{
    dbTable_t   *pTable;
    ssize       *pRow;
    int         tid, nColumns, nRows;

    gassert(tablename);
    tid = dbGetTableId(0, tablename);
    gassert(tid >= 0);

    if ((tid >= 0) && (tid < dbMaxTables) && (dbListTables[tid] != NULL)) {
        pTable = dbListTables[tid];
    } else {
        return DB_ERR_TABLE_NOT_FOUND;
    }
    nColumns = pTable->nColumns;
    nRows = pTable->nRows;

    if ((row >= 0) && (row < nRows)) {
        pRow = pTable->rows[row];
        if (pRow) {
            int column = 0;
            /*
                Free up any allocated strings
             */
            while (column < nColumns) {
                if (pRow[column] && (pTable->columnTypes[column] == T_STRING)) {
                    gfree((char_t*) pRow[column]);
                }
                column++;
            }
            gfree(pRow);
            pTable->nRows = gfreeHandle((void***)&pTable->rows, row);
            trace(5, T("DB: Deleted row <%d> from table <%s>"), row, tablename);
        }
        return 0;
    } else {
        trace(3, T("DB: Unable to delete row <%d> from table <%s>"), row, tablename);
    }
    return -1;
}


/*
    Grow the rows in the table to the nominated size. 
 */
int dbSetTableNrow(int did, char_t *tablename, int nNewRows)
{
    int         nRet, tid, nRows;
    dbTable_t   *pTable;

    gassert(tablename);
    tid = dbGetTableId(0, tablename);
    gassert(tid >= 0) ;

    if ((tid >= 0) && (tid < dbMaxTables) && (dbListTables[tid] != NULL)) {
        pTable = dbListTables[tid];
    } else {
        return DB_ERR_TABLE_NOT_FOUND;
    }
    nRet = -1;

    gassert(pTable);
    if (pTable) {
        nRows = pTable->nRows;
        nRet = 0;
        if (nRows >= nNewRows) {
            /*      
                If number of rows already allocated exceeds requested number, do nothing
             */
            trace(4, T("DB: Ignoring row set to <%d> in table <%s>"), nNewRows, tablename);
        } else {
            trace(4, T("DB: Setting rows to <%d> in table <%s>"), nNewRows, tablename);
            while (pTable->nRows < nNewRows) {
                if (dbAddRow(did, tablename) < 0) {
                    return -1;
                }
            }
        }
    } 
    return nRet;
}


/*
    Return the number of rows in the given table
 */
int dbGetTableNrow(int did, char_t *tablename)
{
    int tid;
    
    gassert(tablename);
    tid = dbGetTableId(did, tablename);

    if ((tid >= 0) && (tid < dbMaxTables) && (dbListTables[tid] != NULL)) {
        return (dbListTables[tid])->nRows;
    } else {
        return -1;
    }
}


static int dbRead(int did, char_t *table, char_t *column, int row, ssize *returnValue)
{
    dbTable_t   *pTable;
    ssize       *pRow;
    int         colIndex, tid;
    
    gassert(table);
    gassert(column);
    gassert(returnValue);

    tid = dbGetTableId(0, table);
    gassert(tid >= 0);

    if (tid < 0) {
        return DB_ERR_TABLE_NOT_FOUND;
    }
    pTable = dbListTables[tid];
    if (pTable == NULL) {
        return DB_ERR_TABLE_DELETED;
    }
    gassert(row >= 0);

    if ((row >= 0) && (row < pTable->nRows)) {
        colIndex = getColumnIndex(tid, column);
        gassert(colIndex >= 0);
        if (colIndex >= 0) {
            pRow = pTable->rows[row];
            if (pRow) {
                *returnValue = pRow[colIndex];
                return 0;
            }  
            return DB_ERR_ROW_DELETED;
        }
        return DB_ERR_COL_NOT_FOUND;
    }
    return DB_ERR_ROW_NOT_FOUND;
}


int dbReadInt(int did, char_t *table, char_t *column, int row, int *returnValue)
{
    ssize   value;
    int     rc;

    if ((rc = dbRead(did, table, column, row, &value)) < 0) {
        return rc;
    }
    *returnValue = (int) value;
    return 0;
}


int dbReadStr(int did, char_t *table, char_t *column, int row, char_t **returnValue)
{
    return dbRead(did, table, column, row, (ssize*) returnValue);
}


/*
    The dbWriteInt function writes a value into a table at a given row and column.  The existence of the row and column
    is verified before the write.  0 is returned on succes, -1 is returned on error.
 */
int dbWriteInt(int did, char_t *table, char_t *column, int row, int iData)
{
    dbTable_t   *pTable;
    ssize       *pRow;
    int         tid, colIndex;

    gassert(table);
    gassert(column);

    if ((tid = dbGetTableId(0, table)) < 0) {
        return DB_ERR_TABLE_NOT_FOUND;
    }
    pTable = dbListTables[tid];
    if (pTable) {
        /*
            Make sure that the column exists
         */
        colIndex = getColumnIndex(tid, column);
        gassert(colIndex >= 0);
        if (colIndex >= 0) {
            /*
                Make sure that the row exists
             */
            gassert((row >= 0) && (row < pTable->nRows));
            if ((row >= 0) && (row < pTable->nRows)) {
                pRow = pTable->rows[row];
                if (pRow) {
                    pRow[colIndex] = iData;
                    return 0;
                }
                return DB_ERR_ROW_DELETED;
            }
            return DB_ERR_ROW_NOT_FOUND;
        }
        return DB_ERR_COL_NOT_FOUND;
    }
    return DB_ERR_TABLE_DELETED;
}


/*
    The dbWriteStr function writes a string value into a table at a given row and column.  The existence of the row and
    column is verified before the write.  The column is also checked to confirm it is a string field.  0 is returned on
    succes, -1 is returned on error.
 */
int dbWriteStr(int did, char_t *table, char_t *column, int row, char_t *s)
{
    dbTable_t   *pTable;
    ssize       *pRow;
    int         tid, colIndex;

    gassert(table);
    gassert(column);

    if ((tid = dbGetTableId(0, table)) < 0) {
        return DB_ERR_TABLE_NOT_FOUND;
    }
    /*
        Make sure that this table exists
     */
    pTable = dbListTables[tid];
    gassert(pTable);
    if (!pTable) {
        return DB_ERR_TABLE_DELETED;
    }
    /*
        Make sure that this column exists
     */
    colIndex = getColumnIndex(tid, column);
    if (colIndex < 0) {
        return DB_ERR_COL_NOT_FOUND;
    }

    /*
        Make sure that this column is a string column
     */
    if (pTable->columnTypes[colIndex] != T_STRING) {
        return DB_ERR_BAD_FORMAT;
    }
    /*
        Make sure that the row exists
     */
    gassert((row >= 0) && (row < pTable->nRows));
    if ((row >= 0) && (row < pTable->nRows)) {
        pRow = pTable->rows[row];
    } else {
        return DB_ERR_ROW_NOT_FOUND;
    }
    if (!pRow) {
        return DB_ERR_ROW_DELETED;
    }

    /*
        If the column already has a value, be sure to delete it to prevent memory leaks.
     */
    if (pRow[colIndex]) {
        gfree((char*) pRow[colIndex]);
    }

    /*
        Make sure we make a copy of the string to write into the column. This allocated string will be deleted when the row is deleted.  
     */
    pRow[colIndex] = (ssize) gstrdup(s);
    return 0;
}


/*
    Print a key-value pair to a file
 */
static ssize dbWriteKeyValue(int fd, char_t *key, char_t *value)
{
    char_t  *pLineOut;
    ssize   len, rc;

    gassert(key && *key);
    gassert(value);
    
    gfmtAlloc(&pLineOut, -1, T("%s=%s\n"), key, value);

    if (pLineOut) {
        len = gstrlen(pLineOut);
        //MOB
#if CE
        rc = gwriteUniToAsc(fd, pLineOut, len);
#else
        rc = gwrite(fd, pLineOut, len);
#endif
        gfree(pLineOut);
    } else {
        rc = -1;
    }
    return rc;
}


//  MOB - then why supply a filename on open?
//  MOB - did is unused
int dbSave(int did, char_t *filename, int flags)
{
    dbTable_t   *pTable;
    char_t      *tmpFile, *tmpNum, **colNames;
    ssize       rc, *pRow;
    int         row, column, nColumns, nRows, fd, *colTypes, nRet, tid;

    trace(5, T("DB: About to save database to file"));

    gassert(dbMaxTables > 0);

    /*
        First write to a temporary file, then switch around later.
     */
    tmpFile = T("data.tmp");
    if ((fd = gopen(tmpFile, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0666)) < 0) {
        trace(1, T("WARNING: Failed to open file %s"), tmpFile);
        return -1;
    }
    nRet = 0;
    for (tid = 0; (tid < dbMaxTables) && (nRet != -1); tid++) {
        pTable = dbListTables[tid];

        if (pTable) {
            /*
                Print the TABLE=tableName directive to the file
             */
            rc = dbWriteKeyValue(fd, KEYWORD_TABLE, pTable->name);

            nColumns = pTable->nColumns;
            nRows = pTable->nRows;

            for (row = 0; (row < nRows) && (nRet == 0); row++) {
                pRow = pTable->rows[row];
                /*
                    If row is NULL, the row has been deleted, so don't write it out.
                 */
                if ((pRow == NULL) || (pRow[0] == '\0') || (*(char_t*)(pRow[0]) == '\0')) {
                    continue;
                }
                /*
                    Print the ROW=rowNumber directive to the file
                 */
                gfmtAlloc(&tmpNum, 20, T("%d"), row);        
                rc = dbWriteKeyValue(fd, KEYWORD_ROW, tmpNum);
                gfree(tmpNum);

                colNames = pTable->columnNames;
                colTypes = pTable->columnTypes;
                /*
                    Print the key-value pairs (COLUMN=value) for data cells
                 */
                for (column = 0; (column < nColumns) && (rc >= 0); 
                    column++, colNames++, colTypes++) {
                    if (*colTypes == T_STRING) {
                        rc = dbWriteKeyValue(fd, *colNames, (char_t*)(pRow[column]));
                    } else {
                        //  MOB - need support for '%Ld'
                        gfmtAlloc(&tmpNum, 20, T("%d"), (int) pRow[column]);       
                        rc = dbWriteKeyValue(fd, *colNames, tmpNum);
                        gfree(tmpNum);
                    }
                }
                if (rc < 0) {
                    trace(1, T("WARNING: Failed to write to file %s"), tmpFile);
                    nRet = -1;
                }
            }
        }
    }
    gclose(fd);

    /*
        Replace the existing file with the temporary file, if no errors
     */
    if (nRet == 0) {
        gunlink(filename);
        if (grename(tmpFile, filename) != 0) {
            trace(1, T("WARNING: Failed to rename %s to %s"), tmpFile, filename);
            nRet = -1;
        }
    }
    return nRet;
}


/*
    Crack a keyword=value string into keyword and value. We can change buf.
 */
static int crack(char_t *buf, char_t **key, char_t **val)
{
    char_t  *ptr;

    if ((ptr = gstrrchr(buf, '\n')) != NULL ||
            (ptr = gstrrchr(buf, '\r')) != NULL) {
        *ptr = '\0';
    }
    /*
        Find the = sign. It must exist.
     */
    if ((ptr = gstrstr(buf, T("="))) == NULL) {
        return -1;
    }
    *ptr++ = '\0';
    *key = trim(buf);
    *val = trim(ptr);
    return 0;
}


/*
    Parse the file. These files consist of key-value pairs, separated by the "=" sign. Parsing of tables starts with the
    "TABLE=value" pair, and rows are parsed starting with the "ROW=value" pair.
 */
int dbLoad(int did, char_t *filename, int flags)
{
    WebsStat   sbuf;
    char_t      *buf, *keyword, *value, *ptr;
    char_t      *tablename;
    int         fd, tid, row;
    dbTable_t   *pTable;

    gassert(did >= 0);

    trace(4, T("DB: About to read data file <%s>"), filename);

    if (gstat(filename, &sbuf) < 0) {
        error(T("DB: Failed to stat persistent data file"));
        return -1;
    }
    fd = gopen(filename, O_RDONLY | O_BINARY, 0666);
    if (fd < 0) {
        trace(3, T("DB: No persistent data file present."));
        return -1;
    }
    if (sbuf.st_size <= 0) {
        trace(3, T("DB: Persistent data file is empty."));
        gclose(fd);
        return -1;
    }
    /*
        Read entire file into temporary buffer
     */
    buf = galloc((ssize) sbuf.st_size + 1);

    //  MOB - unify
#if CE
    if (greadAscToUni(fd, &buf, sbuf.st_size) != (ssize) sbuf.st_size) {
#else
    if (gread(fd, buf, (ssize) sbuf.st_size) != (ssize) sbuf.st_size) {
#endif
        trace(3, T("DB: Persistent data read failed."));
        gfree(buf);
        gclose(fd);
        return -1;
    }
    gclose(fd);
    *(buf + sbuf.st_size) = '\0';

    row = -1;
    tid = -1;
    pTable = NULL;
    ptr = gstrtok(buf, T("\n"));
    tablename = NULL;

    do {
        if (crack(ptr, &keyword, &value) < 0) {
            trace(5, T("DB: Failed to crack line %s"), ptr);
            continue;
        }
        gassert(keyword && *keyword);

        if (gstrcmp(keyword, KEYWORD_TABLE) == 0) {
            /*
                Table name found, check to see if it's registered
             */
            if (tablename) {
                gfree(tablename);
            }
            tablename = gstrdup(value);
            tid = dbGetTableId(did, tablename);

            if (tid >= 0) {
                pTable = dbListTables[tid];
            } else {
                pTable = NULL;
            }

        } else if (gstrcmp(keyword, KEYWORD_ROW) == 0) {
            /*
                Row/Record indicator found, add a new row to table
             */
            if (tid >= 0) {
                int nRows = dbGetTableNrow(did, tablename);

                if (dbSetTableNrow(did, tablename, nRows + 1) == 0) {
                    row = nRows;
                }
            }

        } else if (row != -1) {
            /*
                Some other data found, assume it's a COLUMN=value
             */
            int nColumn = getColumnIndex(tid, keyword);

            if ((nColumn >= 0) && (pTable != NULL)) {
                int nColumnType = pTable->columnTypes[nColumn];
                if (nColumnType == T_STRING) {
                    dbWriteStr(did, tablename, keyword, row, value);
                } else {
                    dbWriteInt(did, tablename, keyword, row, gstrtoi(value));
                }
            }
        }
    } while ((ptr = gstrtok(NULL, T("\n"))) != NULL);

    gfree(tablename);
    gfree(buf);
    return 0;
}


int dbGetTableId(int did, char_t *tablename)
{
    int         tid;
    dbTable_t   *pTable;

    gassert(tablename);

    for (tid = 0; (tid < dbMaxTables); tid++) {
        if ((pTable = dbListTables[tid]) != NULL) {
            if (gstrcmp(tablename, pTable->name) == 0) {
                return tid;
            }
        }
    }
    return -1;
}


char_t *dbGetTableName(int did, int tid)
{
    if (tid >= 0 && tid < dbMaxTables && dbListTables[tid]) {
        return dbListTables[tid]->name;
    }
    return NULL;
}


static char_t *trim(char_t *str)
{
    while (isspace((int) *str)) {
        str++;
    }
    return str;
}


/*
    Return a column index given the column name
 */
static int getColumnIndex(int tid, char_t *colName) 
{
    int         column;
    dbTable_t   *pTable;

    gassert(colName);

    if ((tid >= 0) && (tid < dbMaxTables) && (dbListTables[tid] != NULL)) {
        pTable = dbListTables[tid];
        for (column = 0; (column < pTable->nColumns); column++) {
            if (gstrcmp(colName, pTable->columnNames[column]) == 0)
                return column;
        }
    }
    return -1;
}

#endif /* BIT_DATABASE */

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
