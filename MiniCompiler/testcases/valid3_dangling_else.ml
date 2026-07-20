// Dangling else must bind to the nearest unmatched if (the inner one).
int a;
int b;
a = 1;
b = 0;
if (a == 1)
    if (b == 1)
        print(1);
    else
        print(2);
