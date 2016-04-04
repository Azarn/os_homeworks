#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>


volatile int g_signo, g_pid;

void sig_handler(int signo, siginfo_t* siginfo, void* ucontext) {
    g_signo = signo;
    g_pid = siginfo->si_pid;
}


int main(int argc, char** argv) {
    struct sigaction sa;
    bzero(&sa, sizeof(sa));
    sa.sa_sigaction = sig_handler;
    sa.sa_flags = SA_SIGINFO;

    if (sigaction(SIGUSR1, &sa, 0) == -1) {
        perror("Cannot catch SIGUSR1");
        return errno;
    }

    if (sigaction(SIGUSR2, &sa, 0) == -1) {
        perror("Cannot catch SIGUSR2");
        return errno;
    }

    if (sigaction(SIGALRM, &sa, 0) == -1) {
        perror("Cannot catch SIGALRM");
        return errno;
    }

    alarm(10);
    pause();
    
    switch(g_signo) {
    case SIGUSR1:
        printf("SIGUSR1 from %d\n", g_pid);
        break;
    case SIGUSR2:
        printf("SIGUSR2 from %d\n", g_pid);
        break;
    case SIGALRM:
        printf("No signals were caught\n");
        break;
    }
    return 0;
}
