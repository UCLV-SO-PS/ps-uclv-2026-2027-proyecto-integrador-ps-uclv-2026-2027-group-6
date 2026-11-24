#include "monitor.h"

#include <string.h>

static const Episode episode_db[MONITOR_EPISODE_COUNT] = {
    { "S01E01", "The Hedge Knight", 9.2f, 45120u },
    { "S01E02", "Hard Salt Beef", 8.8f, 38900u },
    { "S01E03", "The Squire", 9.5f, 52400u },
    { "S01E04", "Seven", 9.1f, 41200u },
    { "S01E05", "In the Name of the Mother", 9.7f, 68300u },
    { "S01E06", "The Morrow", 9.4f, 49800u }
};

static int episode_is_valid(const Episode *episode)
{
    size_t code_length;

    if (episode == NULL) {
        return 0;
    }

    code_length = strlen(episode->code);
    if (code_length != MONITOR_CODE_SIZE - 1) {
        return 0;
    }

    if (episode->title[0] == '\0') {
        return 0;
    }

    if (episode->rating < 0.0f || episode->rating > 10.0f) {
        return 0;
    }

    return 1;
}

int monitor_db_init(void)
{
    size_t index;

    for (index = 0; index < MONITOR_EPISODE_COUNT; index++) {
        if (!episode_is_valid(&episode_db[index])) {
            return MONITOR_ERR_INVALID_ARG;
        }
    }

    return MONITOR_OK;
}

const Episode *monitor_find_episode(const char *code)
{
    size_t index;

    if (code == NULL) {
        return NULL;
    }

    if (strlen(code) != MONITOR_CODE_SIZE - 1) {
        return NULL;
    }

    for (index = 0; index < MONITOR_EPISODE_COUNT; index++) {
        if (strcmp(code, episode_db[index].code) == 0) {
            return &episode_db[index];
        }
    }

    return NULL;
}

size_t monitor_get_episode_count(void)
{
    return MONITOR_EPISODE_COUNT;
}
