#include <stdio.h>

int main() {
    int res = 0;

    for (int i = 0; i < 10000; ++i) {
        res += i * i * i;  // Increment res by i
        res -= i;
        for (int j = 0; j < 1000; ++j) {
            res *= j;  // Multiply res by j
            for (int k = 0; k < 1000; ++k) {
                res -= k * k / 2;  // Subtract k from res
            }
        }
    }

    printf("%d\n", res);
    return 0;
}
