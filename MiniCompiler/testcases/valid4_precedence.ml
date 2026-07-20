// Precedence/associativity: * / bind tighter than + -, which bind tighter
// than < >, which bind tighter than == !=; unary minus binds tightest.
int r;
bool flag;
r = 2 + 3 * 4 - 1;        // 2 + 12 - 1 = 13
flag = 1 + 2 < 10 - 3 == true;  // (1+2) < (10-3) -> 3<7 -> true; true == true -> true
print(r);
print(flag);

int neg;
neg = -2 * -3 + 1;        // (-2)*(-3)+1 = 7
print(neg);
