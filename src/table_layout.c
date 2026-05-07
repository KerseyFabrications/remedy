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

/* Word-wrap text into lines of max_width display columns. Returns array of strdup'd lines. */
static char **wrap_cell_text(const char *text, int max_width, int *out_line_count)
{
    if (!text || max_width < 1) {
        *out_line_count = 1;
        char **lines    = calloc(1, sizeof(char *));
        if (lines) {
            lines[0] = strdup("");
        }
        return lines;
    }

    int cap      = 8;
    int count    = 0;
    char **lines = calloc((size_t) cap, sizeof(char *));
    if (!lines) {
        *out_line_count = 0;
        return NULL;
    }

    size_t len = strlen(text);
    size_t pos = 0;

    while (pos <= len) {
        if (pos == len) {
            if (count == 0) {
                lines[count++] = strdup("");
            }
            break;
        }

        int col           = 0;
        size_t line_start = pos;
        size_t last_space = pos;
        bool found_space  = false;
        size_t end        = pos;

        while (end < len && col < max_width) {
            if (text[end] == ' ') {
                last_space  = end;
                found_space = true;
            }
            col++;
            end++;
        }

        size_t break_at;
        if (end >= len) {
            break_at = len;
        } else if (found_space && last_space > line_start) {
            break_at = last_space;
        } else {
            break_at = end;
        }

        if (count >= cap) {
            cap *= 2;
            char **tmp = realloc(lines, (size_t) cap * sizeof(char *));
            if (!tmp) {
                break;
            }
            lines = tmp;
        }

        lines[count] = strndup(text + line_start, break_at - line_start);
        count++;

        pos = break_at;
        if (pos < len && text[pos] == ' ') {
            pos++;
        }
    }

    *out_line_count = count > 0 ? count : 1;
    if (count == 0) {
        lines[0]        = strdup("");
        *out_line_count = 1;
    }
    return lines;
}

static void emit_data_row(line_buffer_t *buf,
                          int indent,
                          char **cells,
                          int num_cols,
                          const int *col_widths,
                          const col_align_t *alignments,
                          bool is_header)
{
    /* Word-wrap each cell and find max line count for this row */
    char ***wrapped  = calloc((size_t) num_cols, sizeof(char **));
    int *wrap_counts = calloc((size_t) num_cols, sizeof(int));
    if (!wrapped || !wrap_counts) {
        free(wrapped);
        free(wrap_counts);
        return;
    }

    int max_lines = 1;
    for (int c = 0; c < num_cols; c++) {
        const char *text = (cells && cells[c]) ? cells[c] : "";
        wrapped[c]       = wrap_cell_text(text, col_widths[c], &wrap_counts[c]);
        if (wrap_counts[c] > max_lines) {
            max_lines = wrap_counts[c];
        }
    }

    style_flags_t cell_style = is_header ? STYLE_BOLD : STYLE_NORMAL;
    color_id_t cell_color    = is_header ? COLOR_HEADING3 : COLOR_NORMAL;

    /* Emit one display line per wrap line */
    for (int wl = 0; wl < max_lines; wl++) {
        rendered_line_t *line = line_buffer_append_line(buf);
        if (!line) {
            break;
        }
        line->indent = indent;

        styled_span_t border = styled_span_create("\xe2\x94\x82 ", STYLE_TABLE_BORDER, COLOR_TABLE_BORDER);
        rendered_line_append_span(line, &border);

        for (int c = 0; c < num_cols; c++) {
            const char *text = (wl < wrap_counts[c] && wrapped[c] && wrapped[c][wl]) ? wrapped[c][wl] : "";
            int text_dw      = display_width(text);
            int width        = col_widths[c];
            int padding      = width - text_dw;
            if (padding < 0) {
                padding = 0;
            }

            int pad_left  = 0;
            int pad_right = padding;

            if (wl == 0) {
                switch (alignments[c]) {
                    case ALIGN_RIGHT:
                        pad_left  = padding;
                        pad_right = 0;
                        break;
                    case ALIGN_CENTER:
                        pad_left  = padding / 2;
                        pad_right = padding - pad_left;
                        break;
                    default:
                        break;
                }
            }

            size_t text_bytes   = strlen(text);
            size_t cell_buf_len = (size_t) pad_left + text_bytes + (size_t) pad_right + 1;
            char *cell_buf      = malloc(cell_buf_len + 1);
            if (cell_buf) {
                size_t p = 0;
                for (int i = 0; i < pad_left; i++) {
                    cell_buf[p++] = ' ';
                }
                memcpy(cell_buf + p, text, text_bytes);
                p += text_bytes;
                for (int i = 0; i < pad_right; i++) {
                    cell_buf[p++] = ' ';
                }
                cell_buf[p] = '\0';

                styled_span_t cell_span = styled_span_create(cell_buf, cell_style, cell_color);
                rendered_line_append_span(line, &cell_span);
                free(cell_buf);
            }

            if (c < num_cols - 1) {
                styled_span_t sep = styled_span_create(" \xe2\x94\x82 ", STYLE_TABLE_BORDER, COLOR_TABLE_BORDER);
                rendered_line_append_span(line, &sep);
            } else {
                styled_span_t sep = styled_span_create(" \xe2\x94\x82", STYLE_TABLE_BORDER, COLOR_TABLE_BORDER);
                rendered_line_append_span(line, &sep);
            }
        }
    }

    /* Free wrapped text */
    for (int c = 0; c < num_cols; c++) {
        if (wrapped[c]) {
            for (int wl = 0; wl < wrap_counts[c]; wl++) {
                free(wrapped[c][wl]);
            }
            free(wrapped[c]);
        }
    }
    free(wrapped);
    free(wrap_counts);
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
    int overhead      = num_cols * 3 + 1;
    int content_avail = available - overhead;
    if (content_avail < num_cols * MIN_COL_WIDTH) {
        content_avail = num_cols * MIN_COL_WIDTH;
    }

    int total_content = 0;
    for (int c = 0; c < num_cols; c++) {
        total_content += col_widths[c];
    }

    if (total_content > content_avail) {
        /* Proportional distribution: each column gets its share of available space */
        for (int c = 0; c < num_cols; c++) {
            int proportional = (int) ((double) col_widths[c] / (double) total_content * (double) content_avail);
            if (proportional < MIN_COL_WIDTH) {
                proportional = MIN_COL_WIDTH;
            }
            col_widths[c] = proportional;
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

    /* Data rows with separator lines between them */
    for (r = 1; r < num_rows; r++) {
        if (r > 1) {
            emit_border_line(buf, indent, col_widths, num_cols, mid_left, mid_mid, mid_right, horiz);
        }
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
