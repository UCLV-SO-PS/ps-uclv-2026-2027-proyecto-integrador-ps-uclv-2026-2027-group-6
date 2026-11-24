#include "monitor.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(int condition, const char *description)
{
    if (condition) {
        printf("ok   - %s\n", description);
    } else {
        printf("FAIL - %s\n", description);
        failures++;
    }
}

static void test_database(void)
{
    const Episode *episode;

    check(monitor_db_init() == MONITOR_OK, "monitor_db_init devuelve MONITOR_OK");
    check(monitor_get_episode_count() == 6, "la base de datos tiene 6 registros");

    episode = monitor_find_episode("S01E01");
    check(episode != NULL, "encuentra S01E01");
    if (episode != NULL) {
        check(strcmp(episode->code, "S01E01") == 0, "codigo de S01E01 correcto");
        check(strcmp(episode->title, "The Hedge Knight") == 0, "titulo de S01E01 correcto");
        check(episode->rating > 9.19f && episode->rating < 9.21f, "rating de S01E01 correcto");
        check(episode->votes == 45120u, "votos de S01E01 correctos");
    }

    check(monitor_find_episode("S01E05") != NULL, "encuentra S01E05");
    check(monitor_find_episode("S01E06") != NULL, "encuentra S01E06");

    check(monitor_find_episode("S09E09") == NULL, "codigo inexistente devuelve NULL");
    check(monitor_find_episode(NULL) == NULL, "codigo nulo devuelve NULL");
    check(monitor_find_episode("S01E0") == NULL, "codigo corto devuelve NULL");
    check(monitor_find_episode("S01E011") == NULL, "codigo largo devuelve NULL");
}

static void test_format(void)
{
    const Episode *episode = monitor_find_episode("S01E02");
    char buffer[512];
    char small[8];
    int written;

    check(episode != NULL, "episodio disponible para formatear");

    written = monitor_format_episode(episode, buffer, sizeof(buffer));
    check(written > 0, "formatea un episodio correctamente");
    check(strstr(buffer, "S01E02") != NULL, "la respuesta contiene el codigo");
    check(strstr(buffer, "Hard Salt Beef") != NULL, "la respuesta contiene el titulo");
    check(strstr(buffer, "8.8") != NULL, "la respuesta contiene la calificacion");

    check(
        monitor_format_episode(episode, small, sizeof(small)) == MONITOR_ERR_BUFFER,
        "buffer insuficiente devuelve MONITOR_ERR_BUFFER"
    );
    check(
        monitor_format_episode(NULL, buffer, sizeof(buffer)) == MONITOR_ERR_INVALID_ARG,
        "episodio nulo devuelve MONITOR_ERR_INVALID_ARG"
    );
    check(
        monitor_format_episode(episode, NULL, sizeof(buffer)) == MONITOR_ERR_INVALID_ARG,
        "buffer nulo devuelve MONITOR_ERR_INVALID_ARG"
    );
}

int main(void)
{
    test_database();
    test_format();

    printf("\n%s (%d fallo(s))\n", failures == 0 ? "TODAS LAS PRUEBAS PASARON" : "HAY FALLOS", failures);

    return failures == 0 ? 0 : 1;
}
