#define _XOPEN_SOURCE 600
#include "networking.h"
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <stropts.h>
#include <string.h>
#include <stdio.h>
#include <unordered_map>



const static char PID_FILE[] = "/tmp/rshd.pid";


struct rshd_data {
    const static int BUFFER_SIZE = 1500;

    rshd_data(io_service& ios, connection& con) : ios(ios), client_con(con) {
        ptymfd = posix_openpt(O_RDWR);
        grantpt(ptymfd);
        unlockpt(ptymfd);
        ptysfd = open(ptsname(ptymfd), O_RDWR);

        pty_events = EPOLLIN | EPOLLOUT;
        ios.add(ptymfd, pty_events, [this](int event) {
            if (event == EPOLLOUT) {
                printf("pty EPOLLOUT\n");
                if (buf_in.size() == 0) {
                    enable_in(false);
                } else {
                    write(ptymfd, buf_in.data(), buf_in.size());
                    buf_in.clear();
                    client_con.set_read_state(true);
                }
            } else if (event == EPOLLIN) {
                printf("pty EPOLLIN\n");
                if (buf_out.size() != 0) {
                    enable_out(false);
                } else {
                    char buf[1500];
                    int cnt = read(ptymfd, buf, sizeof(buf));
                    buf_out = std::string(buf, cnt);
                    client_con.set_write_state(true);
                }
            }
        });

        fork_shell();
    }

    ~rshd_data() {
        ios.remove(ptymfd);
        kill(shell, SIGKILL);
        close(ptymfd);
    }


    void enable_in(bool new_state) {
        pty_events = (new_state ? pty_events | EPOLLOUT : pty_events & ~EPOLLOUT);
        ios.change(ptymfd, pty_events);
    }

    void enable_out(bool new_state) {
        pty_events = (new_state ? pty_events | EPOLLIN : pty_events & ~EPOLLIN);
        ios.change(ptymfd, pty_events);
    }


// private:
    void fork_shell() {
        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork()");
            exit(errno);
        } else if (child_pid == 0) {
            struct termios slave_orig_term_settings; // Saved terminal settings
            struct termios new_term_settings; // Current terminal settings

            // Close the master side of the PTY
            close(ptymfd);

            // Save the default parameters of the slave side of the PTY
            int rc = tcgetattr(ptysfd, &slave_orig_term_settings);

            // Set raw mode on the slave side of the PTY
            new_term_settings = slave_orig_term_settings;
            cfmakeraw(&new_term_settings);
            tcsetattr(ptysfd, TCSANOW, &new_term_settings);

            // The slave side of the PTY becomes the standard input and outputs of the child process
            close(0); // Close standard input (current terminal)
            close(1); // Close standard output (current terminal)
            close(2); // Close standard error (current terminal)

            dup(ptysfd); // PTY becomes standard input (0)
            dup(ptysfd); // PTY becomes standard output (1)
            dup(ptysfd); // PTY becomes standard error (2)

            setsid();
            ioctl(0, TIOCSCTTY, 1);

            if (execl("/bin/sh", "/bin/sh", 0) == -1) {
                perror("execl()");
                exit(errno);
            }
        } else {
            close(ptysfd);
            shell = child_pid;
        }
    }

    int pty_events;
    int ptymfd, ptysfd;
    std::string buf_in, buf_out;
    pid_t shell;
    io_service &ios;
    connection &client_con; //, &pipe_in, &pipe_out;
};


struct rshd: tcp_server {
    rshd(io_service &ios, int port) : tcp_server(ios, port) {

    }

    virtual ~rshd() = default;

    void on_new_connection(connection& new_con) {
        printf("on_new_connection, sock=%d\n", new_con.get_fd());
        cons.emplace(new_con.get_fd(), new rshd_data(ios, new_con));

        new_con.add_on_read_ready_handler([this](connection& con) {
            printf("%d - read_ready\n", con.get_fd());
            rshd_data* data = cons.find(con.get_fd())->second;
            data->buf_in = con.read();
            con.set_read_state(false);
            data->enable_in(true);
        });

        new_con.add_on_write_ready_handler([this](connection& con) {
           printf("%d - write_ready\n", con.get_fd());
           rshd_data* data = cons.find(con.get_fd())->second;
           con.write(data->buf_out);
           data->buf_out.clear();
           data->enable_out(true);
           con.set_write_state(false);
        });

        new_con.add_on_close_handler([this](connection& con) {
            printf("%d - connection closed\n", con.get_fd());
            auto it = cons.find(con.get_fd());
            delete it->second;
            cons.erase(it);
        });
    }

private:
    std::unordered_map<int, rshd_data*> cons;
};


void daemonize() {
    int fd = open(PID_FILE, O_RDWR|O_CREAT|O_EXCL, 0644);
    if (fd < 0) {
        perror("Cannot create pid file");
        exit(errno);
    }

    int pid = fork();
    if (pid == -1) {
        perror("fork()");
        exit(errno);
    } else if (pid != 0) {
        exit(0);
    }

    if (setsid() < 0) {
        perror("setsid()");
        exit(errno);
    }


    pid = fork();
    if (pid == -1) {
        perror("fork()_2");
        exit(errno);
    } else if (pid != 0) {
        std::string pid_str = std::to_string(pid);
        if (write(fd, pid_str.data(), pid_str.size()) < 0) {
            perror("Cannot write pid to file, kill me manually");
            exit(errno);
        }
        exit(0);
    }

    umask(0);
    chdir("/");

    for (int i = sysconf(_SC_OPEN_MAX) - 1; i >= 0; --i) {
        if (i != fd) {
            close(i);
        }
    }

    int null_fd = open("/dev/null", O_RDWR);
    dup(null_fd);
    dup(null_fd);
}


int main(int argc, char const *argv[]) {
    int port = 12345;
    if (argc > 2) {
        printf("Usage: rshd [port | stop]\n");
        return 0;
    } else if (argc == 2) {
        if (strcmp(argv[1], "stop") == 0) {
            int fd;
            if ((fd = open(PID_FILE, O_RDONLY)) < 0) {
                  perror("Pid file is not found. May be the server is not running?");
                  exit(errno);
            }

            char pid_buf[17];
            int len = read(fd, pid_buf, 16);
            pid_buf[len] = 0;
            kill(atoi(pid_buf), SIGTERM);
            close(fd);
            unlink(PID_FILE);
            return 0;
        } else {
            port = atoi(argv[1]);
        }
    }

    daemonize();
    io_service ios;
    rshd server(ios, port);
    ios.run();

    return 0;
}
