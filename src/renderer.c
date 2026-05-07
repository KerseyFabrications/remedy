/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _XOPEN_SOURCE 700

#include "renderer.h"

#include "kitty_graphics.h"
#include "syntax_highlight.h"
#include "table_layout.h"

#include <cmark-gfm-extension_api.h>
#include <cmark-gfm.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define MAX_STYLE_DEPTH 32

typedef struct {
    style_flags_t style;
    color_id_t color;
    int heading_level;
    char *link_url;
} style_entry_t;

typedef struct {
    style_entry_t stack[MAX_STYLE_DEPTH];
    int depth;
    int indent;
    int list_depth;
    int blockquote_depth;
    bool in_code_block;
    int terminal_width;
    line_buffer_t *buf;
    toc_t *toc;
    rendered_line_t *current_line;
    const char *base_dir;
    /* For collecting heading text */
    char heading_text[1024];
    size_t heading_text_len;
    bool in_heading;
    int current_heading_level;
} render_context_t;

static style_flags_t ctx_current_style(const render_context_t *ctx)
{
    if (ctx->depth <= 0) {
        return STYLE_NORMAL;
    }
    return ctx->stack[ctx->depth - 1].style;
}

static color_id_t ctx_current_color(const render_context_t *ctx)
{
    if (ctx->depth <= 0) {
        return COLOR_NORMAL;
    }
    return ctx->stack[ctx->depth - 1].color;
}

static void ctx_push_style(render_context_t *ctx, style_flags_t style, color_id_t color)
{
    if (ctx->depth >= MAX_STYLE_DEPTH) {
        return;
    }

    style_flags_t merged_style = ctx_current_style(ctx) | style;
    color_id_t use_color       = (color != COLOR_NORMAL) ? color : ctx_current_color(ctx);

    ctx->stack[ctx->depth].style         = merged_style;
    ctx->stack[ctx->depth].color         = use_color;
    ctx->stack[ctx->depth].heading_level = 0;
    ctx->stack[ctx->depth].link_url      = NULL;
    ctx->depth++;
}

static void ctx_pop_style(render_context_t *ctx)
{
    if (ctx->depth > 0) {
        ctx->depth--;
        free(ctx->stack[ctx->depth].link_url);
        ctx->stack[ctx->depth].link_url = NULL;
    }
}

static void ensure_current_line(render_context_t *ctx)
{
    if (!ctx->current_line) {
        ctx->current_line = line_buffer_append_line(ctx->buf);
        if (ctx->current_line) {
            ctx->current_line->indent = ctx->indent;
        }
    }
}

static void emit_span(render_context_t *ctx, const char *text)
{
    if (!text || !text[0]) {
        return;
    }

    ensure_current_line(ctx);
    if (!ctx->current_line) {
        return;
    }

    styled_span_t span = styled_span_create(text, ctx_current_style(ctx), ctx_current_color(ctx));

    if (ctx->in_heading) {
        span.heading_level = ctx->current_heading_level;
    }

    /* Copy link URL from style stack to span for OSC 8 rendering */
    if (ctx->depth > 0 && ctx->stack[ctx->depth - 1].link_url) {
        span.link_url = strdup(ctx->stack[ctx->depth - 1].link_url);
    }

    if (rendered_line_append_span(ctx->current_line, &span) != SUCCESS) {
        styled_span_destroy(&span);
    }
}

static void finalize_line(render_context_t *ctx)
{
    ctx->current_line = NULL;
}

static void emit_blank_line(render_context_t *ctx)
{
    finalize_line(ctx);
    line_buffer_append_line(ctx->buf);
    ctx->current_line = NULL;
}

static color_id_t heading_color(int level)
{
    switch (level) {
        case 1:
            return COLOR_HEADING1;
        case 2:
            return COLOR_HEADING2;
        case 3:
            return COLOR_HEADING3;
        case 4:
            return COLOR_HEADING4;
        case 5:
            return COLOR_HEADING5;
        case 6:
            return COLOR_HEADING6;
        default:
            return COLOR_HEADING1;
    }
}

static void collect_heading_text(render_context_t *ctx, const char *text)
{
    if (!ctx->in_heading || !text) {
        return;
    }

    size_t text_len  = strlen(text);
    size_t remaining = sizeof(ctx->heading_text) - ctx->heading_text_len - 1;
    size_t to_copy   = text_len < remaining ? text_len : remaining;

    memcpy(ctx->heading_text + ctx->heading_text_len, text, to_copy);
    ctx->heading_text_len += to_copy;
    ctx->heading_text[ctx->heading_text_len] = '\0';
}

static void handle_node_enter(render_context_t *ctx, cmark_node *node, cmark_node_type type)
{
    switch (type) {
        case CMARK_NODE_HEADING: {
            int level                  = cmark_node_get_heading_level(node);
            ctx->current_heading_level = level;
            ctx->in_heading            = true;
            ctx->heading_text_len      = 0;
            ctx->heading_text[0]       = '\0';

            finalize_line(ctx);
            ensure_current_line(ctx);

            ctx_push_style(ctx, STYLE_BOLD | STYLE_HEADING, heading_color(level));

            if (level < 1) {
                level = 1;
            }
            if (level > 6) {
                level = 6;
            }
            char prefix[16];
            memset(prefix, '#', (size_t) level);
            prefix[level]     = ' ';
            prefix[level + 1] = '\0';
            emit_span(ctx, prefix);
            break;
        }

        case CMARK_NODE_PARAGRAPH:
            ensure_current_line(ctx);
            break;

        case CMARK_NODE_EMPH:
            ctx_push_style(ctx, STYLE_ITALIC, COLOR_NORMAL);
            break;

        case CMARK_NODE_STRONG:
            ctx_push_style(ctx, STYLE_BOLD, COLOR_NORMAL);
            break;

        case CMARK_NODE_CODE: {
            const char *literal = cmark_node_get_literal(node);
            if (literal) {
                ctx_push_style(ctx, STYLE_CODE, COLOR_CODE);
                emit_span(ctx, literal);
                ctx_pop_style(ctx);
            }
            break;
        }

        case CMARK_NODE_CODE_BLOCK: {
            const char *literal    = cmark_node_get_literal(node);
            const char *fence_info = cmark_node_get_fence_info(node);
            if (literal) {
                finalize_line(ctx);
                syntax_highlight_render(literal, fence_info, ctx->indent + 2, ctx->terminal_width, ctx->buf);
                ctx->current_line = NULL;
            }
            break;
        }

        case CMARK_NODE_BLOCK_QUOTE:
            ctx->blockquote_depth++;
            ctx->indent += 4;
            ctx_push_style(ctx, STYLE_BLOCKQUOTE, COLOR_BLOCKQUOTE);
            break;

        case CMARK_NODE_LIST:
            ctx->list_depth++;
            break;

        case CMARK_NODE_ITEM: {
            finalize_line(ctx);
            ensure_current_line(ctx);

            cmark_node *parent        = cmark_node_parent(node);
            cmark_list_type list_type = cmark_node_get_list_type(parent);

            ctx_push_style(ctx, STYLE_LIST_BULLET, COLOR_LIST_BULLET);
            if (list_type == CMARK_ORDERED_LIST) {
                int start           = cmark_node_get_list_start(parent);
                int index           = 0;
                cmark_node *sibling = cmark_node_first_child(parent);
                while (sibling && sibling != node) {
                    index++;
                    sibling = cmark_node_next(sibling);
                }
                char num_str[16];
                snprintf(num_str, sizeof(num_str), "%d. ", start + index);
                emit_span(ctx, num_str);
            } else {
                /* Cycle bullet style by nesting depth: bullet, circle, square */
                int depth = ctx->list_depth - 1;
                switch (depth % 3) {
                    case 0:
                        emit_span(ctx, "\xe2\x80\xa2 "); /* • BULLET */
                        break;
                    case 1:
                        emit_span(ctx, "\xe2\x97\xa6 "); /* ◦ WHITE BULLET */
                        break;
                    case 2:
                        emit_span(ctx, "\xe2\x96\xaa "); /* ▪ BLACK SMALL SQUARE */
                        break;
                }
            }
            ctx_pop_style(ctx);

            ctx->indent += 4;
            break;
        }

        case CMARK_NODE_LINK: {
            const char *url = cmark_node_get_url(node);
            ctx_push_style(ctx, STYLE_LINK, COLOR_LINK);
            if (url && ctx->depth > 0) {
                ctx->stack[ctx->depth - 1].link_url = strdup(url);
            }
            break;
        }

        case CMARK_NODE_IMAGE: {
            const char *url   = cmark_node_get_url(node);
            const char *title = cmark_node_get_title(node);
            const char *alt   = (title && title[0]) ? title : url;

            finalize_line(ctx);
            kitty_graphics_render(url, alt, ctx->base_dir, ctx->indent, ctx->buf);
            ctx->current_line = NULL;
            break;
        }

        case CMARK_NODE_THEMATIC_BREAK:
            finalize_line(ctx);
            ensure_current_line(ctx);
            ctx_push_style(ctx, STYLE_HR, COLOR_HR);
            {
                int hr_width = ctx->terminal_width - ctx->indent;
                if (hr_width < 3) {
                    hr_width = 3;
                }
                if (hr_width > 80) {
                    hr_width = 80;
                }
                /* UTF-8 for ─ is 3 bytes: 0xe2 0x94 0x80 */
                size_t buf_size = (size_t) hr_width * 3 + 1;
                char *hr_buf    = malloc(buf_size);
                if (hr_buf) {
                    size_t pos = 0;
                    for (int i = 0; i < hr_width; i++) {
                        hr_buf[pos++] = (char) 0xe2;
                        hr_buf[pos++] = (char) 0x94;
                        hr_buf[pos++] = (char) 0x80;
                    }
                    hr_buf[pos] = '\0';
                    emit_span(ctx, hr_buf);
                    free(hr_buf);
                }
            }
            ctx_pop_style(ctx);
            finalize_line(ctx);
            break;

        case CMARK_NODE_SOFTBREAK:
            emit_span(ctx, " ");
            break;

        case CMARK_NODE_LINEBREAK:
            finalize_line(ctx);
            ensure_current_line(ctx);
            if (ctx->current_line) {
                ctx->current_line->indent = ctx->indent;
            }
            break;

        case CMARK_NODE_TEXT: {
            const char *literal = cmark_node_get_literal(node);
            if (literal) {
                collect_heading_text(ctx, literal);
                emit_span(ctx, literal);
            }
            break;
        }

        case CMARK_NODE_HTML_BLOCK: {
            const char *literal = cmark_node_get_literal(node);
            if (literal) {
                finalize_line(ctx);
                ctx_push_style(ctx, STYLE_CODE, COLOR_CODE_BLOCK);
                emit_span(ctx, literal);
                ctx_pop_style(ctx);
                finalize_line(ctx);
            }
            break;
        }

        case CMARK_NODE_HTML_INLINE: {
            const char *literal = cmark_node_get_literal(node);
            if (literal) {
                ctx_push_style(ctx, STYLE_CODE, COLOR_CODE);
                emit_span(ctx, literal);
                ctx_pop_style(ctx);
            }
            break;
        }

        default: {
            /* Handle GFM extension nodes by type string */
            const char *type_str = cmark_node_get_type_string(node);
            if (type_str) {
                if (strcmp(type_str, "table") == 0) {
                    finalize_line(ctx);
                    table_layout_render(node, ctx->indent + 2, ctx->terminal_width, ctx->buf);
                    ctx->current_line = NULL;
                } else if (strcmp(type_str, "strikethrough") == 0) {
                    ctx_push_style(ctx, STYLE_STRIKETHROUGH, COLOR_NORMAL);
                }
            }
            break;
        }
    }
}

static void handle_node_exit(render_context_t *ctx, cmark_node *node, cmark_node_type type)
{
    switch (type) {
        case CMARK_NODE_HEADING:
            ctx_pop_style(ctx);
            finalize_line(ctx);

            if (ctx->toc && ctx->heading_text_len > 0) {
                size_t heading_line = ctx->buf->line_count > 0 ? ctx->buf->line_count - 1 : 0;
                toc_add_entry(ctx->toc, ctx->current_heading_level, ctx->heading_text, heading_line);
            }

            ctx->in_heading = false;
            emit_blank_line(ctx);
            break;

        case CMARK_NODE_PARAGRAPH:
            finalize_line(ctx);
            emit_blank_line(ctx);
            break;

        case CMARK_NODE_EMPH:
        case CMARK_NODE_STRONG:
            ctx_pop_style(ctx);
            break;

        case CMARK_NODE_BLOCK_QUOTE:
            ctx->blockquote_depth--;
            ctx->indent -= 4;
            ctx_pop_style(ctx);
            break;

        case CMARK_NODE_LIST:
            ctx->list_depth--;
            emit_blank_line(ctx);
            break;

        case CMARK_NODE_ITEM:
            ctx->indent -= 4;
            finalize_line(ctx);
            break;

        case CMARK_NODE_LINK: {
            /* Show URL after link text */
            const char *url = cmark_node_get_url(node);
            ctx_pop_style(ctx);
            if (url && url[0]) {
                ctx_push_style(ctx, STYLE_NORMAL, COLOR_LINK_URL);
                emit_span(ctx, " (");
                emit_span(ctx, url);
                emit_span(ctx, ")");
                ctx_pop_style(ctx);
            }
            break;
        }

        case CMARK_NODE_CODE_BLOCK:
            emit_blank_line(ctx);
            break;

        default: {
            const char *type_str = cmark_node_get_type_string(node);
            if (type_str) {
                if (strcmp(type_str, "table") == 0) {
                    emit_blank_line(ctx);
                } else if (strcmp(type_str, "strikethrough") == 0) {
                    ctx_pop_style(ctx);
                }
            }
            break;
        }
    }
}

int renderer_render(cmark_node *doc, int terminal_width, line_buffer_t *buf, toc_t *toc, const char *base_dir)
{
    if (!doc || !buf) {
        return FAILURE;
    }

    render_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.terminal_width = terminal_width;
    ctx.buf            = buf;
    ctx.toc            = toc;
    ctx.base_dir       = base_dir;

    cmark_iter *iter = cmark_iter_new(doc);
    if (!iter) {
        return FAILURE;
    }

    cmark_event_type ev_type;
    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node     = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        /* Skip children of table nodes — table_layout_render processes them */
        if (ev_type == CMARK_EVENT_ENTER) {
            /* Skip children of IMAGE — alt text is handled in the IMAGE enter handler */
            if (type == CMARK_NODE_IMAGE) {
                handle_node_enter(&ctx, node, type);
                cmark_iter_reset(iter, node, CMARK_EVENT_EXIT);
                continue;
            }

            const char *type_str = cmark_node_get_type_string(node);
            if (type_str
                && (strcmp(type_str, "table_row") == 0 || strcmp(type_str, "table_cell") == 0 || strcmp(type_str, "table_header") == 0)) {
                cmark_iter_reset(iter, node, CMARK_EVENT_EXIT);
                continue;
            }
        }
        if (ev_type == CMARK_EVENT_EXIT) {
            const char *type_str = cmark_node_get_type_string(node);
            if (type_str
                && (strcmp(type_str, "table_row") == 0 || strcmp(type_str, "table_cell") == 0 || strcmp(type_str, "table_header") == 0)) {
                continue;
            }
        }

        if (ev_type == CMARK_EVENT_ENTER) {
            handle_node_enter(&ctx, node, type);
        } else if (ev_type == CMARK_EVENT_EXIT) {
            handle_node_exit(&ctx, node, type);
        }
    }

    finalize_line(&ctx);
    cmark_iter_free(iter);

    return SUCCESS;
}

static void wrap_single_line(rendered_line_t *line, int available_width, line_buffer_t *out)
{
    int col                  = 0;
    size_t break_span_idx    = 0;
    size_t break_byte_offset = 0;
    bool found_break         = false;

    size_t last_space_span = 0;
    size_t last_space_byte = 0;
    bool has_space         = false;

    for (size_t s = 0; s < line->span_count && !found_break; s++) {
        const char *text = line->spans[s].text;
        size_t len       = line->spans[s].text_len;
        size_t b         = 0;

        while (b < len) {
            if (text[b] == ' ') {
                last_space_span = s;
                last_space_byte = b;
                has_space       = true;
                col++;
                b++;
            } else {
                unsigned char c = (unsigned char) text[b];
                int char_width  = 1;
                size_t char_len = 1;

                if (c >= 0x80) {
                    wchar_t wc;
                    mbstate_t state;
                    memset(&state, 0, sizeof(state));
                    size_t consumed = mbrtowc(&wc, text + b, len - b, &state);
                    if (consumed != (size_t) -1 && consumed != (size_t) -2 && consumed > 0) {
                        int w = wcwidth(wc);
                        if (w > 0) {
                            char_width = w;
                        }
                        char_len = consumed;
                    }
                }

                col += char_width;
                b += char_len;
            }

            if (col > available_width) {
                if (has_space) {
                    break_span_idx    = last_space_span;
                    break_byte_offset = last_space_byte;
                } else {
                    break_span_idx    = s;
                    break_byte_offset = b > 0 ? b - 1 : 0;
                }
                found_break = true;
                break;
            }
        }
    }

    if (!found_break) {
        rendered_line_t *out_line = line_buffer_append_line(out);
        if (out_line) {
            *out_line = *line;
        }
        line->spans      = NULL;
        line->span_count = 0;
        return;
    }

    /* Build the truncated first line */
    rendered_line_t *first = line_buffer_append_line(out);
    if (!first) {
        return;
    }

    first->indent          = line->indent;
    first->is_continuation = line->is_continuation;

    /* Copy spans up to the break point */
    for (size_t s = 0; s < break_span_idx; s++) {
        styled_span_t copy = styled_span_create(line->spans[s].text, line->spans[s].style, line->spans[s].color);
        copy.heading_level = line->spans[s].heading_level;
        if (line->spans[s].link_url) {
            copy.link_url = strdup(line->spans[s].link_url);
        }
        if (rendered_line_append_span(first, &copy) != SUCCESS) {
            styled_span_destroy(&copy);
        }
    }

    /* Truncate the break span */
    styled_span_t *break_span = &line->spans[break_span_idx];
    if (break_byte_offset > 0) {
        char *trunc_text = strndup(break_span->text, break_byte_offset);
        if (trunc_text) {
            styled_span_t truncated = styled_span_create(trunc_text, break_span->style, break_span->color);
            free(trunc_text);
            truncated.heading_level = break_span->heading_level;
            if (break_span->link_url) {
                truncated.link_url = strdup(break_span->link_url);
            }
            if (rendered_line_append_span(first, &truncated) != SUCCESS) {
                styled_span_destroy(&truncated);
            }
        }
    }

    /* Build the remainder line */
    rendered_line_t remainder_line;
    memset(&remainder_line, 0, sizeof(remainder_line));
    remainder_line.indent          = line->indent;
    remainder_line.is_continuation = true;

    size_t split_at = has_space ? break_byte_offset + 1 : break_byte_offset;
    if (split_at < break_span->text_len) {
        const char *rem_text   = break_span->text + split_at;
        styled_span_t rem_span = styled_span_create(rem_text, break_span->style, break_span->color);
        rem_span.heading_level = break_span->heading_level;
        if (break_span->link_url) {
            rem_span.link_url = strdup(break_span->link_url);
        }
        if (rendered_line_append_span(&remainder_line, &rem_span) != SUCCESS) {
            styled_span_destroy(&rem_span);
        }
    }

    for (size_t s = break_span_idx + 1; s < line->span_count; s++) {
        styled_span_t copy = styled_span_create(line->spans[s].text, line->spans[s].style, line->spans[s].color);
        copy.heading_level = line->spans[s].heading_level;
        if (line->spans[s].link_url) {
            copy.link_url = strdup(line->spans[s].link_url);
        }
        if (rendered_line_append_span(&remainder_line, &copy) != SUCCESS) {
            styled_span_destroy(&copy);
        }
    }

    /* Recursively wrap the remainder if still too long */
    int rem_width = 0;
    for (size_t s = 0; s < remainder_line.span_count; s++) {
        rem_width += remainder_line.spans[s].display_width;
    }

    if (rem_width > available_width) {
        wrap_single_line(&remainder_line, available_width, out);
        rendered_line_destroy(&remainder_line);
    } else {
        rendered_line_t *out_rem = line_buffer_append_line(out);
        if (out_rem) {
            *out_rem = remainder_line;
        } else {
            rendered_line_destroy(&remainder_line);
        }
    }

    /* Free original spans (ownership transferred to first/remainder) */
    for (size_t s = 0; s < line->span_count; s++) {
        styled_span_destroy(&line->spans[s]);
    }
    free(line->spans);
    line->spans      = NULL;
    line->span_count = 0;
}

int renderer_word_wrap(line_buffer_t *buf, int terminal_width)
{
    if (!buf || terminal_width <= 0) {
        return FAILURE;
    }

    /* Two-pass approach: build wrapped output into a new buffer, then swap */
    line_buffer_t out;
    if (line_buffer_init(&out) != SUCCESS) {
        return FAILURE;
    }

    for (size_t i = 0; i < buf->line_count; i++) {
        rendered_line_t *line = &buf->lines[i];
        int available_width   = terminal_width - line->indent;

        if (available_width < 10) {
            available_width = 10;
        }

        /* Skip wrapping for code/preformatted lines — they should truncate, not wrap */
        bool is_nowrap = false;
        if (line->span_count > 0) {
            is_nowrap = true;
            for (size_t s = 0; s < line->span_count; s++) {
                if (!(line->spans[s].style & (STYLE_CODE | STYLE_PREFORMATTED))) {
                    is_nowrap = false;
                    break;
                }
            }
        }

        int total_width = 0;
        for (size_t s = 0; s < line->span_count; s++) {
            total_width += line->spans[s].display_width;
        }

        if (is_nowrap || total_width <= available_width) {
            rendered_line_t *out_line = line_buffer_append_line(&out);
            if (out_line) {
                *out_line = *line;
            }
            line->spans      = NULL;
            line->span_count = 0;
        } else {
            wrap_single_line(line, available_width, &out);
        }
    }

    /* Swap the new buffer into the original */
    free(buf->lines);
    buf->lines         = out.lines;
    buf->line_count    = out.line_count;
    buf->line_capacity = out.line_capacity;

    return SUCCESS;
}

void renderer_strip_continuations(line_buffer_t *buf)
{
    if (!buf) {
        return;
    }

    size_t write = 0;
    for (size_t read = 0; read < buf->line_count; read++) {
        if (buf->lines[read].is_continuation && write > 0) {
            /* Merge continuation spans back into the parent line */
            rendered_line_t *parent = &buf->lines[write - 1];
            rendered_line_t *cont   = &buf->lines[read];

            for (size_t s = 0; s < cont->span_count; s++) {
                if (rendered_line_append_span(parent, &cont->spans[s]) != SUCCESS) {
                    for (size_t r = s; r < cont->span_count; r++) {
                        styled_span_destroy(&cont->spans[r]);
                    }
                    break;
                }
            }

            free(cont->spans);
            cont->spans         = NULL;
            cont->span_count    = 0;
            cont->span_capacity = 0;
        } else {
            if (write != read) {
                buf->lines[write] = buf->lines[read];
            }
            write++;
        }
    }

    buf->line_count = write;
}
