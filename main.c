/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>

#define test_round      1    /* 10/18/2018, 4pm */

int flb_net_tcp_fd_connect(int fd, char *host, int port, int family)
{
    int ret;
    struct addrinfo hints;
    struct addrinfo *res;
    char _port[6];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(_port, sizeof(_port), "%d", port);
    ret = getaddrinfo(host, _port, &hints, &res);
    if (ret != 0) {
        fprintf(stderr, "net_tcp_fd_connect: getaddrinfo(host='%s'): %s",
                host, gai_strerror(ret));
        return -1;
    }

    ret = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    return ret;
}

int flb_net_socket_tcp_nodelay(int fd)
{
    int on = 1;
    int ret;

    ret = setsockopt(fd, SOL_TCP, TCP_NODELAY, &on, sizeof(on));
    if (ret == -1) {
        perror("setsockopt");
        return -1;
    }

    return 0;
}

int flb_net_socket_nonblocking(int fd)
{
    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
        perror("fcntl");
        return -1;
    }

    return 0;
}

int flb_net_socket_create(int family, int nonblock)
{
    int fd;

    /* create the socket and set the nonblocking flag status */
    fd = socket(family, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    flb_net_socket_nonblocking(fd);

    return fd;
}

int main(int argc, char **argv)
{
    int fd;
    int ret;
    int port;
    char *host;

    if (argc < 3) {
        fprintf(stderr, "Usage: test_getaddrinfo host port\n");
        exit(EXIT_FAILURE);
    }

    printf("test round => %i\n", test_round);

    fd = flb_net_socket_create(AF_INET, 0);
    if (fd == -1) {
        fprintf(stderr, "Error creating socket\n");
        exit(EXIT_FAILURE);
    }

    host = argv[1];
    port = atoi(argv[2]);

    /* testing AF_INET */
    ret = flb_net_tcp_fd_connect(fd, host, port, AF_INET);
    /* no crashes expected, ret should be -1 and errno = EAGAIN */
    printf("test: AF_INET passed (ret => %i)\n", ret);

    close(fd);

    /* creat new socket again */
    fd = flb_net_socket_create(AF_INET, 0);
    if (fd == -1) {
        fprintf(stderr, "Error creating socket #2\n");
        exit(EXIT_FAILURE);
    }

    /* testing AF_UNSPEC */
    ret = flb_net_tcp_fd_connect(fd, host, port, AF_UNSPEC);

    /* maybe we never reach this point, testing... */
    printf("test: AF_UNSPEC passed (ret => %i)\n", ret);

    close(fd);

    return 0;
}
