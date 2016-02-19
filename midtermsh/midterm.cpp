#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <vector>


const char PROMPT[] = "$ \n";
const char NEWLINE = '\n';
const size_t BUFFER_SIZE = 1024;


std::vector<int> launched_pids;


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
}


void print_prompt() {
    int cnt = 0;

    while(true) {
        cnt = write(STDOUT_FILENO, &PROMPT[cnt], sizeof(PROMPT) - cnt);
        if (cnt == -1) {
            if(errno != EINTR) {
                perror("write()");
                exit(errno);
            } else {
                cnt = 0;
            }
        } else if (sizeof(PROMPT) - cnt == 0) {
            break;
        }
    }
}


int main(int argc, char const *argv[]) {
    struct sigaction sa;

    bzero(&sa, sizeof(sa));
    sa.sa_sigaction = sig_handler;
    sa.sa_flags = SA_SIGINFO;

    if (sigaction(SIGINT, &sa, 0) == -1) {
        perror("sigaction()");
        return errno;
    }

    bool g_is_eof = false;
    while(!g_is_eof) {
        print_prompt();

        std::string input_buffer;
        char buf[BUFFER_SIZE];
        while(true) {
            int cnt = read(STDIN_FILENO, buf, BUFFER_SIZE);
            if (cnt == 0) {
                // EOF
                g_is_eof = true;
                break;
            } else if (cnt == -1) {
                if (errno != EINTR) {
                    perror("read");
                    return errno;
                } else {
                    print_prompt();
                    input_buffer.clear();
                    continue;
                }
            }
            input_buffer += {buf, static_cast<size_t>(cnt)};
            if (input_buffer.back() == NEWLINE) {
                break;
            }
        }

        if(g_is_eof) {
            break;
        }

        // Ready to process commands
        std::vector<std::vector<char*>> parsed_data;
        if(!parse_buffer(input_buffer, parsed_data)) {
            continue;
        }

        int *fildes;

        if (parsed_data.size() == 1) {
            create_process(parsed_data[0][0], parsed_data[0].data());
        } else {
            fildes = new int[(parsed_data.size() - 1) * 2];
            for(size_t i = 0; i < parsed_data.size() - 1; ++i) {
                if (pipe2(&fildes[i * 2], O_CLOEXEC) == -1) {
                    perror("pipe2()");
                    return errno;
                }
                // printf("created pipe: %d -> %d\n", fildes[i * 2 + 1], fildes[i * 2]);
            }

            create_process(parsed_data[0][0], parsed_data[0].data(), STDIN_FILENO, fildes[1], fildes[0]);
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

        for(size_t i = 0; i < launched_pids.size(); ++i) {
            waitpid(launched_pids[i], 0, 0);
        }
        launched_pids.clear();

        for(auto it = parsed_data.begin(); it != parsed_data.end(); ++it) {
            for(auto inner_it = it->begin(); inner_it != it->end(); ++inner_it) {
                delete[] *inner_it;
            }
        }
    }
    return 0;
}
