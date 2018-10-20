/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/resource.h>

#define test_round      2    /* 10/20/2018, 9am */

#ifndef WORKAROUND
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
#elif defined(WORKAROUND) /* using getaddrinfo_a() in sync mode */
int flb_net_tcp_fd_connect(int fd, char *host, int port, int family)
{
    int ret;
    struct gaicb *reqs[1];
    struct addrinfo hints;
    struct addrinfo *res;
    char _port[6];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(_port, sizeof(_port), "%d", port);

    reqs[0] = calloc(1, sizeof(*reqs[0]));
    if (!reqs[0]) {
        perror("malloc");
        return -1;
    }
    reqs[0]->ar_name = host;

    ret = getaddrinfo_a(GAI_WAIT, reqs, 1, NULL);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo_a() failed: %s\n",
                gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    ret = gai_error(reqs[0]);
    if (ret == 0) {
        res = reqs[0]->ar_result;
        char h[1024];
        ret = getnameinfo(res->ai_addr, res->ai_addrlen,
                          h, sizeof(h),
                          NULL, 0, NI_NUMERICHOST);
        if (ret != 0) {
            fprintf(stderr, "getnameinfo() failed: %s\n",
                    gai_strerror(ret));
            exit(EXIT_FAILURE);
        }
    }
    else {
        fprintf(stderr, "[getaddrinfo_a() failed: %s\n",
                gai_strerror(ret));
        exit(EXIT_FAILURE);
    }
    ret = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    return ret;
}
#endif

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

    printf("test: round => %i\n", test_round);

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
    printf("test: AF_UNSPEC passed (ret => %i)\n", ret);
    close(fd);


    /* Limit stack size to what Fluent Bit uses by default */
    struct rlimit old_lim = {0, 0};
    struct rlimit new_lim = {24576, 24576}; /* 24 kb */

    ret = getrlimit(RLIMIT_STACK, &old_lim);
    if (ret == -1) {
        perror("getrlimit");
    }

    ret = setrlimit(RLIMIT_STACK, &new_lim);
    if (ret == -1) {
        perror("getrlimit");
    }

    ret = getrlimit(RLIMIT_STACK, &new_lim);
    if (ret == -1) {
        perror("getrlimit");
    }

    printf("test: stack.old_max=%lu, stack.new_max=%lu\n",
           old_lim.rlim_max, new_lim.rlim_max);

    fd = flb_net_socket_create(AF_INET, 0);
    if (fd == -1) {
        fprintf(stderr, "Error creating socket\n");
        exit(EXIT_FAILURE);
    }
    ret = flb_net_tcp_fd_connect(fd, host, port, AF_UNSPEC);
    printf("test: limited stack + AF_UNSPEC passed (ret => %i)\n", ret);
    close(fd);

    return 0;
}
