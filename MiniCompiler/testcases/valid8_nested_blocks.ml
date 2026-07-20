// Three levels of shadowing: x -> x$2 -> x$3.
int x;
x = 1;
{
    int x;
    x = 2;
    {
        int x;
        x = 3;
        print(x);
    }
    print(x);
}
print(x);
