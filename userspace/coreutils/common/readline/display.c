
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

/* Ideally, we should be using termcap. However, we don't provide that
 * ourselves and we want to avoid the dependency, so due to that we just
 * hard-code the escape sequences. */

static void cursor_off(void)
{
    fputs("\e[?25l", stdout);
}

static void cursor_on(void)
{
    fputs("\e[?25h", stdout);
}

static void cursor_move_down(void)
{
    fputs("\n", stdout);
}

static void cursor_move_up(void)
{
    fputs("\e[A", stdout);
}

static void cursor_reset_column(void)
{
    fputs("\r", stdout);
}

static void cursor_move_to_column(int col)
{
    fprintf(stdout, "\e[%dC", col);
}

void readline_lite_setup_tty(struct readline_state *state)
{
    tcgetattr(STDIN_FILENO, &state->saved_tty_settings);

    struct termios new = state->saved_tty_settings;
    new.c_iflag &= ~(IXON | ICRNL);
    new.c_oflag &= ~(OPOST);
    new.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    tcsetattr(STDIN_FILENO, TCSADRAIN, &new);
}

void readline_lite_restore_tty(struct readline_state *state)
{
    tcsetattr(STDIN_FILENO, TCSADRAIN, &state->saved_tty_settings);
}

void readline_lite_render_display(struct readline_state *state)
{
    int i;
    int current_row = (state->prompt_len + state->current_pos) / state->winsize.ws_col;

    int target_row = (state->prompt_len + state->next_current_pos) / state->winsize.ws_col;
    int target_col = (state->prompt_len + state->next_current_pos) % state->winsize.ws_col;

    size_t comp_len = strlen(state->complete_line);

    /* First, we scroll the screen if we need more space, and then render the
     * whole thing into a temporary buffer */
    int total_lines = (state->prompt_len + comp_len) / state->winsize.ws_col + 1;

    if (total_lines < state->total_scroll_rows)
        total_lines = state->total_scroll_rows;

    state->total_scroll_rows = total_lines;

    char line_buf[total_lines][state->winsize.ws_col + 1];

    for (i = 0; i < total_lines; i++)  {
        memset(line_buf[i], 32, state->winsize.ws_col);
        line_buf[i][state->winsize.ws_col] = 0;
    }

    int prompt_lines = state->prompt_len / state->winsize.ws_col + 1;

    int offset;
    for (i = 0; i < prompt_lines; i++) {
        offset = i * state->winsize.ws_col;
        int len = state->prompt_len - i * state->winsize.ws_col;

        if (len > state->winsize.ws_col)
            len = state->winsize.ws_col;

        memcpy(line_buf[i], state->prompt_str + offset, len);
    }

    int line_offset = state->prompt_len % state->winsize.ws_col;

    offset = 0;
    for (i = prompt_lines - 1; i < total_lines; i++) {
        int len = comp_len - offset;

        if (len > state->winsize.ws_col - line_offset)
            len = state->winsize.ws_col - line_offset;

        memcpy(line_buf[i] + line_offset, state->complete_line + offset, len);
        offset += len;
        line_offset = 0;
    }

    cursor_off();

    /* Reset cursor to the beginning of the first line */
    cursor_reset_column();
    for (i = current_row; i > 0; i--)
        cursor_move_up();

    for (i = 0; i < total_lines; i++) {
        cursor_reset_column();
        fputs(line_buf[i], stdout);

        /* When we reach the end, we stay on the current line */
        if (i < total_lines - 1)
            cursor_move_down();
    }

    /* Reposition cursor */
    for (i = total_lines - 1; i > target_row; i--)
        cursor_move_up();

    cursor_reset_column();
    if (target_col)
        cursor_move_to_column(target_col);

    state->current_pos = state->next_current_pos;
    cursor_on();
}

void readline_lite_char_add(struct readline_state *state, char c)
{
    if (state->complete_line_len >= state->complete_line_alloc_len - 1) {
        state->complete_line_alloc_len = (state->complete_line_len + 1) * 2 + 1;
        state->complete_line = realloc(state->complete_line, state->complete_line_alloc_len);
    }

    state->complete_line[state->complete_line_len + 1] = '\0';

    if (state->current_pos < state->complete_line_len)
        memmove(state->complete_line + state->current_pos + 1, state->complete_line + state->current_pos, state->complete_line_len - state->current_pos);

    state->complete_line[state->current_pos] = c;

    state->complete_line_len++;
    state->next_current_pos++;
}

void readline_lite_char_del(struct readline_state *state)
{
    if (state->complete_line_len == 0 || state->current_pos == 0)
        return;

    if (state->current_pos < state->complete_line_len)
        memmove(state->complete_line + state->current_pos - 1, state->complete_line + state->current_pos, state->complete_line_len - state->current_pos);

    state->complete_line_len--;
    state->next_current_pos--;
    state->complete_line[state->complete_line_len] = '\0';
}

char *readline_lite_swap_line(struct readline_state *state, const char *new_line)
{
    char *tmp_line = state->complete_line;

    state->complete_line = strdup(new_line);
    state->complete_line_len = strlen(state->complete_line);
    state->complete_line_alloc_len = state->complete_line_len + 1;

    state->next_current_pos = state->complete_line_len;

    return tmp_line;
}

void readline_lite_display_init(struct readline_state *state, const char *prompt)
{
    readline_lite_setup_tty(state);

    ioctl(STDIN_FILENO, TIOCGWINSZ, &state->winsize);

    if (!prompt)
        prompt = "";

    state->prompt_str = prompt;
    state->prompt_len = strlen(prompt);

    state->total_scroll_rows = 1;
    state->current_pos = 0;
    state->next_current_pos = 0;

    state->complete_line_len = 0;
    state->complete_line_alloc_len = 1;
    state->complete_line = malloc(1);
    *state->complete_line = 0;

    readline_lite_render_display(state);
}

void readline_lite_display_fini(struct readline_state *state)
{
    /* Reset cursor to the end of the last line */
    int i;
    int cur_row = (state->prompt_len + state->current_pos) / state->winsize.ws_col;
    int cur_col = (state->prompt_len + state->current_pos) % state->winsize.ws_col;

    size_t comp_len = strlen(state->complete_line);
    int total_lines = (state->prompt_len + comp_len) / state->winsize.ws_col + 1;

    cursor_reset_column();
    for (i = cur_row; i < total_lines - 1; i++)
        cursor_move_down();

    if (cur_col)
        cursor_move_to_column(cur_col);

    /* Output one final newline, since the user finished entering text */
    cursor_move_down();

    readline_lite_restore_tty(state);
}
