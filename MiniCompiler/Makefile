CC      = gcc
CFLAGS  = -Wall -Wextra -g
TARGET  = minicompiler

# Explicit source list (deterministic build).
#
# The assignment's example build command is:
#   gcc lex.yy.c parser.tab.c *.c -o minicompiler
# On a normal shell this silently double-compiles lex.yy.c and parser.tab.c,
# because the *.c glob matches those two generated files a second time after
# they are already named explicitly - producing duplicate-symbol link errors
# (duplicate main, yylex, ...) on modern GCC. Listing every source file
# explicitly avoids that while still using exactly the three required tools
# (flex, bison -d, gcc) in the same roles.
SRCS    = lex.yy.c parser.tab.c ast.c symbol_table.c semantic.c codegen.c main.c

.PHONY: all clean

all: $(TARGET)

parser.tab.c parser.tab.h: parser.y
	bison -d parser.y

lex.yy.c: lexer.l parser.tab.h
	flex lexer.l

$(TARGET): $(SRCS) ast.h symbol_table.h semantic.h codegen.h
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) lex.yy.c parser.tab.c parser.tab.h *.o output.tac output.asm
