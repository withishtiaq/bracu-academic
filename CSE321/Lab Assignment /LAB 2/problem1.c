#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
    int *fib;
    int len;
    int *queries;
    int qcount;
} pkg;

void *build_fib(void *arg);
void *search_fib(void *arg);

int main(void) {
    int n;
    if (printf("Enter the term of fibonacci sequence: "), fflush(stdout), scanf("%d", &n) != 1) return 1;
    if (n < 0) return 1;

    pthread_t th1;
    if (pthread_create(&th1, NULL, build_fib, &n) != 0) return 1;

    int *fibarr = NULL;
    pthread_join(th1, (void **)&fibarr);

    for (int i = 0; i <= n; i++) printf("a[%d] = %d\n", i, fibarr[i]);

    int s;
    if (printf("How many numbers you are willing to search?: "), fflush(stdout), scanf("%d", &s) != 1 || s <= 0) {
        free(fibarr);
        return 1;
    }

    int *queries = malloc(s * sizeof(int));
    if (!queries) { free(fibarr); return 1; }

    for (int i = 0; i < s; i++) {
        printf("Enter search %d: ", i + 1);
        if (scanf("%d", &queries[i]) != 1) { free(fibarr); free(queries); return 1; }
    }

    pkg pack;
    pack.fib = fibarr;
    pack.len = n + 1;
    pack.queries = queries;
    pack.qcount = s;

    pthread_t th2;
    if (pthread_create(&th2, NULL, search_fib, &pack) != 0) { free(fibarr); free(queries); return 1; }

    int *results = NULL;
    pthread_join(th2, (void **)&results);

    for (int i = 0; i < s; i++) printf("result of search #%d = %d\n", i + 1, results[i]);

    free(fibarr);
    free(queries);
    free(results);
    return 0;
}

void *build_fib(void *arg) {
    int n = *(int *)arg;
    int len = n + 1;
    int *arr = malloc(len * sizeof(int));
    if (!arr) pthread_exit(NULL);
    if (len >= 1) arr[0] = 0;
    if (len >= 2) arr[1] = 1;
    for (int i = 2; i < len; i++) arr[i] = arr[i - 1] + arr[i - 2];
    return arr;
}

void *search_fib(void *arg) {
    pkg *p = (pkg *)arg;
    int *out = malloc(p->qcount * sizeof(int));
    if (!out) pthread_exit(NULL);
    for (int i = 0; i < p->qcount; i++) {
        int pos = p->queries[i];
        if (pos < 0 || pos >= p->len) out[i] = -1;
        else out[i] = p->fib[pos];
    }
    return out;
}