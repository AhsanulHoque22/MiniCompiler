// Exercises every supported MiniLang construct.
int a;
int b;
int sum;
bool flag;

a = 5;
b = 10;
sum = a + b * 2;
flag = sum > 20;

if (flag) {
    print(sum);
} else {
    print(0);
}

int i;
i = 0;
while (i < 3) {
    print(i);
    i = i + 1;
}
