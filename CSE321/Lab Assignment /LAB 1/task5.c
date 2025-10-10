#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid_child, pid_gc;
    int i;

    printf("1. Parent process ID: %d\n", getpid());

    pid_child = fork();
    if (pid_child < 0) {
        perror("Fork failed");
        return 1;
    }
    if (pid_child == 0) {
        printf("2. Child process ID: %d\n", getpid());

  
        for (i = 0; i < 3; i++) {
            pid_gc = fork();
            if (pid_gc < 0) {
                perror("Fork failed");
                return 1;
            }
            if (pid_gc == 0) {
          
                printf("%d. Grand Child process ID: %d\n", i + 3, getpid());
                return 0; 
            } else {
                wait(NULL); 
            }
        }
        return 0; 
    } else {
        wait(NULL); 
    }

    return 0;
}
