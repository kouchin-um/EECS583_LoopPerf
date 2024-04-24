#include <stdio.h>

int sum_to_n(int n) {
    int sum = 0;
    int b = 1;
    for (int i = 0; i < n; i++) {
        b = b * 3;
        b = b / 2;
        sum += i;
    }
    return sum;
}

int main(int argc, char const *argv[]) {
    // FILE *file = fopen("result_sum_to_n_1.txt", "w");
    for (int i = 0; i < 100; ++i) {
        sum_to_n(1000000);
    }
    int res = sum_to_n(1000000);
    // fprintf(file, "%d", res);
    printf("%d\n", res);
    // printf("sum_to_n finished executing \n");
}

