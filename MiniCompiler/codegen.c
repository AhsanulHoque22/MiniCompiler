#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "codegen.h"

/* ---------------------------------------------------------------------- *
 * A minimal chained-hash-table utility. Used by both the scope-correct
 * name resolver below and the copy-propagation / dead-code-elimination
 * optimizer passes further down, to keep every one of them at O(1)
 * average lookup/insert - a flat array or linked list scanned linearly
 * per lookup would instead give O(k) per lookup and O(k^2) for a pass or
 * scope with k entries. Entries are bump-allocated from a preallocated
 * pool sized for the caller's use case, so there is no per-insert malloc.
 * ---------------------------------------------------------------------- */

typedef struct StrEntry {
    const char *key;
    const char *value; /* unused by the int-map instantiation below */
    int ivalue;
    struct StrEntry *next;
} StrEntry;

typedef struct {
    StrEntry **buckets;
    int nbuckets;
    StrEntry *pool;
    int poolUsed;
} StrMap;

static unsigned long djb2hash(const char *s)
{
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++))
        h = ((h << 5) + h) + c;
    return h;
}

static StrMap *smCreate(int sizeHint)
{
    StrMap *m = (StrMap *)malloc(sizeof(StrMap));
    if (!m) {
        fprintf(stderr, "Fatal: out of memory allocating hash table\n");
        exit(1);
    }
    m->nbuckets = sizeHint < 8 ? 8 : sizeHint;
    m->buckets = (StrEntry **)calloc((size_t)m->nbuckets, sizeof(StrEntry *));
    m->pool = (StrEntry *)malloc((size_t)(sizeHint + 1) * sizeof(StrEntry));
    m->poolUsed = 0;
    if (!m->buckets || !m->pool) {
        fprintf(stderr, "Fatal: out of memory allocating hash table\n");
        exit(1);
    }
    return m;
}

static void smClear(StrMap *m)
{
    memset(m->buckets, 0, (size_t)m->nbuckets * sizeof(StrEntry *));
    m->poolUsed = 0;
}

static void smDestroy(StrMap *m)
{
    free(m->buckets);
    free(m->pool);
    free(m);
}

static StrEntry *smFind(StrMap *m, const char *key)
{
    unsigned long idx = djb2hash(key) % (unsigned long)m->nbuckets;
    for (StrEntry *e = m->buckets[idx]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e;
    return NULL;
}

static void smSetStr(StrMap *m, const char *key, const char *value)
{
    StrEntry *e = smFind(m, key);
    if (e) { e->value = value; return; }
    unsigned long idx = djb2hash(key) % (unsigned long)m->nbuckets;
    e = &m->pool[m->poolUsed++];
    e->key = key; e->value = value; e->ivalue = 0;
    e->next = m->buckets[idx];
    m->buckets[idx] = e;
}

static void smSetInt(StrMap *m, const char *key, int value)
{
    StrEntry *e = smFind(m, key);
    if (e) { e->ivalue = value; return; }
    unsigned long idx = djb2hash(key) % (unsigned long)m->nbuckets;
    e = &m->pool[m->poolUsed++];
    e->key = key; e->value = NULL; e->ivalue = value;
    e->next = m->buckets[idx];
    m->buckets[idx] = e;
}

static const char *smGetStr(StrMap *m, const char *key)
{
    StrEntry *e = smFind(m, key);
    return e ? e->value : NULL;
}

static int smGetInt(StrMap *m, const char *key, int missingValue)
{
    StrEntry *e = smFind(m, key);
    return e ? e->ivalue : missingValue;
}

/* ---------------------------------------------------------------------- */
/* TAC list management                                                    */
/* ---------------------------------------------------------------------- */

static void ensureCap(TACList *l)
{
    if (l->count >= l->capacity) {
        l->capacity = l->capacity ? l->capacity * 2 : 64;
        l->instrs = (TACInstr *)realloc(l->instrs, (size_t)l->capacity * sizeof(TACInstr));
        if (!l->instrs) {
            fprintf(stderr, "Fatal: out of memory growing TAC list\n");
            exit(1);
        }
    }
}

static TACList *tac;

static void emit(TACKind kind, char *op, char *arg1, char *arg2, char *result)
{
    ensureCap(tac);
    TACInstr *ins = &tac->instrs[tac->count++];
    ins->kind = kind;
    ins->op = op;
    ins->arg1 = arg1;
    ins->arg2 = arg2;
    ins->result = result;
    ins->dead = 0;
}

/* ---------------------------------------------------------------------- */
/* Temporary / label name generation                                      */
/* ---------------------------------------------------------------------- */

static int tempCounter;
static int labelCounter;

static char *newTemp(void)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "$t%d", ++tempCounter);
    return strdup(buf);
}

static char *newLabel(void)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "$L%d", ++labelCounter);
    return strdup(buf);
}

/* ---------------------------------------------------------------------- */
/* Scope-correct naming for shadowed declarations (x, x$2, x$3, ...)      */
/* This mirrors symbol_table.c's scope-stack shadowing resolution order,  */
/* but is independent of it: semantic.c's symbol table is already freed  */
/* by the time code generation runs. Each scope's bindings and the global */
/* per-name declaration counter are StrMaps (O(1) average lookup/insert), */
/* not linked lists - for a wide program with many distinct top-level     */
/* names, a linked-list scan here would be exactly the same O(k^2)        */
/* mistake already fixed in the optimizer passes below (see §6.4/§6.5     */
/* of the report and the empirical benchmark that caught it in §6.6).     */
/* ---------------------------------------------------------------------- */

typedef struct CGScope {
    StrMap *bindings;
    struct CGScope *parent;
} CGScope;

static CGScope *cgCurrent = NULL;
static StrMap *declCounts = NULL;

/* Sizing hint for every StrMap created during codegen: an exact upper
   bound (V, the total number of declarations in the program - see report
   §7.2) computed once via a single O(n) AST walk before codegen starts. */
static int cgSizeHint = 16;

static int nextDeclIndex(const char *name)
{
    int c = smGetInt(declCounts, name, 0) + 1;
    smSetInt(declCounts, name, c);
    return c;
}

static void cgEnterScope(void)
{
    CGScope *s = (CGScope *)malloc(sizeof(CGScope));
    s->bindings = smCreate(cgSizeHint);
    s->parent = cgCurrent;
    cgCurrent = s;
}

static void cgExitScope(void)
{
    if (!cgCurrent) return;
    CGScope *dead = cgCurrent;
    cgCurrent = cgCurrent->parent;
    smDestroy(dead->bindings);
    free(dead);
}

/* Registers a new declaration and returns its TAC storage name (owned by caller). */
static char *cgDeclare(const char *sourceName)
{
    int idx = nextDeclIndex(sourceName);
    char *tacName;
    if (idx == 1) {
        tacName = strdup(sourceName);
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s$%d", sourceName, idx);
        tacName = strdup(buf);
    }

    /* Store an independent copy as the map's value: the caller frees the
       tacName this function returns once it has emitted its own TAC. */
    smSetStr(cgCurrent->bindings, sourceName, strdup(tacName));

    return tacName;
}

/* Resolves an identifier use to its innermost visible TAC storage name. */
static char *cgResolve(const char *sourceName)
{
    for (CGScope *sc = cgCurrent; sc; sc = sc->parent) {
        const char *found = smGetStr(sc->bindings, sourceName);
        if (found) return strdup(found);
    }
    /* Should be unreachable: semantic analysis already rejected undeclared uses. */
    return strdup(sourceName);
}

/* One O(n) walk to size every StrMap created during codegen (see cgSizeHint
   above): counts every NODE_VAR_DECL reachable from a statement list,
   recursing into if/while bodies and nested blocks. */
static int countVarDeclsInStmt(ASTNode *stmt)
{
    if (!stmt) return 0;
    switch (stmt->type) {
        case NODE_VAR_DECL:
            return 1;
        case NODE_IF: {
            int c = countVarDeclsInStmt(stmt->data.if_stmt.then_stmt);
            if (stmt->data.if_stmt.else_stmt)
                c += countVarDeclsInStmt(stmt->data.if_stmt.else_stmt);
            return c;
        }
        case NODE_WHILE:
            return countVarDeclsInStmt(stmt->data.while_stmt.body);
        case NODE_BLOCK: {
            int c = 0;
            for (ASTNode *cur = stmt->data.block.stmts; cur; cur = cur->data.stmt_list.next)
                c += countVarDeclsInStmt(cur->data.stmt_list.stmt);
            return c;
        }
        default:
            return 0;
    }
}

static int countVarDecls(ASTNode *list)
{
    int count = 0;
    for (ASTNode *cur = list; cur; cur = cur->data.stmt_list.next)
        count += countVarDeclsInStmt(cur->data.stmt_list.stmt);
    return count;
}

/* ---------------------------------------------------------------------- */
/* AST -> TAC translation                                                 */
/* ---------------------------------------------------------------------- */

static void genStmtList(ASTNode *list);
static void genStmt(ASTNode *n);

/* Mirrors semantic.c's checkExpr depth guard: genExpr recurses once per
   BINOP/UNARY_MINUS nesting level, so an unbounded left-deep expression
   (e.g. a long chain of binary operators, which Bison's own stack limit
   does not catch) could otherwise overflow the C call stack. In practice
   semantic analysis (which runs first and shares this same limit) already
   rejects such input before codegen ever runs, but this guard is kept here
   too so genExpr is safe standalone. */
#define MAX_GEN_EXPR_DEPTH 3000
static int genExprDepth = 0;

static char *genExpr(ASTNode *n)
{
    if (!n) return strdup("0");

    genExprDepth++;
    if (genExprDepth > MAX_GEN_EXPR_DEPTH) {
        genExprDepth--;
        return strdup("0"); /* semantic analysis should have already rejected this program */
    }

    char *result;
    switch (n->type) {
        case NODE_INT_LIT: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", n->data.int_val);
            result = strdup(buf);
            break;
        }
        case NODE_BOOL_LIT:
            result = strdup(n->data.bool_val ? "1" : "0");
            break;

        case NODE_IDENT:
            result = cgResolve(n->data.ident);
            break;

        case NODE_UNARY_MINUS: {
            char *v = genExpr(n->data.unary.expr);
            char *t = newTemp();
            emit(TAC_UNOP, strdup("-"), v, NULL, strdup(t));
            result = t;
            break;
        }

        case NODE_BINOP: {
            char *l = genExpr(n->data.binop.left);
            char *r = genExpr(n->data.binop.right);
            char *t = newTemp();
            emit(TAC_BINOP, strdup(binOpToStr(n->data.binop.op)), l, r, strdup(t));
            result = t;
            break;
        }

        default:
            result = strdup("0");
            break;
    }

    genExprDepth--;
    return result;
}

static void genStmt(ASTNode *n)
{
    if (!n) return;

    switch (n->type) {
        case NODE_VAR_DECL: {
            /* Evaluate the initializer *before* the new binding is declared, so a
               self-shadowing initializer (e.g. `int x; { int x = x + 1; }`) resolves
               the right-hand `x` to the outer variable, matching semantic.c's order. */
            char *v = NULL;
            if (n->data.var_decl.init)
                v = genExpr(n->data.var_decl.init);
            char *tacName = cgDeclare(n->data.var_decl.name);
            if (v) {
                emit(TAC_ASSIGN, NULL, v, NULL, strdup(tacName));
            }
            free(tacName);
            break;
        }

        case NODE_ASSIGN: {
            char *tacName = cgResolve(n->data.assign.name);
            char *v = genExpr(n->data.assign.expr);
            emit(TAC_ASSIGN, NULL, v, NULL, tacName);
            break;
        }

        case NODE_IF: {
            char *c = genExpr(n->data.if_stmt.cond);
            if (n->data.if_stmt.else_stmt) {
                char *lFalse = newLabel();
                char *lEnd = newLabel();
                emit(TAC_IFZ, NULL, c, NULL, strdup(lFalse));
                genStmt(n->data.if_stmt.then_stmt);
                emit(TAC_GOTO, NULL, NULL, NULL, strdup(lEnd));
                emit(TAC_LABEL, NULL, NULL, NULL, lFalse);
                genStmt(n->data.if_stmt.else_stmt);
                emit(TAC_LABEL, NULL, NULL, NULL, lEnd);
            } else {
                char *lFalse = newLabel();
                emit(TAC_IFZ, NULL, c, NULL, strdup(lFalse));
                genStmt(n->data.if_stmt.then_stmt);
                emit(TAC_LABEL, NULL, NULL, NULL, lFalse);
            }
            break;
        }

        case NODE_WHILE: {
            char *lStart = newLabel();
            char *lEnd = newLabel();
            emit(TAC_LABEL, NULL, NULL, NULL, strdup(lStart));
            char *c = genExpr(n->data.while_stmt.cond);
            emit(TAC_IFZ, NULL, c, NULL, strdup(lEnd));
            genStmt(n->data.while_stmt.body);
            emit(TAC_GOTO, NULL, NULL, NULL, lStart);
            emit(TAC_LABEL, NULL, NULL, NULL, lEnd);
            break;
        }

        case NODE_PRINT: {
            char *v = genExpr(n->data.print_stmt.expr);
            emit(TAC_PRINT, NULL, v, NULL, NULL);
            break;
        }

        case NODE_BLOCK:
            cgEnterScope();
            genStmtList(n->data.block.stmts);
            cgExitScope();
            break;

        default:
            break;
    }
}

static void genStmtList(ASTNode *list)
{
    for (ASTNode *cur = list; cur; cur = cur->data.stmt_list.next)
        genStmt(cur->data.stmt_list.stmt);
}

TACList *generateTAC(ASTNode *program)
{
    tac = (TACList *)calloc(1, sizeof(TACList));
    tempCounter = 0;
    labelCounter = 0;
    cgCurrent = NULL;

    cgSizeHint = countVarDecls(program);
    if (cgSizeHint < 16) cgSizeHint = 16;
    declCounts = smCreate(cgSizeHint);

    cgEnterScope();
    genStmtList(program);
    cgExitScope();

    smDestroy(declCounts);
    declCounts = NULL;

    return tac;
}

/* ---------------------------------------------------------------------- */
/* Optimization passes                                                    */
/* ---------------------------------------------------------------------- */

static int isIntLiteral(const char *s)
{
    if (!s || !*s) return 0;
    int i = 0;
    if (s[0] == '-') i = 1;
    if (!s[i]) return 0; /* lone '-' */
    for (; s[i]; i++)
        if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}

static int constantFoldPass(TACList *list)
{
    int changed = 0;
    for (int i = 0; i < list->count; i++) {
        TACInstr *ins = &list->instrs[i];
        if (ins->dead) continue;

        if (ins->kind == TAC_BINOP && isIntLiteral(ins->arg1) && isIntLiteral(ins->arg2)) {
            long a = atol(ins->arg1);
            long b = atol(ins->arg2);
            long r;
            int folded = 1;

            if (strcmp(ins->op, "+") == 0) r = a + b;
            else if (strcmp(ins->op, "-") == 0) r = a - b;
            else if (strcmp(ins->op, "*") == 0) r = a * b;
            else if (strcmp(ins->op, "/") == 0) {
                if (b == 0) { folded = 0; r = 0; }
                else r = a / b;
            }
            else if (strcmp(ins->op, "<") == 0) r = (a < b);
            else if (strcmp(ins->op, ">") == 0) r = (a > b);
            else if (strcmp(ins->op, "==") == 0) r = (a == b);
            else if (strcmp(ins->op, "!=") == 0) r = (a != b);
            else { folded = 0; r = 0; }

            if (folded) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%ld", r);
                ins->kind = TAC_ASSIGN;
                ins->op = NULL;
                ins->arg1 = strdup(buf);
                ins->arg2 = NULL;
                changed = 1;
            }
        } else if (ins->kind == TAC_UNOP && isIntLiteral(ins->arg1) && strcmp(ins->op, "-") == 0) {
            long v = -atol(ins->arg1);
            char buf[32];
            snprintf(buf, sizeof(buf), "%ld", v);
            ins->kind = TAC_ASSIGN;
            ins->op = NULL;
            ins->arg1 = strdup(buf);
            changed = 1;
        }
    }
    return changed;
}

static int algebraicSimplifyPass(TACList *list)
{
    int changed = 0;
    for (int i = 0; i < list->count; i++) {
        TACInstr *ins = &list->instrs[i];
        if (ins->dead || ins->kind != TAC_BINOP) continue;

        char *keep = NULL;
        char *lit = NULL; /* replace with a literal instead */

        if (strcmp(ins->op, "+") == 0) {
            if (isIntLiteral(ins->arg1) && atol(ins->arg1) == 0) keep = ins->arg2;
            else if (isIntLiteral(ins->arg2) && atol(ins->arg2) == 0) keep = ins->arg1;
        } else if (strcmp(ins->op, "-") == 0) {
            if (isIntLiteral(ins->arg2) && atol(ins->arg2) == 0) keep = ins->arg1;
        } else if (strcmp(ins->op, "*") == 0) {
            if (isIntLiteral(ins->arg1) && atol(ins->arg1) == 0) lit = "0";
            else if (isIntLiteral(ins->arg2) && atol(ins->arg2) == 0) lit = "0";
            else if (isIntLiteral(ins->arg1) && atol(ins->arg1) == 1) keep = ins->arg2;
            else if (isIntLiteral(ins->arg2) && atol(ins->arg2) == 1) keep = ins->arg1;
        } else if (strcmp(ins->op, "/") == 0) {
            if (isIntLiteral(ins->arg2) && atol(ins->arg2) == 1) keep = ins->arg1;
        }

        if (lit) {
            ins->kind = TAC_ASSIGN;
            ins->op = NULL;
            ins->arg1 = strdup(lit);
            ins->arg2 = NULL;
            changed = 1;
        } else if (keep) {
            char *kept = strdup(keep);
            ins->kind = TAC_ASSIGN;
            ins->op = NULL;
            ins->arg1 = kept;
            ins->arg2 = NULL;
            changed = 1;
        }
    }
    return changed;
}

/* ---------------------------------------------------------------------- *
 * Copy propagation, O(m) amortized.
 *
 * copySrc[Y]   = X   : "Y currently holds a straight-line copy of X"
 * copySrcVer[Y]= S    : the sequence number X had when that copy was made
 * lastDefSeq[N]= S    : the sequence number at which N was most recently
 *                        (re)defined (bumped on every ASSIGN/BINOP/UNOP result)
 *
 * A recorded copy Y->X is only trusted if lastDefSeq[X] still equals the
 * sequence number stamped at copy time - i.e. X has not been redefined
 * since. This makes invalidation an O(1) side effect of bumping X's
 * def-sequence, instead of an O(m) scan for every entry whose value
 * happens to equal the redefined name.
 * ---------------------------------------------------------------------- */

static const char *resolveCopy(StrMap *copySrc, StrMap *copySrcVer, StrMap *lastDefSeq, const char *name)
{
    /* copySrc entries are always written from an already-resolved RHS (see
       copyPropagatePass below), so chains are acyclic by construction and
       this loop always terminates on its own; the cap is just a defensive
       backstop against a future change accidentally introducing a cycle,
       set high enough to never truncate propagation on any real chain. */
    for (int guard = 0; guard < 1000000; guard++) {
        const char *src = smGetStr(copySrc, name);
        if (!src) break;
        int recordedVer = smGetInt(copySrcVer, name, -1);
        int currentVer = smGetInt(lastDefSeq, src, -1);
        if (recordedVer != currentVer) break; /* stale: source changed since the copy */
        name = src;
    }
    return name;
}

static int copyPropagatePass(TACList *list)
{
    int changed = 0;
    int sizeHint = 2 * list->count + 8;

    StrMap *copySrc = smCreate(sizeHint);
    StrMap *copySrcVer = smCreate(sizeHint);
    StrMap *lastDefSeq = smCreate(sizeHint);
    int seq = 0;

    for (int i = 0; i < list->count; i++) {
        TACInstr *ins = &list->instrs[i];
        if (ins->dead) continue;

        if (ins->kind == TAC_LABEL) {
            /* Control-flow merge point: forget straight-line copy facts, but
               keep def-sequence history (it stays globally meaningful). */
            smClear(copySrc);
            smClear(copySrcVer);
            continue;
        }

        if (ins->arg1 && ins->kind != TAC_GOTO) {
            const char *resolved = resolveCopy(copySrc, copySrcVer, lastDefSeq, ins->arg1);
            if (resolved != ins->arg1) { ins->arg1 = strdup(resolved); changed = 1; }
        }
        if (ins->arg2 && ins->kind == TAC_BINOP) {
            const char *resolved = resolveCopy(copySrc, copySrcVer, lastDefSeq, ins->arg2);
            if (resolved != ins->arg2) { ins->arg2 = strdup(resolved); changed = 1; }
        }

        if (ins->kind == TAC_ASSIGN) {
            smSetStr(copySrc, ins->result, ins->arg1);
            smSetInt(copySrcVer, ins->result, smGetInt(lastDefSeq, ins->arg1, -1));
            smSetInt(lastDefSeq, ins->result, ++seq);
        } else if (ins->kind == TAC_BINOP || ins->kind == TAC_UNOP) {
            smSetInt(lastDefSeq, ins->result, ++seq); /* invalidates any stale copies of it */
        }
    }

    smDestroy(copySrc);
    smDestroy(copySrcVer);
    smDestroy(lastDefSeq);
    return changed;
}

static int deadCodeEliminatePass(TACList *list)
{
    StrMap *used = smCreate(2 * list->count + 8);

    for (int i = 0; i < list->count; i++) {
        TACInstr *ins = &list->instrs[i];
        if (ins->dead) continue;
        if (ins->arg1 && ins->kind != TAC_GOTO) smSetInt(used, ins->arg1, 1);
        if (ins->arg2 && ins->kind == TAC_BINOP) smSetInt(used, ins->arg2, 1);
    }

    int changed = 0;
    for (int i = 0; i < list->count; i++) {
        TACInstr *ins = &list->instrs[i];
        if (ins->dead) continue;
        if (ins->kind != TAC_ASSIGN && ins->kind != TAC_BINOP && ins->kind != TAC_UNOP) continue;
        if (!ins->result || strncmp(ins->result, "$t", 2) != 0) continue; /* only compiler temps */

        if (!smGetInt(used, ins->result, 0)) {
            ins->dead = 1;
            changed = 1;
        }
    }

    smDestroy(used);

    if (changed) {
        int w = 0;
        for (int i = 0; i < list->count; i++)
            if (!list->instrs[i].dead)
                list->instrs[w++] = list->instrs[i];
        list->count = w;
    }

    return changed;
}

void optimizeTAC(TACList *list)
{
    int changed = 1;
    int iterations = 0;
    while (changed && iterations < 8) {
        changed = 0;
        changed |= constantFoldPass(list);
        changed |= algebraicSimplifyPass(list);
        changed |= copyPropagatePass(list);
        changed |= deadCodeEliminatePass(list);
        iterations++;
    }
}

/* ---------------------------------------------------------------------- */
/* Output: TAC listing                                                    */
/* ---------------------------------------------------------------------- */

void writeTAC(const TACList *list, const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Error: could not open '%s' for writing\n", filename);
        return;
    }

    for (int i = 0; i < list->count; i++) {
        const TACInstr *ins = &list->instrs[i];
        if (ins->dead) continue;
        switch (ins->kind) {
            case TAC_ASSIGN: fprintf(f, "%s = %s\n", ins->result, ins->arg1); break;
            case TAC_BINOP:  fprintf(f, "%s = %s %s %s\n", ins->result, ins->arg1, ins->op, ins->arg2); break;
            case TAC_UNOP:   fprintf(f, "%s = %s%s\n", ins->result, ins->op, ins->arg1); break;
            case TAC_LABEL:  fprintf(f, "%s:\n", ins->result); break;
            case TAC_GOTO:   fprintf(f, "goto %s\n", ins->result); break;
            case TAC_IFZ:    fprintf(f, "ifz %s goto %s\n", ins->arg1, ins->result); break;
            case TAC_PRINT:  fprintf(f, "print %s\n", ins->arg1); break;
        }
    }

    fclose(f);
}

/* ---------------------------------------------------------------------- */
/* Output: simplified stack-machine target code                          */
/* ---------------------------------------------------------------------- */

static const char *opMnemonic(const char *op)
{
    if (strcmp(op, "+") == 0) return "ADD";
    if (strcmp(op, "-") == 0) return "SUB";
    if (strcmp(op, "*") == 0) return "MUL";
    if (strcmp(op, "/") == 0) return "DIV";
    if (strcmp(op, "<") == 0) return "LT";
    if (strcmp(op, ">") == 0) return "GT";
    if (strcmp(op, "==") == 0) return "EQ";
    if (strcmp(op, "!=") == 0) return "NE";
    return "NOP";
}

void generateTargetCode(const TACList *list, const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Error: could not open '%s' for writing\n", filename);
        return;
    }

    for (int i = 0; i < list->count; i++) {
        const TACInstr *ins = &list->instrs[i];
        if (ins->dead) continue;
        switch (ins->kind) {
            case TAC_ASSIGN:
                fprintf(f, "LOAD %s\nSTORE %s\n", ins->arg1, ins->result);
                break;
            case TAC_BINOP:
                fprintf(f, "LOAD %s\nLOAD %s\n%s\nSTORE %s\n", ins->arg1, ins->arg2, opMnemonic(ins->op), ins->result);
                break;
            case TAC_UNOP:
                fprintf(f, "LOAD %s\nNEG\nSTORE %s\n", ins->arg1, ins->result);
                break;
            case TAC_LABEL:
                fprintf(f, "%s:\n", ins->result);
                break;
            case TAC_GOTO:
                fprintf(f, "JMP %s\n", ins->result);
                break;
            case TAC_IFZ:
                fprintf(f, "LOAD %s\nJZ %s\n", ins->arg1, ins->result);
                break;
            case TAC_PRINT:
                fprintf(f, "LOAD %s\nPRINT\n", ins->arg1);
                break;
        }
    }

    fclose(f);
}
