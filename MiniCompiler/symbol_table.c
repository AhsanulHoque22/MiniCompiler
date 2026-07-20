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
    memset(s->buckets, 0, sizeof(s->buckets));
    s->parent = current;
    current = s;
}

void exitScope(void)
{
    if (!current) return;
    Scope *dead = current;
    current = current->parent;

    for (int i = 0; i < NUM_BUCKETS; i++) {
        Symbol *sym = dead->buckets[i];
        while (sym) {
            Symbol *next = sym->next;
            free(sym->name);
            free(sym);
            sym = next;
        }
    }
    free(dead);
}

int declareSymbol(const char *name, ValueType type)
{
    if (!current) enterScope(); /* safety net: should not normally happen */

    unsigned long idx = hashStr(name) % NUM_BUCKETS;
    for (Symbol *s = current->buckets[idx]; s; s = s->next) {
        if (strcmp(s->name, name) == 0)
            return 0; /* already declared in this scope */
    }

    Symbol *sym = (Symbol *)malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->type = type;
    sym->next = current->buckets[idx];
    current->buckets[idx] = sym;
    return 1;
}

Symbol *lookupSymbol(const char *name)
{
    unsigned long idx = hashStr(name) % NUM_BUCKETS;
    for (Scope *sc = current; sc; sc = sc->parent) {
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
