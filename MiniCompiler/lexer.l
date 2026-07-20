%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "parser.tab.h"

int lexErrorCount = 0;
%}

%option yylineno
%option noyywrap

%x COMMENT

DIGIT  [0-9]
LETTER [a-zA-Z_]

%%

"//"[^\n]*              { /* line comment: discard */ }

"/*"                     { BEGIN(COMMENT); }
<COMMENT>"*/"            { BEGIN(INITIAL); }
<COMMENT>\n              { /* consumed; yylineno auto-advances */ }
<COMMENT>.               { /* consume block-comment body */ }
<COMMENT><<EOF>>         {
                            fprintf(stderr, "Lexical Error at line %d: unterminated comment\n", yylineno);
                            lexErrorCount++;
                            yyterminate();
                          }

[ \t\r]+                 { /* whitespace: discard */ }
\n                        { /* newline: discard, yylineno auto-advances */ }

"int"                    { return INT_KW; }
"bool"                   { return BOOL_KW; }
"if"                     { return IF; }
"else"                   { return ELSE; }
"while"                  { return WHILE; }
"print"                  { return PRINT; }
"true"                   { yylval.ival = 1; return BOOL_CONST; }
"false"                  { yylval.ival = 0; return BOOL_CONST; }

{LETTER}({LETTER}|{DIGIT})* { yylval.sval = strdup(yytext); return IDENT; }
{DIGIT}+                  { yylval.ival = atoi(yytext); return INT_CONST; }

"=="                     { return EQ; }
"!="                     { return NE; }
"<"                      { return LT; }
">"                      { return GT; }
"="                      { return ASSIGN; }
"+"                      { return PLUS; }
"-"                      { return MINUS; }
"*"                      { return STAR; }
"/"                      { return SLASH; }
"("                      { return LPAREN; }
")"                      { return RPAREN; }
"{"                      { return LBRACE; }
"}"                      { return RBRACE; }
";"                      { return SEMI; }
","                      { return COMMA; }

.                        {
                            fprintf(stderr, "Lexical Error at line %d: unexpected character '%c'\n", yylineno, yytext[0]);
                            lexErrorCount++;
                          }

%%
