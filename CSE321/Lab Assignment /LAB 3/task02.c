#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

struct my_msg {
    long type;
    char txt[12];
};

int main() {
    int msgid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget failed");
        exit(1);
    }

    printf("Please enter the workspace name:\n");
    char workspace[10];
    scanf("%9s", workspace);

    if (strcmp(workspace, "cse321") != 0) {
        printf("Invalid workspace name\n");
        msgctl(msgid, IPC_RMID, NULL);
        exit(0);
    }

    struct my_msg workspace_msg;
    workspace_msg.type = 1;
    strncpy(workspace_msg.txt, workspace, sizeof(workspace_msg.txt) - 1);
    workspace_msg.txt[sizeof(workspace_msg.txt) - 1] = '\0';
    if (msgsnd(msgid, &workspace_msg, sizeof(workspace_msg.txt), 0) == -1) {
        perror("msgsnd failed");
        exit(1);
    }
    printf("Workspace name sent to OTP generator: %s\n", workspace);

    pid_t otp_pid = fork();
    if (otp_pid < 0) {
        perror("fork failed");
        exit(1);
    }

    if (otp_pid == 0) {
        struct my_msg recv_workspace;
        if (msgrcv(msgid, &recv_workspace, sizeof(recv_workspace.txt), 1, 0) == -1) {
            perror("msgrcv failed");
            exit(1);
        }
        printf("OTP generator received workspace: %s\n", recv_workspace.txt);

        int otp = getpid();
        char otp_str[12];
        snprintf(otp_str, sizeof(otp_str), "%d", otp);

        struct my_msg otp_msg_login;
        otp_msg_login.type = 2;
        strncpy(otp_msg_login.txt, otp_str, sizeof(otp_msg_login.txt) - 1);
        otp_msg_login.txt[sizeof(otp_msg_login.txt) - 1] = '\0';
        if (msgsnd(msgid, &otp_msg_login, sizeof(otp_msg_login.txt), 0) == -1) {
            perror("msgsnd failed");
            exit(1);
        }
        printf("OTP sent to log in from OTP generator: %s\n", otp_str);

        pid_t mail_pid = fork();
        if (mail_pid < 0) {
            perror("fork failed");
            exit(1);
        }

        if (mail_pid == 0) {
            struct my_msg recv_mail;
            if (msgrcv(msgid, &recv_mail, sizeof(recv_mail.txt), 3, 0) == -1) {
                perror("msgrcv failed");
                exit(1);
            }
            printf("Mail received OTP from OTP generator: %s\n", recv_mail.txt);

            struct my_msg otp_mail_msg;
            otp_mail_msg.type = 4;
            strncpy(otp_mail_msg.txt, recv_mail.txt, sizeof(otp_mail_msg.txt) - 1);
            otp_mail_msg.txt[sizeof(otp_mail_msg.txt) - 1] = '\0';
            if (msgsnd(msgid, &otp_mail_msg, sizeof(otp_mail_msg.txt), 0) == -1) {
                perror("msgsnd failed");
                exit(1);
            }
            printf("OTP sent to log in from mail: %s\n", recv_mail.txt);
            exit(0);
        } else {
            struct my_msg otp_mail_msg;
            otp_mail_msg.type = 3;
            strncpy(otp_mail_msg.txt, otp_str, sizeof(otp_mail_msg.txt) - 1);
            otp_mail_msg.txt[sizeof(otp_mail_msg.txt) - 1] = '\0';
            if (msgsnd(msgid, &otp_mail_msg, sizeof(otp_mail_msg.txt), 0) == -1) {
                perror("msgsnd failed");
                exit(1);
            }
            printf("OTP sent to mail from OTP generator: %s\n", otp_str);
            wait(NULL);
            exit(0);
        }
    } else {
        wait(NULL);

        struct my_msg recv_otp_generator;
        if (msgrcv(msgid, &recv_otp_generator, sizeof(recv_otp_generator.txt), 2, 0) == -1) {
            perror("msgrcv failed");
            exit(1);
        }
        printf("Log in received OTP from OTP generator: %s\n", recv_otp_generator.txt);

        struct my_msg recv_otp_mail;
        if (msgrcv(msgid, &recv_otp_mail, sizeof(recv_otp_mail.txt), 4, 0) == -1) {
            perror("msgrcv failed");
            exit(1);
        }
        printf("Log in received OTP from mail: %s\n", recv_otp_mail.txt);

        if (strcmp(recv_otp_generator.txt, recv_otp_mail.txt) == 0) {
            printf("OTP Verified\n");
        } else {
            printf("OTP Incorrect\n");
        }

        msgctl(msgid, IPC_RMID, NULL);
    }

    return 0;
}
