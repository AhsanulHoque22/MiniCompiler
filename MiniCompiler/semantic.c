#include <stdio.h>
#include <stdarg.h>
#include "semantic.h"
#include "symbol_table.h"

int semErrorCount = 0;

static const char *typeToStr(ValueType t)
{
    switch (t) {
        case TYPE_INT:  return "int";
        case TYPE_BOOL: return "bool";
        default:        return "unknown";
    }
}

static void semError(int line, const char *fmt, ...);

/* checkExpr recurses once per BINOP/UNARY_MINUS nesting level. A flat chain
   of binary operators (e.g. "1+1+1+...+1") builds a left-deep expression
   tree that Bison's own parser-stack limit does NOT protect against (unlike
   deeply nested parentheses: left-associative operators reduce as they go,
   so they never grow Bison's stack) - so an unbounded input could exhaust
   the C call stack and crash instead of failing cleanly. Guard explicitly. */
#define MAX_EXPR_DEPTH 3000
static int exprDepth = 0;
static int exprDepthErrorReported = 0;

static ValueType checkExpr(ASTNode *n)
{
    if (!n) return TYPE_UNKNOWN;

    exprDepth++;
    if (exprDepth > MAX_EXPR_DEPTH) {
        /* The global depth counter can cross the threshold more than once
           per program (e.g. once descending the deep spine, again for a
           shallow sibling visited right after it unwinds), so gate the
           message on a one-shot flag rather than an exact depth value. */
        if (!exprDepthErrorReported) {
            semError(n->line, "expression nested too deeply (limit %d levels)", MAX_EXPR_DEPTH);
            exprDepthErrorReported = 1;
        }
        exprDepth--;
        return TYPE_UNKNOWN;
    }

    ValueType result = TYPE_UNKNOWN;

    switch (n->type) {
        case NODE_INT_LIT:
            result = TYPE_INT;
            break;

        case NODE_BOOL_LIT:
            result = TYPE_BOOL;
            break;

        case NODE_IDENT: {
            Symbol *sym = lookupSymbol(n->data.ident);
            if (!sym) {
                semError(n->line, "undeclared variable '%s'", n->data.ident);
                break;
            }
            result = sym->type;
            break;
        }

        case NODE_UNARY_MINUS: {
            ValueType t = checkExpr(n->data.unary.expr);
            if (t == TYPE_UNKNOWN) break;
            if (t != TYPE_INT) {
                semError(n->line, "type mismatch: unary '-' requires an int operand, got %s", typeToStr(t));
                break;
            }
            result = TYPE_INT;
            break;
        }

        case NODE_BINOP: {
            ValueType lt = checkExpr(n->data.binop.left);
            ValueType rt = checkExpr(n->data.binop.right);
            if (lt == TYPE_UNKNOWN || rt == TYPE_UNKNOWN) break;

            BinOp op = n->data.binop.op;
            switch (op) {
                case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
                    if (lt != TYPE_INT || rt != TYPE_INT) {
                        semError(n->line, "type mismatch: operator '%s' requires int operands, got %s and %s",
                                 binOpToStr(op), typeToStr(lt), typeToStr(rt));
                        break;
                    }
                    result = TYPE_INT;
                    break;

                case OP_LT: case OP_GT:
                    if (lt != TYPE_INT || rt != TYPE_INT) {
                        semError(n->line, "type mismatch: operator '%s' requires int operands, got %s and %s",
                                 binOpToStr(op), typeToStr(lt), typeToStr(rt));
                        break;
                    }
                    result = TYPE_BOOL;
                    break;

                case OP_EQ: case OP_NE:
                    if (lt != rt) {
                        semError(n->line, "type mismatch: operands of '%s' must have the same type, got %s and %s",
                                 binOpToStr(op), typeToStr(lt), typeToStr(rt));
                        break;
                    }
                    result = TYPE_BOOL;
                    break;
            }
            break;
        }

        default:
            break;
    }

    exprDepth--;
    return result;
}

static void checkStmtList(ASTNode *list);

static void checkStmt(ASTNode *n)
{
    if (!n) return;

    switch (n->type) {
        case NODE_VAR_DECL: {
            ValueType declaredType = n->data.var_decl.var_type;
            ValueType initType = TYPE_UNKNOWN;
            int hasInit = (n->data.var_decl.init != NULL);
            if (hasInit)
                initType = checkExpr(n->data.var_decl.init);

            if (!declareSymbol(n->data.var_decl.name, declaredType)) {
                semError(n->line, "variable '%s' is already declared in this scope", n->data.var_decl.name);
            }

            if (hasInit && initType != TYPE_UNKNOWN && initType != declaredType) {
                semError(n->line, "type mismatch: cannot assign %s value to %s variable '%s'",
                         typeToStr(initType), typeToStr(declaredType), n->data.var_decl.name);
            }
            break;
        }

        case NODE_ASSIGN: {
            Symbol *sym = lookupSymbol(n->data.assign.name);
            ValueType exprType = checkExpr(n->data.assign.expr);
            if (!sym) {
                semError(n->line, "undeclared variable '%s'", n->data.assign.name);
                break;
            }
            if (exprType != TYPE_UNKNOWN && exprType != sym->type) {
                semError(n->line, "type mismatch: cannot assign %s value to %s variable '%s'",
                         typeToStr(exprType), typeToStr(sym->type), n->data.assign.name);
            }
            break;
        }

        case NODE_IF: {
            ValueType condType = checkExpr(n->data.if_stmt.cond);
            if (condType != TYPE_UNKNOWN && condType != TYPE_BOOL) {
                semError(n->line, "invalid conditional expression: expected bool, got %s", typeToStr(condType));
            }
            checkStmt(n->data.if_stmt.then_stmt);
            if (n->data.if_stmt.else_stmt)
                checkStmt(n->data.if_stmt.else_stmt);
            break;
        }

        case NODE_WHILE: {
            ValueType condType = checkExpr(n->data.while_stmt.cond);
            if (condType != TYPE_UNKNOWN && condType != TYPE_BOOL) {
                semError(n->line, "invalid conditional expression: expected bool, got %s", typeToStr(condType));
            }
            checkStmt(n->data.while_stmt.body);
            break;
        }

        case NODE_PRINT:
            checkExpr(n->data.print_stmt.expr);
            break;

        case NODE_BLOCK:
            enterScope();
            checkStmtList(n->data.block.stmts);
            exitScope();
            break;

        default:
            break;
    }
}

static void checkStmtList(ASTNode *list)
{
    for (ASTNode *cur = list; cur; cur = cur->data.stmt_list.next)
        checkStmt(cur->data.stmt_list.stmt);
}

void runSemanticAnalysis(ASTNode *program)
{
    semErrorCount = 0;
    exprDepth = 0;
    exprDepthErrorReported = 0;
    enterScope(); /* global scope */
    checkStmtList(program);
    exitScope();
}

static void semError(int line, const char *fmt, ...)
{
    fprintf(stderr, "Semantic Error at line %d: ", line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    semErrorCount++;
}
