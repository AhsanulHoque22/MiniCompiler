#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

typedef enum {
    TAC_ASSIGN, /* result = arg1                */
    TAC_BINOP,  /* result = arg1 op arg2        */
    TAC_UNOP,   /* result = op arg1             */
    TAC_LABEL,  /* result:                      */
    TAC_GOTO,   /* goto result                  */
    TAC_IFZ,    /* ifz arg1 goto result         */
    TAC_PRINT   /* print arg1                   */
} TACKind;

typedef struct {
    TACKind kind;
    char *op;      /* operator text for BINOP/UNOP, NULL otherwise */
    char *arg1;
    char *arg2;    /* BINOP only, NULL otherwise */
    char *result;  /* destination name, or label name for LABEL/GOTO/IFZ */
    int dead;
} TACInstr;

typedef struct {
    TACInstr *instrs;
    int count;
    int capacity;
} TACList;

TACList *generateTAC(ASTNode *program);
void optimizeTAC(TACList *list);
void writeTAC(const TACList *list, const char *filename);
void generateTargetCode(const TACList *list, const char *filename);

#endif
