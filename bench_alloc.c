#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ITERATIONS 1000000
#define SIZE 256

void baseline() {
    volatile int sink = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        char *ptr = malloc(SIZE);
        if (ptr) {
            memset(ptr, 'a', SIZE - 1);
            ptr[SIZE - 1] = '\0';
            sink += ptr[0];
            free(ptr);
        }
    }
}

void optimized() {
    volatile int sink = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        char ptr[SIZE];
        memset(ptr, 'a', SIZE - 1);
        ptr[SIZE - 1] = '\0';
        sink += ptr[0];
    }
}

int main() {
    clock_t start, end;
    double cpu_time_used;

    start = clock();
    baseline();
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Baseline (malloc/free): %f seconds\n", cpu_time_used);

    start = clock();
    optimized();
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Optimized (stack): %f seconds\n", cpu_time_used);

    return 0;
}
