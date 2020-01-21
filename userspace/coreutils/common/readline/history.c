
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "list.h"
#include "readline_lite.h"
#include "internal.h"

/*
 * Note: list is stored in "opposite" order as the history_get_* commands. So
 * the first entry in the list is the most recent entry, which is also the
 * first entry returned by history_get_previous().
 */
static list_head_t history_entries = LIST_HEAD_INIT(history_entries);

struct history_entry *history_get_previous(struct history_entry *current)
{
    if (current) {
        if (list_is_last(&history_entries, &current->entry))
            return current;
        else
            return list_next_entry(current, entry);
    }

    if (list_empty(&history_entries))
        return NULL;

    return list_first_entry(&history_entries, struct history_entry, entry);
}

struct history_entry *history_get_next(struct history_entry *current)
{
    if (!current)
        return NULL;

    if (list_is_first(&history_entries, &current->entry))
        return NULL;

    return list_prev_entry(current, entry);
}

void history_delete(struct history_entry *ent)
{
    list_del(&ent->entry);
    free(ent->line);
    free(ent);
}

void history_add(const char *line)
{
    struct history_entry *ent = malloc(sizeof(*ent));
    list_node_init(&ent->entry);
    ent->line = strdup(line);

    list_add(&history_entries, &ent->entry);
}
