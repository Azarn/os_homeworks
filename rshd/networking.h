#ifndef NETWORKING_H
#define NETWORKING_H

#include <stdio.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include <string>
#include <errno.h>


struct tcp_server;


struct io_service {
    typedef std::function<void(int)> iofunc_t;

    io_service() {
        epoll_fd = epoll_create(1);
        // TODO: check for an error
        is_terminating = false;
    };

    io_service(io_service const& other) = delete;

    ~io_service() {
        printf("close(), %d\n", epoll_fd);
        close(epoll_fd);
    }

    io_service& operator=(io_service&& other) {
        std::swap(epoll_fd, other.epoll_fd);
        std::swap(is_terminating, other.is_terminating);
        std::swap(handlers, other.handlers);
        return *this;
    }

    void add(int sock, int events, iofunc_t func) {
        printf("adding to epoll, fd=%d, events=%d\n", sock, events);
        epoll_event ev;
        ev.events = events;
        ev.data.fd = sock;
        handlers.emplace(sock, func);
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev);
    }

    void change(int sock, int events) {
        printf("changing epoll, fd=%d, events=%d\n", sock, events);
        epoll_event ev;
        ev.events = events;
        ev.data.fd = sock;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock, &ev);
    }

    void remove(int sock) {
        epoll_event ev;     // for kernel before 2.6.9 support
        handlers.erase(sock);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock, &ev);
    }

    void run() {
        #define MAX_EVENTS 1000
        epoll_event events[MAX_EVENTS];
        while(!is_terminating) {
            // printf("loop\n");
            int num_ev = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
            if (num_ev == -1) {
                perror("epoll_wait1");
                exit(errno);
            }
            for (int i = 0; i < num_ev; ++i) {
                printf("events for fd=%d, events=%d\n", events[i].data.fd, events[i].events);
                handlers[events[i].data.fd](events[i].events);
            }
        }
    }

    void stop() {
        is_terminating = true;
    }

private:
    int epoll_fd;
    bool is_terminating;
    std::unordered_map<int, iofunc_t> handlers;
};


struct connection {
    typedef std::function<void(connection&)> confunc_t;
    friend tcp_server;

    friend bool operator==(connection const& first, connection const& second) {
        return first.sock == second.sock;
    }

    connection(connection&& other) : sock(std::move(other.sock)),
                                           events(std::move(other.events)),
                                           ios(std::move(other.ios)),
                                           on_read_ready(std::move(other.on_read_ready)),
                                           on_write_ready(std::move(other.on_write_ready)),
                                           on_close(std::move(other.on_close)) {};

    connection& operator=(connection&& other) {
        std::swap(sock, other.sock);
        std::swap(events, other.events);
        std::swap(ios, other.ios);
        std::swap(on_read_ready, other.on_read_ready);
        std::swap(on_write_ready, other.on_write_ready);
        std::swap(on_close, other.on_close);
        return *this;
    }

    void add_on_read_ready_handler(confunc_t func) {
        on_read_ready.push_back(func);
    }

    void add_on_write_ready_handler(confunc_t func) {
        on_write_ready.push_back(func);
    }

    void add_on_close_handler(confunc_t func) {
        on_close.push_back(func);
    }

    void add_to_ios(int listen_events) {
        events = listen_events;
        ios->add(sock, events, std::bind(&connection::parse_event, this, std::placeholders::_1));
    }

    void set_events(int new_events) {
        events = new_events;
        ios->change(sock, new_events);
    }

    std::string read() {
        char buf[1500];
        int cnt = recv(sock, buf, sizeof(buf), 0);
        printf("recv()-> %d\n", cnt);
        return std::string(buf, cnt);
    }

    // int write(char* buffer, size_t buffer_size) {
    //     int cnt = send(sock, buffer, buffer_size, MSG_NOSIGNAL);
    //     printf("send() -> %d\n", cnt);
    //     return cnt;
    // }

    int write(std::string const& data) {
        int cnt = send(sock, data.data(), data.size(), MSG_NOSIGNAL);
        printf("send() -> %d\n", cnt);
        return cnt;
    }

    void close() {
        printf("closing connection, sock=%d\n", sock);
        ios->remove(sock);
        call_handlers(on_close);
        ::close(sock);
        delete this;
    }

    void set_read_state(bool new_state) {
        if (new_state) {
            set_events(events | EPOLLIN);
        } else {
            set_events(events & ~EPOLLIN);
        }
    }

    void set_write_state(bool new_state) {
        if (new_state) {
            set_events(events | EPOLLOUT);
        } else {
            set_events(events & ~EPOLLOUT);
        }
    }

    int get_fd() const {
        return sock;
    }

    int get_events() const {
        return events;
    }

protected:
    connection() : connection(-1, nullptr) {};
    connection(int sock, io_service* ios) : sock(sock), ios(ios) {};

    void parse_event(int events) {
        printf("[handler, sock=%d, events=%d IN]\n", sock, events);
        if (events & EPOLLRDHUP) {
            printf("EPOLLRDHUP\n");
            close();
        } else {
            if (events & EPOLLIN) {
                printf("EPOLLIN\n");
                call_handlers(on_read_ready);
            }

            if (events & EPOLLOUT) {
                printf("EPOLLOUT\n");
                call_handlers(on_write_ready);
            }
        }
        printf("[handler OUT]\n");
    };

private:
    void call_handlers(std::vector<confunc_t> const& handlers) {
        for(auto handler: handlers) {
            handler(*this);
        }
    }

    int sock, events;
    io_service* ios;
    std::vector<confunc_t> on_read_ready, on_write_ready, on_close;
};


struct tcp_server {
    tcp_server(io_service& io_service, int port) : ios(io_service) {
        sockaddr_in srv_addr;
        bzero(&srv_addr, sizeof(srv_addr));

        int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        srv_addr.sin_family = AF_INET;
        srv_addr.sin_port = htons(port);
        srv_addr.sin_addr.s_addr = INADDR_ANY;

        int optval = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
                   &optval, sizeof(optval));

        bind(listen_sock, (sockaddr*)(&srv_addr), sizeof(srv_addr));
        listen(listen_sock, 1000);

        listen_conn = connection(listen_sock, &ios);
        printf("tcp_server, before add_on_read_ready_handler\n");
        listen_conn.add_on_read_ready_handler([this](connection& conn) {
            int in_sock = accept(conn.get_fd(), nullptr, nullptr);
            // TODO: this is not thread-safe (e.g. events could be called before on_new_connection(...))
            on_new_connection(construct_connection(in_sock, EPOLLIN | EPOLLRDHUP));
        });

        listen_conn.add_to_ios(EPOLLIN);
    }

    tcp_server(tcp_server const&) = delete;

    virtual ~tcp_server() = default;

    connection& make_connection(const char* addr, int port) {
        struct addrinfo *r, hints;

        bzero(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        printf("getaddrinfo - in <- %s\n", addr);
        int res = getaddrinfo(addr, 0, &hints, &r);
        if (res != 0)
            printf("error %d, %d\n", res, errno);
        ((sockaddr_in*)(r->ai_addr))->sin_port = htons(port);
        printf("getaddrinfo - out\n");

        int sock = socket(AF_INET, SOCK_STREAM, 0 /*SOCK_NONBLOCK*/);
        if (connect(sock, r->ai_addr, r->ai_addrlen) == -1) {
            printf("connect() error %d\n", errno);
        }

        return construct_connection(sock, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
    }

    connection& make_connection(int fd, int events=EPOLLIN | EPOLLOUT | EPOLLRDHUP) {
        return construct_connection(fd, events);
    }

    virtual void on_new_connection(connection&) = 0;

protected:
    io_service &ios;

private:
    connection& construct_connection(int sock, int events) {
        connection* new_conn = new connection(sock, &ios);
        new_conn->add_to_ios(events);
        return *new_conn;
    }

    connection listen_conn;
};

#endif
