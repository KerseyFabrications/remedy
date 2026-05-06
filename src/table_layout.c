/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _XOPEN_SOURCE 700

#include "table_layout.h"

#include <cmark-gfm-extension_api.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int display_width(const char *text)
{
    if (!text) {
        return 0;
    }

    int width  = 0;
    size_t len = strlen(text);
    size_t i   = 0;

    while (i < len) {
        unsigned char c = (unsigned char) text[i];
        if (c < 0x80) {
            width++;
            i++;
        } else {
            wchar_t wc;
            mbstate_t state;
            memset(&state, 0, sizeof(state));
            size_t consumed = mbrtowc(&wc, text + i, len - i, &state);
            if (consumed == (size_t) -1 || consumed == (size_t) -2) {
                width++;
                i++;
            } else {
                int w = wcwidth(wc);
                if (w > 0) {
                    width += w;
                }
                i += consumed;
            }
        }
    }

    return width;
}

#define MAX_COLUMNS   64
#define MIN_COL_WIDTH 3

typedef enum {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT,
} col_align_t;

static char *get_cell_text(cmark_node *cell)
{
    char *result = NULL;
    size_t len   = 0;
    size_t cap   = 64;

    result = malloc(cap);
    if (!result) {
        return NULL;
    }
    result[0] = '\0';

    cmark_iter *iter = cmark_iter_new(cell);
    cmark_event_type ev;

    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node     = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        if (ev == CMARK_EVENT_ENTER && type == CMARK_NODE_TEXT) {
            const char *text = cmark_node_get_literal(node);
            size_t text_len  = text ? strlen(text) : 0;

            if (len + text_len >= cap) {
                cap       = (len + text_len + 1) * 2;
                char *tmp = realloc(result, cap);
                if (!tmp) {
                    break;
                }
                result = tmp;
            }

            memcpy(result + len, text, text_len);
            len += text_len;
            result[len] = '\0';
        } else if (ev == CMARK_EVENT_ENTER && type == CMARK_NODE_CODE) {
            const char *text = cmark_node_get_literal(node);
            size_t text_len  = text ? strlen(text) : 0;

            if (len + text_len >= cap) {
                cap       = (len + text_len + 1) * 2;
                char *tmp = realloc(result, cap);
                if (!tmp) {
                    break;
                }
                result = tmp;
            }

            memcpy(result + len, text, text_len);
            len += text_len;
            result[len] = '\0';
        }
    }

    cmark_iter_free(iter);
    return result;
}

static void emit_border_line(line_buffer_t *buf,
                             int indent,
                             const int *col_widths,
                             int num_cols,
                             const char *left,
                             const char *mid,
                             const char *right,
                             const char *fill)
{
    rendered_line_t *line = line_buffer_append_line(buf);
    if (!line) {
        return;
    }
    line->indent = indent;

    size_t total_len = strlen(left) + strlen(right);
    for (int c = 0; c < num_cols; c++) {
        total_len += (size_t) col_widths[c] * strlen(fill) + 2 * strlen(fill);
        if (c < num_cols - 1) {
            total_len += strlen(mid);
        }
    }

    char *border = malloc(total_len + 1);
    if (!border) {
        return;
    }

    size_t pos      = 0;
    size_t left_len = strlen(left);
    memcpy(border + pos, left, left_len);
    pos += left_len;

    for (int c = 0; c < num_cols; c++) {
        size_t fill_len = strlen(fill);
        for (int i = 0; i < col_widths[c] + 2; i++) {
            memcpy(border + pos, fill, fill_len);
            pos += fill_len;
        }
        if (c < num_cols - 1) {
            size_t mid_len = strlen(mid);
            memcpy(border + pos, mid, mid_len);
            pos += mid_len;
        }
    }

    size_t right_len = strlen(right);
    memcpy(border + pos, right, right_len);
    pos += right_len;
    border[pos] = '\0';

    styled_span_t span = styled_span_create(border, STYLE_TABLE_BORDER, COLOR_TABLE_BORDER);
    rendered_line_append_span(line, &span);
    free(border);
}

static void emit_data_row(line_buffer_t *buf,
                          int indent,
                          char **cells,
                          int num_cols,
                          const int *col_widths,
                          const col_align_t *alignments,
                          bool is_header)
{
    rendered_line_t *line = line_buffer_append_line(buf);
    if (!line) {
        return;
    }
    line->indent = indent;

    /* Left border */
    styled_span_t border_span = styled_span_create("\xe2\x94\x82 ", STYLE_TABLE_BORDER, COLOR_TABLE_BORDER);
    rendered_line_append_span(line, &border_span);

    for (int c = 0; c < num_cols; c++) {
        const char *text    = (cells && cells[c]) ? cells[c] : "";
        int text_disp_width = display_width(text);
        int width           = col_widths[c];
        int padding         = width - text_disp_width;

        if (padding < 0) {
            padding = 0;
        }

        int pad_left  = 0;
        int pad_right = 0;

        switch (alignments[c]) {
            case ALIGN_RIGHT:
                pad_left = padding;
                break;
            case ALIGN_CENTER:
                pad_left  = padding / 2;
                pad_right = padding - pad_left;
                break;
            case ALIGN_LEFT:
            default:
                pad_right = padding;
                break;
        }

        /* Build padded cell content: left_spaces + text + right_spaces */
        size_t text_bytes   = strlen(text);
        size_t cell_buf_len = (size_t) pad_left + text_bytes + (size_t) pad_right + 1;
        char *cell_buf      = malloc(cell_buf_len + 1);
        if (cell_buf) {
            size_t pos = 0;
            for (int p = 0; p < pad_left; p++) {
                cell_buf[pos++] = ' ';
            }
            memcpy(cell_buf + pos, text, text_bytes);
            pos += text_bytes;
            for (int p = 0; p < pad_right; p++) {
                cell_buf[pos++] = ' ';
            }
            cell_buf[pos] = '\0';

            style_flags_t style = is_header ? STYLE_BOLD : STYLE_NORMAL;
            color_id_t color    = is_header ? COLOR_HEADING3 : COLOR_NORMAL;

            styled_span_t cell_span = styled_span_create(cell_buf, style, color);
            rendered_line_append_span(line, &cell_span);
            free(cell_buf);
        }

        /* Column separator or right border */
        if (c < num_cols - 1) {
            styled_span_t sep = styled_span_create(" \xe2\x94\x82 ", STYLE_TABLE_BORDER, COLOR_TABLE_BORDER);
            rendered_line_append_span(line, &sep);
        } else {
            styled_span_t sep = styled_span_create(" \xe2\x94\x82", STYLE_TABLE_BORDER, COLOR_TABLE_BORDER);
            rendered_line_append_span(line, &sep);
        }
    }
}

int table_layout_render(cmark_node *table_node, int indent, int terminal_width, line_buffer_t *buf)
{
    if (!table_node || !buf) {
        return FAILURE;
    }

    /* Count columns and rows, collect cell text */
    int num_cols = 0;
    int num_rows = 0;

    /* First pass: count */
    cmark_node *row = cmark_node_first_child(table_node);
    while (row) {
        num_rows++;
        if (num_rows == 1) {
            cmark_node *cell = cmark_node_first_child(row);
            while (cell) {
                num_cols++;
                cell = cmark_node_next(cell);
            }
        }
        row = cmark_node_next(row);
    }

    if (num_cols == 0 || num_cols > MAX_COLUMNS || num_rows == 0) {
        return FAILURE;
    }

    /* Collect all cell texts */
    char ***rows_text = calloc((size_t) num_rows, sizeof(char **));
    if (!rows_text) {
        return FAILURE;
    }

    int r = 0;
    row   = cmark_node_first_child(table_node);
    while (row && r < num_rows) {
        rows_text[r] = calloc((size_t) num_cols, sizeof(char *));
        if (!rows_text[r]) {
            goto cleanup;
        }

        int c            = 0;
        cmark_node *cell = cmark_node_first_child(row);
        while (cell && c < num_cols) {
            rows_text[r][c] = get_cell_text(cell);
            c++;
            cell = cmark_node_next(cell);
        }

        r++;
        row = cmark_node_next(row);
    }

    /* Calculate column widths */
    int col_widths[MAX_COLUMNS];
    memset(col_widths, 0, sizeof(col_widths));

    for (r = 0; r < num_rows; r++) {
        for (int c = 0; c < num_cols; c++) {
            if (rows_text[r] && rows_text[r][c]) {
                int dw = display_width(rows_text[r][c]);
                if (dw > col_widths[c]) {
                    col_widths[c] = dw;
                }
            }
        }
    }

    /* Enforce minimum widths */
    for (int c = 0; c < num_cols; c++) {
        if (col_widths[c] < MIN_COL_WIDTH) {
            col_widths[c] = MIN_COL_WIDTH;
        }
    }

    /* Constrain total width to terminal */
    int available = terminal_width - indent;
    /* Each col takes: col_width + 3 (padding + border), plus 1 for left border */
    int total_width = 1;
    for (int c = 0; c < num_cols; c++) {
        total_width += col_widths[c] + 3;
    }

    if (total_width > available && available > num_cols * (MIN_COL_WIDTH + 3) + 1) {
        int excess = total_width - available;
        while (excess > 0) {
            int widest     = 0;
            int widest_idx = 0;
            for (int c = 0; c < num_cols; c++) {
                if (col_widths[c] > widest) {
                    widest     = col_widths[c];
                    widest_idx = c;
                }
            }
            if (widest <= MIN_COL_WIDTH) {
                break;
            }
            col_widths[widest_idx]--;
            excess--;
        }
    }

    /* Get alignments */
    col_align_t alignments[MAX_COLUMNS];
    for (int c = 0; c < num_cols; c++) {
        alignments[c] = ALIGN_LEFT;
    }

    /* UTF-8 box-drawing characters */
    /* clang-format off */
    const char *top_left     = "\xe2\x94\x8c";  /* ┌ */
    const char *top_mid      = "\xe2\x94\xac";  /* ┬ */
    const char *top_right    = "\xe2\x94\x90";  /* ┐ */
    const char *mid_left     = "\xe2\x94\x9c";  /* ├ */
    const char *mid_mid      = "\xe2\x94\xbc";  /* ┼ */
    const char *mid_right    = "\xe2\x94\xa4";  /* ┤ */
    const char *bot_left     = "\xe2\x94\x94";  /* └ */
    const char *bot_mid      = "\xe2\x94\xb4";  /* ┴ */
    const char *bot_right    = "\xe2\x94\x98";  /* ┘ */
    const char *horiz        = "\xe2\x94\x80";  /* ─ */
    /* clang-format on */

    /* Top border */
    emit_border_line(buf, indent, col_widths, num_cols, top_left, top_mid, top_right, horiz);

    /* Header row */
    if (num_rows > 0 && rows_text[0]) {
        emit_data_row(buf, indent, rows_text[0], num_cols, col_widths, alignments, true);
        emit_border_line(buf, indent, col_widths, num_cols, mid_left, mid_mid, mid_right, horiz);
    }

    /* Data rows */
    for (r = 1; r < num_rows; r++) {
        emit_data_row(buf, indent, rows_text[r], num_cols, col_widths, alignments, false);
    }

    /* Bottom border */
    emit_border_line(buf, indent, col_widths, num_cols, bot_left, bot_mid, bot_right, horiz);

cleanup:
    for (r = 0; r < num_rows; r++) {
        if (rows_text[r]) {
            for (int c = 0; c < num_cols; c++) {
                free(rows_text[r][c]);
            }
            free(rows_text[r]);
        }
    }
    free(rows_text);

    return SUCCESS;
}
