#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string>
#include <string.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <poll.h>
#include <vector>
#include <list>


#define MAX_EVENTS 1000

const char PROMPT[] = "$ ";
const char NEWLINE = '\n';
const size_t BUFFER_SIZE = 1024;


void stdin_available(int event);
void print_prompt();
void safe_write(int fd, const char* buf, size_t size);
void load_command();
void put_stdin_to_pipe();

std::list<int> launched_pids;
std::vector<std::vector<char*>> parsed_data;
bool is_terminating, is_stdin_eof = false;
int epoll_fd;
std::string input_buffer;
int global_pipe[2];


char* create_from_string(std::string const& str) {
    char* res = new char[str.size() + 1];
    memcpy(res, str.data(), str.size());
    res[str.size()] = 0;
    return res;
}


bool parse_buffer(std::string const& input_buffer, std::vector<std::vector<char*>> &out) {
    auto it = input_buffer.begin();
    std::string arg;
    std::vector<char*> args;

    while(*it != '\n') {
        if(*it == ' ') {
            if(!arg.empty()) {
                args.push_back(create_from_string(arg));
                arg.clear();
            }
        } else if (*it == '|') {
            if(!arg.empty()) {
                args.push_back(create_from_string(arg));
                arg.clear();
            }

            if(args.empty()) {
                return false;
            }

            args.push_back(0);
            out.push_back(args);
            args.clear();
            arg.clear();
        } else {
            arg += *it;
        }
        ++it;
    }

    if(!arg.empty()) {
        args.push_back(create_from_string(arg));
    }

    if(!args.empty()) {
        args.push_back(0);
        out.push_back(args);
    }

    return !args.empty();
}


void create_process(const char* file, char* const* argv, int in_fd=STDIN_FILENO, int out_fd=STDOUT_FILENO, int close_in_child=-1) {
    // printf("proc %s will read from %d and write to %d\n", file, in_fd, out_fd);
    int child_pid = fork();
    if (child_pid == -1) {
        perror("fork()");
        exit(errno);
    } else if (child_pid == 0) {
        if (close_in_child != -1) {
            close(close_in_child);
        }

        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }

        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }

        if(execvp(file, argv) == -1) {
            perror("execvp()");
            exit(errno);
        }
    } else {
        launched_pids.push_back(child_pid);
    }
}


void sig_handler(int signo, siginfo_t* siginfo, void* ucontext) {
    for(auto it = launched_pids.begin(); it != launched_pids.end(); ++it) {
        kill(*it, SIGKILL);
    }
    write(STDIN_FILENO, "\n", 1);

    if (launched_pids.size() == 0) {
        print_prompt();
    }
}


void print_prompt() {
    safe_write(STDOUT_FILENO, PROMPT, sizeof(PROMPT));
}


void add_to_epoll(int fd, int events) {
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}


void remove_from_epoll(int fd) {
    epoll_event ev;     // for kernel before 2.6.9 support
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev);
}


int main(int argc, char const *argv[]) {
    epoll_event events[MAX_EVENTS];
    struct sigaction sa;

    bzero(&sa, sizeof(sa));
    sa.sa_sigaction = sig_handler;
    sa.sa_flags = SA_SIGINFO;

    if (sigaction(SIGINT, &sa, 0) == -1) {
        perror("sigaction()");
        return errno;
    }

    if (pipe2(global_pipe, O_CLOEXEC) == -1) {
        perror("pipe()");
        return errno;
    }

    print_prompt();

    is_terminating = false;
    epoll_fd = epoll_create(1);
    add_to_epoll(STDIN_FILENO, EPOLLIN);

    while(!is_terminating || launched_pids.size() != 0) {
        int num_ev = epoll_wait(epoll_fd, events, MAX_EVENTS, 100);
        if (num_ev == -1) {
            if (errno != EINTR) {
                perror("epoll_wait1");
                exit(errno);
            } else {
                continue;
            }
        }

        if (launched_pids.size() != 0) {
            auto it = launched_pids.begin();
            while (it != launched_pids.end()) {
                int status;
                pid_t return_pid = waitpid(*it, &status, WNOHANG);
                if (return_pid == *it) {
                    it = launched_pids.erase(it);
                } else {
                    ++it;
                }
            }

            if (is_stdin_eof) {
                struct pollfd fd;
                fd.fd = global_pipe[0];
                fd.events = POLLIN;
                if (poll(&fd, 1, 0) == 0) {
                    close(global_pipe[1]);
                }
            }

            if (launched_pids.size() == 0) {
                for(auto it = parsed_data.begin(); it != parsed_data.end(); ++it) {
                    for(auto inner_it = it->begin(); inner_it != it->end(); ++inner_it) {
                        delete[] *inner_it;
                    }
                }

                print_prompt();
                input_buffer.clear();

                if (is_stdin_eof) {
                    add_to_epoll(STDIN_FILENO, EPOLLIN);
                }
            }
        }

        for (int i = 0; i < num_ev; ++i) {
            // printf("event -> %d, %d\n", events[i].data.fd, events[i].events);
            if (launched_pids.size() == 0) {
                load_command();
            } else if (events[i].data.fd == STDIN_FILENO && !is_stdin_eof) {
                put_stdin_to_pipe();
            }
        }
    }

    close(epoll_fd);
    return 0;
}


void safe_write(int fd, const char* buf, size_t size) {
    int cnt = 0;
    while(true) {
        cnt = write(fd, &buf[cnt], size - cnt);
        if (cnt == -1) {
            if(errno != EINTR) {
                perror("write()");
                exit(errno);
            } else {
                cnt = 0;
            }
        } else if (size - cnt == 0) {
            break;
        }
    }
}


int safe_read(int fd, char* buf, size_t size, bool is_non_block=false) {
    while(true) {
        int cnt = read(fd, buf, size);
        if (cnt == -1) {
            if (is_non_block && errno == EAGAIN) {
                return cnt;
            } else if (errno != EINTR) {
                perror("read");
                exit(errno);
            } else {
                continue;
            }
        }
        return cnt;
    }
}


void put_stdin_to_pipe() {
    char buf[BUFFER_SIZE];
    int cnt = safe_read(STDIN_FILENO, buf, BUFFER_SIZE);
    if (cnt == 0) {
        // printf("read -> 0\n");
        is_stdin_eof = true;
        remove_from_epoll(STDIN_FILENO);
        return;
    }

    safe_write(global_pipe[1], buf, BUFFER_SIZE);
}

void load_command() {
    parsed_data.clear();
    char buf[BUFFER_SIZE];

    fcntl(global_pipe[0], F_SETFL, O_NONBLOCK);
    int cnt = safe_read(global_pipe[0], buf, BUFFER_SIZE, true);
    fcntl(global_pipe[0], F_SETFL, 0);
    if (cnt != -1) {
        input_buffer += {buf, static_cast<size_t>(cnt)};
    }

    // printf("cnt: %d\n", cnt);

    cnt = safe_read(STDIN_FILENO, buf, BUFFER_SIZE);
    if (cnt == 0) {
        if (input_buffer.size() == 0) {
            // printf("terminating...\n");
            is_terminating = true;
            remove_from_epoll(global_pipe[0]);
            close(global_pipe[0]);
            close(global_pipe[1]);
            return;
        }
    }

    input_buffer += {buf, static_cast<size_t>(cnt)};

    size_t pos = input_buffer.find(NEWLINE);
    if (pos == std::string::npos) {
        return;
    }

    if (pos == 0) {
        input_buffer.clear();
        print_prompt();
        return;
    }

    std::string extra_data {input_buffer, pos + 1};
    input_buffer.resize(pos + 1);
    if (extra_data.size() > 0) {
        safe_write(global_pipe[1], extra_data.c_str(), extra_data.size());
    }

    // printf("input_buffer: '%s'\n", input_buffer.c_str());
    // printf("extra_data: '%s'\n", extra_data.c_str());

    // Ready to process commands
    if(!parse_buffer(input_buffer, parsed_data)) {
        return;
    }

    int *fildes;

    if (parsed_data.size() == 1) {
        create_process(parsed_data[0][0], parsed_data[0].data(), global_pipe[0]);
    } else {
        fildes = new int[(parsed_data.size() - 1) * 2];
        for(size_t i = 0; i < parsed_data.size() - 1; ++i) {
            if (pipe2(&fildes[i * 2], O_CLOEXEC) == -1) {
                perror("pipe2()");
                exit(errno);
            }
        }

        create_process(parsed_data[0][0], parsed_data[0].data(), global_pipe[0], fildes[1], fildes[0]);
        for(size_t i = 1; i < parsed_data.size(); ++i) {
            if(i + 1 == parsed_data.size()) {
                create_process(parsed_data[i][0], parsed_data[i].data(), fildes[(i - 1) * 2], STDOUT_FILENO, fildes[i * 2 - 1]);
            } else {
                create_process(parsed_data[i][0], parsed_data[i].data(), fildes[(i - 1) * 2], fildes[(i * 2 + 1)]);
            }
        }
    }

    for(size_t i = 0; i < (parsed_data.size() - 1) * 2; ++i) {
        close(fildes[i]);
        if (i + 1 == (parsed_data.size() - 1) * 2) {
            delete[] fildes;
        }
    }

    if (is_stdin_eof) {
        remove_from_epoll(STDIN_FILENO);
    }
}
