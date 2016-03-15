#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage cat <input file>\n");
        return -1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        printf("Error while opening file!\n");
        return errno;
    }

    char buf[4096];
    size_t cnt;
    while((cnt = read(fd, buf, 4096)) > 0) {
        size_t written = 0;
        while (cnt > 0) {
            written = write(1, &buf[written], cnt);
            cnt -= written;
        }
    }

    close(fd);
    return 0;
}
