/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SEARCH_H
#define SEARCH_H

#include "render_line.h"

#include <stddef.h>

#define SEARCH_QUERY_MAX 256

typedef struct {
    char query[SEARCH_QUERY_MAX];
    bool active;
    bool forward;
    size_t match_line;
    size_t match_count;
} search_state_t;

/**
 * @brief Initialize search state
 */
void search_init(search_state_t *state);

/**
 * @brief Find the next match starting from the given line
 *
 * @param state Search state with query set
 * @param buf Line buffer to search
 * @param start_line Line to start searching from
 * @param forward true = search forward, false = search backward
 * @return true if a match was found (state->match_line updated)
 */
bool search_find(search_state_t *state, line_buffer_t *buf, size_t start_line, bool forward);

/**
 * @brief Apply search highlights to all matching lines
 *
 * @param state Search state with query set
 * @param buf Line buffer to mark matches in
 */
void search_highlight(search_state_t *state, line_buffer_t *buf);

/**
 * @brief Clear all search highlights from the line buffer
 *
 * @param buf Line buffer to clear highlights from
 */
void search_clear_highlights(line_buffer_t *buf);

/**
 * @brief Build flat text from a rendered line for searching
 *
 * @param line The rendered line
 * @param out_text Output heap-allocated text (caller must free)
 * @return Length of the text, or 0 on failure
 */
size_t search_line_to_text(const rendered_line_t *line, char **out_text);

#endif /* SEARCH_H */
