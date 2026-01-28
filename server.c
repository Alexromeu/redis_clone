#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#define BUFFER_SIZE 64


int read_data(int connfd, void* buffer, size_t bufsize) {
    ssize_t n = recv(connfd, buffer, bufsize - 1, 0);
    if (n < 0) {
        printf("---Receiving Error---\n");
        return -1;
    }    
    
    return n;
}

int write_data(int fd, void* buffer, size_t bufsize) {
    ssize_t n = send(fd, buffer, bufsize, 0);
    if (n < 0) {
        printf("---Sending error---\n");
        return -1;
    }

    return n;
}

int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }

        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }

    return 0;
}

int32_t write_all(int fd, const char* buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);

        n -= (size_t)rv;
        buf += rv;
    }

    return 0;
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
        printf("---socket Error---\n");
    };
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    
    addr_data->sin_family = AF_INET;
    addr_data->sin_port = htons(3655);
    addr_data->sin_addr.s_addr = htonl(INADDR_ANY);

    int rv = bind(fd, (const struct sockaddr *)addr_data, sizeof(*addr_data));
    if (rv == -1) {
        printf("---Binding Error---\n");
        return -1;
    }

    rv = listen(fd, 10);
    if (rv == -1) {
        printf("---Listen Error---\n");
        return -1;
    }

    return fd;
}

int main() {
    struct sockaddr_in addr; 
    struct sockaddr_in client_addr;
    int fd = create_server(&addr);
    int new_fd;
    if (fd == -1) {
        printf("error creating server in main process\n");
        exit(0);
    }

    if (new_fd = accept_connection(fd, &client_addr) == -1) {
        printf("Error accepting connection\n");
        exit(0);
    }

    char buf[BUFFER_SIZE];
    int n;
    if ((n = read_data(new_fd, buf, BUFFER_SIZE)) == -1) {
        printf("Error receiving data\n");
        exit(0);
    }

    ((char*)buf)[n] = '\0';
    printf("Data read: %s\n", buf);

    return 0;
}