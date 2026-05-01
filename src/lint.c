/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lint.h"

#include <cmark-gfm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define INITIAL_CAPACITY 32

int lint_init(lint_results_t *results)
{
    if (!results) {
        return FAILURE;
    }

    results->issues = calloc(INITIAL_CAPACITY, sizeof(lint_issue_t));
    if (!results->issues) {
        return FAILURE;
    }

    results->count    = 0;
    results->capacity = INITIAL_CAPACITY;

    return SUCCESS;
}

void lint_destroy(lint_results_t *results)
{
    if (!results) {
        return;
    }

    for (size_t i = 0; i < results->count; i++) {
        free(results->issues[i].message);
    }

    free(results->issues);
    results->issues   = NULL;
    results->count    = 0;
    results->capacity = 0;
}

int lint_add(lint_results_t *results, lint_severity_t severity, int line, const char *message)
{
    if (!results || !message) {
        return FAILURE;
    }

    if (results->count >= results->capacity) {
        size_t new_cap        = results->capacity * 2;
        lint_issue_t *new_arr = realloc(results->issues, new_cap * sizeof(lint_issue_t));
        if (!new_arr) {
            return FAILURE;
        }
        results->issues   = new_arr;
        results->capacity = new_cap;
    }

    lint_issue_t *issue = &results->issues[results->count];
    issue->severity     = severity;
    issue->line         = line;
    issue->message      = strdup(message);

    if (!issue->message) {
        return FAILURE;
    }

    results->count++;
    return SUCCESS;
}

static void check_heading_hierarchy(cmark_node *doc, lint_results_t *results)
{
    int prev_level = 0;
    bool has_h1    = false;

    cmark_iter *iter = cmark_iter_new(doc);
    cmark_event_type ev;

    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        if (ev != CMARK_EVENT_ENTER || cmark_node_get_type(node) != CMARK_NODE_HEADING) {
            continue;
        }

        int level = cmark_node_get_heading_level(node);
        int line  = cmark_node_get_start_line(node);

        if (level == 1) {
            if (has_h1) {
                lint_add(results, LINT_WARNING, line, "Multiple H1 headings");
            }
            has_h1 = true;
        }

        if (prev_level > 0 && level > prev_level + 1) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Heading level skips from H%d to H%d", prev_level, level);
            lint_add(results, LINT_WARNING, line, msg);
        }

        prev_level = level;
    }

    cmark_iter_free(iter);

    if (!has_h1) {
        lint_add(results, LINT_WARNING, 1, "Document has no H1 heading");
    }
}

static void check_images(cmark_node *doc, const char *base_dir, lint_results_t *results)
{
    cmark_iter *iter = cmark_iter_new(doc);
    cmark_event_type ev;

    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        if (ev != CMARK_EVENT_ENTER || cmark_node_get_type(node) != CMARK_NODE_IMAGE) {
            continue;
        }

        int line        = cmark_node_get_start_line(node);
        const char *url = cmark_node_get_url(node);

        /* Check for missing alt text */
        cmark_node *child = cmark_node_first_child(node);
        bool has_alt      = false;
        while (child) {
            if (cmark_node_get_type(child) == CMARK_NODE_TEXT) {
                const char *text = cmark_node_get_literal(child);
                if (text && text[0]) {
                    has_alt = true;
                }
            }
            child = cmark_node_next(child);
        }

        if (!has_alt) {
            lint_add(results, LINT_WARNING, line, "Image missing alt text");
        }

        /* Check for broken local image references */
        if (url && url[0] && strncmp(url, "http", 4) != 0) {
            char full_path[4096];
            if (base_dir && url[0] != '/') {
                snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, url);
            } else {
                snprintf(full_path, sizeof(full_path), "%s", url);
            }

            struct stat st;
            if (stat(full_path, &st) != 0) {
                char msg[4096 + 64];
                snprintf(msg, sizeof(msg), "Image file not found: %s", url);
                lint_add(results, LINT_ERROR, line, msg);
            }
        }
    }

    cmark_iter_free(iter);
}

static void check_links(cmark_node *doc, lint_results_t *results)
{
    cmark_iter *iter = cmark_iter_new(doc);
    cmark_event_type ev;

    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        if (ev != CMARK_EVENT_ENTER || cmark_node_get_type(node) != CMARK_NODE_LINK) {
            continue;
        }

        int line        = cmark_node_get_start_line(node);
        const char *url = cmark_node_get_url(node);

        if (!url || !url[0]) {
            lint_add(results, LINT_ERROR, line, "Link has empty URL");
        }

        /* Check for link text */
        cmark_node *child = cmark_node_first_child(node);
        bool has_text     = false;
        while (child) {
            if (cmark_node_get_type(child) == CMARK_NODE_TEXT) {
                const char *text = cmark_node_get_literal(child);
                if (text && text[0]) {
                    has_text = true;
                }
            }
            child = cmark_node_next(child);
        }

        if (!has_text) {
            lint_add(results, LINT_WARNING, line, "Link has no display text");
        }
    }

    cmark_iter_free(iter);
}

static void check_code_blocks(cmark_node *doc, lint_results_t *results)
{
    cmark_iter *iter = cmark_iter_new(doc);
    cmark_event_type ev;

    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        if (ev != CMARK_EVENT_ENTER || cmark_node_get_type(node) != CMARK_NODE_CODE_BLOCK) {
            continue;
        }

        int line               = cmark_node_get_start_line(node);
        const char *fence_info = cmark_node_get_fence_info(node);

        if (!fence_info || !fence_info[0]) {
            lint_add(results, LINT_WARNING, line, "Code block missing language hint");
        }

        const char *literal = cmark_node_get_literal(node);
        if (!literal || !literal[0]) {
            lint_add(results, LINT_WARNING, line, "Empty code block");
        }
    }

    cmark_iter_free(iter);
}

static void check_raw_text(const char *text, size_t text_len, lint_results_t *results)
{
    int line              = 1;
    int line_len          = 0;
    bool has_content      = false;
    int consecutive_blank = 0;

    for (size_t i = 0; i < text_len; i++) {
        if (text[i] == '\n') {
            if (line_len > 200) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Line exceeds 200 characters (%d chars)", line_len);
                lint_add(results, LINT_WARNING, line, msg);
            }

            if (line_len > 0 && (text[i - 1] == ' ' || text[i - 1] == '\t')) {
                lint_add(results, LINT_WARNING, line, "Trailing whitespace");
            }

            if (line_len == 0) {
                consecutive_blank++;
                if (consecutive_blank > 2) {
                    lint_add(results, LINT_WARNING, line, "Excessive blank lines");
                    /* Only report once per run of blank lines */
                    while (i + 1 < text_len && text[i + 1] == '\n') {
                        i++;
                        line++;
                    }
                }
            } else {
                consecutive_blank = 0;
                has_content       = true;
            }

            line++;
            line_len = 0;
        } else {
            line_len++;
        }
    }

    if (!has_content) {
        lint_add(results, LINT_ERROR, 1, "Document is empty");
    }
}

static void check_tables(cmark_node *doc, lint_results_t *results)
{
    cmark_iter *iter = cmark_iter_new(doc);
    cmark_event_type ev;

    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        if (ev != CMARK_EVENT_ENTER) {
            continue;
        }

        const char *type_str = cmark_node_get_type_string(node);
        if (!type_str || strcmp(type_str, "table") != 0) {
            continue;
        }

        int line        = cmark_node_get_start_line(node);
        int header_cols = 0;

        cmark_node *row = cmark_node_first_child(node);
        int row_num     = 0;
        while (row) {
            int cols         = 0;
            cmark_node *cell = cmark_node_first_child(row);
            while (cell) {
                cols++;
                cell = cmark_node_next(cell);
            }

            if (row_num == 0) {
                header_cols = cols;
            } else if (cols != header_cols) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Table row %d has %d columns, header has %d", row_num + 1, cols, header_cols);
                lint_add(results, LINT_WARNING, line, msg);
            }

            row_num++;
            row = cmark_node_next(row);
        }
    }

    cmark_iter_free(iter);
}

static int compare_issues(const void *a, const void *b)
{
    const lint_issue_t *ia = (const lint_issue_t *) a;
    const lint_issue_t *ib = (const lint_issue_t *) b;

    if (ia->line != ib->line) {
        return ia->line - ib->line;
    }

    return (int) ia->severity - (int) ib->severity;
}

int lint_check(cmark_node *doc, const char *text, size_t text_len, const char *base_dir, lint_results_t *results)
{
    if (!doc || !results) {
        return FAILURE;
    }

    check_heading_hierarchy(doc, results);
    check_images(doc, base_dir, results);
    check_links(doc, results);
    check_code_blocks(doc, results);
    check_tables(doc, results);

    if (text && text_len > 0) {
        check_raw_text(text, text_len, results);
    }

    if (results->count > 1) {
        qsort(results->issues, results->count, sizeof(lint_issue_t), compare_issues);
    }

    return SUCCESS;
}
