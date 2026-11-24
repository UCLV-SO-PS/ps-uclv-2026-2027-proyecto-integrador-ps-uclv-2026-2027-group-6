#ifndef MONITOR_H
#define MONITOR_H

#include <stddef.h>

/*
 * Tamanos de los campos de un episodio.
 *
 * MONITOR_CODE_SIZE     = 6 caracteres utiles ("S01E01") + terminador '\0'.
 * MONITOR_TITLE_SIZE    = capacidad suficiente para los titulos del proyecto.
 * MONITOR_EPISODE_COUNT = cantidad de registros de la base de datos.
 */
#define MONITOR_CODE_SIZE 7
#define MONITOR_TITLE_SIZE 100
#define MONITOR_EPISODE_COUNT 6

/*
 * Convencion de retorno de la biblioteca.
 *
 * MONITOR_OK               = 0  Operacion exitosa.
 * MONITOR_ERR_INVALID_ARG  = -1 Argumento invalido (puntero nulo, codigo con
 *                               formato incorrecto, etc.).
 * MONITOR_ERR_BUFFER       = -2 Buffer de destino insuficiente.
 */
#define MONITOR_OK 0
#define MONITOR_ERR_INVALID_ARG (-1)
#define MONITOR_ERR_BUFFER (-2)

/*
 * Episode - registro de un episodio.
 *
 * code   Codigo exacto de seis caracteres (formato "S01E01") mas '\0'.
 * title  Titulo del episodio.
 * rating Puntuacion IMDb (0.0 - 10.0).
 * votes  Cantidad de votos registrados (no negativa).
 */
typedef struct {
    char code[MONITOR_CODE_SIZE];
    char title[MONITOR_TITLE_SIZE];
    float rating;
    unsigned int votes;
} Episode;

/*
 * monitor_db_init - inicializa y valida la base de datos interna.
 *
 * Devuelve MONITOR_OK (0) si la base de datos queda lista para usarse, o un
 * valor negativo (MONITOR_ERR_INVALID_ARG) si detecta datos inconsistentes.
 */
int monitor_db_init(void);

/*
 * monitor_find_episode - busca un episodio por su codigo exacto.
 *
 * code: cadena de seis caracteres ("S01E01").
 *
 * Devuelve un puntero al registro interno (no debe liberarse) si lo encuentra,
 * o NULL si el codigo es invalido o no existe. La comparacion es exacta.
 */
const Episode *monitor_find_episode(const char *code);

/*
 * monitor_get_episode_count - cantidad de registros disponibles.
 *
 * Devuelve el numero de episodios de la base de datos (siempre no negativo).
 */
size_t monitor_get_episode_count(void);

/*
 * monitor_format_episode - formatea un episodio en texto legible.
 *
 * episode:     registro valido a formatear (no nulo).
 * buffer:      destino donde se escribira la cadena terminada en '\0'.
 * buffer_size: capacidad total de buffer en bytes (incluye el terminador).
 *
 * Devuelve el numero de caracteres escritos (sin contar el '\0') si todo fue
 * correcto, MONITOR_ERR_INVALID_ARG si algun puntero es nulo, o
 * MONITOR_ERR_BUFFER si buffer_size es insuficiente para el resultado.
 */
int monitor_format_episode(
    const Episode *episode,
    char *buffer,
    size_t buffer_size
);

#endif
