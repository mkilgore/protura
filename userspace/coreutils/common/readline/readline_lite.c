
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

static void readline_lite_init(struct readline_state *state, const char *prompt)
{
    memset(state, 0, sizeof(*state));

    readline_lite_display_init(state, prompt);
}

static char *readline_lite_fini(struct readline_state *state)
{
    readline_lite_display_fini(state);

    char *tmp_line = state->complete_line;
    state->complete_line = NULL;

    /* The logic here is a little intricate. We always want to return input if
     * we got it. But if we have an empty line, we still want to return that
     * unless we hit an EOF */
    if (tmp_line && *tmp_line)
        return tmp_line;

    if (!feof(stdin))
        return tmp_line;

    free(tmp_line);
    return NULL;
}

static void readline_switch_history(struct readline_state *state, struct history_entry **current, char **tmp_line, struct history_entry *new_ent)
{
    /* If the ent didn't change, then we don't do anything */
    if (new_ent == *current)
        return;

    if (!*current && new_ent) {
        *tmp_line = readline_lite_swap_line(state, new_ent->line);
        *current = new_ent;
    } else if (*current && !new_ent) {
        free(readline_lite_swap_line(state, *tmp_line));

        *tmp_line = NULL;
        *current = NULL;
    } else if (*current && new_ent) {
        free(readline_lite_swap_line(state, new_ent->line));

        *current = new_ent;
    } else if (!*current && !new_ent) {
        /* Nothing to do */
    }
}

char *readline_lite(const char *prompt)
{
    struct readline_state state;

    struct history_entry *current_history_ent = NULL;
    char *tmp_complete_line = NULL;
    readline_lite_init(&state, prompt);

    while (1) {
        readline_lite_render_display(&state);
        int c = getchar();

        /* 4 is ^D, which ends the current line immediately */
        if (c == EOF || c == 4)
            break;

        if (c == '\n' || c == '\r')
            break;

        /* FIXME: Add proper support for tab characters in the input */
        if (c == '\t')
            c = ' ';

        /* Escape character indicates a special character, like cursor keys */
        if (c == 27) {
            int c2 = getchar();
            if (c2 == '[') {
                int dir = getchar();

                switch (dir) {
                case 'A': /* up */
                    readline_switch_history(&state, &current_history_ent, &tmp_complete_line, history_get_previous(current_history_ent));
                    break;

                case 'B': /* down */
                    readline_switch_history(&state, &current_history_ent, &tmp_complete_line, history_get_next(current_history_ent));
                    break;

                case 'C': /* right */
                    if (state.current_pos < state.complete_line_len)
                        state.next_current_pos++;
                    break;

                case 'D': /* left */
                    if (state.current_pos > 0)
                        state.next_current_pos--;
                    break;
                }

                continue;
            }
        }

        if (c == '\b' || c == 127)
            readline_lite_char_del(&state);
        else
            readline_lite_char_add(&state, c);
    }

    if (current_history_ent) {
        history_delete(current_history_ent);
        free(tmp_complete_line);
    }

    return readline_lite_fini(&state);
}
