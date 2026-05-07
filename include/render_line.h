/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RENDER_LINE_H
#define RENDER_LINE_H

#include "remedy.h"

#include <stddef.h>

typedef enum {
    STYLE_NORMAL        = 0,
    STYLE_BOLD          = (1 << 0),
    STYLE_ITALIC        = (1 << 1),
    STYLE_CODE          = (1 << 2),
    STYLE_STRIKETHROUGH = (1 << 3),
    STYLE_HEADING       = (1 << 4),
    STYLE_LINK          = (1 << 5),
    STYLE_BLOCKQUOTE    = (1 << 6),
    STYLE_SEARCH_HIT    = (1 << 7),
    STYLE_LIST_BULLET   = (1 << 8),
    STYLE_HR            = (1 << 9),
    STYLE_TABLE_BORDER  = (1 << 10),
    STYLE_PREFORMATTED  = (1 << 11),
} style_flags_t;

typedef enum {
    COLOR_NORMAL = 1,
    COLOR_HEADING1,
    COLOR_HEADING2,
    COLOR_HEADING3,
    COLOR_HEADING4,
    COLOR_HEADING5,
    COLOR_HEADING6,
    COLOR_CODE,
    COLOR_CODE_BLOCK,
    COLOR_BLOCKQUOTE,
    COLOR_LINK,
    COLOR_LINK_URL,
    COLOR_HR,
    COLOR_TABLE_BORDER,
    COLOR_SEARCH_HIT,
    COLOR_LIST_BULLET,
    COLOR_STATUS_BAR,
    COLOR_SYN_KEYWORD,
    COLOR_SYN_STRING,
    COLOR_SYN_COMMENT,
    COLOR_SYN_NUMBER,
    COLOR_SYN_TYPE,
    COLOR_SYN_FUNCTION,
    COLOR_SYN_PREPROC,
    COLOR_COUNT
} color_id_t;

typedef struct {
    char *text;
    size_t text_len;
    int display_width;
    style_flags_t style;
    color_id_t color;
    int heading_level;
    char *link_url;
} styled_span_t;

typedef struct {
    styled_span_t *spans;
    size_t span_count;
    size_t span_capacity;
    int indent;
    bool is_continuation;
} rendered_line_t;

typedef struct {
    rendered_line_t *lines;
    size_t line_count;
    size_t line_capacity;
} line_buffer_t;

typedef struct {
    int level;
    char *text;
    size_t line_index;
} toc_entry_t;

typedef struct {
    toc_entry_t *entries;
    size_t count;
    size_t capacity;
} toc_t;

/* Line buffer operations */
int line_buffer_init(line_buffer_t *buf);
void line_buffer_destroy(line_buffer_t *buf);
void line_buffer_clear(line_buffer_t *buf);

rendered_line_t *line_buffer_append_line(line_buffer_t *buf);

/* Rendered line operations */
int rendered_line_append_span(rendered_line_t *line, const styled_span_t *span);
void rendered_line_destroy(rendered_line_t *line);

/* Styled span helpers */
styled_span_t styled_span_create(const char *text, style_flags_t style, color_id_t color);
void styled_span_destroy(styled_span_t *span);

/* TOC operations */
int toc_init(toc_t *toc);
void toc_destroy(toc_t *toc);
int toc_add_entry(toc_t *toc, int level, const char *text, size_t line_index);

#endif /* RENDER_LINE_H */
