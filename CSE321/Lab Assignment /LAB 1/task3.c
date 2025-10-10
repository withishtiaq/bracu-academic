#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t a, b, c;
    int process_count = 1; 

    a = fork();
    if (a > 0) process_count++; 

    if (a > 0 && (a % 2 == 1)) { 
        if (fork() == 0) {
            execlp("echo", "echo", "Extra child from odd PID", NULL);
        }
        process_count++;
    }

    b = fork();
    if (b > 0) process_count++;

    if (b > 0 && (b % 2 == 1)) {
        if (fork() == 0) {
            execlp("echo", "echo", "Extra child from odd PID", NULL);
        }
        process_count++;
    }

    c = fork();
    if (c > 0) process_count++;

    if (c > 0 && (c % 2 == 1)) {
        if (fork() == 0) {
            execlp("echo", "echo", "Extra child from odd PID", NULL);
        }
        process_count++;
    }

 
    while (wait(NULL) > 0);

    printf("Total processes created (including first parent): %d\n", process_count);

    return 0;
}
