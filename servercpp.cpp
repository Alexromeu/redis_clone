#define _POSIX_C_SOURCE 200809L
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <vector>
#include <fcntl.h>

#define BUFFER_SIZE 1024
#define PORT 3565
#define MAX_FDS 64

const size_t k_max_msg = 32 << 20;

struct Conn {
    int fd = -1;
    bool want_read = false;
    bool want_write = false; 
    bool want_close = false;

    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;
};


int read_data(int connfd, void* buffer, size_t bufsize) {
    ssize_t n = recv(connfd, buffer, bufsize - 1, 0);
    if (n < 0) {
        perror("---Receiving Error---\n");
        return -1;
    }    
    
    return n;
}

int write_data(int fd, void* buffer, size_t bufsize) {
    ssize_t n = send(fd, buffer, bufsize, 0);
    if (n < 0) {
        perror("---Sending error---\n");
        return -1;
    }

    return n;
}


int accept_connection(int fd, struct sockaddr_in *client_address) {
    while (1) {
        socklen_t addrlen = sizeof(*client_address);
        printf("Waiting...\n");
        int connfd = accept(fd, (struct sockaddr *)client_address, &addrlen);

        if (connfd < 0) {
            continue;
        }

        return connfd;
    }

}



int create_server(struct sockaddr_in *addr_data) {
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("---socket Error---\n");
    };
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    
    memset(addr_data, 0, sizeof(struct sockaddr_in));
    addr_data->sin_family = AF_INET;
    addr_data->sin_port = htons(PORT);
    addr_data->sin_addr.s_addr = htonl(INADDR_ANY);

    
    int rv = bind(fd, (const struct sockaddr *)addr_data, sizeof(*addr_data));
    if (rv == -1) {
        perror("---Binding Error---\n");
        return -1;
    }

    rv = listen(fd, 100);
    if (rv == -1) {
        perror("---Listen Error---\n");
        return -1;
    }
    printf("Listening... in port: %d\n", PORT);
    return fd;
}

Conn* handle_accept(int fd) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);

    if (connfd < 0) {
        return NULL;
    }

    fd_set_nb(connfd);
    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true;
    return conn;
}

static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}

static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}

static void handle_read(Conn *conn) {
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if (rv <= 0) {
        conn->want_close = true;
        return;
    } 

    buf_append(conn->incoming, buf, (size_t)rv);
    while (try_one_request(conn)) {};
}

static bool try_one_request(Conn *conn) {
    if (conn->incoming.size() < 4) {
        return false;
    }

    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if (len > k_max_msg) {
        conn->want_close = true;
        return;
    }

    if (4 + len > conn->incoming.size()) {
        return false;
    }

    const uint8_t *request = &conn->incoming[4];

    buf_append(conn->outgoing, (const uint8_t *)&len, 4);
    buf_append(conn->outgoing, request, len);
    
    buf_consume(conn->incoming, 4 + len);
    return true;        
}

static void msg_errno(const char *msg) {
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

static void handle_write(Conn *conn) {
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, &conn->outgoing[0], conn->outgoing.size());
    if (rv < 0 && errno == EAGAIN) {
        return; 
    }
    if (rv < 0) {
        msg_errno("write() error");
        conn->want_close = true;   
        return;
    }
    
    buf_consume(conn->outgoing, (size_t)rv);

    if (conn->outgoing.size() == 0) {  
        conn->want_read = true;
        conn->want_write = false;
    } 
}


static void fd_set_nb(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

}

int main() {
    std::vector<Conn *>fd2conn;
    std::vector<struct pollfd> poll_args;
    struct sockaddr_in addr_data;
    int fd = create_server(&addr_data);

    while (true) {
        poll_args.clear();

        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);

        for (Conn *conn : fd2conn) {
            if (!conn) {
                continue;
            }

            struct pollfd pfd = {conn->fd, POLLERR, 0};

            if (conn->want_read) {
                pfd.events |= POLLIN;
            }

            if (conn->want_write) {
                pfd.events |= POLLOUT;
            }

            poll_args.push_back(pfd);
        }

        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
        if (rv < 0 && errno == EINTR) {
            continue;
        }

        if (rv < 0) {
            printf("poll");
        }

        if (poll_args[0].revents) {
            if (Conn *conn = handle_accept(fd)) {
                if (fd2conn.size() <= (size_t)conn->fd + 1) {
                    fd2conn.resize(conn->fd + 1);
                }

                fd2conn[conn->fd] = conn;
            }
        }

        for (size_t i = 1; i < poll_args.size(); i++) {
            uint32_t ready = poll_args[i].revents;
            Conn *conn = fd2conn[poll_args[i].fd];
            if (ready & POLLIN) {
                handle_read(conn);
            }

            if (ready & POLLOUT) {
                handle_write(conn);
            }

            if ((ready & POLLERR) || conn->want_close) {
                (void)close(conn->fd);
                fd2conn[conn->fd] = NULL;
                delete conn;
            }
            
        }
    }
   
    return 0;
}



