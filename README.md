# Monitor IMDb de *A Knight of the Seven Kingdoms*

Servidor TCP concurrente en C que permite consultar la calificación IMDb de
los seis episodios de la primera temporada. El servidor escucha en el puerto
TCP `6666` y atiende cada conexión con un proceso hijo.

## Integrantes

- Victor Manuel Martinez Campo
- Aguero Pantoja Héctor David
- Morciego Orozco Gabriel Alejandro

### Nota: 5

## Requisitos

- Linux con las API POSIX para sockets, procesos, tuberías y señales, además
  de `/proc` para comprobar los hijos del servidor en las pruebas.
- GCC, GNU Make y `ar`.

## Estructura del proyecto

- `server.c`: servidor TCP, procesos hijo, tubería y manejo de señales.
- `client.c`: cliente de consola.
- `include/monitor.h`: tipos e interfaz de la biblioteca.
- `include/protocol.h`: puerto y constantes del protocolo.
- `src/database.c`: datos y búsqueda de episodios.
- `src/format.c`: formato de las respuestas.
- `tests/test_monitor.c`: pruebas unitarias de la biblioteca.
- `tests/test_server.c`: pruebas de integración del servidor, cliente,
  concurrencia, IPC y señales.
- `build/`: objetos, ejecutables de prueba y dependencias generadas.
- `screenshot/`: capturas de evidencia para la entrega.

## Compilación y pruebas

Desde la raíz del repositorio:

```sh
make clean
make all
make test
```

`make all` compila los módulos auxiliares en `build/`, crea la biblioteca
estática `libmonitor.a` con `ar rcs` y enlaza `server` y `client`.
`make test` ejecuta las pruebas unitarias y las pruebas de integración.
También se pueden ejecutar estas últimas con `make test-integration`.
`make clean` elimina objetos, dependencias generadas, ejecutables y la
biblioteca; no elimina fuentes ni capturas.

## Ejecución

En una terminal, inicia el servidor:

```sh
./server
```

En otra terminal, consulta un episodio:

```sh
./client 127.0.0.1 S01E01
```

El cliente también permite introducir el servidor y el código de forma
interactiva cuando se ejecuta sin argumentos. Los códigos de episodios son
`S01E01`, `S01E02`, `S01E03`, `S01E04`, `S01E05` y `S01E06`. Un código
correctamente formado pero inexistente devuelve una respuesta de error.

Para detener el servidor, envía una señal distinta de `SIGINT`, que se ignora
intencionalmente:

```sh
kill -TERM <PID>
```

## Protocolo de comunicación

Cada conexión transporta una consulta. El cliente envía exactamente seis
bytes con el código del episodio, sin terminador. El servidor responde con
texto y cierra la conexión al terminar de transmitirlo.

## Decisiones de diseño

- **Biblioteca estática:** la base de datos se mantiene en memoria y el
  servidor consulta y formatea sus registros mediante `libmonitor.a`.
- **Concurrencia:** el proceso padre conserva el socket de escucha y crea un
  hijo mediante `fork()` por cada conexión aceptada.
- **Tubería:** se crea una tubería sin nombre por conexión. Antes de atender
  la consulta, el hijo transmite la dirección IPv4 del cliente; el padre
  imprime esa dirección junto al PID del hijo.
- **Señales:** el servidor ignora `SIGINT` y `SIGPIPE`. El manejador de
  `SIGCHLD` recolecta los hijos terminados con `waitpid(-1, NULL, WNOHANG)`.

## Smoke test y evidencias

El smoke test verifica la base de datos y el intercambio TCP en loopback
usando un puerto efímero:

```sh
./server --test-run
echo $?
```

El código de salida esperado es `0`.

### Capturas de evidencia

Las capturas se recopilaron durante la ejecución real en Linux y se encuentran
en `screenshot/`:

- `screenshot/01_servidor_consulta.png`: servidor escuchando en el puerto
  `6666`, consulta de `S01E05` y registro de la IP del cliente con el PID del
  hijo.
- `screenshot/02_clientes_concurrentes.png`: cuatro clientes concurrentes con
  IP y PID distintos, y sus respuestas.
- `screenshot/03_test_run.png`: resultado exitoso de `./server --test-run`
  con código de salida `0`.
