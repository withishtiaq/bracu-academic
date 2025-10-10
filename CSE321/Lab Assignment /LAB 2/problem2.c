
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *make_fib_sequence(void *input);
void *find_fib_values(void *search_data);

struct search_info {
    int *fib_list;
    int fib_length;
    int *search_positions;
    int num_searches;
};

int main() {
    int user_n;
    printf("Enter n for Fibonacci up to n-th term: ");
    scanf("%d", &user_n);

    pthread_t fib_thread;
    pthread_create(&fib_thread, NULL, make_fib_sequence, &user_n);

    int *fib_list;
    pthread_join(fib_thread, (void **)&fib_list);

    printf("Fibonacci sequence computed.\n");

    int num_searches;
    printf("Enter number of searches: ");
    scanf("%d", &num_searches);

    int *search_positions = static_cast<int*>(malloc(num_searches * sizeof(int)));
    printf("Enter %d search positions: ", num_searches);
    for (int i = 0; i < num_searches; i++) {
        scanf("%d", &search_positions[i]);
    }

    struct search_info info;
    info.fib_list = fib_list;
    info.fib_length = user_n + 1;
    info.search_positions = search_positions;
    info.num_searches = num_searches;

    pthread_t search_thread;
    pthread_create(&search_thread, NULL, find_fib_values, &info);

    int *search_results;
    pthread_join(search_thread, (void **)&search_results);

    printf("Search results:\n");
    for (int i = 0; i < num_searches; i++) {
        printf("%d\n", search_results[i]);
    }

    free(fib_list);
    free(search_positions);
    free(search_results);

    return 0;
}

void *make_fib_sequence(void *input) {
    int n = *(int *)input;
    int *fib = static_cast<int*>(malloc((n + 1) * sizeof(int)));
    if (n >= 0) fib[0] = 0;
    if (n >= 1) fib[1] = 1;
    for (int i = 2; i <= n; i++) {
        fib[i] = fib[i - 1] + fib[i - 2];
    }
    return fib;
}

void *find_fib_values(void *search_data) {
    struct search_info *info = (struct search_info *)search_data;
    int *results = static_cast<int*>(malloc(info->num_searches * sizeof(int)));
    for (int i = 0; i < info->num_searches; i++) {
        int pos = info->search_positions[i];
        if (pos < 0 || pos >= info->fib_length) {
            results[i] = -1;
        } else {
            results[i] = info->fib_list[pos];
        }
    }
    return results;
}