#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

static Scope *current = NULL;

/* djb2 string hash */
static unsigned long hashStr(const char *s)
{
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++))
        h = ((h << 5) + h) + c; /* h*33 + c */
    return h;
}

void enterScope(void)
{
    Scope *s = (Scope *)malloc(sizeof(Scope));
    if (!s) {
        fprintf(stderr, "Fatal: out of memory allocating scope\n");
        exit(1);
    }
    s->nbuckets = NUM_BUCKETS;
    s->count = 0;
    s->buckets = (Symbol **)calloc((size_t)s->nbuckets, sizeof(Symbol *));
    if (!s->buckets) {
        fprintf(stderr, "Fatal: out of memory allocating scope\n");
        exit(1);
    }
    s->parent = current;
    current = s;
}

void exitScope(void)
{
    if (!current) return;
    Scope *dead = current;
    current = current->parent;

    for (int i = 0; i < dead->nbuckets; i++) {
        Symbol *sym = dead->buckets[i];
        while (sym) {
            Symbol *next = sym->next;
            free(sym->name);
            free(sym);
            sym = next;
        }
    }
    free(dead->buckets);
    free(dead);
}

/* Doubles the bucket count and rehashes every existing symbol once the
   load factor (count / nbuckets) would otherwise exceed 2. Without this,
   a single scope holding many thousands of declarations (e.g. a very wide,
   unnested MiniLang program) would degrade the fixed-211-bucket table's
   average chain length - and therefore declareSymbol/lookupSymbol's cost -
   from O(1) toward O(n), the same class of bug already fixed in codegen.c's
   scope resolver and the TAC optimizer (see report §5.4, §6.4-6.6). */
static void growIfNeeded(Scope *s)
{
    if (s->count < 2 * s->nbuckets) return;

    int newCount = s->nbuckets * 2;
    Symbol **newBuckets = (Symbol **)calloc((size_t)newCount, sizeof(Symbol *));
    if (!newBuckets) {
        fprintf(stderr, "Fatal: out of memory growing scope\n");
        exit(1);
    }

    for (int i = 0; i < s->nbuckets; i++) {
        Symbol *sym = s->buckets[i];
        while (sym) {
            Symbol *next = sym->next;
            unsigned long idx = hashStr(sym->name) % (unsigned long)newCount;
            sym->next = newBuckets[idx];
            newBuckets[idx] = sym;
            sym = next;
        }
    }

    free(s->buckets);
    s->buckets = newBuckets;
    s->nbuckets = newCount;
}

int declareSymbol(const char *name, ValueType type)
{
    if (!current) enterScope(); /* safety net: should not normally happen */

    unsigned long idx = hashStr(name) % (unsigned long)current->nbuckets;
    for (Symbol *s = current->buckets[idx]; s; s = s->next) {
        if (strcmp(s->name, name) == 0)
            return 0; /* already declared in this scope */
    }

    growIfNeeded(current);
    idx = hashStr(name) % (unsigned long)current->nbuckets; /* nbuckets may have just changed */

    Symbol *sym = (Symbol *)malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->type = type;
    sym->next = current->buckets[idx];
    current->buckets[idx] = sym;
    current->count++;
    return 1;
}

Symbol *lookupSymbol(const char *name)
{
    for (Scope *sc = current; sc; sc = sc->parent) {
        unsigned long idx = hashStr(name) % (unsigned long)sc->nbuckets;
        for (Symbol *s = sc->buckets[idx]; s; s = s->next) {
            if (strcmp(s->name, name) == 0)
                return s;
        }
    }
    return NULL;
}

Scope *currentScope(void)
{
    return current;
}
