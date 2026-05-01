/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "render_line.h"

#include <cmark-gfm.h>

/**
 * @brief Render a cmark AST into styled lines
 *
 * Walks the AST and produces a line buffer of styled spans suitable for
 * display in the ncurses pager.
 *
 * @param doc AST root node from md_parser_parse()
 * @param terminal_width Current terminal width in columns
 * @param buf Output line buffer (must be initialized)
 * @param toc Output table of contents (must be initialized, may be NULL)
 * @param base_dir Base directory for resolving relative image paths (may be NULL)
 * @return SUCCESS or FAILURE
 */
int renderer_render(cmark_node *doc, int terminal_width, line_buffer_t *buf, toc_t *toc, const char *base_dir);

/**
 * @brief Word-wrap lines that exceed terminal width
 *
 * Splits lines at word boundaries, preserving span styles across splits.
 * Continuation lines are marked with is_continuation = true.
 *
 * @param buf Line buffer to wrap in-place
 * @param terminal_width Maximum line width in columns
 * @return SUCCESS or FAILURE
 */
int renderer_word_wrap(line_buffer_t *buf, int terminal_width);

/**
 * @brief Strip word-wrap continuation lines before re-wrapping
 *
 * @param buf Line buffer to strip
 */
void renderer_strip_continuations(line_buffer_t *buf);

#endif /* RENDERER_H */
