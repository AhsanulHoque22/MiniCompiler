%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern int yylineno;
extern int yylex(void);
extern FILE *yyin;
extern int lexErrorCount;

int syntaxErrorCount = 0;
ASTNode *programRoot = NULL;

void yyerror(const char *s);
%}

%union {
    int ival;
    char *sval;
    struct ASTNode *node;
}

%token <sval> IDENT
%token <ival> INT_CONST BOOL_CONST
%token INT_KW BOOL_KW IF ELSE WHILE PRINT
%token ASSIGN EQ NE LT GT PLUS MINUS STAR SLASH
%token LPAREN RPAREN LBRACE RBRACE SEMI COMMA

%type <node> program stmt_list stmt declaration assignment if_stmt while_stmt print_stmt block expr
%type <ival> type

%left EQ NE
%left LT GT
%left PLUS MINUS
%left STAR SLASH
%right UMINUS
%nonassoc IFX
%nonassoc ELSE

%define parse.error verbose

%%

program:
      stmt_list { programRoot = reverseStmtList($1); }
    ;

/* Left-recursive by design: a right-recursive stmt_list (stmt stmt_list)
   would force Bison to hold every statement on its parse stack until the
   final reduce, giving O(n) parser-stack depth for a plain sequence of n
   statements - not just for genuinely nested constructs. Left recursion
   lets Bison reduce as it goes (O(1) stack per statement), at the cost of
   building the list in reverse; reverseStmtList() restores source order
   in one O(n) pass wherever a stmt_list is finalized (here and in block). */
stmt_list:
      /* empty */      { $$ = NULL; }
    | stmt_list stmt    { $$ = newStmtList($2, $1, yylineno); }
    ;

stmt:
      declaration
    | assignment
    | if_stmt
    | while_stmt
    | print_stmt
    | block
    ;

type:
      INT_KW  { $$ = TYPE_INT; }
    | BOOL_KW { $$ = TYPE_BOOL; }
    ;

declaration:
      type IDENT SEMI               { $$ = newVarDecl($1, $2, NULL, yylineno); }
    | type IDENT ASSIGN expr SEMI   { $$ = newVarDecl($1, $2, $4, yylineno); }
    ;

assignment:
      IDENT ASSIGN expr SEMI { $$ = newAssign($1, $3, yylineno); }
    ;

if_stmt:
      IF LPAREN expr RPAREN stmt %prec IFX      { $$ = newIf($3, $5, NULL, yylineno); }
    | IF LPAREN expr RPAREN stmt ELSE stmt      { $$ = newIf($3, $5, $7, yylineno); }
    ;

while_stmt:
      WHILE LPAREN expr RPAREN stmt { $$ = newWhile($3, $5, yylineno); }
    ;

print_stmt:
      PRINT LPAREN expr RPAREN SEMI { $$ = newPrint($3, yylineno); }
    ;

block:
      LBRACE stmt_list RBRACE { $$ = newBlock(reverseStmtList($2), yylineno); }
    ;

expr:
      expr PLUS expr    { $$ = newBinOp(OP_ADD, $1, $3, yylineno); }
    | expr MINUS expr   { $$ = newBinOp(OP_SUB, $1, $3, yylineno); }
    | expr STAR expr    { $$ = newBinOp(OP_MUL, $1, $3, yylineno); }
    | expr SLASH expr   { $$ = newBinOp(OP_DIV, $1, $3, yylineno); }
    | expr LT expr      { $$ = newBinOp(OP_LT, $1, $3, yylineno); }
    | expr GT expr      { $$ = newBinOp(OP_GT, $1, $3, yylineno); }
    | expr EQ expr      { $$ = newBinOp(OP_EQ, $1, $3, yylineno); }
    | expr NE expr      { $$ = newBinOp(OP_NE, $1, $3, yylineno); }
    | MINUS expr %prec UMINUS { $$ = newUnaryMinus($2, yylineno); }
    | LPAREN expr RPAREN { $$ = $2; }
    | IDENT              { $$ = newIdent($1, yylineno); }
    | INT_CONST          { $$ = newIntLit($1, yylineno); }
    | BOOL_CONST         { $$ = newBoolLit($1, yylineno); }
    ;

%%

void yyerror(const char *s)
{
    fprintf(stderr, "Syntax Error at line %d: %s\n", yylineno, s);
    syntaxErrorCount++;
}
