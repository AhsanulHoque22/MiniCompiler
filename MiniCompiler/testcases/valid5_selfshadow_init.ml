// A shadowing initializer must read the OUTER variable, not itself.
int x;
x = 10;
{
    int x = x + 1;  // must read outer x (10), giving inner x = 11
    print(x);
}
print(x);
