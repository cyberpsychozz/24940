#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define SOCKET_PATH "/tmp/my_socket"
#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

int main() {
    int server_fd, epoll_fd;
    struct sockaddr_un addr;
    struct epoll_event ev, events[MAX_EVENTS];
    char buffer[BUFFER_SIZE];
    ssize_t nread;

    // Удаляем старый сокет
    unlink(SOCKET_PATH);

    // Создаём сокет
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Делаем сокет non-blocking
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    // Привязываем к пути
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Слушаем подключения
    if (listen(server_fd, 5) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    // Создаём epoll
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        exit(EXIT_FAILURE);
    }

    // Добавляем серверный сокет в epoll
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl failed");
        exit(EXIT_FAILURE);
    }

    printf("[Сервер] Ожидание подключений...\n");

    while (1) {
        // Ждём событий (асинхронно)
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);  
        if (nfds == -1) {
            perror("epoll_wait failed");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                // Новое подключение
                int new_client = accept(server_fd, NULL, NULL);
                if (new_client == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("accept failed");
                    }
                    continue;
                }

                // Делаем клиента non-blocking
                fcntl(new_client, F_SETFL, O_NONBLOCK);

                // Добавляем в epoll
                ev.events = EPOLLIN | EPOLLET;  // Edge-triggered
                ev.data.fd = new_client;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client, &ev) == -1) {
                    perror("epoll_ctl add client failed");
                    close(new_client);
                    continue;
                }

                printf("[Сервер] Новый клиент подключён (fd: %d)\n", new_client);
            } else {
                // Данные от клиента
                int fd = events[i].data.fd;
                nread = read(fd, buffer, BUFFER_SIZE - 1);
                if (nread <= 0) {
                    if (nread == 0) {
                        printf("[Сервер] Клиент отключён (fd: %d)\n", fd);
                    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("read failed");
                    }
                    // Закрываем и удаляем из epoll
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                } else {
                    buffer[nread] = '\0';

                    // Переводим в верхний регистр и выводим
                    for (ssize_t j = 0; j < nread; j++) {
                        putchar(toupper(buffer[j]));
                    }
                    fflush(stdout);
                }
            }
        }
    }

    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}