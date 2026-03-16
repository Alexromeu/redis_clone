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

#define BUFFER_SIZE 1024
#define PORT 3565
#define MAX_FDS 64


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

int main() {
    struct sockaddr_in addr, client_addr; 
    struct pollfd fds[MAX_FDS];  
    nfds_t nfds = 1;
    int listen_fd = create_server(&addr);
    int new_fd;
    char buf[BUFFER_SIZE];

    if (listen_fd == -1) {
        perror("error creating server in main process\n");
        exit(0);
    }
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;    

    while (1) {
        int ready = poll(fds, nfds, -1);
        
        if (ready < 0) {
            perror("error with poll\n");
            continue;
        }
        
        for (nfds_t i = 0; i < nfds; i++) {
            
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == listen_fd) {
                    new_fd = accept_connection(listen_fd, &client_addr); 
                    if (new_fd == -1) {
                        perror("accept error\n");
                        continue;
                    }

                    fds[nfds].fd = new_fd;
                    fds[nfds].events = POLLIN;
                    nfds++;

                } else {
                    int n;
                    if ((n = read_data(fds[i].fd, buf, BUFFER_SIZE)) == -1) {
                        printf("Error receiving data\n");
                        close(fds[i].fd);
                        fds[i] = fds[nfds - 1];
                        nfds--;
                        i--;
                        continue;
                    } 

                    if (n == 0) {
                        printf("client disconnected\n");
                        close(fds[i].fd);
                        fds[i] = fds[nfds - 1];
                        nfds--;
                        i--;
                        continue;
                    }
                    
                    printf("Data read: %s\n", buf);
                    buf[n] = '\0';

                      
                }
            }
        }   
    }

//we can do all data but some other day
    
    

    return 0;
}

//create sock structures