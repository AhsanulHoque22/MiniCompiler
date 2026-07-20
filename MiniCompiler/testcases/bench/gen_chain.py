#!/usr/bin/env python3
"""Emits `x = 1+1+1+...+1;` with N terms (left-associative, depth N-1).
Used to test the MAX_EXPR_DEPTH robustness guard (see report Sec. 5.5, 6.6)."""
import sys

n = int(sys.argv[1])
expr = "1" + "+1" * (n - 1)
print("int x;")
print(f"x = {expr};")
print("print(x);")
