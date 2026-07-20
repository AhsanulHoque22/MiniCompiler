// Stress test: many chained binary operators, deep nesting, loops within loops,
// to exercise the optimizer passes with a large TAC instruction count.
int a; int b; int c; int d; int e; int f; int g; int h;
int i; int j; int k; int total;
bool cond;

a = 1; b = 2; c = 3; d = 4; e = 5; f = 6; g = 7; h = 8;
total = a + b * c - d / e + f * g - h + a * b * c * d - e + f - g + h;
total = total + (a + b) * (c - d) / (e + 1) - (f - g) * h + a - b + c - d + e - f + g - h;
cond = total > 0;

i = 0;
total = 0;
while (i < 5) {
    j = 0;
    while (j < 5) {
        k = i * 5 + j;
        if (k == 12) {
            total = total + 100;
        } else {
            if (k > 20) {
                total = total + k * 2 - 1;
            } else {
                total = total + k;
            }
        }
        j = j + 1;
    }
    i = i + 1;
}
print(total);

{
    int a;
    a = 100;
    {
        int a;
        a = 200;
        {
            int a;
            a = 300;
            {
                int a;
                a = 400;
                print(a);
            }
            print(a);
        }
        print(a);
    }
    print(a);
}
print(a);
