#ifndef AST_H
#define AST_H

typedef enum {
    NODE_INT_LIT,
    NODE_BOOL_LIT,
    NODE_IDENT,
    NODE_BINOP,
    NODE_UNARY_MINUS,
    NODE_VAR_DECL,
    NODE_ASSIGN,
    NODE_IF,
    NODE_WHILE,
    NODE_PRINT,
    NODE_BLOCK,
    NODE_STMT_LIST
} NodeType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_LT, OP_GT, OP_EQ, OP_NE
} BinOp;

typedef enum { TYPE_INT, TYPE_BOOL, TYPE_UNKNOWN } ValueType;

typedef struct ASTNode {
    NodeType type;
    int line;
    union {
        int int_val;                                    /* NODE_INT_LIT */
        int bool_val;                                    /* NODE_BOOL_LIT */
        char *ident;                                      /* NODE_IDENT */
        struct { BinOp op; struct ASTNode *left, *right; } binop;      /* NODE_BINOP */
        struct { struct ASTNode *expr; } unary;           /* NODE_UNARY_MINUS */
        struct { ValueType var_type; char *name; struct ASTNode *init; } var_decl; /* NODE_VAR_DECL */
        struct { char *name; struct ASTNode *expr; } assign;           /* NODE_ASSIGN */
        struct { struct ASTNode *cond, *then_stmt, *else_stmt; } if_stmt; /* NODE_IF */
        struct { struct ASTNode *cond, *body; } while_stmt;            /* NODE_WHILE */
        struct { struct ASTNode *expr; } print_stmt;      /* NODE_PRINT */
        struct { struct ASTNode *stmts; } block;          /* NODE_BLOCK: points to a stmt_list chain */
        struct { struct ASTNode *stmt, *next; } stmt_list; /* NODE_STMT_LIST */
    } data;
} ASTNode;

ASTNode *newIntLit(int val, int line);
ASTNode *newBoolLit(int val, int line);
ASTNode *newIdent(char *name, int line);
ASTNode *newBinOp(BinOp op, ASTNode *l, ASTNode *r, int line);
ASTNode *newUnaryMinus(ASTNode *e, int line);
ASTNode *newVarDecl(ValueType t, char *name, ASTNode *init, int line);
ASTNode *newAssign(char *name, ASTNode *expr, int line);
ASTNode *newIf(ASTNode *cond, ASTNode *then_s, ASTNode *else_s, int line);
ASTNode *newWhile(ASTNode *cond, ASTNode *body, int line);
ASTNode *newPrint(ASTNode *expr, int line);
ASTNode *newBlock(ASTNode *stmts, int line);
ASTNode *newStmtList(ASTNode *stmt, ASTNode *next, int line);

/* In-place O(n) reversal of a NODE_STMT_LIST chain (see parser.y's
   left-recursive stmt_list rule, which builds the list back-to-front). */
ASTNode *reverseStmtList(ASTNode *list);

const char *binOpToStr(BinOp op);

#endif
