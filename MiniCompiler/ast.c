#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

static ASTNode *newNode(NodeType type, int line)
{
    ASTNode *n = (ASTNode *)malloc(sizeof(ASTNode));
    if (!n) {
        fprintf(stderr, "Fatal: out of memory allocating AST node\n");
        exit(1);
    }
    n->type = type;
    n->line = line;
    return n;
}

ASTNode *newIntLit(int val, int line)
{
    ASTNode *n = newNode(NODE_INT_LIT, line);
    n->data.int_val = val;
    return n;
}

ASTNode *newBoolLit(int val, int line)
{
    ASTNode *n = newNode(NODE_BOOL_LIT, line);
    n->data.bool_val = val;
    return n;
}

ASTNode *newIdent(char *name, int line)
{
    ASTNode *n = newNode(NODE_IDENT, line);
    n->data.ident = name;
    return n;
}

ASTNode *newBinOp(BinOp op, ASTNode *l, ASTNode *r, int line)
{
    ASTNode *n = newNode(NODE_BINOP, line);
    n->data.binop.op = op;
    n->data.binop.left = l;
    n->data.binop.right = r;
    return n;
}

ASTNode *newUnaryMinus(ASTNode *e, int line)
{
    ASTNode *n = newNode(NODE_UNARY_MINUS, line);
    n->data.unary.expr = e;
    return n;
}

ASTNode *newVarDecl(ValueType t, char *name, ASTNode *init, int line)
{
    ASTNode *n = newNode(NODE_VAR_DECL, line);
    n->data.var_decl.var_type = t;
    n->data.var_decl.name = name;
    n->data.var_decl.init = init;
    return n;
}

ASTNode *newAssign(char *name, ASTNode *expr, int line)
{
    ASTNode *n = newNode(NODE_ASSIGN, line);
    n->data.assign.name = name;
    n->data.assign.expr = expr;
    return n;
}

ASTNode *newIf(ASTNode *cond, ASTNode *then_s, ASTNode *else_s, int line)
{
    ASTNode *n = newNode(NODE_IF, line);
    n->data.if_stmt.cond = cond;
    n->data.if_stmt.then_stmt = then_s;
    n->data.if_stmt.else_stmt = else_s;
    return n;
}

ASTNode *newWhile(ASTNode *cond, ASTNode *body, int line)
{
    ASTNode *n = newNode(NODE_WHILE, line);
    n->data.while_stmt.cond = cond;
    n->data.while_stmt.body = body;
    return n;
}

ASTNode *newPrint(ASTNode *expr, int line)
{
    ASTNode *n = newNode(NODE_PRINT, line);
    n->data.print_stmt.expr = expr;
    return n;
}

ASTNode *newBlock(ASTNode *stmts, int line)
{
    ASTNode *n = newNode(NODE_BLOCK, line);
    n->data.block.stmts = stmts;
    return n;
}

ASTNode *newStmtList(ASTNode *stmt, ASTNode *next, int line)
{
    ASTNode *n = newNode(NODE_STMT_LIST, line);
    n->data.stmt_list.stmt = stmt;
    n->data.stmt_list.next = next;
    return n;
}

ASTNode *reverseStmtList(ASTNode *list)
{
    ASTNode *prev = NULL;
    ASTNode *cur = list;
    while (cur) {
        ASTNode *next = cur->data.stmt_list.next;
        cur->data.stmt_list.next = prev;
        prev = cur;
        cur = next;
    }
    return prev;
}

const char *binOpToStr(BinOp op)
{
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_LT:  return "<";
        case OP_GT:  return ">";
        case OP_EQ:  return "==";
        case OP_NE:  return "!=";
    }
    return "?";
}
