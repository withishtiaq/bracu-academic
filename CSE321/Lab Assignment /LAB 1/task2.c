#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid_child, pid_grandchild;

    pid_child = fork(); 

    if (pid_child < 0) {
        perror("Fork failed");
        exit(1);
    }

    if (pid_child == 0) {
        pid_grandchild = fork();  

        if (pid_grandchild < 0) {
            perror("Fork failed");
            exit(1);
        }

        if (pid_grandchild == 0) {
            printf("I am grandchild\n");
            exit(0);
        } else {
            wait(NULL);
            printf("I am child\n");
            exit(0);
        }
    } else {
        wait(NULL);
        printf("I am parent\n");
    }

    return 0;
}
