#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int compare(const void *a, const void *b) {
    return (*(int*)b - *(int*)a); 
}

int main() {
    int arr[] = {5, 2, 9, 1, 7};
    int n = sizeof(arr) / sizeof(arr[0]);

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return 1;
    }
    else if (pid == 0) {
        qsort(arr, n, sizeof(int), compare);
        printf("Sorted Array (Child): ");
        for (int i = 0; i < n; i++) {
            printf("%d ", arr[i]);
        }
        printf("\n");
        exit(0);
    }
    else {
        wait(NULL);

        printf("Odd/Even Status (Parent):\n");
        for (int i = 0; i < n; i++) {
            if (arr[i] % 2 == 0)
                printf("%d Even\n", arr[i]);
            else
                printf("%d Odd\n", arr[i]);
        }
    }

    return 0;
}
