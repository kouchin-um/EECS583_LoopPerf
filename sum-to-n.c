#include <stdio.h>

int sum_to_n(int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += i;
    }
    return sum;
}

int main(int argc, char const *argv[]) {
    FILE *file = fopen("result_sum_to_n_1.txt", "w");
    int res = sum_to_n(5);
    fprintf(file, "%d", res);
    printf("sum_to_n finished executing \n");
}

