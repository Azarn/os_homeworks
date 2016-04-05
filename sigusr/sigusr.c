#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>


volatile sig_atomic_t g_signo, g_pid;

void sig_handler(int signo, siginfo_t* siginfo, void* ucontext) {
    g_signo = signo;
    g_pid = siginfo->si_pid;
}


int main(int argc, char** argv) {
    struct sigaction sa;
    sigset_t mask;
    
    bzero(&sa, sizeof(sa));
    sa.sa_sigaction = sig_handler;
    sa.sa_flags = SA_SIGINFO;

    sigemptyset (&mask);
    sigaddset (&mask, SIGUSR1);
    sigaddset (&mask, SIGUSR2);
    sigaddset (&mask, SIGALRM);
    sa.sa_mask = mask;

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
