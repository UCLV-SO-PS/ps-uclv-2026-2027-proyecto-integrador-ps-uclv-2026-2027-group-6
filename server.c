#include "monitor.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define RESPONSE_BUFFER_SIZE 512

static ssize_t recv_exact(int fd, void *buffer, size_t length)
{
    char *cursor = buffer;
    size_t total = 0;

    while (total < length) {
        ssize_t received = recv(fd, cursor + total, length - total, 0);

        if (received == 0) {
            break;
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += (size_t)received;
    }

    return (ssize_t)total;
}

static ssize_t send_all(int fd, const void *buffer, size_t length)
{
    const char *cursor = buffer;
    size_t total = 0;

    while (total < length) {
        ssize_t sent = send(fd, cursor + total, length - total, 0);

        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += (size_t)sent;
    }

    return (ssize_t)total;
}

static int create_server_socket(unsigned short port)
{
    int fd;
    int reuse = 1;
    struct sockaddr_in address;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&address, sizeof address) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, SOMAXCONN) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

static int build_response(const Episode *episode, char *buffer, size_t size)
{
    int prefix;
    int body;

    if (episode == NULL) {
        int written = snprintf(
            buffer,
            size,
            "%s\nCodigo de episodio no encontrado.\n",
            RESPONSE_STATUS_ERROR
        );

        if (written < 0 || (size_t)written >= size) {
            return -1;
        }
        return written;
    }

    prefix = snprintf(buffer, size, "%s\n", RESPONSE_STATUS_OK);
    if (prefix < 0 || (size_t)prefix >= size) {
        return -1;
    }

    body = monitor_format_episode(episode, buffer + prefix, size - (size_t)prefix);
    if (body < 0) {
        return -1;
    }

    return prefix + body;
}

static void handle_client(int client_fd, const struct sockaddr_in *client_address)
{
    char ip[INET_ADDRSTRLEN];
    char code[MONITOR_CODE_SIZE];
    char response[RESPONSE_BUFFER_SIZE];
    const Episode *episode;
    ssize_t received;
    int response_length;

    if (inet_ntop(AF_INET, &client_address->sin_addr, ip, sizeof ip) == NULL) {
        strcpy(ip, "desconocida");
    }
    printf("[conexion] cliente %s\n", ip);
    fflush(stdout);

    received = recv_exact(client_fd, code, EPISODE_CODE_LEN);
    if (received != EPISODE_CODE_LEN) {
        fprintf(stderr, "[conexion] solicitud incompleta de %s\n", ip);
        return;
    }
    code[EPISODE_CODE_LEN] = '\0';

    episode = monitor_find_episode(code);
    response_length = build_response(episode, response, sizeof response);
    if (response_length < 0) {
        fprintf(stderr, "[conexion] no se pudo construir la respuesta\n");
        return;
    }

    if (send_all(client_fd, response, (size_t)response_length) < 0) {
        perror("send");
    }
}

int main(void)
{
    int server_fd;
    int client_fd;
    struct sockaddr_in client_address;
    socklen_t client_length;

    if (monitor_db_init() != MONITOR_OK) {
        fprintf(stderr, "Error: la base de datos no es valida\n");
        return EXIT_FAILURE;
    }

    server_fd = create_server_socket(MONITOR_PORT);
    if (server_fd < 0) {
        return EXIT_FAILURE;
    }

    printf("Servidor IMDb escuchando en el puerto %d...\n", MONITOR_PORT);
    fflush(stdout);

    for (;;) {
        client_length = sizeof client_address;
        client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_address,
            &client_length
        );

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        handle_client(client_fd, &client_address);
        close(client_fd);
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
