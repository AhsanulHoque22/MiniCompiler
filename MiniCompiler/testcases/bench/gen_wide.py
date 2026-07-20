#!/usr/bin/env python3
"""Emits N sequential, shallow statements (int x1; x1 = x0 + 1; ...).

Each statement's expression is a single BINOP (constant depth), so this
never trips the MAX_EXPR_DEPTH guard regardless of N - unlike gen_chain.py,
which builds one deeply *nested* expression. This isolates the TAC
optimizer's per-instruction cost (Sec. 6.4/6.5) from the recursion-depth
robustness guard (Sec. 5.5), letting the optimizer's O(m) scaling be
measured on programs with tens of thousands of TAC instructions."""
import sys

n = int(sys.argv[1])
print("int x0;")
print("x0 = 1;")
for i in range(1, n):
    print(f"int x{i};")
    print(f"x{i} = x{i - 1} + 1;")
print(f"print(x{n - 1});")
