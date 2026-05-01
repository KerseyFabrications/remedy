/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINT_H
#define LINT_H

#include "remedy.h"

#include <cmark-gfm.h>
#include <stddef.h>

typedef enum {
    LINT_WARNING,
    LINT_ERROR,
} lint_severity_t;

typedef struct {
    lint_severity_t severity;
    int line;
    char *message;
} lint_issue_t;

typedef struct {
    lint_issue_t *issues;
    size_t count;
    size_t capacity;
} lint_results_t;

int lint_init(lint_results_t *results);
void lint_destroy(lint_results_t *results);
int lint_add(lint_results_t *results, lint_severity_t severity, int line, const char *message);

/**
 * @brief Run all lint checks on a markdown document
 *
 * @param doc Parsed cmark AST
 * @param text Raw markdown source text
 * @param text_len Length of raw text
 * @param base_dir Base directory for resolving relative paths (may be NULL)
 * @param results Output results (must be initialized)
 * @return SUCCESS or FAILURE
 */
int lint_check(cmark_node *doc, const char *text, size_t text_len, const char *base_dir, lint_results_t *results);

#endif /* LINT_H */
