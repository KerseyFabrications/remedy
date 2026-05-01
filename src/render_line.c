/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _XOPEN_SOURCE 700

#include "render_line.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define INITIAL_LINE_CAPACITY 256
#define INITIAL_SPAN_CAPACITY 8
#define INITIAL_TOC_CAPACITY  32

static int compute_display_width(const char *text, size_t len)
{
    int width = 0;
    size_t i  = 0;

    while (i < len) {
        unsigned char c = (unsigned char) text[i];

        if (c < 0x80) {
            if (c == '\t') {
                width += 8 - (width % 8);
            } else if (c >= 0x20) {
                width++;
            }
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

int line_buffer_init(line_buffer_t *buf)
{
    if (!buf) {
        return FAILURE;
    }

    buf->lines = calloc(INITIAL_LINE_CAPACITY, sizeof(rendered_line_t));
    if (!buf->lines) {
        return FAILURE;
    }

    buf->line_count    = 0;
    buf->line_capacity = INITIAL_LINE_CAPACITY;

    return SUCCESS;
}

void line_buffer_destroy(line_buffer_t *buf)
{
    if (!buf) {
        return;
    }

    for (size_t i = 0; i < buf->line_count; i++) {
        rendered_line_destroy(&buf->lines[i]);
    }

    free(buf->lines);
    buf->lines         = NULL;
    buf->line_count    = 0;
    buf->line_capacity = 0;
}

void line_buffer_clear(line_buffer_t *buf)
{
    if (!buf) {
        return;
    }

    for (size_t i = 0; i < buf->line_count; i++) {
        rendered_line_destroy(&buf->lines[i]);
    }

    buf->line_count = 0;
}

rendered_line_t *line_buffer_append_line(line_buffer_t *buf)
{
    if (!buf) {
        return NULL;
    }

    if (buf->line_count >= buf->line_capacity) {
        size_t new_capacity        = buf->line_capacity * 2;
        rendered_line_t *new_lines = realloc(buf->lines, new_capacity * sizeof(rendered_line_t));
        if (!new_lines) {
            return NULL;
        }
        buf->lines         = new_lines;
        buf->line_capacity = new_capacity;
    }

    rendered_line_t *line = &buf->lines[buf->line_count];
    memset(line, 0, sizeof(rendered_line_t));
    buf->line_count++;

    return line;
}

styled_span_t styled_span_create(const char *text, style_flags_t style, color_id_t color)
{
    styled_span_t span;
    memset(&span, 0, sizeof(span));

    if (text) {
        span.text_len      = strlen(text);
        span.text          = strndup(text, span.text_len);
        span.display_width = compute_display_width(text, span.text_len);
    }

    span.style = style;
    span.color = color;

    return span;
}

void styled_span_destroy(styled_span_t *span)
{
    if (!span) {
        return;
    }

    free(span->text);
    span->text = NULL;

    free(span->link_url);
    span->link_url = NULL;
}

int rendered_line_append_span(rendered_line_t *line, const styled_span_t *span)
{
    if (!line || !span) {
        return FAILURE;
    }

    if (line->span_count >= line->span_capacity) {
        size_t new_capacity      = line->span_capacity == 0 ? INITIAL_SPAN_CAPACITY : line->span_capacity * 2;
        styled_span_t *new_spans = realloc(line->spans, new_capacity * sizeof(styled_span_t));
        if (!new_spans) {
            return FAILURE;
        }
        line->spans         = new_spans;
        line->span_capacity = new_capacity;
    }

    line->spans[line->span_count] = *span;
    line->span_count++;

    return SUCCESS;
}

void rendered_line_destroy(rendered_line_t *line)
{
    if (!line) {
        return;
    }

    for (size_t i = 0; i < line->span_count; i++) {
        styled_span_destroy(&line->spans[i]);
    }

    free(line->spans);
    line->spans         = NULL;
    line->span_count    = 0;
    line->span_capacity = 0;
}

int toc_init(toc_t *toc)
{
    if (!toc) {
        return FAILURE;
    }

    toc->entries = calloc(INITIAL_TOC_CAPACITY, sizeof(toc_entry_t));
    if (!toc->entries) {
        return FAILURE;
    }

    toc->count    = 0;
    toc->capacity = INITIAL_TOC_CAPACITY;

    return SUCCESS;
}

void toc_destroy(toc_t *toc)
{
    if (!toc) {
        return;
    }

    for (size_t i = 0; i < toc->count; i++) {
        free(toc->entries[i].text);
    }

    free(toc->entries);
    toc->entries  = NULL;
    toc->count    = 0;
    toc->capacity = 0;
}

int toc_add_entry(toc_t *toc, int level, const char *text, size_t line_index)
{
    if (!toc || !text) {
        return FAILURE;
    }

    if (toc->count >= toc->capacity) {
        size_t new_capacity      = toc->capacity * 2;
        toc_entry_t *new_entries = realloc(toc->entries, new_capacity * sizeof(toc_entry_t));
        if (!new_entries) {
            return FAILURE;
        }
        toc->entries  = new_entries;
        toc->capacity = new_capacity;
    }

    toc_entry_t *entry = &toc->entries[toc->count];
    entry->level       = level;
    entry->text        = strdup(text);
    entry->line_index  = line_index;

    if (!entry->text) {
        return FAILURE;
    }

    toc->count++;

    return SUCCESS;
}
