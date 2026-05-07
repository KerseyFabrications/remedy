/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SYNTAX_HIGHLIGHT_H
#define SYNTAX_HIGHLIGHT_H

#include "render_line.h"

/**
 * @brief Highlight a code block and append styled lines to the buffer
 *
 * Tokenizes the code text using a simple keyword-based highlighter for
 * the specified language. Falls back to plain rendering if the language
 * is not recognized.
 *
 * @param code The code block text
 * @param language Language hint from the fence info (e.g. "c", "python"), may be NULL
 * @param indent Left indent in columns
 * @param buf Line buffer to append highlighted lines to
 * @return SUCCESS or FAILURE
 */
int syntax_highlight_render(const char *code, const char *language, int indent, int available_width, line_buffer_t *buf);

#endif /* SYNTAX_HIGHLIGHT_H */
