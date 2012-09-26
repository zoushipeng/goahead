/* 
    js.h -- JavaScript header

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_JS
#define _h_JS 1

/********************************* Includes ***********************************/

#include    "goahead.h"

#if BIT_JAVASCRIPT
/********************************** Defines ***********************************/
/*
    Constants
 */
#define JS_INC              110     /* Growth for tags/tokens */
#define JS_SCRIPT_INC       1023    /* Growth for ej scripts */
#define JS_OFFSET           1       /* hAlloc doesn't like 0 entries */
#define JS_MAX_RECURSE      100     /* Sanity for maximum recursion */

/*
    Javascript Lexical analyser tokens
 */
#define TOK_ERR             -1      /* Any error */
#define TOK_LPAREN          1       /* ( */
#define TOK_RPAREN          2       /* ) */
#define TOK_IF              3       /* if */
#define TOK_ELSE            4       /* else */
#define TOK_LBRACE          5       /* { */
#define TOK_RBRACE          6       /* } */
#define TOK_LOGICAL         7       /* ||, &&, ! */
#define TOK_EXPR            8       /* +, -, /, % */
#define TOK_SEMI            9       /* ; */
#define TOK_LITERAL         10      /* literal string */
#define TOK_FUNCTION        11      /* function name */
#define TOK_NEWLINE         12      /* newline white space */
#define TOK_ID              13      /* function name */
#define TOK_EOF             14      /* End of script */
#define TOK_COMMA           15      /* Comma */
#define TOK_VAR             16      /* var */
#define TOK_ASSIGNMENT      17      /* = */
#define TOK_FOR             18      /* for */
#define TOK_INC_DEC         19      /* ++, -- */
#define TOK_RETURN          20      /* return */

/*
    Expression operators
 */
#define EXPR_LESS           1       /* < */
#define EXPR_LESSEQ         2       /* <= */
#define EXPR_GREATER        3       /* > */
#define EXPR_GREATEREQ      4       /* >= */
#define EXPR_EQ             5       /* == */
#define EXPR_NOTEQ          6       /* != */
#define EXPR_PLUS           7       /* + */
#define EXPR_MINUS          8       /* - */
#define EXPR_DIV            9       /* / */
#define EXPR_MOD            10      /* % */
#define EXPR_LSHIFT         11      /* << */
#define EXPR_RSHIFT         12      /* >> */
#define EXPR_MUL            13      /* * */
#define EXPR_ASSIGNMENT     14      /* = */
#define EXPR_INC            15      /* ++ */
#define EXPR_DEC            16      /* -- */
#define EXPR_BOOL_COMP      17      /* ! */
/*
    Conditional operators
 */
#define COND_AND            1       /* && */
#define COND_OR             2       /* || */
#define COND_NOT            3       /* ! */

/*
    States
 */
#define STATE_ERR               -1          /* Error state */
#define STATE_EOF               1           /* End of file */
#define STATE_COND              2           /* Parsing a "(conditional)" stmt */
#define STATE_COND_DONE         3
#define STATE_RELEXP            4           /* Parsing a relational expr */
#define STATE_RELEXP_DONE       5
#define STATE_EXPR              6           /* Parsing an expression */
#define STATE_EXPR_DONE         7
#define STATE_STMT              8           /* Parsing General statement */
#define STATE_STMT_DONE         9
#define STATE_STMT_BLOCK_DONE   10          /* End of block "}" */
#define STATE_ARG_LIST          11          /* Function arg list */
#define STATE_ARG_LIST_DONE     12
#define STATE_DEC_LIST          16          /* Declaration list */
#define STATE_DEC_LIST_DONE     17
#define STATE_DEC               18
#define STATE_DEC_DONE          19
#define STATE_RET               20          /* Return statement */

#define STATE_BEGIN             STATE_STMT

/*
    Flags. Used in Js and as parameter to parse()
 */
#define FLAGS_EXE               0x1             /* Execute statements */
#define FLAGS_VARIABLES         0x2             /* Allocated variables store */
#define FLAGS_FUNCTIONS         0x4             /* Allocated function store */

/*
    Function call structure
 */
typedef struct JsFun {
    char        *fname;                         /* Function name */
    char        **args;                         /* Args for function (halloc) */
    int         nArgs;                          /* Number of args */
} JsFun;

/*
    Evaluation block structure
 */
typedef struct JsInput {
    WebsBuf     tokbuf;                         /* Current token */
    WebsBuf     script;                         /* Input script for parsing */
    char        *putBackToken;                  /* Putback token string */
    int         putBackTokenId;                 /* Putback token ID */
    char        *line;                          /* Current line */
    int         lineLength;                     /* Current line length */
    int         lineNumber;                     /* Parse line number */
    int         lineColumn;                     /* Column in line */
} JsInput;


/*
    Per Javascript session structure
 */
typedef struct Js {
    JsInput     *input;                         /* Input evaluation block */
    WebsHash    functions;                      /* Symbol table for functions */
    WebsHash    *variables;                     /* hAlloc list of variables */
    int         variableMax;                    /* Number of entries */
    JsFun       *func;                          /* Current function */
    char        *result;                        /* Current expression result */
    char        *error;                         /* Error message */
    char        *token;                         /* Pointer to token string */
    int         tid;                            /* Current token id */
    int         eid;                            /* Halloc handle */
    int         flags;                          /* Flags */
    void        *userHandle;                    /* User defined handle */
} Js;

/******************************** Prototypes **********************************/

extern int      jsOpenBlock(int eid);
extern int      jsCloseBlock(int eid, int vid);
extern char     *jsEvalBlock(int eid, char *script, char **emsg);

#if !ECOS && UNUSED && KEEP
extern char     *jsEvalFile(int eid, char *path, char **emsg);
#endif

extern int      jsRemoveGlobalFunction(int eid, char *name);
extern void     *jsGetGlobalFunction(int eid, char *name);
extern int      jsSetGlobalFunctionDirect(WebsHash functions, char *name, 
                    int (*fn)(int eid, void *handle, int argc, char **argv));
extern void     jsError(Js *ep, char *fmt, ...);
extern void     jsSetUserHandle(int eid, void *handle);
extern void     *jsGetUserHandle(int eid);
extern int      jsGetLineNumber(int eid);
extern char     *jsGetResult(int eid);
extern void     jsSetLocalVar(int eid, char *var, char *value);
extern void     jsSetGlobalVar(int eid, char *var, char *value);

extern int      jsLexOpen(Js *ep);
extern void     jsLexClose(Js *ep);
extern int      jsLexOpenScript(Js *ep, char *script);
extern void     jsLexCloseScript(Js *ep);
extern void     jsLexSaveInputState(Js *ep, JsInput *state);
extern void     jsLexFreeInputState(Js *ep, JsInput *state);
extern void     jsLexRestoreInputState(Js *ep, JsInput *state);
extern int      jsLexGetToken(Js *ep, int state);
extern void     jsLexPutbackToken(Js *ep, int tid, char *string);

extern WebsHash jsGetVariableTable(int eid);
extern WebsHash jsGetFunctionTable(int eid);

extern int      jsArgs(int argc, char **argv, char *fmt, ...);
extern void     jsSetResult(int eid, char *s);
extern int      jsOpenEngine(WebsHash variables, WebsHash functions);
extern void     jsCloseEngine(int eid);
extern int      jsSetGlobalFunction(int eid, char *name, int (*fn)(int eid, void *handle, int argc, char **argv));
extern void     jsSetVar(int eid, char *var, char *value);
extern int      jsGetVar(int eid, char *var, char **value);
extern char     *jsEval(int eid, char *script, char **emsg);

#if BIT_LEGACY
    typedef Js ej_t;
    typedef JsInput jsinput_t;
    typedef JsFun jsfunc_t;

    #define ejOpenBlock jsOpenBlock
    #define ejCloseBlock jsCloseBlock
    #define ejEvalBlock jsEvalBlock
    #define ejRemoveGlobalFunction jsRemoveGlobalFunction
    #define ejGetGlobalFunction jsGetGlobalFunction
    #define ejSetGlobalFunctionDirect jsSetGlobalFunctionDirect
    #define ejError jsError
    #define ejSetUserHandle jsSetUserHandle
    #define ejGetUserHandle jsGetUserHandle
    #define ejGetLineNumber jsGetLineNumber
    #define ejGetResult jsGetResult
    #define ejSetLocalVar jsSetLocalVar
    #define ejSetGlobalVar jsSetGlobalVar
    #define ejLexOpen jsLexOpen
    #define ejLexClose jsLexClose
    #define ejLexOpenScript jsLexOpenScript
    #define ejLexCloseScript jsLexCloseScript
    #define ejLexSaveInputState jsLexSaveInputState
    #define ejLexFreeInputState jsLexFreeInputState
    #define ejLexRestoreInputState jsLexRestoreInputState
    #define ejLexGetToken jsLexGetToken
    #define ejLexPutbackToken jsLexPutbackToken
    #define ejGetVariableTable jsGetVariableTable
    #define ejGetFunctionTable jsGetFunctionTable
    #define ejArgs jsArgs
    #define ejSetResult jsSetResult
    #define ejOpenEngine jsOpenEngine
    #define ejCloseEngine jsCloseEngine
    #define ejSetGlobalFunction jsSetGlobalFunction
    #define ejSetVar jsSetVar
    #define ejGetVar jsGetVar
    #define ejEval jsEval
#endif

#endif /* BIT_JAVASCRIPT */
#endif /* _h_JS */

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
