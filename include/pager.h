/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PAGER_H
#define PAGER_H

#include "lint.h"
#include "render_line.h"
#include "search.h"

#define MAX_TRACKED_IMAGES 64

typedef struct {
    line_buffer_t *buf;
    toc_t *toc;
    size_t scroll_offset;
    int term_width;
    int term_height;
    const char *filename;
    search_state_t search;
    lint_results_t *lint;
    bool images_transmitted[MAX_TRACKED_IMAGES];
    int images_cols[MAX_TRACKED_IMAGES];
    int images_rows[MAX_TRACKED_IMAGES];
} pager_state_t;

/**
 * @brief Initialize ncurses and the pager state
 *
 * @param state Pager state to initialize
 * @param buf Rendered line buffer to display
 * @param toc Table of contents (may be NULL)
 * @param filename Filename to show in status bar (may be NULL for stdin)
 * @return SUCCESS or FAILURE
 */
int pager_init(pager_state_t *state, line_buffer_t *buf, toc_t *toc, const char *filename);

/**
 * @brief Shut down ncurses and clean up
 *
 * @param state Pager state to clean up
 */
void pager_shutdown(pager_state_t *state);

/**
 * @brief Draw the current viewport to the screen
 *
 * @param state Current pager state
 */
void pager_draw(pager_state_t *state);

/**
 * @brief Initialize ncurses color pairs
 */
void pager_init_colors(void);

/* Scroll operations */
void pager_scroll_down(pager_state_t *state, int lines);
void pager_scroll_up(pager_state_t *state, int lines);
void pager_page_down(pager_state_t *state);
void pager_page_up(pager_state_t *state);
void pager_half_page_down(pager_state_t *state);
void pager_half_page_up(pager_state_t *state);
void pager_home(pager_state_t *state);
void pager_end(pager_state_t *state);

/**
 * @brief Handle terminal resize
 *
 * @param state Pager state
 */
void pager_handle_resize(pager_state_t *state);

/**
 * @brief Enter search mode — draws prompt and collects query
 *
 * @param state Pager state
 * @param forward true for '/' forward search, false for '?' backward
 */
void pager_search_prompt(pager_state_t *state, bool forward);

/**
 * @brief Find and scroll to next/previous search match
 *
 * @param state Pager state
 * @param forward true for next, false for previous
 */
void pager_search_next(pager_state_t *state, bool forward);

/**
 * @brief Show the table of contents overlay
 *
 * @param state Pager state
 */
void pager_show_toc(pager_state_t *state);

/**
 * @brief Show the help overlay
 *
 * @param state Pager state
 */
void pager_show_help(pager_state_t *state);

/**
 * @brief Show the diagnosis overlay with lint results
 */
void pager_show_diagnose(pager_state_t *state);

#endif /* PAGER_H */
