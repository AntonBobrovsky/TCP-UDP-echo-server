#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/wait.h>

#define PORT 3003
#define BUFLEN 1024
#define EPOLL_QUEUE_LEN 3
#define MAX_EPOLL_EVENTS 100

int main(int argc, char const *argv[]) {
    int tcp_sock, udp_sock, epfd;
    struct sockaddr_in addr, cli_addr;
    char buf[BUFLEN];
    int bytes;
    socklen_t cli_len;
    const int on = 1;

    /*Создаем TCP сокет*/
    if ((tcp_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    } else {
        printf("Open tcp socket... OK\n");
    }

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &on, sizeof(on)) < 0) {
        perror("setsockopt()");
        close(tcp_sock);
        exit(-1);
    } else {
        printf("Enable SO_REUSEADDR...ОК\n");
    }

    if (bind(tcp_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("bind()");
        close(tcp_sock);
        exit(-1);
    }

    if (listen(tcp_sock, 2) < 0) {
        perror("listen()");
        close(tcp_sock);
        exit(-1);
    }

    /*Созадем UDP сокет*/
    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    } else {
        printf("Open udp socket... OK\n");
    }

    if (bind(udp_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("bind()");
        close(udp_sock);
        exit(-1);
    }

    /*инициализируем epoll*/
    if ((epfd = epoll_create(EPOLL_QUEUE_LEN)) < 0) {
        perror("epoll_create()");
        exit(-1);
    }

    static struct epoll_event ev1;
    ev1.events = EPOLLIN;
    ev1.data.fd = tcp_sock;

    if(epoll_ctl(epfd, EPOLL_CTL_ADD, tcp_sock, &ev1) < 0) {
        perror("epoll_ctl()1");
        exit(-1);
    }

    ev1.events = EPOLLIN;
    ev1.data.fd = udp_sock;

    if(epoll_ctl(epfd, EPOLL_CTL_ADD, udp_sock, &ev1) < 0) {
        perror("epoll_ctl()2");
        exit(-1);
    }

    int c_socket; //client socket

    struct epoll_event events[MAX_EPOLL_EVENTS];

    while (1) {
        int nfds = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, -1);

        for (int i = 0; i < nfds; i++) {
            /*Если изменился UDP сокет*/
            if (events[i].data.fd == udp_sock) {
                if ((bytes = recvfrom(udp_sock, buf, BUFLEN, 0, (struct sockaddr *)&cli_addr, &cli_len)) < 0) {
                    perror("recvfrom()");
                    close(udp_sock);
                    exit(-1);
                }

                if (bytes == 0) break;

                printf("from UDP client: %s\n", buf);

                if (sendto(udp_sock, buf, strlen(buf), 0, (struct sockaddr *)&cli_addr, cli_len) < 0) {
                    perror("sendto()");
                    close(udp_sock);
                    exit(-1);
                }

            }
            /*Если изменился клиентский TCP сокет*/
            if (events[i].data.fd == c_socket) {
                if ((bytes = recv(c_socket, buf, BUFLEN, 0)) < 0) {
                    perror("recv()");
                    close(tcp_sock);
                    close(c_socket);
                    exit(-1);
                }

                if (bytes == 0) break;

                printf("from TCP client: %s\n", buf);

                if (send(c_socket, buf, BUFLEN, 0) < 0) {
                    perror("send()");
                    close(tcp_sock);
                    close(c_socket);
                    exit(-1);
                }
            }
            /*Если изменился TCP сокет*/
            if (events[i].data.fd == tcp_sock) {
                cli_len = sizeof(cli_addr);
                if((c_socket = accept(tcp_sock, (struct sockaddr*)&cli_addr, &cli_len)) < 0) {
                    perror("accept");
                    continue;
                }

                ev1.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
                ev1.data.fd = c_socket;
                if(epoll_ctl(epfd, EPOLL_CTL_ADD, c_socket, &ev1) < 0) {
                    perror("epoll_ctl()3");
                    exit(-1);
                }
            }
        }
    }

    return 0;
}
