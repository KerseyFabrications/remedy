/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _GNU_SOURCE

#include "search.h"

#include <stdlib.h>
#include <string.h>

void search_init(search_state_t *state)
{
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(search_state_t));
}

size_t search_line_to_text(const rendered_line_t *line, char **out_text)
{
    if (!line || !out_text) {
        return 0;
    }

    size_t total_len = 0;
    for (size_t s = 0; s < line->span_count; s++) {
        if (line->spans[s].text) {
            total_len += line->spans[s].text_len;
        }
    }

    char *text = malloc(total_len + 1);
    if (!text) {
        return 0;
    }

    size_t pos = 0;
    for (size_t s = 0; s < line->span_count; s++) {
        if (line->spans[s].text && line->spans[s].text_len > 0) {
            memcpy(text + pos, line->spans[s].text, line->spans[s].text_len);
            pos += line->spans[s].text_len;
        }
    }
    text[pos] = '\0';

    *out_text = text;
    return total_len;
}

static bool line_contains_query(const rendered_line_t *line, const char *query)
{
    char *text = NULL;
    size_t len = search_line_to_text(line, &text);

    if (len == 0 || !text) {
        free(text);
        return false;
    }

    bool found = (strcasestr(text, query) != NULL);
    free(text);
    return found;
}

bool search_find(search_state_t *state, line_buffer_t *buf, size_t start_line, bool forward)
{
    if (!state || !buf || !state->query[0]) {
        return false;
    }

    state->forward = forward;

    if (forward) {
        for (size_t i = start_line; i < buf->line_count; i++) {
            if (line_contains_query(&buf->lines[i], state->query)) {
                state->match_line = i;
                return true;
            }
        }
        /* Wrap around */
        for (size_t i = 0; i < start_line && i < buf->line_count; i++) {
            if (line_contains_query(&buf->lines[i], state->query)) {
                state->match_line = i;
                return true;
            }
        }
    } else {
        if (start_line > 0) {
            for (size_t i = start_line - 1; i < buf->line_count; i--) {
                if (line_contains_query(&buf->lines[i], state->query)) {
                    state->match_line = i;
                    return true;
                }
                if (i == 0) {
                    break;
                }
            }
        }
        /* Wrap around */
        for (size_t i = buf->line_count - 1; i > start_line && i < buf->line_count; i--) {
            if (line_contains_query(&buf->lines[i], state->query)) {
                state->match_line = i;
                return true;
            }
        }
    }

    return false;
}

void search_highlight(search_state_t *state, line_buffer_t *buf)
{
    if (!state || !buf || !state->query[0]) {
        return;
    }

    size_t query_len   = strlen(state->query);
    state->match_count = 0;

    for (size_t i = 0; i < buf->line_count; i++) {
        rendered_line_t *line = &buf->lines[i];

        char *text = NULL;
        size_t len = search_line_to_text(line, &text);
        if (len == 0 || !text) {
            free(text);
            continue;
        }

        /* Check if this line has any matches */
        char *match = strcasestr(text, state->query);
        if (!match) {
            free(text);
            continue;
        }

        state->match_count++;

        /*
         * Rebuild spans with highlighting.
         * Map flat-text match offsets back to span boundaries.
         */
        size_t flat_offset       = 0;
        size_t new_span_count    = 0;
        size_t new_span_cap      = line->span_count * 3;
        styled_span_t *new_spans = calloc(new_span_cap, sizeof(styled_span_t));
        if (!new_spans) {
            free(text);
            continue;
        }

        /* Find all match positions in the flat text */
        size_t *match_starts   = NULL;
        size_t match_positions = 0;
        size_t match_cap       = 8;
        match_starts           = malloc(match_cap * sizeof(size_t));
        if (!match_starts) {
            free(text);
            free(new_spans);
            continue;
        }

        char *pos = text;
        while ((pos = strcasestr(pos, state->query)) != NULL) {
            if (match_positions >= match_cap) {
                match_cap *= 2;
                size_t *tmp = realloc(match_starts, match_cap * sizeof(size_t));
                if (!tmp) {
                    break;
                }
                match_starts = tmp;
            }
            match_starts[match_positions++] = (size_t) (pos - text);
            pos += query_len;
        }

        /* Walk spans and split at match boundaries */
        flat_offset = 0;
        for (size_t s = 0; s < line->span_count; s++) {
            styled_span_t *span = &line->spans[s];
            size_t span_start   = flat_offset;
            size_t span_end     = flat_offset + span->text_len;
            size_t cursor       = 0;

            while (cursor < span->text_len) {
                size_t abs_pos = span_start + cursor;

                /* Check if we're at a match start */
                bool in_match = false;
                for (size_t m = 0; m < match_positions; m++) {
                    if (abs_pos >= match_starts[m] && abs_pos < match_starts[m] + query_len) {
                        in_match = true;

                        /* Emit any non-match text before this */
                        size_t match_start_in_span = match_starts[m] > span_start ? match_starts[m] - span_start : 0;
                        if (match_start_in_span > cursor) {
                            size_t pre_len = match_start_in_span - cursor;

                            if (new_span_count >= new_span_cap) {
                                new_span_cap *= 2;
                                styled_span_t *tmp = realloc(new_spans, new_span_cap * sizeof(styled_span_t));
                                if (!tmp) {
                                    goto next_line;
                                }
                                new_spans = tmp;
                            }

                            char *pre_text = strndup(span->text + cursor, pre_len);
                            if (pre_text) {
                                new_spans[new_span_count] = styled_span_create(pre_text, span->style, span->color);
                                free(pre_text);
                                new_spans[new_span_count].heading_level = span->heading_level;
                                if (span->link_url) {
                                    new_spans[new_span_count].link_url = strdup(span->link_url);
                                }
                                new_span_count++;
                            }
                            cursor = match_start_in_span;
                        }

                        /* Emit highlighted portion (within this span) */
                        size_t match_end_abs = match_starts[m] + query_len;
                        size_t hl_end        = match_end_abs < span_end ? match_end_abs - span_start : span->text_len;
                        size_t hl_len        = hl_end - cursor;

                        if (hl_len > 0) {
                            if (new_span_count >= new_span_cap) {
                                new_span_cap *= 2;
                                styled_span_t *tmp = realloc(new_spans, new_span_cap * sizeof(styled_span_t));
                                if (!tmp) {
                                    goto next_line;
                                }
                                new_spans = tmp;
                            }

                            char *hl_text = strndup(span->text + cursor, hl_len);
                            if (hl_text) {
                                new_spans[new_span_count] =
                                    styled_span_create(hl_text, span->style | STYLE_SEARCH_HIT, COLOR_SEARCH_HIT);
                                free(hl_text);
                                new_spans[new_span_count].heading_level = span->heading_level;
                                if (span->link_url) {
                                    new_spans[new_span_count].link_url = strdup(span->link_url);
                                }
                                new_span_count++;
                            }
                            cursor = hl_end;
                        }

                        break;
                    }
                }

                if (!in_match) {
                    /* Find next match start within this span */
                    size_t next_match_in_span = span->text_len;
                    for (size_t m = 0; m < match_positions; m++) {
                        if (match_starts[m] > abs_pos && match_starts[m] < span_end) {
                            size_t offset_in_span = match_starts[m] - span_start;
                            if (offset_in_span < next_match_in_span) {
                                next_match_in_span = offset_in_span;
                            }
                        }
                    }

                    size_t normal_len = next_match_in_span - cursor;

                    if (new_span_count >= new_span_cap) {
                        new_span_cap *= 2;
                        styled_span_t *tmp = realloc(new_spans, new_span_cap * sizeof(styled_span_t));
                        if (!tmp) {
                            goto next_line;
                        }
                        new_spans = tmp;
                    }

                    char *normal_text = strndup(span->text + cursor, normal_len);
                    if (normal_text) {
                        new_spans[new_span_count] = styled_span_create(normal_text, span->style, span->color);
                        free(normal_text);
                        new_spans[new_span_count].heading_level = span->heading_level;
                        if (span->link_url) {
                            new_spans[new_span_count].link_url = strdup(span->link_url);
                        }
                        new_span_count++;
                    }
                    cursor = next_match_in_span;
                }
            }

            flat_offset = span_end;
        }

        /* Replace the line's spans */
        for (size_t s = 0; s < line->span_count; s++) {
            styled_span_destroy(&line->spans[s]);
        }
        free(line->spans);
        line->spans         = new_spans;
        line->span_count    = new_span_count;
        line->span_capacity = new_span_cap;

        free(match_starts);
        free(text);
        continue;

next_line:
        for (size_t j = 0; j < new_span_count; j++) {
            styled_span_destroy(&new_spans[j]);
        }
        free(new_spans);
        free(match_starts);
        free(text);
    }
}

void search_clear_highlights(line_buffer_t *buf)
{
    if (!buf) {
        return;
    }

    for (size_t i = 0; i < buf->line_count; i++) {
        rendered_line_t *line = &buf->lines[i];

        /* Check if this line has any highlighted spans */
        bool has_highlight = false;
        for (size_t s = 0; s < line->span_count; s++) {
            if (line->spans[s].style & STYLE_SEARCH_HIT) {
                has_highlight = true;
                break;
            }
        }

        if (!has_highlight) {
            continue;
        }

        /* Merge adjacent spans with the same style (minus search hit) back together */
        for (size_t s = 0; s < line->span_count; s++) {
            line->spans[s].style &= ~STYLE_SEARCH_HIT;
            if (line->spans[s].color == COLOR_SEARCH_HIT) {
                line->spans[s].color = COLOR_NORMAL;
            }
        }

        /* Merge adjacent spans with identical style and color */
        size_t write = 0;
        for (size_t s = 0; s < line->span_count; s++) {
            if (write > 0 && line->spans[write - 1].style == line->spans[s].style && line->spans[write - 1].color == line->spans[s].color) {
                /* Merge */
                size_t old_len = line->spans[write - 1].text_len;
                size_t add_len = line->spans[s].text_len;
                char *merged   = malloc(old_len + add_len + 1);
                if (merged) {
                    memcpy(merged, line->spans[write - 1].text, old_len);
                    memcpy(merged + old_len, line->spans[s].text, add_len);
                    merged[old_len + add_len] = '\0';
                    free(line->spans[write - 1].text);
                    line->spans[write - 1].text     = merged;
                    line->spans[write - 1].text_len = old_len + add_len;
                    line->spans[write - 1].display_width += line->spans[s].display_width;
                    styled_span_destroy(&line->spans[s]);
                } else {
                    /* Merge failed — keep as separate span */
                    if (write != s) {
                        line->spans[write] = line->spans[s];
                    }
                    write++;
                }
            } else {
                if (write != s) {
                    line->spans[write] = line->spans[s];
                }
                write++;
            }
        }
        line->span_count = write;
    }
}
