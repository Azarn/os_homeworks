#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>


int main(int argc, char const *argv[]) {
    int fildes[2];
    if (pipe(fildes) == -1) {
        printf("Error while creating pipe!\n");
        exit(-1);
    }

    int pid_cat = fork();
    int pid_grep = -1;
    int cat_ret, grep_ret;

    if (pid_cat == -1) {
        printf("Error while forking!\n");
    }

    if (pid_cat == 0) {
        close(fildes[0]);
        dup2(fildes[1], STDOUT_FILENO);
        close(fildes[1]);
        execlp("cat", "cat", argv[1], NULL);
    } else {
        pid_grep = fork();
        if (pid_grep == -1) {
            printf("Error while forking!\n");
        }

        if (pid_grep == 0) {
            close(fildes[1]);
            dup2(fildes[0], STDIN_FILENO);
            close(fildes[0]);
            execlp("grep", "grep", "int", NULL);
        } else {
            waitpid(pid_cat, &cat_ret, WEXITED);
            waitpid(pid_grep, &grep_ret, WEXITED);
        }
    }



    return 0;
}
