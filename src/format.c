#include "monitor.h"

#include <stdio.h>

int monitor_format_episode(
    const Episode *episode,
    char *buffer,
    size_t buffer_size
)
{
    int written;

    if (episode == NULL || buffer == NULL) {
        return MONITOR_ERR_INVALID_ARG;
    }

    written = snprintf(
        buffer,
        buffer_size,
        "================================\n"
        "       IMDb EPISODE MONITOR\n"
        "================================\n"
        "\n"
        "Codigo:        %s\n"
        "Episodio:      %s\n"
        "Calificacion:  %.1f / 10\n"
        "Votos:         %u\n"
        "\n"
        "================================\n",
        episode->code,
        episode->title,
        (double)episode->rating,
        episode->votes
    );

    if (written < 0) {
        return MONITOR_ERR_INVALID_ARG;
    }

    if ((size_t)written >= buffer_size) {
        return MONITOR_ERR_BUFFER;
    }

    return written;
}
