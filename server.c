#include "monitor.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define RESPONSE_BUFFER_SIZE 512

typedef struct {
    char ip[INET_ADDRSTRLEN];
} ClientInfo;

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

static ssize_t read_all(int fd, void *buffer, size_t length)
{
    char *cursor = buffer;
    size_t total = 0;

    while (total < length) {
        ssize_t received = read(fd, cursor + total, length - total);

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (received == 0) {
            break;
        }
        total += (size_t)received;
    }

    return (ssize_t)total;
}

static ssize_t write_all(int fd, const void *buffer, size_t length)
{
    const char *cursor = buffer;
    size_t total = 0;

    while (total < length) {
        ssize_t written = write(fd, cursor + total, length - total);

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (written == 0) {
            break;
        }
        total += (size_t)written;
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

static void handle_client(int client_fd, const ClientInfo *info)
{
    char code[MONITOR_CODE_SIZE];
    char response[RESPONSE_BUFFER_SIZE];
    const Episode *episode;
    ssize_t received;
    int response_length;

    received = recv_exact(client_fd, code, EPISODE_CODE_LEN);
    if (received != EPISODE_CODE_LEN) {
        fprintf(stderr, "[conexion] solicitud incompleta de %s\n", info->ip);
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

static void handle_sigchld(int signal_number)
{
    int saved_errno = errno;

    (void)signal_number;

    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }

    errno = saved_errno;
}

static int configure_signals(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof action);
    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_IGN;
    action.sa_flags = 0;
    if (sigaction(SIGINT, &action, NULL) < 0) {
        perror("sigaction SIGINT");
        return -1;
    }
    if (sigaction(SIGPIPE, &action, NULL) < 0) {
        perror("sigaction SIGPIPE");
        return -1;
    }

    action.sa_handler = handle_sigchld;
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &action, NULL) < 0) {
        perror("sigaction SIGCHLD");
        return -1;
    }

    return 0;
}

static int compile_with_gcc(char *output_path)
{
    pid_t compiler_pid;
    int status;

    compiler_pid = fork();
    if (compiler_pid < 0) {
        perror("fork gcc");
        return -1;
    }
    if (compiler_pid == 0) {
        char *const arguments[] = {
            "gcc",
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-D_POSIX_C_SOURCE=200809L",
            "-Iinclude",
            "server.c",
            "src/database.c",
            "src/format.c",
            "-o",
            output_path,
            NULL
        };

        execvp("gcc", arguments);
        perror("execvp gcc");
        _exit(127);
    }

    while (waitpid(compiler_pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        perror("waitpid gcc");
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[test-run] la compilacion con gcc fallo\n");
        return -1;
    }

    return 0;
}

static int run_smoke_test(void)
{
    char temporary_binary[64];
    char formatted[512];
    char response[RESPONSE_BUFFER_SIZE];
    char code[MONITOR_CODE_SIZE];
    const Episode *episode;
    struct sockaddr_in address;
    socklen_t address_length;
    int listener = -1;
    int accepted_fd = -1;
    int reuse = 1;
    int response_length;
    pid_t test_pid = -1;
    int status = 0;
    int child_waited = 0;
    int ok = 0;

    printf("[test-run] compilando con gcc...\n");
    snprintf(temporary_binary, sizeof temporary_binary, "/tmp/imdb_smoke_%ld", (long)getpid());
    if (compile_with_gcc(temporary_binary) != 0) {
        goto cleanup;
    }

    printf("[test-run] verificando la base de datos...\n");
    if (monitor_db_init() != MONITOR_OK) {
        fprintf(stderr, "[test-run] monitor_db_init fallo\n");
        goto cleanup;
    }
    if (monitor_get_episode_count() != MONITOR_EPISODE_COUNT) {
        fprintf(stderr, "[test-run] cantidad de episodios incorrecta\n");
        goto cleanup;
    }
    episode = monitor_find_episode("S01E01");
    if (episode == NULL) {
        fprintf(stderr, "[test-run] no se encontro S01E01\n");
        goto cleanup;
    }
    if (monitor_format_episode(episode, formatted, sizeof formatted) <= 0) {
        fprintf(stderr, "[test-run] no se pudo formatear S01E01\n");
        goto cleanup;
    }
    if (strstr(formatted, "S01E01") == NULL ||
        strstr(formatted, "The Hedge Knight") == NULL) {
        fprintf(stderr, "[test-run] el formato no contiene los datos esperados\n");
        goto cleanup;
    }

    printf("[test-run] verificando la comunicacion TCP...\n");
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket test");
        goto cleanup;
    }
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse) < 0) {
        perror("setsockopt test");
        goto cleanup;
    }
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(listener, (struct sockaddr *)&address, sizeof address) < 0) {
        perror("bind test");
        goto cleanup;
    }
    address_length = sizeof address;
    if (getsockname(listener, (struct sockaddr *)&address, &address_length) < 0) {
        perror("getsockname test");
        goto cleanup;
    }
    if (listen(listener, 1) < 0) {
        perror("listen test");
        goto cleanup;
    }

    test_pid = fork();
    if (test_pid < 0) {
        perror("fork test");
        goto cleanup;
    }
    if (test_pid == 0) {
        int test_client_fd;
        size_t total = 0;

        close(listener);
        test_client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (test_client_fd < 0) {
            perror("socket cliente test");
            _exit(1);
        }
        if (connect(test_client_fd, (struct sockaddr *)&address, sizeof address) < 0) {
            perror("connect test");
            close(test_client_fd);
            _exit(1);
        }
        if (send_all(test_client_fd, "S01E01", EPISODE_CODE_LEN) != EPISODE_CODE_LEN) {
            close(test_client_fd);
            _exit(1);
        }
        for (;;) {
            ssize_t received = recv(test_client_fd, response + total,
                                    sizeof response - total - 1, 0);
            if (received < 0) {
                if (errno == EINTR) {
                    continue;
                }
                close(test_client_fd);
                _exit(1);
            }
            if (received == 0) {
                break;
            }
            total += (size_t)received;
            if (total >= sizeof response - 1) {
                break;
            }
        }
        response[total] = '\0';
        close(test_client_fd);
        if (strstr(response, RESPONSE_STATUS_OK) == NULL ||
            strstr(response, "The Hedge Knight") == NULL) {
            _exit(1);
        }
        _exit(0);
    }

    accepted_fd = accept(listener, NULL, NULL);
    if (accepted_fd < 0) {
        perror("accept test");
        goto cleanup;
    }
    if (recv_exact(accepted_fd, code, EPISODE_CODE_LEN) != EPISODE_CODE_LEN) {
        fprintf(stderr, "[test-run] solicitud de prueba incompleta\n");
        goto cleanup;
    }
    code[EPISODE_CODE_LEN] = '\0';
    episode = monitor_find_episode(code);
    response_length = build_response(episode, response, sizeof response);
    if (response_length < 0) {
        fprintf(stderr, "[test-run] no se pudo construir la respuesta\n");
        goto cleanup;
    }
    if (send_all(accepted_fd, response, (size_t)response_length) != response_length) {
        fprintf(stderr, "[test-run] no se pudo enviar la respuesta\n");
        goto cleanup;
    }
    close(accepted_fd);
    accepted_fd = -1;

    while (waitpid(test_pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        perror("waitpid test");
        goto cleanup;
    }
    child_waited = 1;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[test-run] el cliente de prueba fallo\n");
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (test_pid > 0 && !child_waited) {
        while (waitpid(test_pid, &status, 0) < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
    }
    if (accepted_fd >= 0) {
        close(accepted_fd);
    }
    if (listener >= 0) {
        close(listener);
    }
    if (remove(temporary_binary) != 0 && ok) {
        perror("remove test");
    }
    printf(ok ? "[test-run] todas las verificaciones pasaron\n"
              : "[test-run] verificaciones fallidas\n");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    int server_fd;
    int client_fd;
    struct sockaddr_in client_address;
    socklen_t client_length;

    if (argc == 2 && strcmp(argv[1], "--test-run") == 0) {
        return run_smoke_test();
    }
    if (argc != 1) {
        fprintf(stderr, "Uso: %s [--test-run]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (monitor_db_init() != MONITOR_OK) {
        fprintf(stderr, "Error: la base de datos no es valida\n");
        return EXIT_FAILURE;
    }
    if (configure_signals() < 0) {
        return EXIT_FAILURE;
    }

    server_fd = create_server_socket(MONITOR_PORT);
    if (server_fd < 0) {
        return EXIT_FAILURE;
    }

    printf("Servidor IMDb escuchando en el puerto %d...\n", MONITOR_PORT);
    fflush(stdout);

    for (;;) {
        pid_t child_pid;
        int pipe_fd[2];
        ClientInfo info = {{0}};

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

        if (pipe(pipe_fd) < 0) {
            perror("pipe");
            close(client_fd);
            continue;
        }

        child_pid = fork();
        if (child_pid < 0) {
            perror("fork");
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            close(client_fd);
            continue;
        }

        if (child_pid == 0) {
            close(pipe_fd[0]);
            close(server_fd);

            if (inet_ntop(AF_INET, &client_address.sin_addr, info.ip, sizeof info.ip) == NULL) {
                snprintf(info.ip, sizeof info.ip, "%s", "desconocida");
            }
            if (write_all(pipe_fd[1], &info, sizeof info) != (ssize_t)sizeof info) {
                perror("write pipe");
                close(pipe_fd[1]);
                close(client_fd);
                _exit(EXIT_FAILURE);
            }
            close(pipe_fd[1]);

            handle_client(client_fd, &info);
            close(client_fd);
            _exit(EXIT_SUCCESS);
        }

        close(pipe_fd[1]);
        if (read_all(pipe_fd[0], &info, sizeof info) == (ssize_t)sizeof info) {
            printf("[servidor] IP %s | hijo %ld\n", info.ip, (long)child_pid);
            fflush(stdout);
        } else {
            fprintf(stderr, "[servidor] no se pudo leer la IP del hijo %ld\n", (long)child_pid);
        }
        close(pipe_fd[0]);
        close(client_fd);
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
