#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "ast.h"

#define NUM_BUCKETS 211

typedef struct Symbol {
    char *name;
    ValueType type;
    struct Symbol *next;
} Symbol;

typedef struct Scope {
    Symbol *buckets[NUM_BUCKETS];
    struct Scope *parent;
} Scope;

void enterScope(void);
void exitScope(void);

/* Returns 1 on success, 0 if name already declared in the *current* scope. */
int declareSymbol(const char *name, ValueType type);

/* Searches current scope outward to global scope; returns NULL if not found. */
Symbol *lookupSymbol(const char *name);

Scope *currentScope(void);

#endif
