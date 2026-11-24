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

static int code_is_valid(const char *code)
{
    return code != NULL && strlen(code) == EPISODE_CODE_LEN;
}

static void read_line(char *buffer, size_t size)
{
    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';
}

static int connect_to_server(const char *host, unsigned short port)
{
    int fd;
    struct sockaddr_in address;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &address.sin_addr) != 1) {
        fprintf(stderr, "Direccion IPv4 invalida: %s\n", host);
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&address, sizeof address) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    return fd;
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

static void receive_and_print(int fd)
{
    char buffer[RESPONSE_BUFFER_SIZE];

    for (;;) {
        ssize_t received = recv(fd, buffer, sizeof buffer, 0);

        if (received == 0) {
            break;
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            break;
        }

        fwrite(buffer, 1, (size_t)received, stdout);
    }

    fflush(stdout);
}

int main(int argc, char *argv[])
{
    char host_buffer[64];
    char code_buffer[EPISODE_CODE_STR_SIZE];
    const char *host = "127.0.0.1";
    const char *code = NULL;
    int fd;

    if (argc == 3) {
        host = argv[1];
        code = argv[2];
    } else if (argc == 1) {
        printf("Servidor [127.0.0.1]: ");
        fflush(stdout);
        read_line(host_buffer, sizeof host_buffer);
        if (host_buffer[0] != '\0') {
            host = host_buffer;
        }

        printf("Codigo de episodio (6 caracteres): ");
        fflush(stdout);
        read_line(code_buffer, sizeof code_buffer);
        code = code_buffer;
    } else {
        fprintf(stderr, "Uso: %s [servidor] [codigo]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!code_is_valid(code)) {
        fprintf(
            stderr,
            "Codigo invalido: debe tener exactamente %d caracteres.\n",
            EPISODE_CODE_LEN
        );
        return EXIT_FAILURE;
    }

    fd = connect_to_server(host, MONITOR_PORT);
    if (fd < 0) {
        return EXIT_FAILURE;
    }

    if (send_all(fd, code, EPISODE_CODE_LEN) < 0) {
        perror("send");
        close(fd);
        return EXIT_FAILURE;
    }

    receive_and_print(fd);
    close(fd);

    return EXIT_SUCCESS;
}
