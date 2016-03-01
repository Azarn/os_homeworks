#include <sys/types.h>
#include <unistd.h>


int main(int argc, char** argv) {
    char buf[4096];
    size_t cnt;
    while((cnt = read(0, buf, 4096)) > 0) {
        size_t written = 0;
        while (cnt > 0) {
            written = write(1, &buf[written], cnt);
            cnt -= written;
        }
    }
    return 0;
}
