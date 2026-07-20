#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "semantic.h"
#include "codegen.h"

extern FILE *yyin;
extern int yyparse(void);
extern int lexErrorCount;
extern int syntaxErrorCount;
extern ASTNode *programRoot;

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source.ml>\n", argv[0]);
        return 1;
    }

    yyin = fopen(argv[1], "r");
    if (!yyin) {
        fprintf(stderr, "Error: could not open input file '%s'\n", argv[1]);
        return 1;
    }

    yyparse();
    fclose(yyin);

    if (lexErrorCount > 0 || syntaxErrorCount > 0) {
        fprintf(stderr, "Compilation failed: %d lexical error(s), %d syntax error(s).\n",
                lexErrorCount, syntaxErrorCount);
        return 1;
    }

    runSemanticAnalysis(programRoot);
    if (semErrorCount > 0) {
        fprintf(stderr, "Compilation failed: %d semantic error(s).\n", semErrorCount);
        return 1;
    }

    TACList *tac = generateTAC(programRoot);
    optimizeTAC(tac);
    writeTAC(tac, "output.tac");
    generateTargetCode(tac, "output.asm");

    printf("Compilation successful.\n");
    printf("Three-Address Code written to output.tac\n");
    printf("Target code written to output.asm\n");

    return 0;
}
