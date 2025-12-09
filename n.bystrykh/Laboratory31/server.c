#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/my_socket"
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fds[MAX_CLIENTS];
    struct sockaddr_un addr;
    fd_set readfds;
    char buffer[BUFFER_SIZE];
    int max_fd, nfds;
    int client_count = 0;

    // Удаляем старый сокет
    unlink(SOCKET_PATH);

    // Создаём сокет
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Привязываем к пути
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Слушаем подключения (очередь на 5)
    if (listen(server_fd, 5) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("[Сервер] Ожидание подключений...\n");

    // Инициализируем массив клиентов
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_fds[i] = -1;  
    }

    while (1) {
        // Подготавливаем набор дескрипторов
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        // Добавляем подключённых клиентов
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] != -1) {
                FD_SET(client_fds[i], &readfds);
                if (client_fds[i] > max_fd) {
                    max_fd = client_fds[i];
                }
            }
        }

        // Ждём событий (select блокирует)
        nfds = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (nfds == -1) {
            perror("select failed");
            exit(EXIT_FAILURE);
        }

        // Новое подключение
        if (FD_ISSET(server_fd, &readfds)) {
            int new_client = accept(server_fd, NULL, NULL);
            if (new_client == -1) {
                perror("accept failed");
                continue;
            }

            // Добавляем клиента в массив
            int added = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_fds[i] == -1) {
                    client_fds[i] = new_client;
                    client_count++;
                    added = 1;
                    printf("[Сервер] Новый клиент подключён (fd: %d)\n", new_client);
                    break;
                }
            }
            if (!added) {
                printf("[Сервер] Максимум клиентов достигнут. Отказ.\n");
                close(new_client);
            }
        }

        // Проверяем данные от клиентов
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = client_fds[i];
            if (fd != -1 && FD_ISSET(fd, &readfds)) {
                ssize_t nread = read(fd, buffer, BUFFER_SIZE - 1);
                if (nread <= 0) {
                    // Клиент отключился
                    if (nread == 0) {
                        printf("[Сервер] Клиент отключён (fd: %d)\n", fd);
                    } else {
                        perror("read failed");
                    }
                    close(fd);
                    client_fds[i] = -1;
                    client_count--;
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