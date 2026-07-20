// Division by a variable that happens to be zero must NOT be constant-folded
// away (only literal/literal division is folded); it must survive to codegen
// as a genuine runtime division.
int a;
int z;
int r;
a = 10;
z = 0;
r = a / z;
print(r);
