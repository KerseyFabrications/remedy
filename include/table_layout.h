/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TABLE_LAYOUT_H
#define TABLE_LAYOUT_H

#include "render_line.h"

#include <cmark-gfm.h>

/**
 * @brief Render a GFM table node into styled lines
 *
 * Measures column widths, applies alignment, and renders using
 * box-drawing characters.
 *
 * @param table_node The CMARK_NODE_TABLE node
 * @param indent Left indent in columns
 * @param terminal_width Terminal width for constraining table width
 * @param buf Line buffer to append rendered lines to
 * @return SUCCESS or FAILURE
 */
int table_layout_render(cmark_node *table_node, int indent, int terminal_width, line_buffer_t *buf);

#endif /* TABLE_LAYOUT_H */
