#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"

extern int semErrorCount;

void runSemanticAnalysis(ASTNode *program);

#endif
