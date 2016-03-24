#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>


void sig_handler(int signo) {
    if (signo == SIGUSR1) {
        perror("Catched SIGUSR1\n");
    }
}


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage cat <input file>\n");
        return -1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Error while opening file!\n");
        return errno;
    }

    if (signal(SIGUSR1, sig_handler) == SIG_ERR) {
        perror("Cannot catch SIGUSR1");
        return errno;
    }

    char buf[4096];
    size_t cnt;
    while(1) {
        cnt = read(fd, buf, 4096);
        if (cnt == 0) {
            break;
        } else if (cnt == -1) {
            if (errno != EINTR) {
                perror("Read error occured\n");
                return errno;
            } else {
                perror("Hello from SIGNAL on read");
            }
            continue;
        }

        size_t written = 0;
        while (cnt > 0) {
            written = write(1, &buf[written], cnt);
            if (written == -1) {
                if (errno != EINTR) {
                    perror("Write error occured!\n");
                    return errno;
                } else {
                    perror("Hello from SIGNAL on write");
                }
                continue;
            }
            cnt -= written;
        }
    }

    close(fd);
    return 0;
}
