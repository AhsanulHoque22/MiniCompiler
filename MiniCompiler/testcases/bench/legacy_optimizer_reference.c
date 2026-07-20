/* LEGACY REFERENCE - NOT part of the build.
 *
 * This is a preserved snapshot of codegen.c's copyPropagatePass() and
 * deadCodeEliminatePass() *before* the hash-table rewrite described in
 * report Sec. 6.4/6.5. It used a flat array scanned linearly per lookup:
 * O(m) per lookup, called up to O(m) times per pass, giving O(m^2) total -
 * which directly contradicted the report's own claimed O(m) linear-pass
 * complexity. Kept here so the report's Sec. 6.6 "before" comparison is
 * independently reproducible, not just asserted.
 *
 * To reproduce the comparison yourself:
 *   1. cp -r MiniCompiler /tmp/mc_legacy && cd /tmp/mc_legacy
 *   2. In codegen.c, replace copyPropagatePass()/deadCodeEliminatePass()
 *      (and add the AliasEntry/aliasLookup/aliasInvalidate helpers below)
 *      with the code in this file.
 *   3. make && python3 testcases/bench/gen_wide.py 8000 > /tmp/w.ml
 *      time ./minicompiler /tmp/w.ml
 *      (compare against the current implementation's time for the same N)
 *
 * See testcases/bench/legacy_results.txt for the actual measured numbers.
 */

typedef struct AliasEntry { char *name; char *value; } AliasEntry;

static char *aliasLookup(AliasEntry *tbl, int n, const char *name)
{
    for (int guard = 0; guard < n + 1; guard++) {
        int found = 0;
        for (int i = 0; i < n; i++) {
            if (strcmp(tbl[i].name, name) == 0) { name = tbl[i].value; found = 1; break; }
        }
        if (!found) break;
    }
    return (char *)name;
}

static void aliasInvalidate(AliasEntry *tbl, int *n, const char *name)
{
    int w = 0;
    for (int i = 0; i < *n; i++) {
        if (strcmp(tbl[i].name, name) == 0 || strcmp(tbl[i].value, name) == 0) continue;
        tbl[w++] = tbl[i];
    }
    *n = w;
}

static int copyPropagatePass(TACList *list)
{
    int changed = 0;
    int cap = list->count + 1;
    AliasEntry *tbl = (AliasEntry *)malloc((size_t)cap * sizeof(AliasEntry));
    int n = 0;

    for (int i = 0; i < list->count; i++) {
        TACInstr *ins = &list->instrs[i];
        if (ins->dead) continue;
        if (ins->kind == TAC_LABEL) { n = 0; continue; }

        if (ins->arg1 && ins->kind != TAC_GOTO) {
            char *resolved = aliasLookup(tbl, n, ins->arg1);
            if (resolved != ins->arg1) { ins->arg1 = strdup(resolved); changed = 1; }
        }
        if (ins->arg2 && ins->kind == TAC_BINOP) {
            char *resolved = aliasLookup(tbl, n, ins->arg2);
            if (resolved != ins->arg2) { ins->arg2 = strdup(resolved); changed = 1; }
        }

        if (ins->kind == TAC_ASSIGN) {
            aliasInvalidate(tbl, &n, ins->result);
            tbl[n].name = ins->result; tbl[n].value = ins->arg1; n++;
        } else if (ins->kind == TAC_BINOP || ins->kind == TAC_UNOP) {
            aliasInvalidate(tbl, &n, ins->result);
        }
    }

    free(tbl);
    return changed;
}

static int deadCodeEliminatePass(TACList *list)
{
    int used_cap = 2 * list->count + 1;
    char **used = (char **)malloc((size_t)used_cap * sizeof(char *));
    int usedCount = 0;

    for (int i = 0; i < list->count; i++) {
        TACInstr *ins = &list->instrs[i];
        if (ins->dead) continue;
        if (ins->arg1 && ins->kind != TAC_GOTO) used[usedCount++] = ins->arg1;
        if (ins->arg2 && ins->kind == TAC_BINOP) used[usedCount++] = ins->arg2;
    }

    int changed = 0;
    for (int i = 0; i < list->count; i++) {
        TACInstr *ins = &list->instrs[i];
        if (ins->dead) continue;
        if (ins->kind != TAC_ASSIGN && ins->kind != TAC_BINOP && ins->kind != TAC_UNOP) continue;
        if (!ins->result || strncmp(ins->result, "$t", 2) != 0) continue;

        int isUsed = 0;
        for (int j = 0; j < usedCount; j++) {
            if (strcmp(used[j], ins->result) == 0) { isUsed = 1; break; }
        }
        if (!isUsed) { ins->dead = 1; changed = 1; }
    }

    free(used);

    if (changed) {
        int w = 0;
        for (int i = 0; i < list->count; i++)
            if (!list->instrs[i].dead) list->instrs[w++] = list->instrs[i];
        list->count = w;
    }

    return changed;
}
