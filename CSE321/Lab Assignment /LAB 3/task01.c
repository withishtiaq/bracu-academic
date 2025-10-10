#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

struct bankData {
    char option[100];
    int balance;
};

int main() {
    int shmid = shmget(IPC_PRIVATE, sizeof(struct bankData), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    struct bankData *shared_mem = (struct bankData *)shmat(shmid, NULL, 0);
    if (shared_mem == (void *)-1) {
        perror("shmat failed");
        exit(1);
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe failed");
        exit(1);
    }

    printf("Provide Your Input From Given Options:\n");
    printf("1. Type a to Add Money\n");
    printf("2. Type w to Withdraw Money\n");
    printf("3. Type c to Check Balance\n");

    scanf("%s", shared_mem->option);
    shared_mem->balance = 1000;
    printf("Your selection: %s\n", shared_mem->option);

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork failed");
        exit(1);
    }

    if (child_pid == 0) {
        char choice = shared_mem->option[0];
        int current_balance = shared_mem->balance;

        if (choice == 'a') {
            printf("Enter amount to be added:\n");
            int deposit;
            scanf("%d", &deposit);
            if (deposit > 0) {
                current_balance += deposit;
                shared_mem->balance = current_balance;
                printf("Balance added successfully\n");
                printf("Updated balance after addition:\n%d\n", current_balance);
            } else {
                printf("Adding failed, Invalid amount\n");
            }
        } else if (choice == 'w') {
            printf("Enter amount to be withdrawn:\n");
            int withdraw;
            scanf("%d", &withdraw);
            if (withdraw > 0 && withdraw <= current_balance) {
                current_balance -= withdraw;
                shared_mem->balance = current_balance;
                printf("Balance withdrawn successfully\n");
                printf("Updated balance after withdrawal:\n%d\n", current_balance);
            } else {
                printf("Withdrawal failed, Invalid amount\n");
            }
        } else if (choice == 'c') {
            printf("Your current balance is:\n%d\n", current_balance);
        } else {
            printf("Invalid selection\n");
        }

        close(pipe_fd[0]);
        write(pipe_fd[1], "Thank you for using", 20);
        close(pipe_fd[1]);

        shmdt(shared_mem);
        exit(0);
    } else {
        wait(NULL);
        close(pipe_fd[1]);
        char message[100];
        read(pipe_fd[0], message, 100);
        printf("%s\n", message);
        close(pipe_fd[0]);

        shmdt(shared_mem);
        shmctl(shmid, IPC_RMID, NULL);
    }

    return 0;
}
