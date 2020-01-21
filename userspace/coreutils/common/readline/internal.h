#ifndef SRC_COREUTILS_COMMON_READLINE_INTERNAL_H
#define SRC_COREUTILS_COMMON_READLINE_INTERNAL_H

struct readline_state {
    /* These are the current 'cursor' sosition within the complete line */
    int current_pos;
    int next_current_pos;

    /* The number of rows the display currently spans. We increase this when we
     * have to scroll the screen. */
    int total_scroll_rows;

    /* Size of the terminal, this is necessary so we know the width of each line */
    struct winsize winsize;

    /* The complete input line, this is what eventually gets returned when we're
     * done entering input */
    size_t complete_line_len;
    size_t complete_line_alloc_len;
    char *complete_line;

    /* Length in characters of the initial prompt */
    size_t prompt_len;
    const char *prompt_str;

    struct termios saved_tty_settings;
};

struct history_entry {
    list_node_t entry;
    char *line;
};

struct history_entry *history_get_next(struct history_entry *current);
struct history_entry *history_get_previous(struct history_entry *current);

void history_delete(struct history_entry *);
void history_add(const char *line);

/* Sets/clears various TTY settings to set the terminal in 'raw' mode.
 * The two big things this achieves is turn off buffering, and turn off echoing */
void readline_lite_setup_tty(struct readline_state *state);
void readline_lite_restore_tty(struct readline_state *state);

/* 'Refresh' the currently displayed line */
void readline_lite_render_display(struct readline_state *state);

/* Add or delete a character at the current cursor position */
void readline_lite_char_add(struct readline_state *state, char c);
void readline_lite_char_del(struct readline_state *state);

/* Initialize display - reset state variable, setup prompt, setup tty, and
 * render first screen */
void readline_lite_display_init(struct readline_state *state, const char *prompt);

/* Moves the cursor to the beginning of the first line past the last line of
 * input, and restores that previous TTY settings */
void readline_lite_display_fini(struct readline_state *state);

/* Swaps the current input line for a new one, refreshing the screen in the
 * process, and returns the previous line */
char *readline_lite_swap_line(struct readline_state *state, const char *new_line);

#endif
