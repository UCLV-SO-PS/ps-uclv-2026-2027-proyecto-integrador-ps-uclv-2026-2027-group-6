#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define SERVER_PORT MONITOR_PORT
#define RESPONSE_SIZE 1024
#define CLIENT_COUNT 4

typedef struct {
    const char *code;
    const char *title;
    const char *rating;
    const char *votes;
} ExpectedEpisode;

static const ExpectedEpisode episodes[] = {
    { "S01E01", "The Hedge Knight", "9.2", "45120" },
    { "S01E02", "Hard Salt Beef", "8.8", "38900" },
    { "S01E03", "The Squire", "9.5", "52400" },
    { "S01E04", "Seven", "9.1", "41200" },
    { "S01E05", "In the Name of the Mother", "9.7", "68300" },
    { "S01E06", "The Morrow", "9.4", "49800" }
};

static int failures;
static pid_t server_pid = -1;
static char server_log_path[] = "/tmp/monitor-server-test-XXXXXX";

static void check(int condition, const char *description)
{
    if (condition) {
        printf("ok   - %s\n", description);
    } else {
        fprintf(stderr, "FAIL - %s\n", description);
        failures++;
    }
}

static int connect_to_server(void)
{
    int fd;
    struct sockaddr_in address;
    struct timeval receive_timeout = { 5, 0 };

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    if (setsockopt(
            fd,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &receive_timeout,
            sizeof receive_timeout
        ) < 0) {
        close(fd);
        return -1;
    }

    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(SERVER_PORT);

    if (connect(fd, (struct sockaddr *)&address, sizeof address) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int send_all(int fd, const void *buffer, size_t length)
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
        if (sent == 0) {
            return -1;
        }
        total += (size_t)sent;
    }

    return 0;
}

static int run_smoke_test(char *output, size_t output_size)
{
    int output_pipe[2];
    pid_t smoke_pid;
    int status;
    size_t total = 0;

    output[0] = '\0';
    if (pipe(output_pipe) < 0) {
        perror("pipe smoke test");
        return -1;
    }

    smoke_pid = fork();
    if (smoke_pid < 0) {
        perror("fork smoke test");
        close(output_pipe[0]);
        close(output_pipe[1]);
        return -1;
    }
    if (smoke_pid == 0) {
        alarm(10);
        close(output_pipe[0]);
        if (dup2(output_pipe[1], STDOUT_FILENO) < 0) {
            _exit(126);
        }
        close(output_pipe[1]);
        execl("./server", "./server", "--test-run", (char *)NULL);
        _exit(127);
    }

    close(output_pipe[1]);
    while (total + 1 < output_size) {
        ssize_t received = read(
            output_pipe[0],
            output + total,
            output_size - total - 1
        );

        if (received == 0) {
            break;
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read smoke output");
            close(output_pipe[0]);
            waitpid(smoke_pid, NULL, 0);
            return -1;
        }
        total += (size_t)received;
    }
    output[total] = '\0';
    close(output_pipe[0]);

    while (waitpid(smoke_pid, &status, 0) < 0) {
        if (errno != EINTR) {
            perror("waitpid smoke test");
            return -1;
        }
    }
    if (!WIFEXITED(status)) {
        return -1;
    }
    return WEXITSTATUS(status);
}

static int run_client(const char *code, char *output, size_t output_size)
{
    int output_pipe[2];
    pid_t client_pid;
    int status;
    size_t total = 0;

    output[0] = '\0';
    if (pipe(output_pipe) < 0) {
        perror("pipe client");
        return -1;
    }

    client_pid = fork();
    if (client_pid < 0) {
        perror("fork client");
        close(output_pipe[0]);
        close(output_pipe[1]);
        return -1;
    }
    if (client_pid == 0) {
        alarm(5);
        close(output_pipe[0]);
        if (dup2(output_pipe[1], STDOUT_FILENO) < 0) {
            _exit(126);
        }
        close(output_pipe[1]);
        execl("./client", "./client", "127.0.0.1", code, (char *)NULL);
        _exit(127);
    }

    close(output_pipe[1]);
    while (total + 1 < output_size) {
        ssize_t received = read(
            output_pipe[0],
            output + total,
            output_size - total - 1
        );

        if (received == 0) {
            break;
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read client output");
            close(output_pipe[0]);
            waitpid(client_pid, NULL, 0);
            return -1;
        }
        total += (size_t)received;
    }
    output[total] = '\0';
    close(output_pipe[0]);

    while (waitpid(client_pid, &status, 0) < 0) {
        if (errno != EINTR) {
            perror("waitpid client");
            return -1;
        }
    }

    if (!WIFEXITED(status)) {
        return -1;
    }
    return WEXITSTATUS(status);
}

static int receive_response(int fd, char *response, size_t response_size)
{
    size_t total = 0;

    while (total + 1 < response_size) {
        ssize_t received = recv(
            fd,
            response + total,
            response_size - total - 1,
            0
        );

        if (received == 0) {
            response[total] = '\0';
            return 0;
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += (size_t)received;
    }

    response[total] = '\0';
    return -1;
}

static int wait_for_server(void)
{
    struct timespec delay = { 0, 50000000L };
    int attempt;

    for (attempt = 0; attempt < 100; attempt++) {
        int fd = connect_to_server();

        if (fd >= 0) {
            close(fd);
            return 0;
        }
        if (waitpid(server_pid, NULL, WNOHANG) == server_pid) {
            server_pid = -1;
            return -1;
        }
        nanosleep(&delay, NULL);
    }

    return -1;
}

static int start_server(void)
{
    int log_fd = mkstemp(server_log_path);

    if (log_fd < 0) {
        perror("mkstemp");
        return -1;
    }

    server_pid = fork();
    if (server_pid < 0) {
        perror("fork server");
        close(log_fd);
        unlink(server_log_path);
        return -1;
    }
    if (server_pid == 0) {
        if (dup2(log_fd, STDOUT_FILENO) < 0 ||
            dup2(log_fd, STDERR_FILENO) < 0) {
            _exit(126);
        }
        close(log_fd);
        execl("./server", "./server", (char *)NULL);
        _exit(127);
    }

    close(log_fd);
    if (wait_for_server() < 0) {
        fprintf(stderr, "el servidor no inicio; consulte %s\n", server_log_path);
        return -1;
    }
    return 0;
}

static int wait_for_server_children(void)
{
    char children_path[128];
    struct timespec delay = { 0, 50000000L };
    int attempt;

    snprintf(
        children_path,
        sizeof children_path,
        "/proc/%ld/task/%ld/children",
        (long)server_pid,
        (long)server_pid
    );

    for (attempt = 0; attempt < 100; attempt++) {
        FILE *children_file = fopen(children_path, "r");
        char children[256];
        size_t length;

        if (children_file == NULL) {
            return 0;
        }
        length = fread(children, 1, sizeof children - 1, children_file);
        fclose(children_file);
        children[length] = '\0';

        if (children[0] == '\0') {
            return 0;
        }
        nanosleep(&delay, NULL);
    }

    return -1;
}

static int check_logged_client_pids(void)
{
    FILE *log_file = fopen(server_log_path, "r");
    char line[256];
    long pids[64];
    size_t pid_count = 0;

    if (log_file == NULL) {
        perror("fopen server log");
        return -1;
    }

    while (fgets(line, sizeof line, log_file) != NULL) {
        char ip[INET_ADDRSTRLEN];
        long child_pid;
        size_t index;
        int duplicate = 0;

        if (sscanf(line, "[servidor] IP %15s | hijo %ld", ip, &child_pid) != 2) {
            continue;
        }
        if (strcmp(ip, "127.0.0.1") != 0) {
            fclose(log_file);
            return -1;
        }
        for (index = 0; index < pid_count; index++) {
            if (pids[index] == child_pid) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate && pid_count < sizeof pids / sizeof pids[0]) {
            pids[pid_count++] = child_pid;
        }
    }

    fclose(log_file);
    return pid_count >= CLIENT_COUNT ? 0 : -1;
}

static void stop_server(void)
{
    int status;

    if (server_pid <= 0) {
        return;
    }

    if (kill(server_pid, SIGTERM) < 0 && errno != ESRCH) {
        perror("kill server");
    }
    while (waitpid(server_pid, &status, 0) < 0) {
        if (errno != EINTR) {
            perror("waitpid server");
            break;
        }
    }
    server_pid = -1;
}

static void test_episodes(void)
{
    char output[RESPONSE_SIZE];
    size_t index;

    for (index = 0; index < sizeof episodes / sizeof episodes[0]; index++) {
        int result = run_client(episodes[index].code, output, sizeof output);

        check(result == 0, "cliente consulta un episodio existente");
        check(
            strstr(output, episodes[index].code) != NULL &&
            strstr(output, episodes[index].title) != NULL &&
            strstr(output, episodes[index].rating) != NULL &&
            strstr(output, episodes[index].votes) != NULL,
            "respuesta contiene codigo, titulo, calificacion y votos"
        );
    }
}

static void test_invalid_and_unknown_codes(void)
{
    char output[RESPONSE_SIZE];
    int result;

    result = run_client("S99E99", output, sizeof output);
    check(result == 0 && strstr(output, "ERROR") != NULL,
          "codigo inexistente devuelve ERROR");

    result = run_client("S01E0", output, sizeof output);
    check(result != 0, "cliente rechaza un codigo de longitud invalida");
}

static void test_concurrent_clients(void)
{
    int clients[CLIENT_COUNT];
    size_t index;

    for (index = 0; index < CLIENT_COUNT; index++) {
        clients[index] = connect_to_server();
        if (clients[index] < 0) {
            check(0, "se conectan varios clientes concurrentes");
            while (index > 0) {
                close(clients[--index]);
            }
            return;
        }
    }

    check(1, "se conectan varios clientes concurrentes");
    for (index = 0; index < CLIENT_COUNT; index++) {
        if (send_all(
                clients[index],
                episodes[index].code,
                EPISODE_CODE_LEN
            ) < 0) {
            check(0, "cliente concurrente envia su solicitud");
            for (index = 0; index < CLIENT_COUNT; index++) {
                close(clients[index]);
            }
            return;
        }
    }
    for (index = 0; index < CLIENT_COUNT; index++) {
        char response[RESPONSE_SIZE];

        check(
            receive_response(clients[index], response, sizeof response) == 0 &&
            strstr(response, episodes[index].title) != NULL,
            "servidor responde a cada cliente concurrente"
        );
        close(clients[index]);
    }
}

static void test_unexpected_disconnect(void)
{
    int fd = connect_to_server();

    if (fd < 0) {
        check(0, "cliente puede desconectarse durante una solicitud");
        return;
    }

    check(send(fd, "S01", 3, 0) == 3,
          "cliente puede enviar una solicitud parcial");
    close(fd);
}

static void test_sigint_and_parent_availability(void)
{
    char output[RESPONSE_SIZE];
    int result;

    check(kill(server_pid, SIGINT) == 0, "se envia SIGINT al servidor");
    {
        struct timespec delay = { 0, 100000000L };
        nanosleep(&delay, NULL);
    }
    check(kill(server_pid, 0) == 0, "servidor sigue activo despues de SIGINT");

    result = run_client("S01E01", output, sizeof output);
    check(
        result == 0 && strstr(output, "The Hedge Knight") != NULL,
        "servidor continua atendiendo despues de SIGINT"
    );
}

static void test_processes_and_ipc(void)
{
    check(
        wait_for_server_children() == 0,
        "el servidor recolecta los hijos y no deja procesos pendientes"
    );
    check(
        check_logged_client_pids() == 0,
        "el padre registra la IP del cliente y PID distintos"
    );
}

int main(void)
{
    char output[RESPONSE_SIZE];
    int fd;
    int start_result;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal SIGPIPE");
        return EXIT_FAILURE;
    }

    check(
        run_smoke_test(output, sizeof output) == 0 &&
        strstr(output, "[test-run] todas las verificaciones pasaron") != NULL,
        "server --test-run completa correctamente"
    );

    fd = connect_to_server();
    check(fd < 0, "el puerto 6666 esta libre antes de iniciar la prueba");
    if (fd >= 0) {
        close(fd);
        fprintf(stderr, "Cierre cualquier servidor que ya escuche en el puerto 6666.\n");
        return EXIT_FAILURE;
    }

    start_result = start_server();
    check(start_result == 0, "servidor inicia en el puerto 6666");
    if (start_result != 0) {
        stop_server();
        unlink(server_log_path);
        return EXIT_FAILURE;
    }

    test_episodes();
    test_invalid_and_unknown_codes();
    test_unexpected_disconnect();
    test_concurrent_clients();
    test_processes_and_ipc();
    test_sigint_and_parent_availability();
    stop_server();

    check(
        run_client("S01E01", output, sizeof output) != 0,
        "cliente informa error cuando no hay servidor disponible"
    );
    if (unlink(server_log_path) < 0) {
        perror("unlink server log");
        failures++;
    }

    printf("\n%s (%d fallo(s))\n",
           failures == 0 ? "TODAS LAS PRUEBAS DE INTEGRACION PASARON" :
                           "HAY FALLOS",
           failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
